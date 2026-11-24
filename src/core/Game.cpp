// Rebirth of Civilisation - rules engine: map helpers, research, cities, units, combat
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <queue>
#include <cstdio>

const char* orderName(int o) {
    switch (o) {
        case ORD_FORTIFY: return "Fortified";
        case ORD_SLEEP: return "Sleeping";
        case ORD_GOTO: return "Moving";
        case ORD_BUILD: return "Building";
        case ORD_EXCAVATE: return "Excavating";
        case ORD_DECON: return "Decontaminating";
        case ORD_SKIP: return "Skipping turn";
        default: return "Awaiting orders";
    }
}

// ============================================================ hex helpers (odd-r offset, pointy top)
static const int DIRS[2][6][2] = {
    {{+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1}},
    {{+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1}},
};

int Game::neighbors(int x, int y, Pt out[6]) const {
    int n = 0;
    int par = y & 1;
    for (int d = 0; d < 6; d++) {
        int nx = x + DIRS[par][d][0], ny = y + DIRS[par][d][1];
        if (inMap(nx, ny)) out[n++] = {nx, ny};
    }
    return n;
}

bool Game::neighborDir(int x, int y, int d, Pt& out) const {
    int par = y & 1;
    out = {x + DIRS[par][d][0], y + DIRS[par][d][1]};
    return inMap(out.x, out.y);
}

int Game::dist(int x1, int y1, int x2, int y2) const {
    int q1 = x1 - (y1 - (y1 & 1)) / 2, q2 = x2 - (y2 - (y2 & 1)) / 2;
    int dq = q1 - q2, dr = y1 - y2;
    return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

std::vector<Pt> Game::ring(int x, int y, int r) const {
    std::vector<Pt> out;
    for (int yy = y - r; yy <= y + r; yy++)
        for (int xx = x - r - 1; xx <= x + r + 1; xx++)
            if (inMap(xx, yy) && dist(x, y, xx, yy) <= r) out.push_back({xx, yy});
    return out;
}

// ============================================================ civ queries
int Game::policyPct(int c, PolicyEffect e) const {
    int v = 0;
    for (size_t p = 0; p < g_policies.size(); p++)
        if (civs[c].policies[p] && g_policies[p].eff == e) v += g_policies[p].value;
    return v;
}

bool Game::ownsWonderSpecial(int c, BSpecial s) const {
    for (size_t b = 0; b < g_buildings.size(); b++) {
        if (!g_buildings[b].wonder || g_buildings[b].special != s) continue;
        int cid = wonderCity[b];
        if (cid >= 0 && cities[cid].alive && cities[cid].owner == c) return true;
    }
    return false;
}

bool Game::atWar(int a, int b) const {
    if (a == b || a < 0 || b < 0) return false;
    if (a == CIV_HUMANS || b == CIV_HUMANS) return false;
    if (a == CIV_FERAL || b == CIV_FERAL) return true;
    return civs[a].war[b] != 0;
}

int Game::era(int c) const {
    int e = 0;
    for (size_t t = 0; t < g_techs.size(); t++)
        if (civs[c].techs[t]) e = std::max(e, g_techs[t].era);
    return e;
}

int Game::numCities(int c) const {
    int n = 0;
    for (auto& ci : cities) if (ci.alive && ci.owner == c) n++;
    return n;
}

int Game::militaryPower(int c) const {
    int p = 0;
    for (auto& u : units)
        if (u.alive && u.owner == c && g_units[u.type].military())
            p += std::max(g_units[u.type].str, g_units[u.type].rstr) * u.hp / 100;
    return p;
}

int Game::score(int c) const {
    const Civ& cv = civs[c];
    int s = 0;
    for (auto& ci : cities) if (ci.alive && ci.owner == c) s += 10 + ci.pop * 3;
    for (auto t : cv.techs) if (t) s += 5;
    for (size_t b = 0; b < g_buildings.size(); b++)
        if (g_buildings[b].wonder && wonderCity[b] >= 0 && cities[wonderCity[b]].owner == c) s += 25;
    s += cv.policiesAdopted * 8 + cv.ruinsExcavated * 5 + traitCount(c) * 5 + bandMembers(c);
    return s;
}

// The calendar follows the progress of the most advanced species, not the turn number:
// while brains and language are still evolving each turn spans thousands of years; once
// science takes over, turns shrink era by era as in Civilization (about 40 years down to 1).
int Game::yearsPerTurn() const {
    int trunk = -1, topEra = 0;
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) {
        if (!civs[c].alive) continue;
        for (int t = TR_NEOCORTEX; t <= TR_ABSTRACT; t++) if (hasTrait(c, t)) trunk = std::max(trunk, t);
        topEra = std::max(topEra, era(c));
    }
    int y;
    if (trunk < TR_ADV_SPEECH) {
        static const int EVO_YEARS[] = {20000, 10000, 6000, 3000, 1000};  // none, Neocortex, Gestures, Proto-Speech, Speech
        y = EVO_YEARS[trunk + 1];
    } else {
        static const int ERA_YEARS[NUM_ERAS] = {50, 50, 40, 25, 20, 10, 5, 2, 1};
        y = ERA_YEARS[std::clamp(topEra, 0, NUM_ERAS - 1)];
    }
    // the same span of history at every speed: phases last more turns on slower speeds, so each turn covers fewer years
    return std::max(1, (int)((long long)y * 170 / costPct));
}

std::string Game::calendarPhase() const {
    int y = yearsPerTurn();
    if (!warHappened) return "The last days of humanity";
    bool science = false;
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) if (civs[c].alive && hasTrait(c, TR_ADV_SPEECH)) science = true;
    return std::string(science ? "Age of science" : "Age of evolution") + ": " + std::to_string(y) + (y == 1 ? " year" : " years") + " per turn";
}

static std::string withCommas(long long v) {
    std::string s = std::to_string(v), out;
    int n = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) {
        out.insert(out.begin(), s[i]);
        if (++n % 3 == 0 && i > 0 && s[i - 1] != '-') out.insert(out.begin(), ',');
    }
    return out;
}

std::string Game::dateString(int t) const {
    static const char* pro[] = {"January 2032", "July 2032", "January 2033", "July 2033", "January 2034", "August 2034"};
    if (t <= PROLOGUE_TURNS) return pro[std::max(0, t - 1)];
    std::string s = "Year " + withCommas(std::max(1LL, yearAF)) + " AF";
    if (yearAF < 10000) s += " (" + std::to_string(2034 + yearAF) + " AD)";
    return s;
}

std::string Game::shortDate() const {
    if (turn <= PROLOGUE_TURNS) return dateString(turn);
    return withCommas(std::max(1LL, yearAF)) + " AF";
}

// ============================================================ strategic resources
int Game::resourceCount(int c, int r) const {
    if (c < 0 || c >= (int)civs.size()) return 0;
    if (resDirty || resCache.size() != civs.size()) {
        resCache.assign(civs.size(), std::vector<int>(R_COUNT, 0));
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const Tile& t = at(x, y);
                if (t.owner < 0 || t.res == R_NONE || t.owner >= (int)civs.size()) continue;
                if (resourceVisible(t.owner, x, y)) resCache[t.owner][t.res]++;
            }
        resDirty = false;
    }
    return resCache[c][r];
}

bool Game::hasResource(int c, int r) const {
    if (resourceCount(c, r) > 0) return true;
    if (c >= FIRST_PRIMATE_CIV && lentResource(c, r)) return true;
    return r == R_IRON && resourceCount(c, R_SCRAP) > 0;
}

bool Game::hasNeeds(int c, const std::vector<int>& needs) const {
    for (int r : needs) if (!hasResource(c, r)) return false;
    return true;
}

// ============================================================ research
bool Game::canResearch(int c, int t) const {
    if (t < 0 || civs[c].techs[t]) return false;
    if (g_techs[t].trait >= 0 && !hasTrait(c, g_techs[t].trait)) return false;
    for (int p : g_techs[t].prereqs) if (!civs[c].techs[p]) return false;
    return true;
}

void Game::setResearch(int c, int t) {
    Civ& cv = civs[c];
    if (!canResearch(c, t)) return;
    cv.research = t;
    if (cv.scienceBank > 0) {
        int b = cv.scienceBank;
        cv.scienceBank = 0;
        addScience(c, b);
    }
}

int Game::nextTowardGoal(int c, int goal) const {
    if (goal < 0 || civs[c].techs[goal]) return -1;
    std::vector<uint8_t> need(g_techs.size(), 0);
    std::vector<int> stack = {goal};
    while (!stack.empty()) {
        int t = stack.back();
        stack.pop_back();
        if (need[t] || civs[c].techs[t]) continue;
        need[t] = 1;
        for (int p : g_techs[t].prereqs) stack.push_back(p);
    }
    int best = -1;
    for (size_t t = 0; t < g_techs.size(); t++)
        if (need[t] && canResearch(c, (int)t) && (best < 0 || techCost(t) < techCost(best))) best = (int)t;
    return best;
}

void Game::setGoal(int c, int t) {
    Civ& cv = civs[c];
    if (t < 0 || cv.techs[t]) return;
    cv.goal = t;
    int n = canResearch(c, t) ? t : nextTowardGoal(c, t);
    if (n >= 0) setResearch(c, n);
}

