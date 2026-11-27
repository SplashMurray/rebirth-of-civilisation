// Rebirth of Civilisation - religion
// Shrines, Temples and Cathedrals (and natural wonders) give Faith. With enough Faith, and Symbols, a people
// may found one of a few religions, choosing a follower belief (for every city that follows it) and a founder
// belief (for themselves); with Theology and more Faith they may enhance it with two more. Religions spread
// on their own - every city that follows one presses it on the cities within ten tiles, and trade routes carry
// it - and Missionaries, bought with Faith, preach it in a city to win many followers at once. Faith also pays
// for holy festivals (+2 Happiness everywhere for a while).
//
// The price of devotion: zeal. A people whose Faith is a large share of their Faith and Science together, or
// whose religion is dogmatic, resists ideas that contradict the faith - as religion has resisted science
// through history, and still does. Zeal above 25 costs Science: three quarters of a percent per point,
// up to half. Scholarly beliefs and the Free Thought tradition temper it.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

const BeliefDef g_beliefs[BL_COUNT] = {
    {"Communal Prayer", false, 0, "+1 Happiness in each city following the religion"},
    {"Sacred Groves", false, 0, "+2 Food in each city following the religion"},
    {"Temple Builders", false, 0, "+2 Production in each city following the religion that has a Shrine"},
    {"Dawn Chorus", false, 0, "+2 Culture in each city following the religion"},
    {"Monastic Scribes", false, -10, "+2 Science in each city following the religion that has a Temple. A scholarly faith: -10 zeal"},
    {"Devout Masses", false, 10, "+2 Faith in each city following the religion. +10 zeal"},
    {"Warrior Monks", false, 5, "Military units trained in a city following the religion start with 10 experience. +5 zeal"},
    {"Tithes", true, 0, "Its founders get +1 Gold for every city following the religion"},
    {"Pilgrimage", true, 0, "Its founders get +1 Faith for every city following the religion"},
    {"World Church", true, 0, "Its founders get +1 Culture for every city following the religion"},
    {"Holy Warriors", true, 10, "Its founders' units fight +10% harder within 2 tiles of a city following the religion. +10 zeal"},
    {"Orthodoxy", true, 15, "Other religions spread into its founders' cities at a quarter of their strength. +15 zeal"},
    {"Inquisition", true, 25, "Its Missionaries win almost twice the followers and drive out half of every rival faith. +25 zeal"},
    {"Natural Philosophy", true, -15, "Its founders get +1 Science for every city following the religion. Faith and reason at peace: -15 zeal"},
};

const char* RELIGION_NAMES[] = {"The Way of the Canopy", "Children of the Sun", "The Burning Sky", "Faith of the Returning Rain", "The Silent Tree",
                                "Order of the Ember", "The Moon Chorus", "Keepers of the Old Ones", "The Deep River Path", "Church of the Last Light"};
const int NUM_RELIGION_NAMES = 10;

static int techSymbols() { static int t = techIndex("Symbols"); return t; }
static int techTheology() { static int t = techIndex("Theology"); return t; }
static int bShrine() { static int b = buildingIndex("Shrine"); return b; }
static int bTemple() { static int b = buildingIndex("Temple"); return b; }
static int bCathedral() { static int b = buildingIndex("Cathedral"); return b; }

int Game::maxReligions() const { return std::min(MAX_RELIGIONS, (numMajors() + 1) / 2 + 1); }
int Game::foundCost() const { return sc(90); }
int Game::enhanceCost() const { return sc(300); }
int Game::missionaryCost(int c) const { return sc(70) + sc(10) * std::min(10, civs[c].missionariesBought); }
int Game::festivalCost(int c) const { return sc(120) + sc(30) * std::min(8, civs[c].festivalsHeld); }

bool Game::beliefTaken(int b) const {
    for (auto& r : religions) for (int x : r.beliefs) if (x == b) return true;
    return false;
}
bool Game::hasBelief(int r, int b) const {
    if (r < 0 || r >= (int)religions.size()) return false;
    for (int x : religions[r].beliefs) if (x == b) return true;
    return false;
}
bool Game::founderHas(int c, int b) const {
    return c >= 0 && c < (int)civs.size() && hasBelief(civs[c].founded, b);
}
int Game::stateReligion(int c) const {
    if (c < 0 || c >= (int)civs.size() || civs[c].capital < 0 || civs[c].capital >= (int)cities.size()) return -1;
    const City& cap = cities[civs[c].capital];
    return cap.alive && cap.owner == c ? cap.religion : -1;
}
int Game::holyCityReligion(const City& ci) const {
    for (size_t r = 0; r < religions.size(); r++) if (religions[r].holyCity == ci.id) return (int)r;
    return -1;
}

