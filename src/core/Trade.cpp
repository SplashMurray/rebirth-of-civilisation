// Rebirth of Civilisation - trade caravans, trade routes and roads
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>
#include <queue>

namespace {
const int LAND_RANGE = 15;     // tiles a caravan will travel overland
const int SEA_RANGE = 20;      // tiles by sea along the coast
const int SEA_RANGE_NAV = 30;  // ... and with Navigation, across the ocean
const int ROUTE_TURNS = 30;

int marketId() {
    static int id = buildingIndex("Market");
    return id;
}
}  // namespace

// ------------------------------------------------------------ roads
int Game::bestRoad(int c) const {
    if (c < FIRST_PRIMATE_CIV) return ROAD_TRACK;
    int best = ROAD_NONE;
    for (int l = ROAD_TRACK; l < ROAD_LEVELS; l++)
        if (g_roads[l].techId < 0 || hasTech(c, g_roads[l].techId)) best = l;
    return best;
}

int Game::roadLevelAt(int x, int y) const {
    const Tile& t = at(x, y);
    if (t.city >= 0 && t.city < (int)cities.size()) {
        const City& ci = cities[t.city];
        if (ci.alive && ci.x == x && ci.y == y) return std::max<int>(t.road, std::max<int>(ROAD_TRACK, bestRoad(ci.owner)));
    }
    return t.road;
}

int Game::roadOption(const Unit& u) const {
    if (g_units[u.type].cls != UnitClass::Worker || !inMap(u.x, u.y)) return -1;
    const Tile& t = at(u.x, u.y);
    if (isWater(u.x, u.y) || t.mountain || t.fallout >= 2 || cityAt(u.x, u.y)) return -1;
    if (t.site >= 0 && !warHappened) return -1;
    if (t.owner >= 0 && t.owner != u.owner) return -1;  // not in someone else's land
    int next = t.road + 1;
    if (next >= ROAD_LEVELS || next > bestRoad(u.owner)) return -1;
    return next;
}

// ------------------------------------------------------------ capacity
int Game::tradeCapacity(int c) const {
    if (c < FIRST_PRIMATE_CIV || !hasTech(c, ID.tCurrency)) return 0;
    int cap = 1;
    for (int t : {ID.tNavigation, ID.tBanking, ID.tIndustrial, ID.tComputing})
        if (hasTech(c, t)) cap++;
    int markets = 0;
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c && ci.has[marketId()]) markets++;
    if (resolutionActive(RS_TRADE)) cap++;  // the World Council's Trade Charter
    return cap + markets / 2;
}

int Game::routesOf(int c) const {
    int n = 0;
    for (auto& r : routes) if (r.owner == c) n++;
    return n;
}

int Game::tradeUsed(int c) const {
    int n = routesOf(c);
    for (auto& u : units) if (u.alive && u.owner == c && g_units[u.type].cls == UnitClass::Trader) n++;
    return n;
}

