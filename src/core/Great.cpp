// Rebirth of Civilisation - Great Primates
// A people's science, industry, trade, culture and victories in battle build up points towards
// remarkable individuals. When enough have built up, a Great Primate is born in the capital. Each can
// spend their gift at once (a burst of science, production, gold, culture, or a rallying cry), or settle
// in a city for a lasting bonus there. Each new one of a kind takes more points than the last.
//
// And once in a while a blue box tumbles out of the sky...
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

int Game::greatKind(const Unit& u) const {
    for (int k = 0; k <= GK_COUNT; k++) if (u.type == ID.uGreat[k]) return k;
    return -1;
}

int Game::greatCost(int c, int kind) const {
    int n = civs[c].greatBorn[kind] + 1;  // each takes more than the last, and more steeply
    int base = kind == GK_WARLEADER ? 50 : 400;
    return base * n * n * speedPct / 100;
}

void Game::addGreatPoints(int c, int kind, int pts) {
    if (!isMajor(c) || pts <= 0 || kind < 0 || kind >= GK_COUNT) return;
    civs[c].greatPts[kind] += pts;
}

// Points from this turn's yields; a Great Primate is born when there are enough.
void Game::greatTurn(int c, int sci, int cult, int gold, int prod) {
    Civ& cv = civs[c];
    if (!isMajor(c) || numCities(c) == 0) return;
    addGreatPoints(c, GK_THINKER, sci / 10);
    addGreatPoints(c, GK_ARTISAN, prod / 16);
    addGreatPoints(c, GK_MERCHANT, std::max(0, gold) / 10 + routesOf(c));
    addGreatPoints(c, GK_STORYTELLER, cult / 8);
    for (int k = 0; k < GK_COUNT; k++) {
        int cost = greatCost(c, k);
        if (cv.greatPts[k] < cost) continue;
        cv.greatPts[k] -= cost;
        spawnGreat(c, k);
    }
}

int Game::spawnGreat(int c, int kind, int x, int y) {
    Civ& cv = civs[c];
    if (x < 0) {
        const City* home = cv.capital >= 0 && cities[cv.capital].alive && cities[cv.capital].owner == c ? &cities[cv.capital] : nullptr;
        if (!home) for (auto& ci : cities) if (ci.alive && ci.owner == c) { home = &ci; break; }
        if (!home) return -1;
        x = home->x;
        y = home->y;
    }
    int id = spawnUnit(c, ID.uGreat[kind], x, y);
    if (id < 0) return -1;
    Unit& u = units[id];
    if (kind == GK_LAST_HUMAN) u.name = "Cassandra, the Last Human";
    else {
        const auto& names = g_greatNames[kind];
        int n = cv.greatBorn[kind]++;
        u.name = names[n % names.size()] + (n >= (int)names.size() ? " the Younger" : "");
    }
    u.moves = maxMoves(u);
    cv.greatEarned++;
    if (kind != GK_LAST_HUMAN) {
        if (c == player)
            popup(c, POP_GREAT, "A " + std::string(GREAT_KIND_NAMES[kind]) + " is born",
                  u.name + " has been born among the " + cv.name + ".\n\n" + g_units[u.type].desc, x, y);
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
            if (o != c && o == player && civs[o].met[c]) notify(o, "A " + std::string(GREAT_KIND_NAMES[kind]) + " has been born among the " + cv.name + ".");
    }
    return id;
}

// ------------------------------------------------------------ what they can do
bool Game::canGreatAct(const Unit& u, int act, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    int k = greatKind(u);
    if (k < 0) return fail("Not a Great Primate");
    if (u.moves <= 0) return fail("No moves left this turn");
    if (u.carrier >= 0) return fail("Cannot do this aboard a ship");
    const City* here = cityAt(u.x, u.y);
    const Tile& t = at(u.x, u.y);
    if (act == GA_JOIN) {
        if (k == GK_WARLEADER || k == GK_LAST_HUMAN) return fail("Cannot settle in a city");
        if (!here || here->owner != u.owner) return fail("Move into one of your cities to settle there");
        return true;
    }
    switch (k) {
        case GK_THINKER: return hasTrait(u.owner, TR_NEOCORTEX) ? true : fail("Your species cannot yet learn");
        case GK_ARTISAN:
            if (!here || here->owner != u.owner) return fail("Move into one of your cities");
            if (here->buildKind == BK_NONE) return fail("The city is not building anything");
            if (here->buildKind == BK_BUILDING && g_buildings[here->buildId].special == BSpecial::Victory)
                return fail("The Rebirth Project is beyond any one artisan");
            return true;
        case GK_MERCHANT:
            if (!here || here->owner == u.owner) return fail("Move into another people's city (a tribe's, or another species')");
            if (civs[u.owner].war[here->owner]) return fail("You are at war with them");
            return true;
        case GK_STORYTELLER: return t.owner == u.owner ? true : fail("Tell the tale inside your own lands");
        case GK_WARLEADER: {
            int n = 0;
            for (auto& o : units)
                if (o.alive && o.owner == u.owner && g_units[o.type].military() && dist(o.x, o.y, u.x, u.y) <= 2) n++;
            return n ? true : fail("No troops within 2 tiles to rally");
        }
        case GK_LAST_HUMAN: return true;
    }
    return fail("Not possible");
}

