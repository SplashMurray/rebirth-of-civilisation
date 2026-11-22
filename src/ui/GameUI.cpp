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
    button(bx, by + 116, bw, 46, "How to Play", true, [this] { overlay = OV_HELP; }, "", rgb(70, 60, 50), 18);
    button(bx, by + 174, bw, 46, "Exit", true, [this] { quit = true; }, "", rgb(70, 60, 50), 18);
    cv->text(W - 12, H - 22, "Open source - MIT licence", 12, rgb(170, 200, 170), AL_RIGHT);
    if (overlay == OV_HELP) renderHelp();
}

// ============================================================ setup
void GameUI::renderSetup() {
    cv->fillRect(0, 0, W, H, rgb(18, 22, 28));
    cv->text(W / 2, 24, "Choose Your Primate", 34, C_TEXT, AL_CENTER, true);
    cv->text(W / 2, 66, "Each primate has its own strengths. Humans are not playable - they will not survive 2034.", 15, C_DIM, AL_CENTER);
    int cols = 4, gap = 14;
    int cw = std::min(330, (W - 80 - gap * (cols - 1)) / cols), ch = 236;
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
        int ty = textBlock(x + 12, y + 92, cw - 24, p.abilityDesc + (p.evoPct ? " Evolves +" + std::to_string(p.evoPct) + "% faster." : ""), 13, C_TEXT, 3);
        textBlock(x + 12, ty + 6, cw - 24, p.flavor, 12, C_DIM, 2);
        hot(x, y, cw, ch, [this, i] { setupPrimate = i; });
    }
    int oy = gy + 2 * (ch + gap) + 10;
    const char* sizes[] = {"Small (4 primates)", "Standard (6 primates)", "Large (8 primates)"};
    const char* diffs[] = {"Easy", "Normal", "Hard"};
    cv->text(gx, oy + 8, "Map size:", 16, C_TEXT, AL_LEFT, true);
    for (int i = 0; i < 3; i++)
        button(gx + 110 + i * 200, oy, 190, 34, sizes[i], true, [this, i] { setupSize = i; }, "",
               setupSize == i ? rgb(110, 90, 40) : C_PANEL2, 14);
    cv->text(gx, oy + 52, "Difficulty:", 16, C_TEXT, AL_LEFT, true);
    for (int i = 0; i < 3; i++)
        button(gx + 110 + i * 200, oy + 44, 190, 34, diffs[i], true, [this, i] { setupDiff = i; }, "",
               setupDiff == i ? rgb(110, 90, 40) : C_PANEL2, 14);
    button(W - 260 - gx + 20, oy + 4, 220, 70, "Begin", true, [this] {
        GameSettings s;
        s.primate = setupPrimate;
        s.mapSize = setupSize;
        s.difficulty = setupDiff;
        s.seed = (uint64_t)std::time(nullptr) ^ ((uint64_t)animMs << 20);
        startGame(s);
    }, "", rgb(60, 110, 60), 24);
    button(20, H - 50, 120, 36, "Back", true, [this] { screen = SCR_TITLE; });
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
        if (u.alive && u.owner == game->player && g_units[u.type].cls == UnitClass::Settler) first = u.id;
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
        default: break;
    }
    renderTopBar();
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
        date += g.turn <= PROLOGUE_TURNS ? g.dateString(g.turn) : std::to_string(g.yearAD(g.turn) - 2034) + " AF";
    }
    int dw = cv->textWidth(date, 14, true);
    cv->text(bx - 12, 10, date, 14, g.warHappened ? C_TEXT : rgb(255, 150, 120), AL_RIGHT, true);
    hot(bx - 12 - dw, 0, dw, TOP, nullptr, g.dateString(g.turn));
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
    item((compact ? "Sci +" : "Science +") + std::to_string(me.sciencePerTurn), C_SCI, "Science per turn. Science is used to research new technologies.",
         [this] { overlay = OV_TECH; });
    if (me.evoStage < (int)g_traits.size())
        item((compact ? "Evo " : "Evolution ") + std::to_string(me.evolution) + "/" + std::to_string(g_traits[me.evoStage].cost) + " (+" +
                 std::to_string(g.evolutionRate(g.player)) + ")",
             rgb(130, 220, 150), "Next evolutionary trait: " + g_traits[me.evoStage].name + " - " + g_traits[me.evoStage].effect + ".\n" + pressureTip(g.player),
             [this] { overlay = OV_EVO; });
    else
        item(compact ? "Evolved" : "Fully evolved", rgb(130, 220, 150), "Your species has gained every evolutionary trait.", [this] { overlay = OV_EVO; });
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
        if (!any && me.evoStage < (int)g_traits.size()) rs = "Awaiting " + g_traits[me.evoStage].name + " (" + std::to_string(me.scienceBank) + " science stored)";
    }
    double frac = me.research >= 0 ? (double)me.techProgress[me.research] / g_techs[me.research].cost : 0;
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
    if (hoverX < 0 || !g.civs[g.player].explored[g.idx(hoverX, hoverY)]) return;
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
    else stats = d.cls == UnitClass::Settler ? "Founds settlements" : "Builds improvements, excavates ruins";
    cv->text(px + 62, py + 36, stats, 13, C_DIM);
    cv->text(px + 62, py + 54, "Moves " + std::to_string(u.moves) + "/" + std::to_string(g.maxMoves(u)) + "    HP " + std::to_string(u.hp) + "    " +
             orderName(u.order) + (u.order == ORD_BUILD || u.order == ORD_EXCAVATE || u.order == ORD_DECON ? " (" + std::to_string(u.work) + " turns)" : ""),
             13, u.moves > 0 ? C_TEXT : C_DIM);
    bar(px + 62, py + 72, 200, 5, u.hp / 100.0, u.hp > 50 ? rgb(90, 210, 90) : C_RED);
    // actions
    struct Act { std::string label; bool en; std::function<void()> fn; std::string tip; };
    std::vector<Act> acts;
    int uid = u.id;
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
    if (d.military()) acts.push_back({"Fortify (F)", u.order != ORD_FORTIFY, [this] { unitCommand('F'); }, "+25% defence after one turn. Heals while resting."});
    acts.push_back({"Sleep (S)", u.order != ORD_SLEEP, [this] { unitCommand('S'); }, "Stay put until woken or an enemy appears"});
    if (u.order != ORD_NONE) acts.push_back({"Wake (W)", true, [this] { unitCommand('W'); }, "Cancel current orders"});
    acts.push_back({moveMode ? "Pick target..." : "Move (M)", u.moves > 0, [this] { moveMode = !moveMode; }, "Then left-click a destination. Right-click also moves."});
    acts.push_back({"Skip (Space)", true, [this] { unitCommand(' '); }, "Do nothing this turn"});
    acts.push_back({disbandArm == uid ? "Confirm?" : "Disband", true, [this, uid] {
        if (disbandArm == uid) { unitCommand('D'); }
        else disbandArm = uid;
    }, "Delete this unit"});
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
        int need = e == 0 ? -1 : e == 1 ? TR_SPEECH : e == 2 ? TR_ADV_SPEECH : TR_ABSTRACT;
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
            int left = td.cost - me.techProgress[t];
            info = std::to_string(td.cost) + " science  (" + std::to_string((left + sp - 1) / sp) + " turns)";
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
        if (cur) bar(bx + 1, by + boxH - 4, boxW - 2, 3, (double)me.techProgress[t] / td.cost, C_SCI);
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
std::string GameUI::pressureTip(int c) {
    std::string s = "Evolutionary pressures this turn:";
    for (auto& p : game->evolutionPressures(c)) s += "\n  +" + std::to_string(p.value) + " " + p.name;
    if (const PrimateDef* pd = game->primate(c))
        if (pd->evoPct) s += "\n  +" + std::to_string(pd->evoPct) + "% " + pd->plural + " aptitude";
    return s;
}

