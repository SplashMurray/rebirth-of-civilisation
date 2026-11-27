// Rebirth of Civilisation - turn processing, the Fall, diplomacy, ferals and victory
// SPDX-License-Identifier: MIT
#include "Game.h"

void revealAround(Game& g, int c, int x, int y, int r) {
    Civ& cv = g.civs[c];
    for (auto& p : g.ring(x, y, r)) {
        int i = g.idx(p.x, p.y);
        cv.visible[i] = 1;
        cv.explored[i] = 1;
    }
    g.discoverNaturals(c, x, y, r);
    // meet anyone we can now see
    for (auto& u : g.units)
        if (u.alive && u.owner != c && u.owner >= FIRST_PRIMATE_CIV && !cv.met[u.owner] && g.dist(u.x, u.y, x, y) <= r)
            g.meet(c, u.owner);
    for (auto& ci : g.cities)
        if (ci.alive && ci.owner != c && !cv.met[ci.owner] && g.dist(ci.x, ci.y, x, y) <= r) g.meet(c, ci.owner);
}

void Game::updateVisibility(int c) {
    Civ& cv = civs[c];
    std::fill(cv.visible.begin(), cv.visible.end(), 0);
    for (auto& u : units)
        if (u.alive && u.owner == c) revealAround(*this, c, u.x, u.y, sightRange(u));
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c) revealAround(*this, c, ci.x, ci.y, 3);
}

void Game::updateAllVisibility() {
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++)
        if (civs[c].alive) updateVisibility(c);
}

void Game::meet(int a, int b) {
    if (a == b || a < FIRST_PRIMATE_CIV || b < FIRST_PRIMATE_CIV) return;
    if (civs[a].met[b]) return;
    if (isMinor(a) && isMinor(b)) { civs[a].met[b] = civs[b].met[a] = 1; return; }
    civs[a].met[b] = civs[b].met[a] = 1;
    for (int k = 0; k < 2; k++) {
        int m = k ? b : a, t = k ? a : b;
        if (!isMajor(m) || !isMinor(t)) continue;
        bool first = civs[t].firstMet < 0;
        if (first) { civs[t].firstMet = m; addInfluence(m, t, 15); }
        if (m == player)
            notify(player, "You have met " + tribeTitle(t) + ", a " + TRIBE_KIND_NAMES[g_tribes[civs[t].tribe].kind] + " tribe. " + tribeBonusText(t) +
                               (first ? ". As the first to meet them you start with 15 influence." : ". See the Tribes screen."));
    }
    if (isMinor(a) || isMinor(b)) return;
    if (a == player) notify(a, "You have met the " + civs[b].name + ", led by " + civs[b].leader + ".");
    if (b == player) notify(b, "You have met the " + civs[a].name + ", led by " + civs[a].leader + ".");
}

