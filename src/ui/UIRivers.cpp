// Rebirth of Civilisation - drawing rivers, lakes, terrain features and natural wonders
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
inline uint32_t hashXY(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
const int EDGE[6][2] = {{0, 1}, {5, 0}, {4, 5}, {3, 4}, {2, 3}, {1, 2}};  // hex corners on each side
const Color RIVER = {62, 128, 178};
const Color RIVER_BANK = {58, 74, 46};
const Color RIVER_SHEEN = {176, 218, 240};
void tri(Canvas* cv, int x0, int y0, int x1, int y1, int x2, int y2, Color c) {
    int p[6] = {x0, y0, x1, y1, x2, y2};
    cv->fillPoly(p, 3, c);
}
}  // namespace

#define PX(fx) (sx + (fx) * u / 10)
#define PY(fy) (sy + (fy) * u / 10)

static int riverWidth(int hs, int size) {
    static const int K[4] = {0, 13, 19, 26};
    return std::max(2 + size / 2, hs * K[std::clamp(size, 0, 3)] / 100);
}

// A river crossing its tile: a curve from each tributary's edge through the middle to the edge it flows out of.
void GameUI::drawRiver(int x, int y, int sx, int sy) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    if (!t.river || hs < 6) return;
    int u = hs;
    int xy[12];
    hexCorners(sx, sy, hs, xy);
    auto mid = [&](int d, int& mx, int& my) {
        int a = EDGE[d][0], b = EDGE[d][1];
        mx = (xy[a * 2] + xy[b * 2]) / 2;
        my = (xy[a * 2 + 1] + xy[b * 2 + 1]) / 2;
    };
    auto sizeOf = [&](int d) {
        Pt n;
        return g.neighborDir(x, y, d, n) ? (int)g.at(n.x, n.y).riverSize : 1;
    };
    if (g.isWater(x, y)) {  // a river mouth: fresh water fanning out into the sea or lake
        for (int d = 0; d < 6; d++) {
            if (!(t.river & (1 << d))) continue;
            int mx, my;
            mid(d, mx, my);
            int w = riverWidth(hs, sizeOf(d));
            int ex = mx + (sx - mx) * 35 / 100, ey = my + (sy - my) * 35 / 100;
            cv->lineA(mx, my, ex, ey, RIVER, w * 3 / 2, 120);
            cv->glowEllipse(ex, ey, w * 2, w * 3 / 2, lighten(RIVER, 0.3f), 60);
        }
        return;
    }
    uint32_t h = hashXY(x * 7 + 3, y * 11 + 5);
    int cx = sx + ((int)(h % 7) - 3) * u / 36, cy = sy + ((int)((h >> 3) % 7) - 3) * u / 36;
    int out = t.riverOut;
    int wOut = riverWidth(hs, t.riverSize);
    int ox = cx, oy = cy;
    if (out >= 0) mid(out, ox, oy);
    struct Seg { int ax, ay, wa; };
    Seg ins[6];
    int nIn = 0;
    for (int d = 0; d < 6; d++) {
        if (d == out || !(t.river & (1 << d))) continue;
        Seg s;
        mid(d, s.ax, s.ay);
        s.wa = riverWidth(hs, sizeOf(d));
        ins[nIn++] = s;
    }
    bool spring = nIn == 0;
    if (spring) {  // the source: a spring a little way back from the middle
        Seg s;
        s.ax = cx + (cx - ox) / 4;
        s.ay = cy + (cy - oy) / 4;
        s.wa = std::max(1, wOut / 2);
        ins[nIn++] = s;
    }
    auto curve = [&](const Seg& s, int pass) {
        const int S = std::max(4, std::min(10, hs / 4));
        int px = s.ax, py = s.ay;
        for (int i = 1; i <= S; i++) {
            double k = i / (double)S, m = 1 - k;
            int qx = (int)std::lround(m * m * s.ax + 2 * m * k * cx + k * k * ox);
            int qy = (int)std::lround(m * m * s.ay + 2 * m * k * cy + k * k * oy);
            int w = (int)std::lround(s.wa + (wOut - s.wa) * k);
            if (pass == 0) cv->lineA(px, py, qx, qy, RIVER_BANK, w + std::max(2, u / 9), 95);
            else if (pass == 1) cv->lineA(px, py, qx, qy, RIVER, w, 255);
            else if (w >= 4) cv->lineA(px - w / 5, py - w / 5, qx - w / 5, qy - w / 5, RIVER_SHEEN, std::max(1, w / 4), 110);
            px = qx;
            py = qy;
        }
    };
    for (int pass = 0; pass < 3; pass++)
        for (int k = 0; k < nIn; k++) curve(ins[k], pass);
    if (spring) {
        cv->fillCircleA(ins[0].ax, ins[0].ay, std::max(2, wOut * 2 / 3), RIVER_BANK, 110);
        cv->fillCircle(ins[0].ax, ins[0].ay, std::max(1, wOut / 2), lighten(RIVER, 0.15f));
    }
}

