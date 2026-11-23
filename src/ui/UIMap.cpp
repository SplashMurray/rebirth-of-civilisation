// Rebirth of Civilisation - hex map rendering
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <cmath>

static const double SQ3 = 1.7320508075688772;

static inline uint32_t hashXY(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void GameUI::hexCorners(int cx, int cy, int r, int* xy) const {
    static const double CA[6] = {0.8660254, 0.8660254, 0.0, -0.8660254, -0.8660254, 0.0};
    static const double SA[6] = {-0.5, 0.5, 1.0, 0.5, -0.5, -1.0};
    for (int i = 0; i < 6; i++) {
        xy[i * 2] = cx + (int)std::lround(r * CA[i]);
        xy[i * 2 + 1] = cy + (int)std::lround(r * SA[i]);
    }
}

void GameUI::tileCenter(int tx, int ty, int& sx, int& sy) const {
    double wx = hs * SQ3 * (tx + 0.5 * (ty & 1)) + hs * SQ3 / 2;
    double wy = hs * 1.5 * ty + hs;
    sx = (int)std::lround(wx - camX);
    sy = (int)std::lround(wy - camY);
}

bool GameUI::pickTile(int sx, int sy, int& tx, int& ty) const {
    double px = sx + camX - hs * SQ3 / 2, py = sy + camY - hs;
    double q = (SQ3 / 3 * px - py / 3.0) / hs;
    double r = (2.0 / 3.0 * py) / hs;
    double s = -q - r;
    double rq = std::round(q), rr = std::round(r), rs = std::round(s);
    double dq = std::fabs(rq - q), dr = std::fabs(rr - r), ds = std::fabs(rs - s);
    if (dq > dr && dq > ds) rq = -rr - rs;
    else if (dr > ds) rr = -rq - rs;
    int row = (int)rr;
    if (row < 0) return false;
    int col = (int)rq + (row - (row & 1)) / 2;
    tx = col; ty = row;
    return game && game->inMap(tx, ty);
}

void GameUI::clampCamera() {
    if (!game) return;
    double ww = hs * SQ3 * (game->W + 0.5), wh = hs * 1.5 * (game->H - 1) + hs * 2;
    camX = std::clamp(camX, -W * 0.4, std::max(-W * 0.4, ww - W * 0.6));
    camY = std::clamp(camY, -H * 0.4, std::max(-H * 0.4, wh - H * 0.6));
}

void GameUI::centerOn(int tx, int ty) {
    double wx = hs * SQ3 * (tx + 0.5 * (ty & 1)) + hs * SQ3 / 2;
    double wy = hs * 1.5 * ty + hs;
    camX = wx - W / 2.0;
    camY = wy - H / 2.0;
    clampCamera();
}

void GameUI::drawTrefoil(int cx, int cy, int r) {
    cv->fillCircle(cx, cy, r, rgb(240, 210, 40));
    for (int k = 0; k < 3; k++) {
        double a0 = (-90 + k * 120 - 30) * 3.14159265 / 180, a1 = (-90 + k * 120 + 30) * 3.14159265 / 180;
        int xy[10];
        xy[0] = cx; xy[1] = cy;
        for (int i = 0; i < 4; i++) {
            double a = a0 + (a1 - a0) * i / 3.0;
            xy[2 + i * 2] = cx + (int)(std::cos(a) * r * 0.85);
            xy[3 + i * 2] = cy + (int)(std::sin(a) * r * 0.85);
        }
        cv->fillPoly(xy, 5, rgb(20, 20, 20));
    }
    cv->fillCircle(cx, cy, std::max(1, r / 4), rgb(240, 210, 40));
    cv->fillCircle(cx, cy, std::max(1, r / 6), rgb(20, 20, 20));
}

static void bump(Canvas* cv, int cx, int cy, int w, int h, Color c) {
    int xy[18];
    for (int i = 0; i <= 8; i++) {
        double a = 3.14159265 * i / 8;
        xy[i * 2] = cx - (int)(std::cos(a) * w / 2);
        xy[i * 2 + 1] = cy - (int)(std::sin(a) * h);
    }
    cv->fillPoly(xy, 9, c);
}

static void tri(Canvas* cv, int x0, int y0, int x1, int y1, int x2, int y2, Color c) {
    int xy[6] = {x0, y0, x1, y1, x2, y2};
    cv->fillPoly(xy, 3, c);
}

// short helpers in "unit" coordinates around a centre (fx, fy in tenths of the hex radius)
#define PX(fx) (sx + (fx) * u / 10)
#define PY(fy) (sy + (fy) * u / 10)

void GameUI::drawTile(int x, int y, int sx, int sy, bool visible) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    bool water = g.isWater(x, y);
    uint32_t h = hashXY(x, y);
    Color base = g_terrain[t.terrain].color;
    int v = (int)(h % 11) - 5;
    if (water) v /= 2;
    base = rgb(base.r + v, base.g + v, base.b + v);
    if (t.veg == V_FOREST) base = blend(base, rgb(70, 110, 58), 0.35f);
    if (t.veg == V_JUNGLE) base = blend(base, rgb(50, 118, 60), 0.45f);
    if (t.hills) base = blend(base, rgb(140, 118, 84), 0.20f);
    if (t.mountain) base = rgb(118 + v, 112 + v, 106 + v);
    if (t.fallout) base = blend(base, rgb(150, 168, 40), 0.16f * t.fallout);
    if (t.site >= 0 && g.warHappened && !water) base = blend(base, rgb(70, 68, 66), 0.45f);
    (void)visible;  // fog of war is drawn as a translucent overlay afterwards
    int xy[12];
    hexCorners(sx, sy, hs + 1, xy);
    cv->fillPoly(xy, 6, base);
    int u = hs;
    // bevel: light from the upper left
    if (hs >= 18) {
        int c2[12];
        hexCorners(sx, sy, hs - 1, c2);
        int bw = std::max(1, hs / 16);
        cv->lineA(c2[6], c2[7], c2[8], c2[9], rgb(255, 255, 255), bw, water ? 14 : 26);
        cv->lineA(c2[8], c2[9], c2[10], c2[11], rgb(255, 255, 255), bw, water ? 14 : 26);
        cv->lineA(c2[10], c2[11], c2[0], c2[1], rgb(255, 255, 255), bw, water ? 10 : 18);
        cv->lineA(c2[0], c2[1], c2[2], c2[3], rgb(0, 0, 0), bw, water ? 16 : 34);
        cv->lineA(c2[2], c2[3], c2[4], c2[5], rgb(0, 0, 0), bw, water ? 16 : 34);
        cv->lineA(c2[4], c2[5], c2[6], c2[7], rgb(0, 0, 0), bw, water ? 12 : 26);
    }
    // ground texture
    if (hs >= 22 && !t.mountain) {
        int lw = std::max(1, u / 18);
        if (water) {
            for (int k = 0; k < 2; k++) {
                int wx = PX((int)((h >> (k * 5)) % 9) - 4), wy = PY((int)((h >> (k * 5 + 3)) % 9) - 4);
                cv->lineA(wx - u / 6, wy, wx - u / 12, wy - u / 20, rgb(220, 240, 255), lw, 60);
                cv->lineA(wx - u / 12, wy - u / 20, wx, wy, rgb(220, 240, 255), lw, 60);
                cv->lineA(wx, wy, wx + u / 12, wy - u / 20, rgb(220, 240, 255), lw, 60);
            }
        } else if (t.veg == V_NONE && !t.hills) {
            switch (t.terrain) {
                case T_GRASS:
                    for (int k = 0; k < 4; k++) {
                        int gx = PX((int)((h >> (k * 4)) % 13) - 6), gy = PY((int)((h >> (k * 4 + 2)) % 11) - 5);
                        cv->lineA(gx, gy, gx - u / 20, gy - u / 10, rgb(40, 90, 30), lw, 110);
                        cv->lineA(gx, gy, gx + u / 20, gy - u / 10, rgb(40, 90, 30), lw, 110);
                        cv->lineA(gx, gy, gx, gy - u / 8, rgb(150, 200, 100), lw, 90);
                    }
                    break;
                case T_PLAINS:
                    for (int k = 0; k < 5; k++) {
                        int gx = PX((int)((h >> (k * 3)) % 13) - 6), gy = PY((int)((h >> (k * 3 + 1)) % 11) - 5);
                        cv->lineA(gx, gy, gx + u / 9, gy, k % 2 ? rgb(120, 110, 50) : rgb(210, 200, 120), lw, 100);
                    }
                    break;
                case T_DESERT:
                    for (int k = 0; k < 2; k++) {
                        int dx = PX(k ? 2 : -3), dy = PY(k ? -2 : 3);
                        cv->lineA(dx - u / 4, dy, dx, dy - u / 9, rgb(250, 235, 190), lw + 1, 120);
                        cv->lineA(dx, dy - u / 9, dx + u / 4, dy, rgb(160, 130, 80), lw + 1, 90);
                    }
                    break;
                case T_TUNDRA:
                    for (int k = 0; k < 6; k++)
                        cv->fillCircleA(PX((int)((h >> (k * 3)) % 13) - 6), PY((int)((h >> (k * 3 + 2)) % 11) - 5), std::max(1, u / 22),
                                        k % 2 ? rgb(230, 235, 240) : rgb(80, 85, 70), 110);
                    break;
                case T_SNOW:
                    for (int k = 0; k < 3; k++)
                        cv->fillCircleA(PX((int)((h >> (k * 4)) % 11) - 5), PY((int)((h >> (k * 4 + 2)) % 9) - 4), u / 7, rgb(190, 210, 235), 70);
                    break;
            }
        }
    }
    // relief
    if (t.mountain) {
        // a big peak with a lit and a shaded face, a smaller one behind
        tri(cv, PX(-8), PY(4), PX(-4), PY(-3), PX(0), PY(4), rgb(104, 98, 94));
        tri(cv, PX(-4), PY(-3), PX(-2), PY(1), PX(0), PY(4), rgb(84, 78, 76));
        tri(cv, PX(-5), PY(-1), PX(-4), PY(-3), PX(-3), PY(-1), rgb(235, 238, 242));
        tri(cv, PX(-5), PY(5), PX(1), PY(-6), PX(1), PY(5), rgb(150, 144, 138));
        tri(cv, PX(1), PY(-6), PX(7), PY(5), PX(1), PY(5), rgb(98, 92, 90));
        tri(cv, PX(-1), PY(-2), PX(1), PY(-6), PX(1), PY(-2), rgb(248, 248, 252));
        tri(cv, PX(1), PY(-6), PX(3), PY(-2), PX(1), PY(-2), rgb(200, 206, 218));
    } else if (t.hills) {
        Color dark = darken(base, 0.72f), lit = lighten(base, 0.18f);
        bump(cv, PX(-3), PY(4), u * 7 / 10, u * 4 / 10, dark);
        bump(cv, PX(-3) - u / 20, PY(4) - u / 30, u * 5 / 10, u * 3 / 10, lit);
        bump(cv, PX(3), PY(2), u * 8 / 10, u * 5 / 10, dark);
        bump(cv, PX(3) - u / 20, PY(2) - u / 30, u * 6 / 10, u * 4 / 10, lit);
    }
    // vegetation
    if (t.veg == V_FOREST && !t.mountain) {
        bool conifer = t.terrain == T_TUNDRA || t.hills || (h & 1);
        const int P[5][2] = {{-4, 2}, {3, 1}, {0, -3}, {-2, 5}, {4, 5}};
        int n = hs >= 26 ? 5 : 3;
        for (int k = 0; k < n; k++) {
            int tx = PX(P[k][0]), ty = PY(P[k][1]);
            cv->fillCircleA(tx + u / 14, ty + u / 5, u / 7, rgb(0, 0, 0), 50);  // shadow
            if (conifer) {
                Color c1 = t.terrain == T_TUNDRA ? rgb(46, 88, 70) : rgb(38, 96, 48), c2 = darken(c1, 0.7f);
                cv->line(tx, ty + u / 8, tx, ty + u / 4, rgb(90, 60, 30), std::max(1, u / 16));
                tri(cv, tx - u / 6, ty + u / 7, tx, ty - u / 4, tx, ty + u / 7, c1);
                tri(cv, tx, ty - u / 4, tx + u / 6, ty + u / 7, tx, ty + u / 7, c2);
            } else {
                cv->line(tx, ty, tx, ty + u / 4, rgb(100, 70, 40), std::max(1, u / 14));
                cv->fillCircle(tx, ty, u / 6, rgb(48, 110, 50));
                cv->fillCircleA(tx - u / 18, ty - u / 18, u / 10, rgb(120, 180, 90), 160);
            }
        }
    } else if (t.veg == V_JUNGLE && !t.mountain) {
        const int P[6][2] = {{-4, 2}, {3, 2}, {-1, -3}, {1, 4}, {4, -2}, {-5, -2}};
        for (int k = 0; k < 6; k++) {
            int tx = PX(P[k][0]), ty = PY(P[k][1]);
            cv->fillCircle(tx, ty, u / 6, rgb(20, 86, 40));
            cv->fillCircle(tx - u / 16, ty - u / 16, u / 9, rgb(40, 124, 52));
            cv->fillCircleA(tx - u / 12, ty - u / 10, u / 20 + 1, rgb(160, 220, 120), 170);
        }
    }
    if (t.site >= 0) drawHumanSite(x, y, sx, sy, true);
    // improvements
    if (t.imp != IMP_NONE && hs >= 16) {
        int ix = PX(-4), iy = PY(4);
        switch (t.imp) {
            case IMP_FARM: {
                int fxy[8] = {ix - u / 4, iy, ix, iy - u / 8, ix + u / 4, iy, ix, iy + u / 8};
                cv->fillPoly(fxy, 4, rgb(196, 170, 70));
                for (int k = -1; k <= 1; k++) cv->line(ix - u / 6 + k * u / 12, iy + k * u / 24 + u / 24, ix + u / 12 + k * u / 12, iy - u / 12 + k * u / 24 + u / 24, rgb(120, 140, 40), 1);
                break;
            }
            case IMP_MINE:
                tri(cv, ix - u / 5, iy + u / 10, ix, iy - u / 5, ix + u / 5, iy + u / 10, rgb(60, 48, 36));
                cv->fillCircle(ix, iy + u / 24, u / 14, rgb(20, 16, 12));
                cv->line(ix - u / 8, iy - u / 4, ix + u / 8, iy - u / 10, rgb(210, 210, 210), 2);
                break;
            case IMP_LUMBER:
                for (int k = 0; k < 3; k++) {
                    cv->fillRect(ix - u / 5 + (k % 2) * u / 16, iy - k * u / 12, u * 2 / 5, u / 12, rgb(140 - k * 10, 90, 45));
                    cv->fillCircle(ix + u / 5 + (k % 2) * u / 16, iy - k * u / 12 + u / 24, u / 24, rgb(210, 170, 110));
                }
                break;
            case IMP_TRADEPOST:
                tri(cv, ix - u / 5, iy + u / 10, ix, iy - u / 5, ix + u / 5, iy + u / 10, rgb(220, 180, 70));
                cv->line(ix, iy - u / 5, ix, iy - u / 3, rgb(120, 80, 40), 1);
                break;
            case IMP_PASTURE:
                cv->line(ix - u / 4, iy, ix + u / 4, iy, rgb(150, 110, 60), std::max(1, u / 18));
                cv->line(ix - u / 4, iy - u / 10, ix + u / 4, iy - u / 10, rgb(150, 110, 60), std::max(1, u / 18));
                for (int k = -1; k <= 1; k++) cv->line(ix + k * u / 6, iy - u / 6, ix + k * u / 6, iy + u / 20, rgb(120, 85, 45), std::max(1, u / 16));
                break;
        }
    }
    // resource
    if (g.resourceVisible(g.player, x, y) && hs >= 16) {
        const ResourceDef& r = g_resources[t.res];
        int rx = PX(4), ry = PY(3), rr = std::max(4, u / 6);
        cv->fillCircleA(rx + 1, ry + 2, rr + 1, rgb(0, 0, 0), 90);
        cv->fillCircle(rx, ry, rr + 1, rgb(24, 24, 24));
        cv->fillCircle(rx, ry, rr, r.color);
        cv->fillCircleA(rx - rr / 3, ry - rr / 3, std::max(1, rr / 2), rgb(255, 255, 255), 70);
        if (hs >= 34) cv->text(rx, ry + rr + 1, r.abbr, std::max(9, u / 4), rgb(250, 250, 250), AL_CENTER, true);
    }
    if (t.fallout > 0 && hs >= 16) drawTrefoil(PX(3), PY(-4), std::max(4, u / 6 + t.fallout));
}

