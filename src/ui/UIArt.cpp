// Rebirth of Civilisation - illustrated art: primate portraits and the title scene
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <cmath>

namespace {
struct Look {
    Color fur, skin;
    int ears;          // 0 hidden, 1 small, 2 big
    bool flanges;      // orangutan cheek pads
    bool crest;        // gorilla
    bool muzzle;       // baboon's long snout
    bool cap;          // capuchin
    bool ring;         // gibbon's pale face ring
    bool mane;         // fluffy fur around the head
    bool lips;         // pink lips
    bool tufts;        // parted hair
    int brow;          // 0..2
};
const Look LOOKS[8] = {
    // fur                 skin                 ears flang crest muzz  cap    ring   mane   lips   tufts  brow
    {{46, 34, 28}, {200, 164, 132}, 2, false, false, false, false, false, false, false, false, 1},   // chimpanzee
    {{26, 22, 22}, {76, 60, 56}, 1, false, false, false, false, false, false, true, true, 1},        // bonobo
    {{184, 86, 36}, {104, 80, 64}, 0, true, false, false, false, false, false, false, false, 0},     // orangutan
    {{38, 38, 42}, {56, 54, 58}, 1, false, true, false, false, false, false, false, false, 2},       // gorilla
    {{156, 132, 88}, {96, 76, 74}, 1, false, false, true, false, false, true, false, false, 1},      // baboon
    {{58, 42, 30}, {226, 178, 152}, 1, false, false, false, true, false, false, false, false, 0},    // capuchin
    {{46, 40, 38}, {64, 56, 54}, 0, false, false, false, false, true, false, false, false, 0},       // gibbon
    {{168, 158, 148}, {214, 104, 92}, 0, false, false, false, false, false, true, false, false, 0},  // japanese macaque
};
}  // namespace

