// Rebirth of Civilisation - hex map rendering
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

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

static void tri(Canvas* cv, int x0, int y0, int x1, int y1, int x2, int y2, Color c) {
    int xy[6] = {x0, y0, x1, y1, x2, y2};
    cv->fillPoly(xy, 3, c);
}

// short helpers in "unit" coordinates around a centre (fx, fy in tenths of the hex radius)
#define PX(fx) (sx + (fx) * u / 10)
#define PY(fy) (sy + (fy) * u / 10)

static const int EDGE[6][2] = {{0, 1}, {5, 0}, {4, 5}, {3, 4}, {2, 3}, {1, 2}};  // hex corners on each side

Color GameUI::groundColor(int x, int y) const {
    const Game& g = *game;
    const Tile& t = g.at(x, y);
    bool water = g.isWater(x, y);
    uint32_t h = hashXY(x, y);
    Color base = g_terrain[t.terrain].color;
    if (water) base = t.feature == F_LAKE ? rgb(48, 118, 150) : t.terrain == T_COAST ? rgb(42, 108, 156) : rgb(22, 50, 96);
    if (t.feature == F_FLOODPLAIN) base = blend(base, rgb(112, 128, 64), 0.45f);
    if (t.feature == F_MARSH) base = blend(base, rgb(76, 104, 76), 0.5f);
    if (t.feature == F_OASIS) base = blend(base, rgb(150, 168, 90), 0.35f);
    int v = (int)(h % 9) - 4;
    if (water) v /= 4;  // the sea is one body of water, not a honeycomb
    base = rgb(base.r + v, base.g + v, base.b + v);
    if (t.veg == V_FOREST) base = blend(base, rgb(70, 110, 58), 0.35f);
    if (t.veg == V_JUNGLE) base = blend(base, rgb(50, 118, 60), 0.45f);
    if (t.hills) base = blend(base, rgb(140, 118, 84), 0.20f);
    if (t.mountain) base = blend(base, rgb(122 + v, 114 + v, 104 + v), 0.6f);
    if (t.fallout) base = blend(base, rgb(150, 168, 40), 0.16f * t.fallout);
    if (t.site >= 0 && g.warHappened && !water) base = blend(base, rgb(70, 68, 66), 0.45f);
    return base;
}

// A strip along side d of a hex (corners xy, centre sx,sy) fading from colour c at alpha a on the edge
// to nothing 'depth' of the way to the centre. Drawn from both sides, it blends neighbouring tiles.
void GameUI::edgeFade(int sx, int sy, const int* xy, int d, float depth, Color c, int a) {
    int ia = EDGE[d][0], ib = EDGE[d][1];
    int ax = xy[ia * 2], ay = xy[ia * 2 + 1], bx = xy[ib * 2], by = xy[ib * 2 + 1];
    auto in = [&](int px, int c0) { return px + (int)std::lround((c0 - px) * depth); };
    int mx = (ax + bx) / 2, my = (ay + by) / 2;
    int q[8] = {ax, ay, bx, by, in(bx, sx), in(by, sy), in(ax, sx), in(ay, sy)};
    cv->fillPolyGrad(q, 4, c, a, c, 0, mx, my, in(mx, sx), in(my, sy));
}

// The ground of one tile: lit base colour, blended into its neighbours, with shores and shallows.
void GameUI::drawTileGround(int x, int y, int sx, int sy) {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    bool water = g.isWater(x, y);
    Color base = groundColor(x, y);
    int xy[12];
    hexCorners(sx, sy, hs + 1, xy);
    cv->fillPoly(xy, 6, base);
    if (hs < 12) return;
    int e[12];
    hexCorners(sx, sy, hs, e);
    const Tile& t = g.at(x, y);
    bool cold = t.terrain == T_SNOW || t.terrain == T_TUNDRA;
    for (int d = 0; d < 6; d++) {
        Pt n;
        if (!g.neighborDir(x, y, d, n) || !me.explored[g.idx(n.x, n.y)]) continue;
        bool nw = g.isWater(n.x, n.y);
        if (water && !nw) {
            edgeFade(sx, sy, e, d, 0.6f, t.terrain == T_COAST ? rgb(92, 182, 184) : rgb(70, 140, 170), 150);  // sunlit shallows
            int f[12];
            hexCorners(sx, sy, hs - std::max(1, hs / 12), f);
            int a = EDGE[d][0], b = EDGE[d][1];
            cv->lineA(f[a * 2], f[a * 2 + 1], f[b * 2], f[b * 2 + 1], rgb(235, 248, 250), std::max(1, hs / 14), 80);  // surf
        } else if (!water && nw) {
            edgeFade(sx, sy, e, d, 0.32f, cold ? rgb(206, 208, 200) : rgb(228, 212, 160), 215);  // shore
        } else {
            Color nc = groundColor(n.x, n.y);
            int diff = std::abs(nc.r - base.r) + std::abs(nc.g - base.g) + std::abs(nc.b - base.b);
            if (diff > 14) edgeFade(sx, sy, e, d, 0.5f, nc, 120);
        }
    }
}

