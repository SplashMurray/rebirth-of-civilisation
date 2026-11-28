// Rebirth of Civilisation - terrain art
// What a tile looks like, so that each reads at a glance as what it is: grass that looks like grass, a forest
// that is a forest and not a few trees, hills that rise, fields that are farmed. Everything is drawn procedurally
// from the tile's coordinates, so the same tile always looks the same, and details thin out as the map zooms out.
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <cmath>

namespace {
// a small, fast, repeatable sequence of numbers for one tile
struct TRng {
    uint32_t s;
    explicit TRng(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    double unit() { return (next() & 0xffffff) / 16777216.0; }
    double range(double a, double b) { return a + (b - a) * unit(); }
    bool chance(double p) { return unit() < p; }
};

uint32_t tileHash(int x, int y, int salt) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)salt * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// inside a pointy-topped hex of radius R (in units of the tile radius)?
bool inHex(double fx, double fy, double R) {
    double ax = std::fabs(fx), ay = std::fabs(fy);
    return ax <= 0.866 * R && ay <= R - ax / 1.7320508;
}

struct FPt { double x, y; };

// Points spread evenly but irregularly over the hex: one per cell of a jittered grid, kept if inside radius R.
std::vector<FPt> scatter(TRng& r, int cells, double R) {
    std::vector<FPt> v;
    double step = 2.0 * R / cells;
    for (int j = 0; j < cells; j++)
        for (int i = 0; i < cells; i++) {
            double fx = -R + (i + r.range(0.15, 0.85)) * step;
            double fy = -R + (j + r.range(0.15, 0.85)) * step;
            if (inHex(fx, fy, R)) v.push_back({fx, fy});
        }
    return v;
}

Color shade(Color c, int d) { return rgb(std::clamp(c.r + d, 0, 255), std::clamp(c.g + d, 0, 255), std::clamp(c.b + d, 0, 255)); }
}  // namespace

// fx, fy in units of the tile radius
#define TX(fx) (sx + (int)std::lround((fx) * u))
#define TY(fy) (sy + (int)std::lround((fy) * u))