void Game::declareWar(int a, int b) {
    if (a == b || civs[a].war[b]) return;
    bool friends = hasAgreement(AG_FRIENDSHIP, a, b) || hasAgreement(AG_DEFENSIVE, a, b);
    civs[a].war[b] = civs[b].war[a] = 1;
    statWars++;
    civs[a].warsDeclared++;
    civs[a].warSince[b] = civs[b].warSince[a] = turn;
    meet(a, b);
    if (isMinor(a) || isMinor(b)) {  // a war with a tribe
        if (isMajor(a)) tribeDeclaredOn(a, b);
        cancelRoutesBetween(a, b, "war broke out between the " + civs[a].name + " and the " + civs[b].name);
        if (a == player) notify(a, "You declared war on " + tribeTitle(b) + ".");
        else if (b == player) {
            if (isMinor(a) && civs[a].ally >= 0)
                notify(player, tribeTitle(a) + " have joined their allies, the " + civs[civs[a].ally].name + ", in the war against you.");
            else popup(b, POP_WAR, "War!", "The " + civs[a].name + " have declared war on " + tribeTitle(b) + "!");
        } else if (isMajor(a) && civs[player].met[a] && civs[player].met[b])
            notify(player, "The " + civs[a].name + " declared war on " + tribeTitle(b) + ".");
        return;
    }
    // diplomacy: the victim remembers; betraying a friend shocks everyone
    if (b < (int)civs[a].grievance.size() && a >= FIRST_PRIMATE_CIV && b >= FIRST_PRIMATE_CIV) {
        civs[b].grievance[a] += 30;
        if (friends) {
            civs[a].backstabs++;
            civs[b].grievance[a] += 40;
            for (int x = FIRST_PRIMATE_CIV; x < (int)civs.size(); x++)
                if (x != a && x != b && civs[x].met[a]) civs[x].grievance[a] += 12;
            if (b == player) notify(player, "The " + civs[a].name + " have betrayed your friendship!");
        }
        endAgreements(a, b);
        cancelRoutesBetween(a, b, "war broke out between the " + civs[a].name + " and the " + civs[b].name);
    }
    if (b == player) popup(b, POP_WAR, "War!", civs[a].leader + " of the " + civs[a].name + " has declared war on you!");
    if (a == player) notify(a, "You declared war on the " + civs[b].name + ".");
    else if (b != player && civs[player].met[a]) notify(player, "The " + civs[a].name + " declared war on the " + civs[b].name + ".");
    // defensive pacts: an attack on one is an attack on all of its partners
    if (a >= FIRST_PRIMATE_CIV && b >= FIRST_PRIMATE_CIV) {
        std::vector<int> allies;
        for (auto& ag : agreements)
            if (ag.kind == AG_DEFENSIVE && ag.until > turn && (ag.a == b || ag.b == b)) {
                int x = ag.a == b ? ag.b : ag.a;
                if (x != a && civs[x].alive && !civs[x].war[a]) allies.push_back(x);
            }
        for (int x : allies) {
            if (civs[x].war[a]) continue;
            civs[x].peaceUntil[a] = civs[a].peaceUntil[x] = 0;  // the pact overrides any treaty
            if (x == player) popup(player, POP_WAR, "Your Pact Is Called Upon",
                                   "The " + civs[a].name + " attacked your partners, the " + civs[b].name + ". Honouring your defensive pact, you are now at war with the " + civs[a].name + ".");
            else if (civs[player].met[x]) notify(player, "Honouring their defensive pact, the " + civs[x].name + " join the war against the " + civs[a].name + ".");
            declareWar(x, a);
            civs[x].warsDeclared--;  // defending a partner is not warmongering
            civs[a].grievance[x] -= 30;
            civs[a].grievance[x] = std::max(0, civs[a].grievance[x]);
        }
    }
}

void Game::makePeace(int a, int b) {
    if (!civs[a].war[b]) return;
    civs[a].war[b] = civs[b].war[a] = 0;
    civs[a].warSince[b] = civs[b].warSince[a] = turn;
    if (a >= FIRST_PRIMATE_CIV && b >= FIRST_PRIMATE_CIV) {
        civs[a].peaceUntil[b] = civs[b].peaceUntil[a] = turn + dur(10);
        expelUnits(a, b);
        expelUnits(b, a);
    }
    if (a == player || b == player)
        notify(player, "Peace has been made between " + (isMinor(a) ? tribeTitle(a) : "the " + civs[a].name) + " and " + (isMinor(b) ? tribeTitle(b) : "the " + civs[b].name) + ".");
}

bool Game::proposePeace(int from, int to) {
    if (!civs[from].war[to]) return false;
    if (isMinor(to) || isMinor(from)) {  // tribes make peace once tempers cool and their allies agree
        int m = isMinor(to) ? from : to, t = isMinor(to) ? to : from;
        if (!isMajor(m) || !tribePeaceOk(m, t)) return false;
        makePeace(from, to);
        return true;
    }
    int len = turn - civs[to].warSince[from];
    int pf = militaryPower(from) + 5, pt = militaryPower(to) + 5;
    bool accept = len >= dur(8) && pt < pf * 1.5 && rng.chance(0.75);
    if (civs[to].primate >= 0 && g_primates[civs[to].primate].aggression <= 2 && len >= dur(4)) accept = true;
    if (to != player && skillOf(to) >= 1) accept = peaceValue(to, from) > 0 || (accept && peaceValue(to, from) > -60);
    if (accept) makePeace(from, to);
    return accept;
}

