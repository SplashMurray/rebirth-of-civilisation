// Rebirth of Civilisation - unit upgrades and naval transports
//
// Upgrades: every military unit belongs to a line (Warband -> Spear Troop -> Iron Blades -> ...).
// Inside your own territory a unit can be refitted as the most advanced unit of its line you can
// train. Once you have Currency it costs gold; before that the work is done in one of your cities
// and paid for out of that city's stored production.
//
// Transports: ships with cargo space carry land units. A land unit boards by moving onto the
// ship's tile (or with Board Ship in a harbour city), travels with it, and lands by moving onto an
// adjacent land tile. If the ship sinks, everyone aboard is lost.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

// ------------------------------------------------------------ upgrades
bool Game::canTrain(int c, int t) const {
    if (t < 0 || t >= (int)g_units.size()) return false;
    const UnitDef& d = g_units[t];
    return unitBelongsTo(c, t) && hasTech(c, d.techId) && hasNeeds(c, d.needs);
}

int Game::upgradeTarget(const Unit& u) const {
    const UnitDef& d = g_units[u.type];
    if (!d.military() || u.owner < FIRST_PRIMATE_CIV) return -1;
    int base = d.replacesId >= 0 ? d.replacesId : u.type;  // unique units follow their base unit's line
    int best = -1;
    for (int step = g_units[base].upgradeId, guard = 0; step >= 0 && guard < 12; step = g_units[step].upgradeId, guard++) {
        int t = uniqueFor(u.owner, step);
        if (canTrain(u.owner, t)) best = t;
    }
    return best;
}

int Game::upgradeGold(const Unit& u, int to) const {
    int diff = buildCost(u.owner, BK_UNIT, to) - buildCost(u.owner, BK_UNIT, u.type);
    return std::max(10, diff * 3 / 2 + 10);
}

int Game::upgradeProd(const Unit& u, int to) const {
    int diff = buildCost(u.owner, BK_UNIT, to) - buildCost(u.owner, BK_UNIT, u.type);
    return std::max(5, diff);
}

bool Game::canUpgrade(const Unit& u, std::string* why) const {
    auto fail = [&](const std::string& m) { if (why) *why = m; return false; };
    int to = upgradeTarget(u);
    if (to < 0) return fail("No better unit of this line is available yet");
    if (u.moves <= 0 || u.attacked) return fail("Needs movement left this turn");
    if (!inMap(u.x, u.y) || at(u.x, u.y).owner != u.owner) return fail("Must be inside your own territory");
    if (g_units[u.type].naval() && cargoCount(u) > 0) return fail("Unload the ship first");
    if (u.carrier >= 0) return fail("Cannot be refitted while aboard a ship");
    if (hasGold(u.owner)) {
        int g = upgradeGold(u, to);
        if (civs[u.owner].gold < g) return fail("Costs " + std::to_string(g) + " gold");
        return true;
    }
    const City* ci = cityAt(u.x, u.y);
    if (!ci || ci->owner != u.owner) return fail("Before Currency, units are refitted inside one of your cities");
    int p = upgradeProd(u, to);
    if (ci->prod < p) return fail(ci->name + " needs " + std::to_string(p) + " stored production (has " + std::to_string(ci->prod) + ")");
    return true;
}

bool Game::upgradeUnit(Unit& u) {
    if (!canUpgrade(u)) return false;
    int to = upgradeTarget(u);
    std::string from = g_units[u.type].name;
    if (hasGold(u.owner)) civs[u.owner].gold -= upgradeGold(u, to);
    else if (City* ci = cityAt(u.x, u.y)) ci->prod -= upgradeProd(u, to);
    u.type = to;
    u.moves = 0;
    u.acted = true;
    u.order = ORD_NONE;
    u.fortTurns = 0;
    civs[u.owner].unitsUpgraded++;
    notify(u.owner, "Your " + from + " has been refitted as a " + g_units[to].name + ".", u.x, u.y);
    return true;
}

void Game::aiUpgrades(int c) {
    int sk = skillOf(c);
    if (sk == 0 || !hasGold(c)) return;
    int reserve = sk >= 2 ? sc(25) : sc(60);
    for (int guard = 0; guard < 8; guard++) {
        int best = -1;
        double bv = 0;
        for (auto& u : units) {
            if (!u.alive || u.owner != c || !canUpgrade(u)) continue;
            int to = upgradeTarget(u);
            int cost = upgradeGold(u, to);
            if (civs[c].gold - cost < reserve) continue;
            double gain = std::max(g_units[to].str, g_units[to].rstr) - std::max(g_units[u.type].str, g_units[u.type].rstr);
            double v = gain / cost;
            if (v > bv) { bv = v; best = u.id; }
        }
        if (best < 0) break;
        upgradeUnit(units[best]);
    }
}

// ------------------------------------------------------------ transports
int Game::cargoCount(const Unit& ship) const {
    int n = 0;
    for (auto& o : units) if (o.alive && o.carrier == ship.id) n++;
    return n;
}

int Game::cargoSpace(const Unit& ship) const {
    return std::max(0, g_units[ship.type].cargo - cargoCount(ship));
}

