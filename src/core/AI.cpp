// Rebirth of Civilisation - computer opponents
// SPDX-License-Identifier: MIT
#include "Game.h"

namespace {
bool isTierTech(int t) {
    for (int k = 0; k < RUIN_TIERS; k++) if (g_ruinTierTech[k] == t) return true;
    return false;
}
}  // namespace

int Game::aiSiteScore(int c, int x, int y) const {
    if (!canFoundAt(c, x, y)) return -1;
    if (!warHappened)
        for (auto& s : sites) if (dist(s.x, s.y, x, y) <= 4) return -1;
    int s = 0;
    // keep out of other species' way: don't settle on top of a rival band or crowd a rival city
    for (auto& u : units) {
        if (!u.alive || u.owner == c || u.owner < FIRST_PRIMATE_CIV || g_units[u.type].cls != UnitClass::Band) continue;
        int d = dist(u.x, u.y, x, y);
        if (d <= 2) return -1;
        if (d <= 5) s -= 8 * (6 - d);
    }
    for (auto& ci : cities)
        if (ci.alive && ci.owner != c) {
            int d = dist(ci.x, ci.y, x, y);
            if (d <= 5) s -= 6 * (6 - d);
        }
    bool coast = false;
    for (auto& p : ring(x, y, 2)) {
        const Tile& t = at(p.x, p.y);
        if (t.owner >= 0 && t.owner != c) continue;
        Yields y0 = tileYields(c, p.x, p.y);
        int w = (dist(x, y, p.x, p.y) <= 1) ? 3 : 2;
        s += (y0.food * 3 + y0.prod * 2 + y0.gold + y0.sci * 2) * w / 2;
        if (t.res) s += 2;
        if (t.fallout) s -= 2;
        if (t.ruin && warHappened) s += 3;
        if (t.terrain == T_COAST && dist(x, y, p.x, p.y) == 1) coast = true;
    }
    if (coast) s += 4;
    if (at(x, y).hills) s += 3;
    return s;
}

// Hard and Expert: judge each technology by what it does for this civilisation right now.
double Game::aiTechValue(int c, int t) const {
    const Civ& cv = civs[c];
    int nc = numCities(c);
    bool war = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (cv.war[o] && civs[o].alive) war = true;
    double v = 6;
    for (auto& b : g_buildings) {
        if (b.techId != t || b.special == BSpecial::Victory) continue;
        double bv = b.flat.food * 3 + b.flat.prod * 3 + b.flat.sci * 4 + b.flat.cult * 2 + b.flat.gold * 1.5 +
                    (b.pct.food + b.pct.prod + b.pct.sci * 1.4 + b.pct.cult * 0.7 + b.pct.gold * 0.5) / 4.0 + b.defense * (war ? 0.6 : 0.15);
        v += b.wonder ? bv * 0.6 : bv * std::max(1, nc) * 0.6;
    }
    int bestStr = 0;
    for (int u : availableUnits(c)) bestStr = std::max(bestStr, std::max(g_units[u].str, g_units[u].rstr));
    for (auto& u : g_units)
        if (u.techId == t && u.military() && (u.primate == -1 || u.primate == cv.primate) && std::max(u.str, u.rstr) > bestStr)
            v += (std::max(u.str, u.rstr) - bestStr) * (war ? 2.0 : 0.6);
    for (auto& im : g_improvements) if (im.techId == t && !im.name.empty()) v += 4 + nc;
    for (auto& r : g_resources) if (r.revealTechId == t && !r.name.empty()) v += r.kind == RK_STRATEGIC ? 4 : 2;
    for (auto& p : g_policies) if (p.techId == t) v += 6;
    for (int k = 0; k < RUIN_TIERS; k++) {
        if (g_ruinTierTech[k] != t || !warHappened) continue;
        int ruins = 0;
        for (auto& s : sites) {
            const Tile& tl = at(s.x, s.y);
            if ((g_ruins[tl.ruin].tierMask & (1 << k)) && !(tl.ruinDone & (1 << k)) && (tl.owner == c || tl.owner < 0)) {
                for (auto& ci : cities) if (ci.alive && ci.owner == c && dist(ci.x, ci.y, s.x, s.y) <= 8) { ruins++; break; }
            }
        }
        v += ruins * 6;
    }
    if (t == ID.tCultivation && nc == 0) v += 200;                 // the end of wandering
    if ((t == ID.tForaging || t == ID.tCooking) && nc == 0) v += 25;  // feeds the bands
    if (t == ID.tCurrency) v += 20;
    if (t == ID.tLaw) v += 15;
    // in the later eras, everything on the road to Rebirth matters most
    if (era(c) >= 6 && aiOnRebirthPath(t)) v *= skillOf(c) >= 3 ? 1.4 : 1.2;
    if (t == ID.tRebirth) v *= 4;
    return v;
}

bool Game::aiOnRebirthPath(int t) const {
    static std::vector<int> path;
    if (path.empty()) {
        path.assign(g_techs.size(), 0);
        std::vector<int> stack = {ID.tRebirth};
        while (!stack.empty()) {
            int x = stack.back();
            stack.pop_back();
            if (x < 0 || path[x]) continue;
            path[x] = 1;
            for (int p : g_techs[x].prereqs) stack.push_back(p);
        }
    }
    return t >= 0 && t < (int)path.size() && path[t];
}

