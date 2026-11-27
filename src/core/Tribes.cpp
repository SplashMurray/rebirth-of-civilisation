// Rebirth of Civilisation - minor tribes
// Small one-city peoples of other primate species, living among the great powers. Each major civ has
// an influence with each tribe it has met: gifts of gold, trade routes to their home and help with
// their troubles raise it, and it slowly fades back to nothing. Friends (30+) receive the tribe's gift
// every turn; the civ with the most influence (60+) is its ally: a double gift, the tribe's luxuries
// and strategic resources, and its spears in every war.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

int Game::numMajors() const {
    int n = 0;
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) if (!civs[c].minor) n++;
    return n;
}

std::string Game::tribeTitle(int t) const {
    if (!isMinor(t)) return civs[t].name;
    return "the " + civs[t].name + " of " + g_tribes[civs[t].tribe].city;
}

int Game::influenceFor(int m, int t) const {
    if (!isMinor(t)) return 0;
    const Civ& tv = civs[t];
    return m >= 0 && m < (int)tv.influence.size() ? tv.influence[m] : 0;
}

int Game::tribeStatus(int m, int t) const {
    if (civs[m].war[t]) return TS_WAR;
    if (civs[t].ally == m) return TS_ALLY;
    return influenceFor(m, t) >= TRIBE_FRIEND ? TS_FRIEND : TS_NEUTRAL;
}

void Game::addInfluence(int m, int t, int amount, const std::string& why) {
    if (!isMajor(m) || !isMinor(t) || !civs[t].alive || amount == 0) return;
    Civ& tv = civs[t];
    if ((int)tv.influence.size() < (int)civs.size()) tv.influence.resize(civs.size(), 0);
    tv.influence[m] = std::clamp(tv.influence[m] + amount, -100, 160);
    if (!why.empty())
        notify(m, (amount > 0 ? "+" : "") + std::to_string(amount) + " influence with " + tribeTitle(t) + ": " + why + ".",
               cities[std::max(0, tv.capital)].x, cities[std::max(0, tv.capital)].y);
}

int Game::giftInfluence(int m, int t, int gold) const {
    (void)t;
    return gold * 30 / (60 + 20 * era(m));
}

bool Game::giftGold(int m, int t, int gold) {
    if (!isMajor(m) || !isMinor(t) || !civs[t].alive || !civs[m].met[t] || civs[m].war[t]) return false;
    if (!hasGold(m) || gold <= 0 || civs[m].gold < gold) return false;
    civs[m].gold -= gold;
    addInfluence(m, t, giftInfluence(m, t, gold));
    notify(m, "You gave " + std::to_string(gold) + " gold to " + tribeTitle(t) + ".");
    return true;
}

bool Game::allyResource(int c, int r) const {
    if (!isMajor(c)) return false;
    if (allyResTurn != turn || (int)allyRes.size() != (int)civs.size()) {
        allyRes.assign(civs.size(), std::vector<uint8_t>(R_COUNT, 0));
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const Tile& t = at(x, y);
                if (t.res == R_NONE || t.owner < 0 || !isMinor(t.owner)) continue;
                int a = civs[t.owner].ally;
                if (a >= 0 && civs[t.owner].alive) allyRes[a][t.res] = 1;
            }
        allyResTurn = turn;
    }
    return allyRes[c][r] && hasTech(c, g_resources[r].revealTechId);
}

std::string Game::tribeBonusText(int t) const {
    switch (g_tribes[civs[t].tribe].kind) {
        case TK_CULTURED: return "Friends receive Culture every turn, allies twice as much";
        case TK_LEARNED: return "Friends receive Science every turn, allies twice as much";
        case TK_MERCANTILE: return "Friends receive Gold every turn, allies twice as much";
        case TK_MARITIME: return "Friends get +1 Food in their capital; allies +2 there and +1 in every city";
        default: return "Friends are sent a trained unit now and then; allies more often";
    }
}

static std::string turnsLeft(int n) {
    n = std::max(0, n);
    return std::to_string(n) + (n == 1 ? " turn left" : " turns left");
}