bool Game::canFoundReligion(int c, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    if (!isMajor(c) || !civs[c].alive) return fail("Only the great peoples found religions");
    if (civs[c].founded >= 0) return fail("Your people already have a religion of their own");
    if ((int)religions.size() >= maxReligions()) return fail("Every religion this world has room for has been founded");
    if (!hasTech(c, techSymbols())) return fail("Discover Symbols first");
    if (numCities(c) == 0) return fail("A religion needs a city for its holy city");
    if (civs[c].faith < foundCost()) return fail("Needs " + std::to_string(foundCost()) + " Faith (you have " + std::to_string(civs[c].faith) + ")");
    return true;
}

bool Game::canEnhanceReligion(int c, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    int r = civs[c].founded;
    if (r < 0) return fail("Found a religion first");
    if (religions[r].enhanced) return fail("Your religion is already enhanced");
    if (!hasTech(c, techTheology())) return fail("Discover Theology first");
    if (civs[c].faith < enhanceCost()) return fail("Needs " + std::to_string(enhanceCost()) + " Faith (you have " + std::to_string(civs[c].faith) + ")");
    return true;
}

bool Game::foundReligion(int c, int nameIdx, int follower, int founderBelief) {
    if (!canFoundReligion(c)) return false;
    if (follower < 0 || follower >= BL_COUNT || g_beliefs[follower].founder || beliefTaken(follower)) return false;
    if (founderBelief < 0 || founderBelief >= BL_COUNT || !g_beliefs[founderBelief].founder || beliefTaken(founderBelief)) return false;
    for (auto& r : religions) if (r.nameIdx == nameIdx) return false;
    if (nameIdx < 0 || nameIdx >= NUM_RELIGION_NAMES) return false;
    // the holy city: the most devout of their cities
    City* holy = nullptr;
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c && (!holy || ci.yields.faith > holy->yields.faith || (ci.yields.faith == holy->yields.faith && ci.capital)))
            holy = &ci;
    if (!holy) return false;
    civs[c].faith -= foundCost();
    Religion rel;
    rel.name = RELIGION_NAMES[nameIdx];
    rel.nameIdx = nameIdx;
    rel.founder = c;
    rel.holyCity = holy->id;
    rel.founded = turn;
    rel.beliefs = {follower, founderBelief};
    religions.push_back(rel);
    int r = (int)religions.size() - 1;
    civs[c].founded = r;
    for (auto& ci : cities) if (ci.alive) ci.pressure.resize(religions.size(), 0);
    holy->pressure[r] = 300;
    convertCity(*holy, r, false);
    notify(c, "You have founded " + rel.name + ". " + holy->name + " is its holy city.", holy->x, holy->y);
    for (int v : viewers())
        if (v != c && civs[v].met[c]) notify(v, "The " + civs[c].name + " have founded a religion: " + rel.name + ".", holy->x, holy->y);
    religions[r].followers = 1;
    return true;
}

bool Game::enhanceReligion(int c, int follower, int founderBelief) {
    if (!canEnhanceReligion(c)) return false;
    if (follower < 0 || follower >= BL_COUNT || g_beliefs[follower].founder || beliefTaken(follower)) return false;
    if (founderBelief < 0 || founderBelief >= BL_COUNT || !g_beliefs[founderBelief].founder || beliefTaken(founderBelief)) return false;
    Religion& rel = religions[civs[c].founded];
    civs[c].faith -= enhanceCost();
    rel.enhanced = true;
    rel.beliefs.push_back(follower);
    rel.beliefs.push_back(founderBelief);
    notify(c, rel.name + " is enhanced: " + g_beliefs[follower].name + " and " + g_beliefs[founderBelief].name + ".");
    return true;
}