// ------------------------------------------------------------ paths
// The way a caravan would go: overland (preferring roads), or by sea between two coastal cities.
std::vector<int> Game::tradePath(int c, int from, int to, bool sea) const {
    std::vector<int> path;
    const City& A = cities[from];
    const City& B = cities[to];
    int N = W * H, s = idx(A.x, A.y), goal = idx(B.x, B.y);
    std::vector<int> cost(N, 1 << 29), steps(N, 0), prev(N, -1);
    typedef std::pair<int, int> QE;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
    cost[s] = 0;
    pq.push({0, s});
    bool nav = hasTech(c, ID.tNavigation);
    int range = sea ? (nav ? SEA_RANGE_NAV : SEA_RANGE) : LAND_RANGE;
    while (!pq.empty()) {
        auto [cc, cur] = pq.top();
        pq.pop();
        if (cc != cost[cur]) continue;
        if (cur == goal) break;
        if (steps[cur] >= range) continue;
        int cx = cur % W, cy = cur / W;
        if (sea && cur != s && !isWater(cx, cy) && !hasRiver(cx, cy)) continue;  // a city is only a port at either end
        Pt nb[6];
        int n = neighbors(cx, cy, nb);
        for (int k = 0; k < n; k++) {
            int ni = idx(nb[k].x, nb[k].y);
            const Tile& t = tiles[ni];
            int step;
            if (sea) {  // by boat: along the coast, across lakes and up and down rivers
                if (!waterStep(cx, cy, nb[k].x, nb[k].y, nav, true)) continue;
                if (!isWater(nb[k].x, nb[k].y) && ni != goal && !hasRiver(nb[k].x, nb[k].y)) continue;
                step = !isWater(cx, cy) && downstream(cx, cy, nb[k].x, nb[k].y) ? MP * 2 / 3 : MP;
            } else {
                if (isWater(nb[k].x, nb[k].y) || t.mountain) continue;
                if (t.site >= 0 && !warHappened && ni != goal) continue;
                int l = std::min(roadLevelAt(cx, cy), roadLevelAt(nb[k].x, nb[k].y));
                bool ford = hasRiver(nb[k].x, nb[k].y) && !riverLink(cx, cy, nb[k].x, nb[k].y);
                step = l > 0 ? g_roads[l].cost
                             : MP + (t.hills ? MP / 2 : 0) + (t.veg != V_NONE || t.feature == F_MARSH ? MP / 2 : 0) + (ford ? MP / 2 : 0);
            }
            int ng = cc + step;
            if (ng < cost[ni]) {
                cost[ni] = ng;
                steps[ni] = steps[cur] + 1;
                prev[ni] = cur;
                pq.push({ng, ni});
            }
        }
    }
    if (prev[goal] < 0) return path;
    for (int cur = goal; cur >= 0; cur = prev[cur]) path.push_back(cur);
    std::reverse(path.begin(), path.end());
    return path;
}

// ------------------------------------------------------------ yields
Yields Game::routeYields(const TradeRoute& r, bool forOrigin) const {
    Yields y;
    if (r.from < 0 || r.to < 0 || r.path.size() < 2) return y;
    const City& D = cities[r.to];
    bool foreign = D.owner != r.owner;
    int len = (int)r.path.size() - 1;
    // roads along the way (or the sea) make a route richer
    int pct = 0;
    if (r.sea) pct = 25 + (hasTech(r.owner, ID.tNavigation) ? 15 : 0);
    else {
        int sum = 0;
        for (int ti : r.path) sum += g_roads[roadLevelAt(ti % W, ti / W)].tradePct;
        pct = sum / (int)r.path.size();
    }
    if (foreign) {
        int gold = 2 + D.pop / 2 + len / 4;
        if (forOrigin) {
            y.gold = applyPct(gold, pct);
            int newIdeas = 0;  // what they know that we don't
            for (size_t t = 0; t < g_techs.size(); t++)
                if (civs[D.owner].techs[t] && !civs[r.owner].techs[t]) newIdeas++;
            y.sci = newIdeas > 0 ? std::min(4, 1 + newIdeas / 4) : 0;
        } else {
            y.gold = std::max(1, applyPct(gold, pct) / 3);
        }
    } else if (forOrigin) {
        y.food = applyPct(1 + D.pop / 4, pct);
        y.prod = applyPct(1 + D.pop / 5, pct);
        y.gold = 1;
    }
    return y;
}

bool Game::routeValid(const TradeRoute& r, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    if (r.from < 0 || r.to < 0 || r.from >= (int)cities.size() || r.to >= (int)cities.size()) return fail("gone");
    const City& O = cities[r.from];
    const City& D = cities[r.to];
    if (!O.alive || O.owner != r.owner) return fail("its home city was lost");
    if (!D.alive) return fail(D.name + " is no more");
    if (D.owner != r.owner) {
        if (D.owner < FIRST_PRIMATE_CIV) return fail(D.name + " has fallen");
        if (atWar(r.owner, D.owner)) return fail("war broke out with the " + civs[D.owner].name);
        if (embargoed(r.owner, D.owner)) return fail("of the embargo between you and the " + civs[D.owner].name);
    }
    return true;
}

Yields Game::cityTradeYields(const City& city) const {
    Yields y;
    for (auto& r : routes) {
        if (r.from != city.id && r.to != city.id) continue;
        if (!routeValid(r)) continue;
        if (r.from == city.id && r.owner == city.owner) y += routeYields(r, true);
        if (r.to == city.id && r.owner != city.owner) y += routeYields(r, false);
    }
    return y;
}