void Game::aiResearch(int c) {
    Civ& cv = civs[c];
    if (cv.research >= 0) return;
    int sk = skillOf(c);
    if (sk == 0) {  // Easy: no plan at all
        std::vector<int> ok;
        for (size_t t = 0; t < g_techs.size(); t++) if (canResearch(c, (int)t)) ok.push_back((int)t);
        if (!ok.empty()) setResearch(c, ok[rng.range((int)ok.size())]);
        return;
    }
    if (sk >= 2) {
        int best = -1;
        double bestV = -1;
        for (size_t t = 0; t < g_techs.size(); t++) {
            if (!canResearch(c, (int)t)) continue;
            double v = aiTechValue(c, (int)t) * 1000.0 / techCost((int)t);
            // also count what this opens up next
            for (size_t n = 0; n < g_techs.size(); n++)
                for (int p : g_techs[n].prereqs)
                    if (p == (int)t && !civs[c].techs[n]) v += aiTechValue(c, (int)n) * 150.0 / techCost((int)n);
            v *= sk >= 3 ? 1.0 + 0.04 * rng.unit() : 0.9 + 0.2 * rng.unit();
            if (v > bestV) { bestV = v; best = (int)t; }
        }
        if (best >= 0) setResearch(c, best);
        return;
    }
    bool war = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (cv.war[o]) war = true;
    int best = -1;
    double bestS = 1e18;
    for (size_t t = 0; t < g_techs.size(); t++) {
        if (!canResearch(c, (int)t)) continue;
        double s = techCost(t) * (0.75 + 0.5 * rng.unit());
        if (isTierTech((int)t)) s *= 0.6;
        if ((int)t == ID.tCurrency) s *= 0.7;
        if ((int)t == ID.tRebirth) s *= 0.3;
        if ((int)t == ID.tCultivation && numCities(c) == 0) s *= 0.2;  // the road to settling down
        if ((int)t == ID.tForaging && numCities(c) == 0) s *= 0.6;
        bool mil = false;
        for (auto& u : g_units) if (u.techId == (int)t && u.military() && u.primate != -2) mil = true;
        if (mil && war) s *= 0.7;
        if (s < bestS) { bestS = s; best = (int)t; }
    }
    if (best >= 0) setResearch(c, best);
}

void Game::aiDiplomacy(int c) {
    Civ& cv = civs[c];
    const PrimateDef* pd = primate(c);
    int aggr = pd ? pd->aggression : 5;
    int myPow = militaryPower(c) + 1;
    int wars = 0;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (cv.war[o] && civs[o].alive) wars++;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
        if (o == c || !civs[o].alive || !cv.met[o]) continue;
        int theirPow = militaryPower(o) + 1;
        int sk = skillOf(c);
        if (cv.war[o]) {
            int len = turn - cv.warSince[o];
            if (o == player) continue;
            if (sk == 0 && len > dur(8) && (myPow < theirPow * 1.2 || rng.chance(0.15))) proposePeace(c, o);
            else if (sk == 1 && len > dur(12) && (myPow < theirPow || rng.chance(0.08))) proposePeace(c, o);
            else if (sk >= 2) {
                bool planned = c < (int)aiPlans.size() && aiPlans[c].target >= 0 && cities[aiPlans[c].target].owner == o;
                if ((len > dur(6) && myPow * 10 < theirPow * 9) || (len > dur(15) && !planned) || (len > dur(30) && rng.chance(0.1)))
                    proposePeace(c, o);
            }
            continue;
        }
        if (turn < dur(25) || wars > 0) continue;
        if (turn - cv.warSince[o] < dur(20)) continue;  // recent peace
        if (!canDeclareWar(c, o)) continue;              // peace treaty
        // proximity
        int closest = 99;
        for (auto& a : cities) {
            if (!a.alive || a.owner != c) continue;
            for (auto& b : cities) if (b.alive && b.owner == o) closest = std::min(closest, dist(a.x, a.y, b.x, b.y));
        }
        if (closest > 12) continue;
        double chance = aggr * 0.004;
        if (sk == 0) {
            chance = myPow > theirPow * 1.3 ? aggr * 0.002 : 0;
        } else if (sk >= 2) {
            // only start wars that can be won: clearly stronger, an army to hand, and the enemy nearby
            int mil = 0;
            for (auto& u : units) if (u.alive && u.owner == c && g_units[u.type].landMilitary() && g_units[u.type].cls != UnitClass::Recon) mil++;
            bool busy = false;  // the target is already fighting someone else
            for (int q = FIRST_PRIMATE_CIV; q < (int)civs.size(); q++) if (q != c && civs[o].war[q] && civs[q].alive) busy = true;
            double need = sk >= 3 ? 1.4 : 1.6;
            if (busy) need -= 0.2;
            if (closest > 10 || mil < 3 + numCities(c) || myPow < theirPow * need) chance = 0;
            else chance = aggr * (sk >= 3 ? 0.010 : 0.007) * (busy ? 2 : 1);
        } else if (myPow > theirPow * 1.5) chance *= 2.5;
        else if (myPow < theirPow) chance *= 0.2;
        // opinion: friends are (almost) never attacked, the hated much more readily
        int op = opinion(c, o);
        bool sworn = hasAgreement(AG_FRIENDSHIP, c, o) || hasAgreement(AG_DEFENSIVE, c, o);
        if (sworn) chance *= (pd && pd->loyalty <= 2) ? 0.05 : 0.0;
        else chance *= op <= -30 ? 2.0 : op < -8 ? 1.3 : op < 12 ? 1.0 : op < 30 ? 0.3 : 0.08;
        // never pick a fight that would bring a defensive pact down on us from a stronger partner
        if (sk >= 1)
            for (auto& ag : agreements)
                if (ag.kind == AG_DEFENSIVE && ag.until > turn && (ag.a == o || ag.b == o)) {
                    int x = ag.a == o ? ag.b : ag.a;
                    if (x != c && militaryPower(x) + theirPow > myPow * (sk >= 2 ? 1 : 2)) chance *= 0.2;
                }
        if (rng.chance(chance)) {
            declareWar(c, o);
            return;
        }
    }
}

