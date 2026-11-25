// Rebirth of Civilisation - rivers, lakes and terrain features
// Rivers run through the middle of tiles, from the hills down to the sea or a lake. Each river tile
// records which neighbours the river joins it to, so boats can sail up and down them: river boats
// ride the current downstream faster than they row against it, and a waterfall blocks the way.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>
#include <cmath>
#include <queue>

// ------------------------------------------------------------ queries
int Game::dirTo(int fx, int fy, int tx, int ty) const {
    for (int d = 0; d < 6; d++) {
        Pt p;
        if (neighborDir(fx, fy, d, p) && p.x == tx && p.y == ty) return d;
    }
    return -1;
}

bool Game::riverLink(int fx, int fy, int tx, int ty) const {
    int d = dirTo(fx, fy, tx, ty);
    return d >= 0 && (at(fx, fy).river & (1 << d)) != 0;
}

bool Game::downstream(int fx, int fy, int tx, int ty) const {
    int d = dirTo(fx, fy, tx, ty);
    return d >= 0 && at(fx, fy).riverOut == d;
}

bool Game::isFalls(int x, int y) const {
    const Tile& t = at(x, y);
    return t.feature == F_WONDER && t.natural >= 0 && g_naturals[t.natural].site == WS_FALLS;
}

// Could a boat go from one tile to the next? Open water joins open water (the open ocean only for ships
// that can cross it); a city is a port onto any water beside it; rivers join only along their course.
bool Game::waterStep(int fx, int fy, int tx, int ty, bool ocean, bool river) const {
    bool fw = isWater(fx, fy), tw = isWater(tx, ty);
    if (tw) {
        if (at(tx, ty).terrain == T_OCEAN && !ocean) return false;
        if (fw) return true;
        if (cityAt(fx, fy)) return true;
        return river && riverLink(fx, fy, tx, ty);
    }
    if (isFalls(tx, ty)) return false;
    if (fw && cityAt(tx, ty)) return true;
    return river && riverLink(fx, fy, tx, ty);
}

bool Game::isPort(int x, int y) const { return isCoastal(x, y) || hasRiver(x, y); }

bool Game::freshWater(int x, int y) const {
    const Tile& t = at(x, y);
    if (hasRiver(x, y) || t.feature == F_OASIS) return true;
    Pt nb[6];
    int n = neighbors(x, y, nb);
    for (int k = 0; k < n; k++) {
        const Tile& o = at(nb[k].x, nb[k].y);
        if (o.feature == F_LAKE || o.feature == F_OASIS || hasRiver(nb[k].x, nb[k].y)) return true;
    }
    return false;
}

bool Game::shipCanStand(const Unit& u, int x, int y) const {
    const UnitDef& d = g_units[u.type];
    if (isWater(x, y)) return at(x, y).terrain != T_OCEAN || d.ocean;
    if (const City* c = cityAt(x, y)) return c->owner == u.owner;
    return d.river && hasRiver(x, y) && !isFalls(x, y);
}

// ------------------------------------------------------------ generation
namespace {
int opposite(int d) { return (d + 3) % 6; }
}

