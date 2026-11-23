// Rebirth of Civilisation - screens, panels and input
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <cmath>
#include <fstream>
#include <ctime>
#include <cstdio>

namespace {
const int TOP = 36;
const Color C_TEXT = {234, 238, 242};
const Color C_DIM = {150, 160, 174};
const Color C_GOLD = {240, 200, 70};
const Color C_SCI = {100, 178, 255};
const Color C_CULT = {206, 130, 236};
const Color C_FOOD = {124, 206, 92};
const Color C_PROD = {236, 148, 64};
const Color C_RED = {232, 84, 70};
const Color C_PANEL = {28, 34, 44};
const Color C_PANEL2 = {40, 48, 62};
const Color C_BORDER = {88, 102, 124};

std::string fmtPct(int v) { return (v >= 0 ? "+" : "") + std::to_string(v); }
}  // namespace

GameUI::GameUI() { initData(); }

// ============================================================ small drawing helpers
void GameUI::hot(int x, int y, int w, int h, std::function<void()> fn, const std::string& tip) {
    hots.push_back({x, y, w, h, fn, tip, false});
}

void GameUI::panel(int x, int y, int w, int h, Color fill, Color border) {
    cv->fillRect(x, y, w, h, fill);
    cv->rect(x, y, w, h, border, 1);
    hot(x, y, w, h, nullptr);  // absorb clicks
}

void GameUI::button(int x, int y, int w, int h, const std::string& label, bool enabled, std::function<void()> fn,
                    const std::string& tip, Color base, int size) {
    bool hv = enabled && over(x, y, w, h);
    Color f = !enabled ? darken(base, 0.7f) : hv ? lighten(base, 0.18f) : base;
    cv->fillRect(x, y, w, h, f);
    cv->rect(x, y, w, h, hv ? rgb(220, 200, 140) : lighten(base, 0.25f), 1);
    cv->text(x + w / 2, y + (h - size) / 2 - 1, label, size, enabled ? C_TEXT : C_DIM, AL_CENTER, true);
    hot(x, y, w, h, enabled ? fn : std::function<void()>(), tip);
}

void GameUI::bar(int x, int y, int w, int h, double frac, Color fill, Color back) {
    frac = std::clamp(frac, 0.0, 1.0);
    cv->fillRect(x, y, w, h, back);
    cv->fillRect(x, y, (int)(w * frac), h, fill);
}

std::vector<std::string> GameUI::wrap(const std::string& s, int width, int size, bool bold) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        std::string para = s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        std::string line;
        size_t p = 0;
        while (p < para.size()) {
            size_t sp = para.find(' ', p);
            std::string word = para.substr(p, sp == std::string::npos ? std::string::npos : sp - p);
            std::string cand = line.empty() ? word : line + " " + word;
            if (!line.empty() && cv->textWidth(cand, size, bold) > width) {
                out.push_back(line);
                line = word;
            } else {
                line = cand;
            }
            if (sp == std::string::npos) break;
            p = sp + 1;
        }
        out.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

int GameUI::textBlock(int x, int y, int w, const std::string& s, int size, Color c, int gap, bool bold) {
    for (auto& l : wrap(s, w, size, bold)) {
        cv->text(x, y, l, size, c, AL_LEFT, bold);
        y += size + gap;
    }
    return y;
}

void GameUI::tooltip() {
    for (int i = (int)hots.size() - 1; i >= 0; i--) {
        const Hot& h = hots[i];
        if (!over(h.x, h.y, h.w, h.h)) continue;
        if (h.tip.empty()) return;  // topmost element has no tooltip
        auto lines = wrap(h.tip, 320, 13);
        int tw = 0;
        for (auto& l : lines) tw = std::max(tw, cv->textWidth(l, 13));
        int th = (int)lines.size() * 17 + 10;
        int x = std::min(mouseX + 16, W - tw - 24), y = mouseY + 20;
        if (y + th > H) y = mouseY - th - 6;
        cv->fillRect(x, y, tw + 16, th, rgb(16, 18, 24));
        cv->rect(x, y, tw + 16, th, rgb(200, 180, 120));
        int ly = y + 5;
        for (auto& l : lines) { cv->text(x + 8, ly, l, 13, C_TEXT); ly += 17; }
        return;
    }
}

Color GameUI::civColor(int c) const { return game->civs[c].color; }

Unit* GameUI::sel() {
    if (!game || selUnit < 0 || selUnit >= (int)game->units.size()) return nullptr;
    Unit& u = game->units[selUnit];
    return u.alive ? &u : nullptr;
}

std::string GameUI::yieldsStr(const Yields& y) const {
    std::string s = "Food " + std::to_string(y.food) + "  Prod " + std::to_string(y.prod);
    if (y.gold) s += "  Gold " + std::to_string(y.gold);
    if (y.sci) s += "  Sci " + std::to_string(y.sci);
    if (y.cult) s += "  Cult " + std::to_string(y.cult);
    return s;
}

// ============================================================ top level
void GameUI::render(Canvas& c) {
    cv = &c;
    W = c.width();
    H = c.height();
    hots.clear();
    switch (screen) {
        case SCR_TITLE: renderTitle(); break;
        case SCR_SETUP: renderSetup(); break;
        case SCR_GAME: renderGame(); break;
    }
    renderToast();
    renderAchBanner();
    tooltip();
}

bool GameUI::tick(int ms) {
    animMs += ms;
    bool redraw = false;
    if (toastMs > 0) { toastMs -= ms; redraw = true; }
    if (nukeT >= 0) {
        nukeT += ms;
        if (nukeT > 5200) nukeT = -1;
        redraw = true;
    }
    if (achBannerMs > 0) {
        achBannerMs -= ms;
        if (achBannerMs <= 0 && !achBanners.empty()) {
            achBanners.erase(achBanners.begin());
            if (!achBanners.empty()) achBannerMs = 4500;
        }
        redraw = true;
    }
    if (blastT >= 0) {
        blastT += ms;
        if (blastT > 3400) blastT = -1;
        redraw = true;
    }
    if (screen == SCR_GAME && game) {
        achPollMs += ms;
        if (achPollMs >= 400) {
            achPollMs = 0;
            size_t before = achBanners.size();
            pollAchievements();
            if (achBanners.size() != before) redraw = true;
        }
    }
    if (screen == SCR_GAME && (nukeMode || nukeArmX >= 0)) redraw = true;
    if (screen == SCR_GAME && selUnit >= 0 && overlay == OV_NONE) redraw = true;  // pulsing selection
    if (screen == SCR_TITLE) redraw = (animMs / 100) % 2 == 0 || redraw;
    return redraw;
}

void GameUI::renderToast() {
    if (toastMs <= 0 || toast.empty()) return;
    int tw = cv->textWidth(toast, 16, true) + 40;
    int x = (W - tw) / 2, y = TOP + 50;
    cv->fillRect(x, y, tw, 34, rgb(20, 24, 30));
    cv->rect(x, y, tw, 34, C_GOLD, 2);
    cv->text(W / 2, y + 8, toast, 16, C_TEXT, AL_CENTER, true);
}

// ============================================================ title
void GameUI::renderTitle() {
    // sunset sky
    for (int i = 0; i < 40; i++) {
        float t = i / 39.0f;
        Color c = t < 0.6f ? blend(rgb(28, 20, 48), rgb(196, 80, 46), t / 0.6f) : blend(rgb(196, 80, 46), rgb(250, 180, 80), (t - 0.6f) / 0.4f);
        cv->fillRect(0, H * i / 40, W, H / 40 + 2, c);
    }
    // distant mushroom cloud
    int mx = W * 7 / 10, my = H * 58 / 100;
    cv->fillRect(mx - 14, my - H / 5, 28, H / 5, rgb(150, 110, 100));
    for (int k = 0; k < 7; k++) {
        double a = k * 3.14159 / 6;
        cv->fillCircle(mx + (int)(std::cos(a) * 70) - 0, my - H / 5 - (int)(std::sin(a) * 26), 46, rgb(168, 120, 104));
    }
    cv->fillCircle(mx, my - H / 5 - 30, 52, rgb(186, 136, 112));
    cv->fillCircle(mx, my - H / 5 - 34, 30, rgb(230, 170, 110));
    // ruined skyline
    uint32_t h = 12345;
    for (int x = 0; x < W; x += 34) {
        h = h * 1103515245u + 12345u;
        int bh = 60 + (int)((h >> 16) % 140);
        cv->fillRect(x, H * 3 / 4 - bh, 30, bh + 10, rgb(40, 28, 34));
        if ((h >> 8) % 3 == 0) {
            int xy[6] = {x, H * 3 / 4 - bh, x + 15, H * 3 / 4 - bh - 18, x + 30, H * 3 / 4 - bh};
            cv->fillPoly(xy, 3, rgb(40, 28, 34));
        }
    }
    // jungle foreground
    cv->fillRect(0, H * 3 / 4, W, H / 4, rgb(14, 30, 20));
    for (int x = -20; x < W + 40; x += 46) {
        h = h * 1103515245u + 12345u;
        cv->fillCircle(x, H * 3 / 4 + (int)((h >> 16) % 30), 44 + (int)((h >> 20) % 20), rgb(18, 44 + (int)((h >> 12) % 20), 26));
    }
    // a primate silhouette on a branch
    int px = W / 5, py = H * 3 / 4 - 10;
    cv->line(px - 120, py + 40, px + 140, py + 10, rgb(10, 16, 12), 10);
    cv->fillCircle(px, py - 30, 26, rgb(10, 12, 10));
    cv->fillCircle(px + 6, py - 70, 17, rgb(10, 12, 10));
    cv->line(px - 14, py - 34, px - 34, py + 22, rgb(10, 12, 10), 9);
    cv->line(px + 18, py - 40, px + 46, py + 14, rgb(10, 12, 10), 9);
    cv->line(px - 6, py - 8, px - 8, py + 30, rgb(10, 12, 10), 10);
    cv->line(px + 10, py - 8, px + 20, py + 26, rgb(10, 12, 10), 10);

    int ts = std::min(72, W / 14);
    cv->text(W / 2 + 3, H / 6 + 3, "REBIRTH OF CIVILISATION", ts, rgb(20, 10, 10), AL_CENTER, true);
    cv->text(W / 2, H / 6, "REBIRTH OF CIVILISATION", ts, rgb(255, 236, 200), AL_CENTER, true);
    cv->text(W / 2, H / 6 + ts + 14, "2034. Humanity destroys itself. The primates inherit the Earth.", 20, rgb(255, 230, 210), AL_CENTER);
    int bw = 280, bx = (W - bw) / 2, by = H / 6 + ts + 70;
    button(bx, by, bw, 46, "New Game", true, [this] { screen = SCR_SETUP; }, "", rgb(70, 60, 50), 18);
    bool hasSave = (bool)std::ifstream(savePath);
    button(bx, by + 58, bw, 46, "Load Game", hasSave, [this] { loadGame(); }, hasSave ? "" : "No saved game found", rgb(70, 60, 50), 18);
    button(bx, by + 116, bw, 46, "How to Play", true, [this] { overlay = OV_HELP; helpScroll = 0; }, "", rgb(70, 60, 50), 18);
    button(bx, by + 174, bw, 46, "Achievements (" + std::to_string(ach.unlocked.size()) + "/" + std::to_string(g_achievements.size()) + ")", true,
           [this] { overlay = OV_ACHIEVEMENTS; achScroll = 0; }, "", rgb(70, 60, 50), 18);
    button(bx, by + 232, bw, 46, "Exit", true, [this] { quit = true; }, "", rgb(70, 60, 50), 18);
    uiScaleControl(bx, by + 290, bw);
    cv->text(W - 12, H - 22, "Open source - MIT licence", 12, rgb(170, 200, 170), AL_RIGHT);
    if (overlay == OV_HELP) renderHelp();
    if (overlay == OV_ACHIEVEMENTS) renderAchievements();
}

// ============================================================ setup
void GameUI::renderSetup() {
    cv->fillRect(0, 0, W, H, rgb(18, 22, 28));
    cv->text(W / 2, 24, "Choose Your Primate", 34, C_TEXT, AL_CENTER, true);
    cv->text(W / 2, 66, "Each primate has its own strengths. Humans are not playable - they will not survive 2034.", 15, C_DIM, AL_CENTER);
    int cols = 4, gap = 14;
    int cw = std::min(330, (W - 80 - gap * (cols - 1)) / cols), ch = std::min(236, (H - 100 - 176) / 2);
    int gx = (W - (cw * cols + gap * (cols - 1))) / 2, gy = 100;
    for (int i = 0; i < (int)g_primates.size(); i++) {
        const PrimateDef& p = g_primates[i];
        int x = gx + (i % cols) * (cw + gap), y = gy + (i / cols) * (ch + gap);
        bool selp = setupPrimate == i;
        cv->fillRect(x, y, cw, ch, selp ? rgb(44, 54, 70) : rgb(30, 36, 46));
        cv->fillRect(x, y, cw, 8, p.color);
        cv->rect(x, y, cw, ch, selp ? rgb(255, 230, 150) : C_BORDER, selp ? 3 : 1);
        cv->fillCircle(x + 30, y + 40, 18, p.color);
        cv->circle(x + 30, y + 40, 18, rgb(10, 10, 10), 2);
        cv->text(x + 58, y + 20, p.name, 19, C_TEXT, AL_LEFT, true);
        cv->text(x + 58, y + 44, p.leader, 13, C_DIM);
        cv->text(x + 12, y + 70, p.ability, 15, C_GOLD, AL_LEFT, true);
        int ty = textBlock(x + 12, y + 92, cw - 24, p.abilityDesc + (p.evoPct ? " Evolves +" + std::to_string(p.evoPct) + "% faster." : "") + " Leans towards the " + STRAND_NAMES[p.evoLean] + ".", 13, C_TEXT, 3);
        textBlock(x + 12, ty + 6, cw - 24, p.flavor, 12, C_DIM, 2);
        hot(x, y, cw, ch, [this, i] { setupPrimate = i; });
    }
    int oy = gy + 2 * (ch + gap) + 10;
    const char* sizes[] = {"Small (4 primates)", "Standard (6 primates)", "Large (8 primates)"};
    const char* diffs[] = {"Easy", "Normal", "Hard", "Expert"};
    static const char* DIFF_TIPS[4] = {
        "Easy: rivals play loosely - random research and traditions, few cities, timid and clumsy in war.",
        "Normal: rivals play sensibly but without long-term plans.",
        "Hard: rivals plan - they research what helps them most, choose traditions to suit, expand further, "
        "only start wars they can win, gather armies with siege engines before attacking, and make peace when losing.",
        "Expert: as Hard, but sharper - bigger empires, tighter decisions, opportunistic wars and a determined race to Rebirth.\n"
        "No difficulty gives the computer free yields or units: it simply plays better."};
    int beginX = W - 260 - gx + 20, rowW = std::min(600, beginX - 16 - (gx + 110));
    int s3 = rowW / 3, s4 = rowW / 4;  // button pitch for 3 and 4 choices
    cv->text(gx, oy + 8, "Map size:", 16, C_TEXT, AL_LEFT, true);
    for (int i = 0; i < 3; i++)
        button(gx + 110 + i * s3, oy, s3 - 10, 34, sizes[i], true, [this, i] { setupSize = i; }, "",
               setupSize == i ? rgb(110, 90, 40) : C_PANEL2, 14);
    cv->text(gx, oy + 52, "Difficulty:", 16, C_TEXT, AL_LEFT, true);
    for (int i = 0; i < 4; i++)
        button(gx + 110 + i * s4, oy + 44, s4 - 8, 34, diffs[i], true, [this, i] { setupDiff = i; }, DIFF_TIPS[i],
               setupDiff == i ? rgb(110, 90, 40) : C_PANEL2, 14);
    cv->text(gx, oy + 96, "Game speed:", 16, C_TEXT, AL_LEFT, true);
    static const char* SPEED_TIPS[3] = {"Quick: 330 turns. Everything costs less; a shorter game.",
                                        "Standard: 500 turns, like Civilization VI's standard speed.",
                                        "Epic: 750 turns. Everything costs more; a long, slow game."};
    for (int i = 0; i < 3; i++)
        button(gx + 110 + i * s3, oy + 88, s3 - 10, 34, std::string(SPEED_NAMES[i]) + " (" + std::to_string(SPEED_TURNS[i]) + " turns)", true,
               [this, i] { setupSpeed = i; }, SPEED_TIPS[i], setupSpeed == i ? rgb(110, 90, 40) : C_PANEL2, 14);
    button(beginX, oy + 4, 220, 70, "Begin", true, [this] {
        GameSettings s;
        s.primate = setupPrimate;
        s.mapSize = setupSize;
        s.difficulty = setupDiff;
        s.speed = setupSpeed;
        s.seed = (uint64_t)std::time(nullptr) ^ ((uint64_t)animMs << 20);
        startGame(s);
    }, "", rgb(60, 110, 60), 24);
    button(20, 20, 110, 34, "< Back", true, [this] { screen = SCR_TITLE; }, "Return to the title screen (Esc)");
}