// ------------------------------------------------------------ the ground itself
void GameUI::drawGroundTexture(int x, int y, int sx, int sy, Color base) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    if (hs < 18) return;
    double u = hs;
    TRng r(tileHash(x, y, 11));
    int lw = std::max(1, hs / 20);
    int dense = hs >= 44 ? 6 : hs >= 30 ? 5 : 4;  // grid cells across the tile
    bool water = g.isWater(x, y);
    if (water) {
        bool deep = t.terrain == T_OCEAN;
        // depth: darker swirls far out, sunlit sandbanks near shore
        for (int k = 0; k < 2; k++) {
            double fx = r.range(-0.3, 0.3), fy = r.range(-0.3, 0.3);
            cv->glowEllipse(TX(fx), TY(fy), (int)(u * r.range(0.6, 0.9)), (int)(u * r.range(0.4, 0.6)),
                            deep ? rgb(10, 28, 64) : t.feature == F_LAKE ? rgb(120, 190, 200) : rgb(130, 200, 196), deep ? 28 : 30);
        }
        // wave crests: little double arcs, more of them on the open sea
        int n = deep ? (hs >= 30 ? 4 : 3) : 2;
        Color crest = deep ? rgb(170, 200, 240) : rgb(220, 245, 250);
        for (int k = 0; k < n; k++) {
            double fx = r.range(-0.5, 0.5), fy = r.range(-0.45, 0.45);
            if (!inHex(fx, fy, 0.8)) continue;
            int wx = TX(fx), wy = TY(fy), s = (int)(u * r.range(0.08, 0.13));
            int a1[8] = {wx - 2 * s, wy, wx - s, wy - s / 2, wx, wy, wx + s, wy - s / 2};
            cv->polylineA(a1, 4, crest, lw, deep ? 60 : 50);
            if (deep && hs >= 34) cv->lineA(wx - s / 2, wy - s / 3, wx, wy - s / 2, rgb(255, 255, 255), lw, 70);  // a whitecap
        }
        return;
    }
    if (t.mountain || t.feature == F_VOLCANO) return;  // rock speaks for itself
    bool forested = t.veg != V_NONE;
    if (forested && hs < 30) return;  // hidden under the canopy
    switch (t.terrain) {
        case T_GRASS: {  // tufts of grass in two greens, and a few wild flowers
            Color dark = shade(base, -46), light = shade(base, 52);
            // meadow light and shade, then tufts
            for (int k = 0; k < 2; k++)
                cv->glowEllipse(TX(r.range(-0.4, 0.4)), TY(r.range(-0.35, 0.35)), (int)(u * 0.32), (int)(u * 0.2), k ? light : dark, 45);
            auto pts = scatter(r, hs >= 50 ? dense + 1 : dense, 0.82);
            for (auto& p : pts) {
                if (forested && r.chance(0.6)) continue;
                int gx = TX(p.x), gy = TY(p.y), hgt = (int)(u * r.range(0.07, 0.12));
                cv->lineA(gx, gy, gx - hgt / 2, gy - hgt, dark, lw, 125);
                cv->lineA(gx, gy, gx + hgt / 3, gy - hgt, dark, lw, 115);
                cv->lineA(gx + lw, gy, gx + lw, gy - hgt * 5 / 4, light, lw, 110);
            }
            if (hs >= 30 && !forested)
                for (int k = 0; k < 3; k++) {
                    double fx = r.range(-0.55, 0.55), fy = r.range(-0.5, 0.5);
                    static const Color FL[3] = {{250, 250, 240}, {250, 220, 90}, {200, 150, 230}};
                    if (inHex(fx, fy, 0.8)) cv->fillCircleA(TX(fx), TY(fy), std::max(1, hs / 24), FL[r.next() % 3], 220);
                }
            break;
        }
        case T_PLAINS: {  // dry golden grass bending in the wind, and the odd bush
            Color straw = rgb(226, 206, 120), ochre = rgb(150, 124, 58);
            auto pts = scatter(r, dense + 2, 0.82);
            for (auto& p : pts) {
                if (forested && r.chance(0.6)) continue;
                int gx = TX(p.x), gy = TY(p.y), len = (int)(u * r.range(0.07, 0.11));
                bool lit = r.chance(0.55);
                cv->lineA(gx, gy, gx + len / 3, gy - len, lit ? straw : ochre, lw, 150);
                cv->lineA(gx + lw * 2, gy, gx + len / 2 + lw, gy - len * 3 / 4, lit ? ochre : straw, lw, 110);
            }
            if (!forested)
                for (int k = 0; k < 2; k++) {
                    double fx = r.range(-0.5, 0.5), fy = r.range(-0.45, 0.45);
                    if (!inHex(fx, fy, 0.75)) continue;
                    int bx = TX(fx), by = TY(fy), br = std::max(2, hs / 13);
                    cv->fillEllipseA(bx + br / 3, by + br / 2, br, br / 3, rgb(0, 0, 0), 60);
                    cv->fillCircleGrad(bx, by, br, rgb(140, 146, 72), 255, rgb(76, 84, 38), 255, bx - br / 3, by - br / 3);
                }
            break;
        }
        case T_DESERT: {  // long sinuous dunes: a sharp lit crest, its shadow on the lee side, and wind ripples
            int nd = hs >= 30 ? 3 : 2;
            for (int k = 0; k < nd; k++) {
                double cy = (nd == 3 ? -0.42 + k * 0.4 : -0.22 + k * 0.45) + r.range(-0.05, 0.05);
                double ph = r.range(0, 6.28), amp = r.range(0.05, 0.09);
                double half = std::min(0.78, 0.866 * (1.0 - std::fabs(cy) * 0.75));
                const int N = 7;
                int crest[N * 2], lee[N * 4];
                for (int i = 0; i < N; i++) {
                    double fx = -half + 2 * half * i / (N - 1);
                    double fy = cy + std::sin(ph + fx * 4.2) * amp;
                    crest[i * 2] = TX(fx); crest[i * 2 + 1] = TY(fy);
                    lee[i * 2] = TX(fx); lee[i * 2 + 1] = TY(fy);
                    double drop = 0.11 * std::sin(3.14159 * i / (N - 1));  // the shadow is deepest mid-dune
                    lee[(2 * N - 1 - i) * 2] = TX(fx + 0.04); lee[(2 * N - 1 - i) * 2 + 1] = TY(fy + drop);
                }
                cv->fillPolyA(lee, 2 * N, rgb(178, 140, 84), 130);
                cv->polylineA(crest, N, rgb(255, 242, 206), std::max(1, hs / 18), 210);
            }
            if (hs >= 28)
                for (int k = 0; k < 4; k++) {  // ripples
                    double fx = r.range(-0.5, 0.5), fy = r.range(-0.5, 0.5);
                    if (!inHex(fx, fy, 0.72)) continue;
                    int wx = TX(fx), wy = TY(fy), sz = (int)(u * 0.07);
                    int rp[8] = {wx - sz, wy, wx - sz / 3, wy - sz / 4, wx + sz / 3, wy + sz / 5, wx + sz, wy};
                    cv->polylineA(rp, 4, rgb(176, 146, 96), lw, 80);
                }
            break;
        }
        case T_TUNDRA: {  // moss and lichen between frost-shattered stones, and the odd patch of old snow
            auto pts = scatter(r, dense, 0.8);
            for (auto& p : pts) {
                int mx = TX(p.x), my = TY(p.y);
                double roll = r.unit();
                if (roll < 0.45) {
                    int mr = (int)(u * r.range(0.06, 0.1));
                    cv->fillEllipseA(mx, my, mr, mr * 2 / 3, r.chance(0.5) ? rgb(112, 126, 74) : rgb(140, 132, 82), 190);
                } else if (roll < 0.7) {
                    int sr = std::max(2, (int)(u * r.range(0.04, 0.07)));
                    cv->fillEllipseA(mx + sr / 3, my + sr / 3, sr, sr * 2 / 3, rgb(40, 40, 36), 90);
                    cv->fillCircleGrad(mx, my, sr, rgb(186, 184, 176), 255, rgb(104, 104, 98), 255, mx - sr / 2, my - sr / 2);
                } else if (roll < 0.82) {
                    cv->fillEllipseA(mx, my, (int)(u * 0.14), (int)(u * 0.06), rgb(232, 238, 244), 170);
                } else {
                    cv->fillCircleA(mx, my, std::max(1, hs / 26), rgb(206, 178, 92), 180);  // lichen
                }
            }
            break;
        }
        case T_SNOW: {  // drifts: smooth banks of snow with blue shadow on their lee, and glints of ice
            for (int k = 0; k < 3; k++) {
                double cy = -0.38 + k * 0.36 + r.range(-0.05, 0.05), cx = r.range(-0.25, 0.25), w = r.range(0.28, 0.42);
                int sh[10] = {TX(cx - w), TY(cy + 0.02), TX(cx - w / 3), TY(cy - 0.04), TX(cx + w / 3), TY(cy - 0.03), TX(cx + w), TY(cy + 0.03),
                              TX(cx + w * 0.2), TY(cy + 0.1)};
                cv->fillPolyA(sh, 5, rgb(160, 184, 220), 110);
                int lip[8] = {TX(cx - w), TY(cy + 0.02), TX(cx - w / 3), TY(cy - 0.05), TX(cx + w / 3), TY(cy - 0.04), TX(cx + w), TY(cy + 0.03)};
                cv->polylineA(lip, 4, rgb(255, 255, 255), std::max(1, hs / 20), 220);
            }
            if (hs >= 30)
                for (int k = 0; k < 4; k++) {
                    double fx = r.range(-0.5, 0.5), fy = r.range(-0.5, 0.5);
                    if (inHex(fx, fy, 0.8)) cv->fillCircleA(TX(fx), TY(fy), 1, rgb(255, 255, 255), 255);
                }
            break;
        }
        default: break;
    }
}