void GameUI::drawHumanSite(int x, int y, int sx, int sy, bool visible) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    const HumanSite& s = g.sites[t.site];
    bool alive = !g.warHappened;
    int u = hs;
    auto F = [&](Color c) { return visible ? c : darken(c, 0.55f); };
    uint32_t h = hashXY(x * 7, y * 13);
    Color wall = alive ? rgb(190, 196, 208) : rgb(70, 66, 62);
    Color wall2 = alive ? rgb(150, 156, 170) : rgb(52, 50, 48);
    switch (s.type) {
        case RUIN_CITY: {
            int n = 6;
            for (int k = 0; k < n; k++) {
                int bw = u / 5, bx = sx - u * 6 / 10 + k * u / 5;
                int bh = u / 3 + (int)((h >> (k * 3)) % 5) * u / 9;
                if (!alive) bh = bh * (35 + (int)((h >> (k * 2)) % 40)) / 100;
                int by = sy + u * 4 / 10 - bh;
                cv->fillRect(bx, by, bw - 1, bh, F(k % 2 ? wall : wall2));
                if (alive && u >= 26)
                    for (int wy = by + 3; wy < sy + u * 4 / 10 - 3; wy += 5)
                        if (((h >> (wy & 15)) + k) % 3) cv->fillRect(bx + 2, wy, 2, 2, F(rgb(255, 230, 120)));
                if (!alive) tri(cv, bx, by, bx + bw / 2, by - u / 10, bx + bw - 1, by, F(wall));
            }
            break;
        }
        case RUIN_TOWN:
            for (int k = 0; k < 3; k++) {
                int bx = sx - u / 2 + k * u / 3, by = sy + u / 10 - (k == 1 ? u / 8 : 0);
                int bh = alive ? u / 4 : u / 8;
                cv->fillRect(bx, by, u / 4, bh, F(wall));
                if (alive) tri(cv, bx - 2, by, bx + u / 8, by - u / 6, bx + u / 4 + 2, by, F(rgb(170, 70, 60)));
            }
            break;
        case RUIN_MILITARY:
            cv->fillRect(sx - u / 2, sy - u / 6, u, u / 3, F(alive ? rgb(96, 110, 70) : rgb(60, 62, 50)));
            cv->fillRect(sx - u / 10, sy - u / 3, u / 5, u / 6, F(alive ? rgb(80, 90, 60) : rgb(50, 50, 44)));
            if (alive) {
                cv->line(sx - u / 2, sy, sx + u / 2, sy, F(rgb(220, 220, 200)), 1);
            }
            cv->text(sx, sy - u / 6, alive ? "BASE" : "", std::max(8, u / 4), F(rgb(230, 230, 210)), AL_CENTER, true);
            break;
        case RUIN_LAB: {
            Color dome = alive ? rgb(235, 238, 245) : rgb(80, 80, 84);
            cv->fillCircle(sx, sy, u / 3, F(dome));
            cv->fillRect(sx - u / 2, sy, u, u / 4, F(alive ? rgb(200, 205, 215) : rgb(64, 62, 60)));
            if (!alive) tri(cv, sx - u / 4, sy - u / 4, sx, sy - u / 10, sx + u / 6, sy - u / 3, F(rgb(40, 40, 40)));
            break;
        }
        case RUIN_POWER: {
            Color tw = alive ? rgb(200, 200, 196) : rgb(76, 72, 68);
            int xy[8] = {sx - u / 3, sy + u / 3, sx - u / 5, sy - u / 4, sx + u / 5, sy - u / 4, sx + u / 3, sy + u / 3};
            cv->fillPoly(xy, 4, F(tw));
            if (alive) {
                cv->fillCircle(sx - u / 8, sy - u / 3, u / 7, F(rgb(240, 240, 240)));
                cv->fillCircle(sx + u / 10, sy - u / 2, u / 6, F(rgb(230, 230, 236)));
            } else {
                tri(cv, sx - u / 5, sy - u / 4, sx, sy - u / 8, sx + u / 5, sy - u / 4, F(rgb(40, 40, 40)));
            }
            break;
        }
    }
    // excavation tiers
    if (!alive && hs >= 22) {
        int mask = g_ruins[t.ruin].tierMask, nT = 0;
        for (int k = 0; k < RUIN_TIERS; k++) if (mask & (1 << k)) nT++;
        int dx = sx - (nT - 1) * u / 8, i = 0;
        for (int k = 0; k < RUIN_TIERS; k++) {
            if (!(mask & (1 << k))) continue;
            Color c;
            if (t.ruinDone & (1 << k)) c = rgb(70, 70, 70);
            else if (g.hasTech(g.player, g_ruinTierTech[k])) c = rgb(255, 160, 40);
            else c = rgb(120, 100, 80);
            cv->fillCircle(dx + i * u / 4, sy + u * 6 / 10, std::max(2, u / 12) + 1, rgb(15, 15, 15));
            cv->fillCircle(dx + i * u / 4, sy + u * 6 / 10, std::max(2, u / 12), F(c));
            i++;
        }
    }
}

