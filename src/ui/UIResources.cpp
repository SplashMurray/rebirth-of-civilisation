// Rebirth of Civilisation - resource icons and how resources change the look of their tile
// Every resource has its own pictogram on a medallion whose rim shows its kind (bonus, crop, strategic,
// luxury), and the tile itself shows what is there: golden wheat rows, rice paddies, grazing horses,
// shoals of fish, rust-red iron outcrops, black oil seeps, vineyards, beehives and so on.
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {
inline uint32_t hashRes(int x, int y) {
    uint32_t h = (uint32_t)x * 2246822519u + (uint32_t)y * 3266489917u + 374761393u;
    h = (h ^ (h >> 15)) * 668265263u;
    return h ^ (h >> 13);
}
void tri(Canvas* cv, int x0, int y0, int x1, int y1, int x2, int y2, Color c) {
    int p[6] = {x0, y0, x1, y1, x2, y2};
    cv->fillPoly(p, 3, c);
}
}  // namespace

Color GameUI::resourceKindColor(int kind) const {
    switch (kind) {
        case RK_STRATEGIC: return rgb(214, 112, 70);
        case RK_LUXURY: return rgb(236, 196, 74);
        case RK_CROP: return rgb(176, 206, 84);
        default: return rgb(110, 190, 120);
    }
}