const Unit* Game::transportAt(int c, int x, int y, int exclude) const {
    const Unit* found = nullptr;
    forUnitsAt(x, y, [&](const Unit& s) {
        if (found || s.owner != c || !g_units[s.type].naval() || g_units[s.type].cargo <= 0) return;
        int n = 0;
        if (occDepth > 0) forUnitsAt(x, y, [&](const Unit& o) { if (o.carrier == s.id && o.id != exclude) n++; });  // cargo rides on its ship's tile
        else for (auto& o : units) if (o.alive && o.carrier == s.id && o.id != exclude) n++;
        if (n < g_units[s.type].cargo) found = &s;
    });
    return found;
}

bool Game::boardShip(Unit& u) {
    const UnitDef& d = g_units[u.type];
    if (d.naval() || d.cls == UnitClass::Nuke || u.carrier >= 0 || u.moves <= 0) return false;
    const Unit* s = transportAt(u.owner, u.x, u.y, u.id);
    if (!s) return false;
    u.carrier = s->id;
    u.moves = 0;
    u.order = ORD_SLEEP;
    u.acted = true;
    return true;
}

bool Game::leaveShip(Unit& u) {
    if (u.carrier < 0 || isWater(u.x, u.y)) return false;  // at sea, land by moving onto the shore
    // stepping off in a harbour city: the land slot must be free
    for (auto& o : units)
        if (o.alive && o.id != u.id && o.x == u.x && o.y == u.y && o.carrier < 0 && !g_units[o.type].naval() &&
            g_units[o.type].civilian() == g_units[u.type].civilian())
            return false;
    u.carrier = -1;
    u.order = ORD_NONE;
    return true;
}

// ------------------------------------------------------------ AI ferrying
// A land unit whose goal lies across the water walks to the best harbour, waits for a ship, sails
// and lands next to the goal. Returns true if the unit has been given something to do.
bool Game::aiFerry(Unit& uref, int gx, int gy) {
    int uid = uref.id;
    auto U = [&]() -> Unit& { return units[uid]; };
    int c = U().owner;
    if (U().moves <= 0) return true;
    // aboard: land as soon as there is a shore from which the goal can be reached
    if (U().carrier >= 0) {
        Unit& ship = units[U().carrier];
        ship.ferryX = gx;
        ship.ferryY = gy;
        if (!isWater(U().x, U().y)) return true;  // still in port
        Pt nb[6];
        int n = neighbors(U().x, U().y, nb), best = -1, bd = 99;
        for (int k = 0; k < n; k++) {
            int x = nb[k].x, y = nb[k].y;
            if (isWater(x, y) || moveCost(U(), U().x, U().y, x, y) < 0 || !canEnter(U(), x, y)) continue;
            int d = dist(x, y, gx, gy);
            if (d >= bd) continue;
            Unit probe = U();
            probe.x = x; probe.y = y; probe.carrier = -1;
            if (!(x == gx && y == gy) && d > 1 && findPath(probe, gx, gy, true).empty()) continue;
            bd = d; best = k;
        }
        if (best >= 0 && bd <= 3) {
            stepUnit(U(), nb[best].x, nb[best].y);
            if (U().alive) { U().ferryX = U().ferryY = -1; }
        }
        return true;
    }
    if (skillOf(c) == 0) return false;
    // find the harbour: our coastal city reachable on foot, closest to the goal
    const City* port = nullptr;
    int pd = 1 << 20;
    for (auto& ci : cities) {
        if (!ci.alive || ci.owner != c || !isCoastal(ci.x, ci.y)) continue;
        int d = dist(ci.x, ci.y, gx, gy);
        if (d >= pd) continue;
        if (!(ci.x == U().x && ci.y == U().y)) {
            Unit probe = U();
            if (findPath(probe, ci.x, ci.y).empty()) continue;
        }
        pd = d; port = &ci;
    }
    if (!port) return false;
    U().ferryX = gx;
    U().ferryY = gy;
    if (port->x != U().x || port->y != U().y) {
        U().order = ORD_GOTO; U().gx = port->x; U().gy = port->y;
        continueGoto(U());
        if (U().alive && U().order == ORD_GOTO) U().order = ORD_NONE;
        return true;
    }
    if (boardShip(U())) {
        units[U().carrier].ferryX = gx;
        units[U().carrier].ferryY = gy;
        return true;
    }
    U().order = ORD_NONE;  // wait on the quay; a ship will come (or be built)
    return true;
}

bool Game::aiFerryJob(const Unit& ship) const {
    if (g_units[ship.type].cargo <= 0) return false;
    if (cargoCount(ship) > 0) return true;
    for (auto& o : units)
        if (o.alive && o.owner == ship.owner && o.carrier < 0 && o.ferryX >= 0 && !g_units[o.type].naval()) {
            const City* ci = cityAt(o.x, o.y);
            if (ci && ci->owner == ship.owner && isCoastal(ci->x, ci->y)) return true;
        }
    return false;
}

