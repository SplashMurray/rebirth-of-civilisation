// Rebirth of Civilisation - the nomadic age: roaming bands before the first settlement
//
// Every species starts as wandering bands of primates. A band forages the land around it: the
// richer the tiles (forest fruit, jungle, grassland), the more it eats. Foraged land is picked
// over and needs time to recover, so a band that stays put soon goes hungry - it must keep
// moving. Well-fed bands grow; large bands can split in two, or send members off as war parties
// and scouts. Only once a species has evolved Speech and discovered Cultivation can a band
// settle down and found a city - the great transition from roaming pack to settled people.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

bool Game::canSettle(int c) const {
    return c >= FIRST_PRIMATE_CIV && hasTech(c, ID.tCultivation);
}

int Game::numBands(int c) const {
    int n = 0;
    for (auto& u : units) if (u.alive && u.owner == c && g_units[u.type].cls == UnitClass::Band) n++;
    return n;
}

int Game::bandMembers(int c) const {
    int n = 0;
    for (auto& u : units) if (u.alive && u.owner == c && g_units[u.type].cls == UnitClass::Band) n += u.band;
    return n;
}

int Game::forageFood(int c, int x, int y) const {
    if (!inMap(x, y)) return 0;
    const Tile& t = at(x, y);
    if (t.mountain || t.terrain == T_OCEAN) return 0;
    if (t.owner >= 0 && t.owner != c) return 0;     // someone else's land
    if (!warHappened && t.site >= 0) return 0;      // the humans are still there
    int f = tileYields(c, x, y).food;
    if (!isWater(x, y) && t.fallout < 2) f += 1;  // roots, insects, small game: wild land feeds a band better than a farmer
    if (t.veg == V_FOREST || t.veg == V_JUNGLE) f += 1 + (hasTech(c, ID.tForaging) ? 1 : 0);  // fruit, nuts, honey
    return std::max(0, f - (int)t.foraged);
}

int Game::bandForage(const Unit& u, std::vector<int>* tiles) const {
    std::vector<std::pair<int, int>> cand;  // food, tile index
    for (auto& p : ring(u.x, u.y, 1)) {
        int f = forageFood(u.owner, p.x, p.y);
        if (f > 0) cand.push_back({f, idx(p.x, p.y)});
    }
    std::sort(cand.begin(), cand.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) { return a.first > b.first; });
    int pickers = std::min((int)cand.size(), u.band + 1);
    int food = 0;
    for (int k = 0; k < pickers; k++) {
        food += cand[k].first;
        if (tiles) tiles->push_back(cand[k].second);
    }
    if (hasTech(u.owner, ID.tCooking)) food += 2;  // fire makes food go further
    int pct = civs[u.owner].fx.food + civs[u.owner].yieldBonusPct / 2;
    if (const PrimateDef* pd = primate(u.owner)) pct += pd->foodPct;
    return applyPct(food, pct);
}

int Game::bandGrowth(int size) const {
    return sc(8 + 4 * size);
}

int Game::bandScience(const Unit& u) const {
    int c = u.owner;
    if (!hasTrait(c, TR_NEOCORTEX)) return 0;
    int s = 1 + u.band * 2 / 3 + civs[c].fx.flatSci;
    int pct = civs[c].fx.sci + civs[c].yieldBonusPct + policyPct(c, PolicyEffect::Sci) + diploSciPct(c);
    if (const PrimateDef* pd = primate(c)) pct += pd->sciPct;
    return applyPct(s, pct);
}

int Game::bandCulture(const Unit& u) const {
    int c = u.owner;
    int s = u.band / 3 + civs[c].fx.flatCult;
    int pct = civs[c].fx.cult + policyPct(c, PolicyEffect::Culture);
    if (const PrimateDef* pd = primate(c)) pct += pd->cultPct;
    return applyPct(s, pct);
}

// Land recovers slowly once the bands have moved on.
void Game::regrowForage() {
    for (auto& t : tiles)
        if (t.foraged > 0 && rng.chance(0.3)) t.foraged--;
}