// ------------------------------------------------------------ start of a civ's turn
void Game::beginTurn(int c) {
    for (auto& u : units) {
        if (!u.alive || u.owner != c) continue;
        if (!u.acted && u.hp < 100 && !isWater(u.x, u.y)) {
            int heal = 10;
            const City* ci = cityAt(u.x, u.y);
            if (ci && ci->owner == c) heal = 25;
            else if (at(u.x, u.y).owner == c) heal = 15;
            if (const PrimateDef* pd = primate(c)) heal += pd->healBonus;
            if (c >= FIRST_PRIMATE_CIV) heal += policyPct(c, PolicyEffect::Heal) + civs[c].fx.heal;
            for (auto& hc : cities)
                if (hc.alive && hc.owner == c && dist(hc.x, hc.y, u.x, u.y) <= 1) {
                    int h = buildingIndex("Hospital");
                    if (h >= 0 && hc.has[h]) heal = 100;
                }
            for (auto& m : units)  // a medic tends the unit and its neighbours
                if (m.alive && m.owner == c && hasPromo(m, PR_MEDIC) && dist(m.x, m.y, u.x, u.y) <= 1) { heal += 10; break; }
            u.hp = std::min(100, u.hp + heal);
        }
        // bands tend their sick and wounded even while on the move
        if (u.acted && u.hp < 100 && g_units[u.type].cls == UnitClass::Band && !at(u.x, u.y).fallout) u.hp = std::min(100, u.hp + 10);
        if (u.order == ORD_FORTIFY) u.fortTurns++;
        if (u.order == ORD_SKIP) u.order = ORD_NONE;
        u.acted = false;
        u.attacked = false;
        u.moves = maxMoves(u);
        if (u.order == ORD_SLEEP && u.hp < 100) {}  // keep sleeping
        // work orders keep the unit busy
        if (u.order == ORD_BUILD || u.order == ORD_EXCAVATE || u.order == ORD_DECON) u.moves = 0;
    }
    if (c == player && civs[c].player) {
        for (auto& u : units)
            if (u.alive && u.owner == c && u.order == ORD_GOTO) continueGoto(u);
        // wake sentries when enemies are near
        for (auto& u : units) {
            if (!u.alive || u.owner != c || u.order != ORD_SLEEP) continue;
            for (auto& e : units)
                if (e.alive && atWar(c, e.owner) && dist(e.x, e.y, u.x, u.y) <= 2) { u.order = ORD_NONE; break; }
        }
    }
}

// ------------------------------------------------------------ end of turn: everyone acts, then the world updates
void Game::endTurn() {
    if (gameOver && winner >= 0 && !settings.allAI) {
        // allow continued play after victory; nothing special
    }
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++)
        if (civs[c].alive && !civs[c].player) {
            if (civs[c].minor) aiMinorTurn(c);
            else aiTurn(c);
        }
    feralTurn();
    tradeTurn();
    tribesTurn();
    cassandraCheck();
    volcanoTurn();
    regrowForage();
    for (int c = 0; c < (int)civs.size(); c++)
        if (civs[c].alive && !civs[c].humans) processCiv(c);

    if (turn < PROLOGUE_TURNS) humanNews();
    else if (turn == PROLOGUE_TURNS && !warHappened) triggerNuclearWar();
    if (warHappened) decayFallout();

    checkElimination();
    checkVictory();
    if (turn == PROLOGUE_TURNS) yearAF = 1;
    else if (turn > PROLOGUE_TURNS) yearAF += yearsPerTurn();
    turn++;
    diplomacyTurn();
    for (int c = FIRST_PRIMATE_CIV - 1; c < (int)civs.size(); c++)
        if (civs[c].alive) beginTurn(c);
    updateAllVisibility();
}

void Game::processCiv(int c) {
    Civ& cv = civs[c];
    if (cv.feral) {
        processUnitsEndOfTurn(c);
        return;
    }
    if (isMajor(c)) wearinessTurn(c);
    int sci = 0, cult = 0, gold = 0, prod = 0;
    for (auto& ci : cities) {
        if (!ci.alive || ci.owner != c) continue;
        processCity(ci);
        if (!ci.alive || ci.owner != c) continue;
        sci += ci.yields.sci;
        cult += ci.yields.cult;
        gold += ci.yields.gold;
        prod += ci.yields.prod;
    }
    processBands(c, sci, cult);
    cv.sciencePerTurn = sci;
    cv.culturePerTurn = cult;
    cv.goldPerTurn = hasGold(c) ? gold : 0;
    addScience(c, sci);
    cv.culture += cult;
    cv.cultureTotal += cult;
    cv.gold += cv.goldPerTurn;
    if (cv.minor) {  // tribes neither evolve nor suffer the disasters of the great powers
        processUnitsEndOfTurn(c);
        return;
    }
    greatTurn(c, sci, cult, cv.goldPerTurn, prod);
    disasterTurn(c);
    processEvolution(c);
    processUnitsEndOfTurn(c);
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c) cityBombard(ci);
}