std::string Game::greatActName(const Unit& u, int act) const {
    if (act == GA_JOIN) return "Settle in City";
    switch (greatKind(u)) {
        case GK_THINKER: return "Share Insight";
        case GK_ARTISAN: return "Masterwork";
        case GK_MERCHANT: return "Trade Mission";
        case GK_STORYTELLER: return "Great Tale";
        case GK_WARLEADER: return "Rally Troops";
        case GK_LAST_HUMAN: return "Preservation";
    }
    return "";
}

namespace {
int insightSci(const Game& g, int c) { return std::max(60, g.civs[c].sciencePerTurn * 5); }
int talesCult(const Game& g, int c) { return std::max(50, g.civs[c].culturePerTurn * 5); }
int masterworkProd(const Game& g, const City& ci) { return std::max(100, ci.yields.prod * 6); }
int missionGold(const Game& g, int c) { return 150 + 60 * g.era(c); }
}  // namespace

std::string Game::greatActTip(const Unit& u, int act) const {
    int c = u.owner;
    if (act == GA_JOIN) {
        switch (greatKind(u)) {
            case GK_THINKER: return "Settle here for good: +3 Science in this city every turn";
            case GK_ARTISAN: return "Settle here for good: +3 Production in this city every turn";
            case GK_MERCHANT: return "Settle here for good: +4 Gold in this city every turn";
            case GK_STORYTELLER: return "Settle here for good: +3 Culture in this city every turn";
        }
        return "";
    }
    switch (greatKind(u)) {
        case GK_THINKER: return "+" + std::to_string(insightSci(*this, c)) + " Science at once (5 turns' worth). Uses up the Great Thinker.";
        case GK_ARTISAN: {
            const City* ci = cityAt(u.x, u.y);
            return "+" + std::to_string(ci ? masterworkProd(*this, *ci) : 120) + " Production towards what this city is building. Uses up the Great Artisan.";
        }
        case GK_MERCHANT:
            return "+" + std::to_string(missionGold(*this, c)) + " Gold, and goodwill with the city's owner (+60 influence with a tribe). Uses up the Great Merchant.";
        case GK_STORYTELLER:
            return "+" + std::to_string(talesCult(*this, c)) + " Culture and a season of festivals: +2 Happiness in every city for " + std::to_string(dur(10)) +
                   " turns. Uses up the Great Storyteller.";
        case GK_WARLEADER: return "Troops within 2 tiles heal fully and fight at +20% for " + std::to_string(dur(5)) + " turns. Uses up the Great Warleader.";
        case GK_LAST_HUMAN:
            return "Cassandra recalls the old world: half of each human-knowledge technology (Salvaging, Deciphering, Reverse Engineering, Computing) "
                   "you have not discovered, and from now on excavations yield +50%. She retires to write her memoirs.";
    }
    return "";
}