void GameUI::drawCityOnMap(const City& c, int sx, int sy) {
    Game& g = *game;
    int u = hs;
    Color cc = civColor(c.owner);
    int e = g.era(c.owner);
    // cleared ground
    int gxy[12];
    hexCorners(sx, sy, hs * 7 / 10, gxy);
    cv->fillPolyA(gxy, 6, rgb(90, 70, 50), 110);
    // walls
    bool walls = false;
    for (size_t b = 0; b < g_buildings.size(); b++) if (c.has[b] && g_buildings[b].defense >= 4 && !g_buildings[b].wonder) walls = true;
    if (walls) {
        int wxy[12];
        hexCorners(sx, sy, hs * 7 / 10, wxy);
        for (int k = 0; k < 6; k++)
            cv->line(wxy[k * 2], wxy[k * 2 + 1], wxy[(k + 1) % 6 * 2], wxy[(k + 1) % 6 * 2 + 1], e < 3 ? rgb(120, 86, 50) : rgb(170, 160, 140), std::max(2, u / 12));
    }
    // buildings, back to front
    static const int SPOTS[8][2] = {{-3, -3}, {3, -3}, {0, -1}, {-4, 1}, {4, 1}, {-1, 3}, {2, 3}, {0, -4}};
    int n = std::min(8, 2 + c.pop / 2);
    for (int k = 0; k < n; k++) {
        int bx = PX(SPOTS[k][0]), by = PY(SPOTS[k][1]);
        uint32_t hh = hashXY(c.id * 31 + k, k * 7);
        if (e < 3) {
            // domed huts of woven branches
            int r = u / 7 + (int)(hh % 3) * u / 40;
            cv->fillCircleA(bx + 2, by + 2, r, rgb(0, 0, 0), 60);
            int dxy[18];
            for (int i = 0; i <= 8; i++) {
                double a = 3.14159265 * i / 8;
                dxy[i * 2] = bx - (int)(std::cos(a) * r);
                dxy[i * 2 + 1] = by - (int)(std::sin(a) * r);
            }
            cv->fillPoly(dxy, 9, rgb(150, 110, 60));
            cv->fillCircleA(bx - r / 3, by - r / 2, std::max(1, r / 3), rgb(220, 180, 120), 90);
            cv->fillRect(bx - r / 4, by - r / 2, std::max(2, r / 2), r / 2, rgb(60, 36, 20));
        } else if (e < 6) {
            // stone houses with tiled roofs
            int bw = u / 4, bh2 = u / 6 + (int)(hh % 3) * u / 30;
            cv->fillRectA(bx - bw / 2 + 2, by - bh2 + 2, bw, bh2, rgb(0, 0, 0), 60);
            cv->fillRect(bx - bw / 2, by - bh2, bw, bh2, rgb(196, 176, 140));
            cv->fillRect(bx, by - bh2, bw / 2, bh2, rgb(160, 140, 110));
            tri(cv, bx - bw / 2 - 2, by - bh2, bx, by - bh2 - u / 8, bx + bw / 2 + 2, by - bh2, k == 2 ? cc : rgb(170, 80, 50));
            cv->fillRect(bx - 1, by - u / 14, 3, u / 14, rgb(60, 40, 30));
        } else {
            // industrial and modern blocks with lit windows
            int bw = u / 5, bh2 = u / 4 + (int)(hh % 4) * u / 12;
            cv->fillRectA(bx - bw / 2 + 2, by - bh2 + 2, bw, bh2, rgb(0, 0, 0), 70);
            cv->fillRect(bx - bw / 2, by - bh2, bw, bh2, rgb(176, 182, 192));
            cv->fillRect(bx, by - bh2, bw / 2, bh2, rgb(140, 146, 158));
            for (int wy = by - bh2 + 3; wy < by - 3; wy += 4)
                for (int wx = bx - bw / 2 + 2; wx < bx + bw / 2 - 1; wx += 3)
                    if ((hashXY(wx, wy) & 3) != 0) cv->fillRect(wx, wy, 1, 2, rgb(255, 228, 140));
        }
    }
    if (c.capital) {
        // a pennant in the civilisation's colour
        int fx2 = PX(0), fy2 = PY(-4);
        cv->line(fx2, fy2, fx2, fy2 - u / 3, rgb(60, 40, 30), std::max(1, u / 20));
        tri(cv, fx2, fy2 - u / 3, fx2 + u / 5, fy2 - u / 4, fx2, fy2 - u / 6, cc);
    }
}