void Game::processCity(City& city) {
    int c = city.owner;
    computeCity(city);
    reportMood(city);
    if (city.droughtTurns > 0) city.droughtTurns--;
    // growth
    city.food += city.foodSurplus;
    int need = growthThreshold(city.pop);
    if (city.food >= need) {
        city.pop++;
        city.food -= need;
        if (city.pop % 3 == 0) notify(c, city.name + " has grown to size " + std::to_string(city.pop) + ".", city.x, city.y);
    } else if (city.food < 0) {
        if (city.pop > 1) {
            city.pop--;
            notify(c, city.name + " is starving and has lost population!", city.x, city.y);
        }
        city.food = 0;
    }
    // production
    city.prod += city.yields.prod;
    if (city.buildKind != BK_NONE) {
        if (!canBuild(city, city.buildKind, city.buildId)) {
            // e.g. a wonder completed elsewhere, or settler at size 1 (wait in that case)
            bool settlerWait = city.buildKind == BK_UNIT && g_units[city.buildId].cls == UnitClass::Settler && city.pop < 2;
            if (!settlerWait) {
                if (city.buildKind == BK_BUILDING && g_buildings[city.buildId].wonder)
                    notify(c, g_buildings[city.buildId].name + " was completed elsewhere. " + city.name + " needs new production.", city.x, city.y);
                city.buildKind = BK_NONE;
                city.buildId = -1;
            }
        } else {
            int cost = buildCost(c, city.buildKind, city.buildId);
            if (city.prod >= cost) {
                bool done = false;
                if (city.buildKind == BK_UNIT && g_units[city.buildId].cls == UnitClass::Nuke) {
                    civs[c].warheads++;
                    notify(c, city.name + " has built a Nuclear Warhead. It waits in the silo (" + std::to_string(civs[c].warheads) + " ready).", city.x, city.y);
                    done = true;
                } else if (city.buildKind == BK_UNIT) {
                    const UnitDef& ud = g_units[city.buildId];
                    Pt spot = {-1, -1};
                    Unit probe;
                    probe.type = city.buildId;
                    probe.owner = c;
                    probe.id = -1;
                    if (canEnter(probe, city.x, city.y)) spot = {city.x, city.y};
                    else {
                        Pt nb[6];
                        int n = neighbors(city.x, city.y, nb);
                        for (int k = 0; k < n; k++)
                        {
                            // ships launch onto the water next to the city, everything else steps onto land
                            bool terrainOk = ud.naval() ? moveCost(probe, city.x, city.y, nb[k].x, nb[k].y) >= 0
                                                        : !isWater(nb[k].x, nb[k].y) && !at(nb[k].x, nb[k].y).mountain;
                            if (terrainOk && canEnter(probe, nb[k].x, nb[k].y) && !isEnemyTarget(c, nb[k].x, nb[k].y)) { spot = nb[k]; break; }
                        }
                    }
                    if (spot.x >= 0) {
                        if (ud.cls == UnitClass::Settler) city.pop--;
                        spawnUnit(c, city.buildId, spot.x, spot.y);
                        units.back().moves = 0;
                        if (ud.military())
                            for (size_t b = 0; b < g_buildings.size(); b++)
                                if (city.has[b] && g_buildings[b].special == BSpecial::Veterans) units.back().xp += 15;  // drilled recruits
                        notify(c, city.name + " has trained a " + ud.name + ".", city.x, city.y);
                        done = true;
                    } else {
                        city.prod = cost;  // blocked: wait, but don't stockpile
                    }
                } else {
                    const BuildingDef& bd = g_buildings[city.buildId];
                    city.has[city.buildId] = 1;
                    done = true;
                    if (bd.wonder) {
                        wonderCity[city.buildId] = city.id;
                        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
                            if (o != c && civs[o].alive && civs[o].met[c]) notify(o, "The " + civs[c].name + " have completed the " + bd.name + ".");
                        popup(c, POP_WONDER, "Wonder Completed", "The " + bd.name + " has been completed in " + city.name + ".\n\n" + bd.desc, city.x, city.y);
                        if (bd.special == BSpecial::FreeTech) {
                            int best = -1;
                            for (size_t k = 0; k < g_techs.size(); k++)
                                if (canResearch(c, (int)k) && (best < 0 || techCost(k) > techCost(best))) best = (int)k;
                            if (best >= 0) grantTech(c, best);
                        }
                        if (bd.special == BSpecial::Victory && !gameOver) {
                            gameOver = true;
                            winner = c;
                            victoryType = "Rebirth";
                            if (c == player)
                                popup(player, POP_VICTORY, "Victory: Rebirth of Civilisation",
                                      "The Rebirth Project is complete. From the ashes of humanity your people have built a new civilisation - "
                                      "one that remembers the mistakes of the last. History will remember the " + civs[player].name + ".");
                            else
                                popup(player, POP_DEFEAT, "Defeat",
                                      "The " + civs[c].name + " have completed the Rebirth Project and will lead the new civilisation.");
                        }
                    }
                    notify(c, city.name + " has completed the " + bd.name + ".", city.x, city.y);
                }
                if (done) {
                    city.prod = std::min(city.prod - cost, cost);
                    city.buildKind = BK_NONE;  // players choose again; the AI re-plans next turn
                    city.buildId = -1;
                }
            }
        }
    } else {
        // idle production is banked, but only up to a point
        city.prod = std::min(city.prod, 60 + 20 * era(c));
    }
    // borders
    int bc = std::max(1, city.yields.cult);
    int bp = policyPct(c, PolicyEffect::Borders);
    if (const PrimateDef* pd = primate(c)) bp += pd->borderPct;
    city.borderCulture += applyPct(bc, bp);
    int need2 = sc(10 + 6 * city.claims + city.claims * city.claims / 2);
    if (city.borderCulture >= need2) {
        int bestI = -1, bestS = -1;
        for (auto& p : ring(city.x, city.y, 3)) {
            Tile& t = at(p.x, p.y);
            if (t.owner >= 0) continue;
            bool adj = false;
            Pt nb[6];
            int n = neighbors(p.x, p.y, nb);
            for (int k = 0; k < n; k++) if (at(nb[k].x, nb[k].y).city == city.id) adj = true;
            if (!adj) continue;
            Yields y = tileYields(c, p.x, p.y);
            int s = y.food * 3 + y.prod * 3 + y.gold + y.sci * 3 + (t.res ? 4 : 0) + (t.ruin ? 5 : 0) + 2 - dist(city.x, city.y, p.x, p.y);
            if (s > bestS) { bestS = s; bestI = idx(p.x, p.y); }
        }
        if (bestI >= 0) {
            claimTile(city, bestI % W, bestI / W);
            city.claims++;
            city.borderCulture -= need2;
        } else {
            city.borderCulture = need2;  // nothing to claim
        }
    }
    // heal
    int maxhp = cityMaxHP(city);
    int heal = 15 + policyPct(c, PolicyEffect::CityDefense) / 5;
    city.hp = std::min(maxhp, city.hp + heal);
}