std::string Game::questText(int t) const {
    const Civ& tv = civs[t];
    switch (tv.questKind) {
        case TQ_ROUTE: return "Send a trade route to " + g_tribes[tv.tribe].city + " (" + turnsLeft(tv.questUntil - turn) + ")";
        case TQ_FERALS: return "Destroy the " + (tv.questTarget >= 0 && tv.questTarget < (int)units.size() ? g_units[units[tv.questTarget].type].name : std::string("beasts")) +
                               " troubling them (" + turnsLeft(tv.questUntil - turn) + ")";
        default: return "";
    }
}

// ------------------------------------------------------------ setting up
void Game::placeTribes(const std::vector<Pt>& starts) {
    std::vector<Pt> placed;
    for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++) {
        if (!civs[t].minor) continue;
        int best = -1, bs = -1;
        for (int tries = 0; tries < 3000; tries++) {
            int x = rng.range(W), y = rng.range(H);
            if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2) continue;
            const Tile& tile = at(x, y);
            if (isWater(x, y) || tile.mountain || tile.terrain == T_SNOW || tile.site >= 0 || tile.feature == F_WONDER || tile.feature == F_OASIS || tile.owner >= 0)
                continue;
            bool ok = true;
            for (auto& s : starts) if (dist(s.x, s.y, x, y) < 8) ok = false;
            for (auto& p : placed) if (dist(p.x, p.y, x, y) < 7) ok = false;
            for (auto& s : sites) if (dist(s.x, s.y, x, y) < 4) ok = false;
            if (!ok) continue;
            int sc = 0;
            for (auto& p : ring(x, y, 2)) {
                Yields yy = tileYields(-1, p.x, p.y);
                sc += yy.food * 3 + yy.prod * 2;
            }
            if (sc > bs) { bs = sc; best = idx(x, y); }
            if (tries > 400 && best >= 0) break;
        }
        if (best < 0) { civs[t].alive = false; continue; }
        int x = best % W, y = best / W;
        placed.push_back({x, y});
        int id = foundCity(t, x, y);
        City& ci = cities[id];
        ci.name = g_tribes[civs[t].tribe].city;
        ci.origCapital = false;  // not one of the great capitals a conqueror must take
        ci.pop = 2;
        for (auto& p : ring(x, y, 2))
            if (at(p.x, p.y).owner < 0 && !isWater(p.x, p.y)) claimTile(ci, p.x, p.y);
        computeCity(ci);
        spawnUnit(t, ID.uWarband, x, y);
    }
}

