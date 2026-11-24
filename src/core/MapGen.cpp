// Rebirth of Civilisation - new game setup and world generation
// SPDX-License-Identifier: MIT
#include "Game.h"

namespace {
struct Noise {
    int gw = 0, gh = 0;
    std::vector<double> v;
    void init(Rng& rng, int w, int h) {
        gw = w + 2; gh = h + 2;
        v.resize(gw * gh);
        for (auto& x : v) x = rng.unit();
    }
    static double sm(double t) { return t * t * (3 - 2 * t); }
    double at(double x, double y) const {
        int x0 = (int)x, y0 = (int)y;
        double fx = sm(x - x0), fy = sm(y - y0);
        x0 = std::clamp(x0, 0, gw - 2); y0 = std::clamp(y0, 0, gh - 2);
        double a = v[y0 * gw + x0], b = v[y0 * gw + x0 + 1];
        double c = v[(y0 + 1) * gw + x0], d = v[(y0 + 1) * gw + x0 + 1];
        return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
    }
};
}  // namespace

void Game::generateMap() {
    tiles.assign(W * H, Tile());
    Noise n1, n2, n3, moist, hillN;
    n1.init(rng, W / 7 + 2, H / 7 + 2);
    n2.init(rng, W / 3 + 2, H / 3 + 2);
    n3.init(rng, W / 2 + 2, H / 2 + 2);
    moist.init(rng, W / 5 + 2, H / 5 + 2);
    hillN.init(rng, W / 3 + 2, H / 3 + 2);
    std::vector<double> elev(W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double e = 0.6 * n1.at(x / 7.0, y / 7.0) + 0.3 * n2.at(x / 3.0, y / 3.0) + 0.1 * n3.at(x / 2.0, y / 2.0);
            double ex = std::min(x, W - 1 - x) / (W * 0.10);
            double ey = std::min(y, H - 1 - y) / (H * 0.10);
            double f = std::min(1.0, std::min(ex, ey));
            e -= (1 - f) * 0.45;
            elev[idx(x, y)] = e;
        }
    std::vector<double> sorted = elev;
    std::sort(sorted.begin(), sorted.end());
    double seaLevel = sorted[(size_t)(sorted.size() * 0.58)];
    double mountLevel = sorted[(size_t)(sorted.size() * 0.975)];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            Tile& t = at(x, y);
            double e = elev[idx(x, y)];
            if (e < seaLevel) { t.terrain = T_OCEAN; continue; }
            double lat = std::abs(y - (H - 1) / 2.0) / ((H - 1) / 2.0);
            double m = moist.at(x / 5.0, y / 5.0) + (rng.unit() - 0.5) * 0.12;
            if (lat > 0.86) t.terrain = T_SNOW;
            else if (lat > 0.70) t.terrain = T_TUNDRA;
            else if (m < 0.33 && lat > 0.12 && lat < 0.6) t.terrain = T_DESERT;
            else if (m < 0.5) t.terrain = T_PLAINS;
            else t.terrain = T_GRASS;
            if (t.terrain != T_SNOW && t.terrain != T_DESERT) {
                if (lat < 0.38 && m > 0.56) t.veg = V_JUNGLE;
                else if (m > 0.62 || (t.terrain == T_TUNDRA && m > 0.5)) t.veg = V_FOREST;
                else if (rng.chance(0.08)) t.veg = V_FOREST;
            }
            double h = hillN.at(x / 3.0, y / 3.0);
            if (e > mountLevel || (h > 0.83 && rng.chance(0.5))) { t.mountain = true; t.veg = V_NONE; }
            else if (h > 0.66 || rng.chance(0.05)) t.hills = true;
        }
    // coast
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (at(x, y).terrain != T_OCEAN) continue;
            Pt nb[6];
            int n = neighbors(x, y, nb);
            for (int k = 0; k < n; k++)
                if (!g_terrain[at(nb[k].x, nb[k].y).terrain].water) { at(x, y).terrain = T_COAST; break; }
        }
    // resources
    struct Odds { uint8_t r; int pct10; };  // chance in tenths of a percent
    auto roll = [&](Tile& t, std::initializer_list<Odds> table) {
        int v = rng.range(1000), acc = 0;
        for (auto& o : table) {
            acc += o.pct10;
            if (v < acc) { t.res = o.r; return; }
        }
    };
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            Tile& t = at(x, y);
            if (t.mountain || t.terrain == T_OCEAN) continue;
            double lat = std::abs(y - (H - 1) / 2.0) / ((H - 1) / 2.0);
            if (t.terrain == T_COAST) { roll(t, {{R_FISH, 90}}); continue; }
            if (t.hills) {
                roll(t, {{R_STONE, 45}, {R_IRON, 50}, {R_COPPER, 55}, {R_TIN, 40}, {R_COAL, 45}, {R_GOLD, 20}, {R_URANIUM, 15}});
                continue;
            }
            if (t.veg == V_JUNGLE) { roll(t, {{R_FRUIT, 110}, {R_RICE, 15}}); continue; }
            if (t.veg == V_FOREST) { roll(t, {{R_GAME, 95}, {R_COAL, 20}}); continue; }
            switch (t.terrain) {
                case T_GRASS:
                    if (lat < 0.45) roll(t, {{R_RICE, 50}, {R_FRUIT, 30}, {R_MAIZE, 30}, {R_HORSES, 25}, {R_COPPER, 10}});
                    else roll(t, {{R_WHEAT, 50}, {R_MAIZE, 25}, {R_HORSES, 35}, {R_FRUIT, 15}, {R_COPPER, 10}});
                    break;
                case T_PLAINS:
                    roll(t, {{R_WHEAT, 65}, {R_MAIZE, 30}, {R_HORSES, 60}, {R_STONE, 20}, {R_COPPER, 20}, {R_TIN, 10}});
                    break;
                case T_DESERT: roll(t, {{R_OIL, 70}, {R_GOLD, 20}, {R_URANIUM, 20}, {R_COPPER, 15}}); break;
                case T_TUNDRA: roll(t, {{R_GAME, 70}, {R_OIL, 40}, {R_TIN, 25}, {R_URANIUM, 20}, {R_COAL, 20}}); break;
                default: break;
            }
        }
}