void Game::processUnitsEndOfTurn(int c) {
    size_t count = units.size();  // rewards may spawn units: index, never hold references across calls
    for (size_t ui = 0; ui < count; ui++) {
        if (!units[ui].alive || units[ui].owner != c) continue;
        if (units[ui].order == ORD_EXCAVATE && --units[ui].work <= 0) {
            units[ui].order = ORD_NONE;
            int tier = excavateTier(units[ui]);
            if (tier >= 0) excavateReward(c, units[ui].x, units[ui].y, tier);
        }
        Unit& u = units[ui];
        if (u.order == ORD_BUILD || u.order == ORD_DECON) {
            if (--u.work <= 0) {
                Tile& t = at(u.x, u.y);
                if (u.order == ORD_BUILD && u.buildImp == IMP_ROAD) {
                    int next = roadOption(u);
                    if (next > 0) {
                        t.road = (uint8_t)next;
                        civs[c].bestRoad = std::max(civs[c].bestRoad, next);
                        if (c == player) notify(c, g_roads[next].name + " completed.", u.x, u.y);
                    }
                } else if (u.order == ORD_BUILD) {
                    t.imp = (uint8_t)u.buildImp;
                    if (t.imp == IMP_FARM && t.veg == V_JUNGLE) t.veg = V_NONE;
                    if (t.imp == IMP_FARM && t.feature == F_MARSH) t.feature = F_NONE;  // drained
                    notify(c, g_improvements[u.buildImp].name + " completed.", u.x, u.y);
                } else {
                    if (t.fallout > 0) t.fallout--;
                    civs[c].falloutCleaned++;
                    notify(c, "Fallout reduced.", u.x, u.y);
                }
                u.order = ORD_NONE;
            }
        }
        int fo = at(u.x, u.y).fallout;
        if (fo > 0 && u.owner != CIV_FERAL) {
            int dmg = 8 * fo;
            const PrimateDef* pd = primate(c);
            if (pd && pd->falloutResist) dmg /= 2;
            if (civs[c].fx.falloutHalf) dmg /= 2;
            if (policyPct(c, PolicyEffect::Heal) > 0) dmg /= 2;
            u.hp -= dmg;
            if (u.hp <= 0) {
                notify(c, "Your " + g_units[u.type].name + " died of radiation sickness.", u.x, u.y);
                killUnit(u);
            }
        }
    }
}