void Game::processBands(int c, int& sci, int& cult) {
    for (auto& u : units) {
        if (!u.alive || u.owner != c || g_units[u.type].cls != UnitClass::Band) continue;
        std::vector<int> picked;
        int food = bandForage(u, &picked) - u.band;
        for (int i : picked) tiles[i].foraged = (uint8_t)std::min(3, tiles[i].foraged + 1);
        sci += bandScience(u);
        cult += bandCulture(u);
        u.bandFood += food;
        if (food >= 0) {
            int need = bandGrowth(u.band);
            if (u.bandFood >= need) {
                if (u.band < BAND_MAX) {
                    u.band++;
                    u.bandFood -= need;
                    civs[c].maxBand = std::max(civs[c].maxBand, u.band);
                    if (u.band == BAND_MAX)
                        notify(c, "A band has grown to " + std::to_string(BAND_MAX) + " - as large as the land can feed. It could split in two.", u.x, u.y);
                } else {
                    u.bandFood = need;
                }
            }
        } else if (u.bandFood < 0) {
            u.bandFood = 0;
            if (u.band > 1) {
                u.band--;
                notify(c, "A band is starving and has lost a member. Move it to fresh foraging grounds!", u.x, u.y);
            }
        }
    }
}

bool Game::hurtBand(Unit& u, int loss, const std::string& why) {
    if (loss <= 0) {
        notify(u.owner, "One of your bands drove off an attack: " + why + ".", u.x, u.y);
        return false;
    }
    u.band -= loss;
    if (u.band <= 0) {
        notify(u.owner, "One of your bands was wiped out: " + why + ".", u.x, u.y);
        killUnit(u);
        return true;
    }
    notify(u.owner, "One of your bands lost " + std::to_string(loss) + (loss == 1 ? " member: " : " members: ") + why + ".", u.x, u.y);
    return false;
}

bool Game::bandSpawnSpot(const Unit& u, bool military, Pt& out) const {
    auto ok = [&](int x, int y) {
        if (!inMap(x, y) || isWater(x, y) || at(x, y).mountain) return false;
        if (!warHappened && at(x, y).site >= 0) return false;
        if (const City* ci = cityAt(x, y)) if (ci->owner != u.owner) return false;
        for (auto& o : units) {
            if (!o.alive || o.x != x || o.y != y) continue;
            if (o.owner != u.owner) return false;
            if (g_units[o.type].landMilitary() == military && !g_units[o.type].naval()) return false;
        }
        return true;
    };
    if (ok(u.x, u.y)) { out = {u.x, u.y}; return true; }
    Pt nb[6];
    int n = neighbors(u.x, u.y, nb);
    for (int k = 0; k < n; k++)
        if (ok(nb[k].x, nb[k].y)) { out = nb[k]; return true; }
    return false;
}

bool Game::splitBand(Unit& u) {
    if (g_units[u.type].cls != UnitClass::Band || u.band < 4 || u.moves <= 0) return false;
    Pt nb[6];
    int n = neighbors(u.x, u.y, nb);
    Pt spot = {-1, -1};
    for (int k = 0; k < n && spot.x < 0; k++) {
        int x = nb[k].x, y = nb[k].y;
        if (isWater(x, y) || at(x, y).mountain || (!warHappened && at(x, y).site >= 0) || cityAt(x, y)) continue;
        bool blocked = false;
        for (auto& o : units)
            if (o.alive && o.x == x && o.y == y && (o.owner != u.owner || g_units[o.type].civilian())) blocked = true;
        if (!blocked) spot = nb[k];
    }
    if (spot.x < 0) return false;
    int size = u.band / 2, food = u.bandFood / 2;
    int owner = u.owner, ux = u.x, uy = u.y;
    u.band -= size;
    u.bandFood -= food;
    u.moves = 0;
    civs[owner].bandSplits++;
    int id = spawnUnit(owner, ID.uBand, spot.x, spot.y);  // may reallocate units: don't touch u after this
    units[id].band = size;
    units[id].bandFood = food;
    units[id].moves = 0;
    notify(owner, "The band has split in two. The new band of " + std::to_string(size) + " heads off to forage on its own.", ux, uy);
    if (owner >= FIRST_PRIMATE_CIV) updateVisibility(owner);
    return true;
}

