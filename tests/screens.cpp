// Renders a set of UI screenshots to SVG (convert with rsvg-convert). Used to check the interface on any OS.
// Usage: rebirth_screens <outdir>
// SPDX-License-Identifier: MIT
#include "SvgCanvas.h"
#include "../src/ui/GameUI.h"
#include <cstdio>

static std::string outDir = ".";
static void shot(GameUI& ui, const char* name, int w = 1600, int h = 900) {
    SvgCanvas c(w, h);
    ui.render(c);
    c.save(outDir + "/" + name + ".svg");
    std::printf("wrote %s\n", name);
}

// let the AI play the human's seat for a while
static void autoplay(GameUI& ui, int turns) {
    Game& g = *ui.game;
    for (int i = 0; i < turns && !g.gameOver; i++) {
        g.popups.clear();
        g.aiTurn(g.player);
        if (g.civs[g.player].evoGoal < 0) g.setEvoGoal(g.player, g.aiChooseTrait(g.player));
        ui.endTurnNow();
        ui.nukeT = -1;
    }
    g.popups.clear();
}

int main(int argc, char** argv) {
    if (argc > 1) outDir = argv[1];
    GameUI ui;
    SvgCanvas warm(1600, 900);
    ui.render(warm);
    shot(ui, "01_title");
    ui.screen = GameUI::SCR_SETUP;
    ui.mouseMove(700, 300);
    shot(ui, "02_setup");
    shot(ui, "02b_setup_small", 1280, 720);
    GameSettings s;
    s.primate = 0;
    s.seed = 42;
    s.mapSize = 1;
    ui.startGame(s);
    shot(ui, "03_intro_popup");
    ui.game->popups.clear();
    ui.render(warm);
    ui.mouseMove(800, 470);
    shot(ui, "04_start_map");
    ui.overlay = GameUI::OV_EVO;
    shot(ui, "04b_evolution_start");
    ui.overlay = GameUI::OV_TECH;
    shot(ui, "04c_tech_start");
    ui.overlay = GameUI::OV_NONE;
    // the roaming band, with its foraging overlay
    shot(ui, "05_roaming_band");
    autoplay(ui, 5);
    // trigger the Fall
    ui.game->aiTurn(ui.game->player);
    ui.endTurnNow();
    ui.nukeT = 1700;
    shot(ui, "06_nuke_anim");
    ui.nukeT = -1;
    shot(ui, "07_fall_popup");
    ui.game->popups.clear();
    // wander until the species settles down, and catch the moment
    int wandered = 0;
    {
        Game& g0 = *ui.game;
        while (wandered < 150 && g0.numCities(g0.player) == 0 && !g0.gameOver) {
            g0.popups.clear();
            g0.aiTurn(g0.player);
            if (g0.civs[g0.player].evoGoal < 0) g0.setEvoGoal(g0.player, g0.aiChooseTrait(g0.player));
            ui.endTurnNow();
            ui.nukeT = -1;
            wandered++;
        }
        std::vector<Popup> keep;
        for (auto& p : g0.popups) if (p.kind == POP_SETTLE) keep.push_back(p);
        g0.popups = keep;
        if (!keep.empty()) ui.centerOn(keep[0].x, std::min(g0.H - 1, keep[0].y + 5));  // city above the popup
        ui.selUnit = -1;
        std::printf("settled after %d turns of wandering (turn %d)\n", wandered, g0.turn);
        shot(ui, "07b_first_settlement");
        g0.popups.clear();
    }
    autoplay(ui, std::max(10, 110 - wandered));
    Game& g = *ui.game;
    ui.selUnit = -1;
    if (g.civs[g.player].capital >= 0) ui.centerOn(g.cities[g.civs[g.player].capital].x, g.cities[g.civs[g.player].capital].y);
    ui.render(warm);
    ui.mouseMove(900, 500);
    shot(ui, "08_midgame_map");
    // find a gatherer to show the unit panel near ruins
    for (auto& u : g.units)
        if (u.alive && u.owner == g.player && g_units[u.type].cls == UnitClass::Worker) { ui.selectUnit(u.id, true); break; }
    ui.render(warm);
    shot(ui, "09_unit_panel");
    ui.overlay = GameUI::OV_TECH;
    shot(ui, "10_tech_tree");
    ui.overlay = GameUI::OV_POLICY;
    shot(ui, "11_traditions");
    ui.overlay = GameUI::OV_NONE;
    for (int d = 1; d < 8; d++) if (g.triggerDisaster(g.player, d)) break;  // a drought, if possible
    if (!g.popups.empty() && g.popups.front().x >= 0) ui.centerOn(g.popups.front().x, g.popups.front().y);
    shot(ui, "11c_disaster");
    g.popups.clear();
    ui.overlay = GameUI::OV_EVO;
    shot(ui, "11b_evolution_mid");
    ui.overlay = GameUI::OV_DIPLO;
    shot(ui, "12_diplomacy");
    ui.overlay = GameUI::OV_NONE;
    if (g.civs[g.player].capital >= 0) {
        ui.key(KEY_ESC, false, false);
        ui.key(KEY_ESC, false, false);
        ui.overlay = GameUI::OV_NONE;
        // open via click path
        const City& cap = g.cities[g.civs[g.player].capital];
        (void)cap;
    }
    for (auto& c : g.cities)
        if (c.alive && c.owner == g.player) {
            ui.selUnit = -1;
            ui.overlay = GameUI::OV_NONE;
            ui.game->popups.clear();
            // emulate opening
            ui.selCity = c.id;
            ui.overlay = GameUI::OV_CITY;
            ui.centerOn(c.x, c.y);
            break;
        }
    ui.render(warm);
    shot(ui, "13_city_midgame");
    ui.overlay = GameUI::OV_HELP;
    shot(ui, "14_help");
    ui.overlay = GameUI::OV_NONE;
    shot(ui, "15_small_window", 1280, 720);
    autoplay(ui, 110);
    ui.selUnit = -1;
    ui.overlay = GameUI::OV_NONE;
    if (g.civs[g.player].capital >= 0) ui.centerOn(g.cities[g.civs[g.player].capital].x, g.cities[g.civs[g.player].capital].y);
    ui.render(warm);
    ui.mouseMove(700, 420);
    shot(ui, "16_lategame");
    std::printf("turn %d, %s\n", g.turn, g.dateString(g.turn).c_str());
    return 0;
}