void Game::grantTech(int c, int t, bool announce) {
    Civ& cv = civs[c];
    if (cv.techs[t]) return;
    cv.techs[t] = 1;
    if (t == ID.tLaw) {  // our borders close: foreign armies at peace with us must leave
        for (int x = FIRST_PRIMATE_CIV; x < (int)civs.size(); x++)
            if (x != c && civs[x].alive && !civs[x].war[c] && !hasAgreement(AG_OPEN_BORDERS, c, x)) expelUnits(x, c);
    }
    if (cv.research == t) {
        // completed normally: carry the overflow; granted for free: refund partial progress
        int leftover = cv.techProgress[t] >= techCost(t) ? cv.techProgress[t] - techCost(t) : cv.techProgress[t];
        cv.research = -1;
        cv.scienceBank += leftover;
    }
    if (cv.goal == t) cv.goal = -1;
    markResourcesDirty();
    if (announce && c == player) {
        std::string unlocks;
        for (auto& u : g_units)
            if (u.techId == t && (u.primate == -1 || u.primate == cv.primate)) {
                int real = uniqueFor(c, unitIndex(u.name));
                if (real == unitIndex(u.name)) unlocks += (unlocks.empty() ? "" : ", ") + u.name;
            }
        for (auto& b : g_buildings)
            if (b.techId == t) unlocks += (unlocks.empty() ? "" : ", ") + b.name + (b.wonder ? " (Wonder)" : "");
        for (auto& im : g_improvements)
            if (im.techId == t && !im.name.empty()) unlocks += (unlocks.empty() ? "" : ", ") + im.name;
        std::string extra;
        if (t == ID.tCurrency) extra = "\n\nYour civilisation now earns GOLD. Gold can be used to purchase units and buildings in cities.";
        if (t == ID.tLaw)
            extra = "\n\nYOUR BORDERS ARE NOW CLOSED. In peacetime, other civilisations' armies may no longer enter your lands unless you "
                    "open your borders to them in a deal. You can also impose embargoes from the Diplomacy screen.";
        if (t == ID.tCultivation)
            extra = "\n\nTHE END OF WANDERING. Your bands can now settle down and found cities (B). A band of N members founds a city of "
                    "size 1 + (N-1)/2. Choose the spot well - this will be the heart of your civilisation.";
        if (t == ID.tRafts) extra = "\n\nLand units can now embark onto coastal waters.";
        if (t == ID.tNavigation) extra = "\n\nEmbarked units can now cross the open ocean.";
        if (t == ID.tRadiology) extra = "\n\nGatherers can now decontaminate fallout.";
        for (int k = 0; k < RUIN_TIERS; k++)
            if (g_ruinTierTech[k] == t)
                extra = std::string("\n\nThe wreckage of humanity holds new secrets: Gatherers can now excavate ") +
                        RUIN_TIER_NAME[k] + " from human ruins.";
        for (auto& r : g_resources)
            if (r.revealTechId == t && !r.name.empty())
                extra += "\n\nYour people can now recognise deposits of " + r.name + (r.kind == RK_STRATEGIC ? " (strategic resource)" : "") + ".";
        if (g_techs[t].name == "Bronze Working")
            extra += "\n\nWith both Copper and Tin inside your borders you can train the Bronze Phalanx.";
        std::string body = g_techs[t].flavor;
        if (!unlocks.empty()) body += "\n\nUnlocks: " + unlocks;
        body += extra;
        if (!extra.empty())
            popup(c, POP_TECH, "Discovered: " + g_techs[t].name, body);
        notify(c, "Research complete: " + g_techs[t].name);
    }
    if (cv.research < 0 && cv.goal >= 0) {
        int n = nextTowardGoal(c, cv.goal);
        if (n >= 0) setResearch(c, n);
    }
}

void Game::addScience(int c, int amount) {
    Civ& cv = civs[c];
    if (amount <= 0) return;
    if (c >= FIRST_PRIMATE_CIV && !hasTrait(c, TR_NEOCORTEX)) return;  // brains not yet able to learn
    if (cv.research < 0) {
        cv.scienceBank += amount;
        return;
    }
    int t = cv.research;
    cv.techProgress[t] += amount;
    if (cv.techProgress[t] >= techCost(t)) {
        grantTech(c, t);  // leftover goes to bank, next research picked from goal
        if (cv.research >= 0 && cv.scienceBank > 0) {
            int b = cv.scienceBank;
            cv.scienceBank = 0;
            addScience(c, b);
        }
    }
}

int Game::researchTurns(int c) const {
    const Civ& cv = civs[c];
    if (cv.research < 0) return 0;
    int left = techCost(cv.research) - cv.techProgress[cv.research];
    int s = std::max(1, cv.sciencePerTurn);
    return std::max(1, (left + s - 1) / s);
}

// ============================================================ evolution
std::vector<Pressure> Game::evolutionPressures(int c) const {
    std::vector<Pressure> out;
    const Civ& cv = civs[c];
    int nc = 0, pop = 0;
    for (auto& ci : cities) if (ci.alive && ci.owner == c) { nc++; pop += ci.pop; }
    // social
    int members = bandMembers(c), bands = numBands(c);
    if (nc == 0)
        out.push_back({"Band life", std::min(5, 1 + members / 4), PC_SOCIAL, "Living in close-knit bands rewards communication (+1, +1 per 4 band members, up to +5)."});
    else
        out.push_back({"Group living", std::min(8, 1 + nc), PC_SOCIAL, "Living in larger, settled groups rewards communication (+1, +1 per city, up to +8)."});
    if (pop / 3) out.push_back({"Crowding", std::min(8, pop / 3), PC_SOCIAL, "Dense populations compete for status and mates (+1 per 3 population, up to +8)."});
    int stories = std::min(6, cv.culturePerTurn / 3);
    if (stories) out.push_back({"Shared stories", stories, PC_SOCIAL, "Culture rewards those who speak and remember (+1 per 3 Culture per turn, up to +6)."});
    // ecological
    out.push_back({"Survival", 2, PC_ECOLOGY, "Every generation, the cleverest at finding food survive."});
    if (bands > 0 && nc == 0)
        out.push_back({"Roaming", 2, PC_ECOLOGY, "Always on the move: new lands, new food, new dangers."});
    int hungry = 0;
    for (auto& u : units)
        if (u.alive && u.owner == c && g_units[u.type].cls == UnitClass::Band && bandForage(u) < u.band) hungry++;
    if (hungry) out.push_back({"Hunger", std::min(3, 1 + hungry), PC_ECOLOGY, "Bands that cannot find enough food (+1, +1 per hungry band, up to +3)."});
    int land = 0, harsh = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Tile& t = at(x, y);
            if (t.owner != c || isWater(x, y)) continue;
            land++;
            if (t.terrain == T_TUNDRA || t.terrain == T_SNOW || t.terrain == T_DESERT || t.mountain) harsh++;
        }
    if (land / 12) out.push_back({"Wide ranging", std::min(5, land / 12), PC_ECOLOGY, "Mastering a large territory (+1 per 12 land tiles, up to +5)."});
    if (harsh / 4) out.push_back({"Harsh climate", std::min(3, harsh / 4), PC_ECOLOGY, "Cold, drought and mountains in your lands (+1 per 4 harsh tiles, up to +3)."});
    if (warHappened && turn <= PROLOGUE_TURNS + dur(30))
        out.push_back({"Nuclear winter", 3, PC_ECOLOGY, "The dark, cold years after the Fall cull the weak (lasts " + std::to_string(dur(30)) + " turns)."});
    // conflict
    bool war = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (cv.war[o] && civs[o].alive) war = true;
    if (war) out.push_back({"War", 3, PC_CONFLICT, "Being at war with rival primates."});
    if (cv.battles) out.push_back({"Battles", std::min(4, cv.battles), PC_CONFLICT, "Each fight your people took part in this turn (up to +4)."});
    int beasts = 0;
    for (auto& u : units) {
        if (!u.alive || u.owner != CIV_FERAL) continue;
        bool near = false;
        for (auto& ci : cities) if (ci.alive && ci.owner == c && dist(ci.x, ci.y, u.x, u.y) <= 4) near = true;
        for (auto& b : units)
            if (b.alive && b.owner == c && g_units[b.type].cls == UnitClass::Band && dist(b.x, b.y, u.x, u.y) <= 3) near = true;
        if (near) beasts++;
    }
    if (beasts) out.push_back({"Predators", std::min(3, beasts), PC_CONFLICT, "Feral packs prowling near your settlements or bands (up to +3)."});
    // mutation
    int fall = 0;
    for (auto& t : tiles) if (t.owner == c && t.fallout) fall++;
    for (auto& u : units) if (u.alive && u.owner == c && at(u.x, u.y).fallout) fall += 2;
    if (fall) out.push_back({"Radiation", std::min(4, (fall + 1) / 2), PC_MUTATION, "Fallout in your lands and on your units drives mutation (up to +4)."});
    for (auto& tp : cv.tempPressures)
        out.push_back({tp.name + " (" + std::to_string(tp.turns) + ")", tp.value, tp.cat, "Lingering pressure from a disaster (" + std::to_string(tp.turns) + " more turns)."});
    if (warHappened && turn <= PROLOGUE_TURNS + dur(40))
        out.push_back({"Background radiation", 1, PC_MUTATION, "The whole planet is still mildly radioactive (lasts " + std::to_string(dur(40)) + " turns)."});
    return out;
}

