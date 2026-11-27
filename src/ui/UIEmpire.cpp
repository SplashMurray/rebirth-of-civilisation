// Rebirth of Civilisation - empire overview
// One table of every city you own: size, mood, growth, what it is producing and its yields, with totals.
// Click a column heading to sort by it (again to reverse); click a row to open the city.
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>

namespace {
const int TOP = 36;  // matches GameUI.cpp
const Color C_TEXT = {234, 238, 242};
const Color C_DIM = {150, 160, 174};
const Color C_GOLD = {240, 200, 70};
const Color C_SCI = {100, 178, 255};
const Color C_CULT = {206, 130, 236};
const Color C_FOOD = {124, 206, 92};
const Color C_PROD = {236, 148, 64};
const Color C_RED = {232, 84, 70};
const Color C_PANEL2 = {40, 48, 62};
}  // namespace

void GameUI::renderEmpire() {
    Game& g = *game;
    int p = g.player;
    int pw = std::min(1100, W - 30), ph = H - TOP - 20;
    int px = (W - pw) / 2, py = TOP + 8;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 12, "Empire Overview", 24, C_TEXT, AL_LEFT, true);
    button(px + pw - 40, py + 10, 28, 28, "X", true, [this] { overlay = OV_NONE; }, "Close (Esc)");

    std::vector<City*> cs;
    for (auto& c : g.cities)
        if (c.alive && c.owner == p) {
            g.computeCity(c);
            cs.push_back(&c);
        }
    bool gold = g.hasGold(p);
    auto routesOf = [&](const City& c) {
        int n = 0;
        for (auto& r : g.routes) if ((r.from == c.id && r.owner == c.owner) || (r.to == c.id && r.owner != c.owner)) n++;
        return n;
    };
    // columns: heading, x offset, sort key
    enum { K_NAME, K_POP, K_MOOD, K_FOOD, K_GROW, K_PROD, K_BUILD, K_SCI, K_GOLD, K_CULT, K_ROUTES, K_DEF, K_COUNT };
    auto key = [&](const City& c, int k) -> int {
        switch (k) {
            case K_POP: return c.pop;
            case K_MOOD: return c.happy - c.unhappy;
            case K_FOOD: return c.foodSurplus;
            case K_GROW: { int t = g.turnsToGrow(c); return t > 0 ? -t : -100000; }
            case K_PROD: return c.yields.prod;
            case K_BUILD: { int t = c.buildKind == BK_NONE ? 0 : g.turnsToBuild(c); return c.buildKind == BK_NONE ? 100000 : -t; }
            case K_SCI: return c.yields.sci;
            case K_GOLD: return c.yields.gold;
            case K_CULT: return c.yields.cult;
            case K_ROUTES: return routesOf(c);
            case K_DEF: return g.cityStrength(c);
            default: return 0;
        }
    };
    int sk = std::abs(empireSort) - 1;
    bool rev = empireSort < 0;
    if (sk < 0 || sk >= K_COUNT) sk = K_NAME;
    std::stable_sort(cs.begin(), cs.end(), [&](const City* a, const City* b) {
        if (sk == K_NAME) {
            if (a->capital != b->capital) return a->capital != rev;
            return rev ? a->name > b->name : a->name < b->name;
        }
        int ka = key(*a, sk), kb = key(*b, sk);
        return rev ? ka < kb : ka > kb;  // biggest first by default
    });

    struct Col { const char* h; int x; int key; Color c; const char* tip; };
    int bw = std::max(190, pw - 830);  // the "producing" column takes the slack
    int x0 = px + 20;
    std::vector<Col> cols = {
        {"City", 0, K_NAME, C_TEXT, "City name (* capital)"},
        {"Pop", 170, K_POP, C_TEXT, "Population"},
        {"Mood", 215, K_MOOD, C_TEXT, "Citizens' mood: happy minus unhappy"},
        {"Food", 270, K_FOOD, C_FOOD, "Food surplus a turn"},
        {"Grows", 335, K_GROW, C_FOOD, "Turns until the city grows"},
        {"Prod", 395, K_PROD, C_PROD, "Production a turn"},
        {"Producing", 445, K_BUILD, C_PROD, "What the city is building, and turns left"},
        {"Sci", 455 + bw, K_SCI, C_SCI, "Science a turn"},
        {"Gold", 505 + bw, K_GOLD, C_GOLD, "Gold a turn"},
        {"Cult", 555 + bw, K_CULT, C_CULT, "Culture a turn"},
        {"Routes", 605 + bw, K_ROUTES, rgb(240, 200, 110), "Trade routes from or to this city"},
        {"Defence", 670 + bw, K_DEF, C_TEXT, "City defence strength and garrison"},
    };
    if (!gold) cols.erase(std::remove_if(cols.begin(), cols.end(), [](const Col& c) { return c.key == K_GOLD; }), cols.end());

    // summary line
    const Civ& me = g.civs[p];
    int totPop = 0;
    for (auto* c : cs) totPop += c->pop;
    std::string sum = std::to_string(cs.size()) + (cs.size() == 1 ? " city" : " cities") + ", population " + std::to_string(totPop) + "   |   Science +" +
                      std::to_string(me.sciencePerTurn) + "   Culture +" + std::to_string(me.culturePerTurn);
    if (gold) sum += "   Gold " + std::string(me.goldPerTurn >= 0 ? "+" : "") + std::to_string(me.goldPerTurn) + " (" + std::to_string(me.gold) + " banked)";
    sum += "   |   Trade routes " + std::to_string(g.routesOf(p)) + "/" + std::to_string(g.tradeCapacity(p));
    cv->text(px + 20, py + 46, sum, 13, C_DIM);

    int hy = py + 74;
    cv->fillRect(px + 12, hy - 4, pw - 24, 24, rgb(40, 48, 62));
    for (auto& col : cols) {
        bool active = col.key == sk;
        std::string h = std::string(col.h) + (active ? (rev ? " ^" : " v") : "");
        cv->text(x0 + col.x, hy, h, 13, active ? C_GOLD : col.c, AL_LEFT, true);
        int k = col.key;
        int w = cv->textWidth(h, 13, true) + 8;
        hot(x0 + col.x - 4, hy - 4, w, 24, [this, k] { empireSort = empireSort == k + 1 ? -(k + 1) : k + 1; },
            std::string(col.tip) + ". Click to sort.");
    }

    int rowH = 24, top = hy + 26, listH = ph - (top - py) - 40;
    int maxScroll = std::max(0, (int)cs.size() * rowH - listH);
    empireScroll = std::clamp(empireScroll, 0, maxScroll);
    cv->clip(px, top, pw, listH);
    Hot area{px, top, pw, listH, nullptr, "", true};
    hots.push_back(area);
    if (cs.empty()) cv->text(px + pw / 2, top + 30, "You have no cities yet.", 15, C_DIM, AL_CENTER);
    for (size_t i = 0; i < cs.size(); i++) {
        City& c = *cs[i];
        int y = top + (int)i * rowH - empireScroll;
        if (y + rowH < top || y > top + listH) continue;
        if (i % 2) cv->fillRectA(px + 12, y - 3, pw - 24, rowH, rgb(255, 255, 255), 10);
        int m = g.moodOf(c);
        for (auto& col : cols) {
            int x = x0 + col.x;
            switch (col.key) {
                case K_NAME: {
                    std::string n = (c.capital ? "* " : "") + c.name;
                    while (n.size() > 3 && cv->textWidth(n, 13, true) > 160) n = n.substr(0, n.size() - 3) + "..";
                    cv->text(x, y, n, 13, C_TEXT, AL_LEFT, true);
                    break;
                }
                case K_POP: cv->text(x, y, std::to_string(c.pop), 13, C_TEXT); break;
                case K_MOOD:
                    drawMoodFace(x + 7, y + 8, 7, m);
                    cv->text(x + 18, y, (c.happy - c.unhappy >= 0 ? "+" : "") + std::to_string(c.happy - c.unhappy), 13, moodColor(m));
                    hot(x - 2, y - 3, 50, rowH, nullptr, moodTip(c));
                    break;
                case K_FOOD:
                    cv->text(x, y, (c.foodSurplus >= 0 ? "+" : "") + std::to_string(c.foodSurplus), 13, c.foodSurplus < 0 ? C_RED : C_FOOD);
                    break;
                case K_GROW: {
                    int t = g.turnsToGrow(c);
                    cv->text(x, y, t > 0 ? std::to_string(t) : c.foodSurplus < 0 ? "starve" : "-", 13, t > 0 ? C_DIM : c.foodSurplus < 0 ? C_RED : C_DIM);
                    break;
                }
                case K_PROD: cv->text(x, y, std::to_string(c.yields.prod), 13, C_PROD); break;
                case K_BUILD: {
                    if (c.buildKind == BK_NONE) {
                        cv->text(x, y, "Nothing - choose!", 13, rgb(255, 190, 110), AL_LEFT, true);
                        break;
                    }
                    std::string nm = c.buildKind == BK_UNIT ? g_units[c.buildId].name : g_buildings[c.buildId].name;
                    std::string tl = std::to_string(g.turnsToBuild(c));
                    int tw = cv->textWidth(tl, 13);
                    while (nm.size() > 3 && cv->textWidth(nm, 13) > bw - tw - 26) nm = nm.substr(0, nm.size() - 3) + "..";
                    cv->text(x, y, nm, 13, C_TEXT);
                    cv->text(x + bw - 14, y, tl, 13, C_DIM, AL_RIGHT);
                    int cost = std::max(1, g.buildCost(c.owner, c.buildKind, c.buildId));
                    cv->fillRect(x, y + 17, bw - 14, 2, rgb(50, 56, 66));
                    cv->fillRect(x, y + 17, std::min(bw - 14, (bw - 14) * c.prod / cost), 2, C_PROD);
                    break;
                }
                case K_SCI: cv->text(x, y, std::to_string(c.yields.sci), 13, C_SCI); break;
                case K_GOLD: cv->text(x, y, std::to_string(c.yields.gold), 13, C_GOLD); break;
                case K_CULT: cv->text(x, y, std::to_string(c.yields.cult), 13, C_CULT); break;
                case K_ROUTES: {
                    int n = routesOf(c);
                    cv->text(x, y, n ? std::to_string(n) : "-", 13, n ? rgb(240, 200, 110) : C_DIM);
                    break;
                }
                case K_DEF: {
                    Unit* gar = g.militaryAt(c.x, c.y);
                    cv->text(x, y, std::to_string(g.cityStrength(c) / 10), 13, C_TEXT);
                    std::string gs = gar ? g_units[gar->type].name : "no garrison";
                    while (gs.size() > 3 && cv->textWidth(gs, 12) > pw - (x - px) - 70) gs = gs.substr(0, gs.size() - 3) + "..";
                    cv->text(x + 30, y + 1, gs, 12, gar ? C_DIM : rgb(240, 160, 90));
                    break;
                }
            }
        }
        int id = c.id;
        hot(px + 12, y - 3, pw - 24, rowH, [this, id] { openCity(id); }, "");
    }
    cv->unclip();
    // totals
    int ty = py + ph - 32;
    cv->line(px + 12, ty - 6, px + pw - 12, ty - 6, rgb(88, 102, 124), 1);
    int tf = 0, tp = 0, ts = 0, tg = 0, tc = 0;
    for (auto* c : cs) { tf += c->foodSurplus; tp += c->yields.prod; ts += c->yields.sci; tg += c->yields.gold; tc += c->yields.cult; }
    for (auto& col : cols) {
        int x = x0 + col.x;
        switch (col.key) {
            case K_NAME: cv->text(x, ty, "Total", 13, C_TEXT, AL_LEFT, true); break;
            case K_POP: cv->text(x, ty, std::to_string(totPop), 13, C_TEXT, AL_LEFT, true); break;
            case K_FOOD: cv->text(x, ty, (tf >= 0 ? "+" : "") + std::to_string(tf), 13, C_FOOD, AL_LEFT, true); break;
            case K_PROD: cv->text(x, ty, std::to_string(tp), 13, C_PROD, AL_LEFT, true); break;
            case K_SCI: cv->text(x, ty, std::to_string(ts), 13, C_SCI, AL_LEFT, true); break;
            case K_GOLD: cv->text(x, ty, std::to_string(tg), 13, C_GOLD, AL_LEFT, true); break;
            case K_CULT: cv->text(x, ty, std::to_string(tc), 13, C_CULT, AL_LEFT, true); break;
            default: break;
        }
    }
    if (maxScroll > 0) cv->text(px + pw - 20, ty, "Scroll for more", 12, C_DIM, AL_RIGHT);
}