void GameUI::renderEvolution() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    int pw = std::min(1040, W - 40), ph = std::min(640, H - TOP - 30);
    int px = (W - pw) / 2, py = TOP + 14;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 14, "Evolution", 26, rgb(130, 220, 150), AL_LEFT, true);
    button(px + pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    textBlock(px + 20, py + 50, pw - 40,
              "Your species is still evolving. Pressures on survival reshape brain, throat and society, earning Evolution points. "
              "Without Speech there can be no teaching, and without Advanced Speech no real science.", 13, C_DIM, 3);
    // trait chain
    int n = (int)g_traits.size();
    int gap = 8, cw = (pw - 40 - gap * (n - 1)) / n, ch = 200;
    int y = py + 100;
    for (int i = 0; i < n; i++) {
        const TraitDef& t = g_traits[i];
        int x = px + 20 + i * (cw + gap);
        bool has = i < me.evoStage, cur = i == me.evoStage;
        Color bg = has ? rgb(36, 80, 52) : cur ? rgb(44, 60, 52) : rgb(32, 34, 42);
        cv->fillRect(x, y, cw, ch, bg);
        cv->rect(x, y, cw, ch, has ? rgb(110, 200, 130) : cur ? rgb(200, 240, 200) : rgb(70, 74, 86), cur ? 2 : 1);
        if (i + 1 < n) cv->line(x + cw, y + ch / 2, x + cw + gap, y + ch / 2, rgb(110, 140, 120), 2);
        int ty = textBlock(x + 8, y + 8, cw - 16, t.name, 14, has ? rgb(200, 250, 210) : C_TEXT, 2, true);
        cv->text(x + 8, ty + 2, has ? "Gained" : std::to_string(t.cost) + " points", 11, has ? rgb(130, 220, 150) : C_DIM);
        textBlock(x + 8, ty + 22, cw - 16, t.effect, 12, i == TR_SPEECH || i == TR_ADV_SPEECH || i == TR_ABSTRACT ? C_GOLD : rgb(210, 220, 230), 2);
        if (cur) {
            bar(x + 8, y + ch - 20, cw - 16, 8, (double)me.evolution / t.cost, rgb(110, 210, 130));
            int left = t.cost - me.evolution, rate = std::max(1, g.evolutionRate(g.player));
            cv->text(x + 8, y + ch - 38, std::to_string(me.evolution) + "/" + std::to_string(t.cost) + " - " + std::to_string((left + rate - 1) / rate) + " turns", 11, C_TEXT);
        }
        hot(x, y, cw, ch, nullptr, t.name + ": " + t.flavor);
    }
    // pressures
    y += ch + 24;
    cv->text(px + 20, y, "Evolutionary pressures: +" + std::to_string(g.evolutionRate(g.player)) + " points per turn", 16, C_TEXT, AL_LEFT, true);
    y += 28;
    for (auto& p : g.evolutionPressures(g.player)) {
        cv->text(px + 30, y, "+" + std::to_string(p.value), 14, rgb(130, 220, 150), AL_LEFT, true);
        cv->text(px + 70, y, p.name, 14, C_TEXT, AL_LEFT, true);
        cv->text(px + 260, y, p.why, 13, C_DIM);
        y += 22;
        if (y > py + ph - 30) break;
    }
    if (const PrimateDef* pd = g.primate(g.player))
        if (pd->evoPct && y <= py + ph - 30) cv->text(px + 30, y + 4, "+" + std::to_string(pd->evoPct) + "% " + pd->plural + " evolve faster than most primates", 13, C_GOLD);
}