void GameUI::startGame(const GameSettings& s) {
    game.reset(new Game());
    game->newGame(s);
    screen = SCR_GAME;
    overlay = OV_NONE;
    selUnit = selCity = -1;
    nukeT = -1;
    hs = 36;
    int first = nextUnitNeedingOrders(-1);
    for (auto& u : game->units)
        if (u.alive && u.owner == game->player && (g_units[u.type].cls == UnitClass::Settler || g_units[u.type].cls == UnitClass::Band)) first = u.id;
    if (first >= 0) {
        selectUnit(first, true);
    }
}

// ============================================================ game screen
void GameUI::renderGame() {
    Game& g = *game;
    renderMap();
    if (nukeT >= 0) {
        renderNuke();
        return;
    }
    if (overlay != OV_CITY) {
        renderNotifications();
        renderUnitPanel();
        renderTileInfo();
    }
    renderMinimap();
    renderEndTurn();
    switch (overlay) {
        case OV_CITY: renderCity(); break;
        case OV_TECH: renderTech(); break;
        case OV_POLICY: renderPolicies(); break;
        case OV_DIPLO: renderDiplomacy(); break;
        case OV_MENU: renderMenu(); break;
        case OV_HELP: renderHelp(); break;
        case OV_LOG: renderLog(); break;
        case OV_EVO: renderEvolution(); break;
        case OV_ACHIEVEMENTS: renderAchievements(); break;
        default: break;
    }
    renderTopBar();
    if (blastT >= 0) { renderBlast(); return; }
    if (nukeMode || nukeArmX >= 0) renderNukeTargeting();
    if (!g.popups.empty()) renderPopup();
}

void GameUI::renderTopBar() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    bool compact = W < 1500;
    cv->fillRect(0, 0, W, TOP, rgb(20, 24, 31));
    cv->line(0, TOP - 1, W, TOP - 1, rgb(110, 96, 60), 2);
    hot(0, 0, W, TOP, nullptr);
    // right side first: buttons and date
    int bx = W - 8;
    auto rb = [&](const std::string& l, std::function<void()> fn, const std::string& tip) {
        int w = cv->textWidth(l, 13, true) + (compact ? 12 : 18);
        bx -= w + 4;
        button(bx, 6, w, TOP - 12, l, true, fn, tip, C_PANEL2, 13);
    };
    rb("Menu", [this] { overlay = overlay == OV_MENU ? OV_NONE : OV_MENU; }, "Save, load, help (Esc)");
    rb("Log", [this] { overlay = overlay == OV_LOG ? OV_NONE : OV_LOG; logScroll = 0; }, "Event log (L)");
    rb(compact ? "Evo" : "Evolution", [this] { overlay = overlay == OV_EVO ? OV_NONE : OV_EVO; }, "Evolutionary traits and pressures (V)");
    rb(compact ? "Diplo" : "Diplomacy", [this] { overlay = overlay == OV_DIPLO ? OV_NONE : OV_DIPLO; }, "Other civilisations (D)");
    rb(compact ? "Trad" : "Traditions", [this] { overlay = overlay == OV_POLICY ? OV_NONE : OV_POLICY; }, "Adopt traditions with culture (P)");
    rb("Tech", [this] { overlay = overlay == OV_TECH ? OV_NONE : OV_TECH; }, "Technology tree (T)");
    std::string date = "Turn " + std::to_string(g.turn) + "  |  " + g.dateString(g.turn);
    if (compact) {
        date = "T" + std::to_string(g.turn) + " | ";
        date += g.shortDate();
    }
    int dw = cv->textWidth(date, 14, true);
    cv->text(bx - 12, 10, date, 14, g.warHappened ? C_TEXT : rgb(255, 150, 120), AL_RIGHT, true);
    hot(bx - 12 - dw, 0, dw, TOP, nullptr, g.dateString(g.turn) + ". " + g.calendarPhase() +
        ". Time passes quickly while species are still evolving, then more slowly as science advances.\n" + SPEED_NAMES[g.settings.speed] + " speed: the game ends at turn " +
        std::to_string(g.maxTurns) + " if nobody has won before then.");
    int limit = bx - 12 - dw - 14;
    // left side, skipping whatever no longer fits
    int x = 10, y = 10;
    auto item = [&](const std::string& s, Color c, const std::string& tip, std::function<void()> fn = nullptr) {
        int w = cv->textWidth(s, 14, true);
        if (x + w > limit) return;
        cv->text(x, y, s, 14, c, AL_LEFT, true);
        hot(x - 4, 0, w + 8, TOP, fn, tip);
        x += w + (compact ? 14 : 22);
    };
    cv->fillCircle(x + 8, TOP / 2, 9, me.color);
    cv->circle(x + 8, TOP / 2, 9, rgb(0, 0, 0), 1);
    x += 26;
    if (g.numCities(g.player) == 0 && g.numBands(g.player) > 0) {
        int nb = g.numBands(g.player), nm = g.bandMembers(g.player);
        std::string need = g.canSettle(g.player) ? "Your people can now settle: move a band to a good spot and press B."
                         : !g.hasTrait(g.player, TR_SPEECH) ? "To settle down your species must first evolve Speech (see Evolution), then discover Cultivation."
                                                             : "Discover Cultivation to settle down.";
        item((compact ? "Nomads " : "Nomads: ") + std::to_string(nb) + (nb == 1 ? " band, " : " bands, ") + std::to_string(nm), rgb(222, 190, 130),
             "Your people are wanderers: " + std::to_string(nb) + (nb == 1 ? " band" : " bands") + " with " + std::to_string(nm) +
                 " members in all. Bands forage the land around them; keep them moving to fresh ground.\n" + need,
             [this] { int n = -1; for (auto& u : game->units) if (u.alive && u.owner == game->player && g_units[u.type].cls == UnitClass::Band) { n = u.id; break; } if (n >= 0) selectUnit(n, true); });
    }
    if (g.civs[g.player].warheads > 0) {
        int n = g.civs[g.player].warheads;
        item((compact ? "Nukes " : "Nuclear warheads: ") + std::to_string(n), rgb(255, 120, 90),
             "You have " + std::to_string(n) + (n == 1 ? " nuclear warhead" : " nuclear warheads") +
                 " in your silos.\nClick to choose a target within 12 tiles of one of your cities.",
             [this] { nukeMode = true; nukeArmX = nukeArmY = -1; selUnit = -1; overlay = OV_NONE; });
    }
    if (g.hasTrait(g.player, TR_NEOCORTEX))
        item((compact ? "Sci +" : "Science +") + std::to_string(me.sciencePerTurn), C_SCI, "Science per turn. Science is used to research new technologies.",
             [this] { overlay = OV_TECH; });
    else
        item(compact ? "Sci: none" : "Science: none", rgb(110, 130, 160),
             "Your primates' brains cannot yet learn deliberately, so no science is earned. Evolve an Enlarged Neocortex first (see Evolution).",
             [this] { overlay = OV_EVO; });
    {
        std::string label, tip = pressureTip(g.player);
        Color col = rgb(130, 220, 150);
        if (me.evoGoal >= 0) {
            int tt = g.evolveTurns(g.player, me.evoGoal);
            label = (compact ? "" : "Evolving: ") + g_traits[me.evoGoal].name + (tt < 0 ? " (stalled)" : " (" + std::to_string(tt) + ")");
            if (tt < 0) col = rgb(240, 150, 110);
            tip = "Evolving towards " + g_traits[me.evoGoal].name + ": " + g_traits[me.evoGoal].effect + ".\n" + tip;
        } else {
            bool any = false;
            for (int t = 0; t < TR_COUNT; t++) if (g.canEvolve(g.player, t)) any = true;
            label = any ? "Choose Evolution" : "Fully evolved";
            col = any ? rgb(255, 190, 110) : rgb(130, 220, 150);
        }
        item(label, col, tip, [this] { overlay = OV_EVO; });
    }
    if (g.hasGold(g.player))
        item("Gold " + std::to_string(me.gold) + " (" + fmtPct(me.goldPerTurn) + ")", C_GOLD, "Gold in the treasury (per turn). Spend it to buy units and buildings in your cities.");
    else
        item(compact ? "Gold ?" : "Gold: unknown", rgb(130, 120, 90), "Your people do not yet understand money. Research Currency to unlock Gold income.");
    int pc = g.policyCost(g.player);
    bool can = false;
    for (size_t p = 0; p < g_policies.size(); p++) if (g.canAdopt(g.player, (int)p)) can = true;
    item((compact ? "Cult " : "Culture ") + std::to_string(me.culture) + "/" + std::to_string(pc) + " (+" + std::to_string(me.culturePerTurn) + ")", C_CULT,
         "Culture accumulates towards your next Tradition. Adopting every Tradition wins a Cultural victory.", [this] { overlay = OV_POLICY; });
    if (can) item(compact ? "TRADITION!" : "TRADITION READY", rgb(255, 160, 255), "Choose a new Tradition", [this] { overlay = OV_POLICY; });
    {
        std::string abbr, tip;
        for (int r = 1; r < R_COUNT; r++) {
            if (g_resources[r].kind != RK_STRATEGIC || !g.hasTech(g.player, g_resources[r].revealTechId)) continue;
            int n = g.resourceCount(g.player, r);
            if (n > 0) abbr += (abbr.empty() ? "" : " ") + g_resources[r].abbr;
            tip += (tip.empty() ? "" : ", ") + g_resources[r].name + " " + std::to_string(n);
        }
        if (!tip.empty())
            item(abbr.empty() ? "No resources" : abbr, rgb(230, 170, 120),
                 "Strategic resources inside your borders: " + tip +
                 ". Some units and buildings need them (Bronze Phalanx: Copper and Tin; riders: Horses; Iron Blades: Iron or Scrap Metal; Factory: Coal; tanks: Oil).");
    }
    // research, in whatever space is left
    std::string rs = me.research >= 0 ? g_techs[me.research].name + " (" + std::to_string(g.researchTurns(g.player)) + ")" : "Choose research";
    if (me.research < 0) {
        bool any = false;
        for (size_t t = 0; t < g_techs.size(); t++) if (g.canResearch(g.player, (int)t)) any = true;
        if (!any && !g.hasTrait(g.player, TR_NEOCORTEX)) rs = "No science until Enlarged Neocortex";
        else if (!any) {
            int need = !g.hasTrait(g.player, TR_SPEECH) ? TR_SPEECH : !g.hasTrait(g.player, TR_ADV_SPEECH) ? TR_ADV_SPEECH : TR_ABSTRACT;
            rs = "Awaiting " + g_traits[need].name + " (" + std::to_string(me.scienceBank) + " science stored)";
        }
    }
    double frac = me.research >= 0 ? (double)me.techProgress[me.research] / g.techCost(me.research) : 0;
    int rw = std::min(300, limit - x);
    if (rw >= 130) {
        int rx = std::clamp((W - rw) / 2, x, limit - rw);
        cv->fillRect(rx, 6, rw, TOP - 12, rgb(30, 40, 56));
        bar(rx, TOP - 10, rw, 4, frac, C_SCI);
        std::string shown = rs;
        while (shown.size() > 4 && cv->textWidth(shown, 14, true) > rw - 10) shown = shown.substr(0, shown.size() - 4) + "..";
        cv->text(rx + rw / 2, 9, shown, 14, me.research >= 0 ? C_TEXT : rgb(255, 200, 120), AL_CENTER, true);
        hot(rx, 6, rw, TOP - 12, [this] { overlay = OV_TECH; }, rs + ". Click to open the technology tree.");
    }
}

void GameUI::renderNotifications() {
    Game& g = *game;
    int y = TOP + 8, shown = 0;
    for (int i = (int)g.log.size() - 1; i >= 0 && shown < 7; i--) {
        const Notification& n = g.log[i];
        if (n.turn < g.turn - 1) break;
        auto lines = wrap(n.text, 290, 13);
        int h = (int)lines.size() * 16 + 8;
        int x = W - 318;
        Color bg = n.turn == g.turn ? rgb(34, 40, 52) : rgb(26, 30, 38);
        cv->fillRect(x, y, 310, h, bg);
        cv->fillRect(x, y, 4, h, n.text.find("war") != std::string::npos || n.text.find("attack") != std::string::npos ? C_RED : C_GOLD);
        int ly = y + 4;
        for (auto& l : lines) { cv->text(x + 10, ly, l, 13, n.turn == g.turn ? C_TEXT : C_DIM); ly += 16; }
        int nx = n.x, ny = n.y;
        hot(x, y, 310, h, nx >= 0 ? std::function<void()>([this, nx, ny] { centerOn(nx, ny); }) : std::function<void()>(), nx >= 0 ? "Click to view" : "");
        y += h + 4;
        shown++;
    }
}

std::string GameUI::combatPreview(const Unit& u, int tx, int ty) {
    Game& g = *game;
    const UnitDef& d = g_units[u.type];
    bool ranged = d.ranged();
    double A = g.unitStrength(u, true, ranged, u.x, u.y);
    double D;
    std::string target;
    if (const City* c = g.cityAt(tx, ty)) {
        D = g.cityStrength(*c);
        A *= d.cls == UnitClass::Siege ? 3.0 : 1.5;
        target = c->name;
    } else {
        Unit* m = g.militaryAt(tx, ty);
        if (!m) m = g.civilianAt(tx, ty);
        if (!m) return "";
        D = g.unitStrength(*m, false, false, tx, ty);
        target = g_units[m->type].name;
    }
    int deal = std::min(100, (int)(30 * std::pow(A / D, 1.25)));
    int take = ranged ? 0 : std::min(100, (int)(30 * std::pow(D / A, 1.25)));
    char buf[200];
    std::snprintf(buf, sizeof buf, "Attack %s: strength %.1f vs %.1f - deal ~%d, take ~%d", target.c_str(), A / 10, D / 10, deal, take);
    return buf;
}