void Game::aiTransportShip(Unit& sref) {
    int sid = sref.id;
    auto S = [&]() -> Unit& { return units[sid]; };
    int c = S().owner;
    auto sail = [&](int tx, int ty) {
        S().order = ORD_GOTO; S().gx = tx; S().gy = ty;
        continueGoto(S());
        if (S().alive && S().order == ORD_GOTO) S().order = ORD_NONE;
    };
    // load anyone waiting on our quay
    if (const City* here = cityAt(S().x, S().y)) {
        if (here->owner == c)
            for (auto& o : units) {
                if (cargoSpace(S()) <= 0) break;
                if (o.alive && o.owner == c && o.x == S().x && o.y == S().y && o.carrier < 0 && o.ferryX >= 0 && !g_units[o.type].naval()) {
                    o.carrier = sid;
                    o.order = ORD_SLEEP;
                    S().ferryX = o.ferryX;
                    S().ferryY = o.ferryY;
                }
            }
    }
    int load = cargoCount(S());
    if (load > 0) {
        bool waiting = false;  // still people on the quay and room aboard: wait one more turn
        if (cityAt(S().x, S().y) && cargoSpace(S()) > 0)
            for (auto& o : units)
                if (o.alive && o.owner == c && o.carrier < 0 && o.ferryX >= 0 && o.order == ORD_GOTO && o.gx == S().x && o.gy == S().y &&
                    dist(o.x, o.y, S().x, S().y) <= 3)
                    waiting = true;
        if (waiting) return;
        int gx = S().ferryX, gy = S().ferryY;
        if (gx < 0) {
            for (auto& o : units) if (o.alive && o.carrier == sid && o.ferryX >= 0) { gx = o.ferryX; gy = o.ferryY; break; }
        }
        if (gx < 0) return;
        // a landing spot: water next to land, as close to the goal as possible, that we can reach
        int bx = -1, by = -1, bd = 1 << 20;
        for (int r = 1; r <= 4 && bx < 0; r++)
            for (auto& p : ring(gx, gy, r)) {
                if (dist(p.x, p.y, gx, gy) != r || !isWater(p.x, p.y) || !isCoastal(p.x, p.y)) continue;
                if (moveCost(S(), p.x, p.y, p.x, p.y) < 0) continue;  // coastal-only ship on the ocean
                bool shore = false;
                Pt nb[6];
                int n = neighbors(p.x, p.y, nb);
                for (int k = 0; k < n; k++)
                    if (!isWater(nb[k].x, nb[k].y) && !at(nb[k].x, nb[k].y).mountain && dist(nb[k].x, nb[k].y, gx, gy) < r) shore = true;
                if (!shore) continue;
                int d = dist(p.x, p.y, S().x, S().y);
                if (d >= bd) continue;
                if (!(p.x == S().x && p.y == S().y)) {
                    Unit probe = S();
                    if (findPath(probe, p.x, p.y).empty()) continue;
                }
                bd = d; bx = p.x; by = p.y;
            }
        if (bx < 0) {  // nowhere to land: bring them home
            for (auto& o : units) if (o.alive && o.carrier == sid) { o.ferryX = o.ferryY = -1; }
            S().ferryX = S().ferryY = -1;
            return;
        }
        if (bx != S().x || by != S().y) sail(bx, by);
        return;
    }
    S().ferryX = S().ferryY = -1;
    // empty: go and pick up whoever is waiting in a harbour
    const City* best = nullptr;
    int bd = 99;
    for (auto& o : units) {
        if (!o.alive || o.owner != c || o.carrier >= 0 || o.ferryX < 0 || g_units[o.type].naval()) continue;
        const City* ci = cityAt(o.x, o.y);
        if (!ci || ci->owner != c || !isCoastal(ci->x, ci->y)) continue;
        int d = dist(ci->x, ci->y, S().x, S().y);
        if (d >= bd) continue;
        if (!(ci->x == S().x && ci->y == S().y)) {
            Unit probe = S();
            if (findPath(probe, ci->x, ci->y).empty()) continue;
        }
        bd = d; best = ci;
    }
    if (best && (best->x != S().x || best->y != S().y)) sail(best->x, best->y);
}

int Game::aiNeedsTransport(const City& port) const {
    int c = port.owner, waiting = 0;
    for (auto& o : units)
        if (o.alive && o.owner == c && o.carrier < 0 && o.ferryX >= 0 && o.x == port.x && o.y == port.y && !g_units[o.type].naval()) waiting++;
    if (!waiting) return -1;
    for (auto& s : units)  // a ship that can come for them already exists
        if (s.alive && s.owner == c && g_units[s.type].cargo > 0) return -1;
    for (auto& ci : cities)  // or is being built
        if (ci.alive && ci.owner == c && ci.buildKind == BK_UNIT && g_units[ci.buildId].cargo > 0 && ci.id != port.id) return -1;
    int best = -1;
    for (int t : availableUnits(c)) {
        const UnitDef& d = g_units[t];
        if (d.cargo <= 0) continue;
        if (best < 0 || d.cargo > g_units[best].cargo || (d.cargo == g_units[best].cargo && d.moves > g_units[best].moves)) best = t;
    }
    return best;
}