// Everything that stands on a tile's ground: texture, relief, plants, ruins, improvements, resources.
void GameUI::drawTile(int x, int y, int sx, int sy, bool visible) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    bool water = g.isWater(x, y);
    uint32_t h = hashXY(x, y);
    Color base = groundColor(x, y);
    (void)visible;  // fog of war is drawn as a translucent overlay afterwards
    int u = hs;
    // soft blotches of light and shade that spill over the hex edges, so the ground does not look tiled
    if (hs >= 14) {
        int bx = PX((int)(h % 9) - 4), by = PY((int)((h >> 4) % 9) - 4);
        cv->glowEllipse(bx, by, u * 8 / 10, u * 6 / 10, (h >> 8) & 1 ? lighten(base, 0.35f) : darken(base, 0.6f), water ? 14 : 50);
        int cx2 = PX((int)((h >> 12) % 11) - 5), cy2 = PY((int)((h >> 16) % 11) - 5);
        cv->glowEllipse(cx2, cy2, u * 6 / 10, u * 4 / 10, (h >> 20) & 1 ? lighten(base, 0.3f) : darken(base, 0.65f), water ? 10 : 40);
    }
    drawGroundTexture(x, y, sx, sy, base);
    drawRoads(x, y, sx, sy);
    bool special = t.feature == F_VOLCANO || t.feature == F_WONDER;
    bool keepHills = t.feature == F_WONDER && t.natural >= 0 && g_naturals[t.natural].site == WS_FALLS;
    // relief
    if (special && !keepHills) {
        // volcanoes and natural wonders draw their own shapes below
    } else if (t.mountain) {
        bool coldPeak = t.terrain == T_SNOW || t.terrain == T_TUNDRA;
        Color rock = t.terrain == T_DESERT ? rgb(168, 140, 104) : rgb(126, 118, 110);
        cv->glowEllipse(PX(0), PY(5), u * 8 / 10, u / 4, rgb(0, 0, 0), 110);  // shadow at its foot
        // one peak: lit west face, shaded east face, a snow cap with a ragged edge
        auto peak = [&](int bx, int by, int ax, int ay, int half, bool snow) {
            int rx = bx + half / 5;  // where the ridge meets the ground
            int lit[6] = {bx - half, by, ax, ay, rx, by};
            int shade[6] = {ax, ay, bx + half, by, rx, by};
            cv->fillPolyGrad(lit, 3, lighten(rock, 0.28f), 255, rock, 255, ax, ay, bx - half / 2, by);
            cv->fillPolyGrad(shade, 3, darken(rock, 0.72f), 255, darken(rock, 0.5f), 255, ax, ay, bx + half, by);
            if (!snow) return;
            auto at = [&](int x2, int y2, double f, int& ox, int& oy) { ox = ax + (int)((x2 - ax) * f); oy = ay + (int)((y2 - ay) * f); };
            int lx, ly, mx, my, qx, qy;
            at(bx - half, by, 0.38, lx, ly);
            at(rx, by, 0.44, mx, my);
            at(bx + half, by, 0.32, qx, qy);
            int c1[10] = {ax, ay, lx, ly, (lx + mx) / 2, (ly + my) / 2 - u / 14, (lx + 3 * mx) / 4, (ly + 3 * my) / 4 + u / 30, mx, my};
            cv->fillPoly(c1, 5, rgb(246, 248, 252));
            int c2[8] = {ax, ay, mx, my, (mx + qx) / 2, (my + qy) / 2 - u / 20, qx, qy};
            cv->fillPoly(c2, 4, rgb(184, 196, 218));
        };
        peak(PX(-4), PY(4), PX(-4), PY(-2), u * 4 / 10, coldPeak || (h & 2));
        peak(PX(1), PY(5), PX(1) + u / 20, PY(-6), u * 6 / 10, true);
        if (h & 4) peak(PX(5), PY(6), PX(5), PY(1), u * 3 / 10, coldPeak);
    } else if (t.hills) {
        drawHills(x, y, sx, sy, base);
    }
    // vegetation
    if (t.veg == V_FOREST && !t.mountain) drawForest(x, y, sx, sy);
    else if (t.veg == V_JUNGLE && !t.mountain) drawJungle(x, y, sx, sy);
    drawFeature(x, y, sx, sy);
    if (t.river) {  // the river over the ground and plants, and any road over the river on a bridge
        drawRiver(x, y, sx, sy);
        if (g.roadLevelAt(x, y) > 0 && !water) drawRoads(x, y, sx, sy);
    }
    drawResourceScene(x, y, sx, sy);
    if (t.feature == F_WONDER) drawNatural(x, y, sx, sy);
    if (t.site >= 0) drawHumanSite(x, y, sx, sy, true);
    drawImprovement(x, y, sx, sy);
    // resource
    if (g.resourceVisible(g.player, x, y) && hs >= 10) {
        if (hs >= 16) drawResourceIcon(t.res, PX(4), PY(4), std::max(8, u * 28 / 100));
        else {  // zoomed far out: a dot in the resource's colour
            cv->fillCircle(PX(4), PY(4), 3, rgb(14, 14, 16));
            cv->fillCircle(PX(4), PY(4), 2, g_resources[t.res].color);
        }
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
    double t = animMs / 1000.0;
    // trodden ground, growing with the city
    int gr = u * (6 + std::min(4, c.pop / 4)) / 10;
    cv->glowEllipse(sx, sy + u / 10, gr, gr * 7 / 10, e < 5 ? rgb(118, 92, 62) : rgb(96, 96, 98), 230);
    // walls
    bool walls = false;
    for (size_t b = 0; b < g_buildings.size(); b++) if (c.has[b] && g_buildings[b].defense >= 4 && !g_buildings[b].wonder) walls = true;
    int wxy[12];
    hexCorners(sx, sy + u / 12, hs * 7 / 10, wxy);
    if (walls) {
        Color wc = e < 3 ? rgb(128, 90, 52) : rgb(176, 168, 150);
        int wt = std::max(2, u / 11);
        cv->polylineA(wxy, 6, darken(wc, 0.5f), wt + 2, 255, true);
        cv->polylineA(wxy, 6, wc, wt, 255, true);
        for (int k = 0; k < 6; k++) {  // towers on the corners
            int tx = wxy[k * 2], ty = wxy[k * 2 + 1], tr = std::max(2, u / 12);
            if (e < 3) {
                cv->line(tx, ty, tx, ty - tr * 2, rgb(96, 64, 34), std::max(2, tr));
            } else {
                cv->fillRectGrad(tx - tr, ty - tr * 2, tr * 2, tr * 2 + tr / 2, lighten(wc, 0.2f), darken(wc, 0.7f));
                cv->fillRect(tx - tr, ty - tr * 2 - tr / 2, tr / 2 + 1, tr / 2, wc);
                cv->fillRect(tx + tr / 2, ty - tr * 2 - tr / 2, tr / 2 + 1, tr / 2, wc);
            }
        }
    }
    if (e <= 1) {  // the communal fire
        double fl = 0.75 + 0.25 * std::sin(t * 9 + c.id);
        cv->glowEllipse(sx, sy + u / 6, (int)(u / 4 * fl), (int)(u / 6 * fl), rgb(255, 170, 60), 170);
        cv->fillCircle(sx, sy + u / 6, std::max(1, u / 22), rgb(255, 220, 120));
    }
    // buildings, back to front
    static const int SPOTS[12][2] = {{0, -4}, {-3, -3}, {3, -3}, {-5, -1}, {5, -1}, {0, -1}, {-3, 1}, {3, 1}, {-5, 3}, {5, 3}, {-1, 4}, {2, 4}};
    static const int ORDER[12] = {0, 1, 2, 5, 3, 4, 6, 7, 10, 11, 8, 9};  // which spots fill first (centre outwards)
    int n = std::min(12, 3 + c.pop / 2);
    bool used[12] = {};
    for (int k = 0; k < n; k++) used[ORDER[k]] = true;
    for (int k = 0; k < 12; k++) {
        if (!used[k]) continue;
        int bx = PX(SPOTS[k][0]), by = PY(SPOTS[k][1]);
        if (e <= 1 && k == 5) continue;  // keep the fire clear
        uint32_t hh = hashXY(c.id * 31 + k, k * 7);
        cv->fillEllipseA(bx + u / 16, by + u / 40, u / 7, u / 20, rgb(0, 0, 0), 70);  // shadow
        if (e < 2) {
            // domed huts of woven branches
            int r = u / 8 + (int)(hh % 3) * u / 40;
            int dxy[26];
            for (int i = 0; i <= 12; i++) {
                double a = 3.14159265 * i / 12;
                dxy[i * 2] = bx - (int)(std::cos(a) * r);
                dxy[i * 2 + 1] = by - (int)(std::sin(a) * r * 9 / 10);
            }
            cv->fillPolyGrad(dxy, 13, rgb(196, 152, 92), 255, rgb(112, 76, 40), 255, bx - r / 2, by - r, bx + r / 2, by);
            for (int i = 1; i < 3; i++) cv->lineA(bx - r * i / 3, by, bx - r * i / 3 + r / 6, by - r * 7 / 10, rgb(90, 60, 30), 1, 120);
            cv->fillRect(bx - r / 5, by - r * 4 / 10, std::max(2, r * 2 / 5), r * 4 / 10, rgb(50, 30, 16));
        } else if (e < 6) {
            // mud-brick and stone houses with tiled roofs
            int bw = u / 4 - (int)(hh % 2) * u / 20, bh2 = u / 7 + (int)(hh % 3) * u / 30;
            Color wall = e < 4 ? rgb(206, 178, 132) : rgb(198, 190, 172);
            Color roof = k == 0 ? cc : e < 4 ? rgb(176, 88, 52) : rgb(150, 70, 56);
            cv->fillRectGrad(bx - bw / 2, by - bh2, bw / 2, bh2, lighten(wall, 0.1f), wall);
            cv->fillRectGrad(bx, by - bh2, bw - bw / 2, bh2, darken(wall, 0.78f), darken(wall, 0.68f));
            int rf[6] = {bx - bw / 2 - 2, by - bh2, bx, by - bh2 - u / 8, bx + bw - bw / 2 + 2, by - bh2};
            cv->fillPolyGrad(rf, 3, lighten(roof, 0.15f), 255, darken(roof, 0.65f), 255, bx - bw / 2, by - bh2 - u / 8, bx + bw / 2, by - bh2);
            cv->fillRect(bx - bw / 4, by - u / 14, std::max(2, u / 22), u / 14, rgb(60, 40, 30));
            if (u >= 30) cv->fillRect(bx + bw / 6, by - bh2 + bh2 / 3, std::max(2, u / 26), std::max(2, u / 26), rgb(250, 220, 140));
            if (k == 0 && c.pop >= 8 && e >= 4) {  // a domed hall or temple behind
                cv->fillCircleGrad(bx, by - bh2 - u / 8, u / 9, rgb(240, 232, 210), 255, rgb(150, 140, 120), 255, bx - u / 30, by - bh2 - u / 6);
            }
        } else {
            // industrial and modern blocks with lit windows; towers grow with the city and the era
            int bw = u / 6 + (int)(hh % 2) * u / 24;
            int bh2 = u / 5 + (int)(hh % 4) * u / 14 + (k == 0 ? u * std::min(c.pop, 20) / 60 : 0);
            Color body = e >= 7 ? rgb(150, 176, 204) : rgb(170, 150, 136);
            cv->fillRectGrad(bx - bw / 2, by - bh2, bw / 2, bh2, lighten(body, 0.25f), body);
            cv->fillRectGrad(bx, by - bh2, bw - bw / 2, bh2, darken(body, 0.7f), darken(body, 0.55f));
            if (e >= 7) cv->fillRectA(bx - bw / 2, by - bh2, bw / 2, bh2 / 3, rgb(255, 255, 255), 50);  // glass glint
            int step = std::max(3, u / 10);
            for (int wy = by - bh2 + step / 2; wy < by - 2; wy += step)
                for (int wx = bx - bw / 2 + 2; wx < bx + bw / 2 - 1; wx += std::max(3, u / 12))
                    if ((hashXY(wx * 3 + k, wy) & 3) != 0) cv->fillRect(wx, wy, std::max(1, u / 30), std::max(1, u / 22), rgb(255, 226, 140));
            if (e == 6 && (k == 1 || k == 4)) {  // a factory chimney with drifting smoke
                int cx2 = bx + bw / 3, top = by - bh2 - u / 5;
                cv->fillRect(cx2 - u / 36, top, std::max(2, u / 18), u / 5, rgb(120, 70, 56));
                if (animOn)
                    for (int p = 0; p < 3; p++) {
                        double f = std::fmod(t * 0.35 + p / 3.0 + k * 0.13, 1.0);
                        int pr = (int)(u / 16 + f * u / 8);
                        cv->fillCircleA(cx2 + (int)(f * u / 3), top - (int)(f * u / 2), pr, rgb(190, 190, 196), (int)(110 * (1 - f)));
                    }
            }
        }
    }
    if (c.capital) {
        // a pennant in the civilisation's colour, fluttering
        int fx2 = PX(0) + u / 10, fy2 = PY(-5);
        cv->line(fx2, fy2, fx2, fy2 - u / 3, rgb(60, 40, 30), std::max(1, u / 20));
        double wv = animOn ? std::sin(t * 4 + c.id) : 0;
        int tipY = fy2 - u / 4 + (int)(wv * u / 24);
        int fl[8] = {fx2, fy2 - u / 3, fx2 + u / 8, fy2 - u / 3 + u / 30 + (int)(wv * u / 40), fx2 + u / 4, tipY, fx2, fy2 - u / 6};
        cv->fillPolyGrad(fl, 4, lighten(cc, 0.2f), 255, darken(cc, 0.7f), 255, fx2, fy2 - u / 3, fx2 + u / 4, fy2 - u / 6);
    }
}