bool Game::raiseWarParty(Unit& u) {
    if (g_units[u.type].cls != UnitClass::Band || u.band < 3 || u.moves <= 0) return false;
    Pt spot;
    if (!bandSpawnSpot(u, true, spot)) return false;
    int owner = u.owner;
    u.band -= 2;
    u.moves = 0;
    int id = spawnUnit(owner, uniqueFor(owner, ID.uWarband), spot.x, spot.y);
    units[id].moves = 0;
    return true;
}

bool Game::sendScout(Unit& u) {
    if (g_units[u.type].cls != UnitClass::Band || u.band < 3 || u.moves <= 0) return false;
    Pt spot;
    if (!bandSpawnSpot(u, true, spot)) return false;
    int owner = u.owner;
    u.band -= 1;
    u.moves = 0;
    int id = spawnUnit(owner, uniqueFor(owner, ID.uScout), spot.x, spot.y);
    units[id].moves = 0;
    return true;
}

int Game::settleBand(Unit& u) {
    if (g_units[u.type].cls != UnitClass::Band || u.moves <= 0 || !canFoundAt(u.owner, u.x, u.y)) return -1;
    int c = u.owner;
    bool firstEver = true;
    for (auto& ci : cities) if (ci.origOwner == c) firstEver = false;
    int pop = settlePop(u), food = u.bandFood;
    int cid = foundCity(c, u.x, u.y);
    killUnit(u);
    City& city = cities[cid];
    city.pop = pop;
    city.food = std::min(food, growthThreshold(pop) - 1);
    computeCity(city);
    if (firstEver) {
        const PrimateDef* pd = primate(c);
        std::string who = pd ? pd->plural : civs[c].name;
        if (human(c))
            popup(c, POP_SETTLE, "The First Settlement",
                  "For countless generations the " + who + " wandered, following the fruit and fleeing the cold. Today, beside the gardens "
                  "they planted, they stopped - and stayed.\n\n" + city.name + " is the first settlement of your people. "
                  "Cities grow, build, research and claim land around them. Choose what " + city.name + " should build.\n\n"
                  "Any bands still roaming can settle too - or keep wandering and foraging until they find the right place.",
                  city.x, city.y);
        for (int o : viewers())
            if (o != c && civs[o].met[c]) notify(o, "The " + civs[c].name + " have given up the wandering life and founded " + city.name + ".", city.x, city.y);
    }
    return cid;
}