int Game::aiChoosePolicy(int c) {
    std::vector<int> ok;
    for (size_t p = 0; p < g_policies.size(); p++) if (canAdopt(c, (int)p)) ok.push_back((int)p);
    if (ok.empty()) return -1;
    if (skillOf(c) < 2) return ok[rng.range((int)ok.size())];
    const Civ& cv = civs[c];
    int nc = std::max(1, numCities(c));
    bool war = false, threatened = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
        if (!civs[o].alive || o == c) continue;
        if (cv.war[o]) war = true;
        if (cv.met[o] && militaryPower(o) > militaryPower(c) * 3 / 2) threatened = true;
    }
    const PrimateDef* pd = primate(c);
    int aggr = pd ? pd->aggression : 5;
    int ruins = 0;
    for (auto& s : sites) if (at(s.x, s.y).owner == c) ruins++;
    int best = -1;
    double bestV = -1;
    for (int p : ok) {
        const PolicyDef& pol = g_policies[p];
        double w = 0;
        switch (pol.eff) {
            case PolicyEffect::Sci: w = 11; break;
            case PolicyEffect::Prod: w = 10; break;
            case PolicyEffect::Food: w = turn < dur(250) ? 9 : 5; break;
            case PolicyEffect::Culture: w = 6; break;
            case PolicyEffect::Gold: w = hasGold(c) ? 5 : 0; break;
            case PolicyEffect::Combat: w = war ? 10 : aggr >= 7 ? 6 : 2; break;
            case PolicyEffect::RuinBonus: w = warHappened ? 2 + ruins * 1.5 : 1; break;
            case PolicyEffect::SettlerCost: w = turn < dur(200) && nc < 6 ? 8 : 0.5; break;
            case PolicyEffect::UnitCost: w = war || aggr >= 7 ? 6 : 2; break;
            case PolicyEffect::CityDefense: w = threatened || war ? 8 : 2; break;
            case PolicyEffect::Borders: w = 3; break;
            case PolicyEffect::Heal: w = war ? 6 : 1; break;
            case PolicyEffect::FlatSci: w = 1.6 * nc; break;
            case PolicyEffect::FlatCult: w = 1.0 * nc; break;
        }
        w *= pol.value / 15.0 + 0.3;
        if (w > bestV) { bestV = w; best = p; }
    }
    return best;
}

void Game::aiTurn(int c) {
    Civ& cv = civs[c];
    aiResearch(c);
    for (int guard = 0; guard < 3; guard++) {
        int p = aiChoosePolicy(c);
        if (p < 0) break;
        adoptPolicy(c, p);
    }
    if (skillOf(c) >= 2) {
        aiPlanWar(c);
        aiNukes(c);
    }
    aiUpgrades(c);
    aiDiplomacy(c);
    aiPacts(c);
    aiOffers(c);
    for (size_t i = 0; i < cities.size(); i++)
        if (cities[i].alive && cities[i].owner == c) aiCity(cities[i]);
    // units: iterate by index since vectors may grow
    size_t n = units.size();
    for (size_t i = 0; i < n; i++) {
        if (!units[i].alive || units[i].owner != c) continue;
        Unit copy = units[i];
        (void)copy;
        aiUnit(units[i]);
    }
    (void)cv;
}

