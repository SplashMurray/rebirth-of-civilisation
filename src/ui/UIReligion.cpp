// Rebirth of Civilisation - the Religion screen, religion symbols, and faith in the UI
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <cmath>

namespace {
const int TOP = 36;  // matches GameUI.cpp
const Color C_TEXT = {234, 238, 242};
const Color C_DIM = {150, 160, 174};
const Color C_PANEL2 = {40, 48, 62};
const Color C_SCI = {100, 178, 255};
const Color C_FAITH = {250, 232, 180};
const Color REL_COLORS[] = {{96, 196, 110}, {250, 196, 64}, {236, 96, 64}, {90, 170, 240}, {150, 120, 90},
                            {230, 80, 150}, {190, 190, 240}, {200, 120, 220}, {60, 200, 200}, {240, 240, 240}};
}  // namespace

Color GameUI::religionColor(int r) const {
    if (!game || r < 0 || r >= (int)game->religions.size()) return rgb(120, 120, 120);
    return REL_COLORS[game->religions[r].nameIdx % 10];
}

// A coloured disc with the religion's sign: a tree, a sun, a flame, a raindrop, a leafless tree, an ember, a moon,
// a standing stone, waves, a light.
void GameUI::drawReligionSymbol(int r, int cx, int cy, int rad) {
    if (!game || r < 0 || r >= (int)game->religions.size()) return;
    int k = game->religions[r].nameIdx % 10;
    Color col = REL_COLORS[k], ink = rgb(30, 26, 20);
    cv->fillCircle(cx, cy, rad, col);
    cv->circle(cx, cy, rad, rgb(20, 20, 20), 1);
    double s = rad * 0.62;
    auto P = [&](double fx, double fy, int& x, int& y) { x = cx + (int)std::lround(fx * s); y = cy + (int)std::lround(fy * s); };
    auto poly = [&](std::initializer_list<double> pts) {
        int xy[24], n = 0;
        for (auto it = pts.begin(); it != pts.end() && n < 12; n++) { double fx = *it++; double fy = *it++; P(fx, fy, xy[n * 2], xy[n * 2 + 1]); }
        cv->fillPoly(xy, n, ink);
    };
    auto disc = [&](double fx, double fy, double fr, Color c) { int x, y; P(fx, fy, x, y); cv->fillCircle(x, y, std::max(1, (int)(fr * s)), c); };
    int lw = std::max(1, rad / 6);
    auto L = [&](double ax, double ay, double bx, double by) { int x1, y1, x2, y2; P(ax, ay, x1, y1); P(bx, by, x2, y2); cv->line(x1, y1, x2, y2, ink, lw); };
    switch (k) {
        case 0: poly({0, -1, 0.75, 0.35, -0.75, 0.35}); L(0, 0.3, 0, 1); break;                  // the canopy: a tree
        case 1: disc(0, 0, 0.45, ink); for (int i = 0; i < 8; i++) { double a = i * 0.785; L(std::cos(a) * 0.6, std::sin(a) * 0.6, std::cos(a), std::sin(a)); } break;  // sun
        case 2: poly({0, -1, 0.6, 0.1, 0.4, 0.8, -0.4, 0.8, -0.6, 0.1}); break;                    // flame
        case 3: poly({0, -1, 0.6, 0.15, -0.6, 0.15}); disc(0, 0.35, 0.6, ink); break;             // raindrop
        case 4: L(0, 1, 0, -0.4); L(0, -0.1, -0.6, -0.8); L(0, -0.1, 0.6, -0.8); L(0, 0.4, 0.5, -0.1); break;  // the silent tree
        case 5: poly({0, -0.9, 0.5, 0, 0, 0.9, -0.5, 0}); break;                                   // ember
        case 6: disc(0, 0, 0.8, ink); disc(0.35, -0.2, 0.7, col); break;                          // moon
        case 7: poly({-0.35, 1, -0.45, -0.6, 0, -1, 0.45, -0.6, 0.35, 1}); break;                  // standing stone
        case 8: L(-0.9, -0.3, 0.9, -0.3); L(-0.9, 0.2, 0.9, 0.2); L(-0.9, 0.7, 0.9, 0.7); break;   // river
        default: disc(0, 0, 0.35, ink); L(0, -1, 0, 1); L(-1, 0, 1, 0); break;                     // light
    }
}