void GameUI::drawCityBanner(const City& c, int sx, int sy) {
    Game& g = *game;
    int u = hs;
    Color cc = civColor(c.owner);
    int sz = std::max(10, std::min(16, hs / 3 + 3));
    std::string name = (c.capital && !g.isMinor(c.owner) ? "* " : "") + c.name;
    int tw = cv->textWidth(name, sz, true);
    int bh = sz + 6, pw = sz + 8;
    int bw = tw + pw + 10;
    int bx = sx - bw / 2, by = sy - u - bh + 4;
    cv->fillRoundRectGrad(bx + 1, by + 2, bw, bh, 5, rgb(0, 0, 0), rgb(0, 0, 0), 110);  // shadow
    cv->fillRoundRectGrad(bx - 1, by - 1, bw + 2, bh + 2, 6, rgb(16, 14, 12), rgb(16, 14, 12));
    cv->fillRoundRectGrad(bx, by, bw, bh, 5, darken(cc, 0.62f), darken(cc, 0.34f));
    cv->fillRoundRectGrad(bx, by, pw, bh, 5, lighten(cc, 0.22f), darken(cc, 0.72f));
    cv->lineA(bx + 4, by + 1, bx + bw - 5, by + 1, rgb(255, 255, 255), 1, 60);
    cv->text(bx + pw / 2, by + 3, std::to_string(c.pop), sz, rgb(255, 255, 255), AL_CENTER, true);
    cv->textA(bx + pw + 6, by + 4, name, sz, rgb(0, 0, 0), AL_LEFT, true, 150);
    cv->text(bx + pw + 5, by + 3, name, sz, rgb(250, 250, 250), AL_LEFT, true);
    int yb = by + bh + 1;
    if (g.isMinor(c.owner)) {  // a tribe: its kind, and its ally's colour beneath
        drawTribeBadge(bx - sz / 2 - 3, by + bh / 2, sz / 2 + 2, c.owner);
        int a = g.civs[c.owner].ally;
        if (a >= 0) {
            cv->fillRect(bx, yb, bw, 4, g.civs[a].color);
            cv->rect(bx, yb, bw, 4, rgb(0, 0, 0), 1);
            yb += 5;
        }
    }
    if (c.religion >= 0 && g.civs[g.player].explored[g.idx(c.x, c.y)])  // its faith
        drawReligionSymbol(c.religion, g.isMinor(c.owner) ? bx + bw + sz / 2 + 4 : bx - sz / 2 - 3, by + bh / 2, sz / 2 + 1);
    if (c.owner == g.player && g.moodOf(c) != MOOD_CONTENT) drawMoodFace(bx + bw + sz / 2 + 6, by + bh / 2, sz / 2 + 1, g.moodOf(c));
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
    if (d.cls == UnitClass::Great) {
        if (type == ID.uMissionary) {  // a hooded preacher with a staff
            circ(-0.1, -0.5, 0.24, ic);
            poly({-0.32, -0.62, 0.12, -0.62, 0.45, 0.85, -0.65, 0.85}, ic);
            L(0.55, -0.85, 0.55, 0.9, lw);
            circ(0.55, -0.85, 0.16, accent);
            return;
        }
        if (type == ID.uGreat[GK_LAST_HUMAN]) {  // a person - an upright, hairless one
            circ(0, -0.52, 0.24, ic);
            poly({-0.12, -0.28, 0.12, -0.28, 0.55, 0.8, -0.55, 0.8}, ic);
            L(-0.2, -0.05, -0.62, 0.3, lw);
            L(0.2, -0.05, 0.62, 0.3, lw);
            return;
        }
        int xy[20];  // a star, with the mark of their gift at its heart
        for (int k = 0; k < 10; k++) {
            double a = -1.5708 + k * 0.6283, r = k % 2 ? 0.4 : 0.92;
            P(std::cos(a) * r, std::sin(a) * r + 0.05, xy[k * 2], xy[k * 2 + 1]);
        }
        cv->fillPoly(xy, 10, ic);
        circ(0, 0.05, 0.26, accent);
        return;
    }
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
        case UnitClass::Great: break;  // drawn above
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
        case UnitClass::Trader:  // a laden pack animal: a bundle tied over a back, with a coin
            poly({-0.75, 0.1, -0.55, -0.45, 0.55, -0.45, 0.75, 0.1, 0.4, 0.3, -0.4, 0.3}, ic);
            L(-0.55, 0.3, -0.6, 0.85, lw); L(0.5, 0.3, 0.55, 0.85, lw);
            L(0.0, -0.45, 0.0, 0.3, std::max(1, lw / 2));
            circ(0.55, -0.62, 0.22, accent);
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
    cv->glowEllipse(sx + r / 5, sy + r, r * 13 / 10, r * 5 / 10, rgb(0, 0, 0), 150);  // shadow on the ground
    if (d.civilian()) {
        // civilians carry a shield-shaped flag
        int sxy[10] = {sx - r - 1, sy - r, sx + r + 1, sy - r, sx + r + 1, sy + r / 4, sx, sy + r + 3, sx - r - 1, sy + r / 4};
        if (d.cls == UnitClass::Great)  // Great Primates carry a golden one
            cv->fillPolyGrad(sxy, 5, rgb(255, 236, 150), 255, rgb(170, 118, 30), 255, sx, sy - r, sx, sy + r + 3);
        else cv->fillPolyGrad(sxy, 5, rgb(238, 226, 190), 255, rgb(120, 104, 70), 255, sx, sy - r, sx, sy + r + 3);
        int ixy[10] = {sx - r + 1, sy - r + 2, sx + r - 1, sy - r + 2, sx + r - 1, sy + r / 4 - 1, sx, sy + r, sx - r + 1, sy + r / 4 - 1};
        cv->fillPolyGrad(ixy, 5, lighten(cc, 0.3f), 255, darken(cc, 0.62f), 255, sx - r / 2, sy - r, sx + r / 2, sy + r);
        cv->fillRectA(sx - r + 2, sy - r + 3, 2 * r - 4, r / 2, rgb(255, 255, 255), 40);
    } else {
        // a medallion: bronze rim, then a domed disc in the owner's colour with a glossy highlight
        cv->fillCircleGrad(sx, sy, r + 2, rgb(250, 236, 196), 255, rgb(110, 84, 46), 255, sx - r / 2, sy - r / 2);
        cv->fillCircleGrad(sx, sy, r, lighten(cc, 0.32f), 255, darken(cc, 0.58f), 255, sx - r / 3, sy - r / 2);
        cv->glowEllipse(sx - r / 6, sy - r / 2, r * 6 / 10, r * 3 / 10, rgb(255, 255, 255), 90);  // sheen
    }
    int iy = sy - (d.civilian() ? r / 10 : 0), is = r * 7 / 10;
    if (r >= 12) drawUnitIcon(u.type, sx + 1, iy + 1, is, darken(cc, light ? 0.85f : 0.3f), darken(cc, 0.3f));  // engraved shadow
    drawUnitIcon(u.type, sx, iy, is, ic, cc);
    if (u.hp < 100) {
        int hw = 2 * r, hx = sx - r, hy = sy + r + 4;
        cv->fillRoundRectGrad(hx - 1, hy - 1, hw + 2, 6, 3, rgb(10, 10, 12), rgb(30, 30, 34), 200);
        Color hc = u.hp > 50 ? rgb(90, 210, 90) : u.hp > 25 ? rgb(230, 190, 50) : rgb(220, 60, 50);
        cv->fillRoundRectGrad(hx, hy, std::max(2, hw * u.hp / 100), 4, 2, lighten(hc, 0.3f), darken(hc, 0.7f));
    }
    if (d.military() && r >= 9 && game) {  // experience: gold chevrons for each level, a glowing ring when a promotion waits
        int lv = Game::unitLevel(u);
        if (u.owner == game->player && game->canPromote(u)) {
            cv->circle(sx, sy, r + 4, rgb(255, 214, 90), 2);
            cv->fillCircle(sx - r, sy + r - 2, std::max(4, r / 3), rgb(255, 214, 90));
            cv->text(sx - r, sy + r - 2 - std::max(8, r / 2) / 2 - 1, "+", std::max(8, r / 2), rgb(60, 40, 10), AL_CENTER, true);
        } else if (lv > 0) {
            int cw = std::max(3, r / 3), n = std::min(lv, 3), bx = sx - r - 1, by = sy + r - 2;
            for (int k = 0; k < n; k++) {
                int yy = by - k * 3;
                cv->line(bx - cw, yy, bx, yy - cw * 2 / 3, rgb(20, 16, 8), 3);
                cv->line(bx, yy - cw * 2 / 3, bx + cw, yy, rgb(20, 16, 8), 3);
                cv->line(bx - cw, yy, bx, yy - cw * 2 / 3, rgb(255, 210, 90), 1);
                cv->line(bx, yy - cw * 2 / 3, bx + cw, yy, rgb(255, 210, 90), 1);
            }
        }
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

static const Color MAP_BG = {8, 10, 14};
static const uint64_t TERRAIN_STYLE = 5;  // bump to invalidate cached terrain when its drawing changes

// Draws the terrain of a block of tiles in the current coordinate system: ground, then what stands on it,
// then fog of war, the soft edge of the unexplored world, and borders.
void GameUI::drawTerrainRegion(int x0, int x1, int y0, int y1) {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!me.explored[g.idx(x, y)]) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            drawTileGround(x, y, sx, sy);
        }
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!me.explored[g.idx(x, y)]) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            drawTile(x, y, sx, sy, true);
        }
    // fog of war: remembered but unseen tiles are dimmed, not hidden
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            if (!me.explored[i] || me.visible[i]) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            int xy[12];
            hexCorners(sx, sy, hs, xy);  // not hs + 1: overlapping translucent hexes would draw a dark grid
            cv->fillPolyA(xy, 6, rgb(12, 16, 28), 125);
        }
    // the known world fades into the unknown
    if (hs >= 10)
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                if (!me.explored[g.idx(x, y)]) continue;
                int sx, sy;
                tileCenter(x, y, sx, sy);
                int e[12];
                hexCorners(sx, sy, hs + 1, e);
                for (int d = 0; d < 6; d++) {
                    Pt n;
                    if (g.neighborDir(x, y, d, n) && me.explored[g.idx(n.x, n.y)]) continue;
                    edgeFade(sx, sy, e, d, 0.6f, MAP_BG, 235);
                }
            }
    // borders: a crisp line with a soft glow of the owner's colour inside it
    int bt = std::max(2, hs / 11);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            const Tile& t = g.tiles[i];
            if (!me.explored[i] || t.owner < 0) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            int e[12], xy[12];
            hexCorners(sx, sy, hs, e);
            hexCorners(sx, sy, hs - bt / 2 - 1, xy);
            Color bc = civColor(t.owner);
            if (!me.visible[i]) bc = darken(bc, 0.6f);
            for (int d = 0; d < 6; d++) {
                Pt n;
                bool in = g.neighborDir(x, y, d, n);
                if (in && g.at(n.x, n.y).owner == t.owner) continue;
                int a = EDGE[d][0], b = EDGE[d][1];
                edgeFade(sx, sy, e, d, 0.3f, bc, 105);
                cv->lineA(xy[a * 2], xy[a * 2 + 1], xy[b * 2], xy[b * 2 + 1], darken(bc, 0.35f), bt + 2, 150);
                cv->line(xy[a * 2], xy[a * 2 + 1], xy[b * 2], xy[b * 2 + 1], bc, bt);
            }
        }
}