void GameUI::drawVolcanoCone(int sx, int sy, bool great) {
    int u = hs * (great ? 12 : 10) / 10;
    cv->glowEllipse(PX(0), PY(5), u * 8 / 10, u / 4, rgb(0, 0, 0), 120);
    int base = PY(5), top = PY(-5), half = u * 7 / 10, crater = u * 2 / 10;
    int lit[8] = {PX(0) - half, base, PX(0) - crater, top, PX(0), top, PX(0), base};
    int shade[8] = {PX(0), top, PX(0) + crater, top, PX(0) + half, base, PX(0), base};
    Color rock = great ? rgb(74, 58, 54) : rgb(82, 74, 70);
    cv->fillPolyGrad(lit, 4, lighten(rock, 0.22f), 255, rock, 255, PX(-2), top, PX(-5), base);
    cv->fillPolyGrad(shade, 4, darken(rock, 0.7f), 255, darken(rock, 0.45f), 255, PX(2), top, PX(5), base);
    // the crater's glow and lava running down
    cv->fillEllipseA(PX(0), top, crater, std::max(2, crater / 3), rgb(40, 20, 16), 255);
    cv->glowEllipse(PX(0), top, crater * 2, crater, rgb(255, 120, 40), great ? 200 : 150);
    cv->fillEllipseA(PX(0), top, crater * 2 / 3, std::max(1, crater / 4), rgb(255, 170, 60), 230);
    int lw = std::max(1, u / 22);
    int l1[8] = {PX(0) - crater / 2, top + 1, PX(-1), PY(-2), PX(-3), PY(1), PX(-3), PY(3)};
    cv->polylineA(l1, 4, rgb(255, 110, 30), lw, 210);
    if (great) {
        int l2[8] = {PX(0) + crater / 2, top + 1, PX(2), PY(-1), PX(2), PY(2), PX(4), PY(4)};
        cv->polylineA(l2, 4, rgb(255, 130, 40), lw, 210);
    }
}

// Marshes, oases, floodplains, reefs and volcanoes.
void GameUI::drawFeature(int x, int y, int sx, int sy) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    if (t.feature == F_NONE || hs < 10) return;
    int u = hs;
    uint32_t h = hashXY(x * 5 + 1, y * 3 + 7);
    int lw = std::max(1, u / 20);
    switch (t.feature) {
        case F_FLOODPLAIN: drawFloodplain(x, y, sx, sy); break;
        case F_MARSH: drawMarsh(x, y, sx, sy); break;
        case F_OASIS: {
            cv->fillEllipseA(PX(0), PY(2), u * 4 / 10, u / 5, rgb(214, 190, 120), 200);  // damp sand
            cv->fillCircleGrad(PX(0), PY(2), u * 3 / 10, rgb(90, 180, 200), 255, rgb(30, 100, 150), 255, PX(-1), PY(1));
            cv->fillEllipseA(PX(0), PY(2), u * 3 / 10, u / 7, rgb(40, 120, 160), 255);
            cv->lineA(PX(-2), PY(1), PX(1), PY(1), rgb(220, 245, 255), lw, 150);
            for (int k = 0; k < 2; k++) {  // palms
                int px2 = PX(k ? 4 : -4), py2 = PY(k ? -3 : -1);
                cv->line(px2, py2 + u / 4, px2 + (k ? -u / 20 : u / 20), py2, rgb(120, 86, 46), std::max(1, u / 18));
                for (int f = 0; f < 5; f++) {
                    double a = -2.7 + f * 0.6;
                    cv->lineA(px2, py2, px2 + (int)(std::cos(a) * u / 5), py2 + (int)(std::sin(a) * u / 8) + u / 18, rgb(46, 140, 60), std::max(2, u / 16), 255);
                }
            }
            break;
        }
        case F_REEF:
            cv->glowEllipse(PX(0), PY(0), u * 6 / 10, u * 4 / 10, rgb(110, 220, 210), 110);
            for (int k = 0; k < 7; k++) {
                static const Color CORAL[4] = {{240, 120, 130}, {250, 170, 90}, {180, 110, 200}, {250, 220, 140}};
                int cx = PX((int)((h >> (k * 3)) % 11) - 5), cy = PY((int)((h >> (k * 3 + 2)) % 9) - 4);
                cv->fillCircleA(cx, cy, std::max(1, u / 16 + (int)((h >> k) & 1) * u / 40), CORAL[(h >> (k * 2)) % 4], 200);
            }
            break;
        case F_VOLCANO: drawVolcanoCone(sx, sy, false); break;
        default: break;
    }
}