// ------------------------------------------------------------ hills: rolling mounds, lit from the upper left
static void mound(Canvas* cv, int cx, int cy, int w, int h, Color lit, Color dark) {
    int xy[34];
    for (int i = 0; i <= 16; i++) {
        double a = 3.14159265 * i / 16;
        double sh = std::pow(std::sin(a), 0.8);  // fuller shoulders than a plain ellipse
        xy[i * 2] = cx - (int)(std::cos(a) * w / 2);
        xy[i * 2 + 1] = cy - (int)(sh * h);
    }
    cv->fillPolyGrad(xy, 17, lit, 255, dark, 255, cx - w / 4, cy - h, cx + w / 3, cy);
}

void GameUI::drawHills(int x, int y, int sx, int sy, Color base) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    double u = hs;
    TRng r(tileHash(x, y, 23));
    Color lit = lighten(base, 0.30f), dark = darken(base, 0.62f);
    if (t.terrain == T_SNOW) { lit = rgb(252, 252, 255); dark = rgb(150, 170, 204); }
    if (t.terrain == T_DESERT) { lit = rgb(240, 214, 156); dark = rgb(160, 120, 74); }
    // three mounds, back to front, the back ones higher on the tile
    struct M { double x, y, w, h; };
    M ms[3] = {{r.range(-0.35, -0.15), -0.05, r.range(0.75, 0.9), r.range(0.38, 0.46)},
               {r.range(0.15, 0.35), 0.08, r.range(0.8, 0.95), r.range(0.42, 0.52)},
               {r.range(-0.15, 0.1), 0.52, r.range(0.7, 0.85), r.range(0.3, 0.36)}};
    int n = hs >= 22 ? 3 : 2;
    cv->glowEllipse(TX(0.05), TY(0.55), (int)(u * 0.8), (int)(u * 0.22), rgb(0, 0, 0), 70);
    for (int k = 0; k < n; k++) {
        const M& m = ms[k];
        int cx = TX(m.x), cy = TY(m.y), w = (int)(u * m.w), h = (int)(u * m.h);
        cv->fillEllipseA(cx + w / 8, cy, w / 2, h / 5, rgb(0, 0, 0), 60);  // its shadow on the ground
        mound(cv, cx, cy, w, h, k == 2 ? lighten(base, 0.18f) : lit, dark);
        if (hs >= 26) {  // a rim of light along the brow
            int rim[10];
            for (int i = 0; i < 5; i++) {
                double a = 3.14159265 * (0.2 + i * 0.12);
                rim[i * 2] = cx - (int)(std::cos(a) * w * 0.42);
                rim[i * 2 + 1] = cy - (int)(std::pow(std::sin(a), 0.8) * h * 0.92);
            }
            cv->polylineA(rim, 5, lighten(lit, 0.4f), std::max(1, hs / 22), 120);
        }
        if (hs >= 30 && t.veg == V_NONE && (t.terrain == T_GRASS || t.terrain == T_PLAINS)) {  // grass on the slopes
            Color gc = t.terrain == T_GRASS ? shade(base, -50) : rgb(150, 124, 58);
            for (int i = 0; i < 4; i++) {
                int gx = cx + (int)(r.range(-0.3, 0.3) * w), gy = cy - (int)(r.range(0.15, 0.6) * h);
                cv->lineA(gx, gy, gx - hs / 20, gy - hs / 12, gc, std::max(1, hs / 22), 140);
                cv->lineA(gx, gy, gx + hs / 24, gy - hs / 12, gc, std::max(1, hs / 22), 140);
            }
        }
        if (hs >= 30 && (t.terrain == T_DESERT || t.terrain == T_TUNDRA))  // rocks breaking the surface
            for (int i = 0; i < 2; i++) {
                int rx = cx + (int)(r.range(-0.25, 0.3) * w), ry = cy - (int)(r.range(0.2, 0.55) * h), rr = std::max(2, hs / 20);
                cv->fillCircleGrad(rx, ry, rr, rgb(196, 186, 170), 255, rgb(110, 100, 90), 255, rx - rr / 2, ry - rr / 2);
            }
    }
}