// Moving glints and surf on visible water, drawn every frame on top of the cached terrain.
void GameUI::drawLiveWater(int x0, int x1, int y0, int y1) {
    if (!animOn || hs < 16) return;
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    double t = animMs / 1000.0;
    int u = hs, lw = std::max(1, hs / 18);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            if (!me.visible[i] || !g.isWater(x, y)) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            uint32_t h = hashXY(x * 3 + 1, y * 5 + 2);
            for (int k = 0; k < 2; k++) {
                double ph = t * 0.9 + ((h >> (k * 8)) & 255) / 24.0;
                int a = (int)(70 * std::sin(ph));
                if (a < 8) continue;
                int wx = PX((int)((h >> (k * 5 + 1)) % 11) - 5) + (int)(std::sin(ph * 0.5) * u / 10);
                int wy = PY((int)((h >> (k * 5 + 4)) % 9) - 4);
                int s2 = u / 8 + (int)(std::sin(ph) * u / 24);
                int wv[6] = {wx - s2, wy + s2 / 4, wx, wy - s2 / 5, wx + s2, wy + s2 / 4};
                cv->polylineA(wv, 3, rgb(225, 242, 255), lw, a);
            }
            // surf rolling in to the shore
            int e[12];
            for (int d = 0; d < 6; d++) {
                Pt n;
                if (!g.neighborDir(x, y, d, n) || g.isWater(n.x, n.y) || !me.explored[g.idx(n.x, n.y)]) continue;
                double f = std::fmod(t * 0.45 + ((h >> d) & 7) / 7.0, 1.0);  // 0 = far out, 1 = on the beach
                hexCorners(sx, sy, hs - (int)(hs * 0.32 * (1.0 - f)), e);
                int a = EDGE[d][0], b = EDGE[d][1];
                int alpha = (int)(150 * std::sin(f * 3.14159265));
                if (alpha > 6) cv->lineA(e[a * 2], e[a * 2 + 1], e[b * 2], e[b * 2 + 1], rgb(240, 250, 252), std::max(1, hs / 16), alpha);
            }
        }
}