void GameUI::drawCityBanner(const City& c, int sx, int sy) {
    Game& g = *game;
    int u = hs;
    Color cc = civColor(c.owner);
    int sz = std::max(10, std::min(16, hs / 3 + 3));
    std::string name = (c.capital ? "* " : "") + c.name;
    int tw = cv->textWidth(name, sz, true);
    int bh = sz + 6, pw = sz + 8;
    int bw = tw + pw + 10;
    int bx = sx - bw / 2, by = sy - u - bh + 4;
    cv->fillRect(bx - 1, by - 1, bw + 2, bh + 2, rgb(10, 10, 10));
    cv->fillRect(bx, by, bw, bh, darken(cc, 0.5f));
    cv->fillRect(bx, by, pw, bh, cc);
    cv->text(bx + pw / 2, by + 3, std::to_string(c.pop), sz, rgb(255, 255, 255), AL_CENTER, true);
    cv->text(bx + pw + 5, by + 3, name, sz, rgb(250, 250, 250), AL_LEFT, true);
    int yb = by + bh + 1;
    if (c.owner == g.player) {
        double pf = 0;
        if (c.buildKind != BK_NONE) pf = (double)c.prod / std::max(1, g.buildCost(c.owner, c.buildKind, c.buildId));
        bar(bx, yb, bw, 3, std::min(1.0, pf), rgb(230, 140, 60));
        bar(bx, yb + 3, bw, 3, std::min(1.0, (double)c.food / g.growthThreshold(c.pop)), rgb(110, 200, 90));
        yb += 6;
    }
    int mh = g.cityMaxHP(c);
    if (c.hp < mh) bar(bx, yb, bw, 4, (double)c.hp / mh, c.hp * 3 < mh ? rgb(220, 60, 50) : rgb(90, 200, 90));
}

