// Rebirth of Civilisation - nuclear weapons: the primates rediscover the bomb
//
// Nuclear Fission lets a city build a Nuclear Warhead (it needs Uranium). Warheads are kept in
// the silos rather than on the map and can be launched at any tile within NUKE_RANGE of one of
// your cities. A detonation destroys every unit close to it, wrecks cities, leaves fallout, and
// turns the rest of the world against the one who launched it.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

bool Game::canNukeAt(int c, int x, int y, std::string* why) const {
    auto fail = [&](const char* m) { if (why) *why = m; return false; };
    if (c < 0 || c >= (int)civs.size()) return fail("No such civilisation");
    if (civs[c].warheads <= 0) return fail("You have no nuclear warheads");
    if (!inMap(x, y)) return fail("Off the map");
    if (!civs[c].explored[idx(x, y)]) return fail("You cannot target land you have never seen");
    bool inRange = false;
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c && dist(ci.x, ci.y, x, y) <= NUKE_RANGE) inRange = true;
    if (!inRange) return fail("Out of range: targets must be within 12 tiles of one of your cities");
    return true;
}

bool Game::launchNuke(int c, int x, int y) {
    if (!canNukeAt(c, x, y)) return false;
    Civ& cv = civs[c];
    cv.warheads--;
    cv.nukesLaunched++;
    if (council.formed) {
        if (council.chair == c) {  // the chair of the council that was meant to stop this
            if ((int)council.abuse.size() < (int)civs.size()) council.abuse.resize(civs.size(), 0);
            council.abuse[c] += 4;
        }
        if (resolutionActive(RS_NONPROLIF))  // breaking the treaty: every member condemns it
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
                if (o != c && isMajor(o)) civs[o].grievance[c] += 30;
    }
    statNukes++;
    blasts.push_back({x, y});

    // who was hit?
    std::vector<int> victims;
    auto hit = [&](int o) {
        if (o >= FIRST_PRIMATE_CIV && o != c && std::find(victims.begin(), victims.end(), o) == victims.end()) victims.push_back(o);
    };
    int unitsKilled = 0;
    std::string cityHit;
    for (auto& p : ring(x, y, 2)) {
        int d = dist(x, y, p.x, p.y);
        Tile& t = at(p.x, p.y);
        if (!isWater(p.x, p.y)) {
            int lvl = d <= 1 ? 3 : 2;
            if (lvl > t.fallout) t.fallout = (uint8_t)lvl;
            if (d <= 1) {
                if (t.veg != V_NONE && rng.chance(0.8)) t.veg = V_NONE;
                if (!cityAt(p.x, p.y)) t.imp = IMP_NONE;
            }
        }
        if (t.owner >= 0) hit(t.owner);
        for (auto& u : units) {
            if (!u.alive || u.x != p.x || u.y != p.y) continue;
            hit(u.owner);
            if (d <= 1) {
                if (u.owner != c && u.owner >= FIRST_PRIMATE_CIV) unitsKilled++;
                killUnit(u);
            } else {
                u.hp -= 50;
                if (u.hp <= 0) killUnit(u);
            }
        }
        if (City* ci = cityAt(p.x, p.y)) {
            hit(ci->owner);
            int lost = d == 0 ? (ci->pop + 1) / 2 : ci->pop / 3;
            ci->pop = std::max(1, ci->pop - lost);
            ci->hp = d == 0 ? 1 : std::max(1, ci->hp / 2);
            ci->food = 0;
            if (d == 0)  // half the ordinary buildings are flattened
                for (size_t b = 0; b < g_buildings.size(); b++)
                    if (ci->has[b] && !g_buildings[b].wonder && g_buildings[b].special != BSpecial::Palace && rng.chance(0.5)) ci->has[b] = 0;
            computeCity(*ci);
            if (cityHit.empty() || d == 0) cityHit = ci->name;
            if (ci->owner != c) civs[ci->owner].timesNuked++;
        }
    }
    markResourcesDirty();

    std::string where = cityHit.empty() ? "open country" : cityHit;
    std::string who = "The " + civs[c].name;
    for (int o : viewers()) {
        if (!civs[o].alive) continue;
        if (o == c) {
            popup(o, POP_NUKE, "Here We Go Again",
                  "The warhead streaks into the sky. Seconds later a second sun rises over " + where + ".\n\n"
                  "The first civilisation ended this way. Every species that knows you has seen what you did, and few will forgive it. "
                  "Your victims are now at war with you." + (unitsKilled ? "\n\nEnemy units destroyed: " + std::to_string(unitsKilled) : std::string()),
                  x, y);
        } else if (std::find(victims.begin(), victims.end(), o) != victims.end()) {
            popup(o, POP_NUKE, "Nuclear Attack!",
                  "The " + civs[c].name + " have launched a nuclear warhead at " + where + ". The old nightmare has returned.\n\n"
                  "Your people are at war with the " + civs[c].name + ".",
                  x, y);
        } else if (civs[o].met[c]) {
            notify(o, who + " detonated a nuclear warhead at " + where + "!", x, y);
        }
    }
    // the world reacts: victims go to war, everyone else who knows you is horrified
    for (int o : victims) {
        civs[o].grievance[c] += 60;
        civs[o].peaceUntil[c] = civs[c].peaceUntil[o] = 0;
        declareWar(o, c);
    }
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
        if (o != c && civs[o].met[c]) civs[o].grievance[c] += 15;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
        if (o == c || !civs[o].alive || !civs[o].met[c] || civs[o].war[c]) continue;
        if (!human(o) && o != player && rng.chance(0.35)) declareWar(o, c);
    }
    // horror is evolutionary pressure for everyone nearby
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
        if (!civs[o].alive) continue;
        bool near = false;
        for (auto& ci : cities) if (ci.alive && ci.owner == o && dist(ci.x, ci.y, x, y) <= 6) near = true;
        if (near) civs[o].tempPressures.push_back({"Nuclear fallout", PC_MUTATION, 3, dur(10)});
    }

    return true;
}

// Hard and Expert AIs use the bomb - rarely, and only in a war that matters.
void Game::aiNukes(int c) {
    Civ& cv = civs[c];
    if (cv.warheads <= 0 || skillOf(c) < 2) return;
    const PrimateDef* pd = primate(c);
    int aggr = pd ? pd->aggression : 5;
    if (!rng.chance(0.05 * aggr)) return;
    const City* best = nullptr;
    double bv = 0;
    for (auto& ci : cities) {
        if (!ci.alive || ci.owner < FIRST_PRIMATE_CIV || !atWar(c, ci.owner) || !canNukeAt(c, ci.x, ci.y)) continue;
        bool ownNear = false;  // don't hit our own people
        for (auto& u : units) if (u.alive && u.owner == c && dist(u.x, u.y, ci.x, ci.y) <= 2) ownNear = true;
        for (auto& m : cities) if (m.alive && m.owner == c && dist(m.x, m.y, ci.x, ci.y) <= 2) ownNear = true;
        if (ownNear) continue;
        double v = ci.pop * 2 + cityStrength(ci) / 20.0 + (ci.capital ? 8 : 0);
        if (v > bv) { bv = v; best = &ci; }
    }
    if (best) launchNuke(c, best->x, best->y);
}