void Game::cityBombard(City& city) {
    Unit* target = nullptr;
    for (auto& u : units) {
        if (!u.alive || !atWar(city.owner, u.owner) || dist(u.x, u.y, city.x, city.y) > 2) continue;
        if (!target || u.hp < target->hp) target = &u;
    }
    if (!target) return;
    double A = cityStrength(city) * 0.7;
    double D = unitStrength(*target, false, false, target->x, target->y);
    int dmg = combatDamage(A, D);
    target->hp -= dmg;
    civs[city.owner].battles++;
    civs[target->owner].battles++;
    std::string msg = city.name + " bombarded " + (target->owner == player ? "your " : civs[target->owner].adjective + " ") +
                      g_units[target->type].name + " (-" + std::to_string(dmg) + ")";
    if (target->hp <= 0) {
        msg += ", destroying it";
        killUnit(*target);
    }
    notify(city.owner, msg, target->x, target->y);
    notify(target->owner, msg, target->x, target->y);
}

// ------------------------------------------------------------ the last days of humanity
void Game::humanNews() {
    static const char* NEWS[] = {
        "",
        "Human radio: border clashes erupt between the nuclear powers. Both sides blame the other.",
        "Human radio: armies are mobilising. Bombers fly over the jungle every night.",
        "Human radio: peace talks have collapsed. Leaders retreat to their bunkers.",
        "Human radio: the cities are being evacuated. The night sky flickers with test launches.",
        "Human radio: sirens wail across every human city. DEFCON 1.",
    };
    if (turn >= 1 && turn <= 5) {
        for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) notify(c, NEWS[turn]);
        if (turn == 5)
            popup(player, POP_INFO, "The Sirens",
                  "The human cities are howling with sirens. Strange lights rise from the military bases and arc across the sky.\n\n"
                  "Whatever is coming, it is coming soon. Keep your troop far from the human cities.");
    }
}