// ------------------------------------------------------------ forest: a canopy, not a few trees
void GameUI::drawForest(int x, int y, int sx, int sy) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    double u = hs;
    TRng r(tileHash(x, y, 31));
    bool cold = t.terrain == T_TUNDRA || t.terrain == T_SNOW;
    double coniferShare = cold ? 1.0 : t.hills ? 0.6 : t.terrain == T_PLAINS ? 0.35 : 0.2;
    if (t.imp == IMP_LUMBER) coniferShare = std::max(coniferShare, 0.3);
    // the shade of the forest floor gives the canopy its mass
    cv->glowEllipse(TX(0), TY(0.1), (int)(u * 0.85), (int)(u * 0.7), cold ? rgb(30, 60, 50) : rgb(26, 66, 30), 120);
    int cells = hs >= 44 ? 5 : hs >= 28 ? 4 : 3;
    auto pts = scatter(r, cells, 0.88);
    if (t.imp == IMP_LUMBER)  // the camp's clearing
        pts.erase(std::remove_if(pts.begin(), pts.end(), [](const FPt& p) { return p.x < 0.05 && p.y > 0.0; }), pts.end());
    std::sort(pts.begin(), pts.end(), [](const FPt& a, const FPt& b) { return a.y < b.y; });
    static const Color LEAF_LIT[3] = {{132, 192, 86}, {150, 196, 92}, {118, 176, 80}};
    static const Color LEAF_DARK[3] = {{30, 88, 38}, {44, 96, 36}, {26, 78, 44}};
    double scale = hs >= 28 ? 1.0 : 1.25;  // fewer, bigger trees when zoomed out
    for (auto& p : pts) {
        int tx = TX(p.x), ty = TY(p.y);
        bool conifer = r.chance(coniferShare);
        double sz = r.range(0.85, 1.15) * scale;
        if (conifer) {
            int H = (int)(u * 0.36 * sz), W = (int)(u * 0.13 * sz);
            cv->fillEllipseA(tx + W / 2, ty + H / 6, W, W / 3, rgb(0, 0, 0), 80);
            cv->line(tx, ty, tx, ty + H / 6, rgb(84, 56, 32), std::max(1, hs / 22));
            Color c1 = cold ? rgb(50, 104, 84) : rgb(44, 112, 58), c2 = darken(c1, 0.55f);
            for (int tier = 0; tier < 3; tier++) {  // three tiers of branches, smaller towards the top
                int tw = W * (3 - tier) / 3 + W / 4, tb = ty - tier * H / 4, tt = tb - H / 2 + tier * H / 12;
                int lft[6] = {tx - tw, tb, tx, tt, tx, tb};
                int rgt[6] = {tx, tt, tx + tw, tb, tx, tb};
                cv->fillPoly(lft, 3, lighten(c1, 0.06f * tier));
                cv->fillPoly(rgt, 3, c2);
            }
            if (cold) {  // snow on the boughs
                int cap[6] = {tx - W / 3, ty - H * 3 / 4 + H / 8, tx, ty - H * 3 / 4 - H / 16, tx + W / 3, ty - H * 3 / 4 + H / 8};
                cv->fillPoly(cap, 3, rgb(240, 244, 250));
            }
        } else {
            int R = (int)(u * 0.17 * sz);
            int pal = (int)(r.next() % 3);
            cv->fillEllipseA(tx + R / 3, ty + R, R, R / 3, rgb(0, 0, 0), 85);
            cv->line(tx, ty, tx, ty + R, rgb(96, 66, 38), std::max(1, hs / 18));
            // a crown of three clumps of leaves, lit from the upper left
            cv->fillCircleGrad(tx + R / 3, ty - R / 6, R * 4 / 5, LEAF_LIT[pal], 255, LEAF_DARK[pal], 255, tx, ty - R / 2);
            cv->fillCircleGrad(tx - R / 3, ty - R / 5, R * 4 / 5, LEAF_LIT[pal], 255, LEAF_DARK[pal], 255, tx - R / 2, ty - R / 2);
            cv->fillCircleGrad(tx, ty - R / 2, R * 3 / 4, lighten(LEAF_LIT[pal], 0.12f), 255, LEAF_DARK[pal], 255, tx - R / 3, ty - R);
        }
    }
}

// ------------------------------------------------------------ jungle: a closed, layered rainforest canopy
// A leaf seen from above: from its stalk at (bx, by) out along angle a, drooping under its own weight.
static void leaf(Canvas* cv, int bx, int by, double a, double len, double wid, double droop, Color c, Color rib) {
    double ca = std::cos(a), sa = std::sin(a), nx = -sa, ny = ca;
    auto at = [&](double f, double side, int& ox, int& oy) {
        ox = bx + (int)std::lround(ca * len * f + nx * side);
        oy = by + (int)std::lround(sa * len * f + ny * side + droop * f * f);
    };
    int xy[12];
    at(0, 0, xy[0], xy[1]);
    at(0.35, wid * 0.5, xy[2], xy[3]);
    at(0.72, wid * 0.4, xy[4], xy[5]);
    at(1.0, 0, xy[6], xy[7]);
    at(0.72, -wid * 0.4, xy[8], xy[9]);
    at(0.35, -wid * 0.5, xy[10], xy[11]);
    cv->fillPoly(xy, 6, c);
    if (len > 8) {
        int m1x, m1y, m2x, m2y;
        at(0.1, 0, m1x, m1y);
        at(0.9, 0, m2x, m2y);
        int mid[6] = {m1x, m1y, (m1x + m2x) / 2, (m1y + m2y) / 2 + (int)(droop * 0.2), m2x, m2y};
        cv->polylineA(mid, 3, rib, 1, 160);
    }
}

