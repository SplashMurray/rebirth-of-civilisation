// Rebirth of Civilisation - city art
// A city fills most of its hex and grows denser as its people multiply. Its buildings change with its era: domed
// huts around a fire, flat-roofed mud-brick blocks under a ziggurat, gabled stone houses around a domed hall,
// brick terraces and smoking factories, then towers of glass. Each city's layout comes from its id, so it keeps
// its look from turn to turn; new houses fill in from the centre outwards as it grows.
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
uint32_t cityHash(int a, int b) {
    uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + 0x5bd1e995u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
double unitOf(uint32_t h) { return (h & 0xffff) / 65536.0; }

struct Slot { double x, y; uint32_t h; };

// Spots for buildings inside the hex, nearest the centre first; the centre itself is kept for the landmark.
std::vector<Slot> citySlots(int id) {
    std::vector<Slot> out;
    const double step = 0.25;
    for (int j = -4; j <= 4; j++)
        for (int i = -4; i <= 4; i++) {
            double x = (i + (j & 1) * 0.5) * step, y = j * step * 0.78;
            uint32_t h = cityHash(id * 97 + i, j);
            x += (unitOf(h) - 0.5) * 0.09;
            y += (unitOf(h >> 8) - 0.5) * 0.07;
            double ax = std::fabs(x), ay = std::fabs(y + 0.04);
            if (ax > 0.64 || ay > 0.66 - ax * 0.5) continue;  // keep inside a squashed hex
            if (std::hypot(x, (y + 0.1) * 1.2) < 0.24 || (std::fabs(x) < 0.2 && y > -0.1 && y < 0.24)) continue;  // the landmark's place, and the view of it
            out.push_back({x, y, h});
        }
    std::sort(out.begin(), out.end(), [](const Slot& a, const Slot& b) {
        return std::hypot(a.x, a.y * 1.25) + unitOf(a.h >> 4) * 0.12 < std::hypot(b.x, b.y * 1.25) + unitOf(b.h >> 4) * 0.12;
    });
    return out;
}
}  // namespace

// A building drawn in three-quarter view: front face, side face going back to the right, and a roof.
// roof: 0 flat, 1 gabled, 2 none (the caller adds its own)
void GameUI::cityBox(int x, int y, int w, int h, int d, Color wall, int roof, Color roofC, int windows, Color lit) {
    int dx = d * 6 / 10, dy = -d * 45 / 100;
    int l = x - w / 2, r = l + w, top = y - h;
    // side, then front
    int side[8] = {r, y, r + dx, y + dy, r + dx, top + dy, r, top};
    cv->fillPolyGrad(side, 4, darken(wall, 0.72f), 255, darken(wall, 0.55f), 255, r, top, r + dx, y);
    cv->fillRectGrad(l, top, w, h, lighten(wall, 0.08f), darken(wall, 0.88f));
    if (windows > 0 && w >= 6 && h >= 6) {
        int ws = std::max(1, w / 7), gap = std::max(3, w / 4), rows = std::max(1, h / std::max(3, gap + 1));
        for (int rr = 0; rr < rows; rr++)
            for (int wx = l + gap / 2; wx + ws <= r - 1; wx += gap) {
                int wy = top + 2 + rr * (h - 3) / rows;
                uint32_t hh = cityHash(wx * 7 + windows, wy * 3);
                bool on = (hh & 3) != 0;
                cv->fillRect(wx, wy, ws, std::max(1, ws * 3 / 2), on ? lit : darken(wall, 0.45f));
            }
    }
    if (roof == 0) {
        int tp[8] = {l, top, r, top, r + dx, top + dy, l + dx, top + dy};
        cv->fillPoly(tp, 4, lighten(wall, 0.2f));
        cv->polylineA(tp, 4, darken(wall, 0.4f), 1, 120, true);
    } else if (roof == 1) {
        int rh = std::max(2, w * 4 / 10);
        int rlx = l + dx / 2, rrx = r + dx / 2, ry = top + dy / 2 - rh;
        int gable[6] = {r, top, r + dx, top + dy, rrx, ry};
        cv->fillPoly(gable, 3, darken(wall, 0.8f));
        int slope[8] = {l - 1, top + 1, r + 1, top + 1, rrx, ry, rlx, ry};
        cv->fillPolyGrad(slope, 4, lighten(roofC, 0.12f), 255, darken(roofC, 0.62f), 255, l, ry, l, top);
        cv->lineA(rlx, ry, rrx, ry, lighten(roofC, 0.4f), 1, 160);
    }
    cv->lineA(l, y, r, y, rgb(0, 0, 0), 1, 70);
}