bool Game::greatAct(Unit& u, int act) {
    if (!canGreatAct(u, act)) return false;
    int c = u.owner, k = greatKind(u);
    Civ& cv = civs[c];
    City* here = cityAt(u.x, u.y);
    std::string msg;
    if (act == GA_JOIN) {
        switch (k) {
            case GK_THINKER: here->greatBonus.sci += 3; break;
            case GK_ARTISAN: here->greatBonus.prod += 3; break;
            case GK_MERCHANT: here->greatBonus.gold += 4; break;
            case GK_STORYTELLER: here->greatBonus.cult += 3; break;
        }
        msg = u.name + " has settled in " + here->name + ".";
        computeCity(*here);
    } else {
        switch (k) {
            case GK_THINKER: {
                int s = insightSci(*this, c);
                addScience(c, s);
                msg = u.name + " shared a great insight: +" + std::to_string(s) + " Science.";
                break;
            }
            case GK_ARTISAN: {
                int p = masterworkProd(*this, *here);
                here->prod += p;
                msg = u.name + " threw their skill into the work in " + here->name + ": +" + std::to_string(p) + " Production.";
                break;
            }
            case GK_MERCHANT: {
                int gold = missionGold(*this, c), o = here->owner;
                cv.gold += gold;
                if (isMinor(o)) addInfluence(c, o, 60);
                else if (o >= FIRST_PRIMATE_CIV && o < (int)civs[o].grievance.size() && c < (int)civs[o].grievance.size())
                    civs[o].grievance[c] = std::max(0, civs[o].grievance[c] - 15);
                msg = u.name + " led a trade mission to " + here->name + ": +" + std::to_string(gold) + " Gold" +
                      (isMinor(o) ? " and +60 influence with " + tribeTitle(o) : ", and the " + civs[o].name + " think better of you") + ".";
                break;
            }
            case GK_STORYTELLER: {
                int cu = talesCult(*this, c);
                cv.culture += cu;
                cv.cultureTotal += cu;
                cv.festivalUntil = turn + dur(10);
                for (auto& ci : cities) if (ci.alive && ci.owner == c) computeCity(ci);
                msg = u.name + " told a great tale: +" + std::to_string(cu) + " Culture, and festivals in every city (+2 Happiness for " + std::to_string(dur(10)) + " turns).";
                break;
            }
            case GK_WARLEADER: {
                int n = 0;
                for (auto& o : units)
                    if (o.alive && o.owner == c && o.id != u.id && g_units[o.type].military() && dist(o.x, o.y, u.x, u.y) <= 2) {
                        o.hp = 100;
                        o.rallyUntil = turn + dur(5);
                        n++;
                    }
                msg = u.name + " rallied " + std::to_string(n) + (n == 1 ? " unit" : " units") + ": healed, and fighting at +20% for " + std::to_string(dur(5)) + " turns.";
                break;
            }
            case GK_LAST_HUMAN: {
                std::string got;
                for (int t : {ID.tSalvaging, ID.tDeciphering, ID.tReverse, ID.tComputing}) {
                    if (t < 0 || hasTech(c, t)) continue;
                    cv.techProgress[t] = std::max(cv.techProgress[t], std::min(techCost(t) - 1, cv.techProgress[t] + techCost(t) / 2));
                    got += (got.empty() ? "" : ", ") + g_techs[t].name;
                }
                cv.preservation = true;
                msg = "Cassandra dictated everything she remembers of the old world" + (got.empty() ? std::string("") : " - half of " + got + " is already known") +
                      ". From now on your excavations yield +50%.";
                break;
            }
        }
    }
    if (c == player) notify(c, msg, u.x, u.y);
    u.moves = 0;
    killUnit(u);
    return true;
}

// ------------------------------------------------------------ the blue box
// Once per game, at a random moment after the Fall, a blue box tumbles out of the sky near one of a
// species' units. Out falls Cassandra, the last human; then the box fades away with a wheezing groan.
void Game::cassandraCheck() {
    if (cassandraTurn >= 0 || !warHappened || turn < dur(60) || turn > dur(420)) return;
    if (!rng.chance(1.0 / dur(120))) return;
    std::vector<int> who;
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) if (isMajor(c) && civs[c].alive && numCities(c) > 0) who.push_back(c);
    if (who.empty()) return;
    int c = who[rng.range((int)who.size())];
    // near one of their units, on open land
    std::vector<Pt> spots;
    for (auto& u : units) {
        if (!u.alive || u.owner != c || u.carrier >= 0) continue;
        for (auto& p : ring(u.x, u.y, 2)) {
            const Tile& t = at(p.x, p.y);
            if (isWater(p.x, p.y) || t.mountain || t.fallout || cityAt(p.x, p.y) || !unitsAt(p.x, p.y).empty()) continue;
            if (t.owner >= 0 && t.owner != c) continue;
            spots.push_back(p);
        }
    }
    if (spots.empty()) return;
    Pt p = spots[rng.range((int)spots.size())];
    cassandraTurn = turn;
    cassandraX = p.x;
    cassandraY = p.y;
    int id = spawnGreat(c, GK_LAST_HUMAN, p.x, p.y);
    if (id < 0) return;
    civs[c].metCassandra = true;
    if (c == player)
        popup(c, POP_GREAT, "The Last Human",
              "A blue box came tumbling out of the clear sky, spinning end over end. Its door banged open, a figure fell out onto the grass - "
              "and with a strange wheezing, groaning sound the box faded away.\n\n"
              "The figure picked herself up, smoothed her hair and announced that she is Cassandra, the last human, and that she has seen how all "
              "this ends. She seems to think she is a Great Primate. Technically, she is not wrong.\n\n"
              "Use her Preservation: she will recall the knowledge of the old world.",
              p.x, p.y);
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
        if (o != c && o == player && civs[o].explored[idx(p.x, p.y)])
            notify(o, "Your scouts saw a blue box fall from the sky over the lands of the " + civs[c].name + ". Then it was gone.", p.x, p.y);
}

