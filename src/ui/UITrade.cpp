// Rebirth of Civilisation - the caravan screen: choosing a trade route
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>

namespace {
const int TOP = 36;  // matches GameUI.cpp
const Color C_TEXT = {234, 238, 242};
const Color C_DIM = {150, 160, 174};
}  // namespace

// "+12 gold, +2 science": only what a route actually brings
std::string GameUI::tradeYieldStr(const Yields& y) const {
    std::string out;
    auto add = [&](int v, const char* what) {
        if (v <= 0) return;
        out += std::string(out.empty() ? "" : ", ") + "+" + std::to_string(v) + " " + what;
    };
    add(y.gold, "gold"); add(y.sci, "science"); add(y.food, "food"); add(y.prod, "production"); add(y.cult, "culture");
    return out.empty() ? "nothing yet" : out;
}

std::string GameUI::fmtMoves(int m) const {
    if (m <= 0) return "0";
    if (m % MP == 0) return std::to_string(m / MP);
    int tenths = (m * 10 + MP / 2) / MP;
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string GameUI::tradeTip() const {
    const Game& g = *game;
    int c = g.player;
    std::string tip = "Trade routes: " + std::to_string(g.routesOf(c)) + " running, " + std::to_string(g.tradeUsed(c) - g.routesOf(c)) +
                      " caravans waiting, room for " + std::to_string(g.tradeCapacity(c)) +
                      ".\nTrain Caravans in your cities and send them to trade. More routes come with Navigation, Banking, Industrialisation, Computing and "
                      "every two Markets.";
    int shown = 0;
    for (auto& r : g.routes) {
        if (r.owner != c || shown >= 8) continue;
        tip += "\n" + g.cities[r.from].name + " - " + g.cities[r.to].name + ": " + tradeYieldStr(g.routeYields(r, true)) + " (" +
               std::to_string(std::max(0, r.until - g.turn)) + " turns)";
        shown++;
    }
    return tip;
}

void GameUI::openRoutes(int unitId) {
    routeUnit = unitId;
    routeOpts = game->tradeOptions(game->units[unitId]);
    routeHover = -1;
    routeScroll = 0;
    overlay = OV_ROUTES;
}

// The list of cities a caravan can trade with, beside the map so the hovered route shows on it.
void GameUI::renderRoutes() {
    Game& g = *game;
    if (routeUnit < 0 || routeUnit >= (int)g.units.size() || !g.units[routeUnit].alive || g_units[g.units[routeUnit].type].cls != UnitClass::Trader) {
        overlay = OV_NONE;
        return;
    }
    const Unit& u = g.units[routeUnit];
    const City* here = g.cityAt(u.x, u.y);
    if (!here) { overlay = OV_NONE; return; }
    int pw = std::min(540, W - 40), px = W - pw - 12, py = TOP + 10, ph = H - TOP - 200;
    panel(px, py, pw, ph);
    Hot area{px, py, pw, ph, nullptr, "", true};
    hots.push_back(area);
    cv->text(px + 18, py + 14, "Caravan from " + here->name, 22, C_TEXT, AL_LEFT, true);
    cv->text(px + 18, py + 44,
             "Routes last " + std::to_string(g.dur(30)) + " turns, then the caravan comes home. In use: " + std::to_string(g.routesOf(u.owner)) + " of " +
                 std::to_string(g.tradeCapacity(u.owner)) + ".",
             13, C_DIM);
    button(px + pw - 40, py + 12, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    int rowH = 64, top = py + 70, rows = std::max(1, (ph - 80) / rowH);
    routeScroll = std::clamp(routeScroll, 0, std::max(0, (int)routeOpts.size() - rows));
    if (routeOpts.empty()) cv->text(px + 18, top + 10, "No cities within reach that you know of.", 15, C_DIM);
    int newHover = -1;
    for (int i = routeScroll, n = 0; i < (int)routeOpts.size() && n < rows; i++, n++) {
        const RouteOption& o = routeOpts[i];
        const City& b = g.cities[o.city];
        int y = top + n * rowH;
        bool hv = over(px + 8, y, pw - 16, rowH - 4);
        if (hv) newHover = i;
        cv->fillRoundRectGrad(px + 8, y, pw - 16, rowH - 4, 6, hv ? rgb(52, 60, 76) : rgb(34, 40, 52), hv ? rgb(36, 42, 54) : rgb(26, 30, 40));
        cv->fillCircle(px + 26, y + 18, 8, civColor(b.owner));
        std::string who = b.owner == u.owner ? "your city" : g.civs[b.owner].name;
        cv->text(px + 42, y + 8, b.name + "  (" + who + ", size " + std::to_string(b.pop) + ")", 15, C_TEXT, AL_LEFT, true);
        if (o.ok) {
            std::string way = (o.sea ? "By sea, " : "Overland, ") + std::to_string(o.length) + " tiles";
            way += o.sea ? ", +" + std::to_string(o.roadPct) + "% for the sea" : o.roadPct > 0 ? ", +" + std::to_string(o.roadPct) + "% for roads" : ", no roads yet";
            cv->text(px + 42, y + 27, way, 12, C_DIM);
            std::string earn = "You get " + tradeYieldStr(o.origin);
            if (b.owner != u.owner) earn += ";  they get " + tradeYieldStr(o.dest);
            cv->text(px + 42, y + 41, earn, 12, rgb(240, 210, 120));
            int oc = o.city;
            bool sea = o.sea;
            button(px + pw - 98, y + 14, 80, 28, "Trade", true,
                   [this, oc, sea] {
                       Unit& cu = game->units[routeUnit];
                       game->establishRoute(cu, oc, sea);
                       overlay = OV_NONE;
                       routeHover = -1;
                       afterUnitAction();
                   },
                   "Start this trade route. The caravan sets off at once.", rgb(110, 90, 30), 13);
        } else {
            cv->text(px + 42, y + 30, o.why, 12, rgb(220, 140, 110));
        }
    }
    routeHover = newHover;
    if ((int)routeOpts.size() > rows) cv->text(px + pw / 2, py + ph - 22, "Scroll for more", 12, C_DIM, AL_CENTER);
}