void GameUI::renderMap() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    int rowH = (int)(hs * 1.5);
    int y0 = std::max(0, (int)(camY / rowH) - 1), y1 = std::min(g.H - 1, (int)((camY + H) / rowH) + 1);
    double colW = hs * SQ3;
    int x0 = std::max(0, (int)(camX / colW) - 1), x1 = std::min(g.W - 1, (int)((camX + W) / colW) + 1);
    // The terrain (ground, plants, fog, borders) is cut into chunks that the canvas caches, so panning
    // only draws the strips that scroll into view. Backends without a cache draw it directly.
    camX = std::round(camX);
    camY = std::round(camY);
    updateMotion();
    uint64_t global = 1469598103934665603ull;
    auto mixg = [&](uint64_t v) { global = (global ^ v) * 1099511628211ull; };
    mixg(hs); mixg(g.warHappened); mixg(g.player); mixg(g.W); mixg(g.H); mixg(TERRAIN_STYLE);
    for (size_t t = 0; t < g_techs.size(); t++) if (me.techs[t]) mixg(t);
    int CH = cv->chunkSize();
    if (CH > 0) {
        double S = cv->pixelScale();
        mixg((uint64_t)std::lround(S * 1000)); mixg(CH);
        long camPX = std::lround(camX * S), camPY = std::lround(camY * S);
        auto fdiv = [](long a, long b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
        long ci0 = fdiv(camPX, CH), ci1 = fdiv(camPX + (long)std::ceil(W * S), CH);
        long cj0 = fdiv(camPY, CH), cj1 = fdiv(camPY + (long)std::ceil(H * S), CH);
        double saveX = camX, saveY = camY;
        camX = camY = 0;  // chunks are drawn in world coordinates
        for (long cj = cj0; cj <= cj1; cj++)
            for (long ci = ci0; ci <= ci1; ci++) {
                double wx0 = ci * CH / S, wy0 = cj * CH / S, wx1 = (ci + 1) * CH / S, wy1 = (cj + 1) * CH / S;
                // tiles that can touch the chunk, with a margin for trees and peaks that overhang their hex
                int tx0 = std::max(0, (int)std::floor(wx0 / colW) - 2), tx1 = std::min(g.W - 1, (int)std::ceil(wx1 / colW) + 1);
                int ty0 = std::max(0, (int)std::floor(wy0 / rowH) - 2), ty1 = std::min(g.H - 1, (int)std::ceil(wy1 / rowH) + 2);
                uint64_t key = global;
                auto mix = [&](uint64_t v) { key = (key ^ v) * 1099511628211ull; };
                for (int y = std::max(0, ty0 - 1); y <= std::min(g.H - 1, ty1 + 1); y++)
                    for (int x = std::max(0, tx0 - 1); x <= std::min(g.W - 1, tx1 + 1); x++) {
                        int i = g.idx(x, y);
                        const Tile& t = g.tiles[i];
                        mix((uint64_t)me.explored[i] | (uint64_t)me.visible[i] << 1 | (uint64_t)t.veg << 2 | (uint64_t)t.imp << 4 |
                            (uint64_t)t.fallout << 8 | (uint64_t)(uint8_t)t.owner << 12 | (uint64_t)t.res << 20 | (uint64_t)t.ruinDone << 28 |
                            (uint64_t)t.terrain << 36 | (uint64_t)t.hills << 40 | (uint64_t)t.mountain << 41 | (uint64_t)t.road << 42 |
                            (uint64_t)(t.city >= 0 && g.cities[t.city].x == x && g.cities[t.city].y == y) << 46);
                        mix((uint64_t)t.feature | (uint64_t)t.river << 8 | (uint64_t)t.riverSize << 16 | (uint64_t)(uint8_t)t.natural << 24);
                    }
                uint64_t id = ((uint64_t)(uint32_t)ci << 32) | (uint32_t)cj;
                int px = (int)(ci * CH - camPX), py = (int)(cj * CH - camPY);
                if (cv->chunkDraw(id, key, px, py)) continue;
                cv->chunkBegin(id, (int)(ci * CH), (int)(cj * CH), px, py);
                cv->fillRect((int)std::floor(wx0) - 1, (int)std::floor(wy0) - 1, (int)(wx1 - wx0) + 3, (int)(wy1 - wy0) + 3, MAP_BG);
                if (tx0 <= tx1 && ty0 <= ty1) drawTerrainRegion(tx0, tx1, ty0, ty1);
                cv->chunkEnd(id, key, px, py);
            }
        camX = saveX;
        camY = saveY;
    } else {
        cv->fillRect(0, 0, W, H, MAP_BG);
        drawTerrainRegion(x0, x1, y0, y1);
    }
    drawLiveWater(x0, x1, y0, y1);
    drawLiveFeatures(x0, x1, y0, y1);
    drawNaturalLabels(x0, x1, y0, y1);
    drawTradeRoutes();
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
        unitScreenPos(u, sx, sy);
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
    renderFx();
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

// ============================================================ roads and trade routes
// Each road tile draws half of every link to a neighbouring road tile; the two halves meet on the edge.
void GameUI::drawRoads(int x, int y, int sx, int sy) {
    Game& g = *game;
    int here = g.roadLevelAt(x, y);
    if (here <= 0 || hs < 10) return;
    bool city = g.cityAt(x, y) != nullptr;
    int links = 0;
    for (int d = 0; d < 6; d++) {
        Pt n;
        if (!g.neighborDir(x, y, d, n) || g.isWater(n.x, n.y)) continue;
        int lv = std::min(here, g.roadLevelAt(n.x, n.y));
        if (lv <= 0) continue;
        if (city && g.cityAt(n.x, n.y) && g.at(n.x, n.y).road == 0) continue;  // two cities side by side need a real road
        if (city && g.at(n.x, n.y).road == 0) continue;
        links++;
        int nx, ny;
        tileCenter(n.x, n.y, nx, ny);
        int mx = (sx + nx) / 2, my = (sy + ny) / 2;
        int w = lv == ROAD_TRACK ? std::max(2, hs / 9) : lv == ROAD_DIRT ? std::max(3, hs / 7) : std::max(3, hs / 6);
        switch (lv) {
            case ROAD_TRACK:  // a trail of trodden earth
                cv->lineA(sx, sy, mx, my, rgb(120, 92, 58), w + 1, 110);
                cv->lineA(sx, sy, mx, my, rgb(176, 146, 100), std::max(1, w - 1), 170);
                break;
            case ROAD_DIRT:
                cv->lineA(sx, sy, mx, my, rgb(96, 70, 44), w + 2, 150);
                cv->lineA(sx, sy, mx, my, rgb(170, 134, 88), w, 235);
                break;
            case ROAD_COBBLE:
                cv->lineA(sx, sy, mx, my, rgb(70, 66, 60), w + 2, 200);
                cv->lineA(sx, sy, mx, my, rgb(160, 154, 142), w, 255);
                if (hs >= 26)
                    for (int k = 1; k < 4; k++) {
                        double a = k / 4.0;
                        cv->fillCircle(sx + (int)((mx - sx) * a), sy + (int)((my - sy) * a), std::max(1, w / 3), rgb(118, 112, 104));
                    }
                break;
            case ROAD_PAVED:
                cv->lineA(sx, sy, mx, my, rgb(190, 186, 178), w + 2, 230);
                cv->lineA(sx, sy, mx, my, rgb(112, 112, 116), w, 255);
                break;
            default:  // tarmac with a dashed centre line
                cv->lineA(sx, sy, mx, my, rgb(30, 30, 34), w + 2, 230);
                cv->lineA(sx, sy, mx, my, rgb(58, 60, 66), w, 255);
                if (hs >= 22) cv->lineA(sx + (mx - sx) / 5, sy + (my - sy) / 5, sx + (mx - sx) * 3 / 5, sy + (my - sy) * 3 / 5, rgb(236, 206, 90), 1, 230);
                break;
        }
    }
    if (!links && !city) cv->fillCircle(sx, sy, std::max(2, hs / 10), here == ROAD_TRACK ? rgb(150, 120, 80) : rgb(150, 136, 110));
}

// Trade routes you run or receive, with caravans and cargo boats travelling along them.
void GameUI::drawTradeRoutes() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    double t = animMs / 1000.0;
    for (size_t ri = 0; ri < g.routes.size(); ri++) {
        const TradeRoute& r = g.routes[ri];
        bool mine = r.owner == g.player, toMe = r.to >= 0 && g.cities[r.to].owner == g.player;
        if (!mine && !toMe) continue;
        int n = std::min<int>((int)r.path.size(), 128);
        if (n < 2) continue;
        int xy[256];
        bool seen = false;
        for (int i = 0; i < n; i++) {
            int ti = r.path[i];
            tileCenter(ti % g.W, ti / g.W, xy[i * 2], xy[i * 2 + 1]);
            if (me.explored[ti]) seen = true;
        }
        if (!seen) continue;
        Color c = r.sea ? rgb(150, 210, 255) : rgb(250, 206, 110);
        cv->polylineA(xy, n, rgb(0, 0, 0), 3, 50);
        cv->polylineA(xy, n, c, 1, mine ? 170 : 100);
        // the caravan going back and forth along the way
        double speed = 1.2;  // tiles a second
        double len = n - 1;
        double p = std::fmod(t * speed + ri * 3.7, len * 2);
        if (p > len) p = len * 2 - p;
        int k = std::min(n - 2, (int)p);
        double f = p - k;
        int cx = xy[k * 2] + (int)((xy[k * 2 + 2] - xy[k * 2]) * f), cy = xy[k * 2 + 1] + (int)((xy[k * 2 + 3] - xy[k * 2 + 1]) * f);
        int ti = r.path[std::min(n - 1, (int)std::lround(p))];
        if (!me.visible[ti] || hs < 14) continue;
        int s2 = std::max(4, hs / 6);
        cv->glowEllipse(cx + 1, cy + s2, s2 * 3 / 2, s2 / 2, rgb(0, 0, 0), 110);
        if (r.sea) {  // a little cargo boat
            int hull[8] = {cx - s2 * 3 / 2, cy, cx + s2 * 3 / 2, cy, cx + s2, cy + s2 * 2 / 3, cx - s2, cy + s2 * 2 / 3};
            cv->fillPoly(hull, 4, rgb(120, 84, 50));
            int sail[6] = {cx, cy - s2 * 3 / 2, cx + s2, cy - 1, cx, cy - 1};
            cv->fillPoly(sail, 3, rgb(240, 236, 220));
            cv->line(cx, cy, cx, cy - s2 * 3 / 2, rgb(80, 60, 40), 1);
        } else {  // a pack animal under its load
            cv->fillEllipseA(cx, cy - s2 / 3, s2, s2 * 2 / 3, rgb(130, 96, 60), 255);
            cv->fillCircle(cx + s2, cy - s2 * 2 / 3, s2 / 2, rgb(120, 88, 56));
            cv->fillRect(cx - s2 * 2 / 3, cy - s2, s2 * 4 / 3, s2 * 2 / 3, civColor(r.owner));
            for (int l = -1; l <= 1; l += 2) cv->line(cx + l * s2 / 2, cy, cx + l * s2 / 2, cy + s2 * 2 / 3, rgb(90, 64, 40), std::max(1, s2 / 4));
        }
    }
    // the route the caravan screen is pointing at
    if (overlay == OV_ROUTES && routeHover >= 0 && routeHover < (int)routeOpts.size()) {
        const RouteOption& o = routeOpts[routeHover];
        int n = std::min<int>((int)o.path.size(), 128);
        if (n >= 2) {
            int xy[256];
            for (int i = 0; i < n; i++) tileCenter(o.path[i] % g.W, o.path[i] / g.W, xy[i * 2], xy[i * 2 + 1]);
            cv->polylineA(xy, n, rgb(0, 0, 0), 7, 120);
            cv->polylineA(xy, n, o.sea ? rgb(150, 210, 255) : rgb(255, 220, 120), 4, 255);
        }
    }
}