std::vector<int> Game::evolutionRates(int c) const {
    std::vector<int> r(PC_COUNT, 0);
    for (auto& p : evolutionPressures(c)) r[p.cat] += p.value;
    if (const PrimateDef* pd = primate(c))
        for (auto& v : r) v = applyPct(v, pd->evoPct);
    return r;
}

int Game::traitCount(int c) const {
    int n = 0;
    for (auto t : civs[c].traits) n += t;
    return n;
}

bool Game::capstoneTaken(int c) const {
    for (size_t t = 0; t < g_traits.size(); t++) if (g_traits[t].capstone && hasTrait(c, (int)t)) return true;
    return false;
}

int Game::strandsStarted(int c) const {
    int n = 0;
    for (int st = ST_TOOL; st < ST_COUNT; st++) {
        for (size_t t = 0; t < g_traits.size(); t++)
            if (g_traits[t].strand == st && hasTrait(c, (int)t)) { n++; break; }
    }
    return n;
}

bool Game::strandStarted(int c, int st) const {
    for (size_t t = 0; t < g_traits.size(); t++)
        if (g_traits[t].strand == st && hasTrait(c, (int)t)) return true;
    return false;
}

bool Game::canEvolve(int c, int t) const {
    if (t < 0 || t >= (int)g_traits.size() || hasTrait(c, t)) return false;
    for (int p : g_traits[t].prereqs) if (!hasTrait(c, p)) return false;
    if (g_traits[t].capstone && capstoneTaken(c)) return false;
    int st = g_traits[t].strand;
    if (st != ST_TRUNK && !strandStarted(c, st) && strandsStarted(c) >= MAX_STRANDS) return false;
    return true;
}

bool Game::affordable(int c, int t) const {
    for (int k = 0; k < PC_COUNT; k++) if (civs[c].evoPool[k] < traitCost(t, k)) return false;
    return true;
}

int Game::evolveTurns(int c, int t) const {
    std::vector<int> r = civs[c].evoRate;
    if (r.empty() || turn == 1) r = evolutionRates(c);
    int turns = 0;
    for (int k = 0; k < PC_COUNT; k++) {
        int need = traitCost(t, k) - civs[c].evoPool[k];
        if (need <= 0) continue;
        if (r[k] <= 0) return -1;
        turns = std::max(turns, (need + r[k] - 1) / r[k]);
    }
    return turns;
}

std::string Game::traitBlocker(int c, int t) const {
    if (hasTrait(c, t)) return "Already evolved";
    for (int p : g_traits[t].prereqs) if (!hasTrait(c, p)) return "Requires " + g_traits[p].name;
    if (g_traits[t].capstone && capstoneTaken(c)) return "Your species has already specialised (only one final adaptation)";
    if (g_traits[t].strand != ST_TRUNK && !strandStarted(c, g_traits[t].strand) && strandsStarted(c) >= MAX_STRANDS)
        return "Your species has already committed to two strands";
    if (evolveTurns(c, t) < 0) {
        std::vector<int> r = evolutionRates(c);
        for (int k = 0; k < PC_COUNT; k++)
            if (traitCost(t, k) > civs[c].evoPool[k] && r[k] <= 0)
                return std::string("Your people face no ") + PRESSURE_NAMES[k] + " pressure";
    }
    return "";
}

void Game::refreshTraitFx(int c) {
    TraitFx f;
    for (size_t t = 0; t < g_traits.size(); t++) if (hasTrait(c, (int)t)) f += g_traits[t].fx;
    civs[c].fx = f;
}

void Game::setEvoGoal(int c, int t) {
    if (t >= 0 && !canEvolve(c, t)) return;
    civs[c].evoGoal = t;
}

void Game::gainTrait(int c, int t) {
    Civ& cv = civs[c];
    const TraitDef& tr = g_traits[t];
    for (int k = 0; k < PC_COUNT; k++) cv.evoPool[k] -= traitCost(t, k);
    cv.traits[t] = 1;
    refreshTraitFx(c);
    if (cv.evoGoal == t) cv.evoGoal = -1;
    notify(c, "Evolution: your people have developed " + tr.name + ".");
    std::string extra;
    if (tr.capstone) extra = "\n\nThis is your species' final adaptation. The other evolutionary paths are now closed.";
    popup(c, POP_EVOLUTION, "Evolution: " + tr.name, tr.flavor + "\n\n" + tr.effect + "." + extra);
    if (t == TR_SPEECH)
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
            if (o != c && civs[o].alive && civs[o].met[c]) notify(o, "The " + cv.name + " have learned to speak.");
    if (cv.research < 0 && cv.goal >= 0) {
        int n = nextTowardGoal(c, cv.goal);
        if (n >= 0) setResearch(c, n);
    }
}

int Game::bestFitStrand(int c) const {
    // the unstarted strand whose final adaptation this species' pressures would reach soonest
    static const int MAIN[ST_COUNT] = {PC_SOCIAL, PC_ECOLOGY, PC_SOCIAL, PC_CONFLICT, PC_MUTATION};
    std::vector<int> r = evolutionRates(c);
    int best = -1;
    double bt = 1e9;
    for (int st = ST_TOOL; st < ST_COUNT; st++) {
        if (strandStarted(c, st)) continue;
        int cap = -1;
        for (size_t t = 0; t < g_traits.size(); t++) if (g_traits[t].strand == st && g_traits[t].capstone) cap = (int)t;
        int k = MAIN[st];
        int rate = r[k] - (k == PC_MUTATION && warHappened && turn <= PROLOGUE_TURNS + dur(40) ? 1 : 0);  // ignore fading background radiation
        if (rate <= 0 || cap < 0) continue;
        double turns = (double)std::max(0, traitCost(cap, k) - civs[c].evoPool[k]) / rate;
        if (turns < bt) { bt = turns; best = st; }
    }
    return best;
}

int Game::aiChooseTrait(int c) {
    // prefer the language trunk (it gates science), but grab other strands when this species' pressures favour them
    int best = -1;
    double bestS = 1e18;
    const PrimateDef* pd = primate(c);
    int aggr = pd ? pd->aggression : 5;
    for (size_t t = 0; t < g_traits.size(); t++) {
        if (!canEvolve(c, (int)t)) continue;
        int turns = evolveTurns(c, (int)t);
        if (turns < 0) continue;
        double s = turns + 1;
        const TraitDef& tr = g_traits[t];
        if (tr.strand == ST_TRUNK) s *= 0.55;
        if (tr.strand != ST_TRUNK && !strandStarted(c, tr.strand)) {
            // commit deliberately: first the species' natural leaning, later whatever its pressures support best
            bool ok = pd && tr.strand == pd->evoLean;
            if (!ok && turn >= dur(30)) {
                bool leanStalled = true;  // the natural leaning lacks the pressure it needs?
                if (pd && strandStarted(c, pd->evoLean)) leanStalled = false;
                if (pd && leanStalled)
                    for (size_t k = 0; k < g_traits.size(); k++)
                        if (g_traits[k].strand == pd->evoLean && g_traits[k].tier == 1) {
                            int lt = evolveTurns(c, (int)k);
                            leanStalled = lt < 0 || lt > 20;
                        }
                if (strandsStarted(c) >= 1 || leanStalled) ok = tr.strand == bestFitStrand(c);
            }
            if (!ok) continue;
        }
        if (tr.strand == ST_PREDATOR) s *= 1.3 - aggr * 0.05;
        if (pd && tr.strand == pd->evoLean) s *= 0.6;
        if (tr.capstone) s *= 0.8;
        s *= 0.9 + 0.2 * rng.unit();
        if (s < bestS) { bestS = s; best = (int)t; }
    }
    return best;
}

void Game::processEvolution(int c) {
    Civ& cv = civs[c];
    cv.evoRate = evolutionRates(c);
    for (int k = 0; k < PC_COUNT; k++) cv.evoPool[k] += cv.evoRate[k];
    cv.battles = 0;
    for (auto& tp : cv.tempPressures) tp.turns--;
    cv.tempPressures.erase(std::remove_if(cv.tempPressures.begin(), cv.tempPressures.end(), [](const TempPressure& t) { return t.turns <= 0; }),
                           cv.tempPressures.end());
    for (int guard = 0; guard < 4; guard++) {
        if (!cv.player && (cv.evoGoal < 0 || !canEvolve(c, cv.evoGoal))) cv.evoGoal = aiChooseTrait(c);
        if (cv.evoGoal >= 0 && !canEvolve(c, cv.evoGoal)) cv.evoGoal = -1;
        if (cv.evoGoal < 0 || !affordable(c, cv.evoGoal)) break;
        gainTrait(c, cv.evoGoal);
    }
}

// ============================================================ traditions
int Game::policyCost(int c) const {
    int n = civs[c].policiesAdopted;
    return sc(40 + 60 * n + 24 * n * n);
}
bool Game::canAdopt(int c, int p) const {
    return !civs[c].policies[p] && hasTech(c, g_policies[p].techId) && civs[c].culture >= policyCost(c);
}
void Game::adoptPolicy(int c, int p) {
    if (!canAdopt(c, p)) return;
    Civ& cv = civs[c];
    cv.culture -= policyCost(c);
    cv.policies[p] = 1;
    cv.policiesAdopted++;
    notify(c, "Tradition adopted: " + g_policies[p].name);
}

