// Rebirth of Civilisation - user interface (screens, map view, panels, input)
// SPDX-License-Identifier: MIT
#pragma once
#include "Canvas.h"
#include "Sound.h"
#include "../core/Game.h"
#include "../core/Achievements.h"
#include <functional>
#include <memory>
#include <unordered_map>

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
    bool animOn = true;
    int noteMask = 31, logMask = 31;  // notification categories shown on the map / in the log (saved in settings.ini)
    bool advisorTips = true;  // advisors speak up at the start of turns (saved in settings.ini)
    // ---- sound: the platform plugs in a backend; volumes 0..100 (saved in settings.ini)
    SoundSink silence;
    SoundSink* snd = &silence;
    int musicVol = 60, sfxVol = 80, ambVol = 60;
    void applyVolumes() { snd->setVolumes(musicVol, sfxVol, ambVol); }  // animated water, unit movement and combat effects (saved in settings.ini)
    static constexpr int UI_SCALE_MIN = 60, UI_SCALE_MAX = 200, UI_SCALE_STEP = 10;
    void adjustUiScale(int dir);  // dir: +1 bigger, -1 smaller, 0 reset

    // ---- used by tests / screenshot tool
    enum Screen { SCR_TITLE, SCR_SETUP, SCR_GAME };
    enum Overlay { OV_NONE, OV_CITY, OV_TECH, OV_POLICY, OV_DIPLO, OV_MENU, OV_HELP, OV_LOG, OV_EVO, OV_ACHIEVEMENTS, OV_TRADE, OV_ROUTES, OV_TRIBES, OV_ADVISORS, OV_EMPIRE, OV_RELIGION };
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
    int boxT = -1;                 // the blue box falling (ms), -1 = off
    bool boxPlayed = false;
    static const int BOX_LEN = 3600;
    void renderBlueBox();
    static constexpr int NUKE_LEN = 6000, NUKE_FADE = 1500;  // plays by itself, fading out over the last NUKE_FADE ms
    void renderUnitGallery(Canvas& c);  // test helper: every unit flag at large size

    // ---- achievements (kept in the player's profile, not in saved games)
    AchievementStore ach;
    std::string achievementsPath;  // empty: don't persist (tests)
    void loadAchievements() { if (!achievementsPath.empty()) ach.load(achievementsPath); }
    void pollAchievements();
    std::vector<std::string> achBanners;  // waiting to be shown
    int achBannerMs = 0;

    // ---- nuclear weapons
    bool nukeMode = false;          // choosing a target
    int nukeArmX = -1, nukeArmY = -1;  // target picked, waiting for confirmation
    int blastT = -1, blastX = -1, blastY = -1;  // detonation animation
    void launchPlayerNuke();