// ------------------------------------------------------------ AI
void Game::aiBand(Unit& uref) {
    int uid = uref.id;
    auto U = [&]() -> Unit& { return units[uid]; };
    int c = U().owner;
    if (U().moves <= 0) return;

    // Settle as soon as the species can, at a decent spot nearby
    if (canSettle(c)) {
        int hereScore = aiSiteScore(c, U().x, U().y);
        if (canFoundAt(c, U().x, U().y) && (hereScore >= 30 || (numCities(c) == 0 && hereScore >= 0))) {
            int best = aiSiteScore(c, U().x, U().y);
            bool better = false;
            for (auto& p : ring(U().x, U().y, 2))
                if (aiSiteScore(c, p.x, p.y) > best + 12) better = true;
            if (!better || U().order == ORD_GOTO) { settleBand(U()); return; }
        }
        if (U().order == ORD_GOTO && aiSiteScore(c, U().gx, U().gy) > 0) {
            continueGoto(U());
            if (U().alive && U().x == U().gx && U().y == U().gy && U().moves > 0) settleBand(U());
            return;
        }
        int best = -1, bx = -1, by = -1;
        for (auto& p : ring(U().x, U().y, 6)) {
            int s = aiSiteScore(c, p.x, p.y);
            if (s < 0) continue;
            s = s * 3 - dist(U().x, U().y, p.x, p.y) * 5;
            if (s > best) {
                if (!(p.x == U().x && p.y == U().y) && findPath(U(), p.x, p.y).empty()) continue;
                best = s; bx = p.x; by = p.y;
            }
        }
        if (bx >= 0) {
            if (bx == U().x && by == U().y) { settleBand(U()); return; }
            U().order = ORD_GOTO; U().gx = bx; U().gy = by;
            continueGoto(U());
            if (U().alive && U().x == bx && U().y == by && U().moves > 0) settleBand(U());
            return;
        }
        // nowhere good: keep foraging below
    }

    // Grow the people: split large bands, raise a guard, send a scout
    int bands = numBands(c), mil = 0, scouts = 0;
    for (auto& o : units) {
        if (!o.alive || o.owner != c) continue;
        if (g_units[o.type].cls == UnitClass::Recon) scouts++;
        else if (g_units[o.type].military()) mil++;
    }
    int sk = skillOf(c);
    int splitAt = sk == 0 ? 8 : sk == 1 ? 6 : 5;
    if (U().band >= splitAt && bands < 3 + settings.mapSize + (sk >= 2 ? 1 : 0) && splitBand(U())) return;
    if (((U().band >= 5 && mil < bands) || (U().band >= 4 && mil == 0)) && raiseWarParty(U())) return;
    if (U().band >= 6 && scouts == 0 && sendScout(U())) return;

    // Forage: walk to the richest unpicked land within reach, away from danger
    auto danger = [&](int x, int y) {
        int d = 0;
        for (auto& e : units)
            if (e.alive && atWar(c, e.owner) && g_units[e.type].military() && dist(e.x, e.y, x, y) <= 2) d += 30 - 8 * dist(e.x, e.y, x, y);
        if (!warHappened)
            for (auto& s : sites) if (dist(s.x, s.y, x, y) <= 2) d += 20;
        d += at(x, y).fallout * 40;
        if (U().hp < 60) d += at(x, y).fallout * 40;
        // other species' bands range over their own lands: give them room
        for (auto& e : units)
            if (e.alive && e.owner != c && e.owner >= FIRST_PRIMATE_CIV && g_units[e.type].cls == UnitClass::Band) {
                int de = dist(e.x, e.y, x, y);
                if (de <= 3) d += 4 * (4 - de);
            }
        return d;
    };
    Unit probe = U();
    int here = bandForage(probe) - danger(U().x, U().y);
    int best = here + 2, bx = -1, by = -1;
    for (auto& p : ring(U().x, U().y, 3)) {
        if (p.x == U().x && p.y == U().y) continue;
        if (isWater(p.x, p.y) || at(p.x, p.y).mountain) continue;
        probe.x = p.x; probe.y = p.y;
        int s = bandForage(probe) - danger(p.x, p.y) - dist(U().x, U().y, p.x, p.y) + rng.range(3);
        if (s > best) {
            if (findPath(U(), p.x, p.y).empty()) continue;
            best = s; bx = p.x; by = p.y;
        }
    }
    if (bx >= 0) {
        U().order = ORD_GOTO; U().gx = bx; U().gy = by;
        continueGoto(U());
        if (U().alive && U().order == ORD_GOTO) U().order = ORD_NONE;  // re-plan next turn
    }
}