// ============================================================ tiles
bool Game::resourceVisible(int c, int x, int y) const {
    const Tile& t = at(x, y);
    if (t.res == R_NONE) return false;
    if (t.res == R_SCRAP && !warHappened) return false;
    return c < 0 ? true : hasTech(c, g_resources[t.res].revealTechId);
}

Yields Game::tileYields(int c, int x, int y) const {
    Yields y0;
    const Tile& t = at(x, y);
    if (t.mountain) return y0;
    const TerrainDef& td = g_terrain[t.terrain];
    y0.food = td.food;
    y0.prod = td.prod;
    y0.gold = td.gold;
    if (t.veg == V_FOREST) { y0.food = 1; y0.prod = 1; }
    if (t.veg == V_JUNGLE) { y0.food = 2; y0.prod = 0; }
    if (t.hills) { y0.food = std::max(0, y0.food - 1); y0.prod += 2; }
    const PrimateDef* pd = c >= 0 ? primate(c) : nullptr;
    if (pd && pd->snowMonkey && (t.terrain == T_TUNDRA || t.terrain == T_SNOW)) { y0.food++; y0.prod++; }
    if (resourceVisible(c, x, y)) {
        const ResourceDef& r = g_resources[t.res];
        y0.food += r.food; y0.prod += r.prod; y0.gold += r.gold;
    }
    if (t.imp != IMP_NONE) {
        const ImprovementDef& im = g_improvements[t.imp];
        y0.food += im.food; y0.prod += im.prod; y0.gold += im.gold;
        if (c >= 0 && t.imp == IMP_FARM && hasTech(c, ID.tChemistry)) y0.food++;
        if (t.imp == IMP_FARM && isCrop(t.res)) y0.food++;  // farmed wheat, rice and maize
        if (c >= 0 && t.imp == IMP_MINE && hasTech(c, ID.tIndustrial)) y0.prod++;
    }
    if (t.ruin != RUIN_NONE && warHappened && c >= 0) {
        if (hasTech(c, ID.tSalvaging)) y0.prod += 1;
        if (hasTech(c, ID.tDeciphering)) y0.sci += 1;
        if (hasTech(c, ID.tComputing)) y0.sci += 2;
    }
    bool adapted = c >= FIRST_PRIMATE_CIV && civs[c].fx.falloutYields;
    if (adapted) {}
    else if (t.fallout >= 2) { y0 = Yields(); }
    else if (t.fallout == 1) { y0.food = std::max(0, y0.food - 1); y0.prod = std::max(0, y0.prod - 1); }
    if (c < 0 || !hasGold(c)) y0.gold = 0;
    return y0;
}

// ============================================================ cities
City* Game::cityAt(int x, int y) {
    for (auto& c : cities) if (c.alive && c.x == x && c.y == y) return &c;
    return nullptr;
}
const City* Game::cityAt(int x, int y) const {
    for (auto& c : cities) if (c.alive && c.x == x && c.y == y) return &c;
    return nullptr;
}

int Game::growthThreshold(int pop) const {
    return sc(12 + 6 * (pop - 1) + (int)std::pow((double)(pop - 1), 1.5));
}

int Game::ruinTilesInTerritory(const City& city) const {
    int n = 0;
    for (auto& p : ring(city.x, city.y, 3)) {
        const Tile& t = at(p.x, p.y);
        if (t.city == city.id && t.ruin != RUIN_NONE) n++;
    }
    return n;
}

void Game::computeCity(City& city) {
    int c = city.owner;
    const PrimateDef* pd = primate(c);
    Yields total;
    // centre tile
    Yields cy = tileYields(c, city.x, city.y);
    cy.food = std::max(cy.food, 2) + 1;
    if (hasTech(c, ID.tCooking)) cy.food += 1;
    cy.prod = std::max(cy.prod, 1) + 1;
    if (hasGold(c)) cy.gold += 1;
    total += cy;
    // worked tiles: greedy by weighted score
    struct Cand { int i; Yields y; int s; };
    std::vector<Cand> cand;
    for (auto& p : ring(city.x, city.y, 3)) {
        if (p.x == city.x && p.y == city.y) continue;
        const Tile& t = at(p.x, p.y);
        if (t.city != city.id || t.mountain) continue;
        Yields y = tileYields(c, p.x, p.y);
        int s = y.food * 30 + y.prod * 22 + y.gold * 10 + y.sci * 18 + y.cult * 18;
        cand.push_back({idx(p.x, p.y), y, s});
    }
    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.s > b.s; });
    city.worked.clear();
    for (int k = 0; k < (int)cand.size() && k < city.pop; k++) {
        total += cand[k].y;
        city.worked.push_back(cand[k].i);
    }
    // base yields from population
    total.sci += 1 + city.pop * 2 / 3;
    total.cult += 1;
    if (hasGold(c)) total.gold += city.pop / 3;
    Yields pct;
    int ruins = -1;
    for (size_t b = 0; b < g_buildings.size(); b++) {
        if (!city.has[b]) continue;
        const BuildingDef& bd = g_buildings[b];
        total += bd.flat;
        pct += bd.pct;
        if (bd.special == BSpecial::RuinProd || bd.special == BSpecial::RuinScience) {
            if (ruins < 0) ruins = ruinTilesInTerritory(city);
            if (bd.special == BSpecial::RuinProd) total.prod += ruins;
            else total.sci += 2 * ruins;
        }
    }
    total.sci += policyPct(c, PolicyEffect::FlatSci);
    // evolutionary traits
    if (c >= FIRST_PRIMATE_CIV) {
        const TraitFx& f = civs[c].fx;
        total.cult += f.flatCult;
        total.sci += f.flatSci;
        total.prod += f.flatProd;
        pct.food += f.food; pct.prod += f.prod; pct.sci += f.sci; pct.cult += f.cult;
    }
    total.cult += policyPct(c, PolicyEffect::FlatCult);
    if (pd) {
        pct.food += pd->foodPct; pct.prod += pd->prodPct; pct.sci += pd->sciPct;
        pct.cult += pd->cultPct; pct.gold += pd->goldPct;
    }
    pct.food += policyPct(c, PolicyEffect::Food);
    pct.prod += policyPct(c, PolicyEffect::Prod);
    pct.sci += policyPct(c, PolicyEffect::Sci) + diploSciPct(c);
    pct.cult += policyPct(c, PolicyEffect::Culture);
    pct.gold += policyPct(c, PolicyEffect::Gold);
    if (ownsWonderSpecial(c, BSpecial::GrowthBonus)) pct.food += 10;
    if (city.droughtTurns > 0) pct.food -= 33;
    int hb = civs[c].yieldBonusPct;
    pct.food += hb / 2; pct.prod += hb; pct.sci += hb;
    total.food = applyPct(total.food, pct.food);
    total.prod = applyPct(total.prod, pct.prod);
    total.sci = applyPct(total.sci, pct.sci);
    if (c >= FIRST_PRIMATE_CIV && !hasTrait(c, TR_NEOCORTEX)) total.sci = 0;  // no deliberate learning yet
    total.cult = applyPct(total.cult, pct.cult);
    total.gold = hasGold(c) ? applyPct(total.gold, pct.gold) : 0;
    city.yields = total;
    city.foodSurplus = total.food - 2 * city.pop;
}

int Game::cityStrength(const City& city) const {
    // x10 fixed point, like unitStrength
    int bestUnit = 0;
    for (int u : availableUnits(city.owner)) bestUnit = std::max(bestUnit, g_units[u].str);
    int def = 0;
    for (size_t b = 0; b < g_buildings.size(); b++) if (city.has[b]) def += g_buildings[b].defense;
    int s = 60 + city.pop * 8 + def * 10 + bestUnit * 4;
    return applyPct(s, policyPct(city.owner, PolicyEffect::CityDefense));
}

int Game::cityMaxHP(const City& city) const {
    int hp = 200;
    for (size_t b = 0; b < g_buildings.size(); b++) if (city.has[b]) hp += g_buildings[b].hp;
    return hp;
}

bool Game::canFoundAt(int c, int x, int y, std::string* why) const {
    auto fail = [&](const char* w) { if (why) *why = w; return false; };
    if (!inMap(x, y)) return fail("Off the map");
    const Tile& t = at(x, y);
    if (!canSettle(c)) return fail("Your people are still nomads: evolve Speech and discover Cultivation to settle down");
    if (isWater(x, y) || t.mountain) return fail("Cannot settle here");
    if (t.fallout > 0) return fail("The land is poisoned by fallout");
    if (t.site >= 0) return fail("Cannot settle on top of human structures");
    if (t.owner >= 0 && t.owner != c) return fail("Territory belongs to someone else");
    for (auto& ci : cities)
        if (ci.alive && dist(ci.x, ci.y, x, y) <= 3) return fail("Too close to another settlement");
    return true;
}

void Game::claimTile(City& city, int x, int y) {
    Tile& t = at(x, y);
    t.owner = (int8_t)city.owner;
    t.city = (int16_t)city.id;
    markResourcesDirty();
}