// The one-of-a-kind places.
void GameUI::drawNatural(int x, int y, int sx, int sy) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    if (t.natural < 0) return;
    int u = hs;
    uint32_t h = hashXY(x, y);
    int lw = std::max(1, u / 18);
    switch (g_naturals[t.natural].site) {
        case WS_FALLS: {  // a cliff across the river, white water plunging over it
            int ox = sx, oy = sy;
            if (t.riverOut >= 0) {
                int xy[12];
                hexCorners(sx, sy, hs, xy);
                int a = EDGE[t.riverOut][0], b = EDGE[t.riverOut][1];
                ox = (xy[a * 2] + xy[b * 2]) / 2;
                oy = (xy[a * 2 + 1] + xy[b * 2 + 1]) / 2;
            }
            double dx = ox - sx, dy = oy - sy, len = std::max(1.0, std::sqrt(dx * dx + dy * dy));
            double nx = -dy / len, ny = dx / len;  // across the river
            int lx = sx + (int)(nx * u * 0.55), ly = sy + (int)(ny * u * 0.55), rx = sx - (int)(nx * u * 0.55), ry = sy - (int)(ny * u * 0.55);
            int fx = (int)(dx / len * u * 0.18), fy = (int)(dy / len * u * 0.18);
            int cliff[8] = {lx, ly, rx, ry, rx + fx, ry + fy + u / 8, lx + fx, ly + fy + u / 8};
            cv->fillPolyGrad(cliff, 4, rgb(120, 104, 88), 255, rgb(58, 48, 40), 255, sx, sy - u / 10, sx + fx, sy + fy + u / 8);
            int w = riverWidth(hs, t.riverSize) + u / 10;
            for (int k = -1; k <= 1; k++) {
                int bx = sx + (int)(nx * k * w / 2.5), by = sy + (int)(ny * k * w / 2.5);
                cv->lineA(bx, by, bx + fx, by + fy + u / 8, rgb(240, 250, 255), std::max(2, w / 3), 230);
            }
            cv->glowEllipse(sx + fx, sy + fy + u / 6, w * 3 / 2, w, rgb(255, 255, 255), 200);
            break;
        }
        case WS_FLAT: {  // the elder baobab
            cv->glowEllipse(PX(1), PY(6), u * 6 / 10, u / 6, rgb(0, 0, 0), 120);
            int trunk[10] = {PX(-2), PY(6), PX(-3), PY(1), PX(-1), PY(-3), PX(1), PY(-3), PX(3), PY(1)};
            int trunk2[8] = {PX(3), PY(1), PX(2), PY(6), PX(-2), PY(6), PX(-3), PY(1)};
            cv->fillPolyGrad(trunk, 5, rgb(170, 140, 110), 255, rgb(110, 86, 64), 255, PX(-2), PY(0), PX(3), PY(0));
            cv->fillPolyGrad(trunk2, 4, rgb(150, 120, 92), 255, rgb(96, 74, 56), 255, PX(-2), PY(3), PX(3), PY(3));
            for (int k = -2; k <= 2; k++) {  // stubby branches holding up a flat crown
                cv->line(PX(k / 2), PY(-3), PX(k * 3), PY(-5 - std::abs(k) % 2), rgb(120, 94, 70), std::max(2, u / 14));
                cv->fillEllipseA(PX(k * 3) + u / 20, PY(-6 - std::abs(k) % 2) + u / 12, u / 5, u / 9, rgb(30, 70, 30), 120);
                cv->fillCircleGrad(PX(k * 3), PY(-6 - std::abs(k) % 2), u / 5, rgb(140, 200, 96), 255, rgb(50, 110, 46), 255, PX(k * 3 - 1), PY(-7));
            }
            break;
        }
        case WS_HILLS: {  // a cave mouth and a spray of crystals
            Color rock = rgb(126, 116, 100);
            cv->fillEllipseA(PX(0), PY(3), u * 6 / 10, u * 4 / 10, darken(rock, 0.8f), 255);
            cv->fillEllipseA(PX(-1), PY(1), u * 5 / 10, u * 3 / 10, lighten(rock, 0.15f), 255);
            cv->fillEllipseA(PX(0), PY(4), u / 4, u / 7, rgb(24, 20, 30), 255);
            static const Color CR[3] = {{120, 220, 240}, {190, 140, 240}, {220, 250, 255}};
            const int C[5][3] = {{-3, -1, 5}, {-1, -3, 7}, {1, -2, 6}, {3, 0, 4}, {0, 0, 4}};
            for (int k = 0; k < 5; k++) {
                int bx = PX(C[k][0]), by = PY(C[k][1] + 3), tall = C[k][2] * u / 10, w = std::max(2, u / 14);
                Color c = CR[(h >> k) % 3];
                tri(cv, bx - w, by, bx + (k - 2) * u / 40, by - tall, bx + w, by, c);
                tri(cv, bx, by, bx + (k - 2) * u / 40, by - tall, bx + w, by, darken(c, 0.7f));
            }
            cv->glowEllipse(PX(0), PY(-1), u / 2, u / 3, rgb(170, 220, 255), 90);
            break;
        }
        case WS_REEF:  // rings of coral round a turquoise lagoon
            cv->glowEllipse(PX(0), PY(0), u * 8 / 10, u * 6 / 10, rgb(110, 230, 220), 170);
            for (int k = 0; k < 14; k++) {
                static const Color CORAL[5] = {{244, 112, 128}, {252, 168, 80}, {176, 104, 210}, {252, 224, 130}, {240, 90, 160}};
                double a = k * 0.449 + (h & 7) * 0.1;
                int rr = u * (4 + (int)(k % 3)) / 12;
                cv->fillCircleA(sx + (int)(std::cos(a) * rr), sy + (int)(std::sin(a) * rr * 0.7), std::max(2, u / 12), CORAL[k % 5], 230);
            }
            break;
        case WS_SALT: {  // a white crust cracked into polygons
            int xy[12];
            hexCorners(sx, sy, hs * 8 / 10, xy);
            cv->fillPolyA(xy, 6, rgb(242, 240, 232), 235);
            cv->glowEllipse(PX(-2), PY(-2), u / 2, u / 4, rgb(170, 210, 250), 120);  // the sky mirrored
            for (int k = 0; k < 6; k++) {
                int ax = PX((int)((h >> (k * 3)) % 11) - 5), ay = PY((int)((h >> (k * 3 + 1)) % 9) - 4);
                cv->lineA(ax, ay, ax + u / 7, ay + u / 14, rgb(190, 184, 170), lw, 200);
                cv->lineA(ax, ay, ax - u / 14, ay + u / 8, rgb(190, 184, 170), lw, 200);
            }
            break;
        }
        case WS_COLD: {  // steaming turquoise pools in the snow
            const int P[3][3] = {{-3, 1, 3}, {2, -1, 4}, {1, 4, 2}};
            for (auto& p : P) {
                cv->fillEllipseA(PX(p[0]), PY(p[1]), u * p[2] / 12 + u / 20, u * p[2] / 24 + u / 30, rgb(200, 190, 170), 255);
                cv->fillEllipseA(PX(p[0]), PY(p[1]), u * p[2] / 12, u * p[2] / 24, rgb(80, 200, 210), 255);
            }
            cv->glowEllipse(PX(0), PY(-3), u * 6 / 10, u * 4 / 10, rgb(255, 255, 255), 150);
            break;
        }
        case WS_PEAK: drawVolcanoCone(sx, sy, true); break;
        case WS_MESA: {  // a flat-topped tower of banded red rock
            cv->glowEllipse(PX(1), PY(6), u * 7 / 10, u / 5, rgb(0, 0, 0), 120);
            int body[8] = {PX(-6), PY(5), PX(-4), PY(-3), PX(4), PY(-3), PX(6), PY(5)};
            cv->fillPolyGrad(body, 4, rgb(196, 104, 64), 255, rgb(120, 56, 36), 255, PX(-4), PY(0), PX(5), PY(0));
            for (int k = 0; k < 3; k++) {
                int yy = PY(-1 + k * 2);
                cv->lineA(PX(-5) + k * u / 30, yy, PX(5) - k * u / 30, yy, rgb(230, 160, 110), std::max(1, u / 20), 150);
            }
            int topp[8] = {PX(-4), PY(-3), PX(-2), PY(-4), PX(3), PY(-4), PX(4), PY(-3)};
            cv->fillPoly(topp, 4, rgb(150, 150, 80));
            break;
        }
    }
}