// ------------------------------------------------------------ missionaries and festivals
bool Game::buyMissionary(int c, std::string* why) {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    int r = civs[c].founded >= 0 ? civs[c].founded : stateReligion(c);
    if (r < 0) return fail("Your people follow no religion to spread");
    if (civs[c].faith < missionaryCost(c)) return fail("Needs " + std::to_string(missionaryCost(c)) + " Faith");
    // trained in the holy city if it is ours, else in one of our cities that follows the faith
    const City* at = nullptr;
    if (religions[r].holyCity >= 0) {
        const City& h = cities[religions[r].holyCity];
        if (h.alive && h.owner == c) at = &h;
    }
    if (!at) for (auto& ci : cities) if (ci.alive && ci.owner == c && ci.religion == r && (!at || ci.capital)) at = &ci;
    if (!at) return fail("None of your cities follows " + religions[r].name);
    civs[c].faith -= missionaryCost(c);
    civs[c].missionariesBought++;
    int id = spawnUnit(c, ID.uMissionary, at->x, at->y);
    if (id < 0) return fail("No room for a Missionary");
    units[id].religion = r;
    units[id].name = "Missionary of " + religions[r].name;
    notify(c, "A Missionary of " + religions[r].name + " sets out from " + at->name + ".", at->x, at->y);
    return true;
}

City* Game::preachTarget(const Unit& u) const {
    if (u.religion < 0 || u.religion >= (int)religions.size()) return nullptr;
    const City* best = nullptr;
    for (auto& ci : cities) {
        if (!ci.alive || dist(ci.x, ci.y, u.x, u.y) > 1 || ci.religion == u.religion) continue;
        if (atWar(u.owner, ci.owner)) continue;
        int hr = holyCityReligion(ci);
        if (hr >= 0 && hr != u.religion) continue;  // a holy city keeps its faith
        if (!best || (ci.x == u.x && ci.y == u.y)) best = &ci;
    }
    return const_cast<City*>(best);
}

bool Game::canPreach(const Unit& u, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    if (u.type != ID.uMissionary || u.religion < 0) return fail("Only Missionaries preach");
    if (u.moves <= 0) return fail("No moves left");
    if (!preachTarget(u)) return fail("Stand in or next to a city that does not yet follow " + religions[u.religion].name +
                                       " (not a rival's holy city, nor a people you are at war with)");
    return true;
}

bool Game::preach(Unit& u) {
    if (!canPreach(u)) return false;
    City* t = preachTarget(u);
    int r = u.religion;
    if ((int)t->pressure.size() < (int)religions.size()) t->pressure.resize(religions.size(), 0);
    bool inq = hasBelief(r, BL_INQUISITION);
    t->pressure[r] += inq ? 450 : 250;
    if (inq)
        for (int o = 0; o < (int)religions.size(); o++) if (o != r) t->pressure[o] /= 2;
    int best = 0;
    for (int o = 0; o < (int)religions.size(); o++) if (o != r) best = std::max(best, t->pressure[o]);
    std::string msg = "A Missionary preached " + religions[r].name + " in " + t->name + ".";
    if (t->pressure[r] > best) convertCity(*t, r, true);
    else msg += " Many listened, but the old faith still holds.";
    notify(u.owner, msg, t->x, t->y);
    if (t->owner != u.owner) notify(t->owner, "Missionaries of " + religions[r].name + " have been preaching in " + t->name + ".", t->x, t->y);
    killUnit(u);
    return true;
}

// Faith can finish a building (not a wonder): the devout give their labour. A zealous people (zeal 40+) will pay
// for units too - soldiers of the faith - and the more zealous, the cheaper (not nuclear warheads).
int Game::faithBuyCost(const City& city) const {
    if (city.buildKind == BK_BUILDING) {
        if (g_buildings[city.buildId].wonder) return 0;
        int remaining = std::max(0, buildCost(city.owner, BK_BUILDING, city.buildId) - city.prod);
        return remaining > 0 ? remaining * 3 + 30 : 0;
    }
    if (city.buildKind == BK_UNIT) {
        int z = civs[city.owner].zeal;
        if (z < ZEAL_UNITS || g_units[city.buildId].cls == UnitClass::Nuke) return 0;
        int remaining = std::max(0, buildCost(city.owner, BK_UNIT, city.buildId) - city.prod);
        int pct = std::clamp(300 - (z - ZEAL_UNITS) * 5 / 2, 200, 300);
        return remaining > 0 ? remaining * pct / 100 + 20 : 0;
    }
    return 0;
}