void Game::generateRivers(const std::vector<double>& elev) {
    int N = W * H;
    // Small enclosed seas become lakes: fresh water, and shallow enough for coastal boats.
    std::vector<int> comp(N, -1);
    std::vector<std::vector<int>> comps;
    for (int i = 0; i < N; i++) {
        if (comp[i] >= 0 || !isWater(i % W, i / W)) continue;
        std::vector<int> members = {i};
        comp[i] = (int)comps.size();
        for (size_t k = 0; k < members.size(); k++) {
            Pt nb[6];
            int n = neighbors(members[k] % W, members[k] / W, nb);
            for (int j = 0; j < n; j++) {
                int ni = idx(nb[j].x, nb[j].y);
                if (comp[ni] < 0 && isWater(nb[j].x, nb[j].y)) { comp[ni] = comp[i]; members.push_back(ni); }
            }
        }
        comps.push_back(members);
    }
    for (auto& m : comps)
        if (m.size() <= 12)
            for (int i : m) { tiles[i].terrain = T_COAST; tiles[i].feature = F_LAKE; tiles[i].res = R_NONE; }

    // Distance from the sea over land (lakes count as land here, so rivers find their way past them).
    auto seaWater = [&](int i) { return isWater(i % W, i / W) && tiles[i].feature != F_LAKE; };
    std::vector<int> dSea(N, 1 << 20);
    std::queue<int> q;
    for (int i = 0; i < N; i++) if (seaWater(i)) { dSea[i] = 0; q.push(i); }
    while (!q.empty()) {
        int cur = q.front();
        q.pop();
        Pt nb[6];
        int n = neighbors(cur % W, cur / W, nb);
        for (int k = 0; k < n; k++) {
            int ni = idx(nb[k].x, nb[k].y);
            if (dSea[ni] <= dSea[cur] + 1 || tiles[ni].mountain) continue;
            dSea[ni] = dSea[cur] + 1;
            q.push(ni);
        }
    }

    // New lakes in the interior, far from the sea.
    int wantLakes = std::max(2, N / 900);
    for (int tries = 0; tries < 400 && wantLakes > 0; tries++) {
        int x = rng.range(W), y = rng.range(H);
        int i = idx(x, y);
        Tile& t = tiles[i];
        if (isWater(x, y) || t.mountain || t.hills || t.terrain == T_SNOW || dSea[i] < 5 || dSea[i] > 1000) continue;
        bool nearLake = false;
        for (auto& p : ring(x, y, 4)) if (at(p.x, p.y).feature == F_LAKE) nearLake = true;
        if (nearLake) continue;
        std::vector<int> lake = {i};
        Pt nb[6];
        int n = neighbors(x, y, nb);
        for (int k = 0; k < n && (int)lake.size() < 3; k++) {
            int ni = idx(nb[k].x, nb[k].y);
            if (!tiles[ni].mountain && dSea[ni] >= 4 && rng.chance(0.4)) lake.push_back(ni);
        }
        for (int li : lake) {
            Tile& l = tiles[li];
            l = Tile();
            l.terrain = T_COAST;
            l.feature = F_LAKE;
        }
        wantLakes--;
    }

    // Rivers: from springs in the hills, down towards the sea.
    auto lakeAt = [&](int i) { return tiles[i].feature == F_LAKE; };
    auto link = [&](int a, int b) {
        int d = dirTo(a % W, a / W, b % W, b / W);
        tiles[a].river |= (uint8_t)(1 << d);
        tiles[b].river |= (uint8_t)(1 << opposite(d));
        tiles[a].riverOut = (int8_t)d;
    };
    std::vector<uint8_t> curLake(N, 0);  // the lake a river is draining, so it does not flow back into it
    // Trace a river from a source tile; returns false (and changes nothing) if it would be too short.
    auto trace = [&](int src, int minLen, int fromLake) -> bool {
        std::vector<int> path = {src};
        std::vector<uint8_t> on(N, 0);
        on[src] = 1;
        int end = -1, cur = src, sideways = 0;
        for (int guard = 0; guard < 200; guard++) {
            int cx = cur % W, cy = cur / W;
            Pt nb[6];
            int n = neighbors(cx, cy, nb);
            // reached the sea, a lake (not the one we flow from) or another river: join it
            int join = -1;
            for (int k = 0; k < n; k++) {
                int ni = idx(nb[k].x, nb[k].y);
                if (curLake[ni] || on[ni]) continue;
                if (seaWater(ni) || (lakeAt(ni) && path.size() >= 2)) { join = ni; break; }
            }
            if (join < 0 && path.size() >= 2)
                for (int k = 0; k < n; k++) {
                    int ni = idx(nb[k].x, nb[k].y);
                    if (!on[ni] && !isWater(nb[k].x, nb[k].y) && tiles[ni].river && dSea[ni] <= dSea[cur]) { join = ni; break; }
                }
            if (join >= 0) { end = join; break; }
            // otherwise flow downhill: nearer the sea, lowest first, now and then meandering sideways
            int best = -1;
            double bs = 1e9;
            for (int k = 0; k < n; k++) {
                int ni = idx(nb[k].x, nb[k].y);
                if (on[ni] || isWater(nb[k].x, nb[k].y) || tiles[ni].mountain || tiles[ni].river) continue;
                int dd = dSea[ni] - dSea[cur];
                if (dd > 0 || (dd == 0 && (sideways >= 4 || !rng.chance(0.5)))) continue;
                // keep clear of the river's own banks so its course reads as one line
                int touching = 0;
                Pt nb2[6];
                int n2 = neighbors(nb[k].x, nb[k].y, nb2);
                for (int j = 0; j < n2; j++) if (on[idx(nb2[j].x, nb2[j].y)]) touching++;
                if (touching > 1) continue;
                double s = dd * 1.0 + elev[ni] * 2.0 + rng.unit() * 0.6;
                if (s < bs) { bs = s; best = ni; }
            }
            if (best < 0) break;
            if (dSea[best] == dSea[cur]) sideways++;
            path.push_back(best);
            on[best] = 1;
            cur = best;
        }
        if (end < 0 || (int)path.size() < minLen) return false;
        if (fromLake >= 0) link(fromLake, path[0]);
        for (size_t k = 0; k + 1 < path.size(); k++) link(path[k], path[k + 1]);
        link(path.back(), end);
        if (isWater(end % W, end / W)) tiles[end].riverOut = -1;
        return true;
    };

    // every lake drains to the sea
    for (int i = 0; i < N; i++) {
        if (!lakeAt(i) || tiles[i].river) continue;
        // the shore tile of this lake nearest the sea
        std::vector<int> lake = {i};
        std::vector<uint8_t> in(N, 0);
        in[i] = 1;
        int out = -1, outFrom = -1;
        for (size_t k = 0; k < lake.size(); k++) {
            Pt nb[6];
            int n = neighbors(lake[k] % W, lake[k] / W, nb);
            for (int j = 0; j < n; j++) {
                int ni = idx(nb[j].x, nb[j].y);
                if (lakeAt(ni)) { if (!in[ni]) { in[ni] = 1; lake.push_back(ni); } continue; }
                if (isWater(nb[j].x, nb[j].y) || tiles[ni].mountain || tiles[ni].river) continue;
                if (out < 0 || dSea[ni] < dSea[out]) { out = ni; outFrom = lake[k]; }
            }
        }
        bool drained = false;
        for (int l : lake) if (tiles[l].river) drained = true;
        for (int l : lake) curLake[l] = 1;
        if (!drained && out >= 0 && dSea[out] < 1000) trace(out, 2, outFrom);
        for (int l : lake) curLake[l] = 0;
    }

    // springs: high ground well inland, not too close to another river
    std::vector<std::pair<double, int>> springs;
    for (int i = 0; i < N; i++) {
        const Tile& t = tiles[i];
        int x = i % W, y = i / W;
        if (isWater(x, y) || t.mountain || dSea[i] < 3 || dSea[i] > 1000) continue;
        bool high = t.hills;
        Pt nb[6];
        int n = neighbors(x, y, nb);
        for (int k = 0; k < n; k++) if (at(nb[k].x, nb[k].y).mountain || at(nb[k].x, nb[k].y).hills) high = true;
        if (!high && dSea[i] < 5) continue;
        springs.push_back({elev[i] + dSea[i] * 0.02 + rng.unit() * 0.25, i});
    }
    std::sort(springs.begin(), springs.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    int want = std::max(6, N / 110);
    for (auto& sp : springs) {
        if (want <= 0) break;
        int x = sp.second % W, y = sp.second / W;
        bool crowded = false;
        for (auto& p : ring(x, y, 2)) if (hasRiver(p.x, p.y)) { crowded = true; break; }
        if (crowded) continue;
        if (trace(sp.second, 4, -1)) want--;
    }

    // how much land drains through each river tile decides how big the river is there
    std::vector<int> flow(N, 0);
    for (int i = 0; i < N; i++) {
        if (!hasRiver(i % W, i / W)) continue;
        int cur = i;
        for (int guard = 0; guard < 300 && cur >= 0; guard++) {
            flow[cur]++;
            const Tile& t = tiles[cur];
            if (t.riverOut < 0 || isWater(cur % W, cur / W)) break;
            Pt p;
            if (!neighborDir(cur % W, cur / W, t.riverOut, p)) break;
            cur = idx(p.x, p.y);
        }
    }
    for (int i = 0; i < N; i++) {
        if (!hasRiver(i % W, i / W)) continue;
        tiles[i].riverSize = flow[i] >= 10 ? RIVER_GREAT : flow[i] >= 4 ? RIVER_RIVER : RIVER_STREAM;
    }
}

void Game::generateFeatures() {
    int N = W * H;
    auto latOf = [&](int y) { return std::abs(y - (H - 1) / 2.0) / ((H - 1) / 2.0); };
    for (int i = 0; i < N; i++) {
        int x = i % W, y = i / W;
        Tile& t = tiles[i];
        if (t.feature != F_NONE) continue;
        double lat = latOf(y);
        if (isWater(x, y)) {
            if (t.terrain == T_COAST && lat < 0.45 && t.res == R_NONE && rng.chance(0.06)) t.feature = F_REEF;
            continue;
        }
        if (t.mountain || t.hills) continue;
        bool river = hasRiver(x, y);
        if (river && t.veg == V_NONE && ((t.terrain == T_DESERT && rng.chance(0.8)) || (t.terrain == T_PLAINS && rng.chance(0.35)) ||
                                         (t.terrain == T_GRASS && rng.chance(0.15)))) {
            t.feature = F_FLOODPLAIN;
            continue;
        }
        if (t.terrain == T_GRASS && lat < 0.65 && (t.veg == V_NONE || t.veg == V_JUNGLE) && freshWater(x, y) && rng.chance(0.2)) {
            t.feature = F_MARSH;
            t.veg = V_NONE;
            continue;
        }
        if (t.terrain == T_DESERT && !river && !isCoastal(x, y) && rng.chance(0.035)) {
            t.feature = F_OASIS;
            t.res = R_NONE;
        }
    }
    // volcanoes
    int vols = 1 + settings.mapSize;
    for (int tries = 0; tries < 2000 && vols > 0; tries++) {
        int x = rng.range(W), y = rng.range(H);
        Tile& t = at(x, y);
        if (!t.mountain || t.feature != F_NONE) continue;
        bool near = false;
        for (auto& p : ring(x, y, 6)) if (at(p.x, p.y).feature == F_VOLCANO) near = true;
        if (near) continue;
        t.feature = F_VOLCANO;
        vols--;
    }
    // natural wonders
    static const int COUNT[] = {3, 4, 5};
    std::vector<int> order;
    for (int k = 0; k < (int)g_naturals.size(); k++) order.push_back(k);
    for (size_t i = order.size(); i > 1; i--) std::swap(order[i - 1], order[rng.range((int)i)]);
    naturals.clear();
    for (int type : order) {
        if ((int)naturals.size() >= COUNT[settings.mapSize]) break;
        int site = g_naturals[type].site;
        std::vector<int> cand;
        for (int i = 0; i < N; i++) {
            int x = i % W, y = i / W;
            const Tile& t = tiles[i];
            if (t.feature != F_NONE && t.feature != F_FLOODPLAIN) continue;
            if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2) continue;
            double lat = latOf(y);
            bool water = isWater(x, y), flat = !water && !t.mountain && !t.hills;
            bool ok = false;
            switch (site) {
                case WS_FALLS: ok = hasRiver(x, y) && t.hills && t.riverOut >= 0; break;
                case WS_FLAT: ok = flat && (t.terrain == T_GRASS || t.terrain == T_PLAINS) && !hasRiver(x, y); break;
                case WS_HILLS: ok = !water && t.hills && !hasRiver(x, y); break;
                case WS_REEF: ok = t.terrain == T_COAST && lat < 0.5; break;
                case WS_SALT: ok = flat && t.terrain == T_DESERT && !hasRiver(x, y); break;
                case WS_COLD: ok = flat && (t.terrain == T_TUNDRA || t.terrain == T_SNOW) && !hasRiver(x, y); break;
                case WS_PEAK: ok = t.mountain; break;
                case WS_MESA: ok = !water && t.hills && (t.terrain == T_DESERT || t.terrain == T_PLAINS) && !hasRiver(x, y); break;
            }
            if (!ok) continue;
            bool near = false;
            for (auto& s : naturals) if (dist(s.x, s.y, x, y) < 9) near = true;
            if (near) continue;
            cand.push_back(i);
        }
        if (cand.empty()) continue;
        int i = cand[rng.range((int)cand.size())];
        Tile& t = tiles[i];
        t.feature = F_WONDER;
        t.natural = (int8_t)type;
        t.res = R_NONE;
        t.veg = V_NONE;
        NaturalSite ns;
        ns.type = type;
        ns.x = i % W;
        ns.y = i / W;
        naturals.push_back(ns);
    }
}