void Game::triggerNuclearWar() {
    warHappened = true;
    markResourcesDirty();
    civs[CIV_HUMANS].alive = false;
    for (auto& t : tiles) if (t.owner == CIV_HUMANS) t.owner = -1;
    int killed = 0;
    for (auto& s : sites) {
        int blast = g_ruins[s.type].blastRadius;
        for (auto& p : ring(s.x, s.y, blast)) {
            Tile& t = at(p.x, p.y);
            int d = dist(s.x, s.y, p.x, p.y);
            int lvl = std::min(3, blast - d + 1);
            if (s.type == RUIN_CITY && d <= 1) lvl = 3;
            if (lvl > t.fallout && !isWater(p.x, p.y)) t.fallout = (uint8_t)lvl;
            if (d <= 1 && !isWater(p.x, p.y) && !t.mountain) {
                if (t.veg != V_NONE && rng.chance(0.8)) t.veg = V_NONE;
                t.imp = IMP_NONE;
            }
        }
        if (s.type == RUIN_CITY)
            for (auto& p : ring(s.x, s.y, 2)) {
                Tile& t = at(p.x, p.y);
                if (!isWater(p.x, p.y) && !t.mountain && t.res == R_NONE && t.site < 0 && rng.chance(0.3)) t.res = R_SCRAP;
            }
    }
    for (auto& u : units) {
        if (!u.alive) continue;
        int fo = at(u.x, u.y).fallout;
        if (fo >= 3) {
            notify(u.owner, "Your " + g_units[u.type].name + " was caught in a nuclear blast.", u.x, u.y);
            killUnit(u);
            killed++;
        } else if (fo == 2) {
            u.hp = std::max(1, u.hp - 50);
        }
    }
    for (auto& ci : cities) {
        if (!ci.alive) continue;
        int fo = at(ci.x, ci.y).fallout;
        for (auto& p : ring(ci.x, ci.y, 1)) fo = std::max(fo, (int)at(p.x, p.y).fallout);
        if (fo > 0) {
            ci.pop = std::max(1, ci.pop - fo);
            ci.hp = cityMaxHP(ci) / 2;
            notify(ci.owner, ci.name + " was struck by the blast and the fallout.", ci.x, ci.y);
        }
    }
    nukeAnimPending = civs[player].player;
    int nCities = 0;
    for (auto& s : sites) if (s.type == RUIN_CITY) nCities++;
    popup(player, POP_NUKE, "The Fall - August 2034",
          "It took less than an hour.\n\n"
          "The missiles rose from every continent. " + std::to_string(nCities) + " great cities and every human settlement on Earth vanished in fire. "
          "The skies darkened, the radio fell silent, and humanity was gone - wiped out completely by its own hand.\n\n"
          "The primates endure. The ruins of the human world still glow with poison, but the fallout will fade with time. "
          "Within those ruins lie the knowledge of a lost civilisation: scrap, books, machines and data, waiting for minds advanced enough to use them.\n\n"
          "The age of humanity is over. The rebirth of civilisation begins with you.");
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++)
        notify(c, "THE FALL: Humanity has destroyed itself in a global nuclear war.");
}

void Game::decayFallout() {
    for (auto& t : tiles)
        if (t.fallout > 0 && rng.range(dur(16)) == 0) t.fallout--;
}

// ------------------------------------------------------------ ferals
void Game::feralTurn() {
    if (!warHappened) return;
    int count = 0;
    for (auto& u : units) if (u.alive && u.owner == CIV_FERAL) count++;
    int cap = std::min(24, numMajors() * 2 + turn * 100 / speedPct / 40);
    if (turn % 3 == 0 && count < cap) {
        for (int tries = 0; tries < 40; tries++) {
            int x, y;
            if (!sites.empty() && rng.chance(0.6)) {
                const HumanSite& s = sites[rng.range((int)sites.size())];
                x = s.x + rng.between(-3, 3);
                y = s.y + rng.between(-3, 3);
            } else {
                x = rng.range(W);
                y = rng.range(H);
            }
            if (!inMap(x, y) || isWater(x, y) || at(x, y).mountain || at(x, y).fallout >= 3) continue;
            if (!unitsAt(x, y).empty()) continue;
            bool seen = false;
            for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size() && !seen; c++)
                if (civs[c].alive && civs[c].visible[idx(x, y)]) seen = true;
            if (seen) continue;
            bool nearCity = false;
            for (auto& ci : cities) if (ci.alive && dist(ci.x, ci.y, x, y) < 5) nearCity = true;
            for (auto& b : units)  // nor right next to wandering bands
                if (b.alive && g_units[b.type].cls == UnitClass::Band && dist(b.x, b.y, x, y) < 6) nearCity = true;
            if (nearCity) continue;
            int type = unitIndex(turn < dur(90) ? "Feral Dogs" : turn < dur(190) ? "Mutant Hounds" : "Rogue Machines");
            spawnUnit(CIV_FERAL, type, x, y);
            break;
        }
    }
    for (size_t i = 0; i < units.size(); i++) {
        Unit& u = units[i];
        if (!u.alive || u.owner != CIV_FERAL) continue;
        // pick nearest prey within 5
        int bx = -1, by = -1, bd = 99;
        for (auto& o : units) {
            if (!o.alive || o.owner < FIRST_PRIMATE_CIV) continue;
            int d = dist(u.x, u.y, o.x, o.y);
            if (d < bd && d <= 5 && !isWater(o.x, o.y)) { bd = d; bx = o.x; by = o.y; }
        }
        if (u.hp < 40) bx = -1;  // wounded animals retreat
        if (bx >= 0) {
            if (bd == 1 && canAttack(u, bx, by)) { attack(u, bx, by); continue; }
            std::vector<Pt> p = findPath(u, bx, by, true);
            for (size_t k = 0; k + 1 < p.size() && u.moves > 0; k++)
                if (!stepUnit(u, p[k].x, p[k].y)) break;
            if (u.alive && u.moves > 0 && dist(u.x, u.y, bx, by) == 1 && canAttack(u, bx, by)) attack(u, bx, by);
        } else {
            for (int s = 0; s < 2 && u.moves > 0; s++) {
                Pt nb[6];
                int n = neighbors(u.x, u.y, nb);
                Pt p = nb[rng.range(n)];
                if (moveCost(u, u.x, u.y, p.x, p.y) > 0 && moveCost(u, u.x, u.y, p.x, p.y) < 99 && !cityAt(p.x, p.y)) stepUnit(u, p.x, p.y);
            }
        }
    }
}