void GameUI::drawJungle(int x, int y, int sx, int sy) {
    double u = hs;
    TRng r(tileHash(x, y, 37));
    // the canopy is closed: deep green shade over the whole tile, so no open ground shows through
    cv->glowEllipse(TX(0), TY(0.05), (int)(u * 1.0), (int)(u * 0.88), rgb(10, 52, 24), 235);
    int cells = hs >= 40 ? 5 : hs >= 26 ? 4 : 3;
    // the understorey: dark lumps of foliage filling every gap
    for (auto& p : scatter(r, cells + 1, 0.95)) {
        int bx = TX(p.x), by = TY(p.y), br = (int)(u * r.range(0.08, 0.12));
        Color c = r.chance(0.5) ? rgb(18, 74, 32) : rgb(26, 88, 38);
        cv->fillCircle(bx, by, br, c);
        cv->fillCircle(bx + br * 2 / 3, by + br / 3, br * 3 / 4, c);
    }
    // the canopy: lumpy crowns of many shades, each a cluster of leafy domes lit from the upper left
    static const Color LIT[5] = {{132, 186, 58}, {78, 162, 72}, {104, 176, 64}, {62, 142, 70}, {150, 196, 74}};
    static const Color DEEP[5] = {{30, 84, 26}, {14, 70, 36}, {24, 80, 30}, {10, 62, 34}, {40, 92, 30}};
    auto crowns = scatter(r, std::max(3, cells - 1), 0.84);
    std::sort(crowns.begin(), crowns.end(), [](const FPt& a, const FPt& b) { return a.y < b.y; });
    double scale = hs >= 26 ? 1.0 : 1.3;
    for (auto& p : crowns) {
        int cx = TX(p.x), cy = TY(p.y);
        double R = u * r.range(0.22, 0.28) * scale;
        int pal = (int)(r.next() % 5);
        cv->fillEllipseA(cx + (int)(R * 0.3), cy + (int)(R * 0.6), (int)(R * 1.1), (int)(R * 0.45), rgb(0, 20, 8), 110);
        int lobes = hs >= 30 ? 7 : 5;
        for (int l = 0; l < lobes; l++) {
            double a = r.range(0, 6.28), d = l == 0 ? 0 : R * r.range(0.5, 0.8);
            int lx = cx + (int)(std::cos(a) * d), ly = cy + (int)(std::sin(a) * d * 0.7) - (l == 0 ? (int)(R * 0.2) : 0);
            int lr = (int)(R * (l == 0 ? 0.7 : r.range(0.38, 0.52)));
            cv->fillCircleGrad(lx, ly, std::max(2, lr), LIT[pal], 255, DEEP[pal], 255, lx - lr / 2, ly - lr * 2 / 3);
        }
        if (hs >= 34 && r.chance(0.5))  // glossy highlights on the top leaves
            cv->fillCircleA(cx - (int)(R * 0.3), cy - (int)(R * 0.5), std::max(1, (int)(R * 0.18)), lighten(LIT[pal], 0.35f), 170);
    }
    // broad leaves - wild banana and elephant ear - spilling out between the crowns
    if (hs >= 22) {
        int n = hs >= 36 ? 4 : 2;
        for (int k = 0; k < n; k++) {
            double fx = r.range(-0.55, 0.55), fy = r.range(-0.45, 0.55);
            if (!inHex(fx, fy, 0.78)) continue;
            double a = r.range(-3.0, 0.2);
            leaf(cv, TX(fx), TY(fy), a, u * r.range(0.22, 0.3), u * r.range(0.11, 0.15), u * 0.06, rgb(96, 172, 62), rgb(40, 100, 36));
        }
    }
    // hanging vines
    if (hs >= 34)
        for (int k = 0; k < 3; k++) {
            double fx = r.range(-0.5, 0.5), fy = r.range(-0.45, 0.25);
            if (!inHex(fx, fy, 0.7)) continue;
            int vx = TX(fx), vy = TY(fy), len = (int)(u * r.range(0.15, 0.25));
            int v[6] = {vx, vy, vx + len / 4, vy + len / 2, vx - len / 8, vy + len};
            cv->polylineA(v, 3, rgb(30, 70, 24), std::max(1, hs / 34), 200);
        }
    // palms breaking through the roof of the forest, fronds drooping
    if (hs >= 20) {
        int palms = hs >= 30 ? 2 : 1;
        for (int k = 0; k < palms; k++) {
            double fx = r.range(-0.45, 0.45), fy = r.range(-0.4, 0.3);
            if (!inHex(fx, fy, 0.7)) { fx *= 0.5; fy *= 0.5; }
            int px = TX(fx), py = TY(fy);
            int lean = (int)(u * r.range(-0.06, 0.06));
            int tr[6] = {px, py + (int)(u * 0.2), px + lean / 2, py + (int)(u * 0.08), px + lean, py};
            cv->polylineA(tr, 3, rgb(118, 92, 58), std::max(2, hs / 18), 255);
            int fronds = hs >= 34 ? 8 : 6;
            double L = u * (hs >= 30 ? 0.27 : 0.32);
            for (int f = 0; f < fronds; f++) {
                double a = -3.14159 + f * (6.28318 / fronds) + r.range(-0.15, 0.15);
                leaf(cv, px + lean, py, a, L, L * 0.32, L * 0.45, f % 2 ? rgb(70, 160, 58) : rgb(52, 138, 50), rgb(30, 86, 30));
            }
            cv->fillCircle(px + lean, py + hs / 40, std::max(1, hs / 26), rgb(120, 86, 40));  // coconuts
        }
    }
    if (hs >= 32) {  // flowers, and the humid haze of the rainforest
        for (int f = 0; f < 3; f++) {
            double fx = r.range(-0.5, 0.5), fy = r.range(-0.5, 0.5);
            static const Color FL[3] = {{236, 70, 90}, {250, 190, 60}, {240, 120, 200}};
            if (inHex(fx, fy, 0.75)) cv->fillCircleA(TX(fx), TY(fy), std::max(1, hs / 26), FL[r.next() % 3], 230);
        }
        cv->glowEllipse(TX(r.range(-0.3, 0.3)), TY(r.range(-0.2, 0.3)), (int)(u * 0.5), (int)(u * 0.18), rgb(230, 240, 235), 26);
    }
}