// Names of the natural wonders you have found, so they read as places on the map.
void GameUI::drawNaturalLabels(int x0, int x1, int y0, int y1) {
    if (hs < 20) return;
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    for (auto& s : g.naturals) {
        if (s.x < x0 || s.x > x1 || s.y < y0 || s.y > y1 || !me.explored[g.idx(s.x, s.y)]) continue;
        if (g.cityAt(s.x, s.y)) continue;
        int sx, sy;
        tileCenter(s.x, s.y, sx, sy);
        int size = std::max(10, hs / 4);
        const std::string& name = g_naturals[s.type].name;
        int w = cv->textWidth(name, size, true) + 10;
        int ly = sy + hs * 6 / 10;
        cv->fillRoundRectGrad(sx - w / 2, ly, w, size + 6, 4, rgb(40, 34, 22), rgb(24, 20, 14), 190);
        cv->roundRect(sx - w / 2, ly, w, size + 6, 4, rgb(214, 182, 110), 1, 200);
        cv->text(sx, ly + 3, name, size, rgb(250, 232, 180), AL_CENTER, true);
    }
}

// Movement on the map's features: water sliding down rivers, volcano smoke, spray and steam.
void GameUI::drawLiveFeatures(int x0, int x1, int y0, int y1) {
    if (!animOn || hs < 14) return;
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    double tm = animMs / 1000.0;
    int u = hs;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            if (!me.visible[i]) continue;
            const Tile& t = g.tiles[i];
            int sx, sy;
            tileCenter(x, y, sx, sy);
            uint32_t h = hashXY(x * 13 + 5, y * 17 + 3);
            if (g.hasRiver(x, y) && t.riverOut >= 0 && hs >= 18) {  // glints carried downstream
                int xy[12];
                hexCorners(sx, sy, hs, xy);
                int a = EDGE[t.riverOut][0], b = EDGE[t.riverOut][1];
                int ox = (xy[a * 2] + xy[b * 2]) / 2, oy = (xy[a * 2 + 1] + xy[b * 2 + 1]) / 2;
                int w = riverWidth(hs, t.riverSize);
                for (int k = 0; k < 2; k++) {
                    double f = std::fmod(tm * 0.35 + k * 0.5 + (h & 15) / 16.0, 1.0);
                    int px = sx + (int)((ox - sx) * f), py = sy + (int)((oy - sy) * f);
                    int dx = (ox - sx) / 8, dy = (oy - sy) / 8;
                    int alpha = (int)(160 * std::sin(f * 3.14159265));
                    if (alpha > 10) cv->lineA(px - dx / 2, py - dy / 2, px + dx / 2, py + dy / 2, RIVER_SHEEN, std::max(1, w / 3), alpha);
                }
            }
            bool smoke = t.feature == F_VOLCANO || (t.feature == F_WONDER && t.natural >= 0 && g_naturals[t.natural].site == WS_PEAK);
            bool steam = t.feature == F_WONDER && t.natural >= 0 && g_naturals[t.natural].site == WS_COLD;
            bool spray = t.feature == F_WONDER && t.natural >= 0 && g_naturals[t.natural].site == WS_FALLS;
            if (smoke || steam) {
                int top = smoke ? PY(-6) : PY(-1);
                for (int k = 0; k < 4; k++) {
                    double f = std::fmod(tm * (smoke ? 0.12 : 0.25) + k * 0.25 + (h & 7) / 8.0, 1.0);
                    int px = sx + (int)(std::sin(f * 4 + k) * u / 10) + (int)(f * u / 4), py = top - (int)(f * u * 0.9);
                    int r = u / 10 + (int)(f * u / 4);
                    int alpha = (int)((smoke ? 150 : 120) * (1 - f) * std::min(1.0, f * 6));
                    cv->fillCircleA(px, py, r, smoke ? rgb(96, 90, 88) : rgb(240, 244, 248), alpha);
                }
            }
            if (spray) {
                for (int k = 0; k < 3; k++) {
                    double f = std::fmod(tm * 0.8 + k / 3.0, 1.0);
                    cv->fillCircleA(sx + (int)((k - 1) * u / 8), sy + u / 6 - (int)(f * u / 5), u / 12 + (int)(f * u / 10), rgb(255, 255, 255),
                                    (int)(120 * (1 - f)));
                }
            }
        }
}