// ------------------------------------------------------------ options
RouteOption Game::routeOption(int c, int from, int to, bool sea) const {
    RouteOption o;
    o.city = to;
    o.sea = sea;
    auto fail = [&](const std::string& w) { o.ok = false; o.why = w; return o; };
    if (from < 0 || to < 0 || from == to) return fail("Not possible");
    const City& A = cities[from];
    const City& B = cities[to];
    if (!A.alive || A.owner != c || !B.alive) return fail("Not possible");
    int d = B.owner;
    if (d < FIRST_PRIMATE_CIV) return fail("Not possible");
    if (d != c) {
        if (!civs[c].met[d]) return fail("You have not met them");
        if (atWar(c, d)) return fail("You are at war");
        if (embargoed(c, d)) return fail(civs[c].embargo[d] ? "You have an embargo on them" : "They have an embargo on you");
    }
    for (auto& r : routes)
        if (r.owner == c && r.from == from && r.to == to) return fail("A caravan already trades between these cities");
    if (sea) {
        if (!hasTech(c, ID.tRafts)) return fail("Needs Rafts");
        if (!isPort(A.x, A.y) || !isPort(B.x, B.y)) return fail("Both cities must be on the coast, a lake or a river");
    }
    o.path = tradePath(c, from, to, sea);
    if (o.path.empty()) return fail(sea ? "No sea route within reach" : "Too far overland (" + std::to_string(LAND_RANGE) + " tiles at most)");
    TradeRoute r;
    r.owner = c; r.from = from; r.to = to; r.sea = sea; r.path = o.path;
    o.length = (int)o.path.size() - 1;
    o.origin = routeYields(r, true);
    o.dest = d != c ? routeYields(r, false) : Yields();
    if (sea) o.roadPct = 25 + (hasTech(c, ID.tNavigation) ? 15 : 0);
    else {
        int sum = 0;
        for (int ti : o.path) sum += g_roads[roadLevelAt(ti % W, ti / W)].tradePct;
        o.roadPct = sum / (int)o.path.size();
    }
    o.ok = true;
    return o;
}

std::vector<RouteOption> Game::tradeOptions(const Unit& u) const {
    std::vector<RouteOption> out;
    if (g_units[u.type].cls != UnitClass::Trader) return out;
    const City* here = cityAt(u.x, u.y);
    if (!here || here->owner != u.owner) return out;
    const Civ& me = civs[u.owner];
    for (auto& b : cities) {
        if (!b.alive || b.id == here->id || b.owner < FIRST_PRIMATE_CIV) continue;
        if (b.owner != u.owner && !me.met[b.owner]) continue;
        if (!me.explored[idx(b.x, b.y)]) continue;
        if (dist(here->x, here->y, b.x, b.y) > SEA_RANGE_NAV + 2) continue;
        RouteOption land = routeOption(u.owner, here->id, b.id, false);
        RouteOption sea = isPort(here->x, here->y) && isPort(b.x, b.y) ? routeOption(u.owner, here->id, b.id, true) : RouteOption();
        // offer the better of the two ways (by sea only when it pays more or the land way is impossible)
        auto value = [](const RouteOption& o) { return o.origin.gold * 3 + o.origin.sci * 4 + o.origin.food * 3 + o.origin.prod * 3; };
        if (sea.ok && (!land.ok || value(sea) > value(land))) out.push_back(sea);
        else if (land.ok || dist(here->x, here->y, b.x, b.y) <= LAND_RANGE) out.push_back(land);
    }
    std::sort(out.begin(), out.end(), [](const RouteOption& a, const RouteOption& b) {
        if (a.ok != b.ok) return a.ok;
        return a.origin.gold * 3 + a.origin.sci * 4 + a.origin.food * 3 + a.origin.prod * 3 >
               b.origin.gold * 3 + b.origin.sci * 4 + b.origin.food * 3 + b.origin.prod * 3;
    });
    return out;
}