// ------------------------------------------------------------ the AI's use of them
void Game::aiGreat(int c) {
    size_t n = units.size();
    for (size_t i = 0; i < n; i++) {
        if (!units[i].alive || units[i].owner != c) continue;
        int k = greatKind(units[i]);
        if (k < 0) continue;
        Unit& u = units[i];
        if (u.moves <= 0) continue;
        if (k == GK_LAST_HUMAN || k == GK_THINKER || k == GK_STORYTELLER || k == GK_WARLEADER) {
            bool early = turn < dur(220);
            if (k == GK_THINKER && early && cityAt(u.x, u.y) && cityAt(u.x, u.y)->owner == c) { greatAct(u, GA_JOIN); continue; }
            if (k == GK_STORYTELLER && cityAt(u.x, u.y) && cityAt(u.x, u.y)->owner == c) {
                bool sad = false;
                for (auto& ci : cities) if (ci.alive && ci.owner == c && ci.happy - ci.unhappy <= 0) sad = true;
                greatAct(u, sad ? GA_USE : GA_JOIN);
                continue;
            }
            if (k == GK_WARLEADER) {  // follow the biggest army, and rally it when at war
                bool war = false;
                for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (civs[c].war[o] && civs[o].alive) war = true;
                int near = 0;
                for (auto& o : units) if (o.alive && o.owner == c && g_units[o.type].military() && dist(o.x, o.y, u.x, u.y) <= 2) near++;
                if (war && near >= 3) { greatAct(u, GA_USE); continue; }
                u.order = ORD_SLEEP;
                continue;
            }
            if (canGreatAct(u, GA_USE)) { greatAct(u, GA_USE); continue; }
            continue;
        }
        if (k == GK_ARTISAN) {  // best spent on a wonder; otherwise settle in the capital
            for (auto& ci : cities)
                if (ci.alive && ci.owner == c && ci.buildKind == BK_BUILDING && g_buildings[ci.buildId].wonder && ci.x == u.x && ci.y == u.y &&
                    canGreatAct(u, GA_USE)) {
                    greatAct(u, GA_USE);
                    break;
                }
            if (!u.alive) continue;
            if (cityAt(u.x, u.y) && cityAt(u.x, u.y)->owner == c) {
                bool wonderSomewhere = false;
                for (auto& ci : cities)
                    if (ci.alive && ci.owner == c && ci.buildKind == BK_BUILDING && g_buildings[ci.buildId].wonder && g_buildings[ci.buildId].special != BSpecial::Victory)
                        wonderSomewhere = true;
                if (!wonderSomewhere) { greatAct(u, GA_JOIN); continue; }
                for (auto& ci : cities)
                    if (ci.alive && ci.owner == c && ci.buildKind == BK_BUILDING && g_buildings[ci.buildId].wonder && g_buildings[ci.buildId].special != BSpecial::Victory) {
                        u.order = ORD_GOTO; u.gx = ci.x; u.gy = ci.y;
                        continueGoto(u);
                        break;
                    }
            }
            continue;
        }
        if (k == GK_MERCHANT) {  // a mission to the nearest friendly tribe, else settle at home
            if (canGreatAct(u, GA_USE)) { greatAct(u, GA_USE); continue; }
            const City* best = nullptr;
            for (auto& ci : cities)
                if (ci.alive && isMinor(ci.owner) && civs[c].met[ci.owner] && !civs[c].war[ci.owner] && dist(ci.x, ci.y, u.x, u.y) <= 12 &&
                    (!best || dist(ci.x, ci.y, u.x, u.y) < dist(best->x, best->y, u.x, u.y)))
                    best = &ci;
            if (best && (u.order != ORD_GOTO || u.gx != best->x || u.gy != best->y)) {
                u.order = ORD_GOTO; u.gx = best->x; u.gy = best->y;
            }
            if (u.order == ORD_GOTO) {
                continueGoto(u);
                if (u.alive && canGreatAct(u, GA_USE)) greatAct(u, GA_USE);
                if (u.alive && u.order != ORD_GOTO && cityAt(u.x, u.y) && cityAt(u.x, u.y)->owner == c) greatAct(u, GA_JOIN);
                continue;
            }
            if (cityAt(u.x, u.y) && cityAt(u.x, u.y)->owner == c) greatAct(u, GA_JOIN);
        }
    }
}