// ------------------------------------------------------------ disasters on the move
bool Game::nomadDisaster(int c) {
    Civ& cv = civs[c];
    std::vector<int> mine;
    for (auto& u : units) if (u.alive && u.owner == c && g_units[u.type].cls == UnitClass::Band) mine.push_back(u.id);
    if (mine.empty()) return false;
    int bid = mine[rng.range((int)mine.size())];
    Unit& b = units[bid];
    int bx = b.x, by = b.y;
    bool trees = false, cold = false;
    for (auto& p : ring(bx, by, 1)) {
        const Tile& t = at(p.x, p.y);
        if (t.veg != V_NONE) trees = true;
        if (t.terrain == T_TUNDRA || t.terrain == T_SNOW) cold = true;
    }
    bool nuclearWinter = warHappened && turn <= PROLOGUE_TURNS + dur(30);
    enum { N_FIRE, N_DROUGHT, N_SICK, N_COLD, N_PREDATOR, N_COUNT };
    int w[N_COUNT] = {};
    w[N_FIRE] = trees ? 6 : 0;
    w[N_DROUGHT] = 5;
    w[N_SICK] = b.band >= 5 ? 5 : 0;
    w[N_COLD] = (cold ? 6 : 0) + (nuclearWinter ? 8 : 0);
    w[N_PREDATOR] = 5;
    int total = 0;
    for (int v : w) total += v;
    int r = rng.range(total), type = 0;
    for (; type < N_COUNT; type++) {
        if (r < w[type]) break;
        r -= w[type];
    }
    std::string title, body, ptext;
    auto pressure = [&](const std::string& name, int cat, int burst, int perTurn, int turns) {
        int bb = sc(burst);
        cv.evoPool[cat] += bb;
        if (perTurn > 0) cv.tempPressures.push_back({name, cat, perTurn, dur(turns)});
        std::string line = std::string("\n") + PRESSURE_NAMES[cat] + ": ";
        if (bb > 0) line += "+" + std::to_string(bb) + " points now";
        if (bb > 0 && perTurn > 0) line += ", then ";
        if (perTurn > 0) line += "+" + std::to_string(perTurn) + " per turn for " + std::to_string(dur(turns)) + " turns";
        ptext += line;
    };
    switch (type) {
        case N_FIRE:
            for (auto& p : ring(bx, by, 1)) {
                Tile& t = at(p.x, p.y);
                if (t.veg != V_NONE && rng.chance(0.6)) { t.veg = V_NONE; t.foraged = 3; }
            }
            b.bandFood = 0;
            title = "Wildfire";
            body = "Lightning set the forest ablaze around one of your bands. They escaped with their lives, but their foraging grounds are ash.";
            pressure("Wildfire aftermath", PC_ECOLOGY, 12, 2, 8);
            break;
        case N_DROUGHT:
            for (auto& p : ring(bx, by, 2)) at(p.x, p.y).foraged = 3;
            b.bandFood = 0;
            title = "Drought";
            body = "The rains have failed. The fruit withers and the waterholes dry up around one of your bands. It must move far to find food.";
            pressure("Drought", PC_ECOLOGY, 10, 2, 8);
            break;
        case N_SICK: {
            int loss = std::max(1, b.band / 4);
            hurtBand(b, loss, "a sickness swept through the band");
            title = "Sickness";
            body = "A fever spread from one to another in a crowded band. " + std::to_string(loss) +
                   (loss == 1 ? " member died." : " members died.") + " The survivors are hardier for it.";
            pressure("Disease survivors", PC_MUTATION, 10, 2, 6);
            pressure("Grief", PC_SOCIAL, 6, 0, 0);
            break;
        }
        case N_COLD:
            b.bandFood = 0;
            if (b.band > 2) hurtBand(b, 1, "the bitter cold");
            title = nuclearWinter ? "Nuclear Winter Storm" : "Harsh Winter";
            body = std::string(nuclearWinter ? "Ash still blots out the sun. " : "") +
                   "A savage cold snap caught one of your bands in the open. Its food stores are gone and the weakest did not survive.";
            pressure("Bitter cold", PC_ECOLOGY, 15, 3, 6);
            break;
        default: {
            int ft = unitIndex(turn < dur(90) ? "Feral Dogs" : "Mutant Hounds");
            if (!warHappened) ft = unitIndex("Feral Dogs");
            int made = 0;
            for (int tries = 0; tries < 40 && made < 1; tries++) {
                auto cand = ring(bx, by, 5);
                Pt p = cand[rng.range((int)cand.size())];
                int d = dist(p.x, p.y, bx, by);
                if (d < 3 || isWater(p.x, p.y) || at(p.x, p.y).mountain || cityAt(p.x, p.y) || !unitsAt(p.x, p.y).empty()) continue;
                spawnUnit(CIV_FERAL, ft, p.x, p.y);
                made++;
            }
            if (!made) return false;
            title = "Predators";
            body = "A predator pack has picked up the scent of one of your bands. Fight them off or keep moving.";
            pressure("Hunted", PC_CONFLICT, 10, 2, 8);
            break;
        }
    }
    statDisasters++;
    cv.disastersSuffered++;
    notify(c, "Disaster: " + title + ".", bx, by);
    popup(c, POP_DISASTER, title, body + "\n\nEvolutionary pressure:" + ptext, bx, by);
    return true;
}