// The pictogram for a resource on its medallion, centred on (cx, cy) with radius r.
void GameUI::drawResourceIcon(int res, int cx, int cy, int r, bool badge) {
    if (res <= R_NONE || res >= R_COUNT) return;
    const ResourceDef& rd = g_resources[res];
    if (badge) {
        cv->fillCircleA(cx + std::max(1, r / 8), cy + std::max(1, r / 6), r + 1, rgb(0, 0, 0), 100);  // shadow
        cv->fillCircle(cx, cy, r + 1, rgb(14, 14, 16));
        cv->fillCircleGrad(cx, cy, r, lighten(resourceKindColor(rd.kind), 0.15f), 255, darken(resourceKindColor(rd.kind), 0.55f), 255, cx - r / 2, cy - r / 2);
        cv->fillCircleGrad(cx, cy, r * 4 / 5, rgb(66, 70, 80), 255, rgb(22, 24, 30), 255, cx - r / 3, cy - r / 2);
    }
    double s = r * 0.78;  // the glyph's own radius
    auto X = [&](double f) { return cx + (int)std::lround(f * s); };
    auto Y = [&](double f) { return cy + (int)std::lround(f * s); };
    auto R = [&](double f) { return std::max(1, (int)std::lround(f * s)); };
    auto poly = [&](std::initializer_list<double> pts, Color c) {
        int xy[48], n = 0;
        auto it = pts.begin();
        while (it != pts.end() && n < 24) { double fx = *it++; double fy = *it++; xy[n * 2] = X(fx); xy[n * 2 + 1] = Y(fy); n++; }
        cv->fillPoly(xy, n, c);
    };
    auto line = [&](double x0, double y0, double x1, double y1, Color c, double w) { cv->lineA(X(x0), Y(y0), X(x1), Y(y1), c, R(w), 255); };
    Color col = rd.color, dark = darken(col, 0.55f), lite = lighten(col, 0.35f);
    switch (res) {
        case R_FRUIT:
            cv->fillCircleGrad(X(-0.28), Y(0.15), R(0.36), rgb(250, 120, 90), 255, rgb(170, 40, 30), 255, X(-0.4), Y(0.0));
            cv->fillCircleGrad(X(0.26), Y(0.2), R(0.33), rgb(255, 200, 80), 255, rgb(200, 110, 20), 255, X(0.15), Y(0.05));
            line(0.0, -0.15, 0.1, -0.55, rgb(110, 80, 40), 0.08);
            cv->fillEllipseA(X(0.32), Y(-0.48), R(0.24), R(0.11), rgb(90, 180, 70), 255);
            break;
        case R_FISH:
            poly({0.35, 0.0, 0.8, -0.35, 0.72, 0.0, 0.8, 0.35}, darken(col, 0.8f));
            cv->fillEllipseA(X(-0.1), Y(0.0), R(0.55), R(0.3), col, 255);
            cv->fillEllipseA(X(-0.1), Y(0.08), R(0.45), R(0.14), lite, 255);
            cv->fillCircle(X(-0.42), Y(-0.07), R(0.07), rgb(20, 30, 40));
            line(-0.05, -0.28, 0.2, -0.05, darken(col, 0.7f), 0.06);
            break;
        case R_GAME:
            cv->fillEllipseA(X(0.0), Y(0.28), R(0.24), R(0.36), col, 255);
            cv->fillEllipseA(X(0.0), Y(0.52), R(0.12), R(0.1), rgb(40, 30, 20), 255);
            for (int side = -1; side <= 1; side += 2) {
                line(side * 0.12, -0.02, side * 0.42, -0.62, lite, 0.09);
                line(side * 0.3, -0.36, side * 0.62, -0.42, lite, 0.08);
                line(side * 0.36, -0.5, side * 0.18, -0.75, lite, 0.07);
                cv->fillEllipseA(X(side * 0.3), Y(0.05), R(0.14), R(0.07), col, 255);
            }
            break;
        case R_STONE:
            for (auto b : {std::array<double, 2>{-0.58, 0.02}, std::array<double, 2>{0.02, 0.02}, std::array<double, 2>{-0.28, -0.42}}) {
                poly({b[0], b[1], b[0] + 0.56, b[1], b[0] + 0.56, b[1] + 0.42, b[0], b[1] + 0.42}, rgb(178, 178, 172));
                poly({b[0], b[1], b[0] + 0.56, b[1], b[0] + 0.5, b[1] + 0.08, b[0] + 0.06, b[1] + 0.08}, rgb(222, 222, 216));
                line(b[0] + 0.56, b[1], b[0] + 0.56, b[1] + 0.42, rgb(110, 110, 104), 0.06);
            }
            break;
        case R_IRON:
        case R_COPPER:
        case R_TIN: {  // ingots
            Color c1 = res == R_IRON ? rgb(150, 160, 176) : res == R_COPPER ? rgb(222, 130, 72) : rgb(200, 208, 214);
            int n = res == R_TIN ? 3 : 2;
            for (int k = 0; k < n; k++) {
                double oy = (n == 3 ? 0.42 : 0.28) - k * (n == 3 ? 0.36 : 0.48), ox = (k % 2) * 0.1 - 0.05;
                poly({ox - 0.55, oy + 0.16, ox + 0.55, oy + 0.16, ox + 0.4, oy - 0.16, ox - 0.4, oy - 0.16}, darken(c1, 0.7f));
                poly({ox - 0.4, oy - 0.16, ox + 0.4, oy - 0.16, ox + 0.32, oy - 0.3, ox - 0.32, oy - 0.3}, lighten(c1, 0.25f));
                line(ox - 0.3, oy - 0.02, ox + 0.2, oy - 0.02, lighten(c1, 0.4f), 0.05);
            }
            break;
        }
        case R_HORSES:
            poly({-0.1, -0.62, 0.05, -0.75, 0.12, -0.5, 0.45, -0.15, 0.55, 0.08, 0.42, 0.18, 0.1, 0.0, 0.05, 0.62, -0.48, 0.62, -0.36, 0.0, -0.3, -0.42}, col);
            poly({-0.1, -0.62, -0.3, -0.42, -0.36, 0.0, -0.48, 0.62, -0.58, 0.55, -0.52, -0.05, -0.38, -0.5}, rgb(90, 56, 30));  // mane
            cv->fillCircle(X(0.12), Y(-0.3), R(0.06), rgb(30, 20, 10));
            cv->fillCircle(X(0.46), Y(0.06), R(0.04), rgb(40, 25, 15));
            break;
        case R_OIL:
            cv->fillCircleGrad(X(0.0), Y(0.2), R(0.42), rgb(90, 90, 100), 255, rgb(8, 8, 10), 255, X(-0.2), Y(0.0));
            poly({-0.37, 0.05, 0.0, -0.68, 0.37, 0.05}, rgb(30, 30, 34));
            cv->fillEllipseA(X(-0.15), Y(0.05), R(0.08), R(0.16), rgb(170, 180, 220), 200);
            break;
        case R_SCRAP: {
            int xy[32];
            for (int k = 0; k < 16; k++) {
                double a = k * 3.14159265 / 8, rr = k % 2 ? 0.42 : 0.62;
                xy[k * 2] = X(std::cos(a) * rr);
                xy[k * 2 + 1] = Y(std::sin(a) * rr);
            }
            cv->fillPoly(xy, 16, rgb(176, 104, 60));
            cv->fillCircle(cx, cy, R(0.34), rgb(150, 86, 48));
            cv->fillCircle(cx, cy, R(0.16), rgb(30, 30, 34));
            break;
        }
        case R_COAL:
            poly({-0.6, 0.45, -0.5, 0.0, -0.15, -0.1, 0.05, 0.3, -0.1, 0.55}, rgb(44, 44, 48));
            poly({-0.1, 0.0, 0.05, -0.5, 0.45, -0.45, 0.6, 0.0, 0.35, 0.3}, rgb(56, 56, 62));
            poly({0.1, 0.35, 0.4, 0.2, 0.65, 0.45, 0.4, 0.62}, rgb(36, 36, 40));
            line(-0.45, 0.1, -0.2, 0.0, rgb(150, 150, 160), 0.05);
            line(0.1, -0.42, 0.4, -0.38, rgb(150, 150, 160), 0.05);
            break;
        case R_URANIUM:
            cv->glowEllipse(cx, cy, R(0.8), R(0.8), rgb(140, 255, 90), 170);
            drawTrefoil(cx, cy, R(0.5));
            break;
        case R_GOLD:
            cv->fillCircleGrad(X(0.12), Y(0.12), R(0.48), rgb(255, 236, 140), 255, rgb(190, 130, 20), 255, X(-0.1), Y(-0.1));
            cv->fillCircleGrad(X(-0.2), Y(-0.15), R(0.48), rgb(255, 240, 150), 255, rgb(200, 140, 20), 255, X(-0.4), Y(-0.4));
            cv->circle(X(-0.2), Y(-0.15), R(0.32), rgb(170, 110, 20), 1);
            line(-0.32, -0.32, -0.18, -0.4, rgb(255, 255, 230), 0.06);
            break;
        case R_WHEAT:
            line(0.0, 0.7, 0.0, -0.55, rgb(200, 160, 60), 0.07);
            for (int k = 0; k < 4; k++) {
                double yy = -0.45 + k * 0.24;
                for (int side = -1; side <= 1; side += 2) {
                    int ex = X(side * 0.14), ey = Y(yy);
                    cv->fillEllipseA(ex, ey, R(0.12), R(0.2), rgb(240, 200, 90), 255);
                }
            }
            cv->fillEllipseA(X(0.0), Y(-0.62), R(0.1), R(0.16), rgb(250, 214, 100), 255);
            break;
        case R_RICE:
            cv->fillEllipseA(X(0.0), Y(-0.02), R(0.56), R(0.4), rgb(246, 246, 236), 255);  // the heap
            for (int k = 0; k < 5; k++) cv->fillEllipseA(X(-0.3 + k * 0.15), Y(-0.15 + (k % 2) * 0.12), R(0.07), R(0.04), rgb(210, 210, 196), 255);
            poly({-0.7, 0.05, 0.7, 0.05, 0.5, 0.5, -0.5, 0.5}, rgb(70, 110, 170));  // the bowl
            line(-0.55, 0.15, 0.55, 0.15, rgb(140, 180, 230), 0.06);
            break;
        case R_MAIZE:
            poly({-0.1, 0.7, -0.55, -0.1, -0.3, -0.2, 0.0, 0.4}, rgb(90, 170, 70));
            poly({0.1, 0.7, 0.55, -0.1, 0.3, -0.2, 0.0, 0.4}, rgb(70, 150, 60));
            cv->fillEllipseA(X(0.0), Y(-0.1), R(0.24), R(0.55), rgb(250, 200, 50), 255);
            for (int k = 0; k < 4; k++) line(-0.18, -0.45 + k * 0.22, 0.18, -0.45 + k * 0.22, rgb(200, 140, 20), 0.04);
            line(0.0, -0.6, 0.0, 0.4, rgb(200, 140, 20), 0.04);
            break;
        case R_HONEY: {
            auto hex = [&](double hx, double hy, double hr, Color c) {
                int xy[12];
                for (int k = 0; k < 6; k++) {
                    double a = k * 3.14159265 / 3 + 3.14159265 / 6;
                    xy[k * 2] = X(hx + std::cos(a) * hr);
                    xy[k * 2 + 1] = Y(hy + std::sin(a) * hr);
                }
                cv->fillPoly(xy, 6, c);
            };
            hex(-0.28, -0.2, 0.32, rgb(250, 190, 40));
            hex(0.28, -0.2, 0.32, rgb(230, 160, 30));
            hex(0.0, 0.28, 0.32, rgb(250, 180, 30));
            hex(-0.28, -0.2, 0.18, rgb(255, 220, 110));
            hex(0.0, 0.28, 0.18, rgb(255, 210, 90));
            cv->fillEllipseA(X(0.42), Y(0.5), R(0.1), R(0.14), rgb(240, 160, 20), 255);
            break;
        }
        case R_CACAO:
            cv->fillEllipseA(X(0.05), Y(0.1), R(0.32), R(0.58), rgb(206, 110, 40), 255);
            cv->fillEllipseA(X(-0.06), Y(0.0), R(0.12), R(0.42), rgb(240, 160, 70), 255);
            line(-0.15, -0.35, -0.15, 0.5, rgb(140, 60, 20), 0.05);
            line(0.18, -0.35, 0.18, 0.5, rgb(140, 60, 20), 0.05);
            line(0.05, -0.48, 0.3, -0.72, rgb(90, 70, 40), 0.07);
            cv->fillEllipseA(X(-0.35), Y(-0.55), R(0.25), R(0.1), rgb(70, 150, 60), 255);
            break;
        case R_SPICES: {
            int pts[16];
            for (int k = 0; k < 8; k++) {  // a curved chilli
                double f = k / 7.0;
                pts[k * 2] = X(-0.5 + f * 0.9);
                pts[k * 2 + 1] = Y(0.5 - f * 0.9 + std::sin(f * 3.14159265) * 0.25);
            }
            cv->polylineA(pts, 8, rgb(200, 40, 30), R(0.3), 255);
            cv->polylineA(pts, 6, rgb(240, 90, 60), R(0.1), 255);
            line(0.4, -0.4, 0.62, -0.62, rgb(60, 140, 50), 0.12);
            cv->fillEllipseA(X(0.36), Y(-0.36), R(0.13), R(0.1), rgb(70, 150, 60), 255);
            break;
        }
        case R_GRAPES: {
            const double G[6][2] = {{-0.3, -0.15}, {0.0, -0.15}, {0.3, -0.15}, {-0.15, 0.15}, {0.15, 0.15}, {0.0, 0.45}};
            for (auto& p : G) cv->fillCircleGrad(X(p[0]), Y(p[1]), R(0.17), rgb(180, 110, 200), 255, rgb(80, 30, 100), 255, X(p[0] - 0.06), Y(p[1] - 0.06));
            line(0.0, -0.32, 0.05, -0.62, rgb(110, 80, 40), 0.07);
            cv->fillEllipseA(X(0.3), Y(-0.55), R(0.26), R(0.12), rgb(90, 170, 70), 255);
            break;
        }
        case R_SALT:
            poly({-0.68, 0.48, 0.0, -0.5, 0.68, 0.48}, rgb(236, 236, 240));
            poly({0.0, -0.5, 0.68, 0.48, 0.18, 0.48}, rgb(190, 196, 208));
            for (int k = 0; k < 3; k++) poly({-0.35 + k * 0.28, 0.2 + (k % 2) * 0.1, -0.25 + k * 0.28, 0.2 + (k % 2) * 0.1, -0.25 + k * 0.28, 0.3 + (k % 2) * 0.1, -0.35 + k * 0.28, 0.3 + (k % 2) * 0.1}, rgb(255, 255, 255));
            break;
        case R_PEARLS: {
            int fan[20];
            fan[0] = X(0.0); fan[1] = Y(0.5);
            for (int k = 0; k <= 8; k++) {
                double a = 3.14159265 + k * 3.14159265 / 8;
                fan[2 + k * 2] = X(std::cos(a) * 0.68);
                fan[3 + k * 2] = Y(0.4 + std::sin(a) * 0.68);
            }
            cv->fillPoly(fan, 10, rgb(226, 170, 190));
            for (int k = 1; k < 8; k += 2) {
                double a = 3.14159265 + k * 3.14159265 / 8;
                line(0.0, 0.45, std::cos(a) * 0.62, 0.4 + std::sin(a) * 0.62, rgb(170, 110, 140), 0.04);
            }
            cv->fillCircleGrad(X(0.0), Y(0.05), R(0.24), rgb(255, 255, 255), 255, rgb(200, 200, 215), 255, X(-0.1), Y(-0.05));
            break;
        }
        case R_FURS:
            poly({-0.5, -0.55, -0.15, -0.4, 0.15, -0.4, 0.5, -0.55, 0.42, -0.15, 0.6, 0.35, 0.25, 0.3, 0.15, 0.62, -0.15, 0.62, -0.25, 0.3, -0.6, 0.35, -0.42, -0.15},
                 rgb(170, 110, 64));
            cv->fillEllipseA(X(0.0), Y(0.05), R(0.22), R(0.38), rgb(220, 170, 120), 255);
            break;
        case R_GEMS:
            poly({-0.55, -0.15, -0.3, -0.5, 0.3, -0.5, 0.55, -0.15, 0.0, 0.62}, rgb(80, 210, 210));
            poly({-0.55, -0.15, 0.55, -0.15, 0.0, 0.62}, rgb(40, 160, 180));
            poly({-0.3, -0.5, 0.0, -0.15, 0.3, -0.5}, rgb(170, 250, 250));
            line(-0.2, -0.15, 0.0, 0.6, rgb(150, 240, 240), 0.04);
            line(0.2, -0.15, 0.0, 0.6, rgb(20, 120, 140), 0.04);
            break;
        default:
            cv->fillCircle(cx, cy, R(0.45), col);
            break;
    }
    (void)dark;
}