bool Game::buyWithFaith(City& city) {
    int cost = faithBuyCost(city);
    Civ& cv = civs[city.owner];
    if (cost <= 0 || cv.faith < cost) return false;
    cv.faith -= cost;
    city.prod = buildCost(city.owner, city.buildKind, city.buildId);
    return true;
}

bool Game::canHoldFestival(int c, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    if (!isMajor(c) || numCities(c) == 0) return fail("You have no cities");
    if (civs[c].festivalUntil >= turn) return fail("A festival is already under way");
    if (civs[c].faith < festivalCost(c)) return fail("Needs " + std::to_string(festivalCost(c)) + " Faith");
    return true;
}

bool Game::holdFestival(int c) {
    if (!canHoldFestival(c)) return false;
    civs[c].faith -= festivalCost(c);
    civs[c].festivalsHeld++;
    civs[c].festivalUntil = turn + dur(10);
    notify(c, "A holy festival begins: +2 Happiness in every city for " + std::to_string(dur(10)) + " turns.");
    return true;
}

// ------------------------------------------------------------ spreading
int Game::religionPressure(const City& from, const City& to, int r) const {
    int d = dist(from.x, from.y, to.x, to.y);
    if (d > 10 || from.id == to.id) return 0;
    int base = religions[r].holyCity == from.id ? 8 : 3;
    if (from.has[bTemple()]) base += 2;
    if (from.has[bCathedral()]) base += 3;
    int p = std::max(1, base * (12 - d) / 10);
    int f = religions[r].founder;
    if (to.owner != f && to.owner >= 0 && founderHas(to.owner, BL_ORTHODOXY) && civs[to.owner].founded != r) p = std::max(1, p / 4);
    if (atWar(from.owner, to.owner)) p = std::max(1, p / 2);
    return p;
}

void Game::convertCity(City& ci, int r, bool announce) {
    int old = ci.religion;
    if (old == r) return;
    ci.religion = r;
    for (int o = 0; o < (int)ci.pressure.size(); o++) if (o != r) ci.pressure[o] /= 2;
    if (!announce) return;
    notify(ci.owner, ci.name + " has turned to " + religions[r].name + (old >= 0 ? ", leaving " + religions[old].name : "") + ".", ci.x, ci.y);
    int f = religions[r].founder;
    if (f != ci.owner && f >= 0) notify(f, ci.name + " (" + civs[ci.owner].adjective + ") has turned to your religion, " + religions[r].name + ".", ci.x, ci.y);
}

void Game::religionTurn() {
    int R = (int)religions.size();
    if (R == 0) return;
    for (auto& ci : cities) {
        if (!ci.alive) continue;
        ci.pressure.resize(R, 0);
        for (int& p : ci.pressure) p = p * 96 / 100;
    }
    // every faithful city presses its faith on its neighbours, and holds on to it at home
    for (auto& s : cities) {
        if (!s.alive || s.religion < 0) continue;
        int r = s.religion;
        s.pressure[r] += 6;
        for (auto& t : cities)
            if (t.alive && t.id != s.id && std::abs(t.x - s.x) <= 12) t.pressure[r] += religionPressure(s, t, r);
    }
    // trade routes carry it further
    for (auto& rt : routes) {
        if (rt.from < 0 || rt.to < 0 || rt.from >= (int)cities.size() || rt.to >= (int)cities.size()) continue;
        City& a = cities[rt.from];
        City& b = cities[rt.to];
        if (!a.alive || !b.alive) continue;
        if (a.religion >= 0) b.pressure[a.religion] += 5;
        if (b.religion >= 0) a.pressure[b.religion] += 5;
    }
    for (auto& r : religions) r.followers = 0;
    for (auto& t : cities) {
        if (!t.alive) continue;
        for (int& p : t.pressure) p = std::min(p, 3000);
        int hr = holyCityReligion(t);
        if (hr >= 0) {  // a holy city keeps its faith
            if (t.religion != hr) convertCity(t, hr, true);
        } else {
            int best = -1;
            for (int r = 0; r < R; r++) if (best < 0 || t.pressure[r] > t.pressure[best]) best = r;
            if (best >= 0 && best != t.religion) {
                bool conv = t.religion < 0 ? t.pressure[best] >= 50 + t.pop * 6 : t.pressure[best] > t.pressure[t.religion] * 13 / 10 + 40;
                if (conv) convertCity(t, best, true);
            }
        }
        if (t.religion >= 0) religions[t.religion].followers++;
    }
}