void Game::aiCity(City& city) {
    int c = city.owner;
    Civ& cv = civs[c];
    // threat assessment
    int threat = 0;
    for (auto& u : units)
        if (u.alive && atWar(c, u.owner) && g_units[u.type].military() && dist(u.x, u.y, city.x, city.y) <= 4) threat++;
    bool garrison = false;
    for (auto& u : units)
        if (u.alive && u.owner == c && u.x == city.x && u.y == city.y && g_units[u.type].landMilitary()) garrison = true;

    auto bestMilitary = [&](bool wantRanged) {
        int best = -1, bs = -1;
        for (int u : availableUnits(c)) {
            const UnitDef& d = g_units[u];
            if (!d.landMilitary() || d.cls == UnitClass::Recon) continue;
            if (wantRanged != d.ranged()) continue;
            int s = std::max(d.str, d.rstr) * 100 / std::max(1, d.cost) + std::max(d.str, d.rstr) * 3;
            if (s > bs) { bs = s; best = u; }
        }
        if (best < 0 && wantRanged) {
            for (int u : availableUnits(c))
                if (g_units[u].landMilitary() && g_units[u].cls != UnitClass::Recon && (best < 0 || g_units[u].str > g_units[best].str)) best = u;
        }
        return best;
    };

    bool keep = city.buildKind != BK_NONE && canBuild(city, city.buildKind, city.buildId);
    if (keep && threat > 0 && !garrison && !(city.buildKind == BK_UNIT && g_units[city.buildId].landMilitary())) keep = false;
    int sk = skillOf(c);
    if (keep) {
        if (sk > 0 && threat > 0 && hasGold(c) && buyCost(city) > 0 && cv.gold >= buyCost(city)) buy(city);
        else if (sk >= 2 && hasGold(c) && buyCost(city) > 0 && cv.gold >= buyCost(city) * 13 / 10 &&
                 !(city.buildKind == BK_BUILDING && g_buildings[city.buildId].wonder))
            buy(city);
        return;
    }

    int myCities = numCities(c), mil = 0, workers = 0, settlers = 0, ships = 0;
    for (auto& u : units) {
        if (!u.alive || u.owner != c) continue;
        const UnitDef& d = g_units[u.type];
        if (d.cls == UnitClass::Worker) workers++;
        else if (d.cls == UnitClass::Settler) settlers++;
        else if (d.naval()) ships++;
        else if (d.landMilitary() && d.cls != UnitClass::Recon) mil++;
    }
    for (auto& oc : cities)
        if (oc.alive && oc.owner == c && oc.id != city.id && oc.buildKind == BK_UNIT) {
            const UnitDef& d = g_units[oc.buildId];
            if (d.cls == UnitClass::Settler) settlers++;
            else if (d.cls == UnitClass::Worker) workers++;
            else if (d.naval()) ships++;
            else if (d.landMilitary()) mil++;
        }
    bool atWarAny = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (cv.war[o] && civs[o].alive) atWarAny = true;
    int targetCities = 4 + settings.mapSize * 2 + (sk - 1);
    const PrimateDef* pd = primate(c);
    if (pd && pd->aggression >= 7) targetCities++;
    bool neighbourThreat = false;
    if (sk >= 2)
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
            if (o != c && civs[o].alive && cv.met[o] && militaryPower(o) > militaryPower(c) * 3 / 2) neighbourThreat = true;
    int milWant = 1 + myCities + (sk >= 2 && neighbourThreat ? (myCities + 1) / 2 : 0);
    int workWant = sk >= 2 ? std::max(1, myCities) : std::max(1, (myCities + 1) * 2 / 3);
    int settlerSlots = sk >= 3 && myCities >= 2 ? 2 : 1;
    bool wantWalls = sk >= 2 && (threat > 0 || neighbourThreat);

    int kind = BK_NONE, id = -1;
    if (canBuild(city, BK_BUILDING, ID.bRebirth)) { kind = BK_BUILDING; id = ID.bRebirth; }
    else if (threat > 0 && !garrison) { kind = BK_UNIT; id = bestMilitary(false); }
    else if (mil < milWant) { kind = BK_UNIT; id = bestMilitary(mil % 3 == 2); }
    else if (workers < workWant && city.pop >= 2) { kind = BK_UNIT; id = uniqueFor(c, ID.uGatherer); }
    else if (myCities + settlers < targetCities && settlers < settlerSlots && city.pop >= 2 && turn < dur(260) && canBuild(city, BK_UNIT, ID.uElder)) { kind = BK_UNIT; id = ID.uElder; }
    else if (isCoastal(city.x, city.y) && aiNeedsTransport(city) >= 0) {
        kind = BK_UNIT;
        id = aiNeedsTransport(city);
    }
    else if (isCoastal(city.x, city.y) && ships < aiShipWant(c, atWarAny) && aiBestShip(c, ships % 2 == 1) >= 0 && rng.chance(sk >= 2 ? 0.8 : 0.5)) {
        kind = BK_UNIT;
        id = aiBestShip(c, ships % 2 == 1);
    }
    else if (sk >= 2 && atWarAny && pd && pd->aggression >= 6 && cv.warheads == 0 && canBuild(city, BK_UNIT, ID.uNuke) && rng.chance(0.15)) {
        kind = BK_UNIT;
        id = ID.uNuke;
    }
    else if (atWarAny && mil < myCities * 3 && rng.chance(0.6)) {
        kind = BK_UNIT;
        id = bestMilitary(rng.chance(0.35));
        if (sk >= 2) {  // a real army brings siege engines
            int siege = 0;
            for (auto& u : units) if (u.alive && u.owner == c && g_units[u.type].cls == UnitClass::Siege) siege++;
            if (siege < 1 + myCities / 3)
                for (int u : availableUnits(c))
                    if (g_units[u].cls == UnitClass::Siege && (id < 0 || g_units[id].cls != UnitClass::Siege || g_units[u].rstr > g_units[id].rstr)) id = u;
        }
    }
    if (id < 0) {
        int best = -1;
        double bs = -1;
        for (int b : availableBuildings(city)) {
            const BuildingDef& bd = g_buildings[b];
            if (bd.special == BSpecial::Victory) continue;
            double v = bd.flat.food * 3 + bd.flat.prod * 3 + bd.flat.sci * 3 + bd.flat.cult * 2.5 + bd.flat.gold * 1.5;
            v += (bd.pct.food * city.yields.food + bd.pct.prod * city.yields.prod + bd.pct.sci * city.yields.sci +
                  bd.pct.cult * city.yields.cult + bd.pct.gold * city.yields.gold) / 30.0;
            v += bd.defense * (atWarAny ? 1.5 : 0.4) * (wantWalls ? 2.5 : 1.0);
            if (bd.special == BSpecial::RuinScience || bd.special == BSpecial::RuinProd) v += ruinTilesInTerritory(city) * 2.5;
            if (bd.special == BSpecial::FreeTech) v += 10;
            if (bd.wonder) v = v * 0.8 + (city.yields.prod >= 8 ? 4 : -100);
            double noise = sk == 0 ? 1.0 : sk == 1 ? 0.4 : sk == 2 ? 0.16 : 0.06;
            v = v * 100.0 / (bd.cost + 30) * (1.0 - noise / 2 + noise * rng.unit());
            if (v > bs) { bs = v; best = b; }
        }
        if (best >= 0) { kind = BK_BUILDING; id = best; }
        else { kind = BK_UNIT; id = bestMilitary(rng.chance(0.3)); }
    }
    if (id >= 0) setProduction(city, kind, id);
    if (sk > 0 && hasGold(c) && city.buildKind == BK_UNIT && buyCost(city) > 0 && cv.gold > buyCost(city) * 2) buy(city);
}