// ------------------------------------------------------------ every turn
void Game::tribesTurn() {
    int majorsAlive = 0;
    for (int m = FIRST_PRIMATE_CIV; m < (int)civs.size(); m++)
        if (isMajor(m)) {
            civs[m].tribeFoodCapital = civs[m].tribeFoodAll = 0;
            if (civs[m].alive) majorsAlive++;
        }
    for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++) {
        Civ& tv = civs[t];
        if (!tv.minor || !tv.alive || tv.capital < 0) continue;
        if ((int)tv.influence.size() < (int)civs.size()) tv.influence.resize(civs.size(), 0);
        if ((int)tv.questDone.size() < (int)civs.size()) tv.questDone.resize(civs.size(), 0);
        if ((int)tv.lastUnitGift.size() < (int)civs.size()) tv.lastUnitGift.resize(civs.size(), -100);
        const City& home = cities[tv.capital];
        if (!home.alive || home.owner != t) continue;
        const TribeDef& td = g_tribes[tv.tribe];
        // they learn what most of the great powers know
        for (size_t k = 0; k < g_techs.size(); k++) {
            if (tv.techs[k]) continue;
            int n = 0;
            for (int m = FIRST_PRIMATE_CIV; m < (int)civs.size(); m++) if (isMajor(m) && civs[m].alive && civs[m].techs[k]) n++;
            if (n >= 1 && n * 2 >= majorsAlive) tv.techs[k] = 1;
        }
        tv.gold = 0;
        tv.culture = 0;
        // influence fades towards nothing; caravans to their home raise it
        for (int m = FIRST_PRIMATE_CIV; m < (int)civs.size(); m++) {
            if (!isMajor(m) || !civs[m].alive || !civs[m].met[t] || tv.war[m]) continue;
            int& inf = tv.influence[m];
            if (inf > 0 && turn % (tv.ally == m ? 1 : 1) == 0) inf--;
            else if (inf < 0) inf++;
        }
        for (auto& r : routes)
            if (r.to == home.id && isMajor(r.owner) && !tv.war[r.owner]) tv.influence[r.owner] = std::min(160, tv.influence[r.owner] + 2);
        // their warriors are re-armed as they learn
        for (auto& u : units) {
            if (!u.alive || u.owner != t) continue;
            for (int guard = 0; guard < 6; guard++) {
                int up = g_units[u.type].upgradeId;
                if (up < 0 || !hasTech(t, g_units[up].techId) || !hasNeeds(t, g_units[up].needs)) break;
                u.type = up;
            }
        }
        // quests
        if (tv.questKind != TQ_NONE) {
            bool over = turn >= tv.questUntil;
            if (tv.questKind == TQ_FERALS &&
                (tv.questTarget < 0 || tv.questTarget >= (int)units.size() || !units[tv.questTarget].alive || units[tv.questTarget].owner != CIV_FERAL))
                over = true;
            if (tv.questKind == TQ_ROUTE)
                for (auto& r : routes)
                    if (r.to == home.id && isMajor(r.owner) && !tv.questDone[r.owner]) {
                        tv.questDone[r.owner] = 1;
                        civs[r.owner].questsCompleted++;
                        addInfluence(r.owner, t, 40, "you sent them a trade route as they asked");
                    }
            if (over) tv.questKind = TQ_NONE;
        } else if (turn > dur(20) && rng.chance(1.0 / dur(10))) {
            int feral = -1, fd = 99;
            for (auto& u : units)
                if (u.alive && u.owner == CIV_FERAL && dist(u.x, u.y, home.x, home.y) <= 5 && dist(u.x, u.y, home.x, home.y) < fd) {
                    fd = dist(u.x, u.y, home.x, home.y);
                    feral = u.id;
                }
            bool traders = false;
            for (int m = FIRST_PRIMATE_CIV; m < (int)civs.size(); m++)
                if (isMajor(m) && civs[m].alive && civs[m].met[t] && tradeCapacity(m) > 0) traders = true;
            if (feral >= 0) { tv.questKind = TQ_FERALS; tv.questTarget = feral; }
            else if (traders) tv.questKind = TQ_ROUTE;
            if (tv.questKind != TQ_NONE) {
                tv.questUntil = turn + dur(40);
                std::fill(tv.questDone.begin(), tv.questDone.end(), 0);
                for (int v : viewers())
                    if (civs[v].met[t] && !civs[v].war[t] && civs[v].alive) notify(v, "The " + civs[t].name + " of " + td.city + " ask for help: " + questText(t) + ". Reward: +40 influence.", home.x, home.y);
            }
        }
        // who is their ally?
        int old = tv.ally, best = -1, bi = TRIBE_ALLY - 1;
        if (old >= 0 && (!civs[old].alive || tv.war[old] || tv.influence[old] < TRIBE_ALLY)) old = -1;
        if (old >= 0) { best = old; bi = tv.influence[old]; }
        for (int m = FIRST_PRIMATE_CIV; m < (int)civs.size(); m++)
            if (isMajor(m) && civs[m].alive && !tv.war[m] && tv.influence[m] > bi) { best = m; bi = tv.influence[m]; }
        if (best != tv.ally) {
            int was = tv.ally;
            tv.ally = best;
            tv.allySince = turn;
            allyResTurn = -1;
            markResourcesDirty();
            if (best >= 0) popup(best, POP_INFO, "A New Ally", "The " + civs[t].name + " of " + td.city + " are now your allies.\n\n" + tribeBonusText(t) +
                                      ". You may use their luxuries and strategic resources, and they will fight your enemies.", home.x, home.y);
            if (was >= 0) notify(was, "The " + civs[t].name + " of " + td.city + " are no longer your allies" +
                                     (best >= 0 ? ": the " + civs[best].name + " have won them over." : "."), home.x, home.y);
            for (int v : viewers())
                if (v != best && v != was && best >= 0 && civs[v].met[t]) notify(v, "The " + civs[t].name + " of " + td.city + " have allied with the " + civs[best].name + ".", home.x, home.y);
        }
        // allies' enemies are their enemies; old quarrels end when the reason does
        // - but a tribe only takes up arms against an enemy on its doorstep, and only for an ally of some standing
        for (int x = FIRST_PRIMATE_CIV; x < (int)civs.size(); x++) {
            if (!isMajor(x) || !civs[x].alive || x == tv.ally) continue;
            bool allyWar = tv.ally >= 0 && civs[tv.ally].war[x];
            if (allyWar && !tv.war[x] && turn - tv.allySince >= dur(5)) {
                bool near = false;
                for (auto& ci : cities)
                    if (ci.alive && ci.owner == x && dist(ci.x, ci.y, home.x, home.y) <= 10) { near = true; break; }
                if (!near) continue;
                declareWar(t, x);
            } else if (!allyWar && tv.war[x] && tv.influence[x] > -30) {
                makePeace(t, x);
            }
        }
        // gifts to friends and allies
        for (int m = FIRST_PRIMATE_CIV; m < (int)civs.size(); m++) {
            if (!isMajor(m) || !civs[m].alive) continue;
            int st = tribeStatus(m, t);
            if (st < TS_FRIEND) continue;
            int mult = st == TS_ALLY ? 2 : 1;
            int amt = (2 + era(m) / 2) * mult;
            switch (td.kind) {
                case TK_CULTURED: civs[m].culture += amt; civs[m].cultureTotal += amt; break;
                case TK_LEARNED: addScience(m, amt); break;
                case TK_MERCANTILE: if (hasGold(m)) civs[m].gold += amt; break;
                case TK_MARITIME:
                    civs[m].tribeFoodCapital += mult;
                    if (st == TS_ALLY) civs[m].tribeFoodAll += 1;
                    break;
                case TK_MILITANT: {
                    int every = st == TS_ALLY ? dur(12) : dur(22);
                    if (turn - tv.lastUnitGift[m] < every || numCities(m) == 0) break;
                    int type = -1;
                    for (int u : availableUnits(m)) {
                        const UnitDef& d = g_units[u];
                        if (!d.landMilitary() || d.cls == UnitClass::Recon || d.cls == UnitClass::Siege || !hasNeeds(m, d.needs)) continue;
                        if (type < 0 || d.str > g_units[type].str) type = u;
                    }
                    if (type < 0) break;
                    const City* near = nullptr;
                    for (auto& ci : cities)
                        if (ci.alive && ci.owner == m && (!near || dist(ci.x, ci.y, home.x, home.y) < dist(near->x, near->y, home.x, home.y))) near = &ci;
                    if (!near) break;
                    Unit probe;
                    probe.type = type;
                    probe.owner = m;
                    for (auto& p : ring(near->x, near->y, 2)) {
                        probe.x = p.x; probe.y = p.y;
                        if (isWater(p.x, p.y) || at(p.x, p.y).mountain || !unitsAt(p.x, p.y).empty() || !canEnter(probe, p.x, p.y)) continue;
                        if (at(p.x, p.y).owner >= 0 && at(p.x, p.y).owner != m) continue;
                        if (cityAt(p.x, p.y) && cityAt(p.x, p.y)->owner != m) continue;
                        spawnUnit(m, type, p.x, p.y);
                        tv.lastUnitGift[m] = turn;
                        notify(m, "The " + civs[t].name + " of " + td.city + " have sent you " + g_units[type].name + ".", p.x, p.y);
                        break;
                    }
                    break;
                }
            }
        }
    }
    for (int m = FIRST_PRIMATE_CIV; m < (int)civs.size(); m++) {
        if (!isMajor(m)) continue;
        int allies = 0;
        for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++) if (isMinor(t) && civs[t].alive && civs[t].ally == m) allies++;
        civs[m].mostAllies = std::max(civs[m].mostAllies, allies);
    }
}