void GameUI::drawCityOnMap(const City& c, int sx, int sy) {
    Game& g = *game;
    int u = hs;
    Color cc = civColor(c.owner);
    int e = g.era(c.owner);
    double t = animMs / 1000.0;
    auto PXf = [&](double f) { return sx + (int)std::lround(f * u); };
    auto PYf = [&](double f) { return sy + (int)std::lround(f * u); };
    static std::vector<std::vector<Slot>> cache;
    if ((int)cache.size() <= c.id) cache.resize(c.id + 1);
    if (cache[c.id].empty()) cache[c.id] = citySlots(c.id);
    const auto& slots = cache[c.id];
    int n = std::min((int)slots.size(), 3 + c.pop * (e >= 7 ? 3 : 4) / 4);
    double reach = 0.3;
    for (int k = 0; k < n; k++) reach = std::max(reach, std::hypot(slots[k].x, slots[k].y * 1.2) + 0.12);

    // trodden ground, then paving and streets as the city matures
    Color groundC = e < 2 ? rgb(124, 98, 66) : e < 6 ? rgb(150, 132, 104) : rgb(110, 108, 106);
    cv->glowEllipse(sx, sy + u / 20, (int)(u * (reach + 0.1)), (int)(u * (reach + 0.05) * 0.8), groundC, 235);
    cv->fillEllipseA(sx, sy + u / 20, (int)(u * reach * 0.82), (int)(u * reach * 0.62), groundC, 150);
    if (e >= 2) {
        Color street = e < 6 ? rgb(176, 160, 128) : rgb(80, 80, 84);
        for (int k = 0; k < 3; k++) {
            double a = 0.5 + k * 2.094 + (c.id % 3) * 0.3;
            cv->lineA(sx, sy, PXf(std::cos(a) * reach * 0.95), PYf(std::sin(a) * reach * 0.75), street, std::max(1, u / 18), 170);
        }
    }

    // walls ring the city
    bool walls = false;
    for (size_t b = 0; b < g_buildings.size(); b++) if (c.has[b] && g_buildings[b].defense >= 4 && !g_buildings[b].wonder) walls = true;
    int wxy[12];
    hexCorners(sx, sy + u / 14, u * 78 / 100, wxy);
    for (int k = 0; k < 6; k++) wxy[k * 2 + 1] = sy + u / 14 + (wxy[k * 2 + 1] - sy - u / 14) * 8 / 10;  // flattened to sit on the ground
    auto drawWalls = [&](bool front) {
        if (!walls) return;
        Color wc = e < 3 ? rgb(128, 90, 52) : rgb(186, 178, 160);
        int wt = std::max(2, u / 10);
        for (int k = 0; k < 6; k++) {
            int a = k, b = (k + 1) % 6;
            bool isFront = wxy[a * 2 + 1] + wxy[b * 2 + 1] > 2 * (sy + u / 14);
            if (isFront != front) continue;
            int x1 = wxy[a * 2], y1 = wxy[a * 2 + 1], x2 = wxy[b * 2], y2 = wxy[b * 2 + 1];
            if (e < 3) {  // a palisade of stakes
                int nst = std::max(3, u / 6);
                for (int i = 0; i <= nst; i++) {
                    int px = x1 + (x2 - x1) * i / nst, py = y1 + (y2 - y1) * i / nst;
                    cv->line(px, py, px, py - wt * 2, darken(wc, 0.8f), std::max(2, wt * 2 / 3));
                    cv->fillCircle(px, py - wt * 2, std::max(1, wt / 3), lighten(wc, 0.2f));
                }
            } else {
                int q[8] = {x1, y1, x2, y2, x2, y2 - wt * 2, x1, y1 - wt * 2};
                cv->fillPolyGrad(q, 4, lighten(wc, 0.1f), 255, darken(wc, 0.7f), 255, x1, y1 - wt * 2, x1, y1);
                cv->polylineA(q, 4, darken(wc, 0.4f), 1, 160, true);
                int nb = std::max(2, u / 10);  // battlements
                for (int i = 0; i < nb; i++) {
                    int px = x1 + (x2 - x1) * (2 * i + 1) / (2 * nb), py = y1 + (y2 - y1) * (2 * i + 1) / (2 * nb);
                    cv->fillRect(px - wt / 3, py - wt * 2 - wt / 2, std::max(1, wt * 2 / 3), wt / 2 + 1, wc);
                }
            }
        }
        for (int k = 0; k < 6; k++) {  // corner towers
            int tx = wxy[k * 2], ty = wxy[k * 2 + 1];
            bool isFront = ty > sy + u / 14;
            if (isFront != front) continue;
            int tr = std::max(2, u / 11);
            if (e < 3) cv->line(tx, ty, tx, ty - tr * 3, rgb(96, 64, 34), std::max(2, tr));
            else {
                cityBox(tx, ty, tr * 2, tr * 3, tr, wc, 0, wc, 0, wc);
                cv->fillRect(tx - tr, ty - tr * 3 - tr / 2, tr / 2 + 1, tr / 2 + 1, wc);
                cv->fillRect(tx + tr / 2, ty - tr * 3 - tr / 2, tr / 2 + 1, tr / 2 + 1, wc);
            }
        }
    };
    drawWalls(false);

    // the buildings, back to front, with the landmark among them
    struct Piece { double x, y; int kind; uint32_t h; };  // kind: 0 house, 1 landmark
    std::vector<Piece> pieces;
    for (int k = 0; k < n; k++) pieces.push_back({slots[k].x, slots[k].y, 0, slots[k].h});
    pieces.push_back({0.0, -0.1, 1, 0});
    std::sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) { return a.y < b.y; });
    int bs = std::max(4, u * 23 / 100);  // a house's width
    Color lit = rgb(255, 222, 140);
    bool fire = e <= 1;
    for (auto& pc : pieces) {
        int bx = PXf(pc.x), by = PYf(pc.y);
        uint32_t hh = pc.h;
        cv->fillEllipseA(bx + bs / 3, by + 1, bs * 7 / 10, std::max(1, bs / 4), rgb(0, 0, 0), 60);  // shadow
        if (pc.kind == 1) {
            // ---------------------------------------------------- the landmark
            if (e <= 1) {
                // a long communal hut, a totem in the owner's colour, and the fire before them
                int r = bs * 9 / 10;
                int dxy[26];
                for (int i = 0; i <= 12; i++) {
                    double a = 3.14159265 * i / 12;
                    dxy[i * 2] = bx - (int)(std::cos(a) * r * 1.5);
                    dxy[i * 2 + 1] = by - u / 12 - (int)(std::sin(a) * r);
                }
                cv->fillPolyGrad(dxy, 13, rgb(204, 166, 104), 255, rgb(110, 78, 42), 255, bx - r, by - r, bx + r, by);
                for (int i = -2; i <= 2; i++) cv->lineA(bx + i * r / 3, by - u / 12, bx + i * r / 4, by - u / 12 - r * 9 / 10, rgb(96, 64, 30), 1, 130);
                cv->fillRect(bx - r / 5, by - u / 12 - r * 4 / 10, std::max(2, r * 2 / 5), r * 4 / 10, rgb(50, 30, 16));
                int tx = bx + r * 3 / 2 + 2;
                cv->line(tx, by, tx, by - r * 2, rgb(110, 74, 40), std::max(2, u / 22));
                cv->fillRect(tx - u / 22, by - r * 2, std::max(3, u / 11), u / 14, cc);
                continue;
            }
            if (e <= 3) {  // a stepped ziggurat with a shrine on top
                Color sand = e == 2 ? rgb(208, 178, 128) : rgb(214, 196, 158);
                int w = bs * 2, h = bs * 2 / 5;
                for (int s = 0; s < 3; s++) cityBox(bx, by - s * h, w - s * bs / 2, h, bs * 2 / 3 - s * bs / 8, sand, 0, sand, 0, lit);
                int topY = by - 3 * h;
                cityBox(bx, topY, bs * 2 / 3, bs / 2, bs / 3, lighten(sand, 0.1f), 1, cc, 0, lit);
                cv->fillRect(bx - bs / 10, topY - bs / 4, std::max(2, bs / 5), bs / 4, rgb(60, 40, 26));
                continue;
            }
            if (e <= 5) {  // a domed hall with a bell tower
                Color stone = rgb(226, 216, 196);
                cityBox(bx - bs / 4, by, bs * 3 / 2, bs * 7 / 10, bs * 2 / 3, stone, 0, stone, 1, lit);
                int domeX = bx - bs / 4 + bs / 5, domeY = by - bs * 7 / 10 - bs / 8;
                int dr = bs / 2;
                int dome[26];
                for (int i = 0; i <= 12; i++) {
                    double a = 3.14159265 * i / 12;
                    dome[i * 2] = domeX - (int)(std::cos(a) * dr);
                    dome[i * 2 + 1] = domeY - (int)(std::sin(a) * dr);
                }
                Color domeC = e == 4 ? rgb(120, 170, 150) : rgb(176, 120, 70);
                cv->fillPolyGrad(dome, 13, lighten(domeC, 0.25f), 255, darken(domeC, 0.6f), 255, domeX - dr / 2, domeY - dr, domeX + dr, domeY);
                cv->line(domeX, domeY - dr, domeX, domeY - dr - bs / 4, rgb(230, 200, 90), std::max(1, u / 40));
                int tx = bx + bs * 2 / 3;
                cityBox(tx, by + bs / 10, bs / 2, bs * 3 / 2, bs / 3, stone, 2, stone, 0, lit);
                int sp[6] = {tx - bs / 4 - 1, by + bs / 10 - bs * 3 / 2, tx + bs / 4 + bs / 5, by + bs / 10 - bs * 3 / 2 - bs / 8, tx + bs / 10, by - bs * 2};
                cv->fillPolyGrad(sp, 3, lighten(cc, 0.2f), 255, darken(cc, 0.6f), 255, tx, by - bs * 2, tx, by - bs * 3 / 2);
                if (u >= 30) cv->fillCircle(tx, by + bs / 10 - bs * 6 / 5, std::max(1, bs / 8), rgb(250, 240, 210));  // the clock
                continue;
            }
            if (e == 6) {  // a town hall with a clock tower, flags out
                Color brick = rgb(176, 104, 76);
                cityBox(bx, by, bs * 3 / 2, bs, bs * 2 / 3, brick, 0, brick, 2, lit);
                int tx = bx + bs / 8;
                cityBox(tx, by - bs, bs / 2, bs, bs / 3, rgb(196, 188, 170), 1, rgb(80, 96, 110), 0, lit);
                cv->fillCircle(tx, by - bs - bs / 2, std::max(1, bs / 7), rgb(250, 240, 210));
                continue;
            }
            // a tower of glass, its spire lit
            int h = bs * 3 + std::min(c.pop, 24) * bs / 10;
            cityBox(bx, by, bs, h, bs * 2 / 3, rgb(130, 170, 210), 0, rgb(130, 170, 210), 3, rgb(230, 246, 255));
            cv->fillRectA(bx - bs / 2, by - h, bs / 3, h, rgb(255, 255, 255), 50);
            cv->line(bx + bs / 4, by - h - bs / 3, bx + bs / 4, by - h - bs * 6 / 5, rgb(200, 210, 220), std::max(1, u / 40));
            double blink = 0.5 + 0.5 * std::sin(t * 3 + c.id);
            cv->fillCircleA(bx + bs / 4, by - h - bs * 6 / 5, std::max(1, u / 30), rgb(255, 80, 60), animOn ? (int)(120 + 130 * blink) : 220);
            continue;
        }
        // -------------------------------------------------------- houses
        double v = unitOf(hh >> 12);
        if (e <= 1) {  // domed huts of woven branches and thatch
            int r = bs / 2 + (int)(v * bs / 4);
            int dxy[26];
            for (int i = 0; i <= 12; i++) {
                double a = 3.14159265 * i / 12;
                dxy[i * 2] = bx - (int)(std::cos(a) * r);
                dxy[i * 2 + 1] = by - (int)(std::sin(a) * r * 9 / 10);
            }
            cv->fillPolyGrad(dxy, 13, rgb(200, 158, 96), 255, rgb(108, 74, 40), 255, bx - r / 2, by - r, bx + r / 2, by);
            for (int i = 1; i < 3; i++) cv->lineA(bx - r * i / 3, by, bx - r * i / 3 + r / 6, by - r * 7 / 10, rgb(90, 60, 30), 1, 120);
            cv->fillRect(bx - r / 5, by - r * 4 / 10, std::max(2, r * 2 / 5), r * 4 / 10, rgb(50, 30, 16));
        } else if (e <= 3) {  // flat-roofed mud brick, some with an upper storey
            Color wall = blend(rgb(214, 182, 130), rgb(196, 160, 112), (float)v);
            int w = bs * (8 + (int)(v * 4)) / 10, h = bs * 6 / 10;
            cityBox(bx, by, w, h, bs / 2, wall, 0, wall, 0, lit);
            cv->fillRect(bx - w / 6, by - h / 2, std::max(2, w / 4), h / 2, rgb(70, 46, 30));
            if (v > 0.6) cityBox(bx - w / 6, by - h, w / 2, h * 2 / 3, bs / 3, lighten(wall, 0.05f), 0, wall, 0, lit);
            if (v < 0.2) cv->lineA(bx + w / 2, by - h, bx + w / 2 + bs / 3, by - h - bs / 4, cc, 1, 200);  // an awning in the owner's colour
        } else if (e <= 5) {  // gabled stone and timber houses
            Color wall = e == 4 ? blend(rgb(226, 208, 170), rgb(206, 196, 180), (float)v) : blend(rgb(214, 206, 192), rgb(222, 196, 160), (float)v);
            Color roof = v < 0.15 ? cc : v < 0.6 ? rgb(176, 84, 52) : rgb(132, 72, 56);
            int w = bs * (8 + (int)(v * 4)) / 10, h = bs * (6 + (int)(v * 4)) / 10;
            cityBox(bx, by, w, h, bs / 2, wall, 1, roof, u >= 30 ? 1 : 0, lit);
            cv->fillRect(bx - w / 5, by - h * 4 / 10, std::max(2, w / 5), h * 4 / 10, rgb(80, 54, 36));
        } else if (e == 6) {  // brick terraces, chimneys, and here and there a factory
            Color brick = blend(rgb(170, 96, 70), rgb(150, 120, 100), (float)v);
            int w = bs, h = bs * (8 + (int)(v * 8)) / 10;
            bool factory = (hh % 7) == 0;
            if (factory) {
                cityBox(bx, by, bs * 3 / 2, bs * 7 / 10, bs * 2 / 3, rgb(120, 110, 104), 0, brick, 2, lit);
                int cx2 = bx + bs / 2, top = by - bs * 7 / 10 - bs;
                cv->fillRect(cx2 - bs / 10, top, std::max(2, bs / 5), bs, rgb(120, 70, 56));
                if (animOn)
                    for (int p = 0; p < 3; p++) {
                        double f = std::fmod(t * 0.35 + p / 3.0 + (hh % 13) * 0.07, 1.0);
                        int pr = (int)(u / 16 + f * u / 8);
                        cv->fillCircleA(cx2 + (int)(f * u / 3), top - (int)(f * u / 2), pr, rgb(190, 190, 196), (int)(110 * (1 - f)));
                    }
            } else {
                cityBox(bx, by, w, h, bs / 2, brick, 0, brick, u >= 30 ? 2 : 0, lit);
                cv->fillRect(bx + w / 4, by - h - bs / 4, std::max(2, bs / 6), bs / 4, rgb(110, 60, 46));
            }
        } else {  // concrete and glass, rising with the city
            Color body = (hh & 1) ? rgb(150, 178, 210) : (hh & 2) ? rgb(186, 186, 182) : rgb(120, 150, 170);
            int w = bs * (7 + (int)(v * 4)) / 10;
            double centre = 1.0 - std::min(1.0, std::hypot(pc.x, pc.y) / 0.7);
            int h = bs * (8 + (int)(v * 10)) / 10 + (int)(centre * bs * std::min(c.pop, 24) / 8);
            cityBox(bx, by, w, h, bs / 2, body, 0, body, 3 + (int)(hh % 5), rgb(255, 232, 160));
            cv->fillRectA(bx - w / 2, by - h, w / 3, h, rgb(255, 255, 255), 36);
        }
    }
    if (fire) {  // the communal fire
        double fl = animOn ? 0.75 + 0.25 * std::sin(t * 9 + c.id) : 0.9;
        int fx = sx - bs / 2, fy = sy + bs;
        cv->glowEllipse(fx, fy, (int)(u / 4 * fl), (int)(u / 6 * fl), rgb(255, 170, 60), 170);
        cv->fillCircle(fx, fy, std::max(1, u / 22), rgb(255, 220, 120));
    }
    drawWalls(true);
    if (c.capital) {
        // a banner in the civilisation's colour, fluttering over the landmark
        int fx2 = sx - bs * 6 / 5, fy2 = sy - bs / 2;
        cv->line(fx2, fy2, fx2, fy2 - u * 6 / 10, rgb(60, 40, 30), std::max(1, u / 24));
        double wv = animOn ? std::sin(t * 4 + c.id) : 0;
        int top = fy2 - u * 6 / 10;
        int fl[8] = {fx2, top, fx2 + u / 9, top + u / 36 + (int)(wv * u / 40), fx2 + u * 22 / 100, top + u / 12 + (int)(wv * u / 24), fx2, top + u / 6};
        cv->fillPolyGrad(fl, 4, lighten(cc, 0.2f), 255, darken(cc, 0.7f), 255, fx2, top, fx2 + u / 4, top + u / 5);
    }
}
