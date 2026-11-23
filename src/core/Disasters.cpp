// Rebirth of Civilisation - natural and man-made disasters
// Disasters are rolled each turn for every primate civilisation. Which disaster strikes depends on
// where that civilisation lives (forests burn, coasts flood, crowded cities catch disease, ruined
// reactors leak...). Every disaster also adds evolutionary pressure: an immediate burst to the
// matching pool plus a lingering pressure for a few turns.
// SPDX-License-Identifier: MIT
#include "Game.h"

namespace {
enum DisasterType { DS_WILDFIRE, DS_DROUGHT, DS_FLOOD, DS_EPIDEMIC, DS_EARTHQUAKE, DS_WINTER, DS_REACTOR, DS_MIGRATION, DS_COUNT };
}

void Game::disasterTurn(int c) {
    Civ& cv = civs[c];
    if (turn < 3) return;
    if (turn - cv.lastDisaster < dur(6)) return;
    // roughly one disaster every 14 turns at Quick speed, proportionally rarer (per turn) on longer games
    if (!rng.chance(0.10 * 100.0 / speedPct)) return;
    if (numCities(c) == 0) {  // still wandering: disasters strike the bands
        if (nomadDisaster(c)) cv.lastDisaster = turn;
        return;
    }

    int forest = 0, dry = 0, rough = 0, cold = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Tile& t = at(x, y);
            if (t.owner != c || isWater(x, y)) continue;
            if (t.veg != V_NONE) forest++;
            if ((t.terrain == T_DESERT || t.terrain == T_PLAINS) && t.veg == V_NONE) dry++;
            if (t.hills || t.mountain) rough++;
            if (t.terrain == T_TUNDRA || t.terrain == T_SNOW) cold++;
        }
    int coastal = 0, crowd = 0, plants = 0;
    for (auto& ci : cities) {
        if (!ci.alive || ci.owner != c) continue;
        Pt nb[6];
        int n = neighbors(ci.x, ci.y, nb);
        for (int k = 0; k < n; k++)
            if (at(nb[k].x, nb[k].y).terrain == T_COAST) { coastal++; break; }
        crowd += std::max(0, ci.pop - 5) * 2;
    }
    if (warHappened)
        for (auto& s : sites) {
            if (s.type != RUIN_POWER) continue;
            for (auto& ci : cities)
                if (ci.alive && ci.owner == c && dist(s.x, s.y, ci.x, ci.y) <= 6) { plants++; break; }
        }
    bool nuclearWinter = warHappened && turn <= PROLOGUE_TURNS + dur(30);
    int w[DS_COUNT] = {};
    w[DS_WILDFIRE] = forest >= 3 ? forest : 0;
    w[DS_DROUGHT] = dry >= 4 ? dry / 2 + 2 : 0;
    w[DS_FLOOD] = coastal * 4;
    w[DS_EPIDEMIC] = crowd;
    w[DS_EARTHQUAKE] = rough >= 3 ? rough / 2 : 0;
    w[DS_WINTER] = (cold >= 3 ? cold / 2 : 0) + (nuclearWinter ? 8 : 0);
    w[DS_REACTOR] = plants * 8;
    w[DS_MIGRATION] = warHappened ? 5 : 0;
    for (int& v : w) v = std::min(v, 20);  // keep the mix varied in big empires
    int total = 0;
    for (int v : w) total += v;
    if (total <= 0) return;
    int r = rng.range(total);
    for (int d = 0; d < DS_COUNT; d++) {
        if (r < w[d]) {
            if (triggerDisaster(c, d)) cv.lastDisaster = turn;
            return;
        }
        r -= w[d];
    }
}