// ============================================================ menu, help, log
void GameUI::renderMenu() {
    int pw = 320, ph = 360, px = (W - pw) / 2, py = (H - ph) / 2;
    hot(0, 0, W, H, [this] { overlay = OV_NONE; });
    panel(px, py, pw, ph);
    cv->text(px + pw / 2, py + 16, "Menu", 24, C_TEXT, AL_CENTER, true);
    int y = py + 60;
    auto b = [&](const std::string& l, std::function<void()> fn) { button(px + 30, y, pw - 60, 40, l, true, fn, "", C_PANEL2, 16); y += 48; };
    b("Resume", [this] { overlay = OV_NONE; });
    b("Save Game (Ctrl+S)", [this] { saveGame(); overlay = OV_NONE; });
    b("Load Game (Ctrl+L)", [this] { loadGame(); });
    b("How to Play (F1)", [this] { overlay = OV_HELP; });
    b("Main Menu", [this] { overlay = OV_NONE; screen = SCR_TITLE; });
    b("Exit Game", [this] { quit = true; });
}

void GameUI::renderHelp() {
    int pw = std::min(960, W - 40), ph = std::min(680, H - 40);
    int px = (W - pw) / 2, py = (H - ph) / 2;
    hot(0, 0, W, H, nullptr);
    panel(px, py, pw, ph, rgb(24, 28, 36));
    cv->text(px + 20, py + 14, "How to Play", 26, C_GOLD, AL_LEFT, true);
    button(px + pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    const char* text =
        "THE STORY  The game begins in 2032, shortly before humanity wipes itself out in a global nuclear war in August 2034. "
        "You lead a band of primates. Survive the Fall, then build a new civilisation from the ashes.\n"
        "WINNING  Rebirth victory: research Rebirth and complete the Rebirth Project. Domination: control every original primate capital. "
        "Cultural: adopt every Tradition. Otherwise the highest score wins when time runs out.\n"
        "EVOLUTION  Your species starts without language, so only the most basic discoveries - Fire, Foraging, Stone Tools, Cooking, Woodcraft - "
        "are possible. Evolutionary pressures (survival, group living, crowding, conflict, predators, harsh climate, the nuclear winter and even "
        "radiation) earn Evolution points that unlock traits in order: Expressive Gestures, Proto-Speech, Speech, Advanced Speech, Abstract Thought "
        "and beyond. Speech is required for Tribal technologies, Advanced Speech for Ancient ones and Abstract Thought for everything after. "
        "Science earned while you wait is stored, not lost. Press V to see the Evolution screen.\n"
        "THE RUINS  The Fall leaves ruined cities, towns, bases, labs and power plants. Each holds up to four layers of salvage - Scrap, Books, "
        "Machines and Data - that only become usable once you research Salvaging, Deciphering, Reverse Engineering and Computing. Move a Gatherer "
        "onto a ruin and press Excavate. Orange dots under a ruin are layers you can dig now. Ruins inside your borders also add Production and Science.\n"
        "FALLOUT  Radioactive tiles (yellow symbols) damage units and produce nothing. Fallout fades with time; Radiology lets Gatherers clean it.\n"
        "RESOURCES  Technologies reveal resources on the map: Copper (Digging), Tin (Bronze Working), Horses (Animal Taming), Iron (Smelting), "
        "Coal (Steam Power), Oil (Combustion), Uranium (Radiology) and Gold Ore (Currency). Strategic resources inside your borders unlock units and "
        "buildings - Copper plus Tin for the Bronze Phalanx, Horses for riders, Iron (or salvaged Scrap Metal) for Iron Blades, Coal for Factories, "
        "Oil for tanks. Wheat, Rice and Maize are crops: build a Farm on them for extra food. Build Pastures on Horses and Mines on minerals.\n"
        "GOLD  Your people only learn the value of money with the Currency technology. From then on cities earn Gold, used to buy items.\n"
        "CITIES  Found settlements with an Elder Troop. Click a city to choose what it builds. Cities grow with surplus food and claim land with culture.\n"
        "COMBAT  Move a military unit onto an enemy to attack. Ranged units attack from a distance. Hills, forest, jungle and fortifying help defence. "
        "Capture cities by reducing their HP to zero with a melee unit. Beware the feral packs that roam the wastelands.\n"
        "CONTROLS  Left-click: select.  Right-click: move / attack.  Drag or arrow keys: scroll.  Wheel or +/-: zoom.  Enter: end turn.  "
        "Space: skip unit.  Tab: next unit.  B: found city.  F: fortify.  S: sleep.  E: excavate.  X: decontaminate.  M: move mode.  C: centre on unit.  "
        "T: technologies.  V: evolution.  P: traditions.  D: diplomacy.  L: log.  Ctrl+S / Ctrl+L: save / load.  Esc: menu / close.";
    textBlock(px + 20, py + 56, pw - 40, text, 14, C_TEXT, 5);
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
    int ph = 110 + (int)lines.size() * 21;
    if (p.kind == POP_VICTORY || p.kind == POP_DEFEAT) ph += 30 + (int)(g.civs.size() - FIRST_PRIMATE_CIV) * 20;
    if (p.kind == POP_NUKE) ph = std::max(ph, 330);
    int px = (W - pw) / 2, py = std::max(TOP + 10, (H - ph) / 2);
    Color head = p.kind == POP_NUKE ? rgb(150, 50, 30) : p.kind == POP_WAR ? rgb(140, 40, 40) : p.kind == POP_VICTORY ? rgb(150, 120, 40) :
                 p.kind == POP_DEFEAT ? rgb(80, 40, 40) : p.kind == POP_RUIN ? rgb(130, 84, 40) : p.kind == POP_TECH ? rgb(40, 80, 130) : p.kind == POP_EVOLUTION ? rgb(40, 110, 70) : rgb(56, 70, 92);
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
            if (g_units[u.type].cls == UnitClass::Settler && u.moves > 0 && g.canFoundAt(u.owner, u.x, u.y)) {
                int cid = g.foundCity(u.owner, u.x, u.y);
                g.killUnit(g.units[selUnit]);
                g.updateVisibility(g.player);
                selUnit = -1;
                openCity(cid);
                return;
            }
            return;
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

void GameUI::key(int k, bool shift, bool ctrl) {
    (void)shift;
    if (screen == SCR_TITLE) {
        if (overlay == OV_HELP && (k == KEY_ESC || k == KEY_ENTER)) { overlay = OV_NONE; return; }
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
            s.primate = setupPrimate; s.mapSize = setupSize; s.difficulty = setupDiff;
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
        case KEY_F1: overlay = OV_HELP; return;
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
        case 'B': case 'F': case 'S': case 'W': case 'E': case 'X': case KEY_SPACE: unitCommand((char)k); return;
        case KEY_DELETE: if (sel()) { if (disbandArm == selUnit) unitCommand('D'); else disbandArm = selUnit; } return;
        default: return;
    }
}
