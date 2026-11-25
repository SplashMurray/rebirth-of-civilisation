// Headless simulation test: plays whole games with AI in every seat and checks invariants.
// Build: see CMakeLists.txt (target rebirth_sim). Usage: rebirth_sim [games] [mapSize]
// SPDX-License-Identifier: MIT
#include "../src/core/Game.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>

static int fails = 0;
static long riverShips = 0;  // ship-turns spent on rivers
#define CHECK(cond, ...) do { if (!(cond)) { fails++; std::printf("CHECK FAILED %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void invariants(Game& g) {
    for (auto& u : g.units) {
        if (!u.alive) continue;
        CHECK(g.inMap(u.x, u.y), "unit off map");
        CHECK(u.hp > 0 && u.hp <= 100, "bad hp %d", u.hp);
        CHECK(!g.at(u.x, u.y).mountain, "unit on mountain");
        if (g_units[u.type].naval()) {
            CHECK(g.shipCanStand(u, u.x, u.y), "ship on land type=%s", g_units[u.type].name.c_str());
            if (!g.isWater(u.x, u.y) && !g.cityAt(u.x, u.y)) riverShips++;
            CHECK(g_units[u.type].ocean || g.at(u.x, u.y).terrain != T_OCEAN, "coastal ship on ocean type=%s", g_units[u.type].name.c_str());
        } else if (u.carrier >= 0) {
            CHECK(u.carrier < (int)g.units.size() && g.units[u.carrier].alive && g_units[g.units[u.carrier].type].naval() &&
                      g.units[u.carrier].x == u.x && g.units[u.carrier].y == u.y && g.units[u.carrier].owner == u.owner,
                  "cargo not with its ship type=%s", g_units[u.type].name.c_str());
        } else if (g.isWater(u.x, u.y)) CHECK(g.canEmbark(u.owner, u.x, u.y) || u.owner < FIRST_PRIMATE_CIV, "unit in water without tech type=%s", g_units[u.type].name.c_str());
    }
    // stacking
    for (size_t i = 0; i < g.units.size(); i++) {
        auto& a = g.units[i];
        if (!a.alive) continue;
        for (size_t j = i + 1; j < g.units.size(); j++) {
            auto& b = g.units[j];
            if (!b.alive || a.x != b.x || a.y != b.y) continue;
            if (a.owner == b.owner && (a.carrier >= 0 || b.carrier >= 0)) continue;  // aboard a ship
            CHECK(a.owner == b.owner, "enemy units stacked %d %d at %d,%d", a.owner, b.owner, a.x, a.y);
            auto layer = [](const UnitDef& d) { return d.civilian() ? 0 : d.naval() ? 2 : 1; };
            CHECK(layer(g_units[a.type]) != layer(g_units[b.type]), "illegal stack %s + %s", g_units[a.type].name.c_str(), g_units[b.type].name.c_str());
        }
    }
    for (auto& u : g.units)
        if (u.alive && g_units[u.type].naval()) CHECK(g.cargoCount(u) <= g_units[u.type].cargo, "ship overloaded %s", g_units[u.type].name.c_str());
    for (auto& c : g.cities) {
        if (!c.alive) continue;
        CHECK(c.pop >= 1, "pop < 1");
        CHECK(g.at(c.x, c.y).city == c.id, "city tile not owned");
        if (!g.hasGold(c.owner)) CHECK(c.yields.gold == 0, "gold before currency");
    }
    for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++)
        if (!g.hasGold(c)) CHECK(g.civs[c].gold == 0, "civ has gold before currency");
}

int unitIndex(const std::string& n);