bool Game::triggerDisaster(int c, int type) {
    Civ& cv = civs[c];
    std::vector<int> mine;
    for (auto& ci : cities) if (ci.alive && ci.owner == c) mine.push_back(ci.id);
    if (mine.empty()) return false;
    std::string title, body, pressureText;
    int fx = -1, fy = -1;

    // immediate burst to a pool plus a lingering pressure
    auto pressure = [&](const std::string& name, int cat, int burst, int perTurn, int turns) {
        int b = sc(burst);
        cv.evoPool[cat] += b;
        if (perTurn > 0) {
            TempPressure tp;
            tp.name = name;
            tp.cat = cat;
            tp.value = perTurn;
            tp.turns = dur(turns);
            cv.tempPressures.push_back(tp);
        }
        std::string line = std::string("\n") + PRESSURE_NAMES[cat] + ": ";
        if (b > 0) line += "+" + std::to_string(b) + " points now";
        if (b > 0 && perTurn > 0) line += ", then ";
        if (perTurn > 0) line += "+" + std::to_string(perTurn) + " per turn for " + std::to_string(dur(turns)) + " turns";
        pressureText += line;
    };
    auto hurt = [&](Unit& u, int dmg) {
        u.hp -= dmg;
        if (u.hp <= 0) {
            notify(u.owner, "Your " + g_units[u.type].name + " was killed by the disaster.", u.x, u.y);
            killUnit(u);
        }
    };

    switch (type) {
        case DS_WILDFIRE: {
            std::vector<Pt> trees;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                    if (at(x, y).owner == c && at(x, y).veg != V_NONE && !isWater(x, y)) trees.push_back({x, y});
            if (trees.empty()) return false;
            Pt o = trees[rng.range((int)trees.size())];
            int burned = 0;
            for (auto& p : ring(o.x, o.y, 1)) {
                Tile& t = at(p.x, p.y);
                if (t.veg == V_NONE || isWater(p.x, p.y)) continue;
                if (!(p.x == o.x && p.y == o.y) && !rng.chance(0.65)) continue;
                t.veg = V_NONE;
                if (t.imp == IMP_LUMBER) t.imp = IMP_NONE;
                burned++;
                for (auto& u : units) if (u.alive && u.x == p.x && u.y == p.y) hurt(u, 25);
            }
            fx = o.x; fy = o.y;
            title = "Wildfire";
            body = "Lightning struck a dry forest and the fire spread fast. " + std::to_string(burned) +
                   " tiles of forest and jungle in your lands have burned to ash. The troop must learn to read the land - and to flee in time.";
            pressure("Wildfire aftermath", PC_ECOLOGY, 12, 2, 8);
            break;
        }
        case DS_DROUGHT: {
            int best = -1, bestDry = -1;
            for (int id : mine) {
                const City& ci = cities[id];
                int d = 0;
                for (auto& p : ring(ci.x, ci.y, 3)) {
                    const Tile& t = at(p.x, p.y);
                    if (t.city == ci.id && (t.terrain == T_DESERT || t.terrain == T_PLAINS) && t.veg == V_NONE) d++;
                }
                if (d > bestDry) { bestDry = d; best = id; }
            }
            City& centre = cities[best];
            int hit = 0;
            std::string names;
            for (int id : mine) {
                City& ci = cities[id];
                if (dist(ci.x, ci.y, centre.x, centre.y) > 6) continue;
                ci.droughtTurns = dur(8);
                hit++;
                names += (names.empty() ? "" : ", ") + ci.name;
            }
            fx = centre.x; fy = centre.y;
            title = "Drought";
            body = "The rains have failed. Rivers shrink to mud and the fruit withers on the branch. Food is a third lower in " + names +
                   " for " + std::to_string(dur(8)) + " turns. Only those who plan ahead - and those who fight hardest for water - will thrive.";
            pressure("Drought", PC_ECOLOGY, 10, 2, 8);
            pressure("Competition for water", PC_SOCIAL, 0, 1, 8);
            break;
        }
        case DS_FLOOD: {
            std::vector<int> coast;
            for (int id : mine) {
                const City& ci = cities[id];
                Pt nb[6];
                int n = neighbors(ci.x, ci.y, nb);
                for (int k = 0; k < n; k++)
                    if (at(nb[k].x, nb[k].y).terrain == T_COAST) { coast.push_back(id); break; }
            }
            if (coast.empty()) return false;
            City& ci = cities[coast[rng.range((int)coast.size())]];
            if (ci.pop > 1) ci.pop--;
            ci.food /= 2;
            int lost = 0;
            for (auto& p : ring(ci.x, ci.y, 1)) {
                Tile& t = at(p.x, p.y);
                if (t.imp != IMP_NONE && rng.chance(0.5)) { t.imp = IMP_NONE; lost++; }
            }
            fx = ci.x; fy = ci.y;
            title = "Flood";
            body = "A storm surge swept into " + ci.name + ". Lives were lost, stores were ruined" +
                   (lost ? " and " + std::to_string(lost) + " improvements were washed away." : ".");
            pressure("Flood", PC_ECOLOGY, 8, 0, 0);
            pressure("Flood survivors band together", PC_SOCIAL, 0, 1, 6);
            break;
        }
        case DS_EPIDEMIC: {
            int best = mine[0];
            for (int id : mine) if (cities[id].pop > cities[best].pop) best = id;
            City& ci = cities[best];
            if (ci.pop < 3) return false;
            int loss = std::max(1, ci.pop / 5);
            ci.pop -= loss;
            int spread = 0;
            for (int id : mine) {
                City& o = cities[id];
                if (o.id == ci.id || o.pop < 4 || dist(o.x, o.y, ci.x, ci.y) > 5) continue;
                o.pop--;
                spread++;
            }
            for (auto& u : units)
                if (u.alive && u.owner == c && dist(u.x, u.y, ci.x, ci.y) <= 1) hurt(u, 20);
            fx = ci.x; fy = ci.y;
            title = "Epidemic";
            body = "A sickness tears through crowded " + ci.name + ", killing " + std::to_string(loss) + " population" +
                   (spread ? " and spreading to " + std::to_string(spread) + " nearby settlements." : ".") +
                   " The survivors carry stronger immune systems - and a new understanding of keeping clean.";
            pressure("Disease survivors", PC_MUTATION, 10, 2, 6);
            pressure("Plague", PC_SOCIAL, 8, 0, 0);
            break;
        }
        case DS_EARTHQUAKE: {
            int best = -1, bestRough = -1;
            for (int id : mine) {
                const City& ci = cities[id];
                int rr = 0;
                for (auto& p : ring(ci.x, ci.y, 2)) if (at(p.x, p.y).hills || at(p.x, p.y).mountain) rr++;
                if (rr > bestRough) { bestRough = rr; best = id; }
            }
            City& ci = cities[best];
            ci.hp = std::max(1, ci.hp - cityMaxHP(ci) * 40 / 100);
            std::string lostB;
            if (rng.chance(0.35)) {
                std::vector<int> cand;
                for (size_t b = 0; b < g_buildings.size(); b++)
                    if (ci.has[b] && !g_buildings[b].wonder && g_buildings[b].special != BSpecial::Palace) cand.push_back((int)b);
                if (!cand.empty()) {
                    int b = cand[rng.range((int)cand.size())];
                    ci.has[b] = 0;
                    lostB = g_buildings[b].name;
                }
            }
            int lost = 0;
            for (auto& p : ring(ci.x, ci.y, 1)) {
                Tile& t = at(p.x, p.y);
                if (t.imp != IMP_NONE && rng.chance(0.4)) { t.imp = IMP_NONE; lost++; }
            }
            fx = ci.x; fy = ci.y;
            title = "Earthquake";
            body = "The ground heaved beneath " + ci.name + ". Its defences are badly damaged" +
                   (lostB.empty() ? "" : ", the " + lostB + " collapsed") + (lost ? " and nearby improvements were wrecked." : ".");
            pressure("Earthquake", PC_ECOLOGY, 12, 0, 0);
            pressure("Rebuilding together", PC_SOCIAL, 0, 1, 6);
            break;
        }
        case DS_WINTER: {
            bool nuclearWinter = warHappened && turn <= PROLOGUE_TURNS + dur(30);
            int hitC = 0;
            for (int id : mine) {
                City& ci = cities[id];
                bool coldNear = nuclearWinter;
                for (auto& p : ring(ci.x, ci.y, 2))
                    if (at(p.x, p.y).terrain == T_TUNDRA || at(p.x, p.y).terrain == T_SNOW) coldNear = true;
                if (!coldNear) continue;
                ci.food /= 2;
                hitC++;
                if (fx < 0) { fx = ci.x; fy = ci.y; }
            }
            if (!hitC) return false;
            for (auto& u : units) {
                if (!u.alive || u.owner != c || cityAt(u.x, u.y)) continue;
                int tr = at(u.x, u.y).terrain;
                if (nuclearWinter || tr == T_TUNDRA || tr == T_SNOW) hurt(u, 15);
            }
            title = nuclearWinter ? "Nuclear Winter Storm" : "Harsh Winter";
            body = std::string(nuclearWinter ? "Ash still blots out the sun. " : "") +
                   "A savage winter freezes your lands. Food stores in " + std::to_string(hitC) +
                   " settlements are halved and troops caught in the open suffer frostbite. Only the resourceful survive the cold.";
            pressure("Bitter cold", PC_ECOLOGY, 15, 3, 6);
            break;
        }
        case DS_REACTOR: {
            const HumanSite* plant = nullptr;
            for (auto& s : sites) {
                if (s.type != RUIN_POWER) continue;
                for (int id : mine)
                    if (dist(s.x, s.y, cities[id].x, cities[id].y) <= 6 && (!plant || rng.chance(0.5))) plant = &s;
            }
            if (!plant) return false;
            for (auto& p : ring(plant->x, plant->y, 2)) {
                if (isWater(p.x, p.y)) continue;
                int lvl = 3 - dist(plant->x, plant->y, p.x, p.y);
                Tile& t = at(p.x, p.y);
                if (lvl > t.fallout) t.fallout = (uint8_t)lvl;
                for (auto& u : units) if (u.alive && u.x == p.x && u.y == p.y) hurt(u, 15 * lvl);
            }
            fx = plant->x; fy = plant->y;
            title = "Reactor Leak";
            body = "The ruined " + plant->name + " has cracked open again. Fresh fallout pours across the surrounding land. "
                   "Your people sicken - and some of their children are born different.";
            pressure("Radiation leak", PC_MUTATION, 20, 3, 10);
            break;
        }
        case DS_MIGRATION: {
            City& ci = cities[mine[rng.range((int)mine.size())]];
            int ft = unitIndex(turn < dur(90) ? "Feral Dogs" : turn < dur(190) ? "Mutant Hounds" : "Rogue Machines");
            int want = 3 + (era(c) >= 4 ? 1 : 0), made = 0;
            for (int tries = 0; tries < 60 && made < want; tries++) {
                auto cand = ring(ci.x, ci.y, 5);
                Pt p = cand[rng.range((int)cand.size())];
                int d = dist(p.x, p.y, ci.x, ci.y);
                if (d < 3 || isWater(p.x, p.y) || at(p.x, p.y).mountain || cityAt(p.x, p.y) || !unitsAt(p.x, p.y).empty()) continue;
                spawnUnit(CIV_FERAL, ft, p.x, p.y);
                made++;
            }
            if (!made) return false;
            fx = ci.x; fy = ci.y;
            title = "Predator Migration";
            body = "Driven from their old hunting grounds, " + std::to_string(made) + " packs of " + g_units[ft].name +
                   " have arrived near " + ci.name + ". Your people must fight or flee.";
            pressure("Predator migration", PC_CONFLICT, 10, 2, 8);
            break;
        }
        default: return false;
    }
    statDisasters++;
    statDisasterKind[type]++;
    notify(c, "Disaster: " + title + ".", fx, fy);
    popup(c, POP_DISASTER, title, body + "\n\nEvolutionary pressure:" + pressureText, fx, fy);
    return true;
}