// ------------------------------------------------------------ floodplains and marshes
void GameUI::drawFloodplain(int x, int y, int sx, int sy) {
    double u = hs;
    TRng r(tileHash(x, y, 41));
    // dark, wet silt laid down by the floods...
    cv->glowEllipse(TX(0), TY(0.05), (int)(u * 0.8), (int)(u * 0.65), rgb(88, 70, 44), 150);
    // ...and lush green growth along the old channels, sweeping across the tile
    for (int k = 0; k < 3; k++) {
        double cy = -0.36 + k * 0.36 + r.range(-0.04, 0.04), ph = r.range(0, 6.28);
        double half = std::min(0.76, 0.866 * (1.0 - std::fabs(cy) * 0.75));
        const int N = 7;
        int band[N * 2];
        for (int i = 0; i < N; i++) {
            double fx = -half + 2 * half * i / (N - 1);
            band[i * 2] = TX(fx);
            band[i * 2 + 1] = TY(cy + std::sin(ph + fx * 3.5) * 0.08);
        }
        cv->polylineA(band, N, rgb(80, 150, 54), std::max(3, (int)(u * 0.13)), 210);
        cv->polylineA(band, N, rgb(128, 188, 80), std::max(1, (int)(u * 0.04)), 150);
    }
    if (hs >= 26)
        for (int k = 0; k < 2; k++) {  // pools left by the last flood
            double fx = r.range(-0.4, 0.4), fy = r.range(-0.3, 0.3);
            int px = TX(fx), py = TY(fy), pr = (int)(u * 0.1);
            cv->fillEllipseA(px, py, pr, pr / 2, rgb(70, 130, 150), 230);
            cv->lineA(px - pr / 2, py - pr / 6, px + pr / 4, py - pr / 6, rgb(210, 240, 250), std::max(1, hs / 24), 170);
        }
}

void GameUI::drawMarsh(int x, int y, int sx, int sy) {
    double u = hs;
    TRng r(tileHash(x, y, 43));
    int lw = std::max(1, hs / 20);
    // open water between the reed beds
    for (int k = 0; k < 4; k++) {
        double fx = r.range(-0.45, 0.45), fy = r.range(-0.4, 0.45);
        if (!inHex(fx, fy, 0.7)) continue;
        int px = TX(fx), py = TY(fy), pw = (int)(u * r.range(0.2, 0.28)), ph = pw * 2 / 5;
        cv->fillEllipseA(px, py, pw, ph, rgb(40, 84, 98), 245);
        cv->fillEllipseA(px + pw / 3, py + ph / 3, pw * 2 / 3, ph * 2 / 3, rgb(40, 84, 98), 245);
        cv->lineA(px - pw / 2, py - ph / 3, px + pw / 4, py - ph / 3, rgb(180, 220, 225), lw, 140);
        if (hs >= 30) cv->fillCircleA(px + pw / 2, py, std::max(2, hs / 20), rgb(80, 150, 70), 230);  // a lily pad
    }
    // reed beds: clumps of blades with brown cattail heads
    int clumps = hs >= 30 ? 8 : 5;
    for (int c = 0; c < clumps; c++) {
        double fx = r.range(-0.6, 0.6), fy = r.range(-0.5, 0.6);
        if (!inHex(fx, fy, 0.8)) continue;
        int rx = TX(fx), ry = TY(fy);
        for (int b = 0; b < 4; b++) {
            int tall = (int)(u * r.range(0.14, 0.22));
            int bx = rx + (b - 2) * lw * 2;
            cv->lineA(bx, ry, bx + (b - 2) * lw, ry - tall, b % 2 ? rgb(150, 176, 80) : rgb(92, 128, 50), std::max(1, lw + (hs >= 40 ? 1 : 0)), 255);
            if (b == 1 && hs >= 24) cv->fillEllipseA(bx + (b - 2) * lw, ry - tall, std::max(1, lw), std::max(2, hs / 18), rgb(112, 72, 38), 255);
        }
    }
}