void Game::aiUnit(Unit& uref) {
    int uid = uref.id;
    auto U = [&]() -> Unit& { return units[uid]; };
    if (!U().alive) return;
    int c = U().owner;
    const UnitDef& d = g_units[U().type];
    if (U().order == ORD_BUILD || U().order == ORD_EXCAVATE || U().order == ORD_DECON) return;

    if (d.cls == UnitClass::Band) {
        aiBand(U());
        return;
    }
    if (d.naval()) {
        aiNaval(U());
        return;
    }
    // crossing the water: keep going while the goal still makes sense
    if (U().ferryX >= 0 || U().carrier >= 0) {
        int fx = U().ferryX, fy = U().ferryY;
        bool valid = fx >= 0 && (d.cls == UnitClass::Settler ? aiSiteScore(c, fx, fy) > 0 : isEnemyTarget(c, fx, fy));
        if (!valid) {
            U().ferryX = U().ferryY = -1;
            if (U().carrier >= 0) {
                if (!isWater(U().x, U().y)) { leaveShip(U()); }
                else {  // go home: land next to our nearest city
                    const City* home = nullptr;
                    for (auto& ci : cities)
                        if (ci.alive && ci.owner == c && (!home || dist(ci.x, ci.y, U().x, U().y) < dist(home->x, home->y, U().x, U().y))) home = &ci;
                    if (home) { aiFerry(U(), home->x, home->y); return; }
                }
            }
        } else {
            if (U().carrier < 0) {
                // already on the same landmass as the goal? then just walk
                Unit probe = U();
                if (!findPath(probe, fx, fy, true).empty() && dist(U().x, U().y, fx, fy) <= 4) U().ferryX = U().ferryY = -1;
            }
            if (U().ferryX >= 0 && aiFerry(U(), fx, fy)) {
                if (U().alive && U().carrier < 0 && U().ferryX < 0 && d.cls == UnitClass::Settler && canFoundAt(c, U().x, U().y) &&
                    dist(U().x, U().y, fx, fy) <= 1 && U().moves > 0) {
                    foundCity(c, U().x, U().y);
                    killUnit(U());
                }
                return;
            }
            U().ferryX = U().ferryY = -1;
        }
        if (!U().alive || U().moves <= 0) return;
    }
    // ---------------- settlers
    if (d.cls == UnitClass::Settler) {
        if (numCities(c) == 0 && (turn >= 2 || aiSiteScore(c, U().x, U().y) >= 40) && canFoundAt(c, U().x, U().y)) {
            bool nearHumans = false;
            for (auto& s : sites) if (!warHappened && dist(s.x, s.y, U().x, U().y) <= 4) nearHumans = true;
            if (!nearHumans || turn > 4) { foundCity(c, U().x, U().y); killUnit(U()); return; }
        }
        if (U().order == ORD_GOTO && aiSiteScore(c, U().gx, U().gy) > 0) {
            continueGoto(U());
            if (U().order != ORD_GOTO && U().x == U().gx && U().y == U().gy && canFoundAt(c, U().x, U().y)) {
                foundCity(c, U().x, U().y);
                killUnit(U());
            }
            return;
        }
        int best = -1, bx = -1, by = -1;
        for (auto& p : ring(U().x, U().y, 9)) {
            int s = aiSiteScore(c, p.x, p.y);
            if (s < 0) continue;
            bool danger = false;
            for (auto& e : units)
                if (e.alive && atWar(c, e.owner) && dist(e.x, e.y, p.x, p.y) <= 2) { danger = true; break; }
            if (danger) continue;
            s = s * 3 - dist(U().x, U().y, p.x, p.y) * 4;
            if (s > best) {
                if (findPath(U(), p.x, p.y).empty() && !(p.x == U().x && p.y == U().y)) continue;
                best = s; bx = p.x; by = p.y;
            }
        }
        if (bx < 0 || best < 60) {
            // nothing good on this landmass: look across the water
            bool canSail = false;
            for (int t : availableUnits(c)) if (g_units[t].cargo > 0) canSail = true;
            for (auto& o : units) if (o.alive && o.owner == c && g_units[o.type].cargo > 0) canSail = true;
            if (canSail && skillOf(c) >= 1) {
                int sb = -1, sx = -1, sy = -1;
                for (auto& p : ring(U().x, U().y, 14)) {
                    int s = aiSiteScore(c, p.x, p.y);
                    if (s < 35) continue;
                    s = s * 3 - dist(U().x, U().y, p.x, p.y) * 3;
                    if (s > sb && s > best + 30) { sb = s; sx = p.x; sy = p.y; }
                }
                if (sx >= 0) {
                    Unit probe = U();
                    if (findPath(probe, sx, sy, true).empty() && aiFerry(U(), sx, sy)) return;
                }
            }
        }
        if (bx < 0) {
            if (canFoundAt(c, U().x, U().y)) { foundCity(c, U().x, U().y); killUnit(U()); }
            return;
        }
        if (bx == U().x && by == U().y) { foundCity(c, bx, by); killUnit(U()); return; }
        U().order = ORD_GOTO; U().gx = bx; U().gy = by;
        continueGoto(U());
        if (U().x == bx && U().y == by && U().moves > 0) { foundCity(c, bx, by); killUnit(U()); }
        return;
    }

    // ---------------- workers
    if (d.cls == UnitClass::Worker) {
        int fo = at(U().x, U().y).fallout;
        if (excavateTier(U()) >= 0 && (fo <= 1 || U().hp > 70)) { startExcavate(U()); return; }
        if (U().order == ORD_GOTO) {
            continueGoto(U());
            if (U().order == ORD_GOTO) return;
        }
        if (excavateTier(U()) >= 0 && (fo <= 1 || U().hp > 70)) { startExcavate(U()); return; }
        if (U().hp < 50 && fo > 0) {
            // flee the fallout
            for (auto& ci : cities)
                if (ci.alive && ci.owner == c) { U().order = ORD_GOTO; U().gx = ci.x; U().gy = ci.y; continueGoto(U()); return; }
        }
        // look for ruins to excavate
        int bestD = 99, tx = -1, ty = -1;
        for (auto& s : sites) {
            int dd = dist(U().x, U().y, s.x, s.y);
            if (dd >= bestD || dd > 14) continue;
            const Tile& t = at(s.x, s.y);
            if (excavateTierFor(c, s.x, s.y) < 0 || t.fallout > 2 || (t.fallout == 2 && U().hp < 90)) continue;
            if (t.owner >= 0 && t.owner != c) continue;
            bool danger = false, taken = false;
            for (auto& e : units) {
                if (!e.alive) continue;
                if (atWar(c, e.owner) && dist(e.x, e.y, s.x, s.y) <= 2) danger = true;
                if (e.id != uid && e.x == s.x && e.y == s.y && g_units[e.type].civilian()) taken = true;
                if (e.id != uid && e.owner == c && e.order == ORD_GOTO && e.gx == s.x && e.gy == s.y && g_units[e.type].civilian()) taken = true;
            }
            if (danger || taken) continue;
            if (findPath(U(), s.x, s.y).empty()) continue;
            bestD = dd; tx = s.x; ty = s.y;
        }
        if (tx >= 0) {
            U().order = ORD_GOTO; U().gx = tx; U().gy = ty;
            continueGoto(U());
            if (U().x == tx && U().y == ty && excavateTier(U()) >= 0) startExcavate(U());
            return;
        }
        if (canDecon(U()) && at(U().x, U().y).owner == c) { startDecon(U()); return; }
        auto pickImp = [&](const std::vector<int>& opts) {
            const Tile& here = at(U().x, U().y);
            int r = resourceVisible(c, U().x, U().y) ? (int)here.res : (int)R_NONE;
            int pref = r == R_HORSES ? IMP_PASTURE : isCrop(r) ? IMP_FARM : isMineral(r) ? IMP_MINE : -1;
            for (int o : opts) if (o == pref) return o;
            for (int want : {IMP_MINE, IMP_FARM, IMP_LUMBER, IMP_PASTURE, IMP_TRADEPOST})
                for (int o : opts) if (o == want) return o;
            return opts.empty() ? -1 : opts[0];
        };
        // improve here?
        {
            std::vector<int> opts = buildOptions(U());
            const Tile& t = at(U().x, U().y);
            if (!opts.empty() && t.imp == IMP_NONE && t.city >= 0) { startBuild(U(), pickImp(opts)); return; }
        }
        bestD = 99; tx = -1;
        for (auto& ci : cities) {
            if (!ci.alive || ci.owner != c) continue;
            std::vector<int> cand = ci.worked;
            for (auto& p : ring(ci.x, ci.y, 3))  // unimproved resources in territory are worth it too
                if (at(p.x, p.y).city == ci.id && resourceVisible(c, p.x, p.y)) cand.push_back(idx(p.x, p.y));
            for (int wi : cand) {
                int x = wi % W, y = wi / W;
                const Tile& t = at(x, y);
                if (t.imp != IMP_NONE) continue;
                int dd = dist(U().x, U().y, x, y);
                if (dd >= bestD) continue;
                Unit probe = U();
                probe.x = x; probe.y = y;
                if (buildOptions(probe).empty()) continue;
                bool taken = false;
                for (auto& e : units)
                    if (e.alive && e.id != uid && e.owner == c && g_units[e.type].civilian() &&
                        ((e.x == x && e.y == y) || (e.order == ORD_GOTO && e.gx == x && e.gy == y))) taken = true;
                if (taken) continue;
                bestD = dd; tx = x; ty = y;
            }
        }
        if (tx >= 0) {
            if (tx == U().x && ty == U().y) { startBuild(U(), pickImp(buildOptions(U()))); return; }
            U().order = ORD_GOTO; U().gx = tx; U().gy = ty;
            continueGoto(U());
            if (U().x == tx && U().y == ty && U().moves > 0) {
                std::vector<int> o = buildOptions(U());
                if (!o.empty()) startBuild(U(), pickImp(o));
            }
            return;
        }
        U().order = ORD_SKIP;
        return;
    }

    // ---------------- scouts explore
    if (d.cls == UnitClass::Recon) {
        if (U().order == ORD_GOTO) {
            continueGoto(U());
            if (U().moves == 0) return;
        }
        const Civ& cv = civs[c];
        int bestD = 99, tx = -1, ty = -1;
        for (auto& p : ring(U().x, U().y, 14)) {
            if (cv.explored[idx(p.x, p.y)] || isWater(p.x, p.y)) continue;
            int dd = dist(U().x, U().y, p.x, p.y);
            if (dd < bestD && dd > 1) {
                bestD = dd; tx = p.x; ty = p.y;
            }
        }
        if (tx >= 0) {
            // find a reachable explored tile near it
            std::vector<Pt> p = findPath(U(), tx, ty);
            if (!p.empty()) { U().order = ORD_GOTO; U().gx = tx; U().gy = ty; continueGoto(U()); return; }
        }
        // nothing left: random walk
        Pt nb[6];
        int n = neighbors(U().x, U().y, nb);
        if (n > 0) {
            Pt q = nb[rng.range(n)];
            int mc = moveCost(U(), U().x, U().y, q.x, q.y);
            if (mc > 0 && mc < 99) stepUnit(U(), q.x, q.y);
        }
        return;
    }

    // ---------------- military
    if (U().order == ORD_FORTIFY || U().order == ORD_SLEEP) U().order = ORD_NONE;
    City* here = cityAt(U().x, U().y);
    bool isGarrison = false;
    if (here && here->owner == c) {
        isGarrison = true;
        for (auto& o : units)
            if (o.alive && o.id < uid && o.owner == c && o.x == U().x && o.y == U().y && g_units[o.type].landMilitary()) isGarrison = false;
    }
    // ranged: shoot first
    if (d.ranged()) {
        Unit* tgt = nullptr;
        for (auto& e : units)
            if (e.alive && atWar(c, e.owner) && canAttack(U(), e.x, e.y) && (!tgt || e.hp < tgt->hp)) tgt = &e;
        if (tgt) { attack(U(), tgt->x, tgt->y); return; }
        for (auto& ci : cities)
            if (ci.alive && canAttack(U(), ci.x, ci.y)) { attack(U(), ci.x, ci.y); return; }
    }
    if (isGarrison) {
        // garrison only strikes adjacent weak enemies
        for (auto& e : units)
            if (e.alive && atWar(c, e.owner) && dist(e.x, e.y, U().x, U().y) == 1 && canAttack(U(), e.x, e.y) && !d.ranged()) {
                double A = unitStrength(U(), true, false, U().x, U().y), D = unitStrength(e, false, false, e.x, e.y);
                if (A > D * 1.3) { attack(U(), e.x, e.y); return; }
            }
        U().order = ORD_FORTIFY;
        return;
    }
    // nomads: warriors guard the bands
    if (numCities(c) == 0) {
        int ward = -1, wd = 99;
        for (auto& b : units) {
            if (!b.alive || b.owner != c || g_units[b.type].cls != UnitClass::Band) continue;
            int dd = dist(b.x, b.y, U().x, U().y);
            if (dd == 0) { ward = b.id; wd = 0; break; }
            bool guarded = false;
            for (auto& o : units)
                if (o.alive && o.id != uid && o.owner == c && o.x == b.x && o.y == b.y && g_units[o.type].landMilitary()) guarded = true;
            if (!guarded && dd < wd) { wd = dd; ward = b.id; }
        }
        if (ward >= 0) {
            int wx = units[ward].x, wy = units[ward].y;
            for (auto& e : units)
                if (e.alive && atWar(c, e.owner) && g_units[e.type].military() && dist(e.x, e.y, wx, wy) <= 2 && canAttack(U(), e.x, e.y)) {
                    double A = unitStrength(U(), true, d.ranged(), U().x, U().y), D = unitStrength(e, false, false, e.x, e.y);
                    if (A > D * 0.8 || e.hp < U().hp) { attack(U(), e.x, e.y); return; }
                }
            if (wd == 0) return;
            U().order = ORD_GOTO; U().gx = wx; U().gy = wy;
            continueGoto(U());
            if (U().alive && U().order == ORD_GOTO) U().order = ORD_NONE;
            return;
        }
    }
    if (U().hp < (skillOf(c) >= 2 ? 50 : 40)) {
        City* home = nullptr;
        for (auto& ci : cities)
            if (ci.alive && ci.owner == c && (!home || dist(ci.x, ci.y, U().x, U().y) < dist(home->x, home->y, U().x, U().y))) home = &ci;
        if (home) {
            if (home->x == U().x && home->y == U().y) { U().order = ORD_FORTIFY; return; }
            U().order = ORD_GOTO; U().gx = home->x; U().gy = home->y; continueGoto(U());
            return;
        }
    }
    if (skillOf(c) >= 2) {
        if (aiArmyUnit(U())) return;
    } else {
    // attack nearby enemies
    {
        int bx = -1, by = -1, bs = -1;
        for (auto& e : units) {
            if (!e.alive || !atWar(c, e.owner)) continue;
            int dd = dist(e.x, e.y, U().x, U().y);
            if (dd > 5) continue;
            int s = 200 - dd * 20 - e.hp + (g_units[e.type].civilian() ? 40 : 0);
            if (s > bs) { bs = s; bx = e.x; by = e.y; }
        }
        for (auto& ci : cities) {
            if (!ci.alive || !atWar(c, ci.owner)) continue;
            int dd = dist(ci.x, ci.y, U().x, U().y);
            if (dd > 6) continue;
            int s = 160 - dd * 15 - ci.hp / 4;
            if (s > bs) { bs = s; bx = ci.x; by = ci.y; }
        }
        if (bx >= 0) {
            if (canAttack(U(), bx, by)) {
                double A = unitStrength(U(), true, d.ranged(), U().x, U().y);
                double D;
                if (const City* tc = cityAt(bx, by)) D = cityStrength(*tc) * (tc->hp > 60 ? 1.0 : 0.5);
                else {
                    Unit* m = militaryAt(bx, by);
                    D = m ? unitStrength(*m, false, false, bx, by) : 1;
                }
                if (A >= D * (skillOf(c) == 0 ? 0.5 : 0.75) || U().hp > 80) { attack(U(), bx, by); return; }
            } else if (!d.ranged() || dist(U().x, U().y, bx, by) > d.range) {
                orderMoveOrAttack(U(), bx, by);
                return;
            }
        }
    }
    }
    // ungarrisoned city needs us?
    for (auto& ci : cities) {
        if (!ci.alive || ci.owner != c) continue;
        bool has = false;
        for (auto& o : units)
            if (o.alive && o.owner == c && g_units[o.type].landMilitary() &&
                ((o.x == ci.x && o.y == ci.y) || (o.order == ORD_GOTO && o.gx == ci.x && o.gy == ci.y && o.id != uid))) has = true;
        if (!has && dist(ci.x, ci.y, U().x, U().y) <= 12) {
            Unit probe = U();
            if (findPath(probe, ci.x, ci.y).empty()) continue;
            U().order = ORD_GOTO; U().gx = ci.x; U().gy = ci.y;
            continueGoto(U());
            if (U().x == ci.x && U().y == ci.y) U().order = ORD_FORTIFY;
            return;
        }
    }
    // offensive: march on the nearest enemy city
    if (skillOf(c) < 2) {
        City* tgt = nullptr;
        for (auto& ci : cities)
            if (ci.alive && atWar(c, ci.owner) && ci.owner >= FIRST_PRIMATE_CIV &&
                (!tgt || dist(ci.x, ci.y, U().x, U().y) < dist(tgt->x, tgt->y, U().x, U().y))) tgt = &ci;
        if (tgt && dist(tgt->x, tgt->y, U().x, U().y) <= 25) {
            if (d.ranged() && dist(tgt->x, tgt->y, U().x, U().y) <= d.range) return;
            orderMoveOrAttack(U(), tgt->x, tgt->y);
            return;
        }
    }
    // patrol: hunt ferals near our lands, otherwise rest
    if (U().order == ORD_GOTO) { continueGoto(U()); return; }
    for (auto& e : units)
        if (e.alive && e.owner == CIV_FERAL && at(e.x, e.y).owner == c && dist(e.x, e.y, U().x, U().y) <= 8) {
            orderMoveOrAttack(U(), e.x, e.y);
            return;
        }
    if (here && here->owner == c) U().order = ORD_FORTIFY;
    else {
        // drift towards own territory
        City* home = nullptr;
        for (auto& ci : cities)
            if (ci.alive && ci.owner == c && (!home || dist(ci.x, ci.y, U().x, U().y) < dist(home->x, home->y, U().x, U().y))) home = &ci;
        if (home && dist(home->x, home->y, U().x, U().y) > 2) { U().order = ORD_GOTO; U().gx = home->x; U().gy = home->y; continueGoto(U()); }
        else U().order = ORD_FORTIFY;
    }
}