void GameUI::drawUnitIcon(int type, int cx, int cy, int s, Color ic, Color accent) {
    const UnitDef& d = g_units[type];
    int era = d.techId >= 0 ? g_techs[d.techId].era : 0;
    int lw = std::max(1, s / 5);
    auto P = [&](double fx, double fy, int& ox, int& oy) { ox = cx + (int)std::lround(fx * s); oy = cy + (int)std::lround(fy * s); };
    auto L = [&](double x1, double y1, double x2, double y2, int w) {
        int a, b, c2, d2;
        P(x1, y1, a, b); P(x2, y2, c2, d2);
        cv->line(a, b, c2, d2, ic, w);
    };
    auto poly = [&](std::initializer_list<double> pts, Color col) {
        int xy[32], n = 0;
        auto it = pts.begin();
        while (it != pts.end() && n < 16) { double fx = *it++; double fy = *it++; P(fx, fy, xy[n * 2], xy[n * 2 + 1]); n++; }
        cv->fillPoly(xy, n, col);
    };
    auto circ = [&](double fx, double fy, double r, Color col) { int a, b; P(fx, fy, a, b); cv->fillCircle(a, b, std::max(1, (int)(r * s)), col); };
    if (d.primate == -2 && d.cls != UnitClass::Armor) {  // feral beasts: a paw print
        circ(0, 0.3, 0.42, ic);
        circ(-0.5, -0.25, 0.18, ic); circ(-0.18, -0.55, 0.18, ic); circ(0.18, -0.55, 0.18, ic); circ(0.5, -0.25, 0.18, ic);
        return;
    }
    switch (d.cls) {
        case UnitClass::Naval:
            if (d.str >= 40 || (d.ocean && d.ranged() && d.rstr >= 40)) {  // steel ship: long hull, superstructure, guns
                poly({-0.9, 0.2, 0.9, 0.2, 0.62, 0.6, -0.7, 0.6}, ic);
                poly({-0.35, 0.2, -0.35, -0.2, 0.25, -0.2, 0.25, 0.2}, ic);
                poly({-0.12, -0.2, -0.12, -0.55, 0.05, -0.55, 0.05, -0.2}, ic);
                L(0.25, -0.05, 0.75, -0.2, lw);
                if (d.ranged()) L(-0.35, -0.05, -0.85, -0.2, lw);
            } else {  // hull, mast and sail (oars for the earliest boats)
                poly({-0.85, 0.25, 0.85, 0.25, 0.55, 0.62, -0.55, 0.62}, ic);
                L(0, 0.25, 0, -0.8, lw);
                if (d.moves <= 3 && !d.ocean) {
                    L(-0.5, 0.45, -0.75, 0.85, lw); L(0.5, 0.45, 0.75, 0.85, lw);  // oars
                    poly({0.05, -0.75, 0.55, -0.1, 0.05, -0.1}, ic);
                } else {
                    poly({0.05, -0.8, 0.65, 0.05, 0.05, 0.05}, ic);
                    poly({-0.05, -0.6, -0.55, 0.05, -0.05, 0.05}, ic);
                }
                if (d.ranged()) circ(0.45, 0.42, 0.08, rgb(30, 30, 30));
            }
            break;
        case UnitClass::Nuke:  // radiation trefoil
            circ(0, 0, 0.16, ic);
            for (int k = 0; k < 3; k++) {
                double a0 = -1.5708 + k * 2.0944 - 0.5, a1 = a0 + 1.0;
                poly({0.3 * std::cos(a0), 0.3 * std::sin(a0), 0.85 * std::cos(a0), 0.85 * std::sin(a0),
                      0.85 * std::cos(a1), 0.85 * std::sin(a1), 0.3 * std::cos(a1), 0.3 * std::sin(a1)}, ic);
            }
            break;
        case UnitClass::Band:  // a family: two adults and a youngster
            circ(-0.42, -0.42, 0.2, ic); poly({-0.72, 0.65, -0.62, -0.12, -0.22, -0.12, -0.12, 0.65}, ic);
            circ(0.42, -0.42, 0.2, ic); poly({0.12, 0.65, 0.22, -0.12, 0.62, -0.12, 0.72, 0.65}, ic);
            circ(0.0, 0.0, 0.14, ic); poly({-0.18, 0.65, -0.12, 0.18, 0.12, 0.18, 0.18, 0.65}, ic);
            break;
        case UnitClass::Settler:  // a hut with a door
            poly({-0.7, 0.6, -0.7, -0.05, 0.0, -0.7, 0.7, -0.05, 0.7, 0.6}, ic);
            poly({-0.18, 0.6, -0.18, 0.1, 0.18, 0.1, 0.18, 0.6}, accent);
            break;
        case UnitClass::Worker:  // a hammer
            L(-0.55, 0.65, 0.25, -0.15, lw + 1);
            poly({0.0, -0.55, 0.35, -0.9, 0.75, -0.5, 0.4, -0.15}, ic);
            break;
        case UnitClass::Recon: {  // an eye
            poly({-0.85, 0.0, -0.45, -0.38, 0.0, -0.5, 0.45, -0.38, 0.85, 0.0, 0.45, 0.38, 0.0, 0.5, -0.45, 0.38}, ic);
            circ(0, 0, 0.28, accent);
            circ(0, 0, 0.12, ic);
            break;
        }
        case UnitClass::Melee:
            if (era >= 5) {  // a rifle
                L(-0.8, 0.45, 0.85, -0.5, lw);
                poly({-0.85, 0.35, -0.45, 0.15, -0.3, 0.45, -0.7, 0.7}, ic);
                L(-0.1, 0.12, 0.0, 0.35, lw);
            } else if (era >= 1 && d.str >= 14) {  // a sword
                poly({-0.6, 0.6, 0.55, -0.55, 0.75, -0.75, 0.6, -0.45, -0.45, 0.7}, ic);
                L(-0.55, 0.25, -0.2, 0.65, lw + 1);
                circ(-0.68, 0.72, 0.13, ic);
            } else if (era >= 1) {  // a spear
                L(-0.75, 0.75, 0.55, -0.55, lw);
                poly({0.4, -0.35, 0.85, -0.85, 0.65, -0.3}, ic);
            } else {  // a club
                L(-0.6, 0.7, 0.2, -0.15, lw + 2);
                poly({0.0, -0.1, 0.25, -0.7, 0.55, -0.75, 0.75, -0.45, 0.55, -0.05, 0.25, 0.1}, ic);
            }
            break;
        case UnitClass::Ranged:  // a bow and arrow
            for (int i = 0; i < 8; i++) {  // the bow's limb, bulging to the right
                double a0 = -1.05 + i * 0.2625, a1 = a0 + 0.2625;
                L(-0.55 + std::cos(a0) * 0.95, std::sin(a0) * 0.95, -0.55 + std::cos(a1) * 0.95, std::sin(a1) * 0.95, lw);
            }
            L(-0.55 + std::cos(-1.05) * 0.95, std::sin(-1.05) * 0.95, -0.55 + std::cos(1.05) * 0.95, std::sin(1.05) * 0.95, 1);  // string
            L(-0.75, 0.0, 0.62, 0.0, std::max(1, lw - 1));  // arrow
            poly({0.85, 0.0, 0.55, -0.2, 0.55, 0.2}, ic);
            poly({-0.75, 0.0, -0.9, -0.15, -0.6, 0.0, -0.9, 0.15}, ic);
            break;
        case UnitClass::Mounted:  // a horse's head
            poly({-0.55, 0.8, -0.45, 0.1, -0.2, -0.45, 0.0, -0.8, 0.15, -0.5, 0.65, -0.15, 0.7, 0.15, 0.3, 0.1, 0.1, 0.35, 0.25, 0.8}, ic);
            circ(0.05, -0.25, 0.08, accent);
            break;
        case UnitClass::Siege:
            if (d.rstr >= 25) {  // a cannon
                L(-0.55, 0.15, 0.75, -0.45, lw + 2);
                circ(-0.25, 0.4, 0.33, ic);
                circ(-0.25, 0.4, 0.13, accent);
            } else {  // a catapult
                poly({-0.8, 0.35, 0.8, 0.35, 0.8, 0.5, -0.8, 0.5}, ic);
                L(-0.5, 0.35, 0.55, -0.6, lw);
                circ(0.6, -0.65, 0.18, ic);
                circ(-0.5, 0.6, 0.2, ic);
                circ(0.5, 0.6, 0.2, ic);
            }
            break;
        case UnitClass::Armor:  // a tank
            poly({-0.85, 0.15, 0.85, 0.15, 0.7, 0.6, -0.7, 0.6}, ic);
            poly({-0.35, -0.3, 0.3, -0.3, 0.4, 0.15, -0.45, 0.15}, ic);
            L(0.3, -0.1, 0.9, -0.25, lw);
            for (int i = -2; i <= 2; i++) circ(i * 0.3, 0.4, 0.09, accent);
            break;
    }
}

