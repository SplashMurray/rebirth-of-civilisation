// Rebirth of Civilisation - naval warfare: the computer's admirals
//
// Ships are ordinary units with their own rules (see moveCost, canEnter and canAttack in Game.cpp):
// they sail on water (early ones only along the coast), dock in their own coastal cities, ram
// ships and boats at sea, storm coastal cities, and ranged ships bombard anything in range.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

int Game::aiShipWant(int c, bool warNow) const {
    int coastal = 0, river = 0;
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c) {
            if (isCoastal(ci.x, ci.y)) coastal++;
            else if (hasRiver(ci.x, ci.y)) river++;
        }
    coastal += river / 2;  // inland river ports want a boat or two as well
    if (!coastal) return 0;
    int sk = skillOf(c);
    // a small navy in peacetime: enough to guard the coast and scout, not to starve the land army
    int want = sk == 0 ? coastal / 4 : sk == 1 ? std::max(1, coastal / 3) : std::max(1, (coastal + 1) / 2);
    if (warNow) {
        bool seaWar = false;  // an enemy we can reach by sea
        for (auto& ci : cities)
            if (ci.alive && ci.owner >= FIRST_PRIMATE_CIV && atWar(c, ci.owner) && isCoastal(ci.x, ci.y)) seaWar = true;
        for (auto& u : units)
            if (u.alive && atWar(c, u.owner) && g_units[u.type].naval()) seaWar = true;
        if (seaWar) want += sk >= 2 ? coastal / 2 + 1 : coastal / 3 + 1;
    }
    return want;
}

int Game::aiBestShip(int c, bool wantRanged, bool riverOnly) const {
    int best = -1, bs = -1;
    for (int u : availableUnits(c)) {
        const UnitDef& d = g_units[u];
        if (!d.naval() || (riverOnly && !d.river)) continue;
        int s = std::max(d.str, d.rstr) * 3 + (d.ocean ? 10 : 0) + (d.ranged() == wantRanged ? 25 : 0);
        if (s > bs) { bs = s; best = u; }
    }
    return best;
}