// ------------------------------------------------------------ improvements: what the people made of the land
void GameUI::drawImprovement(int x, int y, int sx, int sy) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    if (t.imp == IMP_NONE || hs < 14) return;
    double u = hs;
    TRng r(tileHash(x, y, 53));
    int lw = std::max(1, hs / 20);
    switch (t.imp) {
        case IMP_FARM: {  // a patchwork of four plots seen at an angle, rows of crops, hedges between
            static const Color CROPS[4] = {{226, 194, 92}, {132, 178, 70}, {150, 108, 64}, {200, 208, 98}};
            int first = (int)(r.next() % 4);
            // the farm's outline: a lozenge across the tile, split through its middle both ways
            FPt L{-0.72, 0.06}, T{0.02, -0.42}, R{0.72, 0.02}, B{-0.02, 0.52};
            FPt C{(L.x + R.x + T.x + B.x) / 4, (L.y + R.y + T.y + B.y) / 4};
            auto mid = [](FPt a, FPt b) { return FPt{(a.x + b.x) / 2, (a.y + b.y) / 2}; };
            FPt LT = mid(L, T), TR = mid(T, R), RB = mid(R, B), BL = mid(B, L);
            FPt plots[4][4] = {{L, LT, C, BL}, {LT, T, TR, C}, {C, TR, R, RB}, {BL, C, RB, B}};
            for (int f = 0; f < 4; f++) {
                Color c = CROPS[(first + f) % 4];
                if (t.terrain == T_DESERT && f % 2) c = CROPS[2];
                int q[8];
                for (int i = 0; i < 4; i++) { q[i * 2] = TX(plots[f][i].x); q[i * 2 + 1] = TY(plots[f][i].y); }
                cv->fillPolyA(q, 4, c, 240);
                // rows along the plot, alternating direction from plot to plot
                int rows = hs >= 34 ? 4 : 2;
                FPt a0 = plots[f][f % 2 ? 0 : 1], a1 = plots[f][f % 2 ? 3 : 2], b0 = plots[f][f % 2 ? 1 : 0], b1 = plots[f][f % 2 ? 2 : 3];
                for (int k = 1; k <= rows; k++) {
                    double s2 = (double)k / (rows + 1);
                    FPt p0{a0.x + (b0.x - a0.x) * s2, a0.y + (b0.y - a0.y) * s2}, p1{a1.x + (b1.x - a1.x) * s2, a1.y + (b1.y - a1.y) * s2};
                    cv->lineA(TX(p0.x), TY(p0.y), TX(p1.x), TY(p1.y), darken(c, 0.7f), lw, 210);
                }
            }
            int out[8] = {TX(L.x), TY(L.y), TX(T.x), TY(T.y), TX(R.x), TY(R.y), TX(B.x), TY(B.y)};
            int hw = std::max(1, hs / 16);
            cv->polylineA(out, 4, rgb(62, 100, 42), hw, 240, true);  // hedges
            cv->lineA(TX(LT.x), TY(LT.y), TX(RB.x), TY(RB.y), rgb(62, 100, 42), hw, 240);
            cv->lineA(TX(TR.x), TY(TR.y), TX(BL.x), TY(BL.y), rgb(62, 100, 42), hw, 240);
            break;
        }
        case IMP_MINE: {  // a timbered shaft in the hillside, a spoil heap and an ore cart
            int ex = TX(-0.18), ey = TY(0.18), er = (int)(u * 0.15);
            int arch[18];
            for (int i = 0; i <= 8; i++) {
                double a = 3.14159265 * i / 8;
                arch[i * 2] = ex - (int)(std::cos(a) * er);
                arch[i * 2 + 1] = ey - (int)(std::sin(a) * er * 1.1);
            }
            cv->fillPoly(arch, 9, rgb(18, 14, 12));
            int tw = std::max(2, hs / 14);
            cv->line(ex - er, ey, ex - er, ey - er, rgb(126, 86, 46), tw);
            cv->line(ex + er, ey, ex + er, ey - er, rgb(126, 86, 46), tw);
            cv->line(ex - er - tw, ey - er, ex + er + tw, ey - er, rgb(150, 104, 56), tw);
            // spoil heap
            int hx = TX(0.38), hy = TY(0.3);
            int heap[8] = {hx - (int)(u * 0.2), hy, hx - (int)(u * 0.05), hy - (int)(u * 0.16), hx + (int)(u * 0.06), hy - (int)(u * 0.14), hx + (int)(u * 0.2), hy};
            cv->fillPolyGrad(heap, 4, rgb(150, 136, 120), 255, rgb(84, 74, 64), 255, hx - hs / 8, hy - hs / 6, hx + hs / 6, hy);
            // rails and a cart full of ore
            int ry = ey + hs / 12;
            cv->line(ex, ry, hx - hs / 6, ry, rgb(80, 80, 84), std::max(1, hs / 26));
            int cx = (ex + hx) / 2, cw = (int)(u * 0.1);
            cv->fillRect(cx - cw, ry - cw, cw * 2, cw, rgb(110, 74, 40));
            cv->fillCircle(cx - cw / 2, ry - cw - cw / 4, cw / 2, rgb(70, 70, 76));
            cv->fillCircle(cx + cw / 3, ry - cw - cw / 5, cw / 2, rgb(160, 130, 70));
            cv->fillCircle(cx - cw / 2, ry, std::max(1, cw / 3), rgb(40, 40, 44));
            cv->fillCircle(cx + cw / 2, ry, std::max(1, cw / 3), rgb(40, 40, 44));
            break;
        }
        case IMP_LUMBER: {  // a clearing: stumps, a stack of logs and a saw-pit shelter
            int cx = TX(-0.3), cy = TY(0.38);
            cv->fillEllipseA(cx, cy, (int)(u * 0.3), (int)(u * 0.14), rgb(150, 120, 70), 150);
            for (int k = 0; k < 3; k++) {  // stumps
                int stx = cx + (int)(u * (-0.2 + k * 0.17)), sty = cy - (int)(u * (k == 1 ? 0.08 : 0.0));
                int sr = std::max(2, hs / 16);
                cv->fillRect(stx - sr, sty - sr / 2, sr * 2, sr, rgb(110, 72, 40));
                cv->fillEllipseA(stx, sty - sr / 2, sr, sr / 2, rgb(214, 176, 120), 255);
            }
            // the log pile
            int lx = TX(0.05), ly = TY(0.6), ll = (int)(u * 0.2), lr = std::max(2, hs / 18);
            for (int row = 0; row < 3; row++)
                for (int k = 0; k < 3 - row; k++) {
                    int ox = lx + k * lr * 2 + row * lr, oy = ly - row * lr * 2 + lr;
                    cv->line(ox - ll, oy, ox, oy, rgb(126, 82, 44), lr * 2);
                    cv->fillCircle(ox, oy, lr, rgb(220, 180, 120));
                    cv->circle(ox, oy, lr, rgb(140, 96, 52), 1);
                }
            break;
        }
        case IMP_PASTURE: {  // a fenced paddock with grazing flock
            int f[8] = {TX(-0.55), TY(0.05), TX(0.0), TY(-0.3), TX(0.55), TY(0.05), TX(0.0), TY(0.42)};
            cv->fillPolyA(f, 4, rgb(120, 170, 70), 90);
            cv->polylineA(f, 4, rgb(150, 110, 60), std::max(1, hs / 20), 255, true);
            for (int c = 0; c < 4; c++) {  // posts at the corners and between
                for (int m = 0; m < 2; m++) {
                    int ax = f[c * 2], ay = f[c * 2 + 1], bx = f[((c + 1) % 4) * 2], by = f[((c + 1) % 4) * 2 + 1];
                    int px = ax + (bx - ax) * m / 2, py = ay + (by - ay) * m / 2;
                    cv->line(px, py, px, py - hs / 10, rgb(120, 84, 46), std::max(1, hs / 20));
                }
            }
            int flock = hs >= 30 ? 4 : 3;
            for (int k = 0; k < flock; k++) {  // sheep: woolly backs, dark faces
                double fx = r.range(-0.3, 0.3), fy = r.range(-0.12, 0.2);
                int ax = TX(fx), ay = TY(fy), s = std::max(3, hs / 9);
                cv->fillEllipseA(ax + s / 4, ay + s / 2, s, s / 3, rgb(0, 0, 0), 70);
                cv->fillEllipseA(ax, ay, s, s * 2 / 3, rgb(244, 242, 232), 255);
                cv->fillCircle(ax + (k % 2 ? s : -s), ay - s / 4, std::max(1, s / 3), rgb(50, 44, 40));
            }
            break;
        }
        case IMP_TRADEPOST: {  // a thatched hut, crates and a pennant
            int hx = TX(-0.2), hy = TY(0.35), hw = (int)(u * 0.2), hh = (int)(u * 0.16);
            cv->fillEllipseA(hx + hw / 3, hy + hh / 6, hw, hh / 3, rgb(0, 0, 0), 80);
            cv->fillRect(hx - hw / 2, hy - hh, hw, hh, rgb(170, 128, 80));
            cv->fillRect(hx - hw / 8, hy - hh / 2, hw / 4, hh / 2, rgb(70, 46, 26));
            int roof[6] = {hx - hw * 3 / 4, hy - hh, hx, hy - hh * 2 - hh / 3, hx + hw * 3 / 4, hy - hh};
            cv->fillPolyGrad(roof, 3, rgb(226, 196, 116), 255, rgb(150, 116, 60), 255, hx - hw / 2, hy - hh * 2, hx + hw / 2, hy - hh);
            int px = hx + hw, py = hy - hh * 2;
            cv->line(px, hy, px, py, rgb(90, 62, 36), std::max(1, hs / 26));
            int fl[6] = {px, py, px + hw * 2 / 3, py + hh / 4, px, py + hh / 2};
            cv->fillPoly(fl, 3, rgb(210, 60, 50));
            for (int k = 0; k < 2; k++) {
                int bx = hx - hw + k * hw / 3, by = hy + hh / 4, bs = std::max(2, hs / 12);
                cv->fillRect(bx - bs, by - bs, bs * 2, bs * 2, rgb(150, 106, 58));
                cv->rect(bx - bs, by - bs, bs * 2, bs * 2, rgb(90, 60, 30), 1);
            }
            break;
        }
        default: break;
    }
}