// A major civ attacked a tribe: it is hated there, and the other tribes grow wary of it.
void Game::tribeDeclaredOn(int m, int t) {
    if (!isMajor(m) || !isMinor(t)) return;
    Civ& tv = civs[t];
    if ((int)tv.influence.size() < (int)civs.size()) tv.influence.resize(civs.size(), 0);
    tv.influence[m] = std::min(tv.influence[m], -60);
    if (tv.ally == m) { tv.ally = -1; allyResTurn = -1; markResourcesDirty(); }
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
        if (o != t && isMinor(o) && civs[o].alive && civs[o].met[m]) addInfluence(m, o, -15);
}

void Game::feralKilled(int killer, const Unit& feral) {
    if (!isMajor(killer)) return;
    for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++) {
        Civ& tv = civs[t];
        if (!tv.minor || !tv.alive || tv.capital < 0 || !civs[killer].met[t] || tv.war[killer]) continue;
        const City& home = cities[tv.capital];
        if (tv.questKind == TQ_FERALS && tv.questTarget == feral.id) {
            tv.questKind = TQ_NONE;
            civs[killer].questsCompleted++;
            addInfluence(killer, t, 40, "you destroyed the beasts that troubled them");
        } else if (dist(home.x, home.y, feral.x, feral.y) <= 4) {
            addInfluence(killer, t, 5, "you killed beasts near their home");
        }
    }
}