void GameUI::renderTileInfo() {
    Game& g = *game;
    if (!g.inMap(hoverX, hoverY) || !g.civs[g.player].explored[g.idx(hoverX, hoverY)]) return;
    if (over(0, H - 190, 420, 190) || over(W - 260, H - 260, 260, 260)) return;
    const Tile& t = g.at(hoverX, hoverY);
    std::vector<std::pair<std::string, Color>> lines;
    std::string terr = t.mountain ? "Mountains" : g_terrain[t.terrain].name;
    if (t.hills) terr += ", Hills";
    if (t.veg == V_FOREST) terr += ", Forest";
    if (t.veg == V_JUNGLE) terr += ", Jungle";
    if (g.resourceVisible(g.player, hoverX, hoverY)) terr += ", " + g_resources[t.res].name;
    if (t.imp) terr += ", " + g_improvements[t.imp].name;
    lines.push_back({terr, C_TEXT});
    if (!t.mountain) lines.push_back({yieldsStr(g.tileYields(g.player, hoverX, hoverY)), C_DIM});
    if (g.resourceVisible(g.player, hoverX, hoverY)) {
        const ResourceDef& rd = g_resources[t.res];
        static const char* KIND[] = {"Bonus resource", "Strategic resource", "Luxury resource", "Crop"};
        lines.push_back({std::string(KIND[rd.kind]) + ": " + rd.name + (rd.hint.empty() ? "" : ". " + rd.hint), rgb(230, 190, 120)});
    }
    if (t.owner >= 0) {
        std::string o = g.civs[t.owner].name;
        if (t.city >= 0) o = g.cities[t.city].name + " (" + o + ")";
        lines.push_back({"Territory: " + o, civColor(t.owner)});
    }
    if (t.fallout) lines.push_back({"Fallout level " + std::to_string(t.fallout) + ": damages units, reduces yields", rgb(220, 220, 60)});
    if (t.site >= 0) {
        const HumanSite& s = g.sites[t.site];
        if (!g.warHappened) {
            std::string pop = s.population >= 1000 ? std::to_string(s.population / 1000) + "." + std::to_string(s.population % 1000 / 100) + " million" : std::to_string(s.population) + " thousand";
            lines.push_back({g_ruins[s.type].liveName + ": " + s.name + (s.population ? " - " + pop + " humans" : ""), rgb(220, 220, 240)});
        } else {
            lines.push_back({g_ruins[s.type].ruinName + ": " + s.name, rgb(230, 190, 140)});
            std::string tiers;
            for (int k = 0; k < RUIN_TIERS; k++) {
                if (!(g_ruins[s.type].tierMask & (1 << k))) continue;
                std::string st = (t.ruinDone & (1 << k)) ? "taken" : g.hasTech(g.player, g_ruinTierTech[k]) ? "ready" : std::string("needs ") + RUIN_TIER_TECH[k];
                tiers += std::string(tiers.empty() ? "" : ", ") + RUIN_TIER_NAME[k] + " (" + st + ")";
            }
            lines.push_back({"Salvage: " + tiers, rgb(255, 170, 80)});
        }
    }
    if (g.civs[g.player].visible[g.idx(hoverX, hoverY)]) {
        for (auto& u : g.units) {
            if (!u.alive || u.x != hoverX || u.y != hoverY) continue;
            const UnitDef& d = g_units[u.type];
            std::string s = g.civs[u.owner].adjective + " " + d.name + "  HP " + std::to_string(u.hp);
            if (d.military()) s += "  Str " + std::to_string(d.str) + (d.ranged() ? " / Rng " + std::to_string(d.rstr) : "");
            lines.push_back({s, civColor(u.owner)});
        }
        Unit* su = sel();
        if (su && su->owner == g.player && g_units[su->type].military() && g.isEnemyTarget(g.player, hoverX, hoverY)) {
            std::string cp = combatPreview(*su, hoverX, hoverY);
            if (!cp.empty()) lines.push_back({cp, rgb(255, 120, 100)});
        }
        if (const City* c = g.cityAt(hoverX, hoverY))
            lines.push_back({c->name + " - size " + std::to_string(c->pop) + ", defence " + std::to_string(g.cityStrength(*c) / 10) +
                             ", HP " + std::to_string(c->hp) + "/" + std::to_string(g.cityMaxHP(*c)), civColor(c->owner)});
    }
    int w = 470, h = (int)lines.size() * 18 + 12;
    int x = (W - w) / 2, y = H - h - 8;
    if (x < 440) x = 440;
    if (x + w > W - 270) w = std::max(260, W - 270 - x);
    cv->fillRect(x, y, w, h, rgb(20, 24, 31));
    cv->rect(x, y, w, h, C_BORDER);
    int ly = y + 6;
    for (auto& l : lines) {
        std::string s = l.first;
        while (s.size() > 4 && cv->textWidth(s, 13) > w - 16) s = s.substr(0, s.size() - 4) + "..";
        cv->text(x + 8, ly, s, 13, l.second);
        ly += 18;
    }
}

void GameUI::renderUnitPanel() {
    Game& g = *game;
    Unit* up = sel();
    if (!up || up->owner != g.player) return;
    Unit& u = *up;
    const UnitDef& d = g_units[u.type];
    int pw = 420, ph = 178, px = 0, py = H - ph;
    panel(px, py, pw, ph, rgb(22, 27, 35));
    cv->fillRect(px, py, pw, 4, civColor(u.owner));
    drawUnitToken(u, px + 32, py + 36, 20);
    cv->text(px + 62, py + 12, d.name, 19, C_TEXT, AL_LEFT, true);
    std::string stats;
    if (d.military()) stats = "Strength " + std::to_string(d.str) + (d.ranged() ? "   Ranged " + std::to_string(d.rstr) + " (range " + std::to_string(d.range) + ")" : "");
    else if (d.cls == UnitClass::Band) {
        int forage = g.bandForage(u);
        stats = "Band of " + std::to_string(u.band) + "   Forage " + std::to_string(forage) + " food, eats " + std::to_string(u.band) +
                " (" + (forage - u.band >= 0 ? "+" : "") + std::to_string(forage - u.band) + ")";
    } else stats = d.cls == UnitClass::Settler ? "Founds settlements" : "Builds improvements, excavates ruins";
    if (d.naval() && d.cargo > 0) {
        std::string aboard;
        for (auto& o : g.units) if (o.alive && o.carrier == u.id) aboard += std::string(aboard.empty() ? "" : ", ") + g_units[o.type].name;
        stats += "   Cargo " + std::to_string(g.cargoCount(u)) + "/" + std::to_string(d.cargo);
        if (!aboard.empty()) hot(px + 62, py + 32, 340, 20, nullptr, "Aboard: " + aboard);
    }
    if (u.carrier >= 0 && u.carrier < (int)g.units.size()) stats += "   Aboard " + g_units[g.units[u.carrier].type].name;
    cv->text(px + 62, py + 36, stats, 13, C_DIM);
    cv->text(px + 62, py + 54, "Moves " + std::to_string(u.moves) + "/" + std::to_string(g.maxMoves(u)) + "    HP " + std::to_string(u.hp) + "    " +
             orderName(u.order) + (u.order == ORD_BUILD || u.order == ORD_EXCAVATE || u.order == ORD_DECON ? " (" + std::to_string(u.work) + " turns)" : ""),
             13, u.moves > 0 ? C_TEXT : C_DIM);
    bar(px + 62, py + 72, 200, 5, u.hp / 100.0, u.hp > 50 ? rgb(90, 210, 90) : C_RED);
    if (d.cls == UnitClass::Band) {
        int need = g.bandGrowth(u.band);
        bool full = u.band >= Game::BAND_MAX;
        bar(px + 270, py + 72, 140, 5, full ? 1.0 : (double)u.bandFood / need, rgb(120, 200, 80));
        hot(px + 262, py + 62, 156, 20, nullptr, full ? "The band is as large as the land can feed (" + std::to_string(Game::BAND_MAX) + "). Split it to grow further."
                           : "Food stored " + std::to_string(u.bandFood) + "/" + std::to_string(need) + " towards the next member. "
                             "A band forages the best tiles around it, one per member plus one. Foraged land is picked over and recovers slowly - keep moving.");
    }
    // actions
    struct Act { std::string label; bool en; std::function<void()> fn; std::string tip; };
    std::vector<Act> acts;
    int uid = u.id;
    if (d.cls == UnitClass::Band) {
        std::string why;
        bool ok = g.canFoundAt(u.owner, u.x, u.y, &why);
        acts.push_back({"Settle Here (B)", ok && u.moves > 0, [this] { unitCommand('B'); },
                        ok ? "End the wandering: found a city of size " + std::to_string(g.settlePop(u)) + " here" : why});
        acts.push_back({"Split Band (G)", u.band >= 4 && u.moves > 0, [this] { unitCommand('G'); },
                        u.band >= 4 ? "Half the band leaves to forage on its own" : "The band needs at least 4 members to split"});
        acts.push_back({"War Party (R)", u.band >= 3 && u.moves > 0, [this] { unitCommand('R'); },
                        u.band >= 3 ? "Two members become a " + g_units[g.uniqueFor(u.owner, ID.uWarband)].name + " to guard the band" : "The band needs at least 3 members"});
        acts.push_back({"Send Scout", u.band >= 3 && u.moves > 0, [this, uid] { game->sendScout(game->units[uid]); afterUnitAction(); },
                        u.band >= 3 ? "One member becomes a " + g_units[g.uniqueFor(u.owner, ID.uScout)].name : "The band needs at least 3 members"});
    }
    if (d.cls == UnitClass::Settler) {
        std::string why;
        bool ok = g.canFoundAt(u.owner, u.x, u.y, &why);
        acts.push_back({"Found City (B)", ok && u.moves > 0, [this] { unitCommand('B'); }, ok ? "Found a new settlement here" : why});
    }
    if (d.cls == UnitClass::Worker) {
        for (int im : g.buildOptions(u)) {
            const ImprovementDef& idf = g_improvements[im];
            std::string tip = idf.name + ": ";
            if (idf.food) tip += "+" + std::to_string(idf.food) + " Food ";
            if (idf.prod) tip += "+" + std::to_string(idf.prod) + " Production ";
            if (idf.gold) tip += "+" + std::to_string(idf.gold) + " Gold ";
            tip += "(" + std::to_string(g.workTurns(u, idf.turns)) + " turns)";
            acts.push_back({idf.name, u.moves > 0, [this, uid, im] { game->startBuild(game->units[uid], im); afterUnitAction(); }, tip});
        }
        int tier = g.excavateTier(u);
        if (tier >= 0)
            acts.push_back({std::string("Excavate ") + RUIN_TIER_NAME[tier] + " (E)", u.moves > 0, [this] { unitCommand('E'); },
                            "Dig through the human ruins. The reward arrives when the work is done."});
        else if (g.at(u.x, u.y).ruin && g.warHappened) {
            std::string need;
            for (int k = 0; k < RUIN_TIERS; k++) {
                const Tile& t = g.at(u.x, u.y);
                if ((g_ruins[t.ruin].tierMask & (1 << k)) && !(t.ruinDone & (1 << k))) { need = RUIN_TIER_TECH[k]; break; }
            }
            acts.push_back({"Excavate", false, nullptr, need.empty() ? "Nothing left to salvage here" : "Research " + need + " to salvage more from these ruins"});
        }
        if (g.canDecon(u)) acts.push_back({"Decontaminate (X)", u.moves > 0, [this] { unitCommand('X'); }, "Reduce fallout on this tile by one level"});
    }
    if (d.military()) {
        int to = g.upgradeTarget(u);
        if (to >= 0) {
            std::string why;
            bool ok = g.canUpgrade(u, &why);
            std::string price = g.hasGold(u.owner) ? std::to_string(g.upgradeGold(u, to)) + " gold"
                                                    : std::to_string(g.upgradeProd(u, to)) + " production from the city it stands in";
            acts.push_back({"Upgrade: " + g_units[to].name + " (U)", ok, [this] { unitCommand('U'); },
                            ok ? "Refit as a " + g_units[to].name + " for " + price + ". Keeps its health; ends its turn."
                               : why + " (refit as a " + g_units[to].name + " for " + price + ")"});
        }
    }
    if (!d.naval() && d.cls != UnitClass::Nuke) {
        if (u.carrier >= 0 && !g.isWater(u.x, u.y))
            acts.push_back({"Leave Ship (K)", true, [this] { unitCommand('K'); }, "Step ashore in this harbour"});
        else if (u.carrier < 0 && !g.isWater(u.x, u.y) && g.transportAt(u.owner, u.x, u.y, u.id))
            acts.push_back({"Board Ship (K)", u.moves > 0, [this] { unitCommand('K'); }, "Go aboard the ship docked here. It will carry this unit wherever it sails."});
    }
    if (d.naval() && g.cargoCount(u) > 0)
        for (auto& o : g.units)
            if (o.alive && o.carrier == u.id) {
                int oid = o.id;
                acts.push_back({"Select " + g_units[o.type].name, true, [this, oid] { selectUnit(oid, false); },
                                "Select this unit aboard, then move it onto an adjacent land tile to land it"});
            }
    if (d.military()) acts.push_back({"Fortify (F)", u.order != ORD_FORTIFY, [this] { unitCommand('F'); }, "+25% defence after one turn. Heals while resting."});
    acts.push_back({"Sleep (S)", u.order != ORD_SLEEP, [this] { unitCommand('S'); }, "Stay put until woken or an enemy appears"});
    if (u.order != ORD_NONE) acts.push_back({"Wake (W)", true, [this] { unitCommand('W'); }, "Cancel current orders"});
    acts.push_back({moveMode ? "Pick target..." : "Move (M)", u.moves > 0, [this] { moveMode = !moveMode; }, "Then left-click a destination. Right-click also moves."});
    acts.push_back({"Skip (Space)", true, [this] { unitCommand(' '); }, "Do nothing this turn"});
    bool loaded = d.naval() && g.cargoCount(u) > 0;
    acts.push_back({disbandArm == uid ? "Confirm?" : "Disband", !loaded, [this, uid] {
        if (disbandArm == uid) { unitCommand('D'); }
        else disbandArm = uid;
    }, loaded ? "Land the units aboard first" : "Delete this unit"});
    int bx = px + 10, by = py + 86, bw = 98, bh = 26;
    for (size_t i = 0; i < acts.size() && i < 12; i++) {
        auto& a = acts[i];
        int w = a.label.size() > 13 ? bw * 2 + 4 : bw;
        if (bx + w > px + pw - 6) { bx = px + 10; by += bh + 4; }
        if (by + bh > H - 2) break;
        button(bx, by, w, bh, a.label, a.en, a.fn, a.tip, C_PANEL2, 12);
        bx += w + 4;
    }
}

void GameUI::renderMinimap() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    int maxW = 240, maxH = 150;
    double sc = std::min((double)maxW / (g.W + 0.5), (double)maxH / g.H);
    mmW = (int)(sc * (g.W + 0.5));
    mmH = (int)(sc * g.H);
    mmX = W - mmW - 10;
    mmY = H - mmH - 10;
    mmScale = sc;
    cv->fillRect(mmX - 4, mmY - 4, mmW + 8, mmH + 8, rgb(14, 16, 20));
    cv->rect(mmX - 4, mmY - 4, mmW + 8, mmH + 8, C_BORDER);
    int pw = std::max(1, (int)std::ceil(sc)), ph = std::max(1, (int)std::ceil(sc));
    for (int y = 0; y < g.H; y++)
        for (int x = 0; x < g.W; x++) {
            int i = g.idx(x, y);
            if (!me.explored[i]) continue;
            const Tile& t = g.tiles[i];
            Color c = t.owner >= 0 ? civColor(t.owner) : t.mountain ? rgb(120, 116, 110) : g_terrain[t.terrain].color;
            if (t.owner < 0 && t.fallout) c = blend(c, rgb(150, 168, 40), 0.4f);
            if (!me.visible[i]) c = darken(c, 0.6f);
            cv->fillRect(mmX + (int)((x + 0.5 * (y & 1)) * sc), mmY + (int)(y * sc), pw, ph, c);
        }
    for (auto& c : g.cities)
        if (c.alive && me.explored[g.idx(c.x, c.y)])
            cv->fillRect(mmX + (int)((c.x + 0.5 * (c.y & 1)) * sc) - 1, mmY + (int)(c.y * sc) - 1, 4, 4, rgb(255, 255, 255));
    // viewport
    const double SQ3 = 1.7320508075688772;
    double vx = camX / (hs * SQ3), vy = camY / (hs * 1.5), vw = W / (hs * SQ3), vh = H / (hs * 1.5);
    cv->clip(mmX - 2, mmY - 2, mmW + 4, mmH + 4);
    cv->rect(mmX + (int)(vx * sc), mmY + (int)(vy * sc), (int)(vw * sc), (int)(vh * sc), rgb(255, 255, 255));
    cv->unclip();
    hot(mmX, mmY, mmW, mmH, [this] {
        int tx = (int)((mouseX - mmX) / mmScale), ty = (int)((mouseY - mmY) / mmScale);
        centerOn(std::clamp(tx, 0, game->W - 1), std::clamp(ty, 0, game->H - 1));
    }, "");
}