// ------------------------------------------------------------ zeal
int Game::zealOf(int c, int* share, int* beliefs, int* traditions) const {
    const Civ& cv = civs[c];
    int f2 = 2 * std::max(0, cv.faithPerTurn);  // faith counts double: devotion weighs on minds more than its numbers say
    int s = 100 * f2 / (f2 + std::max(0, cv.sciRawPerTurn) + 5);
    int b = 0;
    int st = stateReligion(c);
    if (st >= 0) for (int x : religions[st].beliefs) if (!g_beliefs[x].founder) b += g_beliefs[x].zeal;
    if (cv.founded >= 0) for (int x : religions[cv.founded].beliefs) if (g_beliefs[x].founder) b += g_beliefs[x].zeal;
    static int freeThought = policyIndex("Free Thought");
    int t = freeThought >= 0 && freeThought < (int)cv.policies.size() && cv.policies[freeThought] ? -10 : 0;
    if (resolutionActive(RS_SECULAR)) t -= 10;  // the World Council's Secular Accord
    if (share) *share = s;
    if (beliefs) *beliefs = b;
    if (traditions) *traditions = t;  // traditions and accords
    return std::max(0, s + b + t);
}

int Game::zealToPenalty(int zeal) { return std::clamp((zeal - 25) * 3 / 4, 0, 50); }

bool Game::nearFaithful(int c, int x, int y) const {
    int r = civs[c].founded;
    if (r < 0) return false;
    for (auto& ci : cities)
        if (ci.alive && ci.religion == r && dist(ci.x, ci.y, x, y) <= 2) return true;
    return false;
}

// ------------------------------------------------------------ the AI
int Game::aiPickBelief(int c, bool founder) const {
    const PrimateDef* pd = primate(c);
    int piety = pd ? pd->piety : 5, aggr = pd ? pd->aggression : 5;
    int sk = skillOf(c);
    int best = -1;
    double bv = -1e9;
    for (int b = 0; b < BL_COUNT; b++) {
        if (g_beliefs[b].founder != founder || beliefTaken(b)) continue;
        double v = 5;
        switch (b) {
            case BL_PRAYER: v = 6; break;
            case BL_GROVES: v = 6; break;
            case BL_BUILDERS: v = 5; break;
            case BL_SONGS: v = 5; break;
            case BL_SCRIBES: v = 5 + (10 - piety) * 0.2; break;
            case BL_DEVOUT: v = 3 + piety * 0.5; break;
            case BL_MONKS: v = 2 + aggr * 0.5; break;
            case BL_TITHES: v = 6; break;
            case BL_PILGRIMS: v = 4 + piety * 0.4; break;
            case BL_WORLD_CHURCH: v = 5; break;
            case BL_HOLY_WARRIORS: v = 2 + aggr * 0.5; break;
            case BL_ORTHODOXY: v = 2 + piety * 0.5; break;
            case BL_INQUISITION: v = 1 + piety * 0.7; break;
            case BL_NATURAL_PHIL: v = 7 - piety * 0.4; break;
        }
        // a clever people weighs what zeal would cost its science; a pious one cares less
        v -= g_beliefs[b].zeal * (sk >= 2 ? 0.05 * (10 - piety) : 0.04);
        v += (sk == 0 ? 3.0 : sk == 1 ? 1.5 : 0.6) * const_cast<Game*>(this)->rng.unit();
        if (v > bv) { bv = v; best = b; }
    }
    return best;
}