int Game::foundCity(int c, int x, int y) {
    Civ& cv = civs[c];
    City city;
    city.id = (int)cities.size();
    const PrimateDef* pd = primate(c);
    if (pd && cv.nameIdx < (int)pd->cityNames.size()) city.name = pd->cityNames[cv.nameIdx];
    else city.name = "New " + (pd ? pd->cityNames[cv.nameIdx % pd->cityNames.size()] : std::string("Camp"));
    cv.nameIdx++;
    city.owner = city.origOwner = c;
    city.x = x; city.y = y;
    city.has.assign(g_buildings.size(), 0);
    city.founded = turn;
    if (cv.capital < 0) {
        bool first = true;
        for (auto& ci : cities) if (ci.origOwner == c) first = false;
        city.capital = true;
        city.origCapital = first;
        city.has[ID.bPalace] = 1;
        cv.capital = city.id;
    }
    cities.push_back(city);
#ifdef REBIRTH_STRESS_REALLOC
    cities.shrink_to_fit();
#endif
    City& ref = cities.back();
    for (auto& p : ring(x, y, 1)) {
        Tile& t = at(p.x, p.y);
        if (t.owner < 0 || t.owner == c) claimTile(ref, p.x, p.y);
    }
    at(x, y).veg = at(x, y).veg == V_JUNGLE ? (uint8_t)V_NONE : at(x, y).veg;
    at(x, y).imp = IMP_NONE;
    ref.hp = cityMaxHP(ref);
    computeCity(ref);
    notify(c, ref.name + " has been founded.", x, y);
    return ref.id;
}

int Game::uniqueFor(int c, int base) const {
    int pr = civs[c].primate;
    for (size_t k = 0; k < g_units.size(); k++)
        if (g_units[k].primate == pr && pr >= 0 && g_units[k].replacesId == base) return (int)k;
    return base;
}

bool Game::unitBelongsTo(int c, int u) const {
    const UnitDef& d = g_units[u];
    if (d.primate == -2) return false;
    if (d.primate >= 0) return d.primate == civs[c].primate;
    return uniqueFor(c, u) == u;
}
static bool unitBelongs(const Game& g, int c, int u) { return g.unitBelongsTo(c, u); }

bool Game::unitObsolete(int c, int u) const {
    const UnitDef& d = g_units[u];
    if (d.civilian() || d.cls == UnitClass::Recon) return false;
    for (size_t v = 0; v < g_units.size(); v++) {
        const UnitDef& o = g_units[v];
        if ((int)v == u || o.cls != d.cls) continue;
        if (!unitBelongs(*this, c, (int)v) || !hasTech(c, o.techId) || !hasNeeds(c, o.needs)) continue;
        if (std::max(o.str, o.rstr) > std::max(d.str, d.rstr) && o.cost > d.cost) return true;
    }
    return false;
}

std::vector<int> Game::availableUnits(int c) const {
    std::vector<int> out;
    for (size_t u = 0; u < g_units.size(); u++) {
        if (!unitBelongs(*this, c, (int)u) || g_units[u].cls == UnitClass::Band) continue;
        if (!hasTech(c, g_units[u].techId) || !hasNeeds(c, g_units[u].needs)) continue;
        if (unitObsolete(c, (int)u)) continue;
        out.push_back((int)u);
    }
    return out;
}

std::vector<int> Game::availableBuildings(const City& city) const {
    std::vector<int> out;
    for (size_t b = 0; b < g_buildings.size(); b++)
        if (canBuild(city, BK_BUILDING, (int)b)) out.push_back((int)b);
    return out;
}

bool Game::canBuild(const City& city, int kind, int id) const {
    int c = city.owner;
    if (kind == BK_UNIT) {
        if (id < 0 || id >= (int)g_units.size()) return false;
        if (!unitBelongs(*this, c, id) || !hasTech(c, g_units[id].techId) || g_units[id].cls == UnitClass::Band) return false;
        if (!hasNeeds(c, g_units[id].needs)) return false;
        if (g_units[id].cls == UnitClass::Settler && city.pop < 2) return false;
        if (g_units[id].naval() && !isCoastal(city.x, city.y)) return false;
        return true;
    }
    if (kind == BK_BUILDING) {
        if (id < 0 || id >= (int)g_buildings.size()) return false;
        const BuildingDef& b = g_buildings[id];
        if (b.special == BSpecial::Palace || city.has[id]) return false;
        if (!hasTech(c, b.techId)) return false;
        if (b.wonder && wonderCity[id] >= 0) return false;
        if (!hasNeeds(c, b.needs)) return false;
        return true;
    }
    return false;
}

int Game::buildCost(int c, int kind, int id) const {
    if (kind == BK_UNIT) {
        const UnitDef& d = g_units[id];
        int pct = 0;
        if (d.military()) {
            const PrimateDef* pd = primate(c);
            if (pd) pct += pd->militaryCostPct;
            pct -= policyPct(c, PolicyEffect::UnitCost);
            if (c >= FIRST_PRIMATE_CIV) pct -= civs[c].fx.milCost;
        }
        if (d.cls == UnitClass::Settler) pct -= policyPct(c, PolicyEffect::SettlerCost);
        return std::max(5, sc(applyPct(d.cost, pct)));
    }
    if (kind == BK_BUILDING) return sc(g_buildings[id].cost);
    return 0;
}

int Game::buyCost(const City& city) const {
    if (city.buildKind == BK_NONE) return 0;
    if (city.buildKind == BK_BUILDING && g_buildings[city.buildId].wonder) return 0;
    int remaining = std::max(0, buildCost(city.owner, city.buildKind, city.buildId) - city.prod);
    if (remaining == 0) return 0;
    return remaining * 2 + (city.buildKind == BK_BUILDING ? 20 : 10);
}

bool Game::buy(City& city) {
    int cost = buyCost(city);
    Civ& cv = civs[city.owner];
    if (cost <= 0 || !hasGold(city.owner) || cv.gold < cost) return false;
    cv.gold -= cost;
    city.prod = buildCost(city.owner, city.buildKind, city.buildId);
    return true;
}

void Game::setProduction(City& city, int kind, int id) {
    if (kind != BK_NONE && !canBuild(city, kind, id)) return;
    city.buildKind = kind;
    city.buildId = id;
}

int Game::turnsToBuild(const City& city) const {
    if (city.buildKind == BK_NONE) return 0;
    int left = buildCost(city.owner, city.buildKind, city.buildId) - city.prod;
    int p = std::max(1, city.yields.prod);
    return std::max(1, (left + p - 1) / p);
}

int Game::turnsToGrow(const City& city) const {
    if (city.foodSurplus <= 0) return -1;
    int left = growthThreshold(city.pop) - city.food;
    return std::max(1, (left + city.foodSurplus - 1) / city.foodSurplus);
}

void Game::captureCity(City& city, int newOwner) {
    int old = city.owner;
    markResourcesDirty();
    // units of the old owner in the city are lost
    for (auto& u : units)
        if (u.alive && u.x == city.x && u.y == city.y && u.owner != newOwner) killUnit(u);
    if (city.pop <= 1 && !city.origCapital) {
        notify(old, city.name + " has been razed to the ground!", city.x, city.y);
        notify(newOwner, "You razed " + city.name + ".", city.x, city.y);
        statRazes++;
        civs[newOwner].citiesCaptured++;
        if (old >= 0) civs[old].citiesLost++;
        city.alive = false;
        for (auto& t : tiles) if (t.city == city.id) { t.city = -1; t.owner = -1; }
        for (size_t b = 0; b < g_buildings.size(); b++) if (wonderCity[b] == city.id) wonderCity[b] = -2;  // lost forever
    } else {
        statCaptures++;
        civs[newOwner].citiesCaptured++;
        if (old >= 0) civs[old].citiesLost++;
        if (old >= FIRST_PRIMATE_CIV && newOwner >= FIRST_PRIMATE_CIV) civs[old].grievance[newOwner] += 20;
        city.owner = newOwner;
        city.pop = std::max(1, city.pop - (city.pop + 2) / 3);
        city.food = 0;
        city.prod = 0;
        city.buildKind = BK_NONE;
        city.buildId = -1;
        city.has[ID.bPalace] = 0;
        city.capital = false;
        city.hp = cityMaxHP(city) / 4;
        for (auto& t : tiles) if (t.city == city.id) t.owner = (int8_t)newOwner;
        notify(old, city.name + " has been captured by the " + civs[newOwner].name + "!", city.x, city.y);
        notify(newOwner, "You captured " + city.name + "!", city.x, city.y);
        if (civs[newOwner].capital < 0) {
            city.capital = true;
            city.has[ID.bPalace] = 1;
            civs[newOwner].capital = city.id;
        }
    }
    if (civs[old].capital == city.id) {
        civs[old].capital = -1;
        for (auto& ci : cities)
            if (ci.alive && ci.owner == old) {
                ci.capital = true;
                ci.has[ID.bPalace] = 1;
                civs[old].capital = ci.id;
                notify(old, "The Nest Hall has moved to " + ci.name + ".", ci.x, ci.y);
                break;
            }
    }
}

// ============================================================ units
int Game::spawnUnit(int c, int type, int x, int y) {
    Unit u;
    u.id = (int)units.size();
    u.type = type;
    u.owner = c;
    u.x = x; u.y = y;
    units.push_back(u);
#ifdef REBIRTH_STRESS_REALLOC
    units.shrink_to_fit();
#endif
    Unit& r = units.back();
    r.moves = maxMoves(r);
    return r.id;
}