// What the resource looks like on the ground, drawn with the terrain.
void GameUI::drawResourceScene(int x, int y, int sx, int sy) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    if (!g.resourceVisible(g.player, x, y) || hs < 18 || t.site >= 0 || t.feature == F_WONDER) return;
    if (t.city >= 0 && g.cities[t.city].x == x && g.cities[t.city].y == y) return;
    uint32_t h = hashRes(x, y);
    int u = hs * 3 / 2;  // things are drawn a little larger than life so they read at a glance
    // most scenes sit to the left of the tile, clear of units in the middle and the icon at the lower right
    int ax = sx - hs * 3 / 10 + (int)(h % 5) * hs / 40, ay = sy - hs / 10 + (int)((h >> 3) % 5) * hs / 40;
    auto Q = [&](double f) { return (int)std::lround(f * hs / 8.5); };
    int lw = std::max(1, u / 22);
    auto rock = [&](int rx, int ry, int rr, Color c) {
        cv->fillEllipseA(rx + rr / 4, ry + rr / 2, rr, rr / 3, rgb(0, 0, 0), 70);
        int p[10] = {rx - rr, ry + rr / 3, rx - rr * 2 / 3, ry - rr * 2 / 3, rx + rr / 4, ry - rr, rx + rr, ry - rr / 4, rx + rr * 3 / 4, ry + rr / 3};
        cv->fillPolyGrad(p, 5, lighten(c, 0.25f), 255, darken(c, 0.6f), 255, rx - rr / 2, ry - rr, rx + rr / 2, ry + rr / 3);
    };
    auto animal = [&](int bx, int by, Color c, bool antlers, bool flip) {
        int d = flip ? -1 : 1, b = std::max(3, u / 10);
        cv->fillEllipseA(bx + d * b / 4, by + b * 3 / 4, b, b / 4, rgb(0, 0, 0), 60);
        for (int k = -1; k <= 1; k += 2) cv->line(bx + k * b * 2 / 3, by, bx + k * b * 2 / 3, by + b * 2 / 3, darken(c, 0.6f), std::max(1, u / 40));
        cv->fillEllipseA(bx, by, b, b / 2, c, 255);
        cv->line(bx + d * b * 3 / 4, by - b / 6, bx + d * b, by - b * 2 / 3, c, std::max(1, b / 3));
        cv->fillEllipseA(bx + d * b * 9 / 8, by - b * 2 / 3, b / 3, b / 5, c, 255);
        cv->line(bx - d * b, by - b / 6, bx - d * b * 5 / 4, by + b / 3, darken(c, 0.6f), std::max(1, u / 40));
        if (antlers) {
            cv->lineA(bx + d * b, by - b * 3 / 4, bx + d * b * 3 / 4, by - b * 3 / 2, rgb(220, 200, 160), std::max(1, u / 40), 255);
            cv->lineA(bx + d * b * 5 / 4, by - b * 3 / 4, bx + d * b * 3 / 2, by - b * 3 / 2, rgb(220, 200, 160), std::max(1, u / 40), 255);
        }
    };
    switch (t.res) {
        case R_WHEAT:
            for (int k = 0; k < 4; k++) {
                int y0 = ay + Q(-2.2 + k * 1.5);
                cv->lineA(ax - Q(3.6), y0 + Q(0.4), ax + Q(2.6), y0 - Q(0.4), rgb(226, 190, 80), std::max(2, u / 11), 220);
                cv->lineA(ax - Q(3.6), y0 + Q(1.0), ax + Q(2.6), y0 + Q(0.2), rgb(150, 120, 50), lw, 140);
                for (int j = 0; j < 5; j++) cv->lineA(ax - Q(3.0 - j * 1.3), y0 + Q(0.3 - j * 0.16), ax - Q(3.0 - j * 1.3), y0 - Q(0.4 + j * 0.16), rgb(250, 220, 120), lw, 200);
            }
            break;
        case R_RICE:
            for (int k = 0; k < 4; k++) {
                int px = ax + Q((k % 2) * 3.0 - 2.5), py = ay + Q((k / 2) * 2.4 - 1.8);
                int p[8] = {px - Q(1.6), py, px + Q(1.4), py - Q(0.5), px + Q(1.7), py + Q(1.4), px - Q(1.3), py + Q(1.9)};
                cv->fillPolyA(p, 4, rgb(100, 160, 150), 220);
                cv->lineA(p[0], p[1], p[2], p[3], rgb(200, 230, 220), lw, 120);
                for (int j = 0; j < 4; j++) cv->lineA(px - Q(0.8 - j * 0.6), py + Q(0.8), px - Q(0.8 - j * 0.6), py + Q(0.2), rgb(120, 200, 90), lw, 230);
            }
            break;
        case R_MAIZE:
            for (int k = 0; k < 9; k++) {
                int px = ax + Q((k % 3) * 1.8 - 2.5 + (k / 3) * 0.5), py = ay + Q((k / 3) * 1.6 - 1.2);
                cv->lineA(px, py + Q(0.9), px, py - Q(0.7), rgb(70, 140, 50), std::max(1, u / 26), 255);
                cv->lineA(px, py, px - Q(0.6), py - Q(0.3), rgb(90, 170, 60), lw, 255);
                cv->lineA(px, py + Q(0.3), px + Q(0.6), py, rgb(90, 170, 60), lw, 255);
                cv->fillEllipseA(px + Q(0.25), py - Q(0.1), std::max(1, u / 40), std::max(2, u / 22), rgb(250, 200, 60), 255);
            }
            break;
        case R_FISH:
            for (int k = 0; k < 4; k++) {
                int fx = ax + Q(((h >> (k * 3)) % 7) - 3.0), fy = ay + Q(((h >> (k * 3 + 2)) % 5) - 2.0 + k * 0.6);
                int d = k % 2 ? 1 : -1;
                cv->fillEllipseA(fx, fy, std::max(2, u / 12), std::max(1, u / 28), rgb(30, 60, 90), 140);
                int tl[6] = {fx - d * u / 12, fy, fx - d * u / 7, fy - u / 24, fx - d * u / 7, fy + u / 24};
                cv->fillPolyA(tl, 3, rgb(30, 60, 90), 140);
            }
            cv->circle(ax + Q(1.5), ay - Q(1.5), u / 10, rgb(220, 240, 255), 1);
            break;
        case R_PEARLS:
            cv->glowEllipse(ax, ay, u / 3, u / 5, rgb(140, 220, 220), 110);
            for (int k = 0; k < 5; k++) {
                int px = ax + Q(((h >> (k * 3)) % 7) - 3.0), py = ay + Q(((h >> (k * 3 + 1)) % 5) - 2.0);
                cv->fillEllipseA(px, py, std::max(2, u / 12), std::max(1, u / 20), rgb(220, 180, 196), 240);
                cv->lineA(px - u / 14, py, px + u / 14, py, rgb(170, 120, 150), std::max(1, u / 50), 200);
                cv->fillCircle(px, py - u / 34, std::max(1, u / 30), rgb(255, 255, 255));
            }
            break;
        case R_HORSES:
            animal(ax - Q(1.5), ay - Q(0.5), rgb(130, 80, 44), false, h & 1);
            animal(ax + Q(1.6), ay + Q(1.4), (h & 2) ? rgb(220, 210, 196) : rgb(80, 50, 30), false, !(h & 1));
            break;
        case R_GAME:
            animal(ax - Q(1.2), ay, rgb(176, 124, 74), true, h & 1);
            if (hs >= 26) animal(ax + Q(1.8), ay + Q(1.8), rgb(160, 110, 66), false, !(h & 1));
            break;
        case R_FRUIT:
            for (int k = 0; k < 9; k++) {
                int px = sx + Q(((h >> (k * 3)) % 11) - 5.0), py = sy + Q(((h >> (k * 3 + 1)) % 9) - 5.0);
                cv->fillCircle(px, py, std::max(1, u / 26), k % 3 ? rgb(240, 90, 50) : rgb(250, 200, 50));
            }
            break;
        case R_STONE:
            rock(ax - Q(1.5), ay, u / 7, rgb(170, 168, 160));
            rock(ax + Q(1.2), ay + Q(1.2), u / 9, rgb(150, 148, 142));
            cv->fillRect(ax - Q(0.5), ay + Q(1.8), Q(1.6), Q(0.9), rgb(196, 194, 186));
            cv->rect(ax - Q(0.5), ay + Q(1.8), Q(1.6), Q(0.9), rgb(110, 108, 100), 1);
            break;
        case R_IRON:
            rock(ax - Q(1.3), ay, u / 6, rgb(150, 80, 54));
            rock(ax + Q(1.4), ay + Q(1.3), u / 9, rgb(130, 70, 48));
            cv->lineA(ax - Q(2.2), ay - Q(0.4), ax - Q(0.6), ay + Q(0.2), rgb(90, 40, 30), lw, 200);
            break;
        case R_COPPER:
            rock(ax - Q(1.3), ay, u / 6, rgb(176, 110, 70));
            rock(ax + Q(1.4), ay + Q(1.3), u / 9, rgb(160, 100, 64));
            cv->fillEllipseA(ax - Q(1.6), ay - Q(0.6), std::max(2, u / 18), std::max(1, u / 30), rgb(80, 180, 150), 230);
            cv->fillEllipseA(ax + Q(1.2), ay + Q(1.0), std::max(2, u / 22), std::max(1, u / 34), rgb(80, 180, 150), 230);
            break;
        case R_TIN:
            rock(ax - Q(1.3), ay, u / 6, rgb(176, 180, 186));
            rock(ax + Q(1.4), ay + Q(1.3), u / 9, rgb(150, 156, 162));
            cv->lineA(ax - Q(2.0), ay - Q(0.6), ax - Q(0.8), ay - Q(0.9), rgb(240, 244, 250), lw, 220);
            break;
        case R_COAL:
            rock(ax - Q(1.3), ay, u / 6, rgb(110, 100, 90));
            cv->lineA(ax - Q(2.4), ay + Q(0.1), ax - Q(0.2), ay - Q(0.5), rgb(20, 20, 22), std::max(2, u / 14), 240);
            for (int k = 0; k < 3; k++) cv->fillCircle(ax + Q(0.8 + k * 0.7), ay + Q(1.4 + (k % 2) * 0.4), std::max(1, u / 22), rgb(30, 30, 32));
            break;
        case R_GOLD:
            rock(ax - Q(1.3), ay, u / 6, rgb(150, 130, 100));
            rock(ax + Q(1.4), ay + Q(1.3), u / 9, rgb(140, 120, 94));
            for (int k = 0; k < 4; k++) {
                int px = ax + Q(-2.0 + k * 1.2), py = ay + Q(-0.5 + (k % 2) * 1.6);
                int r = std::max(2, u / 22);
                cv->lineA(px - r, py, px + r, py, rgb(255, 230, 120), lw, 255);
                cv->lineA(px, py - r, px, py + r, rgb(255, 230, 120), lw, 255);
            }
            break;
        case R_GEMS:
            rock(ax - Q(1.2), ay + Q(0.4), u / 6, rgb(120, 110, 104));
            for (int k = 0; k < 4; k++) {
                int px = ax + Q(-2.0 + k * 1.1), py = ay + Q(0.4);
                Color c = k % 2 ? rgb(230, 90, 200) : rgb(90, 220, 230);
                int p[6] = {px - u / 30, py, px + u / 60, py - u / 8 - (k % 2) * u / 30, px + u / 24, py};
                cv->fillPoly(p, 3, c);
            }
            break;
        case R_SALT:
            for (int k = 0; k < 3; k++) {
                int px = ax + Q(-1.8 + k * 1.7), py = ay + Q(-0.8 + (k % 2) * 1.6);
                cv->fillEllipseA(px, py, u / 7, u / 14, rgb(246, 246, 240), 220);
                cv->lineA(px - u / 12, py, px + u / 16, py - u / 40, rgb(200, 196, 186), lw, 160);
            }
            tri(cv, ax + Q(1.2), ay + Q(2.2), ax + Q(2.0), ay + Q(0.9), ax + Q(2.8), ay + Q(2.2), rgb(250, 250, 250));
            break;
        case R_OIL:
            cv->fillEllipseA(ax, ay + Q(0.4), u / 5, u / 10, rgb(14, 14, 18), 240);
            cv->glowEllipse(ax - Q(0.5), ay + Q(0.2), u / 10, u / 24, rgb(140, 120, 220), 150);
            cv->glowEllipse(ax + Q(0.6), ay + Q(0.6), u / 12, u / 26, rgb(120, 200, 160), 120);
            break;
        case R_URANIUM:
            rock(ax - Q(1.2), ay, u / 6, rgb(110, 120, 90));
            cv->glowEllipse(ax - Q(1.2), ay - Q(0.4), u / 5, u / 7, rgb(150, 255, 90), 130);
            for (int k = 0; k < 3; k++) cv->fillCircle(ax + Q(0.6 + k * 0.8), ay + Q(1.2), std::max(1, u / 24), rgb(170, 255, 110));
            break;
        case R_SCRAP:
            for (int k = 0; k < 4; k++) {
                int px = ax + Q(-2.0 + k * 1.3), py = ay + Q(((h >> (k * 2)) % 3) - 1.0);
                int p[8] = {px - u / 12, py, px + u / 14, py - u / 20, px + u / 10, py + u / 30, px - u / 16, py + u / 18};
                cv->fillPoly(p, 4, k % 2 ? rgb(150, 86, 50) : rgb(120, 110, 100));
            }
            cv->circle(ax + Q(1.4), ay + Q(1.6), u / 12, rgb(40, 40, 44), std::max(1, u / 30));
            break;
        case R_HONEY: {  // a wild hive, or a straw skep in the open
            int hx = ax, hy = ay;
            if (t.veg == V_NONE) {
                cv->fillEllipseA(hx, hy + u / 8, u / 7, u / 24, rgb(0, 0, 0), 70);
                int dome[10] = {hx - u / 8, hy + u / 8, hx - u / 9, hy - u / 20, hx, hy - u / 7, hx + u / 9, hy - u / 20, hx + u / 8, hy + u / 8};
                cv->fillPolyGrad(dome, 5, rgb(236, 196, 110), 255, rgb(170, 120, 50), 255, hx - u / 10, hy - u / 8, hx + u / 10, hy + u / 8);
                for (int k = 0; k < 3; k++) cv->lineA(hx - u / 9, hy + k * u / 20 - u / 30, hx + u / 9, hy + k * u / 20 - u / 30, rgb(150, 100, 40), lw, 200);
                cv->fillCircle(hx, hy + u / 12, std::max(1, u / 34), rgb(40, 30, 20));
            } else {
                cv->line(hx, hy - u / 5, hx, hy - u / 8, rgb(90, 60, 30), lw);
                cv->fillEllipseA(hx, hy, u / 12, u / 8, rgb(220, 160, 50), 255);
                for (int k = 0; k < 3; k++) cv->lineA(hx - u / 13, hy - u / 16 + k * u / 16, hx + u / 13, hy - u / 16 + k * u / 16, rgb(140, 90, 30), lw, 220);
            }
            for (int k = 0; k < 4; k++) cv->fillCircle(hx + Q(((h >> (k * 2)) % 5) - 2.0) + u / 6, hy + Q(((h >> (k * 2 + 1)) % 5) - 2.5), std::max(1, u / 40), rgb(30, 24, 10));
            break;
        }
        case R_CACAO:
            for (int k = 0; k < 6; k++) {
                int px = sx + Q(((h >> (k * 3)) % 9) - 5.0), py = sy + Q(((h >> (k * 3 + 1)) % 7) - 3.0);
                cv->fillEllipseA(px, py, std::max(1, u / 34), std::max(2, u / 18), k % 2 ? rgb(220, 120, 40) : rgb(180, 80, 30), 255);
            }
            break;
        case R_SPICES:
            for (int k = 0; k < 4; k++) {
                int px = ax + Q(-2.4 + k * 1.5), py = ay + Q((k % 2) * 1.4 - 0.4);
                cv->fillCircleGrad(px, py, u / 11, rgb(110, 170, 70), 255, rgb(40, 90, 40), 255, px - u / 30, py - u / 30);
                for (int j = 0; j < 3; j++) cv->fillCircle(px + (j - 1) * u / 26, py - u / 40 + (j % 2) * u / 30, std::max(1, u / 40), rgb(220, 50, 30));
            }
            break;
        case R_GRAPES:
            for (int row = 0; row < 3; row++) {
                int y0 = ay + Q(-1.8 + row * 1.6);
                cv->lineA(ax - Q(3.0), y0 + Q(0.4), ax + Q(2.4), y0 - Q(0.3), rgb(110, 80, 50), lw, 220);
                for (int k = 0; k < 4; k++) {
                    int px = ax - Q(2.5) + k * Q(1.5), py = y0 + Q(0.3) - k * Q(0.18);
                    cv->fillEllipseA(px, py - u / 30, u / 16, u / 26, rgb(70, 140, 60), 255);
                    cv->fillCircle(px + u / 40, py + u / 40, std::max(1, u / 36), rgb(110, 40, 130));
                }
            }
            break;
        case R_FURS: {
            int fx = ax, fy = ay;
            int b = std::max(3, u / 11);
            cv->fillEllipseA(fx + b / 3, fy + b * 2 / 3, b, b / 4, rgb(0, 0, 0), 60);
            cv->fillEllipseA(fx, fy, b, b / 2, rgb(200, 110, 50), 255);  // a fox
            cv->fillEllipseA(fx + b, fy - b / 3, b / 2, b / 3, rgb(200, 110, 50), 255);
            tri(cv, fx + b * 3 / 4, fy - b / 2, fx + b * 7 / 8, fy - b, fx + b, fy - b / 2, rgb(170, 90, 40));
            int tail[8] = {fx - b, fy, fx - b * 2, fy - b / 3, fx - b * 2, fy + b / 4, fx - b * 3 / 4, fy + b / 3};
            cv->fillPoly(tail, 4, rgb(210, 120, 60));
            cv->fillCircle(fx - b * 2, fy, std::max(1, b / 4), rgb(250, 250, 250));
            for (int k = 0; k < 4; k++) cv->fillCircleA(fx + Q(1.0 + k * 0.8), fy + Q(1.6 + (k % 2) * 0.3), std::max(1, u / 40), rgb(80, 70, 60), 160);
            break;
        }
        default: break;
    }
}