std::string GameUI::endTurnPrompt(int& action) const {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    action = 0;
    if (g.gameOver && g.winner >= 0) { /* continue playing */ }
    if (me.research < 0) {
        for (size_t t = 0; t < g_techs.size(); t++)
            if (g.canResearch(g.player, (int)t)) { action = 1; return "Choose Research"; }
    }
    if (me.evoGoal < 0)
        for (int t = 0; t < TR_COUNT; t++)
            if (g.canEvolve(g.player, t)) { action = 5; return "Choose Evolution"; }
    for (size_t p = 0; p < g_policies.size(); p++)
        if (g.canAdopt(g.player, (int)p)) { action = 2; return "Choose Tradition"; }
    for (auto& c : g.cities)
        if (c.alive && c.owner == g.player && c.buildKind == BK_NONE) { action = 3; return "Choose Production"; }
    if (nextUnitNeedingOrders(-1) >= 0) { action = 4; return "Unit Needs Orders"; }
    return "Next Turn";
}

void GameUI::endTurnButton() {
    int action;
    endTurnPrompt(action);
    Game& g = *game;
    switch (action) {
        case 1: overlay = OV_TECH; break;
        case 2: overlay = OV_POLICY; break;
        case 5: overlay = OV_EVO; break;
        case 3:
            for (auto& c : g.cities)
                if (c.alive && c.owner == g.player && c.buildKind == BK_NONE) { openCity(c.id); break; }
            break;
        case 4: selectUnit(nextUnitNeedingOrders(selUnit), true); break;
        default: endTurnNow(); break;
    }
}

void GameUI::renderEndTurn() {
    int action;
    std::string label = endTurnPrompt(action);
    int bw = mmW + 8, bh = 54;
    int x = mmX - 4, y = mmY - 4 - bh - 8;
    Color c = action == 0 ? rgb(52, 112, 64) : rgb(132, 96, 36);
    button(x, y, bw, bh, label, true, [this] { endTurnButton(); },
           action == 0 ? "End your turn (Enter)" : "Click to resolve. Press Enter to end the turn anyway.", c, 18);
}

// ============================================================ city screen
void GameUI::openCity(int id) {
    selCity = id;
    overlay = OV_CITY;
    listScroll = 0;
    selUnit = -1;
    const City& c = game->cities[id];
    // place the city to the right of the panel
    centerOn(c.x, c.y);
    camX -= 210;
}

void GameUI::renderCity() {
    Game& g = *game;
    if (selCity < 0 || !g.cities[selCity].alive || g.cities[selCity].owner != g.player) { overlay = OV_NONE; return; }
    City& c = g.cities[selCity];
    g.computeCity(c);
    int pw = 440, px = 0, py = TOP, ph = H - TOP;
    panel(px, py, pw, ph, rgb(22, 27, 35));
    cv->fillRect(px, py, pw, 6, civColor(c.owner));
    cv->text(px + 16, py + 14, (c.capital ? "* " : "") + c.name, 26, C_TEXT, AL_LEFT, true);
    cv->text(px + 16, py + 46, "Population " + std::to_string(c.pop) + (c.capital ? "  -  Capital" : "") + "    Defence " +
             std::to_string(g.cityStrength(c) / 10) + "    HP " + std::to_string(c.hp) + "/" + std::to_string(g.cityMaxHP(c)), 13, C_DIM);
    button(pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; }, "Close (Esc)");
    // prev/next city
    std::vector<int> mine;
    for (auto& o : g.cities) if (o.alive && o.owner == g.player) mine.push_back(o.id);
    if (mine.size() > 1) {
        int pos = 0;
        for (size_t i = 0; i < mine.size(); i++) if (mine[i] == c.id) pos = (int)i;
        int prev = mine[(pos + mine.size() - 1) % mine.size()], next = mine[(pos + 1) % mine.size()];
        button(pw - 106, py + 12, 28, 28, "<", true, [this, prev] { openCity(prev); }, "Previous city");
        button(pw - 74, py + 12, 28, 28, ">", true, [this, next] { openCity(next); }, "Next city");
    }
    int y = py + 70;
    struct YI { const char* n; int v; Color c; };
    YI ys[] = {{"Food", c.yields.food, C_FOOD}, {"Production", c.yields.prod, C_PROD}, {"Science", c.yields.sci, C_SCI},
               {"Culture", c.yields.cult, C_CULT}, {"Gold", c.yields.gold, C_GOLD}};
    int x = px + 16;
    for (auto& yi : ys) {
        if (std::string(yi.n) == "Gold" && !g.hasGold(g.player)) continue;
        cv->text(x, y, std::to_string(yi.v), 20, yi.c, AL_LEFT, true);
        cv->text(x, y + 22, yi.n, 11, C_DIM);
        x += 82;
    }
    y += 46;
    int need = g.growthThreshold(c.pop);
    int tg = g.turnsToGrow(c);
    cv->text(px + 16, y, "Growth: " + std::to_string(c.food) + "/" + std::to_string(need) + " food (" + fmtPct(c.foodSurplus) + "/turn)" +
             (tg > 0 ? " - grows in " + std::to_string(tg) + " turns" : c.foodSurplus < 0 ? " - STARVING" : " - stagnant"), 13, C_FOOD);
    bar(px + 16, y + 18, pw - 32, 6, (double)c.food / need, C_FOOD);
    y += 32;
    if (c.droughtTurns > 0) {
        cv->text(px + 16, y - 4, "DROUGHT: -33% Food for " + std::to_string(c.droughtTurns) + " more turns", 13, rgb(240, 160, 80), AL_LEFT, true);
        y += 16;
    }
    // current production
    cv->fillRect(px + 10, y, pw - 20, 64, rgb(32, 38, 50));
    if (c.buildKind != BK_NONE) {
        std::string nm = c.buildKind == BK_UNIT ? g_units[c.buildId].name : g_buildings[c.buildId].name;
        int cost = g.buildCost(c.owner, c.buildKind, c.buildId);
        cv->text(px + 20, y + 6, "Producing: " + nm, 15, C_TEXT, AL_LEFT, true);
        cv->text(px + 20, y + 26, std::to_string(std::min(c.prod, cost)) + "/" + std::to_string(cost) + "  -  " + std::to_string(g.turnsToBuild(c)) + " turns", 13, C_DIM);
        bar(px + 20, y + 46, pw - 160, 8, (double)c.prod / cost, C_PROD);
        int bc = g.buyCost(c);
        if (g.hasGold(g.player) && bc > 0) {
            bool afford = g.civs[g.player].gold >= bc;
            button(pw - 126, y + 14, 106, 36, "Buy " + std::to_string(bc) + "g", afford, [this] { City& cc = game->cities[selCity]; game->buy(cc); },
                   "Complete this item at the end of the turn for gold.", rgb(110, 90, 30), 13);
        }
    } else {
        cv->text(px + 20, y + 10, "Choose something to produce", 16, rgb(255, 190, 110), AL_LEFT, true);
        cv->text(px + 20, y + 34, std::to_string(c.prod) + " production stored", 13, C_DIM);
    }
    y += 72;
    // production list
    struct Row { int kind, id; std::string name, info, tip; };
    std::vector<Row> rows;
    for (int u : g.availableUnits(g.player)) {
        if (!g.canBuild(c, BK_UNIT, u) && g_units[u].cls != UnitClass::Settler) continue;
        const UnitDef& d = g_units[u];
        std::string tip = d.name + ": " + d.desc;
        if (d.military()) tip += " Strength " + std::to_string(d.str) + (d.ranged() ? ", ranged " + std::to_string(d.rstr) : "") + ", moves " + std::to_string(d.moves) + ".";
        if (d.primate >= 0) tip += " (Unique unit)";
        if (!d.needs.empty()) tip += " Uses " + needsText(d.needs) + ".";
        rows.push_back({BK_UNIT, u, d.name, "", tip});
    }
    for (size_t u = 0; u < g_units.size(); u++) {  // known but missing strategic resources
        const UnitDef& d = g_units[u];
        if (!g.unitBelongsTo(g.player, (int)u) || !g.hasTech(g.player, d.techId) || g.hasNeeds(g.player, d.needs)) continue;
        rows.push_back({BK_UNIT, (int)u, d.name, "", d.name + ": " + d.desc + " Strength " + std::to_string(d.str) + ". Requires " + needsText(d.needs) +
                       " inside your borders."});
    }
    for (size_t b = 0; b < g_buildings.size(); b++) {
        const BuildingDef& bd = g_buildings[b];
        if (bd.needs.empty() || c.has[b] || !g.hasTech(g.player, bd.techId) || g.hasNeeds(g.player, bd.needs)) continue;
        rows.push_back({BK_BUILDING, (int)b, bd.name, "", bd.name + ": requires " + needsText(bd.needs) + " inside your borders."});
    }
    for (int b : g.availableBuildings(c)) {
        const BuildingDef& bd = g_buildings[b];
        std::string eff;
        auto add = [&](int v, const char* n, bool pct) { if (v) eff += (eff.empty() ? "" : ", ") + fmtPct(v) + (pct ? "% " : " ") + n; };
        add(bd.flat.food, "Food", false); add(bd.flat.prod, "Production", false); add(bd.flat.sci, "Science", false);
        add(bd.flat.cult, "Culture", false); add(bd.flat.gold, "Gold", false);
        add(bd.pct.food, "Food", true); add(bd.pct.prod, "Production", true); add(bd.pct.sci, "Science", true);
        add(bd.pct.cult, "Culture", true); add(bd.pct.gold, "Gold", true);
        if (bd.defense) eff += (eff.empty() ? "" : ", ") + std::string("+") + std::to_string(bd.defense) + " Defence";
        if (!bd.desc.empty()) eff += (eff.empty() ? "" : ". ") + bd.desc;
        if (!bd.needs.empty()) eff += ". Uses " + needsText(bd.needs);
        rows.push_back({BK_BUILDING, b, bd.name + (bd.wonder ? " (Wonder)" : ""), "", bd.name + ": " + eff});
    }
    int listTop = y, listH = ph - (y - py) - 110;
    cv->text(px + 16, y - 2, "Units, buildings and wonders", 12, C_DIM);
    listTop += 16;
    listH -= 16;
    int rh = 30;
    int maxScroll = std::max(0, (int)rows.size() * rh - listH);
    listScroll = std::clamp(listScroll, 0, maxScroll);
    cv->clip(px, listTop, pw, listH);
    Hot area{px, listTop, pw, listH, nullptr, "", true};
    hots.push_back(area);
    for (size_t i = 0; i < rows.size(); i++) {
        int ry = listTop + (int)i * rh - listScroll;
        if (ry + rh < listTop || ry > listTop + listH) continue;
        const Row& r = rows[i];
        bool cur = c.buildKind == r.kind && c.buildId == r.id;
        bool ok = g.canBuild(c, r.kind, r.id);
        bool hv = over(px + 10, ry, pw - 20, rh - 2) && mouseY >= listTop && mouseY < listTop + listH;
        Color bg = cur ? rgb(80, 66, 30) : hv ? rgb(48, 58, 76) : (i % 2 ? rgb(30, 36, 46) : rgb(34, 40, 52));
        cv->fillRect(px + 10, ry, pw - 20, rh - 2, bg);
        Color kc = r.kind == BK_UNIT ? rgb(200, 120, 90) : g_buildings[r.id].wonder ? C_GOLD : rgb(120, 170, 220);
        cv->fillRect(px + 10, ry, 4, rh - 2, kc);
        int cost = g.buildCost(c.owner, r.kind, r.id);
        int turns = std::max(1, (cost - c.prod + std::max(1, c.yields.prod) - 1) / std::max(1, c.yields.prod));
        cv->text(px + 22, ry + 7, r.name, 13, ok ? C_TEXT : C_DIM, AL_LEFT, true);
        bool needLocked = (r.kind == BK_UNIT ? !g.hasNeeds(g.player, g_units[r.id].needs) : !g.hasNeeds(g.player, g_buildings[r.id].needs));
        if (needLocked) cv->text(px + 30 + cv->textWidth(r.name, 13, true), ry + 8, "needs " + needsText(r.kind == BK_UNIT ? g_units[r.id].needs : g_buildings[r.id].needs), 11, rgb(220, 130, 100));
        cv->text(pw - 20, ry + 7, std::to_string(cost) + "  (" + std::to_string(turns) + "t)", 13, C_DIM, AL_RIGHT);
        if (ry >= listTop && ry + rh <= listTop + listH + 2) {
            int kind = r.kind, id = r.id;
            hot(px + 10, ry, pw - 20, rh - 2, ok ? std::function<void()>([this, kind, id] {
                City& cc = game->cities[selCity];
                game->setProduction(cc, kind, id);
            }) : std::function<void()>(), r.tip + (ok || r.tip.find("Requires") != std::string::npos || r.tip.find("requires") != std::string::npos ? "" : " (city must be size 2)"));
        }
    }
    cv->unclip();
    // buildings owned
    int by = py + ph - 100;
    cv->line(px + 10, by - 6, pw - 10, by - 6, C_BORDER);
    cv->text(px + 16, by, "Buildings:", 13, C_DIM, AL_LEFT, true);
    std::string built;
    for (size_t b = 0; b < g_buildings.size(); b++) if (c.has[b]) built += (built.empty() ? "" : ", ") + g_buildings[b].name;
    textBlock(px + 16, by + 18, pw - 32, built.empty() ? "None yet" : built, 12, C_TEXT, 3);
}