void Game::aiNaval(Unit& uref) {
    int uid = uref.id;
    auto U = [&]() -> Unit& { return units[uid]; };
    int c = U().owner;
    const UnitDef& d = g_units[U().type];
    int sk = skillOf(c);
    if (U().moves <= 0) return;
    if (U().order == ORD_FORTIFY || U().order == ORD_SLEEP) U().order = ORD_NONE;
    auto portFor = [&](const City& ci) { return isCoastal(ci.x, ci.y) || (d.river && hasRiver(ci.x, ci.y)); };  // where this boat can go
    if (aiFerryJob(U())) {
        aiTransportShip(U());
        return;
    }
    auto sail = [&](int tx, int ty) {
        U().order = ORD_GOTO; U().gx = tx; U().gy = ty;
        continueGoto(U());
        if (U().alive && U().order == ORD_GOTO) U().order = ORD_NONE;
    };
    auto standOff = [&](int tx, int ty, int want) {  // sail to `want` tiles from a target
        std::vector<Pt> p = findPath(U(), tx, ty, true);
        if (p.size() < 2) return false;
        int k = std::max(0, (int)p.size() - 1 - want);
        sail(p[k].x, p[k].y);
        return true;
    };
    auto odds = [&](const Unit& e) {
        double A = unitStrength(U(), true, d.ranged(), U().x, U().y), D = unitStrength(e, false, false, e.x, e.y);
        return std::make_pair(combatDamage(A, D), d.ranged() || g_units[e.type].civilian() ? 0 : combatDamage(D, A));
    };

    // 1. strike anything in reach worth hitting
    auto strike = [&]() {
        int bx = -1, by = -1;
        double bv = 0;
        for (auto& e : units) {
            if (!e.alive || !atWar(c, e.owner) || !canAttack(U(), e.x, e.y)) continue;
            auto o = odds(e);
            bool kill = o.first >= e.hp;
            double v = kill ? 300 + o.first : o.first - o.second * 1.2;
            if (g_units[e.type].civilian()) v = 250;
            if (sk >= 1 && !d.ranged() && !kill && o.second >= U().hp - 10) continue;
            if (sk == 0) v = std::max(v, 1.0);
            if (v > bv) { bv = v; bx = e.x; by = e.y; }
        }
        for (auto& ci : cities) {
            if (!ci.alive || !atWar(c, ci.owner) || !canAttack(U(), ci.x, ci.y)) continue;
            double A = unitStrength(U(), true, d.ranged(), U().x, U().y) * (d.ranged() ? 1.0 : 1.5);
            bool capture = !d.ranged() && combatDamage(A, cityStrength(ci)) >= ci.hp && ci.owner != CIV_FERAL;
            double v = d.ranged() ? 120 : capture ? 400 : 0;
            if (v > bv) { bv = v; bx = ci.x; by = ci.y; }
        }
        if (bx >= 0 && bv > 0) { attack(U(), bx, by); return true; }
        return false;
    };
    if (strike()) return;

    // 2. wounded ships go home to repair
    if (U().hp < (sk == 0 ? 30 : 50)) {
        const City* home = nullptr;
        int hd = 99;
        for (auto& ci : cities)
            if (ci.alive && ci.owner == c && portFor(ci) && dist(ci.x, ci.y, U().x, U().y) < hd) {
                Unit probe = U();
                if (!(ci.x == U().x && ci.y == U().y) && findPath(probe, ci.x, ci.y).empty()) continue;
                hd = dist(ci.x, ci.y, U().x, U().y);
                home = &ci;
            }
        if (home) {
            if (home->x != U().x || home->y != U().y) sail(home->x, home->y);
            if (U().alive) U().order = ORD_FORTIFY;
            return;
        }
    }

    // 3. war at sea: hunt enemy ships, then bombard or storm enemy coastal cities
    bool atWarAny = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (civs[c].war[o] && civs[o].alive) atWarAny = true;
    if (atWarAny) {
        int bx = -1, by = -1, bd = 99;
        for (auto& e : units) {
            if (!e.alive || !atWar(c, e.owner) || (!isWater(e.x, e.y) && !g_units[e.type].naval())) continue;
            int dd = dist(e.x, e.y, U().x, U().y);
            if (dd > 8 || dd >= bd) continue;
            auto o = odds(e);
            if (sk >= 1 && !d.ranged() && o.first < o.second && e.hp > 40) continue;
            bd = dd; bx = e.x; by = e.y;
        }
        if (bx >= 0 && standOff(bx, by, d.ranged() ? d.range : 1)) {
            if (U().alive && U().moves > 0) strike();
            return;
        }
        // an enemy coastal city: the war plan's target if it has one, else the nearest
        const City* tgt = nullptr;
        if (sk >= 2 && c < (int)aiPlans.size() && aiPlans[c].target >= 0) {
            const City& T = cities[aiPlans[c].target];
            if (T.alive && portFor(T) && (aiPlans[c].assault || d.ranged())) tgt = &T;
        }
        if (!tgt) {
            int td = 99;
            for (auto& ci : cities)
                if (ci.alive && ci.owner >= FIRST_PRIMATE_CIV && atWar(c, ci.owner) && portFor(ci) && dist(ci.x, ci.y, U().x, U().y) < std::min(td, 14)) {
                    td = dist(ci.x, ci.y, U().x, U().y);
                    tgt = &ci;
                }
        }
        if (tgt) {
            int want = d.ranged() ? d.range : 1;
            if (dist(tgt->x, tgt->y, U().x, U().y) > want) {
                int tx = tgt->x, ty = tgt->y;
                if (!standOff(tx, ty, want)) tgt = nullptr;
            }
            if (tgt) {
                if (U().alive && U().moves > 0) strike();
                return;
            }
        }
    }

    // 4. peacetime: one ship scouts unknown waters, the rest guard the home coast
    bool scout = true;
    for (auto& o : units)
        if (o.alive && o.owner == c && o.id < uid && g_units[o.type].naval()) scout = false;
    if (scout && sk >= 1) {
        const Civ& cv = civs[c];
        for (int r = 1; r <= 10; r++) {
            int tries = 0;
            for (auto& p : ring(U().x, U().y, r)) {
                if (dist(p.x, p.y, U().x, U().y) != r || cv.explored[idx(p.x, p.y)] || !isWater(p.x, p.y)) continue;
                if (at(p.x, p.y).terrain == T_OCEAN && !d.ocean) continue;
                if (++tries > 4) break;
                Unit probe = U();
                if (findPath(probe, p.x, p.y).empty()) continue;
                sail(p.x, p.y);
                return;
            }
        }
    }
    const City* home = nullptr;
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c && portFor(ci) && (!home || dist(ci.x, ci.y, U().x, U().y) < dist(home->x, home->y, U().x, U().y)))
            home = &ci;
    if (!home) return;
    if (dist(home->x, home->y, U().x, U().y) > 2) {
        int hx = home->x, hy = home->y;
        sail(hx, hy);
        return;
    }
    U().order = ORD_FORTIFY;
}