// ------------------------------------------------------------ establishing and ending routes
bool Game::establishRoute(Unit& u, int toCity, bool sea) {
    if (g_units[u.type].cls != UnitClass::Trader || u.moves <= 0) return false;
    const City* here = cityAt(u.x, u.y);
    if (!here || here->owner != u.owner) return false;
    int c = u.owner, from = here->id;
    RouteOption o = routeOption(c, from, toCity, sea);
    if (!o.ok) return false;
    TradeRoute r;
    r.owner = c; r.from = from; r.to = toCity; r.sea = sea; r.path = o.path;
    r.start = turn;
    r.until = turn + dur(ROUTE_TURNS);
    if (!sea)  // the caravans' feet wear a track
        for (int ti : r.path) {
            Tile& t = tiles[ti];
            if (t.road == ROAD_NONE && !isWater(ti % W, ti / W) && !t.mountain) t.road = ROAD_TRACK;
        }
    routes.push_back(r);
    civs[c].routesMade++;
    City& D = cities[toCity];
    if (D.owner != c)
        notify(D.owner, "A " + civs[c].adjective + " caravan from " + cities[from].name + " has begun trading with " + D.name + ".", D.x, D.y);
    killUnit(u);
    computeCity(cities[from]);
    computeCity(D);
    return true;
}

// Ends route i. A caravan that comes home reappears in (or next to) its home city.
void Game::endRoute(size_t i, bool comeHome, const std::string& why) {
    if (i >= routes.size()) return;
    TradeRoute r = routes[i];
    routes.erase(routes.begin() + (long)i);
    if (r.from >= 0 && r.from < (int)cities.size()) {
        City& home = cities[r.from];
        std::string where = r.to >= 0 && r.to < (int)cities.size() ? cities[r.to].name : std::string("abroad");
        bool back = false;
        if (comeHome && home.alive && home.owner == r.owner) {
            Unit probe;
            probe.type = uniqueFor(r.owner, ID.uCaravan);
            probe.owner = r.owner;
            probe.id = -1;
            Pt spot = {-1, -1};
            if (canEnter(probe, home.x, home.y)) spot = {home.x, home.y};
            else
                for (auto& p : ring(home.x, home.y, 2))
                    if (!isWater(p.x, p.y) && !at(p.x, p.y).mountain && canEnter(probe, p.x, p.y) && !isEnemyTarget(r.owner, p.x, p.y)) { spot = p; break; }
            if (spot.x >= 0) {
                spawnUnit(r.owner, probe.type, spot.x, spot.y);
                units.back().moves = 0;
                back = true;
            }
        }
        std::string msg = "The trade route from " + home.name + " to " + where + (why.empty() ? " has ended" : " ended: " + why) + ".";
        if (comeHome) msg += back ? " The caravan is back home." : " The caravan could not find its way home.";
        notify(r.owner, msg, home.x, home.y);
        if (home.alive) computeCity(home);
    }
    if (r.to >= 0 && r.to < (int)cities.size() && cities[r.to].alive) computeCity(cities[r.to]);
}

void Game::cancelRoutesBetween(int a, int b, const std::string& why) {
    for (size_t i = routes.size(); i-- > 0;) {
        const TradeRoute& r = routes[i];
        if (r.to < 0 || r.to >= (int)cities.size()) continue;
        int d = cities[r.to].owner;
        if ((r.owner == a && d == b) || (r.owner == b && d == a)) endRoute(i, true, why);
    }
}