// Rivers are navigable: river boats sail along them (faster downstream), big ships cannot, land units ford them.
static void riverTests() {
    Game g;
    GameSettings s;
    s.seed = 31;
    s.mapSize = 1;
    s.allAI = true;
    g.newGame(s);
    g.units.clear();
    int c = FIRST_PRIMATE_CIV;
    int galley = unitIndex("War Galley"), caravel = unitIndex("Caravel"), warband = unitIndex("Warband");
    int tested = 0;
    for (int i = 0; i < g.W * g.H && tested < 5; i++) {
        int ax = i % g.W, ay = i / g.W;
        const Tile& t = g.tiles[i];
        if (!g.hasRiver(ax, ay) || t.riverOut < 0 || g.isFalls(ax, ay) || t.site >= 0) continue;
        Pt b;
        if (!g.neighborDir(ax, ay, t.riverOut, b) || g.isFalls(b.x, b.y) || g.cityAt(b.x, b.y)) continue;
        g.units.clear();
        g.spawnUnit(c, galley, ax, ay);
        Unit boat = g.units.back();
        CHECK(g.moveCost(boat, ax, ay, b.x, b.y) == MP * 2 / 3, "downstream should cost 2/3 of a move");
        CHECK(g.moveCost(boat, b.x, b.y, ax, ay) == (g.isWater(b.x, b.y) && g.at(b.x, b.y).terrain == T_OCEAN ? -1 : MP) ||
                  g.isWater(b.x, b.y), "upstream should cost a move");
        g.spawnUnit(c, caravel, ax, ay);
        CHECK(g.moveCost(g.units.back(), b.x, b.y, ax, ay) < 0 || g.isWater(b.x, b.y), "ocean ship sailed up a river");
        // off the river's course onto plain land: no boat may go there
        for (int d = 0; d < 6; d++) {
            Pt n;
            if (!g.neighborDir(ax, ay, d, n) || g.isWater(n.x, n.y) || g.riverLink(ax, ay, n.x, n.y) || g.cityAt(n.x, n.y)) continue;
            CHECK(g.moveCost(boat, ax, ay, n.x, n.y) < 0, "boat left the river onto land");
            // a land unit fording into the river from the side pays extra, unless a road bridges it
            const Tile& nt = g.at(n.x, n.y);
            const Tile& at = g.at(ax, ay);
            if (!nt.mountain && !at.hills && at.veg == V_NONE && at.feature == F_NONE && at.road == 0 && nt.road == 0 && nt.site < 0 && at.owner < 0 && nt.owner < 0) {
                g.spawnUnit(c, warband, n.x, n.y);
                CHECK(g.moveCost(g.units.back(), n.x, n.y, ax, ay) == MP + MP / 2, "fording should cost half a move more (got %d)",
                      g.moveCost(g.units.back(), n.x, n.y, ax, ay));
            }
            break;
        }
        // a boat can find its way from the sea up to here
        int cur = i, guard = 0;
        while (guard++ < 400 && !g.isWater(cur % g.W, cur / g.W)) {
            Pt p;
            if (!g.neighborDir(cur % g.W, cur / g.W, g.tiles[cur].riverOut, p)) break;
            cur = g.idx(p.x, p.y);
        }
        bool fallsOnWay = false;
        for (int k = i, gd = 0; gd < 400 && k != cur; gd++) {
            if (g.isFalls(k % g.W, k / g.W)) fallsOnWay = true;
            Pt p;
            if (!g.neighborDir(k % g.W, k / g.W, g.tiles[k].riverOut, p)) break;
            k = g.idx(p.x, p.y);
        }
        if (g.isWater(cur % g.W, cur / g.W) && g.at(cur % g.W, cur / g.W).terrain == T_COAST && !fallsOnWay) {
            g.units.clear();
            g.spawnUnit(c, galley, cur % g.W, cur / g.W);
            auto path = g.findPath(g.units.back(), ax, ay);
            CHECK(!path.empty(), "no river path from the mouth at %d,%d up to %d,%d", cur % g.W, cur / g.W, ax, ay);
            tested++;
        }
    }
    CHECK(tested > 0, "no navigable river found to test");
    // waterfalls block boats
    for (auto& n : g.naturals)
        if (g_naturals[n.type].site == WS_FALLS)
            for (int d = 0; d < 6; d++) {
                Pt p;
                if (g.neighborDir(n.x, n.y, d, p)) CHECK(!g.waterStep(p.x, p.y, n.x, n.y, true, true), "boat sailed over a waterfall");
            }
    std::printf("river tests: %d river courses sailed\n", tested);
}