void Game::aiReligion(int c) {
    Civ& cv = civs[c];
    const PrimateDef* pd = primate(c);
    int piety = pd ? pd->piety : 5;
    if (canFoundReligion(c)) {
        std::vector<int> names;
        for (int n = 0; n < NUM_RELIGION_NAMES; n++) {
            bool used = false;
            for (auto& r : religions) if (r.nameIdx == n) used = true;
            if (!used) names.push_back(n);
        }
        int f = aiPickBelief(c, false), g = aiPickBelief(c, true);
        if (!names.empty() && f >= 0 && g >= 0) foundReligion(c, names[rng.range((int)names.size())], f, g);
    }
    if (canEnhanceReligion(c) && (piety >= 4 || cv.faith >= enhanceCost() * 3 / 2)) {
        int f = aiPickBelief(c, false), g = aiPickBelief(c, true);
        // a clever, worldly people would rather not enhance than take on a dogma that would cost it its science
        bool tooZealous = f >= 0 && g >= 0 && skillOf(c) >= 2 && g_beliefs[f].zeal + g_beliefs[g].zeal > piety * 3;
        if (f >= 0 && g >= 0 && !tooZealous) enhanceReligion(c, f, g);
    }
    // a holy festival when the people are restless
    int sad = 0, n = 0;
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c) { n++; if (ci.happy - ci.unhappy < 0) sad++; }
    bool slotsLeft = (int)religions.size() < maxReligions() && cv.founded < 0;
    if (sad > 0 && sad * 3 >= n && canHoldFestival(c) && (!slotsLeft || cv.faith >= foundCost() + festivalCost(c))) holdFestival(c);
    // missionaries
    int r = cv.founded;
    if (r < 0 && piety >= 7) r = stateReligion(c);
    if (r >= 0) {
        int mine = 0;
        for (auto& u : units) if (u.alive && u.owner == c && u.type == ID.uMissionary) mine++;
        bool target = false;
        for (auto& ci : cities)
            if (ci.alive && ci.religion != r && !atWar(c, ci.owner) && holyCityReligion(ci) < 0 && civs[c].explored[idx(ci.x, ci.y)]) { target = true; break; }
        bool enhancePending = cv.founded >= 0 && !religions[cv.founded].enhanced && hasTech(c, techTheology());
        int keep = enhancePending ? enhanceCost() : 0;
        if (target && mine < 2 && cv.faith - keep >= missionaryCost(c) && (piety >= 4 || cv.faith - keep >= missionaryCost(c) * 2)) buyMissionary(c);
    }
    // spare Faith raises buildings: keep enough for what still matters
    {
        int reserve = 0;
        if (slotsLeft && hasTech(c, techSymbols())) reserve = foundCost();
        if (cv.founded >= 0 && !religions[cv.founded].enhanced) reserve = std::max(reserve, enhanceCost());
        if (r >= 0) reserve += missionaryCost(c);
        bool war = false;
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (cv.war[o] && civs[o].alive && isMajor(o)) war = true;
        for (int guard = 0; guard < 3; guard++) {
            City* best = nullptr;
            for (auto& ci : cities)
                if (ci.alive && ci.owner == c && faithBuyCost(ci) > 0 && cv.faith - faithBuyCost(ci) >= reserve &&
                    (ci.buildKind == BK_BUILDING || (war && g_units[ci.buildId].military())) &&  // soldiers of the faith in wartime
                    (!best || faithBuyCost(ci) > faithBuyCost(*best)))
                    best = &ci;
            if (!best || !buyWithFaith(*best)) break;
        }
    }
    // send them to the nearest city that does not follow their faith (our own first)
    size_t nu = units.size();
    for (size_t i = 0; i < nu; i++) {
        if (!units[i].alive || units[i].owner != c || units[i].type != ID.uMissionary) continue;
        Unit& u = units[i];
        if (u.moves <= 0) continue;
        if (canPreach(u)) { preach(u); continue; }
        const City* best = nullptr;
        int bd = 1 << 30;
        for (auto& ci : cities) {
            if (!ci.alive || ci.religion == u.religion || atWar(c, ci.owner) || !civs[c].explored[idx(ci.x, ci.y)]) continue;
            int hr = holyCityReligion(ci);
            if (hr >= 0 && hr != u.religion) continue;
            int d = dist(ci.x, ci.y, u.x, u.y) - (ci.owner == c ? 6 : 0) + (isMinor(ci.owner) ? 2 : 0);
            if (dist(ci.x, ci.y, u.x, u.y) > 30) continue;
            if (d < bd) { bd = d; best = &ci; }
        }
        if (!best) { u.order = ORD_SLEEP; continue; }
        if (u.order != ORD_GOTO || u.gx != best->x || u.gy != best->y) { u.order = ORD_GOTO; u.gx = best->x; u.gy = best->y; }
        continueGoto(u);
        if (u.alive && u.order != ORD_GOTO && !canPreach(u)) {  // could not get there: try the next one later
            u.order = ORD_NONE;
            if (u.moves > 0 && dist(u.x, u.y, best->x, best->y) > 1) u.moves = 0;
        }
        if (u.alive && canPreach(u)) preach(u);
    }
}