// Each turn: routes run out, lose their cities, or are plundered by enemies standing on them.
void Game::tradeTurn() {
    for (size_t i = routes.size(); i-- > 0;) {
        TradeRoute& r = routes[i];
        std::string why;
        if (!routeValid(r, &why)) {
            bool homeOk = r.from >= 0 && r.from < (int)cities.size() && cities[r.from].alive && cities[r.from].owner == r.owner;
            endRoute(i, homeOk, why);
            continue;
        }
        // plunder: an enemy warrior or warship standing on the route seizes the goods
        int robber = -1, rx = 0, ry = 0;
        {
        OccScope occ(*this);  // the unit index is only valid until units appear or move, so only while we look
        for (int ti : r.path) {
            int x = ti % W, y = ti / W;
            bool water = r.sea;  // boats rob boats, also on rivers
            forUnitsAt(x, y, [&](const Unit& e) {
                if (robber >= 0 || !g_units[e.type].military() || g_units[e.type].naval() != water || e.carrier >= 0) return;
                if (atWar(r.owner, e.owner)) { robber = e.owner; rx = x; ry = y; }
            });
            if (robber >= 0) break;
        }
        }
        if (robber >= 0) {
            int loot = sc(20) + routeYields(r, true).gold * 4;
            if (robber >= FIRST_PRIMATE_CIV) {
                if (hasGold(robber)) civs[robber].gold += loot;
                civs[robber].routesPlundered++;
                notify(robber, "We plundered a " + civs[r.owner].adjective + " caravan" + (hasGold(robber) ? " for " + std::to_string(loot) + " gold." : "."), rx, ry);
                civs[r.owner].grievance[robber] += 5;
            }
            endRoute(i, false, robber >= FIRST_PRIMATE_CIV ? "the " + civs[robber].name + " plundered the caravan" : "raiders plundered the caravan");
            continue;
        }
        if (turn >= r.until) endRoute(i, true, "");
    }
}

// ------------------------------------------------------------ AI
void Game::aiTrader(Unit& uref) {
    int uid = uref.id;
    auto U = [&]() -> Unit& { return units[uid]; };
    int c = U().owner;
    if (U().order == ORD_GOTO) {
        continueGoto(U());
        if (!U().alive || U().order == ORD_GOTO) return;
    }
    if (U().moves <= 0) return;
    auto value = [&](const RouteOption& o) {
        int v = o.origin.gold * 3 + o.origin.sci * 5 + o.origin.food * 3 + o.origin.prod * 3;
        int d = cities[o.city].owner;
        if (d != c) {
            v += opinion(c, d) / 8;  // happier to enrich friends
            if (civs[c].grievance[d] > 20) v -= 6;
            if (isMinor(d)) {  // caravans win a tribe's favour
                int inf = influenceFor(c, d);
                v += inf < TRIBE_ALLY + 10 ? 10 : 4;
                if (civs[d].questKind == TQ_ROUTE && !(c < (int)civs[d].questDone.size() && civs[d].questDone[c])) v += 15;
            }
        }
        return v;
    };
    // trade from here, or walk to a better market town nearby
    int bestV = -1000, bestFrom = -1, bestTo = -1;
    bool bestSea = false;
    const City* here = cityAt(U().x, U().y);
    std::vector<const City*> homes;
    if (here && here->owner == c) homes.push_back(here);
    for (auto& ci : cities)
        if (ci.alive && ci.owner == c && (!here || ci.id != here->id) && dist(ci.x, ci.y, U().x, U().y) <= 8) homes.push_back(&ci);
    std::sort(homes.begin() + (here && here->owner == c ? 1 : 0), homes.end(),
              [&](const City* a, const City* b) { return dist(a->x, a->y, U().x, U().y) < dist(b->x, b->y, U().x, U().y); });
    if (homes.size() > 3) homes.resize(3);
    for (const City* h : homes) {
        Unit probe = U();
        probe.x = h->x; probe.y = h->y;
        for (auto& o : tradeOptions(probe)) {
            if (!o.ok) continue;
            int v = value(o) - dist(h->x, h->y, U().x, U().y) * 2;
            if (v > bestV) { bestV = v; bestFrom = h->id; bestTo = o.city; bestSea = o.sea; }
        }
    }
    if (bestFrom < 0) {
        // nowhere to trade: wait at home
        if (!here) {
            for (auto& ci : cities)
                if (ci.alive && ci.owner == c) { U().order = ORD_GOTO; U().gx = ci.x; U().gy = ci.y; continueGoto(U()); return; }
        }
        U().order = ORD_SKIP;
        return;
    }
    const City& from = cities[bestFrom];
    if (here && here->id == bestFrom) { establishRoute(U(), bestTo, bestSea); return; }
    U().order = ORD_GOTO; U().gx = from.x; U().gy = from.y;
    continueGoto(U());
    if (U().alive && U().x == from.x && U().y == from.y && U().moves > 0) {
        U().order = ORD_NONE;
        establishRoute(U(), bestTo, bestSea);
    }
}