// Make sure every start can reach the key strategic resources, so nobody is locked out of bronze or iron.
static void ensureNear(Game& g, Pt s, int res, int maxD) {
    for (auto& p : g.ring(s.x, s.y, maxD))
        if (g.at(p.x, p.y).res == res) return;
    std::vector<Pt> spots;
    for (auto& p : g.ring(s.x, s.y, maxD)) {
        int d = g.dist(s.x, s.y, p.x, p.y);
        const Tile& t = g.at(p.x, p.y);
        if (d < 2 || g.isWater(p.x, p.y) || t.mountain || t.res != R_NONE || t.site >= 0 || t.owner >= 0) continue;
        spots.push_back(p);
    }
    if (spots.empty()) return;
    Pt p = spots[g.rng.range((int)spots.size())];
    Tile& t = g.at(p.x, p.y);
    t.res = (uint8_t)res;
    if (res == R_HORSES && t.veg != V_NONE) t.veg = V_NONE;
}

void Game::placeHumanity() {
    static const int CITY_COUNT[] = {8, 12, 16};
    int nCities = CITY_COUNT[settings.mapSize];
    std::vector<std::string> names = g_humanCityNames;
    for (size_t i = names.size(); i > 1; i--) std::swap(names[i - 1], names[rng.range((int)i)]);
    auto okTile = [&](int x, int y) {
        const Tile& t = at(x, y);
        return !isWater(x, y) && !t.mountain && t.terrain != T_SNOW && t.site < 0;
    };
    auto farFromSites = [&](int x, int y, int d) {
        for (auto& s : sites) if (dist(s.x, s.y, x, y) < d) return false;
        return true;
    };
    for (int minD = 8; minD >= 3 && (int)sites.size() < nCities; minD--) {
        for (int tries = 0; tries < 4000 && (int)sites.size() < nCities; tries++) {
            int x = rng.range(W), y = rng.range(H);
            if (!okTile(x, y) || !farFromSites(x, y, minD)) continue;
            const Tile& t = at(x, y);
            if (t.terrain == T_TUNDRA && rng.chance(0.7)) continue;
            HumanSite s;
            s.name = names[sites.size() % names.size()];
            s.x = x; s.y = y;
            s.type = RUIN_CITY;
            s.population = rng.between(900, 24000);
            sites.push_back(s);
        }
    }
    // smaller structures
    struct Minor { int type; int count; };
    Minor minors[] = {{RUIN_TOWN, nCities}, {RUIN_MILITARY, nCities / 2 + 1}, {RUIN_LAB, nCities / 3 + 1}, {RUIN_POWER, nCities / 3 + 1}};
    int townNo = 1;
    for (auto& m : minors) {
        int placed = 0;
        for (int tries = 0; tries < 3000 && placed < m.count; tries++) {
            int x = rng.range(W), y = rng.range(H);
            if (!okTile(x, y) || !farFromSites(x, y, 4)) continue;
            HumanSite s;
            s.x = x; s.y = y; s.type = m.type;
            s.population = m.type == RUIN_TOWN ? rng.between(20, 400) : 0;
            switch (m.type) {
                case RUIN_TOWN: s.name = "Town " + std::to_string(townNo++); break;
                case RUIN_MILITARY: s.name = "Military Base"; break;
                case RUIN_LAB: s.name = "Research Laboratory"; break;
                default: s.name = "Nuclear Power Plant"; break;
            }
            sites.push_back(s);
            placed++;
        }
    }
    // name towns after nearest city
    for (auto& s : sites) {
        if (s.type == RUIN_CITY) continue;
        const HumanSite* best = nullptr;
        for (auto& c : sites)
            if (c.type == RUIN_CITY && (!best || dist(c.x, c.y, s.x, s.y) < dist(best->x, best->y, s.x, s.y))) best = &c;
        if (best && s.type == RUIN_TOWN) s.name = best->name + " Outskirts";
        else if (best) s.name = s.name + " near " + best->name;
    }
    for (size_t i = 0; i < sites.size(); i++) {
        HumanSite& s = sites[i];
        Tile& t = at(s.x, s.y);
        t.site = (int16_t)i;
        t.ruin = (uint8_t)s.type;
        t.veg = V_NONE;
        t.res = R_NONE;
        int r = s.type == RUIN_CITY ? 2 : 1;
        for (auto& p : ring(s.x, s.y, r))
            if (!isWater(p.x, p.y)) at(p.x, p.y).owner = CIV_HUMANS;
        if (s.type == RUIN_CITY)
            for (auto& p : ring(s.x, s.y, 1)) {
                Tile& tt = at(p.x, p.y);
                if (!isWater(p.x, p.y) && !tt.mountain && tt.veg != V_NONE && rng.chance(0.6)) tt.veg = V_NONE;
                if (!isWater(p.x, p.y) && !tt.mountain && tt.site < 0 && rng.chance(0.5)) tt.imp = IMP_FARM;
            }
    }
}