// ------------------------------------------------------------ end of game
void Game::checkElimination() {
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) {
        Civ& cv = civs[c];
        if (!cv.alive) continue;
        if (numCities(c) > 0) continue;
        bool settler = false;
        for (auto& u : units)
            if (u.alive && u.owner == c && (g_units[u.type].cls == UnitClass::Settler || g_units[u.type].cls == UnitClass::Band)) settler = true;
        if (settler) continue;
        cv.alive = false;
        for (auto& u : units) if (u.alive && u.owner == c) killUnit(u);
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
            if (o != c && civs[o].alive && (!cv.minor || civs[o].met[c]))
                notify(o, cv.minor ? "The " + cv.name + " of " + g_tribes[cv.tribe].city + " are no more." : "The " + cv.name + " have been wiped out.");
        if (c == player && !gameOver) {
            gameOver = true;
            winner = -1;
            victoryType = "Defeat";
            popup(player, POP_DEFEAT, "Defeat", "Your people have been wiped out. Like the humans before you, your civilisation has ended.");
        }
    }
}

void Game::checkVictory() {
    if (gameOver) return;
    int alivePrim = 0;
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) if (civs[c].alive && !civs[c].minor) alivePrim++;
    auto win = [&](int c, const std::string& type, const std::string& text) {
        gameOver = true;
        winner = c;
        victoryType = type;
        if (c == player) popup(player, POP_VICTORY, type + " Victory!", text);
        else popup(player, POP_DEFEAT, "Defeat", "The " + civs[c].name + " have achieved a " + type + " victory.\n\n" + text);
    };
    // domination
    int capitals = 0;
    for (auto& ci : cities) if (ci.alive && ci.origCapital) capitals++;
    if (capitals >= 2 || alivePrim == 1) {
        for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) {
            if (!civs[c].alive || civs[c].minor) continue;
            bool all = true;
            for (auto& ci : cities) if (ci.alive && ci.origCapital && ci.owner != c) all = false;
            if (all && turn > PROLOGUE_TURNS + 1 && numCities(c) > 0) {
                win(c, "Domination", "The " + civs[c].name + " control every original capital of the primate world.");
                return;
            }
        }
    }
    // culture
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++)
        if (civs[c].alive && !civs[c].minor && civs[c].policiesAdopted >= CULTURE_WIN_POLICIES) {
            win(c, "Cultural", "The " + civs[c].name + " have adopted " + std::to_string(CULTURE_WIN_POLICIES) + " traditions. Their songs, stories and art define the new world.");
            return;
        }
    // time
    if (turn >= maxTurns) {
        int best = -1;
        for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++)
            if (civs[c].alive && !civs[c].minor && (best < 0 || score(c) > score(best))) best = c;
        if (best >= 0) win(best, "Score", "Time has run out. The " + civs[best].name + " have the highest score.");
    }
}