bool Game::tribePeaceOk(int m, int t, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    const Civ& tv = civs[t];
    if (!tv.war[m]) return fail("You are not at war");
    if (tv.ally >= 0 && civs[tv.ally].war[m]) return fail("Their allies, the " + civs[tv.ally].name + ", are at war with you");
    if (turn - tv.warSince[m] < dur(10)) return fail("They will not talk peace for " + std::to_string(dur(10) - (turn - tv.warSince[m])) + " more turns");
    return true;
}

// ------------------------------------------------------------ the tribes' own AI: defend the home
void Game::aiMinorTurn(int c) {
    Civ& cv = civs[c];
    if (cv.capital < 0 || !cities[cv.capital].alive || cities[cv.capital].owner != c) return;
    City& home = cities[cv.capital];
    aiPromote(c);
    int mil = 0, threat = 0;
    for (auto& u : units) {
        if (!u.alive) continue;
        if (u.owner == c && g_units[u.type].landMilitary()) mil++;
        if (atWar(c, u.owner) && g_units[u.type].military() && dist(u.x, u.y, home.x, home.y) <= 4) threat++;
    }
    int want = std::min(6, 2 + era(c) / 3 + (threat ? 2 : 0));
    bool building = home.buildKind != BK_NONE && canBuild(home, home.buildKind, home.buildId);
    if (!building || (mil < want && home.buildKind == BK_BUILDING && threat)) {
        int kind = BK_NONE, id = -1;
        if (mil < want) {
            for (int u : availableUnits(c)) {
                const UnitDef& d = g_units[u];
                if (!d.landMilitary() || d.cls == UnitClass::Recon || d.cls == UnitClass::Siege || !canBuild(home, BK_UNIT, u)) continue;
                if (id < 0 || d.str > g_units[id].str) id = u;
            }
            if (id >= 0) kind = BK_UNIT;
        }
        if (kind == BK_NONE) {
            int bc = 1 << 30;
            for (int b : availableBuildings(home)) {
                const BuildingDef& bd = g_buildings[b];
                if (bd.wonder || !canBuild(home, BK_BUILDING, b)) continue;
                if (bd.cost < bc) { bc = bd.cost; id = b; kind = BK_BUILDING; }
            }
        }
        if (kind != BK_NONE && (home.buildKind != kind || home.buildId != id)) {
            home.buildKind = kind;
            home.buildId = id;
        }
    }
    // units: fight what comes near, otherwise stay home
    size_t n = units.size();
    for (size_t i = 0; i < n; i++) {
        if (!units[i].alive || units[i].owner != c) continue;
        int uid = (int)i;
        auto U = [&]() -> Unit& { return units[uid]; };
        const UnitDef& d = g_units[U().type];
        if (!d.military()) continue;
        if (U().order == ORD_FORTIFY || U().order == ORD_SLEEP) U().order = ORD_NONE;
        // strike the best target in reach
        int bx = -1, by = -1;
        double bv = 0;
        for (auto& e : units) {
            if (!e.alive || !atWar(c, e.owner) || dist(e.x, e.y, home.x, home.y) > 4 || !canAttack(U(), e.x, e.y)) continue;
            double A = unitStrength(U(), true, d.ranged(), U().x, U().y), D = unitStrength(e, false, false, e.x, e.y);
            double v = A / std::max(1.0, D) + (g_units[e.type].civilian() ? 2 : 0);
            if (v > bv && (v >= 0.8 || d.ranged())) { bv = v; bx = e.x; by = e.y; }
        }
        if (bx >= 0) { attack(U(), bx, by); continue; }
        if (!U().alive || U().moves <= 0) continue;
        // go home: the city if it has no garrison, else a free tile next to it
        int tx = -1, ty = -1;
        bool garrisoned = false;
        for (auto& o : units)
            if (o.alive && o.owner == c && o.id != uid && o.x == home.x && o.y == home.y && g_units[o.type].landMilitary()) garrisoned = true;
        if (U().x == home.x && U().y == home.y) { U().order = ORD_FORTIFY; continue; }
        if (!garrisoned) { tx = home.x; ty = home.y; }
        else if (dist(U().x, U().y, home.x, home.y) <= 1 && at(U().x, U().y).owner == c) { U().order = ORD_FORTIFY; continue; }
        else
            for (auto& p : ring(home.x, home.y, 1)) {
                if (isWater(p.x, p.y) || at(p.x, p.y).mountain || !unitsAt(p.x, p.y).empty()) continue;
                tx = p.x; ty = p.y;
                break;
            }
        if (tx < 0) { U().order = ORD_FORTIFY; continue; }
        U().order = ORD_GOTO; U().gx = tx; U().gy = ty;
        continueGoto(U());
        if (U().alive && U().order == ORD_GOTO) U().order = ORD_NONE;
    }
}