// ============================================================ tech tree
void GameUI::renderTech() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    int top = TOP, h = H - TOP;
    cv->fillRect(0, top, W, h, rgb(16, 20, 27));
    hot(0, top, W, h, nullptr);
    Hot area{0, top, W, h, nullptr, "", true};
    hots.push_back(area);
    const int colW = 232, boxW = 206, boxH = 62, rowH = 78, left = 24;
    int totalW = left + 16 * colW + 40;
    techScroll = std::clamp(techScroll, 0, std::max(0, totalW - W));
    int ox = left - techScroll, oy = top + 56;
    // eras
    int eraStart[NUM_ERAS + 1];
    for (int e = 0; e <= NUM_ERAS; e++) eraStart[e] = 99;
    for (auto& t : g_techs) eraStart[t.era] = std::min(eraStart[t.era], t.col);
    eraStart[NUM_ERAS] = 16;
    for (int e = 0; e < NUM_ERAS; e++) {
        int x0 = ox + eraStart[e] * colW - 8, x1 = ox + eraStart[e + 1] * colW - 14;
        cv->fillRect(x0, top + 8, x1 - x0, 28, e % 2 ? rgb(34, 40, 54) : rgb(40, 34, 50));
        std::string hdr = std::string(ERA_NAMES[e]) + " Era";
        int need = e == 0 ? TR_NEOCORTEX : e == 1 ? TR_SPEECH : e == 2 ? TR_ADV_SPEECH : TR_ABSTRACT;
        if (need >= 0 && e <= 3) hdr += g.hasTrait(g.player, need) ? "" : "  -  requires " + g_traits[need].name;
        cv->text((x0 + x1) / 2, top + 14, hdr, 14, need >= 0 && !g.hasTrait(g.player, need) ? rgb(240, 140, 110) : C_GOLD, AL_CENTER, true);
    }
    auto boxPos = [&](int t, int& bx, int& by) { bx = ox + g_techs[t].col * colW; by = oy + g_techs[t].row * rowH; };
    // goal path
    std::vector<uint8_t> onPath(g_techs.size(), 0);
    if (me.goal >= 0) {
        std::vector<int> st = {me.goal};
        while (!st.empty()) {
            int t = st.back(); st.pop_back();
            if (onPath[t] || me.techs[t]) continue;
            onPath[t] = 1;
            for (int p : g_techs[t].prereqs) st.push_back(p);
        }
    }
    // lines
    for (size_t t = 0; t < g_techs.size(); t++) {
        int bx, by;
        boxPos((int)t, bx, by);
        for (int p : g_techs[t].prereqs) {
            int px, py;
            boxPos(p, px, py);
            int x1 = px + boxW, y1 = py + boxH / 2, x2 = bx, y2 = by + boxH / 2;
            int mx = x2 - 12;
            Color lc = me.techs[p] ? rgb(110, 150, 110) : rgb(80, 88, 104);
            cv->line(x1, y1, mx, y1, lc, 2);
            cv->line(mx, y1, mx, y2, lc, 2);
            cv->line(mx, y2, x2, y2, lc, 2);
        }
    }
    // boxes
    for (size_t ti = 0; ti < g_techs.size(); ti++) {
        int t = (int)ti;
        const TechDef& td = g_techs[t];
        int bx, by;
        boxPos(t, bx, by);
        if (bx > W || bx + boxW < 0) continue;
        bool known = me.techs[t], cur = me.research == t, avail = g.canResearch(g.player, t);
        Color bg = known ? rgb(36, 74, 46) : cur ? rgb(92, 76, 30) : onPath[t] ? rgb(40, 62, 96) : avail ? rgb(38, 54, 78) : rgb(34, 36, 44);
        if (over(bx, by, boxW, boxH)) bg = lighten(bg, 0.12f);
        cv->fillRect(bx, by, boxW, boxH, bg);
        cv->rect(bx, by, boxW, boxH, cur ? C_GOLD : known ? rgb(90, 160, 100) : avail ? rgb(100, 140, 200) : rgb(70, 74, 86), cur ? 2 : 1);
        bool tier = false;
        for (int k = 0; k < RUIN_TIERS; k++) if (g_ruinTierTech[k] == t) tier = true;
        cv->text(bx + 8, by + 5, td.name, 14, known ? rgb(200, 240, 200) : C_TEXT, AL_LEFT, true);
        if (tier) cv->text(bx + boxW - 8, by + 5, "RUINS", 10, rgb(255, 170, 80), AL_RIGHT, true);
        if (t == ID.tCurrency) cv->text(bx + boxW - 8, by + 5, "GOLD", 10, C_GOLD, AL_RIGHT, true);
        std::string info;
        bool traitLocked = td.trait >= 0 && !g.hasTrait(g.player, td.trait);
        if (known) info = "Researched";
        else if (traitLocked) info = "Needs " + g_traits[td.trait].name + " (evolution)";
        else {
            int sp = std::max(1, me.sciencePerTurn);
            int left = g.techCost(t) - me.techProgress[t];
            info = std::to_string(g.techCost(t)) + " science  (" + std::to_string((left + sp - 1) / sp) + " turns)";
        }
        cv->text(bx + 8, by + 24, info, 11, !known && traitLocked ? rgb(240, 140, 110) : C_DIM);
        std::string un;
        for (auto& u : g_units)
            if (u.techId == t && u.primate != -2 && (u.primate == -1 ? g.uniqueFor(g.player, unitIndex(u.name)) == unitIndex(u.name) : u.primate == me.primate))
                un += (un.empty() ? "" : ", ") + u.name;
        for (auto& b : g_buildings) if (b.techId == t) un += (un.empty() ? "" : ", ") + b.name;
        for (auto& im : g_improvements) if (im.techId == t && !im.name.empty()) un += (un.empty() ? "" : ", ") + im.name;
        for (auto& rs2 : g_resources) if (rs2.revealTechId == t && !rs2.name.empty()) un += (un.empty() ? "" : ", ") + std::string("Reveals ") + rs2.name;
        while (un.size() > 4 && cv->textWidth(un, 11) > boxW - 16) un = un.substr(0, un.size() - 4) + "..";
        cv->text(bx + 8, by + 40, un, 11, rgb(190, 200, 150));
        if (cur) bar(bx + 1, by + boxH - 4, boxW - 2, 3, (double)me.techProgress[t] / g.techCost(t), C_SCI);
        std::string tip = td.name + " - " + td.flavor;
        if (!known && !avail) tip += " (Click to set as a research goal.)";
        hot(bx, by, boxW, boxH, known ? std::function<void()>() : std::function<void()>([this, t] { game->setGoal(game->player, t); }), tip);
    }
    cv->fillRect(0, H - 34, W, 34, rgb(20, 24, 31));
    cv->text(16, H - 25, "Click a technology to research it (or set it as a goal). Scroll with the mouse wheel or arrow keys.", 13, C_DIM);
    button(W - 110, H - 30, 100, 26, "Close", true, [this] { overlay = OV_NONE; });
}

// ============================================================ traditions
void GameUI::renderPolicies() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    int pw = std::min(980, W - 40), ph = std::min(620, H - TOP - 30);
    int px = (W - pw) / 2, py = TOP + 14;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 14, "Traditions", 26, C_CULT, AL_LEFT, true);
    cv->text(px + 20, py + 48, "Culture " + std::to_string(me.culture) + " / " + std::to_string(g.policyCost(g.player)) + " needed for the next tradition (+" +
             std::to_string(me.culturePerTurn) + " per turn).  Adopted " + std::to_string(me.policiesAdopted) + " of " + std::to_string(g_policies.size()) +
             " - adopting all of them wins a Cultural victory.", 13, C_DIM);
    button(px + pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    int cols = 4, gap = 10;
    int cw = (pw - 40 - gap * (cols - 1)) / cols, ch = std::min(112, (ph - 90 - gap * 3) / 4);
    for (size_t i = 0; i < g_policies.size(); i++) {
        const PolicyDef& p = g_policies[i];
        int x = px + 20 + (int)(i % cols) * (cw + gap), y = py + 78 + (int)(i / cols) * (ch + gap);
        bool has = me.policies[i], can = g.canAdopt(g.player, (int)i), locked = !g.hasTech(g.player, p.techId);
        Color bg = has ? rgb(70, 44, 84) : can ? rgb(52, 46, 74) : rgb(32, 34, 44);
        if (can && over(x, y, cw, ch)) bg = lighten(bg, 0.15f);
        cv->fillRect(x, y, cw, ch, bg);
        cv->rect(x, y, cw, ch, has ? C_CULT : can ? rgb(240, 200, 255) : rgb(70, 72, 84), can ? 2 : 1);
        cv->text(x + 10, y + 8, p.name, 15, locked ? C_DIM : C_TEXT, AL_LEFT, true);
        textBlock(x + 10, y + 30, cw - 20, p.desc, 13, locked ? C_DIM : rgb(220, 210, 240), 3);
        std::string st = has ? "ADOPTED" : locked ? "Requires " + p.tech : can ? "Click to adopt" : "Not enough culture";
        cv->text(x + 10, y + ch - 20, st, 11, has ? C_CULT : locked ? rgb(200, 120, 100) : C_DIM, AL_LEFT, true);
        int pi = (int)i;
        hot(x, y, cw, ch, can ? std::function<void()>([this, pi] {
            game->adoptPolicy(game->player, pi);
            bool more = false;
            for (size_t q = 0; q < g_policies.size(); q++) if (game->canAdopt(game->player, (int)q)) more = true;
            if (!more) overlay = OV_NONE;
        }) : std::function<void()>(), "");
    }
}

// ============================================================ diplomacy
void GameUI::renderDiplomacy() {
    Game& g = *game;
    int pw = std::min(1000, W - 40), ph = std::min(600, H - TOP - 30);
    int px = (W - pw) / 2, py = TOP + 14;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 14, "Diplomacy", 26, C_TEXT, AL_LEFT, true);
    button(px + pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    int y = py + 60;
    int myPow = g.militaryPower(g.player);
    cv->text(px + 20, y, "Your score " + std::to_string(g.score(g.player)) + "   Military strength " + std::to_string(myPow), 14, C_DIM);
    y += 30;
    for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) {
        if (c == g.player) continue;
        const Civ& o = g.civs[c];
        int rh = 74;
        cv->fillRect(px + 16, y, pw - 32, rh - 6, rgb(32, 38, 50));
        if (!g.civs[g.player].met[c]) {
            cv->fillCircle(px + 46, y + 32, 18, rgb(60, 60, 60));
            cv->text(px + 46, y + 22, "?", 18, C_DIM, AL_CENTER, true);
            cv->text(px + 78, y + 22, "An unknown civilisation", 16, C_DIM, AL_LEFT, true);
            y += rh;
            continue;
        }
        cv->fillCircle(px + 46, y + 32, 18, o.color);
        cv->circle(px + 46, y + 32, 18, rgb(0, 0, 0), 2);
        cv->text(px + 78, y + 8, o.leader + " of the " + o.name, 16, o.alive ? C_TEXT : C_DIM, AL_LEFT, true);
        if (!o.alive) {
            cv->text(px + 78, y + 32, "Destroyed", 14, C_RED, AL_LEFT, true);
            y += rh;
            continue;
        }
        const PrimateDef& pd = g_primates[o.primate];
        cv->text(px + 78, y + 30, pd.ability + ": " + pd.abilityDesc, 12, C_DIM);
        bool war = g.civs[g.player].war[c];
        cv->text(px + 78, y + 48, std::string(war ? "AT WAR" : "At peace") + "   Score " + std::to_string(g.score(c)) + "   Cities " +
                 std::to_string(g.numCities(c)) + "   Military " + std::to_string(g.militaryPower(c)), 13, war ? C_RED : C_FOOD, AL_LEFT, true);
        if (war)
            button(px + pw - 180, y + 18, 150, 32, "Propose Peace", true, [this, c] {
                bool ok = game->proposePeace(game->player, c);
                toast = ok ? "The " + game->civs[c].name + " accept peace." : "The " + game->civs[c].name + " refuse peace.";
                toastMs = 2500;
            }, "They are more likely to agree after a long war, or if you are stronger.", rgb(50, 100, 60));
        else
            button(px + pw - 180, y + 18, 150, 32, "Declare War", true, [this, c] { game->declareWar(game->player, c); }, "", rgb(120, 46, 40));
        y += rh;
        if (y > py + ph - rh) break;
    }
}

// ============================================================ evolution
static Color catColor(int k) {
    static const Color C[PC_COUNT] = {{232, 150, 210}, {124, 212, 110}, {240, 112, 92}, {214, 232, 64}};
    return C[k];
}

std::string GameUI::pressureTip(int c) {
    Game& g = *game;
    const Civ& cv = g.civs[c];
    std::vector<int> r = g.evolutionRates(c);
    std::string s = "Evolution pools:";
    for (int k = 0; k < PC_COUNT; k++)
        s += std::string("\n  ") + PRESSURE_NAMES[k] + " " + std::to_string(cv.evoPool[k]) + " (+" + std::to_string(r[k]) + " per turn)";
    s += "\nOpen the Evolution screen (V) to choose what to evolve.";
    return s;
}

void GameUI::renderEvolution() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    int pw = std::min(1180, W - 30), ph = std::min(700, H - TOP - 16);
    int px = (W - pw) / 2, py = TOP + 8;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 12, "Evolution", 24, rgb(130, 220, 150), AL_LEFT, true);
    button(px + pw - 40, py + 10, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    cv->text(px + 170, py + 18, "Pressures fill four pools; each trait needs particular kinds. Commit to at most two strands - and only one final adaptation.", 13, C_DIM);
    std::vector<int> rate = g.evolutionRates(g.player);
    std::vector<Pressure> pres = g.evolutionPressures(g.player);
    // ---- pools
    int y = py + 48;
    int bw = (pw - 40 - 3 * 10) / 4, bh = 98;
    for (int k = 0; k < PC_COUNT; k++) {
        int x = px + 20 + k * (bw + 10);
        Color cc = catColor(k);
        cv->fillRect(x, y, bw, bh, rgb(30, 34, 44));
        cv->fillRect(x, y, 4, bh, cc);
        cv->text(x + 12, y + 6, PRESSURE_NAMES[k], 15, cc, AL_LEFT, true);
        cv->text(x + bw - 10, y + 6, std::to_string(me.evoPool[k]) + "  (+" + std::to_string(rate[k]) + ")", 15, C_TEXT, AL_RIGHT, true);
        int ly = y + 28, n = 0;
        std::string tip;
        for (auto& p : pres) {
            if (p.cat != k) continue;
            tip += (tip.empty() ? "" : "\n") + std::string("+") + std::to_string(p.value) + " " + p.name + ": " + p.why;
            if (n < 4) {
                cv->text(x + 12, ly, "+" + std::to_string(p.value) + " " + p.name, 12, C_DIM);
                ly += 16;
            }
            n++;
        }
        if (!n) cv->text(x + 12, ly, "No pressure of this kind", 12, rgb(200, 120, 100));
        static const char* HINT[PC_COUNT] = {"Grows with cities, population and culture.", "Grows with territory, harsh lands and hard times.",
                                             "Grows with war, battles and predators.", "Grows with fallout in your lands."};
        hot(x, y, bw, bh, nullptr, std::string(PRESSURE_NAMES[k]) + " pressure. " + HINT[k] + (tip.empty() ? "" : "\n" + tip));
    }
    // ---- trait card
    auto card = [&](int t, int x, int cy, int w, int h) {
        const TraitDef& td = g_traits[t];
        bool has = g.hasTrait(g.player, t), goal = me.evoGoal == t, can = g.canEvolve(g.player, t);
        int turns = can ? g.evolveTurns(g.player, t) : -1;
        Color bg = has ? rgb(34, 78, 50) : goal ? rgb(70, 62, 30) : can ? rgb(36, 46, 60) : rgb(30, 32, 38);
        if (can && over(x, cy, w, h)) bg = lighten(bg, 0.12f);
        cv->fillRect(x, cy, w, h, bg);
        cv->rect(x, cy, w, h, goal ? C_GOLD : has ? rgb(110, 200, 130) : can ? rgb(100, 130, 170) : rgb(60, 64, 74), goal ? 2 : 1);
        std::string nm = td.name;
        while (nm.size() > 4 && cv->textWidth(nm, 13, true) > w - (td.capstone ? 56 : 14)) nm = nm.substr(0, nm.size() - 3) + ".";
        cv->text(x + 7, cy + 5, nm, 13, has ? rgb(200, 250, 210) : can ? C_TEXT : C_DIM, AL_LEFT, true);
        if (td.capstone) cv->text(x + w - 6, cy + 6, "FINAL", 10, C_GOLD, AL_RIGHT, true);
        int cx = x + 7;
        for (int k = 0; k < PC_COUNT; k++) {
            if (!td.cost[k]) continue;
            std::string cs = std::string(PRESSURE_ABBR[k]) + " " + std::to_string(g.traitCost(t, k));
            cv->text(cx, cy + 23, cs, 11, has ? C_DIM : catColor(k), AL_LEFT, true);
            cx += cv->textWidth(cs, 11, true) + 8;
        }
        std::string eff = td.effect;
        while (eff.size() > 4 && cv->textWidth(eff, 11) > w - 14) eff = eff.substr(0, eff.size() - 4) + "..";
        cv->text(x + 7, cy + 39, eff, 11, rgb(200, 210, 220));
        std::string st;
        Color sc = C_DIM;
        if (has) { st = "EVOLVED"; sc = rgb(130, 220, 150); }
        else if (!can) { st = g.traitBlocker(g.player, t); sc = rgb(150, 150, 160); }
        else if (turns < 0) { st = g.traitBlocker(g.player, t); sc = rgb(240, 140, 110); }
        else { st = (goal ? "Evolving - " : "") + (turns == 0 ? std::string("ready") : std::to_string(turns) + " turns"); sc = goal ? C_GOLD : C_TEXT; }
        while (st.size() > 4 && cv->textWidth(st, 11, true) > w - 14) st = st.substr(0, st.size() - 4) + "..";
        cv->text(x + 7, cy + h - 17, st, 11, sc, AL_LEFT, true);
        std::string tip = td.name + ": " + td.effect + ".\n" + td.flavor;
        std::string blk = g.traitBlocker(g.player, t);
        if (!blk.empty() && !has) tip += "\n" + blk + ".";
        if (td.capstone) tip += "\nFinal adaptation: a species can only ever have one.";
        hot(x, cy, w, h, can ? std::function<void()>([this, t] { game->setEvoGoal(game->player, t); }) : std::function<void()>(), tip);
    };
    // ---- trunk
    y += bh + 12;
    cv->text(px + 20, y, std::string(STRAND_NAMES[ST_TRUNK]) + "  -  required for science", 14, C_GOLD, AL_LEFT, true);
    y += 20;
    int tw = (pw - 40 - 5 * 8) / 6, th = 76;
    for (int i = 0; i < 6; i++) {
        int x = px + 20 + i * (tw + 8);
        card(i, x, y, tw, th);
        if (i < 5) cv->line(x + tw, y + th / 2, x + tw + 8, y + th / 2, rgb(110, 140, 120), 2);
    }
    // ---- strands
    y += th + 16;
    int sw = (pw - 40 - 3 * 12) / 4, sh = std::min(76, (py + ph - (y + 40) - 2 * 8) / 3);
    static const int MAIN_CAT[ST_COUNT] = {PC_SOCIAL, PC_ECOLOGY, PC_SOCIAL, PC_CONFLICT, PC_MUTATION};
    for (int st = ST_TOOL; st < ST_COUNT; st++) {
        int x = px + 20 + (st - 1) * (sw + 12);
        bool closed = !g.strandStarted(g.player, st) && g.strandsStarted(g.player) >= MAX_STRANDS;
        cv->text(x, y, std::string(STRAND_NAMES[st]) + (closed ? " (closed)" : ""), 14, closed ? C_DIM : C_TEXT, AL_LEFT, true);
        cv->text(x + sw, y + 2, std::string("needs ") + PRESSURE_NAMES[MAIN_CAT[st]], 11, catColor(MAIN_CAT[st]), AL_RIGHT, true);
        int cy = y + 20, i = 0;
        for (int t = 0; t < TR_COUNT; t++) {
            if (g_traits[t].strand != st) continue;
            card(t, x, cy + i * (sh + 8), sw, sh);
            if (i < 2) cv->line(x + sw / 2, cy + i * (sh + 8) + sh, x + sw / 2, cy + (i + 1) * (sh + 8), rgb(110, 140, 120), 2);
            i++;
        }
    }
    if (const PrimateDef* pd = g.primate(g.player))
        cv->text(px + 20, py + ph - 20, std::string("The ") + pd->plural + " lean towards the " + STRAND_NAMES[pd->evoLean] +
                 (pd->evoPct ? " and evolve " + std::to_string(pd->evoPct) + "% faster than most primates." : "."), 12, C_DIM);
}