void Game::killUnit(Unit& u) {
    u.alive = false;
    u.order = ORD_NONE;
    if (g_units[u.type].naval())  // everyone aboard goes down with the ship
        for (auto& o : units)
            if (o.alive && o.carrier == u.id) { o.alive = false; o.order = ORD_NONE; }
    u.carrier = -1;
}

Unit* Game::militaryAt(int x, int y) {
    // a tile can hold a land unit and a ship: at sea the ship defends, on land (a harbour city) the land unit
    Unit* any = nullptr;
    bool water = isWater(x, y);
    for (auto& u : units) {
        if (!u.alive || u.x != x || u.y != y || !g_units[u.type].military()) continue;
        if (g_units[u.type].naval() == water) return &u;
        if (!any) any = &u;
    }
    return any;
}

bool Game::isCoastal(int x, int y) const {
    Pt nb[6];
    int n = neighbors(x, y, nb);
    for (int k = 0; k < n; k++) if (isWater(nb[k].x, nb[k].y)) return true;
    return false;
}
Unit* Game::civilianAt(int x, int y) {
    for (auto& u : units) if (u.alive && u.x == x && u.y == y && g_units[u.type].civilian()) return &u;
    return nullptr;
}
std::vector<Unit*> Game::unitsAt(int x, int y) {
    std::vector<Unit*> v;
    for (auto& u : units) if (u.alive && u.x == x && u.y == y) v.push_back(&u);
    return v;
}

int Game::maxMoves(const Unit& u) const {
    if (inMap(u.x, u.y) && isWater(u.x, u.y) && !g_units[u.type].naval()) return hasTech(u.owner, ID.tNavigation) ? 3 : 2;
    int m = g_units[u.type].moves;
    const PrimateDef* pd = primate(u.owner);
    if (pd && g_units[u.type].military()) m += pd->extraMoves;
    return m;
}

bool Game::canEmbark(int c, int x, int y) const {
    if (c < FIRST_PRIMATE_CIV) return false;
    if (at(x, y).terrain == T_COAST) return hasTech(c, ID.tRafts);
    return hasTech(c, ID.tNavigation);
}

int Game::moveCost(const Unit& u, int fx, int fy, int tx, int ty) const {
    const Tile& t = at(tx, ty);
    const UnitDef& ud = g_units[u.type];
    if (ud.naval()) {  // ships: water, or into one of our own coastal cities
        if (isWater(tx, ty)) return t.terrain == T_OCEAN && !ud.ocean ? -1 : 1;
        const City* ci = cityAt(tx, ty);
        return ci && ci->owner == u.owner ? 1 : -1;
    }
    if (t.mountain) return -1;
    if (!warHappened && t.site >= 0) return -1;  // living human settlements are off limits
    // closed borders: at peace, military units stay out of other civilisations' lands
    if (t.owner >= FIRST_PRIMATE_CIV && t.owner != u.owner && u.owner >= FIRST_PRIMATE_CIV && ud.military() && ud.cls != UnitClass::Recon &&
        !civs[u.owner].war[t.owner] && !bordersOpenTo(t.owner, u.owner) && at(fx, fy).owner != t.owner)
        return -1;
    bool toWater = isWater(tx, ty), fromWater = isWater(fx, fy);
    if (toWater && ud.cls != UnitClass::Nuke && transportAt(u.owner, tx, ty, u.id)) return 1;  // going aboard one of our ships
    if (toWater && !canEmbark(u.owner, tx, ty)) return -1;
    if (toWater != fromWater) return 99;  // embark/disembark uses all movement
    if (toWater) return 1;
    int cost = 1;
    if (t.hills) cost++;
    if (t.veg != V_NONE) {
        const PrimateDef* pd = primate(u.owner);
        if (!(pd && pd->forestWalker)) cost++;
    }
    return cost;
}

bool Game::canEnter(const Unit& u, int x, int y) const {
    if (const City* c = cityAt(x, y)) {
        if (c->owner != u.owner) return false;
    }
    // one civilian, one land military unit and one ship may share a tile; units aboard a ship don't count
    auto layer = [](const UnitDef& d) { return d.civilian() ? 0 : d.naval() ? 2 : 1; };
    int mine = layer(g_units[u.type]);
    bool boarding = !g_units[u.type].naval() && isWater(x, y) && transportAt(u.owner, x, y, u.id);
    for (auto& o : units) {
        if (!o.alive || o.x != x || o.y != y || o.id == u.id) continue;
        if (o.owner == u.owner && (o.carrier >= 0 || boarding)) continue;
        if (o.owner != u.owner) return false;
        if (layer(g_units[o.type]) == mine) return false;
    }
    return true;
}

std::vector<Pt> Game::findPath(const Unit& u, int tx, int ty, bool ignoreUnits) const {
    std::vector<Pt> path;
    if (!inMap(tx, ty) || (tx == u.x && ty == u.y)) return path;
    int N = W * H;
    std::vector<int> g(N, 1 << 29), from(N, -1);
    std::vector<uint8_t> closed(N, 0);
    typedef std::pair<int, int> QE;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
    int s = idx(u.x, u.y), goal = idx(tx, ty);
    int mm = std::max(1, g_units[u.type].moves);
    g[s] = 0;
    pq.push({dist(u.x, u.y, tx, ty), s});
    int expanded = 0;
    while (!pq.empty()) {
        int cur = pq.top().second;
        pq.pop();
        if (closed[cur]) continue;
        closed[cur] = 1;
        if (cur == goal) break;
        if (++expanded > 6000) break;
        int cx = cur % W, cy = cur / W;
        Pt nb[6];
        int n = neighbors(cx, cy, nb);
        for (int k = 0; k < n; k++) {
            int ni = idx(nb[k].x, nb[k].y);
            if (closed[ni]) continue;
            int mc = moveCost(u, cx, cy, nb[k].x, nb[k].y);
            if (mc < 0) continue;
            if (mc > mm) mc = mm;
            if (!ignoreUnits && !canEnter(u, nb[k].x, nb[k].y)) {
                // allow walking through own friendly units except final tile
                bool friendlyOnly = true;
                for (auto& o : units)
                    if (o.alive && o.x == nb[k].x && o.y == nb[k].y && o.owner != u.owner) friendlyOnly = false;
                const City* cc = cityAt(nb[k].x, nb[k].y);
                if (cc && cc->owner != u.owner) friendlyOnly = false;
                if (!friendlyOnly || ni == goal) continue;
                mc += 1;  // slight penalty
            }
            int ng = g[cur] + mc;
            if (ng < g[ni]) {
                g[ni] = ng;
                from[ni] = cur;
                pq.push({ng + dist(nb[k].x, nb[k].y, tx, ty), ni});
            }
        }
    }
    if (from[goal] < 0) return path;
    for (int cur = goal; cur != s; cur = from[cur]) path.push_back({cur % W, cur / W});
    std::reverse(path.begin(), path.end());
    return path;
}

void revealAround(Game& g, int c, int x, int y, int r);

bool Game::stepUnit(Unit& u, int nx, int ny) {
    if (u.moves <= 0 || dist(u.x, u.y, nx, ny) != 1) return false;
    int mc = moveCost(u, u.x, u.y, nx, ny);
    if (mc < 0) return false;
    // capture lone enemy civilians
    if (g_units[u.type].military()) {
        Unit* civ = civilianAt(nx, ny);
        if (civ && civ->owner != u.owner && atWar(u.owner, civ->owner) && !militaryAt(nx, ny) && !cityAt(nx, ny)) {
            if (g_units[civ->type].cls == UnitClass::Band) {
                hurtBand(*civ, civ->band >= 6 ? 2 : 1, "attacked by " + (u.owner == CIV_FERAL ? std::string("predators") : "the " + civs[u.owner].name));
                if (civ->alive) return false;  // the survivors stand their ground
            } else if (u.owner != CIV_FERAL && g_units[civ->type].cls == UnitClass::Worker) {
                notify(civ->owner, "Your " + g_units[civ->type].name + " was captured!", nx, ny);
                civ->owner = u.owner;
                civ->type = uniqueFor(u.owner, ID.uGatherer);
                civ->order = ORD_NONE;
                civ->moves = 0;
                notify(u.owner, "You captured an enemy Gatherer.", nx, ny);
            } else {
                notify(civ->owner, "Your " + g_units[civ->type].name + " was killed!", nx, ny);
                killUnit(*civ);
            }
        }
    }
    if (!canEnter(u, nx, ny)) return false;
    bool wasWater = isWater(u.x, u.y);
    if (!g_units[u.type].naval()) {  // board, change ships or step ashore
        const Unit* ship = isWater(nx, ny) ? transportAt(u.owner, nx, ny, u.id) : nullptr;
        int before = u.carrier;
        u.carrier = ship ? ship->id : -1;
        if (ship && before < 0) {
            civs[u.owner].unitsFerried += u.owner >= FIRST_PRIMATE_CIV ? 1 : 0;
            u.x = nx; u.y = ny;
            u.moves = 0;
            u.order = ORD_SLEEP;
            u.acted = true;
            u.fortTurns = 0;
            if (u.owner >= FIRST_PRIMATE_CIV) revealAround(*this, u.owner, nx, ny, sightRange(u));
            return true;
        }
    }
    u.x = nx; u.y = ny;
    if (g_units[u.type].naval())  // the cargo sails with the ship
        for (auto& o : units)
            if (o.alive && o.carrier == u.id) { o.x = nx; o.y = ny; }
    if (mc >= 99) u.moves = 0;
    else u.moves = std::max(0, u.moves - mc);
    if (wasWater != isWater(nx, ny) && !g_units[u.type].naval()) u.moves = 0;  // embarking/landing
    u.acted = true;
    if (u.order == ORD_FORTIFY || u.order == ORD_SLEEP) u.order = ORD_NONE;
    u.fortTurns = 0;
    if (u.owner >= FIRST_PRIMATE_CIV) revealAround(*this, u.owner, nx, ny, sightRange(u));
    return true;
}