// ============================================================ motion and combat effects
void GameUI::resetMotion() {
    lastX.clear();
    lastY.clear();
    glides.clear();
    fxs.clear();
    fxSeen = game ? game->combatSeq : 0;
}

// Compares unit positions with the last frame: units that moved glide to their new tile, and new
// attacks recorded by the game start their effects.
void GameUI::updateMotion() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    if (lastX.size() > g.units.size()) resetMotion();
    lastX.resize(g.units.size(), INT32_MIN);
    lastY.resize(g.units.size(), INT32_MIN);
    for (size_t i = 0; i < g.units.size(); i++) {
        const Unit& u = g.units[i];
        if (!u.alive) { lastX[i] = INT32_MIN; glides.erase((int)i); continue; }
        if (lastX[i] != INT32_MIN && (lastX[i] != u.x || lastY[i] != u.y)) {
            int d = g.inMap(lastX[i], lastY[i]) ? g.dist(lastX[i], lastY[i], u.x, u.y) : 99;
            bool seen = d < 99 && (me.visible[g.idx(u.x, u.y)] || me.visible[g.idx(lastX[i], lastY[i])]);
            if (animOn && seen && d <= 4) glides[(int)i] = {lastX[i], lastY[i], u.x, u.y, animMs, std::min(560, 110 + 110 * d)};
            else glides.erase((int)i);
            if (u.owner == g.player && d <= 6 && u.carrier < 0) moveSound(u);
        }
        lastX[i] = u.x;
        lastY[i] = u.y;
    }
    for (auto it = glides.begin(); it != glides.end();)
        it = animMs - it->second.start >= it->second.dur ? glides.erase(it) : std::next(it);
    for (auto& c : g.combatFx) {
        if (c.seq <= fxSeen) continue;
        fxSeen = c.seq;
        if (!g.inMap(c.ax, c.ay) || !g.inMap(c.tx, c.ty)) continue;
        if (!me.visible[g.idx(c.ax, c.ay)] && !me.visible[g.idx(c.tx, c.ty)]) continue;
        combatSound(c);
        if (animOn) fxs.push_back({c, animMs});
    }
    fxs.erase(std::remove_if(fxs.begin(), fxs.end(), [&](const Fx& f) { return animMs - f.start > 1700; }), fxs.end());
}

