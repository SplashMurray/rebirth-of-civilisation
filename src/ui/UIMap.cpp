// Rebirth of Civilisation - hex map rendering
// SPDX-License-Identifier: MIT
#include "GameUI.h"
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

void GameUI::drawTile(int x, int y, int sx, int sy, bool visible) {
    Game& g = *game;
    const Tile& t = g.at(x, y);
    Color base = g_terrain[t.terrain].color;
    int v = (int)(hashXY(x, y) % 13) - 6;
    base = rgb(base.r + v, base.g + v, base.b + v);
    if (t.veg == V_FOREST) base = blend(base, rgb(52, 100, 52), 0.40f);
    if (t.veg == V_JUNGLE) base = blend(base, rgb(36, 112, 56), 0.50f);
    if (t.hills) base = blend(base, rgb(130, 110, 80), 0.22f);
    if (t.mountain) base = rgb(128 + v, 122 + v, 116 + v);
    if (t.fallout) base = blend(base, rgb(150, 168, 40), 0.16f * t.fallout);
    if (t.site >= 0 && g.warHappened && !g.isWater(x, y)) base = blend(base, rgb(70, 68, 66), 0.45f);
    Color shade = visible ? base : blend(darken(base, 0.55f), rgb(40, 44, 54), 0.25f);
    int xy[12];
    hexCorners(sx, sy, hs + 1, xy);
    cv->fillPoly(xy, 6, shade);
    if (hs >= 20) {
        Color gl = darken(shade, 0.84f);
        int c2[12];
        hexCorners(sx, sy, hs, c2);
        cv->line(c2[0], c2[1], c2[2], c2[3], gl);
        cv->line(c2[2], c2[3], c2[4], c2[5], gl);
        cv->line(c2[4], c2[5], c2[6], c2[7], gl);
    }
    auto F = [&](Color c) { return visible ? c : darken(c, 0.55f); };
    int u = hs;
    // terrain features
    if (t.mountain) {
        tri(cv, sx - u * 6 / 10, sy + u * 4 / 10, sx - u / 10, sy - u * 6 / 10, sx + u * 4 / 10, sy + u * 4 / 10, F(rgb(96, 90, 86)));
        tri(cv, sx - u / 10, sy + u * 4 / 10, sx + u * 3 / 10, sy - u * 3 / 10, sx + u * 7 / 10, sy + u * 4 / 10, F(rgb(110, 104, 98)));
        tri(cv, sx - u * 3 / 10, sy - u * 2 / 10, sx - u / 10, sy - u * 6 / 10, sx + u / 10, sy - u * 2 / 10, F(rgb(240, 240, 245)));
    } else if (t.hills) {
        Color hc = F(darken(base, 0.80f));
        bump(cv, sx - u * 3 / 10, sy + u * 3 / 10, u * 6 / 10, u * 3 / 10, hc);
        bump(cv, sx + u * 2 / 10, sy + u * 2 / 10, u * 7 / 10, u * 4 / 10, hc);
    }
    if (t.veg == V_FOREST && !t.mountain) {
        const int P[3][2] = {{-4, 1}, {3, 0}, {0, -4}};
        for (auto& p : P) {
            int tx = sx + p[0] * u / 10, ty = sy + p[1] * u / 10;
            Color tc = t.terrain == T_TUNDRA ? rgb(40, 76, 58) : rgb(30, 82, 40);
            tri(cv, tx - u / 6, ty + u / 6, tx, ty - u / 4, tx + u / 6, ty + u / 6, F(tc));
            cv->line(tx, ty + u / 6, tx, ty + u / 4, F(rgb(90, 60, 30)), std::max(1, u / 16));
        }
    } else if (t.veg == V_JUNGLE && !t.mountain) {
        const int P[4][2] = {{-4, 1}, {3, 1}, {-1, -3}, {1, 3}};
        for (auto& p : P) {
            cv->fillCircle(sx + p[0] * u / 10, sy + p[1] * u / 10, u / 6, F(rgb(22, 92, 40)));
            cv->fillCircle(sx + p[0] * u / 10 - u / 20, sy + p[1] * u / 10 - u / 20, u / 12, F(rgb(60, 140, 60)));
        }
    }
    if (t.site >= 0) drawHumanSite(x, y, sx, sy, visible);
    // improvements
    if (t.imp != IMP_NONE && hs >= 16) {
        int ix = sx - u * 4 / 10, iy = sy + u * 4 / 10;
        switch (t.imp) {
            case IMP_FARM:
                for (int k = 0; k < 3; k++) cv->line(ix - u / 5, iy - u / 6 + k * u / 8, ix + u / 5, iy - u / 6 + k * u / 8, F(rgb(220, 200, 90)), std::max(1, u / 14));
                break;
            case IMP_MINE:
                tri(cv, ix - u / 6, iy + u / 10, ix, iy - u / 6, ix + u / 6, iy + u / 10, F(rgb(50, 40, 30)));
                cv->line(ix - u / 8, iy - u / 6, ix + u / 8, iy - u / 12, F(rgb(200, 200, 200)), 2);
                break;
            case IMP_LUMBER:
                cv->fillRect(ix - u / 5, iy - u / 10, u * 2 / 5, u / 10, F(rgb(130, 80, 40)));
                cv->fillRect(ix - u / 6, iy + u / 30, u * 2 / 5, u / 10, F(rgb(150, 95, 50)));
                break;
            case IMP_TRADEPOST:
                tri(cv, ix - u / 6, iy + u / 10, ix, iy - u / 5, ix + u / 6, iy + u / 10, F(rgb(220, 180, 60)));
                break;
        }
    }
    // resource
    if (g.resourceVisible(g.player, x, y) && hs >= 16) {
        const ResourceDef& r = g_resources[t.res];
        int rx = sx + u * 4 / 10, ry = sy + u * 3 / 10, rr = std::max(4, u / 6);
        cv->fillCircle(rx, ry, rr + 1, F(rgb(20, 20, 20)));
        cv->fillCircle(rx, ry, rr, F(r.color));
        if (hs >= 34) cv->text(rx, ry + rr + 1, r.abbr, std::max(9, u / 4), F(rgb(250, 250, 250)), AL_CENTER, true);
    }
    if (t.fallout > 0 && hs >= 16) {
        drawTrefoil(sx + u * 3 / 10, sy - u * 4 / 10, std::max(4, u / 6 + t.fallout));
    }
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
    // settlement graphic
    for (int k = 0; k < 4; k++) {
        int bx = sx - u / 2 + (k % 2) * u / 2 + (k / 2) * u / 6;
        int by = sy - u / 6 + (k / 2) * u / 4;
        if (e < 3) {
            tri(cv, bx - u / 6, by + u / 8, bx, by - u / 6, bx + u / 6, by + u / 8, rgb(120, 82, 46));
            tri(cv, bx - u / 10, by + u / 8, bx, by - u / 16, bx + u / 10, by + u / 8, rgb(70, 46, 24));
        } else {
            int bh = u / 5 + (k * 7 % 3) * u / 10 + (e >= 6 ? u / 6 : 0);
            cv->fillRect(bx - u / 8, by + u / 8 - bh, u / 4, bh, e >= 6 ? rgb(170, 175, 185) : rgb(176, 150, 112));
            cv->fillRect(bx - u / 8, by + u / 8 - bh, u / 4, 2, darken(cc, 0.8f));
        }
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

void GameUI::drawUnitToken(const Unit& u, int sx, int sy, int r) {
    const UnitDef& d = g_units[u.type];
    Color cc = civColor(u.owner);
    Color txt = (cc.r * 3 + cc.g * 5 + cc.b * 2) / 10 > 150 ? rgb(20, 20, 20) : rgb(255, 255, 255);
    if (d.civilian()) {
        cv->fillRect(sx - r - 1, sy - r - 1, 2 * r + 2, 2 * r + 2, rgb(10, 10, 10));
        cv->fillRect(sx - r, sy - r, 2 * r, 2 * r, cc);
        cv->rect(sx - r + 2, sy - r + 2, 2 * r - 4, 2 * r - 4, lighten(cc, 0.35f));
    } else {
        cv->fillCircle(sx, sy, r + 2, rgb(10, 10, 10));
        cv->fillCircle(sx, sy, r, cc);
        cv->circle(sx, sy, r - 2, lighten(cc, 0.35f), 1);
        if (d.ranged()) cv->line(sx - r + 3, sy + r - 4, sx + r - 3, sy + r - 4, lighten(cc, 0.5f), 1);
    }
    int ts = std::max(9, r * 8 / 10);
    cv->text(sx, sy - ts / 2 - 1, d.abbr, ts, txt, AL_CENTER, true);
    if (u.hp < 100) {
        cv->fillRect(sx - r, sy + r + 2, 2 * r, 4, rgb(10, 10, 10));
        cv->fillRect(sx - r, sy + r + 2, 2 * r * u.hp / 100, 4, u.hp > 50 ? rgb(90, 210, 90) : u.hp > 25 ? rgb(230, 190, 50) : rgb(220, 60, 50));
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
        cv->fillRect(sx + r - ms / 2, sy - r - ms / 2, ms + 2, ms + 2, rgb(20, 20, 20));
        cv->text(sx + r + 1, sy - r - ms / 2, mark, ms, rgb(255, 230, 120), AL_CENTER, true);
    }
}

void GameUI::drawPathPreview() {
    Game& g = *game;
    Unit* u = sel();
    if (!u || u->owner != g.player || hoverX < 0 || overlay != OV_NONE) return;
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
    cv->fillRect(0, 0, W, H, rgb(8, 10, 14));
    int rowH = (int)(hs * 1.5);
    int y0 = std::max(0, (int)(camY / rowH) - 1), y1 = std::min(g.H - 1, (int)((camY + H) / rowH) + 1);
    double colW = hs * SQ3;
    int x0 = std::max(0, (int)(camX / colW) - 1), x1 = std::min(g.W - 1, (int)((camX + W) / colW) + 1);
    // pass 1: terrain
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            if (!me.explored[i]) continue;
            int sx, sy;
            tileCenter(x, y, sx, sy);
            drawTile(x, y, sx, sy, me.visible[i] != 0);
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
                cv->line(xy[a * 2], xy[a * 2 + 1], xy[b * 2], xy[b * 2 + 1], bc, bt);
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
        int sx, sy;
        tileCenter(u.x, u.y, sx, sy);
        bool civil = g_units[u.type].civilian();
        bool shared = false;
        for (auto& o : g.units) if (o.alive && o.id != u.id && o.x == u.x && o.y == u.y) shared = true;
        int rr = r;
        if (shared && civil) { sx -= hs / 3; sy += hs / 3; rr = r * 3 / 4; }
        else if (shared) { sx += hs / 8; sy -= hs / 8; }
        else if (civil) rr = r * 7 / 8;
        if (u.id == selUnit) {
            double pulse = 0.5 + 0.5 * std::sin(animMs / 160.0);
            cv->circle(sx, sy, rr + 5, blend(rgb(255, 255, 255), rgb(255, 210, 60), (float)pulse), 3);
        }
        drawUnitToken(u, sx, sy, rr);
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
    if (hoverX >= 0 && overlay == OV_NONE) {
        int sx, sy;
        tileCenter(hoverX, hoverY, sx, sy);
        int xy[12];
        hexCorners(sx, sy, hs - 1, xy);
        for (int k = 0; k < 6; k++) cv->line(xy[k * 2], xy[k * 2 + 1], xy[(k + 1) % 6 * 2], xy[(k + 1) % 6 * 2 + 1], rgb(255, 255, 255), 1);
    }
    drawPathPreview();
}