// A small illustrated portrait of a primate species, in a medallion of the civilisation's colour.
void GameUI::drawPortrait(int primate, int cx, int cy, int r, Color ring) {
    cv->fillCircleGrad(cx, cy, r + 2, rgb(250, 232, 180), 255, rgb(96, 72, 40), 255, cx - r / 2, cy - r / 2);  // bronze rim
    cv->fillCircleGrad(cx, cy, r, lighten(ring, 0.35f), 255, darken(ring, 0.45f), 255, cx, cy - r / 2);
    if (primate < 0 || primate >= 8 || r < 8) return;
    const Look& L = LOOKS[primate];
    auto P = [&](double f) { return (int)std::lround(f * r); };
    Color fur = L.fur, furHi = lighten(fur, 0.22f), furLo = darken(fur, 0.6f);
    int hx = cx, hy = cy + P(0.08);
    if (L.mane)
        for (int k = 0; k < 9; k++) {
            double a = 3.14159265 * (0.9 + k * 0.15);
            cv->fillCircleGrad(hx + (int)(std::cos(a) * P(0.5)), hy + (int)(std::sin(a) * P(0.42)) + P(0.1), P(0.26), furHi, 255, furLo, 255);
        }
    if (L.ears) {
        int er = L.ears == 2 ? P(0.2) : P(0.13);
        for (int s = -1; s <= 1; s += 2) {
            cv->fillCircle(hx + s * P(0.56), hy - P(0.02), er, darken(L.skin, 0.8f));
            cv->fillCircle(hx + s * P(0.56), hy - P(0.02), er * 6 / 10, darken(L.skin, 0.6f));
        }
    }
    if (L.flanges)
        for (int s = -1; s <= 1; s += 2)
            cv->fillCircleGrad(hx + s * P(0.48), hy + P(0.06), P(0.3), lighten(L.skin, 0.12f), 255, darken(L.skin, 0.6f), 255, hx + s * P(0.4), hy);
    if (L.crest) cv->fillCircleGrad(hx, hy - P(0.42), P(0.36), furHi, 255, fur, 255, hx - P(0.1), hy - P(0.6));
    cv->fillCircleGrad(hx, hy, P(0.58), furHi, 255, furLo, 255, hx - P(0.2), hy - P(0.3));  // head
    if (L.tufts)
        for (int s = -1; s <= 1; s += 2) {
            int t[8] = {hx, hy - P(0.6), hx + s * P(0.6), hy - P(0.22), hx + s * P(0.64), hy + P(0.14), hx + s * P(0.3), hy - P(0.3)};
            cv->fillPoly(t, 4, darken(fur, 0.8f));  // long hair parted in the middle
        }
    if (L.cap) {  // dark cap, pale fur around the face
        cv->fillCircleGrad(hx, hy + P(0.06), P(0.5), rgb(244, 226, 190), 255, rgb(196, 172, 130), 255);
        int capy = hy - P(0.2);
        int c[14];
        for (int i = 0; i < 7; i++) {
            double a = 3.14159265 * (1.0 + i / 6.0);
            c[i * 2] = hx + (int)(std::cos(a) * P(0.5));
            c[i * 2 + 1] = capy + (int)(std::sin(a) * P(0.4));
        }
        cv->fillPoly(c, 7, rgb(52, 38, 28));
    }
    if (L.ring) cv->fillCircle(hx, hy + P(0.05), P(0.43), rgb(232, 226, 214));
    // the bare face: eye sockets and muzzle
    Color sk = L.skin, skHi = lighten(sk, 0.18f), skLo = darken(sk, 0.72f);
    int ey = hy - P(0.08), ex = P(0.17);
    for (int s = -1; s <= 1; s += 2) cv->fillCircleGrad(hx + s * ex, ey, P(0.2), skHi, 255, sk, 255);
    if (L.muzzle) {
        int m[8] = {hx - P(0.17), ey + P(0.02), hx + P(0.17), ey + P(0.02), hx + P(0.13), hy + P(0.48), hx - P(0.13), hy + P(0.48)};
        cv->fillPolyGrad(m, 4, skHi, 255, skLo, 255, hx, ey, hx, hy + P(0.5));
        cv->fillEllipseA(hx, hy + P(0.46), P(0.15), P(0.08), darken(sk, 0.5f), 255);
    } else {
        cv->fillCircleGrad(hx, hy + P(0.2), P(0.27), skHi, 255, skLo, 255, hx - P(0.08), hy + P(0.08));
    }
    // brow
    if (L.brow) {
        int bw = L.brow == 2 ? std::max(2, P(0.1)) : std::max(1, P(0.06));
        int b[8] = {hx - P(0.36), ey - P(0.16), hx - P(0.05), ey - P(0.2), hx + P(0.05), ey - P(0.2), hx + P(0.36), ey - P(0.16)};
        cv->polylineA(b, 4, darken(L.brow == 2 ? fur : sk, 0.55f), bw, 255);
    }
    // eyes with a glint
    int er = std::max(1, P(0.075));
    for (int s = -1; s <= 1; s += 2) {
        int x = hx + s * (L.muzzle ? P(0.12) : ex);
        cv->fillCircle(x, ey, er + (r >= 16 ? 1 : 0), rgb(30, 18, 10));
        cv->fillCircle(x, ey, er, rgb(96, 56, 24));
        if (r >= 12) cv->fillCircle(x - er / 3, ey - er / 3, std::max(1, er / 3), rgb(255, 250, 240));
    }
    // nose and mouth
    int ny = L.muzzle ? hy + P(0.4) : hy + P(0.14);
    int nw = L.crest ? P(0.1) : P(0.05);
    for (int s = -1; s <= 1; s += 2) cv->fillEllipseA(hx + s * nw, ny, std::max(1, P(0.04)), std::max(1, P(0.03)), rgb(24, 16, 14), 220);
    if (!L.muzzle) {
        int my = hy + P(0.3);
        if (L.lips) cv->fillEllipseA(hx, my, P(0.14), std::max(1, P(0.05)), rgb(206, 120, 130), 255);
        cv->lineA(hx - P(0.13), my, hx + P(0.13), my, darken(sk, 0.4f), std::max(1, P(0.03)), 255);
    }
    cv->glowEllipse(cx - r / 4, cy - r / 2, r / 2, r / 4, rgb(255, 255, 255), 60);  // glassy sheen on the medallion
}