// ============================================================ hotseat handover
// Between the turns of two human players the screen is covered, so the next player does not see the last one's
// lands, cities or armies; they click when they are in the chair.
void GameUI::beginHandover() {
    handover = true;
    overlay = OV_NONE;
    selUnit = -1;
    selCity = -1;
    moveMode = false;
}

void GameUI::endHandover() {
    Game& g = *game;
    handover = false;
    resetMotion();
    // look at home: the capital, else the first unit
    int ax = -1, ay = -1;
    for (auto& c : g.cities) if (c.alive && c.owner == g.player) { ax = c.x; ay = c.y; if (c.capital) break; }
    if (ax < 0) for (auto& u : g.units) if (u.alive && u.owner == g.player) { ax = u.x; ay = u.y; break; }
    if (ax >= 0) centerOn(ax, ay);
    int n = nextUnitNeedingOrders(-1);
    if (n >= 0) selectUnit(n, false);
    if (!g.popups.empty() && g.popups.front().x >= 0) centerOn(g.popups.front().x, g.popups.front().y);
}

void GameUI::renderHandover() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    cv->fillRectGrad(0, 0, W, H, rgb(26, 30, 40), rgb(8, 10, 14));
    cv->glowEllipse(W / 2, H / 2 - 40, W / 3, H / 3, me.color, 60);
    int pn = 1;
    for (int c = FIRST_PRIMATE_CIV; c < g.player; c++) if (g.civs[c].player) pn++;
    int cy = H / 2 - 60;
    if (me.primate >= 0) drawPortrait(me.primate, W / 2, cy - 70, 54, me.color);
    cv->text(W / 2, cy + 6, "Player " + std::to_string(pn) + ": the " + me.name, 34, C_TEXT, AL_CENTER, true);
    cv->text(W / 2, cy + 52, me.leader + "  -  turn " + std::to_string(g.turn) + ", " + g.dateString(g.turn), 17, C_DIM, AL_CENTER);
    int msgs = 0;
    for (auto& n : g.log) if (n.turn >= g.turn - 1) msgs++;
    std::string news = g.popups.empty() ? "" : std::to_string(g.popups.size()) + (g.popups.size() == 1 ? " matter awaits" : " matters await") + " you";
    if (msgs) news += (news.empty() ? "" : ", ") + std::to_string(msgs) + " new message" + (msgs == 1 ? "" : "s");
    if (!news.empty()) cv->text(W / 2, cy + 82, news + ".", 15, rgb(255, 214, 120), AL_CENTER);
    cv->text(W / 2, cy + 124, "Pass the computer to the " + me.adjective + " player. Everyone else, look away!", 16, C_TEXT, AL_CENTER);
    button(W / 2 - 110, cy + 160, 220, 54, "I'm ready", true, [this] { endHandover(); }, "Begin your turn (Enter)", rgb(60, 110, 60), 20);
    button(W / 2 - 110, cy + 226, 105, 32, "Save", true, [this] { saveGame(); }, "Save the game (Ctrl+S)", C_PANEL2, 13);
    button(W / 2 + 5, cy + 226, 105, 32, "Quit", true, [this] { handover = false; game.reset(); screen = SCR_TITLE; overlay = OV_NONE; },
           "Back to the title screen (unsaved progress is lost)", C_PANEL2, 13);
}