// ============================================================ menu, help, log
// "Interface size  [-]  100%  [+]" row, 40 px tall
void GameUI::uiScaleControl(int x, int y, int w) {
    cv->fillRectA(x, y, w, 40, rgb(20, 24, 30), 215);
    cv->text(x + 12, y + 11, "Interface size", 15, C_TEXT);
    std::string tip = "Make text, buttons and the map bigger or smaller (Ctrl + / Ctrl - / Ctrl 0)";
    button(x + w - 128, y + 5, 30, 30, "-", uiScalePct > UI_SCALE_MIN, [this] { adjustUiScale(-1); }, tip, C_PANEL2, 18);
    cv->text(x + w - 64, y + 11, std::to_string(uiScalePct) + "%", 15, C_GOLD, AL_CENTER, true);
    button(x + w - 34, y + 5, 30, 30, "+", uiScalePct < UI_SCALE_MAX, [this] { adjustUiScale(1); }, tip, C_PANEL2, 18);
}

void GameUI::renderMenu() {
    int pw = 320, ph = 460, px = (W - pw) / 2, py = (H - ph) / 2;
    hot(0, 0, W, H, [this] { overlay = OV_NONE; });
    panel(px, py, pw, ph);
    cv->text(px + pw / 2, py + 16, "Menu", 24, C_TEXT, AL_CENTER, true);
    int y = py + 60;
    auto b = [&](const std::string& l, std::function<void()> fn) { button(px + 30, y, pw - 60, 40, l, true, fn, "", C_PANEL2, 16); y += 48; };
    b("Resume", [this] { overlay = OV_NONE; });
    b("Save Game (Ctrl+S)", [this] { saveGame(); overlay = OV_NONE; });
    b("Load Game (Ctrl+L)", [this] { loadGame(); });
    b("How to Play (F1)", [this] { overlay = OV_HELP; helpScroll = 0; });
    b("Achievements", [this] { overlay = OV_ACHIEVEMENTS; achScroll = 0; });
    uiScaleControl(px + 30, y, pw - 60);
    y += 52;
    b("Main Menu", [this] { overlay = OV_NONE; screen = SCR_TITLE; });
    b("Exit Game", [this] { quit = true; });
}

void GameUI::renderHelp() {
    int top0 = screen == SCR_GAME ? TOP + 8 : 20;
    int pw = std::min(1000, W - 40), ph = H - top0 - 20;
    int px = (W - pw) / 2, py = top0;
    hot(0, 0, W, H, nullptr);
    panel(px, py, pw, ph, rgb(24, 28, 36));
    cv->text(px + 20, py + 14, "How to Play", 26, C_GOLD, AL_LEFT, true);
    button(px + pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    const char* text =
        "THE STORY  The game begins in 2032, shortly before humanity wipes itself out in a global nuclear war in August 2034. "
        "You lead a band of primates. Survive the Fall, then build a new civilisation from the ashes.\n"
        "WINNING  Rebirth victory: research Rebirth and complete the Rebirth Project. Domination: control every original primate capital. "
        "Cultural: adopt every Tradition. Otherwise the highest score wins when time runs out.\n"
        "EVOLUTION  Your species starts unable to learn deliberately: it earns no science until it evolves an Enlarged Neocortex. Four kinds of "
        "evolutionary pressure fill four pools each turn: Social (cities, population, culture), Ecological (survival, territory, harsh lands, the "
        "nuclear winter), Conflict (war, battles, predators) and Mutation (fallout). On the Evolution screen (V) you choose which trait to evolve "
        "towards; it appears once your pools can pay for it. The language trunk (Neocortex, Gestures, Proto-Speech, Speech, Advanced Speech, "
        "Abstract Thought) is needed for science: Speech for Tribal technologies, Advanced Speech for Ancient ones, Abstract Thought for the rest. "
        "Four strands - Tool Brain, Social Brain, Predator Brain and the Mutant Line - each need their own kind of pressure, so a peaceful species "
        "will struggle to become predators. A species may commit to only two strands, and can only ever have one final adaptation.\n"
        "NOMADS  Your people begin as Roaming Bands. A band forages the land around it (one tile per member, plus one); foraged land is picked "
        "over and recovers slowly, so keep the band moving to fresh forest, jungle and grassland. A well-fed band grows to at most 8 members. Split "
        "a big band in two (G), or turn members into a War Party (R) or a Scout to guard and explore. Bands that go hungry lose members. Only once "
        "your species has evolved Speech and discovered Cultivation can a band settle (B) and found a city - the end of the wandering age.\n"
        "NAVAL WARFARE  Coastal cities build ships: Dugout Canoes (Rafts), War Galleys (Bronze Working, needs Copper) and Archer Galleys "
        "(Archery) keep to coastal waters; Caravels (Navigation), Frigates (Gunpowder), Ironclads (Steam Power, Coal), Salvaged Destroyers "
        "(Combustion, Oil) and Salvaged Battleships (Motorised Warfare, Oil) cross the ocean. Ships move on water and dock in your own "
        "coastal cities, where they can share the tile with a land garrison. Melee ships ram ships and embarked units and can storm a "
        "coastal city; ranged ships bombard anything in range, including the shore. Embarked land units are easy prey - escort them. "
        "Some coastal military-base ruins hold a warship that can be raised from the harbour.\n"
        "TRANSPORTS  Canoes and War Galleys carry 1 land unit, Caravels 2, Frigates 1 and Salvaged Freighters (Steam Power) 4. Move a land unit "
        "onto your ship's tile to board it (or press K in a harbour city); it sails with the ship and lands by moving onto an adjacent land tile. "
        "Units aboard cannot fight, and if the ship sinks they are lost.\n"
        "UPGRADES  Inside your own territory, a unit can be refitted (U) as the best unit of its line you can train - for example "
        "Warband > Spear Troop > Iron Blades > Rifles > Infantry. With Currency it costs gold; before that, the refit is done in one of your "
        "cities and paid for from that city's stored production. Upgraded units keep their health.\n"
        "NUCLEAR WEAPONS  Nuclear Fission lets a city build a Nuclear Warhead (needs Uranium). Warheads wait in your silos: click the warhead "
        "count on the top bar, pick a tile within 12 of one of your cities and confirm. Everything next to the blast is destroyed, cities are "
        "wrecked, fallout spreads, your victims declare war and others who know you may join them.\n"
        "ACHIEVEMENTS  Earned across all your games and kept in your profile - see Achievements on the title screen or in the menu.\n"
                "DISASTERS  Wildfires, droughts, floods, epidemics, earthquakes, harsh winters, reactor leaks and predator migrations strike at random - "
        "but which ones depends on where you live: forests burn, coasts flood, crowded cities catch disease, ruined reactors leak. Each one hurts, "
        "and each one adds evolutionary pressure - a burst of points plus lingering pressure for a few turns.\n"
        "TIME  The calendar follows progress: while species are still evolving each turn spans thousands of years; once the leading species "
        "masters Advanced Speech, turns shrink era by era from about 50 years to 1. Hover over the date to see the current pace.\n"
        "THE RUINS  The Fall leaves ruined cities, towns, bases, labs and power plants. Each holds up to four layers of salvage - Scrap, Books, "
        "Machines and Data - that only become usable once you research Salvaging, Deciphering, Reverse Engineering and Computing. Move a Gatherer "
        "onto a ruin and press Excavate. Orange dots under a ruin are layers you can dig now. Ruins inside your borders also add Production and Science.\n"
        "FALLOUT  Radioactive tiles (yellow symbols) damage units and produce nothing. Fallout fades with time; Radiology lets Gatherers clean it.\n"
        "RESOURCES  Technologies reveal resources on the map: Copper (Digging), Tin (Bronze Working), Horses (Animal Taming), Iron (Smelting), "
        "Coal (Steam Power), Oil (Combustion), Uranium (Radiology) and Gold Ore (Currency). Strategic resources inside your borders unlock units and "
        "buildings - Copper plus Tin for the Bronze Phalanx, Horses for riders, Iron (or salvaged Scrap Metal) for Iron Blades, Coal for Factories, "
        "Oil for tanks. Wheat, Rice and Maize are crops: build a Farm on them for extra food. Build Pastures on Horses and Mines on minerals.\n"
        "GOLD  Your people only learn the value of money with the Currency technology. From then on cities earn Gold, used to buy items.\n"
        "CITIES  Your first cities are founded by bands; after that, cities train Elder Troops to found more. Click a city to choose what it builds. Cities grow with surplus food and claim land with culture.\n"
        "COMBAT  Move a military unit onto an enemy to attack. Ranged units attack from a distance. Hills, forest, jungle and fortifying help defence. "
        "Capture cities by reducing their HP to zero with a melee unit. Beware the feral packs that roam the wastelands.\n"
        "CONTROLS  Left-click: select.  Right-click: move / attack.  Drag or arrow keys: scroll.  Wheel or +/-: zoom.  Enter: end turn.  "
        "Space: skip unit.  Tab: next unit.  B: settle / found city.  G: split band.  R: war party.  F: fortify.  S: sleep.  E: excavate.  X: decontaminate.  M: move mode.  C: centre on unit.  "
        "T: technologies.  V: evolution.  P: traditions.  D: diplomacy.  L: log.  Ctrl+S / Ctrl+L: save / load.  Ctrl + / Ctrl - / Ctrl 0: interface size.  Esc: menu / close.";
    int top = py + 56, viewH = ph - 70;
    Hot area{px, top, pw, viewH, nullptr, "", true};
    hots.push_back(area);
    int total = (int)wrap(text, pw - 50, 14).size() * 19;
    helpScroll = std::clamp(helpScroll, 0, std::max(0, total - viewH + 10));
    cv->clip(px + 1, top, pw - 2, viewH);
    textBlock(px + 20, top - helpScroll, pw - 50, text, 14, C_TEXT, 5);
    cv->unclip();
    if (total > viewH) {  // scroll bar
        int th = std::max(30, viewH * viewH / total);
        int ty = top + (viewH - th) * helpScroll / std::max(1, total - viewH + 10);
        cv->fillRect(px + pw - 14, top, 6, viewH, rgb(40, 46, 58));
        cv->fillRect(px + pw - 14, ty, 6, th, rgb(120, 136, 160));
        if (helpScroll < total - viewH) cv->text(px + pw / 2, py + ph - 20, "Scroll for more", 12, C_DIM, AL_CENTER);
    }
}

void GameUI::renderLog() {
    Game& g = *game;
    int pw = std::min(760, W - 40), ph = H - TOP - 40;
    int px = (W - pw) / 2, py = TOP + 14;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 14, "Event Log", 24, C_TEXT, AL_LEFT, true);
    button(px + pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    Hot area{px, py, pw, ph, nullptr, "", true};
    hots.push_back(area);
    int y = py + 56;
    int maxLines = (ph - 70) / 18;
    logScroll = std::clamp(logScroll, 0, std::max(0, (int)g.log.size() - maxLines));
    for (int i = (int)g.log.size() - 1 - logScroll, n = 0; i >= 0 && n < maxLines; i--, n++) {
        const Notification& no = g.log[i];
        std::string s = "T" + std::to_string(no.turn) + ": " + no.text;
        while (s.size() > 4 && cv->textWidth(s, 13) > pw - 40) s = s.substr(0, s.size() - 4) + "..";
        cv->text(px + 20, y, s, 13, no.turn == g.turn ? C_TEXT : C_DIM);
        int nx = no.x, ny = no.y;
        if (nx >= 0) hot(px + 20, y, pw - 40, 18, [this, nx, ny] { centerOn(nx, ny); overlay = OV_NONE; });
        y += 18;
    }
}

// ============================================================ popups & the Fall
void GameUI::closePopup() {
    if (game && !game->popups.empty()) game->popups.erase(game->popups.begin());
    if (game && !game->popups.empty()) {
        const Popup& p = game->popups.front();
        if (p.x >= 0) centerOn(p.x, p.y);
    }
}

void GameUI::renderPopup() {
    Game& g = *game;
    const Popup& p = g.popups.front();
    hot(0, 0, W, H, nullptr);
    bool big = p.kind == POP_NUKE || p.kind == POP_INTRO || p.kind == POP_VICTORY || p.kind == POP_DEFEAT;
    int pw = big ? std::min(760, W - 40) : std::min(560, W - 40);
    int textW = pw - 40 - (p.kind == POP_NUKE ? 170 : 0);
    auto lines = wrap(p.body, textW, 15);
    int ph = 132 + (int)lines.size() * 21;
    if (p.kind == POP_VICTORY || p.kind == POP_DEFEAT) ph += 30 + (int)(g.civs.size() - FIRST_PRIMATE_CIV) * 20;
    if (p.kind == POP_NUKE) ph = std::max(ph, 330);
    int px = (W - pw) / 2, py = std::max(TOP + 10, (H - ph) / 2);
    Color head = p.kind == POP_NUKE ? rgb(150, 50, 30) : p.kind == POP_WAR ? rgb(140, 40, 40) : p.kind == POP_VICTORY ? rgb(150, 120, 40) :
                 p.kind == POP_DEFEAT ? rgb(80, 40, 40) : p.kind == POP_RUIN ? rgb(130, 84, 40) : p.kind == POP_TECH ? rgb(40, 80, 130) : p.kind == POP_EVOLUTION ? rgb(40, 110, 70) : p.kind == POP_DISASTER ? rgb(150, 70, 20) : p.kind == POP_SETTLE ? rgb(150, 112, 40) : rgb(56, 70, 92);
    cv->fillRect(px - 2, py - 2, pw + 4, ph + 4, rgb(0, 0, 0));
    cv->fillRect(px, py, pw, ph, rgb(26, 30, 38));
    cv->fillRect(px, py, pw, 46, head);
    cv->rect(px, py, pw, ph, lighten(head, 0.3f), 2);
    cv->text(px + 20, py + 11, p.title, 22, rgb(255, 245, 230), AL_LEFT, true);
    int tx = px + 20;
    if (p.kind == POP_NUKE) {
        // mushroom cloud illustration
        int mx = px + 95, my = py + 250;
        cv->fillRect(mx - 70, my, 140, 30, rgb(60, 40, 30));
        cv->fillRect(mx - 14, my - 120, 28, 120, rgb(170, 110, 80));
        for (int k = 0; k < 6; k++) cv->fillCircle(mx - 50 + k * 20, my - 130 - (k % 2) * 10, 30, rgb(190, 120, 80));
        cv->fillCircle(mx, my - 150, 36, rgb(230, 160, 90));
        cv->fillCircle(mx, my - 150, 18, rgb(255, 220, 140));
        cv->fillRect(mx - 60, my - 70, 120, 14, rgb(200, 130, 90));
        tx = px + 190;
    }
    int y = py + 62;
    for (auto& l : lines) { cv->text(tx, y, l, 15, C_TEXT); y += 21; }
    if (p.kind == POP_VICTORY || p.kind == POP_DEFEAT) {
        y += 10;
        std::vector<int> order;
        for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) order.push_back(c);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return g.score(a) > g.score(b); });
        for (int c : order) {
            cv->fillCircle(tx + 8, y + 8, 7, g.civs[c].color);
            cv->text(tx + 24, y, g.civs[c].name + (g.civs[c].alive ? "" : " (destroyed)"), 14, c == g.player ? C_GOLD : C_TEXT, AL_LEFT, c == g.player);
            cv->text(tx + 330, y, std::to_string(g.score(c)), 14, C_TEXT, AL_RIGHT, true);
            y += 20;
        }
        button(px + pw - 380, py + ph - 52, 170, 38, "Continue Playing", true, [this] { closePopup(); }, "", rgb(50, 90, 60), 15);
        button(px + pw - 200, py + ph - 52, 180, 38, "Main Menu", true, [this] { closePopup(); screen = SCR_TITLE; }, "", C_PANEL2, 15);
        return;
    }
    if (p.x >= 0) {
        int gx = p.x, gy = p.y;
        button(px + pw - 260, py + ph - 52, 110, 38, "Go There", true, [this, gx, gy] { closePopup(); centerOn(gx, gy); }, "", C_PANEL2, 15);
    }
    button(px + pw - 140, py + ph - 52, 120, 38, "Continue", true, [this] { closePopup(); }, "(Enter)", rgb(60, 80, 110), 16);
}

