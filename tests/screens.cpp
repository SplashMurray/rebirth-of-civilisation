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
    // found the capital and look at the city screen
    ui.key('B', false, false);
    shot(ui, "05_city_screen");
    ui.key(KEY_ESC, false, false);
    autoplay(ui, 5);
    // trigger the Fall
    ui.game->aiTurn(ui.game->player);
    ui.endTurnNow();
    ui.nukeT = 1700;
    shot(ui, "06_nuke_anim");
    ui.nukeT = -1;
    shot(ui, "07_fall_popup");
    ui.game->popups.clear();
    autoplay(ui, 110);
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