std::string GameUI::zealTip(int c) {
    Game& g = *game;
    const Civ& cv = g.civs[c];
    int share, beliefs, trad;
    int z = g.zealOf(c, &share, &beliefs, &trad);
    std::string t = "Zeal " + std::to_string(z) + (cv.zealPenalty > 0 ? ": -" + std::to_string(cv.zealPenalty) + "% Science" : ": no cost to Science") + ".\n";
    t += "  Devotion: Faith is " + std::to_string(share) + "% of your Faith and Science (Faith counted double)\n";
    if (beliefs) t += "  Beliefs of your faith: " + std::string(beliefs > 0 ? "+" : "") + std::to_string(beliefs) + "\n";
    if (trad) t += "  Free Thought: " + std::to_string(trad) + "\n";
    t += "A people devoted too heavily to its faith resists ideas that contradict it - as religion has resisted science through "
         "history, and still does. Zeal above 25 costs 3/4 of a percent of Science per point (at most 50%). Fewer houses of worship, "
         "more science, scholarly beliefs and the Free Thought tradition all temper it.";
    return t;
}

void GameUI::renderReligion() {
    Game& g = *game;
    int p = g.player;
    const Civ& me = g.civs[p];
    int pw = std::min(1180, W - 30), ph = H - TOP - 20;
    int px = (W - pw) / 2, py = TOP + 8;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 12, "Religion", 24, C_TEXT, AL_LEFT, true);
    button(px + pw - 40, py + 10, 28, 28, "X", true, [this] { overlay = OV_NONE; }, "Close (Esc)");
    int lw = std::min(560, pw / 2 - 20);
    int x = px + 20, y = py + 50;
    // ---- your faith and its cost
    cv->text(x, y, "Faith " + std::to_string(me.faith) + "  (+" + std::to_string(me.faithPerTurn) + " a turn)", 17, C_FAITH, AL_LEFT, true);
    hot(x, y - 2, 300, 22, nullptr, "Faith comes from Shrines (+2), Temples (+4), Cathedrals (+6), natural wonders your cities work (+2), holy cities (+2) "
                                     "and some beliefs. It founds and enhances religions, buys Missionaries and holy festivals, and can finish buildings "
                                     "(on the city screen).");
    y += 30;
    {
        int z = g.zealOf(p);
        int bw = lw - 10;
        cv->text(x, y, "Zeal", 14, C_TEXT, AL_LEFT, true);
        int bx = x + 50, by = y + 3;
        cv->fillRoundRectGrad(bx, by, bw - 50, 14, 4, rgb(50, 56, 66), rgb(40, 44, 52));
        int fillW = (bw - 50) * std::min(z, 100) / 100;
        Color zc = me.zealPenalty >= 20 ? rgb(232, 90, 70) : me.zealPenalty > 0 ? rgb(240, 170, 80) : rgb(130, 200, 120);
        if (fillW > 0) cv->fillRoundRectGrad(bx, by, fillW, 14, 4, zc, darken(zc, 0.7f));
        int tx = bx + (bw - 50) * 25 / 100;
        cv->line(tx, by - 3, tx, by + 17, rgb(255, 255, 255), 1);
        hot(x, y - 2, bw, 22, nullptr, zealTip(p));
        y += 22;
        std::string line = "Zeal " + std::to_string(z) + (me.zealPenalty > 0 ? " - your people resist new ideas: -" + std::to_string(me.zealPenalty) + "% Science"
                                                                              : " - no cost to Science (it starts above 25)");
        cv->text(x, y, line, 13, me.zealPenalty > 0 ? zc : C_DIM, AL_LEFT, me.zealPenalty > 0);
        hot(x, y - 2, bw, 18, nullptr, zealTip(p));
        y += 22;
    }
    // ---- actions
    {
        int r = me.founded >= 0 ? me.founded : g.stateReligion(p);
        bool canM = r >= 0 && me.faith >= g.missionaryCost(p);
        std::string mt = r < 0 ? "Your people follow no religion to spread" : "A Missionary of " + g.religions[r].name +
                         ": preaches in a city, winning many followers there at once";
        button(x, y, 196, 30, "Missionary (" + std::to_string(g.missionaryCost(p)) + ")", canM, [this] {
            std::string w;
            if (!game->buyMissionary(game->player, &w)) { toast = w; toastMs = 2500; }
            else {
                for (int i = (int)game->units.size() - 1; i >= 0; i--)
                    if (game->units[i].alive && game->units[i].owner == game->player && game->units[i].type == ID.uMissionary) { overlay = OV_NONE; selectUnit(i, true); break; }
            }
        }, mt, C_PANEL2, 13);
        std::string fw;
        bool canF = g.canHoldFestival(p, &fw);
        button(x + 204, y, 196, 30, "Holy festival (" + std::to_string(g.festivalCost(p)) + ")", canF, [this] { game->holdFestival(game->player); },
               canF ? "+2 Happiness in every city for " + std::to_string(g.dur(10)) + " turns" : fw, C_PANEL2, 13);
        y += 42;
    }
    // ---- your religion, or founding one
    cv->line(x, y, x + lw - 10, y, rgb(70, 80, 96), 1);
    y += 10;
    auto beliefList = [&](bool founder, int& pick, int yy, int maxH) {
        int n = 0;
        std::vector<int> order;  // free beliefs first; taken ones after, if there is room
        for (int b = 0; b < BL_COUNT; b++) if (g_beliefs[b].founder == founder && !g.beliefTaken(b)) order.push_back(b);
        for (int b = 0; b < BL_COUNT; b++) if (g_beliefs[b].founder == founder && g.beliefTaken(b)) order.push_back(b);
        for (int b : order) {
            bool taken = g.beliefTaken(b);
            bool on = pick == b;
            int bh = 24;
            if (yy + n * (bh + 3) + bh > maxH) break;
            int by = yy + n * (bh + 3);
            Color bc = on ? rgb(110, 90, 40) : taken ? rgb(34, 36, 42) : C_PANEL2;
            cv->fillRoundRectGrad(x, by, lw - 10, bh, 5, bc, darken(bc, 0.85f));
            std::string label = std::string(g_beliefs[b].name) + (taken ? " (taken)" : "");
            cv->text(x + 8, by + 5, label, 12, taken ? rgb(100, 104, 112) : C_TEXT, AL_LEFT, true);
            if (g_beliefs[b].zeal) {
                std::string zs = (g_beliefs[b].zeal > 0 ? "+" : "") + std::to_string(g_beliefs[b].zeal) + " zeal";
                cv->text(x + lw - 18, by + 5, zs, 12, g_beliefs[b].zeal > 0 ? rgb(240, 150, 90) : rgb(130, 210, 130), AL_RIGHT, true);
            }
            int bb = b;
            int* pp = &pick;
            hot(x, by, lw - 10, bh, taken ? std::function<void()>() : std::function<void()>([pp, bb] { *pp = bb; }), g_beliefs[b].desc);
            n++;
        }
        return yy + n * 27;
    };
    int bottom = py + ph - 14;
    if (me.founded >= 0) {
        const Religion& rel = g.religions[me.founded];
        drawReligionSymbol(me.founded, x + 14, y + 14, 14);
        cv->text(x + 36, y + 2, rel.name, 17, C_TEXT, AL_LEFT, true);
        const City& hc = g.cities[rel.holyCity];
        cv->text(x + 36, y + 24, "Holy city " + hc.name + "  -  followed by " + std::to_string(rel.followers) + (rel.followers == 1 ? " city" : " cities"), 12, C_DIM);
        y += 48;
        for (int b : rel.beliefs) {
            cv->text(x, y, std::string(g_beliefs[b].founder ? "Founder: " : "Follower: ") + g_beliefs[b].name, 13, C_FAITH, AL_LEFT, true);
            y = textBlock(x + 12, y + 18, lw - 22, g_beliefs[b].desc, 12, C_DIM, 2) + 6;
        }
        if (!rel.enhanced) {
            std::string why;
            bool can = g.canEnhanceReligion(p, &why);
            cv->text(x, y + 4, "Enhance it (" + std::to_string(g.enhanceCost()) + " Faith, Theology): one more belief of each kind", 13, can ? C_TEXT : C_DIM, AL_LEFT, true);
            y += 26;
            if (can) {
                int half = (bottom - 40 - y) / 2;
                cv->text(x, y, "Follower belief", 12, C_DIM);
                beliefList(false, relPickFollower, y + 16, y + 16 + half - 20);
                y += half;
                cv->text(x, y, "Founder belief", 12, C_DIM);
                beliefList(true, relPickFounder, y + 16, bottom - 40);
                bool ready = relPickFollower >= 0 && relPickFounder >= 0;
                button(x, bottom - 34, 220, 32, "Enhance", ready, [this] {
                    if (game->enhanceReligion(game->player, relPickFollower, relPickFounder)) { relPickFollower = relPickFounder = -1; sfx(Sfx::Culture, 0.8f); }
                }, ready ? "" : "Choose a follower belief and a founder belief", rgb(60, 110, 60), 15);
            } else cv->text(x, y, why + ".", 12, C_DIM);
        }
    } else {
        std::string why;
        bool can = g.canFoundReligion(p, &why);
        int slots = g.maxReligions() - (int)g.religions.size();
        cv->text(x, y, "Found a religion", 17, can ? C_TEXT : C_DIM, AL_LEFT, true);
        y += 24;
        y = textBlock(x, y, lw - 10, "Needs Symbols and " + std::to_string(g.foundCost()) + " Faith. This world has room for " + std::to_string(g.maxReligions()) +
                                         " religions; " + (slots > 0 ? std::to_string(slots) + " still to be founded." : "all have been founded."),
                      12, C_DIM, 3) + 4;
        if (!can) {
            textBlock(x, y, lw - 10, why + ".", 13, rgb(240, 180, 120), 3);
            if (slots <= 0)
                textBlock(x, y + 22, lw - 10, "Your Faith can still buy Missionaries of the faith your capital follows, holy festivals and buildings.", 12, C_DIM, 3);
        } else {
            // name
            cv->text(x, y, "Name", 12, C_DIM);
            y += 16;
            int nx = x, row = 0;
            for (int n = 0; n < NUM_RELIGION_NAMES; n++) {
                bool used = false;
                for (auto& r : g.religions) if (r.nameIdx == n) used = true;
                if (used) continue;
                std::string nm = RELIGION_NAMES[n];
                int w = cv->textWidth(nm, 11, true) + 14;
                if (nx + w > x + lw - 10) { nx = x; row++; if (row >= 3) break; }
                int nn = n;
                button(nx, y + row * 26, w, 23, nm, true, [this, nn] { relPickName = nn; }, "", relPickName == n ? rgb(110, 90, 40) : C_PANEL2, 11);
                nx += w + 4;
            }
            y += (row + 1) * 26 + 6;
            int half = (bottom - 40 - y) / 2;
            cv->text(x, y, "Follower belief - for every city that follows it", 12, C_DIM);
            beliefList(false, relPickFollower, y + 16, y + 16 + half - 20);
            y += half;
            cv->text(x, y, "Founder belief - for your people", 12, C_DIM);
            beliefList(true, relPickFounder, y + 16, bottom - 40);
            bool ready = relPickName >= 0 && relPickFollower >= 0 && relPickFounder >= 0;
            button(x, bottom - 34, 220, 32, "Found religion", ready, [this] {
                if (game->foundReligion(game->player, relPickName, relPickFollower, relPickFounder)) {
                    relPickName = relPickFollower = relPickFounder = -1;
                    sfx(Sfx::Culture, 0.9f);
                }
            }, ready ? "" : "Choose a name, a follower belief and a founder belief", rgb(60, 110, 60), 15);
        }
    }
    // ---- the world's religions
    int rx = px + 20 + lw + 20, rw = pw - (rx - px) - 20;
    int ry = py + 50;
    cv->text(rx, ry, "Religions of the world", 17, C_TEXT, AL_LEFT, true);
    ry += 28;
    if (g.religions.empty()) cv->text(rx, ry, "No religion has been founded yet.", 13, C_DIM);
    // how many of your cities follow each
    std::vector<int> mine(g.religions.size(), 0);
    int none = 0;
    for (auto& ci : g.cities)
        if (ci.alive && ci.owner == p) { if (ci.religion >= 0) mine[ci.religion]++; else none++; }
    for (size_t r = 0; r < g.religions.size(); r++) {
        const Religion& rel = g.religions[r];
        int ch = 52 + (int)rel.beliefs.size() * 16;
        if (ry + ch > bottom) break;
        cv->fillRoundRectGrad(rx, ry, rw, ch, 6, rgb(38, 44, 58), rgb(28, 32, 42));
        cv->roundRect(rx, ry, rw, ch, 6, darken(religionColor((int)r), 0.7f), 1);
        drawReligionSymbol((int)r, rx + 22, ry + 22, 13);
        cv->text(rx + 44, ry + 6, rel.name, 15, C_TEXT, AL_LEFT, true);
        bool known = g.civs[p].met[rel.founder] || rel.founder == p;
        std::string who = rel.founder == p ? "your people" : known ? "the " + g.civs[rel.founder].name : "a people you have not met";
        cv->text(rx + 44, ry + 26, "Founded by " + who + ", turn " + std::to_string(rel.founded) + ".  " + std::to_string(rel.followers) + " cities follow it (" +
                                       std::to_string(mine[r]) + " of yours).",
                 12, C_DIM);
        int by = ry + 46;
        for (int b : rel.beliefs) {
            std::string s = std::string(g_beliefs[b].founder ? "Founder: " : "Follower: ") + g_beliefs[b].name;
            cv->text(rx + 44, by, s, 12, g_beliefs[b].zeal > 0 ? rgb(240, 170, 110) : rgb(214, 220, 228));
            hot(rx + 40, by - 2, rw - 50, 16, nullptr, g_beliefs[b].desc);
            by += 16;
        }
        ry += ch + 8;
    }
    if (!g.religions.empty() && ry + 20 < bottom)
        textBlock(rx, ry + 4, rw, std::to_string(none) + " of your cities follow no religion.  Religions spread from city to city (within 10 tiles, and along trade routes).", 12, C_DIM, 3);
}