// ------------------------------------------------------------ the great powers' dealings with tribes
void Game::aiTribes(int c) {
    Civ& cv = civs[c];
    int sk = skillOf(c);
    const PrimateDef* pd = primate(c);
    int aggr = pd ? pd->aggression : 5;
    // gifts of gold to tip a tribe into friendship or alliance
    if (hasGold(c) && cv.gold > 120 + 40 * era(c) && rng.chance(sk == 0 ? 0.08 : 0.25)) {
        int best = -1, bestGold = 0;
        double bv = 0;
        for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++) {
            if (!isMinor(t) || !civs[t].alive || !cv.met[t] || cv.war[t]) continue;
            int inf = influenceFor(c, t);
            int target = inf < TRIBE_FRIEND ? TRIBE_FRIEND + 4 : TRIBE_ALLY + 6;
            int a = civs[t].ally;
            if (a >= 0 && a != c) target = std::max(target, influenceFor(a, t) + 8);
            if (a == c && inf >= TRIBE_ALLY + 15) continue;  // comfortably allied
            int need = target - inf;
            if (need <= 0) continue;
            int gold = need * (60 + 20 * era(c)) / 30 + 1;
            if (gold > cv.gold - 60) continue;
            double v = (target >= TRIBE_ALLY ? 2.0 : 1.5) / gold;
            if (g_tribes[civs[t].tribe].kind == TK_MILITANT && aggr >= 6) v *= 1.3;
            if (g_tribes[civs[t].tribe].kind == TK_LEARNED && sk >= 2) v *= 1.2;
            if (v > bv) { bv = v; best = t; bestGold = gold; }
        }
        if (best >= 0) giftGold(c, best, bestGold);
    }
    // the aggressive sometimes swallow a weak neighbour
    if (sk >= 1 && aggr >= 7 && turn > dur(80)) {
        bool busy = false;
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (cv.war[o] && civs[o].alive) busy = true;
        if (busy) return;
        for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++) {
            if (!isMinor(t) || !civs[t].alive || !cv.met[t] || civs[t].capital < 0 || tribeStatus(c, t) >= TS_FRIEND) continue;
            if (!canDeclareWar(c, t)) continue;
            const City& home = cities[civs[t].capital];
            int d = 99;
            for (auto& ci : cities) if (ci.alive && ci.owner == c) d = std::min(d, dist(ci.x, ci.y, home.x, home.y));
            if (d > 8) continue;
            int a = civs[t].ally;
            int theirs = militaryPower(t) + (a >= 0 ? militaryPower(a) : 0) + 1;
            if (militaryPower(c) < theirs * 3) continue;
            if (rng.chance(0.004 * (aggr - 6))) {
                declareWar(c, t);
                return;
            }
        }
    }
}