void Game::continueGoto(Unit& u) {
    if (u.order != ORD_GOTO) return;
    int guard = 0;
    while (u.moves > 0 && guard++ < 20) {
        if (u.x == u.gx && u.y == u.gy) { u.order = ORD_NONE; return; }
        std::vector<Pt> p = findPath(u, u.gx, u.gy);
        if (p.empty()) { u.order = ORD_NONE; return; }
        // stop before stepping next to danger? keep simple
        if (!stepUnit(u, p[0].x, p[0].y)) return;  // blocked: try again next turn
    }
    if (u.x == u.gx && u.y == u.gy) u.order = ORD_NONE;
}

bool Game::isEnemyTarget(int c, int tx, int ty) const {
    if (const City* ci = cityAt(tx, ty)) if (atWar(c, ci->owner)) return true;
    for (auto& o : units)
        if (o.alive && o.x == tx && o.y == ty && atWar(c, o.owner)) return true;
    return false;
}

bool Game::canAttack(const Unit& u, int tx, int ty) const {
    const UnitDef& d = g_units[u.type];
    if (!d.military() || u.attacked || u.moves <= 0 || u.carrier >= 0) return false;
    if (!isEnemyTarget(u.owner, tx, ty)) return false;
    int dd = dist(u.x, u.y, tx, ty);
    if (d.ranged()) return dd <= d.range && dd >= 1;
    if (dd != 1) return false;
    if (d.naval()) return isWater(tx, ty) || cityAt(tx, ty) != nullptr;  // rams ships and boats, or storms a coastal city
    if (isWater(u.x, u.y) || isWater(tx, ty)) return false;
    return moveCost(u, u.x, u.y, tx, ty) >= 0 || cityAt(tx, ty) != nullptr;
}

bool Game::orderMoveOrAttack(Unit& u, int tx, int ty) {
    if (!inMap(tx, ty) || (tx == u.x && ty == u.y)) return false;
    const UnitDef& d = g_units[u.type];
    if (isEnemyTarget(u.owner, tx, ty)) {
        if (!d.military()) return false;
        if (canAttack(u, tx, ty)) { attack(u, tx, ty); return true; }
        if (d.ranged()) return false;
        // walk next to the target, then attack if any movement remains
        std::vector<Pt> p = findPath(u, tx, ty, true);
        if (p.size() < 2) return false;
        Pt stand = p[p.size() - 2];
        u.order = ORD_GOTO; u.gx = stand.x; u.gy = stand.y;
        continueGoto(u);
        if (u.x == stand.x && u.y == stand.y && canAttack(u, tx, ty)) attack(u, tx, ty);
        return true;
    }
    u.order = ORD_GOTO; u.gx = tx; u.gy = ty;
    continueGoto(u);
    return true;
}

int Game::unitStrength(const Unit& u, bool attacking, bool rangedAtk, int x, int y) const {
    const UnitDef& d = g_units[u.type];
    int base = rangedAtk ? d.rstr : d.str;
    if (base <= 0) base = 1;
    bool water = isWater(x, y);
    if (water && !attacking && u.owner >= FIRST_PRIMATE_CIV && !d.naval()) base = std::min(base, 4 + era(u.owner) * 4);  // embarked
    int pct = 0;
    if (const PrimateDef* pd = primate(u.owner)) pct += pd->combatPct;
    if (u.owner >= FIRST_PRIMATE_CIV) {
        pct += policyPct(u.owner, PolicyEffect::Combat);
        pct += civs[u.owner].fx.combat;
        if (ownsWonderSpecial(u.owner, BSpecial::CombatBonus)) pct += 10;
    }
    if (!attacking && !water) {
        const Tile& t = at(x, y);
        if (t.hills) pct += 25;
        if (t.veg != V_NONE) pct += 25;
        if (u.order == ORD_FORTIFY && u.fortTurns >= 1) pct += 25;
    }
    double s = base * 10.0 * (100 + pct) / 100.0;
    s *= (50.0 + u.hp / 2.0) / 100.0;
    return std::max(1, (int)s);
}

int Game::combatDamage(double a, double d) {
    if (d < 1) d = 1;
    double r = a / d;
    double dmg = 30.0 * std::pow(r, 1.25) * (0.8 + 0.4 * rng.unit());
    return std::clamp((int)dmg, 1, 100);
}

CombatResult Game::attack(Unit& u, int tx, int ty) {
    CombatResult res;
    if (!canAttack(u, tx, ty)) return res;
    const UnitDef& d = g_units[u.type];
    bool ranged = d.ranged();
    res.happened = true;
    civs[u.owner].battles++;
    if (const City* tc = cityAt(tx, ty)) civs[tc->owner].battles++;
    for (auto& o : units) if (o.alive && o.x == tx && o.y == ty && o.owner != u.owner) { civs[o.owner].battles++; break; }
    std::string an = (u.owner == player ? "Your " : u.owner == CIV_FERAL ? std::string("Wild ") : civs[u.owner].adjective + " ") + d.name;
    City* city = cityAt(tx, ty);
    if (city && atWar(u.owner, city->owner)) {
        meet(u.owner, city->owner);
        double A = unitStrength(u, true, ranged, u.x, u.y);
        A *= d.cls == UnitClass::Siege ? 3.0 : 1.5;  // cities are built to be besieged
        double D = cityStrength(*city);
        res.dmgToDef = combatDamage(A, D);
        city->hp -= res.dmgToDef;
        if (!ranged) {
            res.dmgToAtt = combatDamage(D, A);
            u.hp -= res.dmgToAtt;
        }
        if (u.hp <= 0) {
            if (city->hp <= 0) { u.hp = 1; }
            else { res.attackerDied = true; }
        }
        std::string msg = an + " attacked " + city->name + " (-" + std::to_string(res.dmgToDef) + " HP)";
        if (res.attackerDied) msg += " and was destroyed";
        if (city->hp <= 0) {
            if (!ranged && !res.attackerDied && u.owner != CIV_FERAL) {
                res.cityCaptured = true;
                captureCity(*city, u.owner);
                if (city->alive) { u.x = tx; u.y = ty; }
            } else {
                city->hp = 1;
            }
        }
        notify(u.owner, msg, tx, ty);
        if (city->owner != u.owner) notify(city->owner, msg, tx, ty);
        if (res.attackerDied) killUnit(u);
    } else {
        Unit* def = militaryAt(tx, ty);
        if (!def) def = civilianAt(tx, ty);
        if (!def) return res;
        meet(u.owner, def->owner);
        double A = unitStrength(u, true, ranged, u.x, u.y);
        double D = unitStrength(*def, false, false, tx, ty);
        res.dmgToDef = combatDamage(A, D);
        if (g_units[def->type].civilian()) res.dmgToDef = 100;
        if (g_units[def->type].cls == UnitClass::Band) {
            // bands scatter rather than die: they lose members until none are left
            int loss = def->band >= 6 ? 2 : def->band >= 3 && rng.chance(0.4) ? 0 : 1;
            res.dmgToDef = 0;
            res.dmgToAtt = ranged ? 0 : std::min(45, 10 + 5 * def->band);  // the band fights back
            std::string by = u.owner == CIV_FERAL ? std::string("attacked by predators") : "attacked by the " + civs[u.owner].name;
            res.defenderDied = hurtBand(*def, loss, by);
            if (!res.defenderDied) {
                u.hp = std::max(1, u.hp - res.dmgToAtt);
                notify(u.owner, "Your " + d.name + " attacked a " + civs[def->owner].adjective + " band.", tx, ty);
                u.attacked = true; u.acted = true; u.moves = 0; u.order = ORD_NONE; u.fortTurns = 0;
                return res;
            }
        }
        if (!ranged && g_units[def->type].military()) res.dmgToAtt = combatDamage(D, A);
        def->hp -= res.dmgToDef;
        u.hp -= res.dmgToAtt;
        if (def->hp <= 0) res.defenderDied = true;
        if (u.hp <= 0) {
            if (res.defenderDied) u.hp = 1;
            else res.attackerDied = true;
        }
        std::string dn = (def->owner == player ? "your " : def->owner == CIV_FERAL ? std::string("wild ") : civs[def->owner].adjective + " ") + g_units[def->type].name;
        std::string msg = an + " attacked " + dn + ": dealt " + std::to_string(res.dmgToDef) + ", took " + std::to_string(res.dmgToAtt);
        if (res.defenderDied) msg += ". The " + g_units[def->type].name + " was destroyed";
        if (res.attackerDied) msg += ". The attacker was destroyed";
        notify(u.owner, msg, tx, ty);
        notify(def->owner, msg, tx, ty);
        int dx = tx, dy = ty;
        if (res.defenderDied && g_units[def->type].naval() && u.owner >= FIRST_PRIMATE_CIV) civs[u.owner].shipsSunk++;
        if (res.defenderDied) killUnit(*def);
        if (res.attackerDied) killUnit(u);
        else if (res.defenderDied && !ranged) {
            // advance into the tile (capturing any civilian)
            u.attacked = true;
            int keep = u.moves;
            u.moves = 1;
            Unit tmp = u;
            if (moveCost(tmp, u.x, u.y, dx, dy) >= 0) stepUnit(u, dx, dy);
            u.moves = keep;
        }
    }
    u.attacked = true;
    u.acted = true;
    u.moves = 0;
    u.order = ORD_NONE;
    u.fortTurns = 0;
    return res;
}