void GameUI::drawUnitToken(const Unit& u, int sx, int sy, int r) {
    const UnitDef& d = g_units[u.type];
    Color cc = civColor(u.owner);
    bool light = (cc.r * 3 + cc.g * 5 + cc.b * 2) / 10 > 150;
    Color ic = light ? rgb(24, 24, 28) : rgb(255, 255, 255);
    cv->fillCircleA(sx + 2, sy + 3, r + 2, rgb(0, 0, 0), 90);  // drop shadow
    if (d.civilian()) {
        // civilians carry a shield-shaped flag
        int sxy[10] = {sx - r - 1, sy - r, sx + r + 1, sy - r, sx + r + 1, sy + r / 4, sx, sy + r + 3, sx - r - 1, sy + r / 4};
        cv->fillPoly(sxy, 5, darken(cc, 0.45f));
        int ixy[10] = {sx - r + 1, sy - r + 2, sx + r - 1, sy - r + 2, sx + r - 1, sy + r / 4 - 1, sx, sy + r, sx - r + 1, sy + r / 4 - 1};
        cv->fillPoly(ixy, 5, cc);
        cv->fillRectA(sx - r + 2, sy - r + 3, 2 * r - 4, r / 2, rgb(255, 255, 255), 40);
    } else {
        cv->fillCircle(sx, sy, r + 2, darken(cc, 0.45f));
        cv->fillCircle(sx, sy, r, cc);
        cv->fillCircleA(sx - r / 4, sy - r / 3, r * 6 / 10, rgb(255, 255, 255), 38);  // sheen
    }
    drawUnitIcon(u.type, sx, sy - (d.civilian() ? r / 10 : 0), r * 7 / 10, ic, cc);
    if (u.hp < 100) {
        cv->fillRectA(sx - r, sy + r + 3, 2 * r, 5, rgb(0, 0, 0), 170);
        cv->fillRect(sx - r + 1, sy + r + 4, (2 * r - 2) * u.hp / 100, 3, u.hp > 50 ? rgb(90, 210, 90) : u.hp > 25 ? rgb(230, 190, 50) : rgb(220, 60, 50));
    }
    if (u.band > 0 && r >= 9) {  // band size badge
        int ms = std::max(9, r * 6 / 10);
        cv->fillCircle(sx - r, sy - r, ms * 6 / 10 + 2, rgb(20, 20, 24));
        cv->text(sx - r, sy - r - ms / 2 - 1, std::to_string(u.band), ms, rgb(160, 235, 120), AL_CENTER, true);
    }
    const char* mark = nullptr;
    switch (u.order) {
        case ORD_FORTIFY: mark = "F"; break;
        case ORD_SLEEP: mark = "z"; break;
        case ORD_BUILD: case ORD_EXCAVATE: case ORD_DECON: mark = "W"; break;
        case ORD_GOTO: mark = ">"; break;
    }
    if (mark && r >= 9) {
        int ms = std::max(9, r * 6 / 10);
        cv->fillCircle(sx + r, sy - r, ms * 6 / 10 + 1, rgb(20, 20, 24));
        cv->text(sx + r, sy - r - ms / 2 - 1, mark, ms, rgb(255, 230, 120), AL_CENTER, true);
    }
}

void GameUI::drawPathPreview() {
    Game& g = *game;
    Unit* u = sel();
    if (!u || u->owner != g.player || !g.inMap(hoverX, hoverY) || overlay != OV_NONE) return;
    if (!g.civs[g.player].explored[g.idx(hoverX, hoverY)]) return;
    if (hoverX == u->x && hoverY == u->y) return;
    bool enemy = g.isEnemyTarget(u->owner, hoverX, hoverY) && g.civs[g.player].visible[g.idx(hoverX, hoverY)];
    int sx, sy;
    if (enemy && g_units[u->type].military()) {
        tileCenter(hoverX, hoverY, sx, sy);
        bool can = g.canAttack(*u, hoverX, hoverY);
        cv->circle(sx, sy, hs * 6 / 10, can ? rgb(255, 60, 40) : rgb(160, 60, 50), 3);
        if (can) cv->text(sx, sy + hs / 2, "ATTACK", std::max(10, hs / 3), rgb(255, 90, 70), AL_CENTER, true);
        if (g_units[u->type].ranged()) return;
    }
    std::vector<Pt> path = g.findPath(*u, hoverX, hoverY, enemy);
    if (path.empty()) {
        tileCenter(hoverX, hoverY, sx, sy);
        if (!enemy) cv->text(sx, sy - 8, "X", 16, rgb(255, 80, 60), AL_CENTER, true);
        return;
    }
    int moves = u->moves, turnNo = moves > 0 ? 1 : 2;
    if (moves <= 0) moves = g.maxMoves(*u);
    int px = u->x, py = u->y;
    int lsx, lsy;
    tileCenter(px, py, lsx, lsy);
    for (size_t i = 0; i < path.size(); i++) {
        Pt p = path[i];
        Unit probe = *u;
        probe.x = px; probe.y = py;
        int mc = g.moveCost(probe, px, py, p.x, p.y);
        if (mc < 0) break;
        if (moves <= 0) { turnNo++; moves = g.maxMoves(probe); }
        if (mc >= 99) moves = 0;
        else moves -= mc;
        tileCenter(p.x, p.y, sx, sy);
        cv->line(lsx, lsy, sx, sy, rgb(255, 255, 255), 2);
        bool endsTurn = moves <= 0 || i + 1 == path.size();
        if (endsTurn) {
            int rr = std::max(7, hs / 4);
            cv->fillCircle(sx, sy, rr, rgb(20, 20, 20));
            cv->circle(sx, sy, rr, rgb(255, 255, 255), 2);
            cv->text(sx, sy - rr / 2 - 2, std::to_string(turnNo), std::max(10, rr), rgb(255, 255, 255), AL_CENTER, true);
        } else {
            cv->fillCircle(sx, sy, 3, rgb(255, 255, 255));
        }
        lsx = sx; lsy = sy;
        px = p.x; py = p.y;
        if (enemy && i + 2 == path.size()) break;  // stop next to target
    }
}