// The title screen: a ruined city at sunset under a distant mushroom cloud, seen from the jungle.
void GameUI::drawTitleScene() {
    double t = animMs / 1000.0;
    int hz = H * 3 / 4;  // horizon
    int mid = hz * 55 / 100;
    Color skyTop = rgb(14, 10, 34), skyMid = rgb(118, 44, 66), skyLow = rgb(248, 156, 74);
    cv->fillRectGrad(0, 0, W, mid + 1, skyTop, skyMid);
    cv->fillRectGrad(0, mid, W, hz - mid + 2, skyMid, skyLow);
    // stars, twinkling
    uint32_t h = 777;
    auto rnd = [&]() { h = h * 1103515245u + 12345u; return h >> 8; };
    for (int k = 0; k < 110; k++) {
        int x = (int)(rnd() % (uint32_t)std::max(1, W)), y = (int)(rnd() % (uint32_t)std::max(1, mid * 8 / 10));
        int a = (int)(110 + 90 * std::sin(t * (1.5 + (k % 5) * 0.4) + k)) * (mid - y) / std::max(1, mid);
        if (a > 10) cv->fillCircleA(x, y, k % 9 == 0 ? 2 : 1, rgb(255, 246, 230), std::min(255, a));
    }
    // the setting sun
    int sunx = W * 21 / 100, suny = hz - H / 6;
    cv->glowEllipse(sunx, suny, W * 4 / 10, H * 3 / 10, rgb(255, 196, 120), 150);
    cv->fillCircleGrad(sunx, suny, H / 11, rgb(255, 250, 226), 255, rgb(255, 176, 90), 255);
    // the mushroom cloud, slowly boiling
    int mx = W * 71 / 100, capY = hz - H * 38 / 100, cr = std::max(30, H / 18);
    cv->glowEllipse(mx, capY, cr * 6, cr * 4, rgb(255, 150, 90), 90);
    int stem[8] = {mx - cr * 7 / 10, hz, mx + cr * 7 / 10, hz, mx + cr * 4 / 10, capY + cr / 2, mx - cr * 4 / 10, capY + cr / 2};
    cv->fillPolyGrad(stem, 4, rgb(236, 150, 104), 255, rgb(150, 92, 90), 255, mx, capY, mx, hz);
    cv->fillEllipseA(mx, hz - H / 8, cr * 2, cr / 2, rgb(196, 124, 106), 190);  // the ring of debris up the stem
    for (int ring = 0; ring < 3; ring++) {
        int n = ring == 0 ? 14 : ring == 1 ? 9 : 5;
        double rx = cr * (ring == 0 ? 2.1 : ring == 1 ? 1.4 : 0.7), ry = cr * (ring == 0 ? 0.75 : ring == 1 ? 0.6 : 0.4);
        int cy0 = capY - ring * cr / 3;
        for (int k = 0; k < n; k++) {
            double a = 6.2831853 * k / n + ring * 0.3;
            int pr = cr * (ring == 2 ? 8 : 9) / 10 + (int)(std::sin(t * 0.7 + k * 1.3 + ring) * cr / 10);
            int px = mx + (int)(std::cos(a) * rx), py = cy0 + (int)(std::sin(a) * ry);
            Color lit = ring == 0 && std::sin(a) > 0.3 ? rgb(214, 140, 116) : rgb(252, 204, 150);
            cv->fillCircleGrad(px, py, pr, lit, 255, rgb(150, 90, 90), 255, px - pr / 3, py - pr / 2);
        }
    }
    cv->glowEllipse(mx, capY + cr / 3, cr * 2, cr, rgb(255, 226, 150), 150);  // the fire at its heart
    // far skyline in the haze, then the near ruins
    for (int layer = 0; layer < 2; layer++) {
        h = layer ? 99991 : 12345;
        Color bc = layer ? rgb(34, 20, 30) : rgb(120, 60, 72);
        int step = layer ? 38 : 26;
        for (int x = -10; x < W + step; x += step) {
            int bh = (layer ? 50 : 70) + (int)(rnd() % (layer ? 150u : 120u));
            int bw = step - 4 - (int)(rnd() % 6);
            int top = hz - bh;
            cv->fillRect(x, top, bw, bh + 12, bc);
            uint32_t r2 = rnd();
            if (r2 % 3 == 0) {  // broken top
                int b[8] = {x, top, x + bw / 3, top - 14, x + bw / 2, top + 6, x + bw, top - 6};
                cv->fillPoly(b, 4, bc);
            } else if (layer && r2 % 4 == 1) {  // twisted girders
                cv->line(x + bw / 3, top, x + bw / 3 + 6, top - 22, bc, 2);
                cv->line(x + bw * 2 / 3, top, x + bw * 2 / 3 - 8, top - 16, bc, 2);
            }
            if (!layer) continue;
            for (int wy = top + 8; wy < hz - 6; wy += 12)  // empty windows, catching the sunset
                for (int wx = x + 4; wx < x + bw - 4; wx += 8)
                    if ((rnd() & 7) == 0) cv->fillRectA(wx, wy, 3, 5, rgb(250, 140, 70), 110);
        }
        if (!layer) cv->fillRectGrad(0, hz - 130, W, 132, skyLow, skyLow, 0);
        if (!layer) {
            int hzq[8] = {0, hz - 140, W, hz - 140, W, hz + 2, 0, hz + 2};
            cv->fillPolyGrad(hzq, 4, skyLow, 0, skyLow, 170, 0, hz - 140, 0, hz);  // haze
        }
    }
    // ash and embers drifting up
    h = 4242;
    for (int k = 0; k < 46; k++) {
        double sp = 18 + rnd() % 40;
        int x = (int)(rnd() % (uint32_t)std::max(1, W)) + (int)(std::sin(t * 0.8 + k) * 20);
        int y = H - (int)std::fmod(t * sp + rnd() % 900, (double)(H + 40));
        Color c = k % 3 ? rgb(255, 170, 90) : rgb(200, 190, 186);
        cv->fillCircleA(x, y, k % 4 == 0 ? 2 : 1, c, 120 + (int)(80 * std::sin(t * 3 + k)));
    }
    // the jungle in the foreground
    cv->fillRectGrad(0, hz, W, H - hz, rgb(20, 40, 26), rgb(4, 10, 6));
    h = 777777;
    for (int row = 0; row < 3; row++)
        for (int x = -30; x < W + 60; x += 26 + row * 8) {
            int r = 18 + row * 10 + (int)(rnd() % 18), y = hz + 2 + row * 22 + (int)(rnd() % 20);
            Color hi = row == 0 ? rgb(46, 80, 46) : row == 1 ? rgb(30, 62, 34) : rgb(18, 42, 22);
            cv->fillCircleGrad(x, y, r, hi, 255, rgb(6, 18, 10), 255, x - r / 3, y - r / 2);
        }
    double sway = std::sin(t * 0.9) * 0.05;
    auto leaf = [&](int bx, int by, double ang, int len, int wid, Color c1, Color c2) {
        double ca = std::cos(ang), sa = std::sin(ang), na = ca;
        double nx = -sa;
        int tipx = bx + (int)(ca * len), tipy = by + (int)(sa * len);
        int mx2 = bx + (int)(ca * len * 0.45), my2 = by + (int)(sa * len * 0.45);
        int lp[8] = {bx, by, mx2 + (int)(nx * wid), my2 + (int)(na * wid), tipx, tipy, mx2 - (int)(nx * wid), my2 - (int)(na * wid)};
        cv->fillPolyGrad(lp, 4, c1, 255, c2, 255, bx, by, tipx, tipy);
        cv->lineA(bx, by, tipx, tipy, darken(c2, 0.6f), 1, 160);
    };
    for (int k = 0; k < 9; k++) {  // palm fronds rising from the bottom corners
        double a = -1.45 + k * 0.17 + sway;
        leaf(-20, H + 20, a - 0.3, H * 45 / 100, H / 22, rgb(20, 60, 30), rgb(52, 112, 56));
        leaf(W + 20, H + 20, 3.14159 - a + 0.3, H * 42 / 100, H / 24, rgb(16, 52, 26), rgb(46, 100, 50));
    }
    // vines hanging from the top-left
    for (int v = 0; v < 4; v++) {
        int vx = 30 + v * 70, len = H * (30 + v * 9) / 100;
        int pts[24];
        for (int i = 0; i < 12; i++) {
            pts[i * 2] = vx + (int)(std::sin(i * 0.6 + v + t * 0.6) * 10 * i / 11.0);
            pts[i * 2 + 1] = len * i / 11;
        }
        cv->polylineA(pts, 12, rgb(14, 34, 18), 4, 255);
        for (int i = 2; i < 12; i += 2) leaf(pts[i * 2], pts[i * 2 + 1], (i % 4 ? 0.5 : 2.6) + sway, 26, 7, rgb(20, 50, 26), rgb(40, 90, 44));
    }
    // a tree on the left, and on its branch a primate watching the end of the old world against the sun
    Color sil = rgb(10, 14, 10), rim = rgb(255, 176, 96);
    int tx0 = W * 5 / 100;
    int trunk[8] = {tx0 - 34, H, tx0 + 40, H, tx0 + 18, 0, tx0 - 16, 0};
    cv->fillPolyGrad(trunk, 4, rgb(30, 24, 18), 255, rgb(8, 8, 6), 255, tx0 - 30, 0, tx0 + 40, 0);
    int px = sunx + 10, py = suny + H / 14;
    cv->line(tx0, py + 70, px + 190, py + 6, sil, 13);
    cv->line(px + 90, py + 34, px + 150, py - 20, sil, 6);
    cv->fillEllipseA(px, py - 28, 26, 32, sil, 255);  // body
    cv->fillCircle(px + 8, py - 72, 18, sil);           // head
    cv->fillCircle(px + 22, py - 66, 8, sil);           // muzzle
    cv->fillCircle(px - 6, py - 78, 6, sil);            // ear
    cv->line(px - 14, py - 34, px - 34, py + 22, sil, 9);
    cv->line(px + 18, py - 40, px + 46, py + 14, sil, 9);
    cv->line(px - 6, py - 8, px - 8, py + 34, sil, 10);
    cv->line(px + 10, py - 8, px + 20, py + 28, sil, 10);
    int tail[10] = {px - 18, py - 6, px - 40, py + 20, px - 52, py + 60, px - 44, py + 92, px - 30, py + 104};
    cv->polylineA(tail, 5, sil, 6, 255);
    cv->glowEllipse(px + 4, py - 40, 60, 70, rim, 40);  // the sun's halo round its fur
    cv->lineA(px - 25, py - 50, px - 27, py - 8, rim, 2, 120);
    cv->lineA(px - 7, py - 86, px - 10, py - 66, rim, 2, 120);
}