void GameUI::renderNuke() {
    Game& g = *game;
    int t = nukeT;
    // screen flashes, then fireballs on every human site that is on screen
    for (auto& s : g.sites) {
        int sx, sy;
        tileCenter(s.x, s.y, sx, sy);
        if (sx < -300 || sy < -300 || sx > W + 300 || sy > H + 300) continue;
        int delay = (int)((s.x * 37 + s.y * 91) % 900);
        int lt = t - delay;
        if (lt < 0) continue;
        double k = std::min(1.0, lt / 1600.0);
        int big = s.type == RUIN_CITY ? hs * 3 : hs * 2;
        int r = (int)(big * (0.3 + k));
        Color outer = blend(rgb(255, 250, 220), rgb(120, 60, 40), (float)k);
        cv->fillCircle(sx, sy, r, outer);
        cv->fillCircle(sx, sy, r * 2 / 3, blend(rgb(255, 255, 255), rgb(230, 120, 40), (float)k));
        if (lt > 600) {
            // rising mushroom
            int rise = std::min(lt - 600, 2400) * big / 1600;
            cv->fillRect(sx - big / 6, sy - rise, big / 3, rise, rgb(150, 100, 80));
            cv->fillCircle(sx, sy - rise, big * 2 / 3, rgb(170, 110, 80));
            cv->fillCircle(sx - big / 2, sy - rise + big / 6, big / 2, rgb(150, 96, 72));
            cv->fillCircle(sx + big / 2, sy - rise + big / 6, big / 2, rgb(150, 96, 72));
            cv->fillCircle(sx, sy - rise - big / 8, big / 3, rgb(240, 170, 90));
        }
    }
    if (t < 350) cv->fillRect(0, 0, W, H, blend(rgb(255, 255, 255), rgb(255, 230, 200), t / 350.0f));
    int ts = std::min(64, W / 16);
    cv->text(W / 2 + 3, H / 3 + 3, "AUGUST 2034", ts, rgb(0, 0, 0), AL_CENTER, true);
    cv->text(W / 2, H / 3, "AUGUST 2034", ts, rgb(255, 220, 180), AL_CENTER, true);
    hot(0, 0, W, H, [this] { if (nukeT > 1200) nukeT = -1; }, "");
}

// ============================================================ actions
int GameUI::nextUnitNeedingOrders(int after) const {
    const Game& g = *game;
    int n = (int)g.units.size();
    for (int k = 1; k <= n; k++) {
        int i = ((after < 0 ? -1 : after) + k) % n;
        if (i < 0) i += n;
        const Unit& u = g.units[i];
        if (u.alive && u.owner == g.player && g.unitNeedsOrders(u)) return i;
    }
    return -1;
}

void GameUI::selectUnit(int id, bool center) {
    selUnit = id;
    moveMode = false;
    disbandArm = -1;
    if (id < 0 || !game) return;
    const Unit& u = game->units[id];
    int sx, sy;
    tileCenter(u.x, u.y, sx, sy);
    if (center || sx < 80 || sy < TOP + 60 || sx > W - 330 || sy > H - 200) centerOn(u.x, u.y);
}

void GameUI::afterUnitAction() {
    Unit* u = sel();
    moveMode = false;
    disbandArm = -1;
    if (!u || u->moves <= 0 || u->order != ORD_NONE) {
        int n = nextUnitNeedingOrders(selUnit);
        if (n >= 0) selectUnit(n, false);
        else if (u && !u->alive) selUnit = -1;
    }
}

void GameUI::unitCommand(char cmd) {
    Unit* up = sel();
    if (!up || up->owner != game->player) return;
    Unit& u = *up;
    Game& g = *game;
    switch (cmd) {
        case 'B':
            if (g_units[u.type].cls == UnitClass::Band) {
                int cid = g.settleBand(u);
                if (cid < 0) return;
                g.updateVisibility(g.player);
                selUnit = -1;
                openCity(cid);
                return;
            }
            if (g_units[u.type].cls == UnitClass::Settler && u.moves > 0 && g.canFoundAt(u.owner, u.x, u.y)) {
                int cid = g.foundCity(u.owner, u.x, u.y);
                g.killUnit(g.units[selUnit]);
                g.updateVisibility(g.player);
                selUnit = -1;
                openCity(cid);
                return;
            }
            return;
        case 'G': if (!g.splitBand(u)) return; break;
        case 'U': if (!g.upgradeUnit(u)) return; break;
        case 'K':
            if (u.carrier >= 0) { if (!g.leaveShip(u)) { toast = "There is no room ashore here"; toastMs = 2000; return; } }
            else if (!g.boardShip(u)) return;
            break;
        case 'R': if (!g.raiseWarParty(u)) return; break;
        case 'F': if (g_units[u.type].military()) { u.order = ORD_FORTIFY; } break;
        case 'S': u.order = ORD_SLEEP; break;
        case 'W': u.order = ORD_NONE; return;
        case ' ': u.order = ORD_SKIP; break;
        case 'E': if (g.excavateTier(u) >= 0 && u.moves > 0) g.startExcavate(u); else return; break;
        case 'X': if (g.canDecon(u) && u.moves > 0) g.startDecon(u); else return; break;
        case 'D': g.killUnit(u); selUnit = -1; break;
    }
    afterUnitAction();
}

void GameUI::clickMap(int tx, int ty) {
    Game& g = *game;
    if (nukeMode) {
        std::string why;
        if (g.canNukeAt(g.player, tx, ty, &why)) { nukeArmX = tx; nukeArmY = ty; nukeMode = false; }
        else { toast = why; toastMs = 2500; }
        return;
    }
    if (moveMode && sel()) {
        Unit& u = *sel();
        g.orderMoveOrAttack(u, tx, ty);
        g.updateVisibility(g.player);
        moveMode = false;
        if (u.alive && u.moves <= 0) afterUnitAction();
        return;
    }
    if (!g.civs[g.player].explored[g.idx(tx, ty)]) { selUnit = -1; return; }
    std::vector<int> mine;
    for (auto& u : g.units)
        if (u.alive && u.owner == g.player && u.x == tx && u.y == ty) mine.push_back(u.id);
    City* c = g.cityAt(tx, ty);
    if (!mine.empty()) {
        Unit* cur = sel();
        if (cur && cur->x == tx && cur->y == ty) {
            if (mine.size() > 1) {
                int next = mine[0] == selUnit ? mine[1] : mine[0];
                selectUnit(next, false);
                return;
            }
            if (c && c->owner == g.player) { openCity(c->id); return; }
            return;
        }
        // prefer a unit that needs orders
        int pick = mine[0];
        for (int id : mine) if (g.unitNeedsOrders(g.units[id])) { pick = id; break; }
        selectUnit(pick, false);
        return;
    }
    if (c && c->owner == g.player) { openCity(c->id); return; }
    selUnit = -1;
}

void GameUI::rightClickMap(int tx, int ty) {
    Game& g = *game;
    Unit* u = sel();
    if (!u || u->owner != g.player) return;
    g.orderMoveOrAttack(*u, tx, ty);
    g.updateVisibility(g.player);
    moveMode = false;
    if (!u->alive || u->moves <= 0) afterUnitAction();
}

void GameUI::endTurnNow() {
    Game& g = *game;
    if (g.gameOver && g.winner < 0) return;  // defeated
    overlay = overlay == OV_CITY ? OV_NONE : overlay;
    g.endTurn();
    moveMode = false;
    disbandArm = -1;
    nukeMode = false;
    nukeArmX = nukeArmY = -1;
    for (auto& b : g.blasts)
        if (g.civs[g.player].explored[g.idx(b.x, b.y)]) { startBlast(b.x, b.y); break; }
    g.blasts.clear();
    pollAchievements();
    if (g.nukeAnimPending) {
        g.nukeAnimPending = false;
        nukeT = 0;
        // look at the human city nearest to our people
        int ax = -1, ay = -1;
        for (auto& c : g.cities) if (c.alive && c.owner == g.player) { ax = c.x; ay = c.y; break; }
        if (ax < 0) for (auto& u : g.units) if (u.alive && u.owner == g.player) { ax = u.x; ay = u.y; break; }
        const HumanSite* best = nullptr;
        for (auto& s : g.sites)
            if (s.type == RUIN_CITY && (!best || g.dist(s.x, s.y, ax, ay) < g.dist(best->x, best->y, ax, ay))) best = &s;
        if (best) {
            centerOn((best->x + std::max(0, ax)) / 2, (best->y + std::max(0, ay)) / 2);
            hs = std::max(22, hs - 8);
            centerOn((best->x + std::max(0, ax)) / 2, (best->y + std::max(0, ay)) / 2);
        }
    }
    selUnit = -1;
    int n = nextUnitNeedingOrders(-1);
    if (n >= 0) selectUnit(n, false);
    if (g.turn % 5 == 0) g.save(savePath + ".autosave");
    if (!g.popups.empty() && g.popups.front().x >= 0 && nukeT < 0) centerOn(g.popups.front().x, g.popups.front().y);
}

void GameUI::saveGame() {
    if (!game) return;
    bool ok = game->save(savePath);
    toast = ok ? "Game saved" : "Could not save the game!";
    toastMs = 2000;
}

void GameUI::loadGame() {
    std::unique_ptr<Game> g(new Game());
    if (!g->load(savePath)) {
        toast = "Could not load the saved game";
        toastMs = 2500;
        return;
    }
    game = std::move(g);
    screen = SCR_GAME;
    overlay = OV_NONE;
    selUnit = selCity = -1;
    nukeT = -1;
    game->updateAllVisibility();
    Civ& me = game->civs[game->player];
    if (me.capital >= 0) centerOn(game->cities[me.capital].x, game->cities[me.capital].y);
    int n = nextUnitNeedingOrders(-1);
    if (n >= 0) selectUnit(n, me.capital < 0);
    toast = "Game loaded";
    toastMs = 2000;
}

// ============================================================ input
void GameUI::mouseDown(int x, int y, int button) {
    mouseX = x; mouseY = y;
    if (button == MOUSE_LEFT) {
        dragging = true;
        dragMoved = false;
        dragSX = x; dragSY = y;
        dragCamX = overlay == OV_TECH ? techScroll : camX;
        dragCamY = camY;
        minimapDrag = screen == SCR_GAME && game && x >= mmX && y >= mmY && x < mmX + mmW && y < mmY + mmH && overlay != OV_TECH;
    }
}

void GameUI::mouseMove(int x, int y) {
    mouseX = x; mouseY = y;
    if (screen != SCR_GAME || !game) return;
    if (dragging) {
        if (std::abs(x - dragSX) + std::abs(y - dragSY) > 6) dragMoved = true;
        if (dragMoved) {
            if (minimapDrag) {
                int tx = (int)((x - mmX) / mmScale), ty = (int)((y - mmY) / mmScale);
                centerOn(std::clamp(tx, 0, game->W - 1), std::clamp(ty, 0, game->H - 1));
            } else if (overlay == OV_TECH) {
                techScroll = (int)dragCamX - (x - dragSX);
            } else if (overlay == OV_NONE || overlay == OV_CITY) {
                camX = dragCamX - (x - dragSX);
                camY = dragCamY - (y - dragSY);
                clampCamera();
            }
        }
    }
    int tx, ty;
    if (pickTile(x, y, tx, ty)) { hoverX = tx; hoverY = ty; }
    else hoverX = hoverY = -1;
}

void GameUI::mouseUp(int x, int y, int button) {
    mouseX = x; mouseY = y;
    bool wasDrag = dragging && dragMoved;
    dragging = false;
    if (button == MOUSE_LEFT && wasDrag) return;
    if (screen == SCR_GAME && game && nukeT >= 0) {
        if (nukeT > 1200) nukeT = -1;
        return;
    }
    // UI hotspots, top-most first
    for (int i = (int)hots.size() - 1; i >= 0; i--) {
        const Hot& h = hots[i];
        if (h.wheelArea) continue;
        if (x >= h.x && y >= h.y && x < h.x + h.w && y < h.y + h.h) {
            if (button == MOUSE_LEFT && h.fn) {
                auto fn = h.fn;  // copy: the callback may rebuild state
                fn();
            }
            return;
        }
    }
    if (screen != SCR_GAME || !game || !game->popups.empty()) return;
    int tx, ty;
    if (!pickTile(x, y, tx, ty)) return;
    if (overlay == OV_CITY) {
        if (button == MOUSE_LEFT) overlay = OV_NONE;
        return;
    }
    if (overlay != OV_NONE) return;
    if (button == MOUSE_LEFT) clickMap(tx, ty);
    else if (button == MOUSE_RIGHT) rightClickMap(tx, ty);
}

void GameUI::wheel(int x, int y, int delta) {
    mouseX = x; mouseY = y;
    auto inside = [&](const Hot& h) { return x >= h.x && y >= h.y && x < h.x + h.w && y < h.y + h.h; };
    for (int i = (int)hots.size() - 1; i >= 0; i--) {
        if (!hots[i].wheelArea || !inside(hots[i])) continue;
        if (overlay == OV_TECH) techScroll -= delta * 120;
        else if (overlay == OV_LOG) logScroll -= delta * 3;
        else if (overlay == OV_HELP) helpScroll -= delta * 60;
        else if (overlay == OV_ACHIEVEMENTS) achScroll -= delta * 60;
        else listScroll -= delta * 60;
        return;
    }
    for (auto& h : hots) if (inside(h)) return;  // over some UI element
    if (screen != SCR_GAME || !game) return;
    if (overlay != OV_NONE && overlay != OV_CITY) return;
    int old = hs;
    hs = std::clamp(hs + delta * 4, 14, 64);
    if (hs == old) return;
    double wx = (x + camX) / old, wy = (y + camY) / old;
    camX = wx * hs - x;
    camY = wy * hs - y;
    clampCamera();
}

void GameUI::adjustUiScale(int dir) {
    if (dir == 0) uiScalePct = 100;
    else uiScalePct = std::max(UI_SCALE_MIN, std::min(UI_SCALE_MAX, uiScalePct + dir * UI_SCALE_STEP));
    toast = "Interface size " + std::to_string(uiScalePct) + "%";
    toastMs = 1500;
}