// Where a unit is drawn this frame: its tile, or part-way along a glide, or lunging at its target.
void GameUI::unitScreenPos(const Unit& u, int& sx, int& sy) const {
    tileCenter(u.x, u.y, sx, sy);
    auto it = glides.find(u.id);
    if (it != glides.end()) {
        const Glide& gl = it->second;
        double p = std::clamp((animMs - gl.start) / (double)gl.dur, 0.0, 1.0);
        p = p * p * (3 - 2 * p);
        int ax, ay;
        tileCenter(gl.fx, gl.fy, ax, ay);
        sx = ax + (int)std::lround((sx - ax) * p);
        sy = ay + (int)std::lround((sy - ay) * p) - (int)(std::sin(p * 3.14159265) * hs / 8);  // a little hop
    }
    for (auto& f : fxs) {
        if (f.c.ranged || f.c.attacker != u.id) continue;
        int el = animMs - f.start;
        if (el < 0 || el >= 340) continue;
        int tx, ty, ax, ay;
        tileCenter(f.c.tx, f.c.ty, tx, ty);
        tileCenter(f.c.ax, f.c.ay, ax, ay);
        double k = std::sin(el / 340.0 * 3.14159265) * 0.42;
        sx += (int)((tx - ax) * k);
        sy += (int)((ty - ay) * k);
    }
}

