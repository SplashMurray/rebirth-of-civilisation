// Headless simulation test: plays whole games with AI in every seat and checks invariants.
// Build: see CMakeLists.txt (target rebirth_sim). Usage: rebirth_sim [games] [mapSize]
// SPDX-License-Identifier: MIT
#include "../src/core/Game.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; std::printf("CHECK FAILED %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void invariants(Game& g) {
    for (auto& u : g.units) {
        if (!u.alive) continue;
        CHECK(g.inMap(u.x, u.y), "unit off map");
        CHECK(u.hp > 0 && u.hp <= 100, "bad hp %d", u.hp);
        CHECK(!g.at(u.x, u.y).mountain, "unit on mountain");
        if (g.isWater(u.x, u.y)) CHECK(g.canEmbark(u.owner, u.x, u.y) || u.owner < FIRST_PRIMATE_CIV, "unit in water without tech type=%s", g_units[u.type].name.c_str());
    }
    // stacking
    for (size_t i = 0; i < g.units.size(); i++) {
        auto& a = g.units[i];
        if (!a.alive) continue;
        for (size_t j = i + 1; j < g.units.size(); j++) {
            auto& b = g.units[j];
            if (!b.alive || a.x != b.x || a.y != b.y) continue;
            CHECK(a.owner == b.owner, "enemy units stacked %d %d at %d,%d", a.owner, b.owner, a.x, a.y);
            CHECK(g_units[a.type].military() != g_units[b.type].military(), "illegal stack %s + %s", g_units[a.type].name.c_str(), g_units[b.type].name.c_str());
        }
    }
    for (auto& c : g.cities) {
        if (!c.alive) continue;
        CHECK(c.pop >= 1, "pop < 1");
        CHECK(g.at(c.x, c.y).city == c.id, "city tile not owned");
        if (!g.hasGold(c.owner)) CHECK(c.yields.gold == 0, "gold before currency");
    }
    for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++)
        if (!g.hasGold(c)) CHECK(g.civs[c].gold == 0, "civ has gold before currency");
}

int main(int argc, char** argv) {
    int games = argc > 1 ? std::atoi(argv[1]) : 3;
    int size = argc > 2 ? std::atoi(argv[2]) : 1;
    int speed = argc > 3 ? std::atoi(argv[3]) : 1;
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // keep output if anything crashes
    initData();
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
        int firstCity = -1, currencyTurn = -1, deciphTurn = -1;
        while (!g.gameOver && g.turn <= g.maxTurns + 1) {
            g.endTurn();
            invariants(g);
            if (firstCity < 0 && g.numCities(g.player) > 0) firstCity = g.turn;
            if (currencyTurn < 0 && g.hasGold(g.player)) currencyTurn = g.turn;
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
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("== game %d over at turn %d: winner %s (%s). first city t%d, currency t%d, deciphering t%d. wars %d captures %d razes %d. %.1fs\n", gi, g.turn,
                    g.winner >= 0 ? g.civs[g.winner].name.c_str() : "-", g.victoryType.c_str(), firstCity, currencyTurn, deciphTurn, g.statWars, g.statCaptures, g.statRazes, secs);
    }
    std::printf("%s (%d failures)\n", fails ? "FAILED" : "OK", fails);
    return fails ? 1 : 0;
}