private:
    // ---- hotspots registered while rendering
    struct Hot {
        int x, y, w, h;
        std::function<void()> fn;
        std::string tip;
        bool wheelArea;
        bool silent = false;  // no click sound
    };
    std::vector<Hot> hots;
    bool quit = false;
    int W = 1280, H = 720;
    Canvas* cv = nullptr;

    // ---- setup choices
    int setupPlayers = 1, setupEditing = 0;  // hotseat: how many humans, and whose primate the cards are choosing
    int setupHot[4] = {0, 1, 2, 3};          // each human's primate (setupHot[0] mirrors setupPrimate)
    bool handover = false;                   // hotseat: the screen is covered until the next player is ready
    void renderHandover();
    void beginHandover();
    void endHandover();
    int setupPrimate = 0, setupSize = 1, setupDiff = 1, setupSpeed = 1, setupMapType = 0, setupSea = 1;

    // ---- map camera
    double camX = 0, camY = 0;
    int hs = 34;  // hex radius in px
    bool dragging = false, dragMoved = false, minimapDrag = false;
    int dragSX = 0, dragSY = 0;
    double dragCamX = 0, dragCamY = 0;
    bool moveMode = false;
    int animMs = 0;
    int listScroll = 0, techScroll = 0, logScroll = 0, helpScroll = 0, achScroll = 0;
    int achPollMs = 0;
    void renderAchievements();
    // diplomacy
    int diploSel = -1;   // civ shown on the diplomacy screen
    int warArm = -1;     // war declaration on a friend waiting for confirmation
    int tribeWarArm = -1;  // war on a tribe waiting for confirmation
    int tribeScroll = 0;
    int empireScroll = 0, empireSort = 1;  // empire overview: sort column + 1, negative = reversed
    Deal draft;          // the deal being negotiated on the trade screen
    void openTrade(int c, int preset = -1);
    void renderTrade();
    DealItem* draftItem(int kind, int from, int value = -1);
    void toggleDraft(int kind, int from, int value);
    void adjustDraftGold(int kind, int from, int delta);
    void balanceDraft();
    void renderAchBanner();
    void drawTrophy(int x, int y, int s, bool lit);
    void renderNukeTargeting();
    void renderBlast();
    // ---- sound (UISound.cpp)
    void sfx(Sfx s, float vol = 1.0f, int screenX = -1, int delayMs = 0);
    void sfxAt(Sfx s, int tx, int ty, float vol = 1.0f, int delayMs = 0);
    void selectSound(const Unit& u);
    void primateCall(int civ, int tx, int ty, float vol, int delayMs = 0);
    int callCooldownMs = 0;
    void moveSound(const Unit& u);
    void combatSound(const CombatFx& c);
    void buildSound(int imp);
    void updateSound(int ms);
    void volumeControl(int x, int y, int w, const std::string& label, int& v);
    int sndTechs = -1, sndTraits = -1, sndCities = -1, sndRoutes = -1, sndPolicies = -1, sndAch = 0, sndAmbMs = 0, sndNotifyMs = 0;
    int sndNuke = -1, sndBlast = -1, lastCombatTurn = -1000, moveSndMs = 0;
    const void* sndGame = nullptr;
    std::string sndPopup;
    // ---- caravans: the route chooser, roads and routes on the map
    int routeUnit = -1, routeHover = -1, routeScroll = 0;
    std::vector<RouteOption> routeOpts;
    void openRoutes(int unitId);
    void renderRoutes();
    void drawRoads(int x, int y, int sx, int sy);
    void drawTradeRoutes();
    std::string fmtMoves(int m) const;
    std::string tradeTip() const;
    std::string tradeYieldStr(const Yields& y) const;
    void skipNuke();
    // ---- unit motion and combat effects (interface only, never saved)
    struct Glide { int fx, fy, tx, ty, start, dur; };
    struct Fx { CombatFx c; int start; };
    std::vector<int> lastX, lastY;
    std::unordered_map<int, Glide> glides;
    std::vector<Fx> fxs;
    uint64_t fxSeen = 0;
    void resetMotion();
    void updateMotion();
    void unitScreenPos(const Unit& u, int& sx, int& sy) const;
    void renderFx();
    void startBlast(int x, int y);
    int disbandArm = -1;
    int promoteFor = -1;  // unit whose promotion chooser is open
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
    void drawTileGround(int x, int y, int sx, int sy);
    void drawPortrait(int primate, int cx, int cy, int r, Color ring);
    void drawTitleScene();
    void drawFireball(int sx, int sy, int r, double k, int alpha = 255);
    void drawMushroom(int sx, int sy, int big, int rise, int ms, int alpha = 255);
    Color groundColor(int x, int y) const;
    void edgeFade(int sx, int sy, const int* xy, int d, float depth, Color c, int a);
    void drawTerrainRegion(int x0, int x1, int y0, int y1);
    void drawLiveWater(int x0, int x1, int y0, int y1);
    // ---- tutorial (UITutorial.cpp)
    struct UIRect { int x, y, w, h; };
    std::unordered_map<std::string, UIRect> uiRects;  // where named controls were drawn, for the tutorial to point at
    void markRect(const std::string& id, int x, int y, int w, int h);
    struct TutInfo {
        std::string title, text, rect, nextLabel = "Next";
        int tileX = -1, tileY = -1;
        bool canNext = false, done = false;
    };
    TutInfo tutorialInfo();
    int tutorialSteps() const;
    void tutorialAdvance();
    void renderTutorial();
    void drawHighlight(int x, int y, int w, int h);
    int tutRefStep = -1, tutRefTurn = 0, tutRefX = -1, tutRefY = -1, tutRefCount = 0;
    bool setupTutorial = false;
    // ---- advisors (UIAdvisors.cpp)
    struct Advice {
        int who = 0, pri = 0;
        std::string text;
        std::function<void()> go;
        std::string goLabel;
    };
    std::vector<Advice> gatherAdvice();
    void renderAdvisors();
    void drawAdvisor(int who, int cx, int cy, int r);
    void updateAdvisorTip();
    void renderAdvisorTip();
    struct { Advice advice; bool active = false; } tip;
    int tipTurn = -1;
    std::unordered_map<std::string, int> tipShown;  // advice text -> turn last raised
    // ---- resources (UIResources.cpp)
    Color resourceKindColor(int kind) const;
    void drawResourceIcon(int res, int cx, int cy, int r, bool badge = true);
    void drawResourceScene(int x, int y, int sx, int sy);
    // ---- minor tribes
    void renderTribes();
    void renderEmpire();
    void renderReligion();
    void drawReligionSymbol(int r, int cx, int cy, int rad);
    Color religionColor(int r) const;
    std::string zealTip(int c);
    int relPickName = -1, relPickFollower = -1, relPickFounder = -1;  // Religion screen choices
    void renderPromote();
    void drawPromoBadge(int p, int cx, int cy, int r);
    std::string promoSummary(const Unit& u) const;
    void drawTribeBadge(int x, int y, int r, int tribeCiv);
    // ---- happiness
    Color moodColor(int mood) const;
    void drawMoodFace(int x, int y, int r, int mood);
    std::string moodTip(const City& c) const;
    // ---- rivers, lakes, terrain features and natural wonders (UIRivers.cpp)
    void drawRiver(int x, int y, int sx, int sy);
    void drawFeature(int x, int y, int sx, int sy);
    void drawVolcanoCone(int sx, int sy, bool great);
    void drawNatural(int x, int y, int sx, int sy);
    void drawNaturalLabels(int x0, int x1, int y0, int y1);
    void drawLiveFeatures(int x0, int x1, int y0, int y1);
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