void GameUI::renderMap() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    int rowH = (int)(hs * 1.5);
    int y0 = std::max(0, (int)(camY / rowH) - 1), y1 = std::min(g.H - 1, (int)((camY + H) / rowH) + 1);
    double colW = hs * SQ3;
    int x0 = std::max(0, (int)(camX / colW) - 1), x1 = std::min(g.W - 1, (int)((camX + W) / colW) + 1);
    // the terrain layer (ground, beaches, fog, borders) only changes when the view or the visible map does
    uint64_t key = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { key = (key ^ v) * 1099511628211ull; };
    mix((uint64_t)(int64_t)std::lround(camX)); mix((uint64_t)(int64_t)std::lround(camY)); mix(hs); mix(W); mix(H);
    mix(g.warHappened); mix(g.player);
    for (size_t t = 0; t < g_techs.size(); t++) if (me.techs[t]) mix(t);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            const Tile& t = g.tiles[i];
            mix((uint64_t)me.explored[i] | (uint64_t)me.visible[i] << 1 | (uint64_t)t.veg << 2 | (uint64_t)t.imp << 4 | (uint64_t)t.fallout << 8 |
                (uint64_t)(uint8_t)t.owner << 12 | (uint64_t)t.res << 20 | (uint64_t)t.ruinDone << 28 | (uint64_t)t.terrain << 36 |
                (uint64_t)t.hills << 40 | (uint64_t)t.mountain << 41);
        }
    if (!cv->layerBegin(0, key)) {
    cv->fillRect(0, 0, W, H, rgb(8, 10, 14));
    // pass 1: terrain
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            if (!me.explored[i]) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            drawTile(x, y, sx, sy, true);
        }
    // beaches where land meets water
    if (hs >= 14)
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                int i = g.idx(x, y);
                if (!me.explored[i] || g.isWater(x, y)) continue;
                int sx, sy;
                tileCenter(x, y, sx, sy);
                int xy[12];
                hexCorners(sx, sy, hs - std::max(1, hs / 14), xy);
                static const int EDGE[6][2] = {{0, 1}, {5, 0}, {4, 5}, {3, 4}, {2, 3}, {1, 2}};
                for (int d = 0; d < 6; d++) {
                    Pt n;
                    if (!g.neighborDir(x, y, d, n) || !g.isWater(n.x, n.y)) continue;
                    int a = EDGE[d][0], b = EDGE[d][1];
                    cv->lineA(xy[a * 2], xy[a * 2 + 1], xy[b * 2], xy[b * 2 + 1], rgb(232, 218, 166), std::max(2, hs / 8), 170);
                }
            }
    // fog of war: remembered but unseen tiles are dimmed, not hidden
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            if (!me.explored[i] || me.visible[i]) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            int xy[12];
            hexCorners(sx, sy, hs + 1, xy);
            cv->fillPolyA(xy, 6, rgb(12, 16, 28), 125);
        }
    // pass 2: borders
    int bt = std::max(2, hs / 10);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            const Tile& t = g.tiles[i];
            if (!me.explored[i] || t.owner < 0) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            int xy[12];
            hexCorners(sx, sy, hs - bt / 2 - 1, xy);
            Color bc = civColor(t.owner);
            if (!me.visible[i]) bc = darken(bc, 0.6f);
            static const int EDGE[6][2] = {{0, 1}, {5, 0}, {4, 5}, {3, 4}, {2, 3}, {1, 2}};
            for (int d = 0; d < 6; d++) {
                Pt n;
                bool in = g.neighborDir(x, y, d, n);
                if (in && g.at(n.x, n.y).owner == t.owner) continue;
                int a = EDGE[d][0], b = EDGE[d][1];
                cv->lineA(xy[a * 2], xy[a * 2 + 1], xy[b * 2], xy[b * 2 + 1], bc, bt * 3, 60);
                cv->line(xy[a * 2], xy[a * 2 + 1], xy[b * 2], xy[b * 2 + 1], bc, bt);
            }
        }
    cv->layerEnd(0);
    }  // terrain layer
    // selected band: what it can forage, and how picked-over the land around it is
    if (overlay == OV_NONE && sel() && g_units[sel()->type].cls == UnitClass::Band && sel()->owner == g.player) {
        const Unit& b = *sel();
        for (auto& p : g.ring(b.x, b.y, 3)) {
            int i = g.idx(p.x, p.y);
            if (!me.visible[i] || g.isWater(p.x, p.y)) continue;
            int fo = g.tiles[i].foraged;
            if (!fo) continue;
            int sx, sy;
            tileCenter(p.x, p.y, sx, sy);
            int xy[12];
            hexCorners(sx, sy, hs - 2, xy);
            cv->fillPolyA(xy, 6, rgb(120, 84, 40), 40 + fo * 30);  // brown: picked over
        }
        std::vector<int> picked;
        g.bandForage(b, &picked);
        for (auto& p : g.ring(b.x, b.y, 1)) {
            int i = g.idx(p.x, p.y);
            int f = g.forageFood(b.owner, p.x, p.y);
            int sx, sy;
            tileCenter(p.x, p.y, sx, sy);
            bool used = std::find(picked.begin(), picked.end(), i) != picked.end();
            if (used) {
                int xy[12];
                hexCorners(sx, sy, hs - 3, xy);
                for (int k = 0; k < 6; k++) cv->lineA(xy[k * 2], xy[k * 2 + 1], xy[(k + 1) % 6 * 2], xy[(k + 1) % 6 * 2 + 1], rgb(255, 255, 255), 2, 200);
            }
            if (hs < 16) continue;
            int r = std::max(7, hs / 4), cx = sx, cy = sy + hs / 2 - r / 2;
            cv->fillCircleA(cx, cy, r, rgb(16, 20, 14), 200);
            cv->text(cx, cy - r * 7 / 10, std::to_string(f), r * 13 / 10, f > 0 ? rgb(140, 230, 100) : rgb(150, 150, 150), AL_CENTER, true);
        }
    }
    // city screen: worked tiles and yields
    if (overlay == OV_CITY && selCity >= 0) {
        const City& c = g.cities[selCity];
        for (auto& p : g.ring(c.x, c.y, 3)) {
            const Tile& t = g.at(p.x, p.y);
            if (t.city != c.id) continue;
            int sx, sy;
            tileCenter(p.x, p.y, sx, sy);
            bool worked = (p.x == c.x && p.y == c.y);
            for (int wi : c.worked) if (wi == g.idx(p.x, p.y)) worked = true;
            if (worked) {
                int xy[12];
                hexCorners(sx, sy, hs - 3, xy);
                for (int k = 0; k < 6; k++) cv->line(xy[k * 2], xy[k * 2 + 1], xy[(k + 1) % 6 * 2], xy[(k + 1) % 6 * 2 + 1], rgb(255, 255, 255), 2);
            }
            Yields y = g.tileYields(c.owner, p.x, p.y);
            int vals[4] = {y.food, y.prod, y.gold, y.sci};
            Color cols[4] = {rgb(110, 210, 80), rgb(240, 150, 60), rgb(250, 210, 60), rgb(90, 170, 255)};
            int total = 0;
            for (int k = 0; k < 4; k++) total += std::min(vals[k], 4);
            int px = sx - total * 4, py = sy + hs / 3;
            for (int k = 0; k < 4; k++)
                for (int n = 0; n < std::min(vals[k], 4); n++) {
                    cv->fillRect(px - 1, py - 1, 8, 8, rgb(10, 10, 10));
                    cv->fillRect(px, py, 6, 6, cols[k]);
                    px += 8;
                }
        }
    }
    // human city labels
    if (hs >= 22)
        for (auto& st : g.sites) {
            if (st.type != RUIN_CITY || !me.explored[g.idx(st.x, st.y)]) continue;
            if (st.x < x0 - 2 || st.x > x1 + 2 || st.y < y0 - 2 || st.y > y1 + 2) continue;
            int sx, sy;
            tileCenter(st.x, st.y, sx, sy);
            bool alive = !g.warHappened;
            std::string label = alive ? st.name : "Ruins of " + st.name;
            int sz = std::max(10, hs / 3);
            int tw = cv->textWidth(label, sz, true);
            int ly = sy + hs * 7 / 10 + 2;
            cv->fillRect(sx - tw / 2 - 4, ly, tw + 8, sz + 4, alive ? rgb(50, 54, 64) : rgb(30, 28, 26));
            cv->text(sx, ly + 1, label, sz, alive ? rgb(240, 240, 250) : rgb(190, 180, 160), AL_CENTER, true);
        }
    // pass 3: cities
    for (auto& c : g.cities) {
        if (!c.alive || !me.explored[g.idx(c.x, c.y)]) continue;
        if (c.x < x0 - 2 || c.x > x1 + 2 || c.y < y0 - 2 || c.y > y1 + 2) continue;
        int sx, sy;
        tileCenter(c.x, c.y, sx, sy);
        drawCityOnMap(c, sx, sy);
    }
    // pass 4: units
    int r = std::max(7, hs * 4 / 10);
    for (auto& u : g.units) {
        if (!u.alive || !me.visible[g.idx(u.x, u.y)]) continue;
        if (u.x < x0 || u.x > x1 || u.y < y0 || u.y > y1) continue;
        if (u.carrier >= 0 && u.id != selUnit) continue;  // aboard a ship: shown as the ship's cargo badge
        int sx, sy;
        tileCenter(u.x, u.y, sx, sy);
        bool civil = g_units[u.type].civilian();
        bool naval = g_units[u.type].naval();
        bool shared = false;
        for (auto& o : g.units) if (o.alive && o.id != u.id && o.x == u.x && o.y == u.y && o.carrier < 0) shared = true;
        int rr = r;
        if (u.carrier >= 0) { sx -= hs / 3; sy -= hs / 3; rr = r * 2 / 3; }
        else if (shared && civil) { sx -= hs / 3; sy += hs / 3; rr = r * 3 / 4; }
        else if (shared && naval) { sx += hs / 3; sy += hs / 4; rr = r * 4 / 5; }
        else if (shared) { sx += hs / 8; sy -= hs / 8; }
        else if (civil) rr = r * 7 / 8;
        if (u.id == selUnit) {
            double pulse = 0.5 + 0.5 * std::sin(animMs / 160.0);
            cv->fillCircleA(sx, sy, rr + 7, rgb(255, 230, 120), 40 + (int)(50 * pulse));
            cv->circle(sx, sy, rr + 5, blend(rgb(255, 255, 255), rgb(255, 210, 60), (float)pulse), 2);
        }
        drawUnitToken(u, sx, sy, rr);
        if (naval && rr >= 9) {
            int n = g.cargoCount(u);
            if (n > 0) {  // cargo badge
                int bs = std::max(8, rr * 5 / 10);
                cv->fillCircle(sx - rr, sy + rr - 2, bs, rgb(40, 90, 160));
                cv->circle(sx - rr, sy + rr - 2, bs, rgb(220, 230, 255), 1);
                cv->text(sx - rr, sy + rr - 2 - bs * 7 / 10, std::to_string(n), bs * 13 / 10, rgb(255, 255, 255), AL_CENTER, true);
            }
        }
    }
    // city banners above units
    for (auto& c : g.cities) {
        if (!c.alive || !me.explored[g.idx(c.x, c.y)]) continue;
        if (c.x < x0 - 2 || c.x > x1 + 2 || c.y < y0 - 2 || c.y > y1 + 2) continue;
        int sx, sy;
        tileCenter(c.x, c.y, sx, sy);
        drawCityBanner(c, sx, sy);
    }
    // hover
    if (g.inMap(hoverX, hoverY) && overlay == OV_NONE) {
        int sx, sy;
        tileCenter(hoverX, hoverY, sx, sy);
        int xy[12];
        hexCorners(sx, sy, hs - 1, xy);
        for (int k = 0; k < 6; k++) cv->line(xy[k * 2], xy[k * 2 + 1], xy[(k + 1) % 6 * 2], xy[(k + 1) % 6 * 2 + 1], rgb(255, 255, 255), 1);
    }
    drawPathPreview();
}

void GameUI::renderUnitGallery(Canvas& c) {
    cv = &c;
    W = c.width(); H = c.height();
    cv->fillRect(0, 0, W, H, rgb(92, 148, 62));
    int cols = 8, cw = W / cols;
    for (size_t t = 0; t < g_units.size(); t++) {
        Unit u;
        u.type = (int)t;
        u.owner = game ? (int)(FIRST_PRIMATE_CIV + t % 3) : 0;
        u.hp = t % 5 == 0 ? 60 : 100;
        int x = (int)(t % cols) * cw + cw / 2, y = (int)(t / cols) * 120 + 60;
        drawUnitToken(u, x, y, 30);
        cv->text(x, y + 40, g_units[t].name, 12, rgb(255, 255, 255), AL_CENTER, true);
    }
}