// ------------------------------------------------------------ discovery
void Game::discoverNaturals(int c, int x, int y, int r) {
    if (c < FIRST_PRIMATE_CIV || c >= (int)civs.size()) return;
    for (auto& s : naturals) {
        if ((int)s.found.size() < (int)civs.size()) s.found.resize(civs.size(), 0);
        if (s.found[c] || dist(s.x, s.y, x, y) > r) continue;
        bool first = true;
        for (size_t o = FIRST_PRIMATE_CIV; o < s.found.size(); o++) if (s.found[o]) first = false;
        s.found[c] = 1;
        if (turn <= 1 && !first) continue;
        const NaturalDef& nd = g_naturals[s.type];
        int cult = first ? 20 : 8;
        int sci = first ? 15 : 0;
        civs[c].culture += cult;
        if (sci) addScience(c, sci);
        civs[c].naturalsFound++;
        std::string body = nd.desc + "\n\n" + (first ? "Your explorers are the first to see it. " : "") + "The sight inspires your people: +" +
                           std::to_string(cult) + " Culture" + (sci ? ", +" + std::to_string(sci) + " Science" : "") + ".";
        popup(c, POP_NATURAL, nd.name, body, s.x, s.y);
    }
}

// ------------------------------------------------------------ volcanoes
void Game::volcanoTurn() {
    if (turn < 20) return;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (at(x, y).feature != F_VOLCANO) continue;
            if (!rng.chance(0.004 * 100.0 / speedPct)) continue;
            int hurtUnits = 0, wrecked = 0, burnt = 0;
            for (auto& p : ring(x, y, 1)) {
                Tile& t = at(p.x, p.y);
                if (p.x == x && p.y == y) continue;
                if (t.imp != IMP_NONE && rng.chance(0.5)) { t.imp = IMP_NONE; wrecked++; }
                if (t.veg != V_NONE && rng.chance(0.6)) { t.veg = V_NONE; burnt++; }
            }
            for (auto& u : units) {
                if (!u.alive || dist(u.x, u.y, x, y) > 1 || u.carrier >= 0) continue;
                u.hp -= 40;
                hurtUnits++;
                if (u.hp <= 0) {
                    if (u.owner >= FIRST_PRIMATE_CIV) notify(u.owner, "Your " + g_units[u.type].name + " was killed by the eruption.", u.x, u.y);
                    killUnit(u);
                }
            }
            City* hit = nullptr;
            for (auto& ci : cities)
                if (ci.alive && dist(ci.x, ci.y, x, y) <= 1) {
                    hit = &ci;
                    if (ci.pop > 1) ci.pop--;
                    ci.hp = std::max(1, ci.hp - cityMaxHP(ci) / 4);
                }
            std::string body = "The volcano erupted, raining ash and fire on the land around it.";
            if (hit) body += " " + hit->name + " was struck and lost people.";
            if (wrecked) body += " " + std::to_string(wrecked) + " improvement" + (wrecked > 1 ? "s were" : " was") + " buried.";
            body += "\n\nIn time the ash will make the soil richer still.";
            for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) {
                if (!civs[c].alive || !civs[c].explored[idx(x, y)]) continue;
                bool close = false;
                for (auto& p : ring(x, y, 2)) if (at(p.x, p.y).owner == c) close = true;
                for (auto& ci : cities) if (ci.alive && ci.owner == c && dist(ci.x, ci.y, x, y) <= 3) close = true;
                if (close) popup(c, POP_DISASTER, "Eruption", body, x, y);
                else notify(c, "A volcano has erupted.", x, y);
            }
            (void)hurtUnits; (void)burnt;
        }
}