int Game::sightRange(const Unit& u) const {
    int r = 2;
    if (g_units[u.type].cls == UnitClass::Recon) r++;
    if (inMap(u.x, u.y) && at(u.x, u.y).hills) r++;
    return r;
}

std::vector<int> Game::buildOptions(const Unit& u) const {
    std::vector<int> out;
    if (g_units[u.type].cls != UnitClass::Worker) return out;
    const Tile& t = at(u.x, u.y);
    if (isWater(u.x, u.y) || t.mountain || t.site >= 0 || t.owner != u.owner || cityAt(u.x, u.y)) return out;
    if (t.fallout >= 2) return out;
    for (int im = 1; im < IMP_COUNT; im++) {
        if (t.imp == im || !hasTech(u.owner, g_improvements[im].techId)) continue;
        bool ok = false;
        bool vis = resourceVisible(u.owner, u.x, u.y);
        int r = vis ? (int)t.res : (int)R_NONE;
        switch (im) {
            case IMP_FARM: ok = t.veg == V_NONE && t.terrain != T_SNOW && !isMineral(r) && r != R_HORSES; break;
            case IMP_MINE: ok = t.hills || isMineral(r); break;
            case IMP_LUMBER: ok = t.veg == V_FOREST && !isMineral(r); break;
            case IMP_TRADEPOST: ok = t.veg != V_FOREST && !t.hills && !isMineral(r) && !isCrop(r) && r != R_HORSES; break;
            case IMP_PASTURE: ok = r == R_HORSES; break;
        }
        if (ok) out.push_back(im);
    }
    return out;
}

int Game::excavateTierFor(int c, int x, int y) const {
    if (!warHappened || c < FIRST_PRIMATE_CIV) return -1;
    const Tile& t = at(x, y);
    if (t.ruin == RUIN_NONE) return -1;
    int mask = g_ruins[t.ruin].tierMask;
    for (int k = 0; k < RUIN_TIERS; k++) {
        if (!(mask & (1 << k)) || (t.ruinDone & (1 << k))) continue;
        return hasTech(c, g_ruinTierTech[k]) ? k : -1;
    }
    return -1;
}

int Game::excavateTier(const Unit& u) const {
    if (g_units[u.type].cls != UnitClass::Worker) return -1;
    return excavateTierFor(u.owner, u.x, u.y);
}

bool Game::canDecon(const Unit& u) const {
    return g_units[u.type].cls == UnitClass::Worker && at(u.x, u.y).fallout > 0 && hasTech(u.owner, ID.tRadiology);
}

int Game::workTurns(const Unit& u, int base) const {
    int t = base;
    if (u.type == ID.uTinkerer) t -= 2;
    if (u.owner >= FIRST_PRIMATE_CIV) t -= civs[u.owner].fx.workFaster;
    return std::max(1, t);
}

void Game::startBuild(Unit& u, int imp) {
    u.order = ORD_BUILD;
    u.buildImp = imp;
    u.work = workTurns(u, g_improvements[imp].turns);
    u.moves = 0;
}
void Game::startExcavate(Unit& u) {
    if (excavateTier(u) < 0) return;
    u.order = ORD_EXCAVATE;
    int base = 4;
    if (const PrimateDef* pd = primate(u.owner)) base -= pd->excavateFaster;
    u.work = workTurns(u, base);
    u.moves = 0;
}
void Game::startDecon(Unit& u) {
    if (!canDecon(u)) return;
    u.order = ORD_DECON;
    u.work = workTurns(u, 4);
    u.moves = 0;
}

bool Game::unitNeedsOrders(const Unit& u) const {
    return u.alive && u.moves > 0 && u.order == ORD_NONE;
}

void Game::excavateReward(int c, int x, int y, int tier) {
    Tile& t = at(x, y);
    t.ruinDone |= (uint8_t)(1 << tier);
    Civ& cv = civs[c];
    cv.ruinsExcavated++;
    int mult = 100 + policyPct(c, PolicyEffect::RuinBonus);
    if (const PrimateDef* pd = primate(c)) mult += pd->ruinPct;
    if (ownsWonderSpecial(c, BSpecial::RuinBonus)) mult += 100;
    // diminishing returns: the richest finds come first, later digs mostly confirm what is already known
    mult = mult * 12 / (12 + cv.ruinsExcavated);
    std::string site = t.site >= 0 ? sites[t.site].name : g_ruins[t.ruin].ruinName;
    if (t.site >= 0 && t.ruin == RUIN_CITY) site = "the ruins of " + site;
    else site = "a " + g_ruins[t.ruin].ruinName;
    int curCost = cv.research >= 0 ? techCost(cv.research) : 60 + 40 * era(c);
    std::string body;
    switch (tier) {
        case 0: {
            int sci = (15 + 10 * era(c)) * mult / 100;
            int prod = 25 * mult / 100;
            City* best = nullptr;
            for (auto& ci : cities)
                if (ci.alive && ci.owner == c && (!best || dist(ci.x, ci.y, x, y) < dist(best->x, best->y, x, y))) best = &ci;
            if (best) best->prod += prod;
            addScience(c, sci);
            body = "Your Gatherers picked through " + site + " and recovered useful scrap.\n\n+" + std::to_string(sci) + " Science" +
                   (best ? ", +" + std::to_string(prod) + " Production in " + best->name : std::string());
            break;
        }
        case 1: {
            int sci = std::max(30, curCost * 3 / 10) * mult / 100;
            addScience(c, sci);
            body = "Among the ashes of " + site + " your scholars found human books that can finally be read.\n\n+" + std::to_string(sci) + " Science";
            break;
        }
        case 2: {
            if (t.ruin == RUIN_CITY) {
                int sci = curCost / 2 * mult / 100;
                addScience(c, sci);
                body = "Your engineers took apart the machines of " + site + " and learned many of their secrets.\n\n+" + std::to_string(sci) + " Science";
            } else {
                int sci = curCost * 2 / 5 * mult / 100;
                addScience(c, sci);
                body = "Machines recovered from " + site + " were reverse-engineered.\n\n+" + std::to_string(sci) + " Science";
                if (t.ruin == RUIN_MILITARY) {
                    int best = -1, ship = -1;
                    for (int ut : availableUnits(c)) {
                        const UnitDef& ud = g_units[ut];
                        if (ud.landMilitary() && (best < 0 || ud.str > g_units[best].str)) best = ut;
                        if (ud.naval() && (ship < 0 || std::max(ud.str, ud.rstr) > std::max(g_units[ship].str, g_units[ship].rstr))) ship = ut;
                    }
                    Pt dock = {-1, -1};  // a naval base: raise a warship from the harbour instead
                    if (ship >= 0 && isCoastal(x, y) && rng.chance(0.5))
                        for (auto& p : ring(x, y, 1))
                            if (isWater(p.x, p.y) && unitsAt(p.x, p.y).empty() && (at(p.x, p.y).terrain == T_COAST || g_units[ship].ocean)) { dock = p; break; }
                    if (dock.x >= 0) {
                        spawnUnit(c, ship, dock.x, dock.y);
                        body += "\n\nA warship was raised from the naval base's harbour: you gain a " + g_units[ship].name + ".";
                    } else if (best >= 0 && !militaryAt(x, y)) {
                        spawnUnit(c, best, x, y);
                        body += "\n\nAn armoury was restored: you gain a " + g_units[best].name + ".";
                    }
                }
            }
            break;
        }
        case 3: {
            body = "Human data archives in " + site + " were brought back online!";
            int sci = curCost * (t.ruin == RUIN_CITY ? 3 : 2) / 5 * mult / 100;
            addScience(c, sci);
            body += "\n+" + std::to_string(sci) + " Science";
            break;
        }
    }
    notify(c, std::string("Excavated ") + RUIN_TIER_NAME[tier] + " at " + site + ".", x, y);
    popup(c, POP_RUIN, std::string("Ruins Excavated: ") + RUIN_TIER_NAME[tier], body, x, y);
}

// ============================================================ messages
void Game::notify(int c, const std::string& text, int x, int y) {
    if (c != player) return;
    Notification n;
    n.turn = turn;
    n.text = text;
    n.x = x; n.y = y;
    log.push_back(n);
    if (log.size() > 300) log.erase(log.begin(), log.begin() + 50);
}

void Game::popup(int c, int kind, const std::string& title, const std::string& body, int x, int y) {
    if (c != player || !civs[c].player) return;
    Popup p;
    p.kind = kind; p.title = title; p.body = body; p.x = x; p.y = y;
    popups.push_back(p);
}
