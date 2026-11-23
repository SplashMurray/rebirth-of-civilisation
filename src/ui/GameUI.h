// Rebirth of Civilisation - user interface (screens, map view, panels, input)
// SPDX-License-Identifier: MIT
#pragma once
#include "Canvas.h"
#include "../core/Game.h"
#include <functional>
#include <memory>

class GameUI {
public:
    GameUI();
    // ---- platform interface
    void render(Canvas& c);
    void mouseDown(int x, int y, int button);
    void mouseUp(int x, int y, int button);
    void mouseMove(int x, int y);
    void wheel(int x, int y, int delta);           // +1 = away from user (zoom in / scroll up)
    void key(int k, bool shift, bool ctrl);
    bool tick(int ms);                             // returns true if a redraw is needed
    bool wantsQuit() const { return quit; }
    std::string savePath = "rebirth_save.roc";
    // Interface size as a percentage on top of the system display scaling. The platform layer
    // reads this after each input event and rescales the canvas when it changes.
    int uiScalePct = 100;
    static constexpr int UI_SCALE_MIN = 60, UI_SCALE_MAX = 200, UI_SCALE_STEP = 10;
    void adjustUiScale(int dir);  // dir: +1 bigger, -1 smaller, 0 reset

    // ---- used by tests / screenshot tool
    enum Screen { SCR_TITLE, SCR_SETUP, SCR_GAME };
    enum Overlay { OV_NONE, OV_CITY, OV_TECH, OV_POLICY, OV_DIPLO, OV_MENU, OV_HELP, OV_LOG, OV_EVO };
    Screen screen = SCR_TITLE;
    Overlay overlay = OV_NONE;
    std::unique_ptr<Game> game;
    int selUnit = -1;
    int selCity = -1;
    void startGame(const GameSettings& s);
    void centerOn(int tx, int ty);
    void selectUnit(int id, bool center);
    void endTurnNow();
    int hoverX = -1, hoverY = -1;  // hovered tile
    int mouseX = 0, mouseY = 0;
    int nukeT = -1;                // nuclear war animation timer (ms), -1 = off
    void renderUnitGallery(Canvas& c);  // test helper: every unit flag at large size

private:
    // ---- hotspots registered while rendering
    struct Hot {
        int x, y, w, h;
        std::function<void()> fn;
        std::string tip;
        bool wheelArea;
    };
    std::vector<Hot> hots;
    bool quit = false;
    int W = 1280, H = 720;
    Canvas* cv = nullptr;

    // ---- setup choices
    int setupPrimate = 0, setupSize = 1, setupDiff = 1, setupSpeed = 1;

    // ---- map camera
    double camX = 0, camY = 0;
    int hs = 34;  // hex radius in px
    bool dragging = false, dragMoved = false, minimapDrag = false;
    int dragSX = 0, dragSY = 0;
    double dragCamX = 0, dragCamY = 0;
    bool moveMode = false;
    int animMs = 0;
    int listScroll = 0, techScroll = 0, logScroll = 0, helpScroll = 0;
    int disbandArm = -1;
    std::string toast;
    int toastMs = 0;
    int mmX = 0, mmY = 0, mmW = 0, mmH = 0;  // minimap rect
    double mmScale = 1;

    // ---- helpers: drawing
    void hot(int x, int y, int w, int h, std::function<void()> fn, const std::string& tip = "");
    bool over(int x, int y, int w, int h) const { return mouseX >= x && mouseY >= y && mouseX < x + w && mouseY < y + h; }
    void button(int x, int y, int w, int h, const std::string& label, bool enabled, std::function<void()> fn,
                const std::string& tip = "", Color base = rgb(46, 56, 72), int size = 14);
    void panel(int x, int y, int w, int h, Color fill = rgb(28, 34, 44), Color border = rgb(88, 102, 124));
    std::vector<std::string> wrap(const std::string& s, int width, int size, bool bold = false);
    int textBlock(int x, int y, int w, const std::string& s, int size, Color c, int lineGap = 4, bool bold = false);
    void bar(int x, int y, int w, int h, double frac, Color fill, Color back = rgb(20, 24, 30));
    void tooltip();
    void drawTrefoil(int cx, int cy, int r);
    void hexCorners(int cx, int cy, int r, int* xy) const;

    // ---- screens
    void renderTitle();
    void renderSetup();
    void renderGame();
    void renderTopBar();
    void renderUnitPanel();
    void renderTileInfo();
    void renderEndTurn();
    void renderMinimap();
    void renderNotifications();
    void renderCity();
    void renderTech();
    void renderPolicies();
    void renderDiplomacy();
    void renderMenu();
    void uiScaleControl(int x, int y, int w);
    void renderHelp();
    void renderLog();
    void renderEvolution();
    std::string pressureTip(int c);
    void renderPopup();
    void renderNuke();
    void renderToast();

    // ---- map (UIMap.cpp)
    void renderMap();
    void drawTile(int x, int y, int sx, int sy, bool visible);
    void drawHumanSite(int x, int y, int sx, int sy, bool visible);
    void drawCityOnMap(const City& c, int sx, int sy);
    void drawCityBanner(const City& c, int sx, int sy);
    void drawUnitToken(const Unit& u, int sx, int sy, int r);
    void drawUnitIcon(int type, int cx, int cy, int s, Color ic, Color accent);
    void drawPathPreview();
    void tileCenter(int tx, int ty, int& sx, int& sy) const;
    bool pickTile(int sx, int sy, int& tx, int& ty) const;
    void clampCamera();

    // ---- actions
    void clickMap(int tx, int ty);
    void rightClickMap(int tx, int ty);
    void afterUnitAction();
    int nextUnitNeedingOrders(int after) const;
    void unitCommand(char cmd);
    void openCity(int id);
    std::string endTurnPrompt(int& action) const;
    void endTurnButton();
    void saveGame();
    void loadGame();
    void closePopup();
    Unit* sel();
    Color civColor(int c) const;
    std::string yieldsStr(const Yields& y) const;
    std::string combatPreview(const Unit& u, int tx, int ty);
};