// ============================================================ Hard / Expert: planned wars
// Instead of every unit charging the nearest enemy on its own, the army picks one target city,
// gathers at a rally point a few tiles short of it, and only attacks once it is strong enough.
void Game::aiPlanWar(int c) {
    if ((int)aiPlans.size() < (int)civs.size()) aiPlans.resize(civs.size());
    AIPlan old = aiPlans[c];
    AIPlan P;
    if (numCities(c) == 0) { aiPlans[c] = P; return; }
    int best = -1;
    double bs = 1e9;
    for (auto& ci : cities) {
        if (!ci.alive || ci.owner < FIRST_PRIMATE_CIV || !atWar(c, ci.owner)) continue;
        int d = 99;
        for (auto& m : cities) if (m.alive && m.owner == c) d = std::min(d, dist(m.x, m.y, ci.x, ci.y));
        if (d > 14) continue;
        double s = d * 3 + cityStrength(ci) / 10.0 * ci.hp / std::max(1, cityMaxHP(ci)) - (old.target == ci.id ? 15 : 0);
        if (s < bs) { bs = s; best = ci.id; }
    }
    if (best < 0) { aiPlans[c] = P; return; }
    const City& T = cities[best];
    P.target = best;
    const City* home = nullptr;
    for (auto& m : cities)
        if (m.alive && m.owner == c && (!home || dist(m.x, m.y, T.x, T.y) < dist(home->x, home->y, T.x, T.y))) home = &m;
    P.rx = home->x; P.ry = home->y;
    Unit probe;
    probe.type = ID.uWarband;
    probe.owner = c;
    probe.x = home->x; probe.y = home->y;
    std::vector<Pt> path = findPath(probe, T.x, T.y, true);
    if (path.size() >= 4) { P.rx = path[path.size() - 4].x; P.ry = path[path.size() - 4].y; }
    for (auto& u : units) {
        if (!u.alive || u.owner != c || !g_units[u.type].landMilitary() || g_units[u.type].cls == UnitClass::Recon || u.hp < 50) continue;
        if (dist(u.x, u.y, P.rx, P.ry) <= 2 || dist(u.x, u.y, T.x, T.y) <= 3) P.army++;
    }
    P.need = 3 + T.pop / 3 + (skillOf(c) >= 3 ? 0 : 1);
    P.assault = old.target == best && old.assault ? P.army >= std::max(2, P.need - 2) : P.army >= P.need;
    aiPlans[c] = P;
}