void GameUI::renderFx() {
    for (auto& f : fxs) {
        int el = animMs - f.start;
        if (el < 0) continue;
        const CombatFx& c = f.c;
        int ax, ay, tx, ty;
        tileCenter(c.ax, c.ay, ax, ay);
        tileCenter(c.tx, c.ty, tx, ty);
        const UnitDef& d = g_units[c.atkType];
        int era = d.techId >= 0 ? g_techs[d.techId].era : 0;
        int hit = c.ranged ? 340 : 170;
        if (c.ranged && el < hit) {
            // the shot in flight: arrows and stones arc, bullets and shells streak
            double p = el / (double)hit;
            bool arc = era < 5;
            auto at = [&](double q, int& ox, int& oy) {
                ox = ax + (int)((tx - ax) * q);
                oy = ay + (int)((ty - ay) * q) - (arc ? (int)(std::sin(q * 3.14159265) * (hs * 0.9 + std::abs(tx - ax) * 0.15)) : 0);
            };
            int px, py, qx, qy;
            at(p, px, py);
            at(std::max(0.0, p - 0.12), qx, qy);
            if (arc) {
                cv->lineA(qx, qy, px, py, rgb(60, 40, 20), std::max(2, hs / 14), 230);
                cv->fillCircle(px, py, std::max(2, hs / 14), d.cls == UnitClass::Siege ? rgb(70, 64, 58) : rgb(220, 220, 210));
            } else {
                cv->lineA(qx, qy, px, py, rgb(255, 220, 120), std::max(2, hs / 12), 220);
                cv->fillCircle(px, py, std::max(2, hs / 16), rgb(255, 250, 220));
            }
            if (el < 90) cv->glowEllipse(ax, ay, hs / 2, hs / 3, rgb(255, 210, 120), (int)(200 * (1 - el / 90.0)));  // muzzle flash
            continue;
        }
        int e2 = el - hit;
        if (e2 < 0) continue;
        if (e2 < 280) {  // impact flash with sparks
            double f2 = e2 / 280.0;
            cv->glowEllipse(tx, ty, (int)(hs * (0.4 + 0.5 * f2)), (int)(hs * (0.3 + 0.4 * f2)), rgb(255, 190, 90), (int)(220 * (1 - f2)));
            for (int k = 0; k < 8; k++) {
                double a = k * 0.785398 + c.seq;
                int r1 = (int)(hs * (0.15 + 0.35 * f2)), r2 = (int)(hs * (0.3 + 0.55 * f2));
                cv->lineA(tx + (int)(std::cos(a) * r1), ty + (int)(std::sin(a) * r1), tx + (int)(std::cos(a) * r2), ty + (int)(std::sin(a) * r2),
                          rgb(255, 240, 180), std::max(1, hs / 18), (int)(255 * (1 - f2)));
            }
        }
        auto smoke = [&](int cx, int cy) {
            if (e2 >= 1300) return;
            double f3 = e2 / 1300.0;
            for (int k = 0; k < 5; k++) {
                double a = k * 1.2566 + c.seq * 0.7;
                int r = (int)(hs * (0.15 + 0.3 * f3));
                cv->fillCircleA(cx + (int)(std::cos(a) * hs * 0.3 * f3), cy - (int)(hs * 0.6 * f3) + (int)(std::sin(a) * hs * 0.15), r, rgb(110, 106, 102),
                                (int)(170 * (1 - f3)));
            }
        };
        if (c.defDied && !c.city) smoke(tx, ty);
        if (c.attDied) smoke(ax, ay);
        // damage numbers float up and fade
        if (e2 < 1300) {
            double f4 = e2 / 1300.0;
            int alpha = (int)(255 * std::min(1.0, (1 - f4) * 2.5));
            int sz = std::max(12, hs / 3 + 4);
            auto num = [&](int x, int y, int v) {
                std::string s = "-" + std::to_string(v);
                cv->textA(x + 1, y + 1, s, sz, rgb(0, 0, 0), AL_CENTER, true, alpha * 3 / 4);
                cv->textA(x, y, s, sz, rgb(255, 96, 70), AL_CENTER, true, alpha);
            };
            int rise = (int)(f4 * hs * 0.7);
            if (c.dmgDef > 0) num(tx, ty - hs / 2 - sz - rise, c.dmgDef);
            if (c.dmgAtt > 0) num(ax, ay - hs / 2 - sz - rise, c.dmgAtt);
        }
    }
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