std::vector<Pt> Game::chooseStarts(int count) {
    std::vector<Pt> starts;
    int minD = (int)(std::sqrt((double)W * H / count) * 0.75);
    for (int ci = 0; ci < count; ci++) {
        int civIdx = FIRST_PRIMATE_CIV + ci;
        const PrimateDef& pd = g_primates[civs[civIdx].primate];
        Pt best = {-1, -1};
        for (int relax = 0; relax < 8 && best.x < 0; relax++) {
            int bestScore = -1;
            int dStarts = std::max(4, minD - relax * 2);
            int dCity = std::max(3, 7 - relax);
            for (int y = 2; y < H - 2; y++)
                for (int x = 2; x < W - 2; x++) {
                    const Tile& t = at(x, y);
                    if (isWater(x, y) || t.mountain || t.terrain == T_SNOW || t.site >= 0 || t.owner >= 0) continue;
                    bool ok = true;
                    for (auto& s : starts) if (dist(s.x, s.y, x, y) < dStarts) { ok = false; break; }
                    if (!ok) continue;
                    for (auto& s : sites)
                        if (dist(s.x, s.y, x, y) < (s.type == RUIN_CITY ? dCity : 4)) { ok = false; break; }
                    if (!ok) continue;
                    int land = 0, sc = 0, bias = 0;
                    for (auto& p : ring(x, y, 2)) {
                        const Tile& tt = at(p.x, p.y);
                        if (!isWater(p.x, p.y) && !tt.mountain) land++;
                        Yields yy = tileYields(-1, p.x, p.y);
                        sc += yy.food * 3 + yy.prod * 2;
                        switch (pd.startBias) {
                            case 0: if (tt.veg == V_JUNGLE) bias += 6; break;
                            case 1: if (tt.veg == V_FOREST || tt.hills) bias += 6; break;
                            case 2: if ((tt.terrain == T_PLAINS || tt.terrain == T_DESERT) && tt.veg == V_NONE) bias += 5; break;
                            case 3: if (tt.terrain == T_COAST) bias += 6; break;
                            case 4: if (tt.terrain == T_TUNDRA || tt.terrain == T_SNOW) bias += 6; break;
                        }
                    }
                    if (land < 12) continue;
                    if (t.terrain == T_DESERT || t.terrain == T_SNOW) sc -= 10;
                    int total = sc + bias + rng.range(12);
                    if (total > bestScore) { bestScore = total; best = {x, y}; }
                }
        }
        if (best.x < 0) {  // desperate fallback
            for (int tries = 0; tries < 10000; tries++) {
                int x = rng.range(W), y = rng.range(H);
                if (!isWater(x, y) && !at(x, y).mountain && at(x, y).site < 0) { best = {x, y}; break; }
            }
        }
        starts.push_back(best);
    }
    return starts;
}