bool Game::aiArmyUnit(Unit& uref) {
    int uid = uref.id;
    auto U = [&]() -> Unit& { return units[uid]; };
    int c = U().owner;
    const UnitDef& d = g_units[U().type];
    int sk = skillOf(c);
    if (U().moves <= 0) return true;
    auto odds = [&](const Unit& e) {  // expected damage we deal / take
        double A = unitStrength(U(), true, d.ranged(), U().x, U().y), D = unitStrength(e, false, false, e.x, e.y);
        return std::make_pair(combatDamage(A, D), d.ranged() || g_units[e.type].civilian() ? 0 : combatDamage(D, A));
    };
    // 1. strikes with good odds on anything in reach
    {
        int bx = -1, by = -1;
        double bv = 0;
        for (auto& e : units) {
            if (!e.alive || !atWar(c, e.owner) || !canAttack(U(), e.x, e.y)) continue;
            auto o = odds(e);
            bool kill = o.first >= e.hp;
            double v = kill ? 300 + o.first : o.first - o.second * 1.2;
            if (g_units[e.type].civilian()) v = 250;
            if (!d.ranged() && !kill && o.second >= U().hp - 10) continue;  // would die or nearly
            if (v > bv) { bv = v; bx = e.x; by = e.y; }
        }
        if (bx >= 0 && (bv > 0 || d.ranged())) { attack(U(), bx, by); return true; }
        if (d.ranged())
            for (auto& ci : cities)
                if (ci.alive && canAttack(U(), ci.x, ci.y)) { attack(U(), ci.x, ci.y); return true; }
    }
    // 2. drive out intruders in our own lands
    {
        int bx = -1, by = -1, bd = 99;
        for (auto& e : units) {
            if (!e.alive || !atWar(c, e.owner) || !g_units[e.type].military()) continue;
            if (at(e.x, e.y).owner != c) continue;
            int dd = dist(e.x, e.y, U().x, U().y);
            if (dd > 5 || dd >= bd) continue;
            auto o = odds(e);
            if (o.first < o.second && e.hp > 40) continue;
            bd = dd; bx = e.x; by = e.y;
        }
        if (bx >= 0) {
            std::vector<Pt> p = findPath(U(), bx, by, true);
            if (p.size() >= 2) {
                Pt stand = p[p.size() - 1 - std::min((int)p.size() - 1, d.ranged() ? d.range : 1)];
                U().order = ORD_GOTO; U().gx = stand.x; U().gy = stand.y;
                continueGoto(U());
                if (U().alive && U().order == ORD_GOTO) U().order = ORD_NONE;
                return true;
            }
        }
    }
    // 3. the war plan
    if (c >= (int)aiPlans.size() || aiPlans[c].target < 0) return false;
    const AIPlan& P = aiPlans[c];
    const City& T = cities[P.target];
    if (!T.alive || !atWar(c, T.owner)) return false;
    int dT = dist(U().x, U().y, T.x, T.y);
    if (dT > 1 && isCoastal(T.x, T.y)) {  // across the sea: go by ship
        Unit probe = U();
        if (findPath(probe, T.x, T.y, true).empty()) return aiFerry(U(), T.x, T.y);
    }
    if (!P.assault) {
        if (dist(U().x, U().y, P.rx, P.ry) > 1) {
            U().order = ORD_GOTO; U().gx = P.rx; U().gy = P.ry;
            continueGoto(U());
            if (U().alive && U().order == ORD_GOTO) U().order = ORD_NONE;
        } else {
            U().order = ORD_FORTIFY;  // wait for the rest of the army
        }
        return true;
    }
    // assault
    int want = d.ranged() ? d.range : 1;
    if (dT > want) {
        std::vector<Pt> p = findPath(U(), T.x, T.y, true);
        if (p.size() < 2) return false;
        int k = std::max(0, (int)p.size() - 1 - want);
        U().order = ORD_GOTO; U().gx = p[k].x; U().gy = p[k].y;
        continueGoto(U());
        if (U().alive && U().order == ORD_GOTO) U().order = ORD_NONE;
    }
    if (!U().alive || U().moves <= 0 || !canAttack(U(), T.x, T.y)) return true;
    if (d.ranged()) { attack(U(), T.x, T.y); return true; }
    double A = unitStrength(U(), true, false, U().x, U().y) * (d.cls == UnitClass::Siege ? 3.0 : 1.5);
    int taken = combatDamage(cityStrength(T), A);
    int dealt = combatDamage(A, cityStrength(T));
    bool capture = dealt >= T.hp;
    if (capture || (U().hp - taken >= (sk >= 3 ? 30 : 20) && T.hp < cityMaxHP(T) * 3 / 4)) attack(U(), T.x, T.y);
    else U().order = ORD_FORTIFY;
    return true;
}