void GameUI::key(int k, bool shift, bool ctrl) {
    (void)shift;
    if (ctrl && (k == KEY_PLUS || k == KEY_MINUS || k == '0')) {
        adjustUiScale(k == KEY_PLUS ? 1 : k == KEY_MINUS ? -1 : 0);
        return;
    }
    if (overlay == OV_HELP && (k == KEY_DOWN || k == KEY_UP || k == KEY_PGDN || k == KEY_PGUP)) {
        helpScroll += k == KEY_DOWN ? 40 : k == KEY_UP ? -40 : k == KEY_PGDN ? 400 : -400;
        return;
    }
    if (screen == SCR_TITLE) {
        if ((overlay == OV_HELP || overlay == OV_ACHIEVEMENTS) && (k == KEY_ESC || k == KEY_ENTER)) { overlay = OV_NONE; return; }
        if (k == KEY_ENTER || k == 'N') screen = SCR_SETUP;
        if (k == 'L') loadGame();
        return;
    }
    if (screen == SCR_SETUP) {
        if (k == KEY_ESC) screen = SCR_TITLE;
        if (k == KEY_LEFT) setupPrimate = (setupPrimate + (int)g_primates.size() - 1) % (int)g_primates.size();
        if (k == KEY_RIGHT) setupPrimate = (setupPrimate + 1) % (int)g_primates.size();
        if (k == KEY_ENTER) {
            GameSettings s;
            s.primate = setupPrimate; s.mapSize = setupSize; s.difficulty = setupDiff; s.speed = setupSpeed;
            s.seed = (uint64_t)std::time(nullptr);
            startGame(s);
        }
        return;
    }
    if (!game) return;
    Game& g = *game;
    if (nukeT >= 0) {
        if (nukeT > 1200) nukeT = -1;
        return;
    }
    if (!g.popups.empty()) {
        if (k == KEY_ENTER || k == KEY_ESC || k == KEY_SPACE) closePopup();
        return;
    }
    if (ctrl && k == 'S') { saveGame(); return; }
    if (ctrl && k == 'L') { loadGame(); return; }
    if (k == KEY_ESC) {
        if (moveMode) moveMode = false;
        else if (nukeMode || nukeArmX >= 0) { nukeMode = false; nukeArmX = nukeArmY = -1; }
        else if (overlay != OV_NONE) overlay = OV_NONE;
        else overlay = OV_MENU;
        return;
    }
    double pan = 60;
    if (overlay == OV_TECH) {
        if (k == KEY_LEFT) techScroll -= 200;
        if (k == KEY_RIGHT) techScroll += 200;
        if (k == 'T') overlay = OV_NONE;
        return;
    }
    switch (k) {
        case KEY_LEFT: camX -= pan; clampCamera(); return;
        case KEY_RIGHT: camX += pan; clampCamera(); return;
        case KEY_UP: camY -= pan; clampCamera(); return;
        case KEY_DOWN: camY += pan; clampCamera(); return;
        case KEY_PLUS: wheel(W / 2, H / 2, 1); return;
        case KEY_MINUS: wheel(W / 2, H / 2, -1); return;
        case KEY_F1: overlay = OV_HELP; helpScroll = 0; return;
        case KEY_ENTER: endTurnNow(); return;
        default: break;
    }
    if (overlay == OV_CITY && (k == 'T' || k == 'P' || k == 'D' || k == 'L' || k == 'V')) overlay = OV_NONE;
    if (overlay != OV_NONE && overlay != OV_CITY) {
        if ((k == 'T' && overlay == OV_TECH) || (k == 'P' && overlay == OV_POLICY) || (k == 'D' && overlay == OV_DIPLO) || (k == 'L' && overlay == OV_LOG) ||
            (k == 'V' && overlay == OV_EVO))
            overlay = OV_NONE;
        return;
    }
    switch (k) {
        case 'T': overlay = OV_TECH; return;
        case 'P': overlay = OV_POLICY; return;
        case 'D': overlay = OV_DIPLO; return;
        case 'L': overlay = OV_LOG; logScroll = 0; return;
        case 'V': overlay = OV_EVO; return;
        case KEY_TAB: { int n = nextUnitNeedingOrders(selUnit); if (n >= 0) selectUnit(n, true); return; }
        case 'C': if (sel()) centerOn(sel()->x, sel()->y); return;
        case 'M': if (sel()) moveMode = !moveMode; return;
        case 'B': case 'F': case 'S': case 'W': case 'E': case 'X': case 'G': case 'R': case 'U': case 'K': case KEY_SPACE: unitCommand((char)k); return;
        case KEY_DELETE: if (sel()) { if (disbandArm == selUnit) unitCommand('D'); else disbandArm = selUnit; } return;
        default: return;
    }
}

// ============================================================ achievements
void GameUI::pollAchievements() {
    if (!game) return;
    size_t won = ach.primatesWon.size();
    std::vector<std::string> got = checkAchievements(*game, ach);
    if (!got.empty() || ach.primatesWon.size() != won) ach.save(achievementsPath);
    for (auto& id : got) {
        achBanners.push_back(id);
        if (achBannerMs <= 0) achBannerMs = 4500;
    }
}

void GameUI::drawTrophy(int x, int y, int s, bool lit) {
    Color c = lit ? rgb(240, 196, 70) : rgb(78, 84, 96), d = lit ? rgb(176, 130, 30) : rgb(58, 62, 72);
    int r = s * 2 / 5;
    // handles
    cv->fillCircle(x - r, y - s / 6, s / 5, d);
    cv->fillCircle(x + r, y - s / 6, s / 5, d);
    cv->fillCircle(x - r, y - s / 6, s / 9, lit ? rgb(30, 26, 20) : rgb(28, 30, 36));
    cv->fillCircle(x + r, y - s / 6, s / 9, lit ? rgb(30, 26, 20) : rgb(28, 30, 36));
    // bowl
    cv->fillRect(x - r, y - s / 2, r * 2, s / 3, c);
    cv->fillCircle(x, y - s / 6, r, c);
    // stem and base
    cv->fillRect(x - s / 14, y + s / 5, s / 7 + 1, s / 6, d);
    cv->fillRect(x - s / 4, y + s / 3, s / 2, s / 8 + 1, c);
    if (lit) cv->fillCircle(x - r / 3, y - s / 3, s / 12, rgb(255, 240, 190));
}

void GameUI::renderAchBanner() {
    if (achBannerMs <= 0 || achBanners.empty()) return;
    const AchievementDef* a = findAchievement(achBanners.front());
    if (!a) return;
    int bw = std::min(400, W - 40), bh = 74;
    int shown = 4500 - achBannerMs;
    int slide = shown < 250 ? (250 - shown) * (bw + 20) / 250 : achBannerMs < 250 ? (250 - achBannerMs) * (bw + 20) / 250 : 0;
    int x = W - bw - 14 + slide, y = (screen == SCR_GAME ? TOP : 0) + 14;
    cv->fillRectA(x + 4, y + 4, bw, bh, rgb(0, 0, 0), 120);
    cv->fillRect(x, y, bw, bh, rgb(26, 30, 38));
    cv->rect(x, y, bw, bh, C_GOLD, 2);
    drawTrophy(x + 38, y + bh / 2, 40, true);
    cv->text(x + 72, y + 9, "ACHIEVEMENT UNLOCKED", 11, C_GOLD, AL_LEFT, true);
    cv->text(x + 72, y + 24, a->name, 18, C_TEXT, AL_LEFT, true);
    cv->text(x + 72, y + 48, a->desc, 13, C_DIM);
}

void GameUI::renderAchievements() {
    int top0 = screen == SCR_GAME ? TOP + 8 : 20;
    int pw = std::min(1000, W - 40), ph = H - top0 - 20;
    int px = (W - pw) / 2, py = top0;
    hot(0, 0, W, H, nullptr);
    panel(px, py, pw, ph, rgb(24, 28, 36));
    cv->text(px + 20, py + 14, "Achievements", 26, C_GOLD, AL_LEFT, true);
    std::string count = std::to_string(ach.unlocked.size()) + " of " + std::to_string(g_achievements.size()) + " unlocked";
    cv->text(px + 220, py + 22, count, 15, C_DIM);
    button(px + pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    int top = py + 56, viewH = ph - 66;
    hots.push_back(Hot{px, top, pw, viewH, nullptr, "", true});
    int cols = pw >= 760 ? 2 : 1, colW = (pw - 40 - (cols - 1) * 16) / cols, rowH = 62;
    // lay out: group headings span the row, entries flow in columns
    int y = 0, total = 0;
    std::string group;
    int col = 0;
    std::vector<std::function<void(int)>> draws;  // draw at scrolled offset
    for (auto& a : g_achievements) {
        if (group != a.group) {
            if (col) { y += rowH; col = 0; }
            group = a.group;
            int gy = y;
            std::string gname = group;
            draws.push_back([this, px, gy, gname](int off) { cv->text(px + 20, gy - off + 8, gname, 16, C_GOLD, AL_LEFT, true); });
            y += 34;
        }
        int ex = px + 20 + col * (colW + 16), ey = y;
        const AchievementDef* ap = &a;
        draws.push_back([this, ex, ey, colW, rowH, ap](int off) {
            int yy = ey - off;
            bool lit = ach.has(ap->id);
            cv->fillRect(ex, yy, colW, rowH - 8, lit ? rgb(40, 44, 34) : rgb(32, 36, 44));
            if (lit) cv->rect(ex, yy, colW, rowH - 8, rgb(120, 100, 40), 1);
            drawTrophy(ex + 28, yy + (rowH - 8) / 2, 32, lit);
            cv->text(ex + 54, yy + 7, ap->name, 15, lit ? C_TEXT : rgb(170, 176, 186), AL_LEFT, true);
            std::string desc = ap->desc;
            if (std::string(ap->id) == "FULL_TROOP")
                desc += "  (" + std::to_string(ach.primatesWon.size()) + "/" + std::to_string(g_primates.size()) + ")";
            cv->text(ex + 54, yy + 29, desc, 12, C_DIM);
            if (lit) {
                std::string when = ach.unlocked.at(ap->id);
                cv->text(ex + colW - 10, yy + 8, when == "yes" ? "Unlocked" : when, 11, C_GOLD, AL_RIGHT);
            }
        });
        if (++col >= cols) { col = 0; y += rowH; }
    }
    if (col) y += rowH;
    total = y;
    achScroll = std::clamp(achScroll, 0, std::max(0, total - viewH + 10));
    cv->clip(px + 1, top, pw - 2, viewH);
    for (auto& d : draws) d(achScroll - top);
    cv->unclip();
    if (total > viewH) {
        int th = std::max(30, viewH * viewH / total);
        int ty = top + (viewH - th) * achScroll / std::max(1, total - viewH + 10);
        cv->fillRect(px + pw - 14, top, 6, viewH, rgb(40, 46, 58));
        cv->fillRect(px + pw - 14, ty, 6, th, rgb(120, 136, 160));
    }
}

// ============================================================ nuclear weapons
void GameUI::launchPlayerNuke() {
    Game& g = *game;
    int x = nukeArmX, y = nukeArmY;
    nukeArmX = nukeArmY = -1;
    nukeMode = false;
    if (!g.launchNuke(g.player, x, y)) return;
    g.blasts.clear();
    g.updateVisibility(g.player);
    startBlast(x, y);
    pollAchievements();
}

void GameUI::startBlast(int x, int y) {
    blastT = 0;
    blastX = x;
    blastY = y;
    centerOn(x, y);
}

void GameUI::renderNukeTargeting() {
    Game& g = *game;
    // the area that would be hit around the hovered (or chosen) tile
    int tx = nukeArmX >= 0 ? nukeArmX : hoverX, ty = nukeArmX >= 0 ? nukeArmY : hoverY;
    if (!g.inMap(tx, ty)) tx = ty = -1;
    std::string why;
    bool ok = tx >= 0 && g.canNukeAt(g.player, tx, ty, &why);
    if (tx >= 0) {
        for (auto& p : g.ring(tx, ty, 2)) {
            int sx, sy, xy[12];
            tileCenter(p.x, p.y, sx, sy);
            hexCorners(sx, sy, hs - 2, xy);
            int d = g.dist(tx, ty, p.x, p.y);
            if (ok) cv->fillPolyA(xy, 6, d <= 1 ? rgb(255, 60, 30) : rgb(255, 150, 40), d <= 1 ? 120 : 70);
            else cv->fillPolyA(xy, 6, rgb(120, 120, 120), 60);
        }
    }
    if (nukeMode) {
        std::string msg = "Choose a target for your nuclear warhead  (Esc to cancel)";
        int tw = cv->textWidth(msg, 16, true) + 40;
        cv->fillRectA((W - tw) / 2, TOP + 10, tw, 34, rgb(60, 14, 10), 230);
        cv->text(W / 2, TOP + 18, msg, 16, rgb(255, 220, 200), AL_CENTER, true);
        if (tx >= 0 && !ok) cv->text(mouseX + 18, mouseY + 14, why, 13, rgb(255, 180, 160), AL_LEFT, true);
        return;
    }
    // confirmation
    std::vector<std::string> lines;
    for (auto& ci : g.cities)
        if (ci.alive && g.dist(ci.x, ci.y, tx, ty) <= 2 && g.civs[g.player].explored[g.idx(ci.x, ci.y)])
            lines.push_back((ci.owner == g.player ? "YOUR city " : "") + ci.name + (g.dist(ci.x, ci.y, tx, ty) == 0 ? " (direct hit)" : " (blast edge)"));
    int own = 0;
    for (auto& u : g.units) if (u.alive && u.owner == g.player && g.dist(u.x, u.y, tx, ty) <= 2) own++;
    if (own) lines.push_back(std::to_string(own) + " of your own units are in the blast area!");
    int pw = 440, ph = 150 + (int)lines.size() * 20, px = (W - pw) / 2, py = H / 2 + 40;
    hot(0, 0, W, H, nullptr);
    panel(px, py, pw, ph, rgb(40, 18, 16));
    cv->text(px + pw / 2, py + 14, "Launch a nuclear warhead?", 20, rgb(255, 200, 170), AL_CENTER, true);
    int yy = py + 48;
    if (lines.empty()) lines.push_back("No cities in the blast area.");
    for (auto& l : lines) { cv->text(px + 24, yy, l, 14, C_TEXT); yy += 20; }
    cv->text(px + 24, yy + 2, "Everyone who knows you will remember this.", 13, C_DIM);
    button(px + 24, py + ph - 52, 180, 38, "Launch", true, [this] { launchPlayerNuke(); }, "", rgb(150, 40, 30), 16);
    button(px + pw - 204, py + ph - 52, 180, 38, "Cancel", true, [this] { nukeArmX = nukeArmY = -1; }, "", C_PANEL2, 16);
}

void GameUI::renderBlast() {
    int t = blastT, sx, sy;
    tileCenter(blastX, blastY, sx, sy);
    int big = hs * 3;
    double k = std::min(1.0, t / 1400.0);
    int r = (int)(big * (0.3 + k));
    if (t < 2600) {
        cv->fillCircleA(sx, sy, r * 3 / 2, rgb(255, 200, 120), (int)(90 * (1 - k)));
        cv->fillCircle(sx, sy, r, blend(rgb(255, 250, 220), rgb(130, 70, 40), (float)k));
        cv->fillCircle(sx, sy, r * 2 / 3, blend(rgb(255, 255, 255), rgb(230, 120, 40), (float)k));
        if (t > 500) {
            int rise = std::min(t - 500, 2000) * big / 1300;
            cv->fillRect(sx - big / 6, sy - rise, big / 3, rise, rgb(150, 100, 80));
            cv->fillCircle(sx, sy - rise, big * 2 / 3, rgb(170, 110, 80));
            cv->fillCircle(sx - big / 2, sy - rise + big / 6, big / 2, rgb(150, 96, 72));
            cv->fillCircle(sx + big / 2, sy - rise + big / 6, big / 2, rgb(150, 96, 72));
            cv->fillCircle(sx, sy - rise - big / 8, big / 3, rgb(240, 170, 90));
        }
    } else {
        cv->fillCircleA(sx, sy, big, rgb(150, 100, 80), (int)(200 * (3400 - t) / 800.0));
    }
    if (t < 300) cv->fillRectA(0, 0, W, H, rgb(255, 255, 255), 255 - t * 255 / 300);
    hot(0, 0, W, H, [this] { if (blastT > 900) blastT = -1; }, "");
}