void revealAround(Game& g, int c, int x, int y, int r);

void Game::newGame(const GameSettings& s) {
    initData();
    settings = s;
    settings.speed = std::clamp(s.speed, 0, 2);
    speedPct = SPEED_PCT[settings.speed];
    costPct = SPEED_COST[settings.speed];
    maxTurns = SPEED_TURNS[settings.speed];
    rng.seed(s.seed * 2654435761ull + 12345);
    static const int SW[] = {44, 56, 70}, SH[] = {28, 36, 44}, NCIV[] = {4, 6, 8};
    W = SW[s.mapSize];
    H = SH[s.mapSize];
    int nPrim = NCIV[s.mapSize];
    turn = 1;
    yearAF = 0;
    warHappened = false;
    gameOver = false;
    winner = -1;
    victoryType.clear();
    log.clear();
    popups.clear();
    units.clear();
    cities.clear();
    sites.clear();
    wonderCity.assign(g_buildings.size(), -1);
    generateMap();

    int total = FIRST_PRIMATE_CIV + nPrim;
    civs.assign(total, Civ());
    std::vector<int> pool;
    for (int p = 0; p < (int)g_primates.size(); p++) if (p != s.primate) pool.push_back(p);
    for (size_t i = pool.size(); i > 1; i--) std::swap(pool[i - 1], pool[rng.range((int)i)]);
    for (int c = 0; c < total; c++) {
        Civ& cv = civs[c];
        cv.id = c;
        cv.techs.assign(g_techs.size(), 0);
        cv.techProgress.assign(g_techs.size(), 0);
        cv.policies.assign(g_policies.size(), 0);
        cv.explored.assign(W * H, 0);
        cv.visible.assign(W * H, 0);
        cv.war.assign(total, 0);
        cv.met.assign(total, 0);
        cv.warSince.assign(total, 0);
        cv.grievance.assign(total, 0);
        cv.goodwill.assign(total, 0);
        cv.peaceUntil.assign(total, 0);
        cv.denouncedUntil.assign(total, 0);
        cv.lastOffer.assign(total, -100);
        cv.traits.assign(g_traits.size(), 0);
        cv.evoPool.assign(PC_COUNT, 0);
        cv.evoRate.assign(PC_COUNT, 0);
        cv.evoGoal = TR_NEOCORTEX;
        if (c == CIV_HUMANS) {
            cv.humans = true; cv.name = "Humanity"; cv.adjective = "Human"; cv.leader = "The Nations of Earth";
            cv.color = rgb(200, 200, 205);
        } else if (c == CIV_FERAL) {
            cv.feral = true; cv.name = "Feral Packs"; cv.adjective = "Feral"; cv.leader = "";
            cv.color = rgb(120, 20, 20);
        } else {
            cv.primate = c == FIRST_PRIMATE_CIV ? s.primate : pool[(c - FIRST_PRIMATE_CIV - 1) % pool.size()];
            const PrimateDef& pd = g_primates[cv.primate];
            cv.name = pd.plural; cv.adjective = pd.name; cv.leader = pd.leader; cv.color = pd.color;
            cv.player = (c == FIRST_PRIMATE_CIV) && !s.allAI;
            cv.aiSkill = std::clamp(s.difficulty, 0, 3);  // difficulty = how cleverly the AI plays, never free yields
        }
    }
    player = FIRST_PRIMATE_CIV;
    placeHumanity();
    std::vector<Pt> starts = chooseStarts(nPrim);
    for (Pt st : starts) {
        ensureNear(*this, st, R_COPPER, 5);
        ensureNear(*this, st, R_TIN, 6);
        ensureNear(*this, st, R_HORSES, 6);
        ensureNear(*this, st, R_IRON, 7);
        ensureNear(*this, st, R_COAL, 8);
        if (rng.chance(0.5)) ensureNear(*this, st, R_WHEAT, 4);
        else ensureNear(*this, st, R_RICE, 4);
    }
    markResourcesDirty();
    for (int i = 0; i < nPrim; i++) {
        int c = FIRST_PRIMATE_CIV + i;
        Pt p = starts[i];
        int bid = spawnUnit(c, ID.uBand, p.x, p.y);
        units[bid].band = 4;
        spawnUnit(c, uniqueFor(c, ID.uWarband), p.x, p.y);
        // scout on a neighbouring land tile
        Pt nb[6];
        int n = neighbors(p.x, p.y, nb);
        Pt sp = p;
        for (int k = 0; k < n; k++)
            if (!isWater(nb[k].x, nb[k].y) && !at(nb[k].x, nb[k].y).mountain) { sp = nb[k]; break; }
        spawnUnit(c, uniqueFor(c, ID.uScout), sp.x, sp.y);
        if (sp.x == p.x && sp.y == p.y) units.back().alive = false;  // nowhere to put it
        // everyone can see the lights of the human cities
        for (auto& s2 : sites)
            if (s2.type == RUIN_CITY)
                for (auto& q : ring(s2.x, s2.y, 1)) civs[c].explored[idx(q.x, q.y)] = 1;
    }
    updateAllVisibility();
    const PrimateDef& pp = g_primates[s.primate];
    popup(player, POP_INTRO, "Rebirth of Civilisation",
          "January 2032. On the horizon the lights of the human cities still burn, but their radios speak of nothing but war.\n\n"
          "You lead the " + pp.plural + " under " + pp.leader + ". " + pp.flavor + "\n\n"
          "Keep your troop away from the human cities. When the end comes - and it will come soon - the survivors will inherit the Earth, "
          "and the wreckage of humanity will one day reveal its secrets to those wise enough to understand them.\n\n"
          "Your species cannot yet learn deliberately, let alone speak: until its brain evolves an Enlarged Neocortex it earns no science at all. The first discoveries will be simple - fire, foraging, stone tools - and real science needs language. Evolution is driven by four kinds of pressure - Social, Ecological, Conflict and Mutation. Choose which trait to evolve towards on the Evolution screen (V): the language trunk leads to Speech and Advanced Speech, while the Tool, Social, Predator and Mutant strands each need their own kind of pressure.\n\n"
          "Your people are wanderers. Your Roaming Band forages the land around it, but picked-over land takes time to recover: keep the band on the move "
          "to fresh fruit and forest. Well-fed bands grow, and large bands can split in two or send members off as war parties and scouts. "
          "Only when your species has evolved Speech and discovered Cultivation can a band settle down and found your first city.");
    notify(player, "The year is 2032. Humanity is on the brink of nuclear war.");
}