int main(int argc, char** argv) {
    int games = argc > 1 ? std::atoi(argv[1]) : 3;
    int size = argc > 2 ? std::atoi(argv[2]) : 1;
    int speed = argc > 3 ? std::atoi(argv[3]) : 1;
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // keep output if anything crashes
    initData();
    riverTests();
    for (int gi = 0; gi < games; gi++) {
        auto t0 = std::chrono::steady_clock::now();
        Game g;
        GameSettings s;
        s.seed = 1000 + gi * 77;
        s.mapSize = size;
        s.primate = gi % (int)g_primates.size();
        s.allAI = true;
        s.speed = speed;
        g.newGame(s);
        {
            int riv = 0, lake = 0, feat[F_COUNT] = {}, linked = 0;
            for (int i = 0; i < g.W * g.H; i++) {
                const Tile& t = g.tiles[i];
                if (g.hasRiver(i % g.W, i / g.W)) riv++;
                if (t.feature == F_LAKE) lake++;
                feat[t.feature]++;
                // river links are always mutual, and never run into a mountain
                for (int d = 0; d < 6; d++) {
                    if (!(t.river & (1 << d))) continue;
                    Pt p;
                    CHECK(g.neighborDir(i % g.W, i / g.W, d, p), "river runs off the map");
                    CHECK(g.at(p.x, p.y).river & (1 << ((d + 3) % 6)), "one-way river link");
                    CHECK(!g.at(p.x, p.y).mountain, "river into a mountain");
                    linked++;
                }
            }
            // every river reaches water
            for (int i = 0; i < g.W * g.H; i++) {
                if (!g.hasRiver(i % g.W, i / g.W)) continue;
                int cur = i, guard = 0;
                while (guard++ < 400 && !g.isWater(cur % g.W, cur / g.W)) {
                    int d = g.tiles[cur].riverOut;
                    Pt p;
                    if (d < 0 || !g.neighborDir(cur % g.W, cur / g.W, d, p)) break;
                    cur = g.idx(p.x, p.y);
                }
                CHECK(g.isWater(cur % g.W, cur / g.W), "river at %d,%d never reaches water", i % g.W, i / g.W);
            }
            std::printf("map: river tiles %d, lake tiles %d, floodplain %d marsh %d oasis %d volcano %d reef %d wonders %d\n", riv, lake, feat[F_FLOODPLAIN],
                        feat[F_MARSH], feat[F_OASIS], feat[F_VOLCANO], feat[F_REEF], (int)g.naturals.size());
        }
        int firstCity = -1, currencyTurn = -1, deciphTurn = -1, neoTurn = -1, speechTurn = -1, advTurn = -1;
        while (!g.gameOver && g.turn <= g.maxTurns + 1) {
            g.endTurn();
            invariants(g);
            if (firstCity < 0 && g.numCities(g.player) > 0) firstCity = g.turn;
            if (currencyTurn < 0 && g.hasGold(g.player)) currencyTurn = g.turn;
            if (neoTurn < 0 && g.hasTrait(g.player, TR_NEOCORTEX)) neoTurn = g.turn;
            if (speechTurn < 0 && g.hasTrait(g.player, TR_SPEECH)) speechTurn = g.turn;
            if (advTurn < 0 && g.hasTrait(g.player, TR_ADV_SPEECH)) advTurn = g.turn;
            if (deciphTurn < 0 && g.hasTech(g.player, ID.tDeciphering)) deciphTurn = g.turn;
            if (g.turn == PROLOGUE_TURNS + 1) CHECK(g.warHappened && !g.civs[CIV_HUMANS].alive, "war did not happen");
            if (g.turn % 50 == 0 || g.gameOver) {
                std::printf("game %d turn %3d %-28s |", gi, g.turn, g.dateString(g.turn).c_str());
                for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) {
                    auto& cv = g.civs[c];
                    int techs = 0, pop = 0, units = 0;
                    for (auto t : cv.techs) techs += t;
                    for (auto& ci : g.cities) if (ci.alive && ci.owner == c) pop += ci.pop;
                    for (auto& u : g.units) if (u.alive && u.owner == c) units++;
                    std::printf(" %.4s:%s c%d p%d t%d u%d r%d", cv.adjective.c_str(), cv.alive ? "" : "X", g.numCities(c), pop, techs, units, cv.ruinsExcavated);
                }
                int ferals = 0;
                for (auto& u : g.units) if (u.alive && u.owner == CIV_FERAL) ferals++;
                std::printf(" | ferals %d\n", ferals);
            }
            if (g.turn == 120 && gi == 0) {
                std::string path = (std::filesystem::temp_directory_path() / "roc_sim_test.roc").string();
                CHECK(g.save(path), "save failed: %s", path.c_str());
                Game h;
                bool loaded = h.load(path);
                CHECK(loaded, "load failed: %s", path.c_str());
                if (loaded) {
                    CHECK(h.turn == g.turn && h.units.size() == g.units.size() && h.cities.size() == g.cities.size(), "roundtrip mismatch");
                    CHECK(h.civs.size() == g.civs.size() && h.civs[3].techs == g.civs[3].techs && h.tiles.size() == g.tiles.size(), "roundtrip mismatch 2");
                    CHECK(h.rng.s == g.rng.s, "rng mismatch");
                    CHECK(h.civs[3].traits == g.civs[3].traits && h.civs[3].evoPool == g.civs[3].evoPool, "evolution not saved");
                }
                std::fflush(stdout);
            }
        }
        {
            int moods[4] = {}, lux = 0, nciv = 0, pop = 0, ncity = 0, wear = 0;
            for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) {
                if (!g.civs[c].alive) continue;
                nciv++;
                lux += g.luxuryCount(c);
                wear += g.civs[c].weariness;
            }
            for (auto& ci : g.cities)
                if (ci.alive && ci.owner >= FIRST_PRIMATE_CIV) {
                    moods[g.moodOf(ci) + 2]++;
                    pop += ci.pop;
                    ncity++;
                    CHECK(ci.happy >= 0 && ci.unhappy >= 0, "negative mood parts");
                }
            std::printf("mood: unrest %d unhappy %d content %d celebrating %d; avg luxuries %.1f, avg city size %.1f, avg weariness %.0f\n", moods[0], moods[1],
                        moods[2], moods[3], nciv ? (double)lux / nciv : 0, ncity ? (double)pop / ncity : 0, nciv ? (double)wear / nciv : 0);
        }
        int sailed = 0;
        for (auto& cv : g.civs) sailed += cv.riverSailed;
        std::printf("river: ship-turns on rivers %ld, river tiles sailed %d\n", riverShips, sailed);
        riverShips = 0;
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("== game %d over at turn %d: winner %s (%s). first city t%d (neocortex t%d, speech t%d, adv speech t%d), currency t%d, deciphering t%d. wars %d captures %d razes %d. %.1fs\n", gi, g.turn,
                    g.winner >= 0 ? g.civs[g.winner].name.c_str() : "-", g.victoryType.c_str(), firstCity, neoTurn, speechTurn, advTurn, currencyTurn, deciphTurn, g.statWars, g.statCaptures, g.statRazes, secs);
    }
    std::printf("%s (%d failures)\n", fails ? "FAILED" : "OK", fails);
    return fails ? 1 : 0;
}