// A nuclear fireball (k: 0 white-hot, 1 burnt out).
void GameUI::drawFireball(int sx, int sy, int r, double k) {
    if (r <= 0) return;
    cv->glowEllipse(sx, sy, r * 2, r * 3 / 2, rgb(255, 200, 120), (int)(220 * (1 - k * 0.7)));
    cv->fillCircleGrad(sx, sy, r, blend(rgb(255, 255, 240), rgb(255, 170, 80), (float)k), 255, blend(rgb(255, 150, 60), rgb(110, 56, 40), (float)k), 255);
}

// A mushroom cloud whose cap is 'rise' pixels above the ground; 'ms' makes it boil.
void GameUI::drawMushroom(int sx, int sy, int big, int rise, int ms) {
    if (big <= 0 || rise <= 0) return;
    double t = ms / 1000.0;
    int capY = sy - rise;
    int stem[8] = {sx - big / 4, sy, sx + big / 4, sy, sx + big / 7, capY + big / 5, sx - big / 7, capY + big / 5};
    cv->fillPolyGrad(stem, 4, rgb(226, 146, 100), 255, rgb(120, 78, 70), 255, sx, capY, sx, sy);
    if (rise > big / 2) cv->fillEllipseA(sx, sy - rise * 45 / 100, big / 2, std::max(2, big / 9), rgb(196, 126, 100), 210);
    cv->glowEllipse(sx, capY, big, big * 2 / 3, rgb(255, 160, 90), 120);
    for (int ring = 0; ring < 2; ring++) {
        int n = ring ? 5 : 9;
        for (int k = 0; k < n; k++) {
            double a = 6.2831853 * k / n + ring * 0.5;
            int pr = big * (ring ? 30 : 34) / 100 + (int)(std::sin(t * 1.3 + k + ring) * big / 30);
            int px = sx + (int)(std::cos(a) * big * (ring ? 0.3 : 0.62)), py = capY - ring * big / 6 + (int)(std::sin(a) * big * (ring ? 0.12 : 0.24));
            cv->fillCircleGrad(px, py, pr, rgb(252, 196, 136), 255, rgb(140, 84, 72), 255, px - pr / 3, py - pr / 2);
        }
    }
    cv->glowEllipse(sx, capY + big / 10, big * 2 / 3, big / 3, rgb(255, 230, 160), 160);
}
