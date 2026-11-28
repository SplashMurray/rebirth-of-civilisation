// Rebirth of Civilisation - unit art
// Units are painted as small figures standing on a base in their owner's colour: primates of the owner's own
// species (fur and face), dressed and armed for their era - a club, a spear and shield, a bow, a rifle - riding
// beasts, crewing siege engines, or aboard boats and ships. Everything is vector shapes, scaled to the token size.
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace {
const Color INK = {24, 18, 14};
const Color WOOD = {132, 92, 54};
const Color DARKWOOD = {92, 62, 36};
const Color STONE = {150, 146, 136};
const Color BRONZE = {196, 140, 62};
const Color IRON = {150, 156, 164};
const Color STEEL = {112, 120, 128};
const Color OLIVE = {104, 112, 70};
const Color GOLD = {240, 196, 70};
const Color CLOTH = {214, 200, 168};
const Color BLACKMETAL = {58, 60, 64};

// Draws shapes given in figure units: x to the right, y down, the figure's feet near y = 0.85 and its head near
// y = -0.9. A local origin and scale let one body be placed and sized inside a scene (a rider, a crew, a family).
struct Pen {
    Canvas* cv;
    double ox, oy, s;   // screen origin and size of one unit
    double lx = 0, ly = 0, k = 1;  // local placement
    int ow;             // outline width
    int X(double fx) const { return (int)std::lround(ox + (lx + fx * k) * s); }
    int Y(double fy) const { return (int)std::lround(oy + (ly + fy * k) * s); }
    int L(double v) const { return std::max(1, (int)std::lround(v * k * s)); }
    void polyv(const std::vector<double>& p, Color c, bool outline = true, float shade = 0.6f, int alpha = 255) {
        int n = (int)p.size() / 2;
        std::vector<int> xy(n * 2);
        int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
        for (int i = 0; i < n; i++) {
            xy[i * 2] = X(p[i * 2]);
            xy[i * 2 + 1] = Y(p[i * 2 + 1]);
            x0 = std::min(x0, xy[i * 2]); x1 = std::max(x1, xy[i * 2]);
            y0 = std::min(y0, xy[i * 2 + 1]); y1 = std::max(y1, xy[i * 2 + 1]);
        }
        cv->fillPolyGrad(xy.data(), n, lighten(c, 0.16f), alpha, darken(c, shade), alpha, x0, y0, x1, y1);
        if (outline && s * k >= 7) cv->polylineA(xy.data(), n, INK, ow, std::min(alpha, 210), true);
    }
    void poly(std::initializer_list<double> p, Color c, bool outline = true, float shade = 0.6f) { polyv(std::vector<double>(p), c, outline, shade); }
    void ell(double cx, double cy, double rx, double ry, Color c, bool outline = true, float shade = 0.6f, double rot = 0) {
        std::vector<double> p;
        const int N = 18;
        for (int i = 0; i < N; i++) {
            double a = i * 6.2831853 / N, ex = std::cos(a) * rx, ey = std::sin(a) * ry;
            p.push_back(cx + ex * std::cos(rot) - ey * std::sin(rot));
            p.push_back(cy + ex * std::sin(rot) + ey * std::cos(rot));
        }
        polyv(p, c, outline, shade);
    }
    // a rod with rounded look: outline first, then the colour, then a thin highlight
    void stick(double x1, double y1, double x2, double y2, double w, Color c, bool outline = true) {
        int lw = L(w);
        if (outline && s * k >= 7) cv->line(X(x1), Y(y1), X(x2), Y(y2), INK, lw + 2 * ow);
        cv->line(X(x1), Y(y1), X(x2), Y(y2), c, lw);
        if (lw >= 3) cv->lineA(X(x1) - 1, Y(y1) - 1, X(x2) - 1, Y(y2) - 1, lighten(c, 0.35f), std::max(1, lw / 3), 120);
    }
    void dot(double cx, double cy, double r, Color c) { cv->fillCircle(X(cx), Y(cy), std::max(1, (int)std::lround(r * k * s)), c); }
    void dotOut(double cx, double cy, double r, Color c) {
        int rr = std::max(1, (int)std::lround(r * k * s));
        if (s * k >= 7) cv->fillCircle(X(cx), Y(cy), rr + ow, INK);
        cv->fillCircleGrad(X(cx), Y(cy), rr, lighten(c, 0.25f), 255, darken(c, 0.6f), 255, X(cx) - rr / 2, Y(cy) - rr / 2);
    }
    void arc(double cx, double cy, double r, double a0, double a1, double w, Color c) {
        const int N = 10;
        std::vector<int> xy;
        for (int i = 0; i <= N; i++) {
            double a = a0 + (a1 - a0) * i / N;
            xy.push_back(X(cx + std::cos(a) * r));
            xy.push_back(Y(cy + std::sin(a) * r));
        }
        if (s * k >= 7) cv->polylineA(xy.data(), N + 1, INK, L(w) + 2 * ow, 230, false);
        cv->polylineA(xy.data(), N + 1, c, L(w), 255, false);
    }
    void place(double x, double y, double scale) { lx = x; ly = y; k = scale; }
};

// What a species looks like: fur, face, and how big and long-faced it is
struct Species {
    Color fur, face;
    double size = 1.0;   // gorillas are big, gibbons and capuchins small
    double muzzle = 1.0; // baboons have long dog-like faces
    bool cape = false;   // capuchins' pale shoulders, gibbons' face ring
};

Species speciesOf(const Game& g, int owner) {
    static const Species S[8] = {
        {{52, 40, 32}, {204, 166, 132}, 1.0, 1.0, false},     // chimpanzee
        {{30, 26, 26}, {84, 64, 58}, 0.97, 0.9, false},       // bonobo
        {{184, 84, 34}, {128, 92, 66}, 1.05, 1.0, false},     // orangutan
        {{40, 40, 46}, {70, 68, 74}, 1.14, 0.9, false},       // gorilla
        {{150, 126, 80}, {74, 60, 54}, 0.98, 1.5, false},     // baboon
        {{78, 54, 34}, {232, 208, 170}, 0.92, 0.9, true},     // capuchin
        {{200, 172, 128}, {44, 36, 34}, 0.9, 0.85, true},     // gibbon
        {{178, 166, 150}, {218, 104, 96}, 1.0, 0.95, false},  // snow monkey
    };
    if (owner >= 0 && owner < (int)g.civs.size()) {
        int p = g.civs[owner].primate;
        if (p >= 0 && p < 8) return S[p];
        if (g.civs[owner].tribe >= 0) {  // a tribe: tint its fur from its colour
            Color tc = g.civs[owner].color;
            Species t = {blend(rgb(70, 56, 44), tc, 0.35f), rgb(196, 160, 130), 0.88, 1.0, false};
            return t;
        }
    }
    return {{90, 70, 52}, {190, 156, 124}, 1.0, 1.0, false};
}

// clothing: 0 none, 1 loincloth/kilt, 2 tunic, 3 uniform, 4 powered armour
// helmets: 0 none, 1 leather cap, 2 bronze crest, 3 iron helm, 4 cap with visor, 5 steel helmet, 6 visored helm, 7 hood
struct Body {
    double x = 0, y = 0, sc = 1;
    int clothes = 0, helmet = 0;
    Color cloth = {180, 60, 50};
    double fx = 0.42, fy = -0.12;   // front hand
    double bx = -0.3, by = 0.05;    // back hand
    bool sitting = false;           // riders: legs hidden
    bool old = false;               // a grey muzzle
    bool robe = false;              // long robe to the feet
};

void bodyBack(Pen& p, const Species& sp, const Body& b) {
    p.place(b.x, b.y, b.sc * sp.size);
    Color fur = sp.fur, furD = darken(sp.fur, 0.72f);
    // back arm
    p.stick(-0.06, -0.3, (b.bx - 0.06) / 2, (b.by - 0.3) / 2 + 0.06, 0.14, furD);
    p.stick((b.bx - 0.06) / 2, (b.by - 0.3) / 2 + 0.06, b.bx, b.by, 0.13, furD);
    if (!b.sitting && !b.robe) {
        p.poly({-0.26, 0.18, -0.04, 0.2, -0.06, 0.8, -0.3, 0.8}, furD);   // back leg
        p.ell(-0.2, 0.82, 0.14, 0.06, darken(sp.face, 0.7f));
    }
    if (b.robe) p.poly({-0.32, -0.25, 0.3, -0.25, 0.42, 0.84, -0.44, 0.84}, b.cloth);
    // torso: hunched, broad-shouldered
    Color torso = b.clothes >= 3 || b.robe ? b.cloth : fur;
    if (b.clothes == 4) torso = rgb(96, 104, 114);
    p.ell(0, -0.04, 0.37, 0.43, torso, true, 0.55f, 0.15);
    if (sp.cape && b.clothes < 3) p.ell(0.06, -0.3, 0.3, 0.14, sp.face, false);
    if (b.clothes == 1) p.poly({-0.34, 0.12, 0.34, 0.12, 0.3, 0.34, -0.36, 0.34}, b.cloth);
    if (b.clothes == 2) {
        p.poly({-0.33, -0.22, 0.32, -0.22, 0.38, 0.36, -0.38, 0.36}, b.cloth);
        p.stick(-0.34, 0.12, 0.36, 0.12, 0.06, DARKWOOD, false);  // belt
    }
    if (b.clothes == 3) {
        p.stick(-0.34, 0.14, 0.36, 0.14, 0.07, darken(b.cloth, 0.45f), false);  // belt
        p.stick(-0.2, -0.36, 0.22, 0.12, 0.05, darken(b.cloth, 0.6f), false);   // cross strap
    }
    if (b.clothes == 4) {  // armour plates with the owner's colour on the chest
        p.poly({-0.2, -0.32, 0.26, -0.3, 0.22, 0.04, -0.18, 0.06}, b.cloth);
        p.stick(-0.3, 0.16, 0.32, 0.16, 0.07, BLACKMETAL, false);
    }
    if (!b.sitting && !b.robe) {
        Color legC = b.clothes >= 3 ? darken(b.cloth, 0.7f) : fur;
        if (b.clothes == 4) legC = rgb(86, 92, 100);
        p.poly({0.0, 0.18, 0.22, 0.18, 0.28, 0.8, 0.04, 0.8}, legC);  // front leg
        p.ell(0.18, 0.82, 0.15, 0.06, b.clothes >= 3 ? BLACKMETAL : darken(sp.face, 0.8f));
    }
    // head
    double hx = 0.17, hy = -0.6, hr = 0.25;
    p.ell(hx - 0.02, hy, hr, hr * 0.96, fur);
    if (sp.cape && b.helmet == 0) p.ell(hx + 0.02, hy + 0.02, hr * 0.82, hr * 0.8, sp.face, false);  // the pale face ring
    double mz = sp.muzzle;
    Color face = b.old ? blend(sp.face, rgb(210, 210, 210), 0.45f) : sp.face;
    p.ell(hx + 0.12 + 0.05 * (mz - 1), hy + 0.06, 0.13 * (0.8 + 0.2 * mz) + 0.03 * (mz - 1), 0.1, face, true, 0.75f);
    p.stick(hx - 0.02, hy - 0.07, hx + 0.2, hy - 0.06, 0.06, darken(fur, 0.55f), false);  // brow ridge
    if (p.s * p.k >= 11) p.dot(hx + 0.12, hy - 0.02, 0.028, rgb(16, 12, 10));             // eye
    if (b.old) p.ell(hx + 0.13, hy + 0.15, 0.08, 0.06, rgb(220, 220, 220), false);       // beard
    // helmet
    switch (b.helmet) {
        case 1: p.poly({hx - 0.26, hy - 0.04, hx - 0.2, hy - 0.22, hx, hy - 0.28, hx + 0.18, hy - 0.2, hx + 0.22, hy - 0.08}, rgb(126, 90, 56)); break;
        case 2:
            p.poly({hx - 0.27, hy - 0.0, hx - 0.22, hy - 0.22, hx, hy - 0.3, hx + 0.2, hy - 0.22, hx + 0.24, hy - 0.05, hx + 0.1, hy - 0.08, hx + 0.06, hy + 0.06,
                    hx - 0.06, hy + 0.06}, BRONZE);
            p.poly({hx - 0.24, hy - 0.26, hx - 0.06, hy - 0.46, hx + 0.16, hy - 0.42, hx + 0.08, hy - 0.26}, b.cloth);  // crest
            break;
        case 3:
            p.poly({hx - 0.27, hy - 0.02, hx - 0.2, hy - 0.24, hx, hy - 0.31, hx + 0.2, hy - 0.22, hx + 0.24, hy - 0.04}, IRON);
            p.stick(hx + 0.17, hy - 0.06, hx + 0.19, hy + 0.08, 0.04, IRON, false);  // nasal
            break;
        case 4:
            p.poly({hx - 0.22, hy - 0.08, hx - 0.2, hy - 0.34, hx + 0.16, hy - 0.34, hx + 0.18, hy - 0.08}, b.cloth);
            p.poly({hx + 0.1, hy - 0.1, hx + 0.36, hy - 0.06, hx + 0.12, hy - 0.02}, BLACKMETAL);
            break;
        case 5:
            p.poly({hx - 0.34, hy - 0.04, hx - 0.24, hy - 0.24, hx, hy - 0.32, hx + 0.24, hy - 0.24, hx + 0.34, hy - 0.04}, OLIVE);
            break;
        case 6:
            p.ell(hx, hy - 0.02, 0.29, 0.28, rgb(84, 92, 102));
            p.poly({hx + 0.04, hy - 0.1, hx + 0.3, hy - 0.08, hx + 0.28, hy + 0.04, hx + 0.04, hy + 0.04}, rgb(90, 220, 240), true, 0.4f);
            break;
        case 7: p.poly({hx - 0.3, hy + 0.1, hx - 0.24, hy - 0.24, hx, hy - 0.33, hx + 0.22, hy - 0.22, hx + 0.26, hy - 0.06, hx + 0.08, hy - 0.16, hx - 0.06, hy + 0.2}, b.cloth); break;
    }
}

void bodyFront(Pen& p, const Species& sp, const Body& b) {
    p.place(b.x, b.y, b.sc * sp.size);
    Color armC = b.clothes == 4 ? rgb(96, 104, 114) : b.clothes >= 3 || b.robe ? b.cloth : sp.fur;
    double sx = 0.14, sy = -0.3, ex = (sx + b.fx) / 2 - 0.02, ey = (sy + b.fy) / 2 + 0.1;
    p.stick(sx, sy, ex, ey, 0.15, armC);
    p.stick(ex, ey, b.fx, b.fy, 0.14, armC);
    p.dotOut(b.fx, b.fy, 0.075, b.clothes == 4 ? BLACKMETAL : sp.face);
}

void baseRing(Pen& p, Color cc, bool water) {
    p.place(0, 0, 1);
    if (water) {  // a wake on the water
        p.cv->glowEllipse(p.X(0), p.Y(0.72), p.L(1.05), p.L(0.3), rgb(230, 245, 255), 120);
        p.cv->fillEllipseA(p.X(0), p.Y(0.74), p.L(0.92), p.L(0.22), darken(cc, 0.7f), 150);
        return;
    }
    p.cv->glowEllipse(p.X(0), p.Y(-0.1), p.L(0.78), p.L(1.0), rgb(255, 250, 230), 80);  // a soft light behind, so dark fur shows on dark ground
    p.cv->glowEllipse(p.X(0.08), p.Y(0.86), p.L(1.0), p.L(0.3), rgb(0, 0, 0), 150);
    p.cv->fillEllipseA(p.X(0), p.Y(0.83), p.L(0.88), p.L(0.29), INK, 210);
    p.cv->fillEllipseA(p.X(0), p.Y(0.81), p.L(0.84), p.L(0.25), lighten(cc, 0.45f), 255);  // a bright rim
    p.cv->fillEllipseA(p.X(0), p.Y(0.82), p.L(0.76), p.L(0.21), cc, 255);
    p.cv->fillEllipseA(p.X(-0.1), p.Y(0.76), p.L(0.5), p.L(0.1), lighten(cc, 0.4f), 90);
}

// ---------------------------------------------------------------- weapons and gear
void spear(Pen& p, double hx, double hy, Color tip, double len = 1.0) {
    double ax = hx - 0.34 * len, ay = hy + 0.6 * len, bx = hx + 0.3 * len, by = hy - 0.82 * len;
    p.stick(ax, ay, bx, by, 0.07, WOOD);
    double dx = bx - ax, dy = by - ay, l = std::hypot(dx, dy);
    dx /= l; dy /= l;
    p.poly({bx + dx * 0.22, by + dy * 0.22, bx - dy * 0.07, by + dx * 0.07, bx + dy * 0.07, by - dx * 0.07}, tip);
}
void club(Pen& p, double hx, double hy) {
    p.stick(hx - 0.05, hy + 0.12, hx + 0.22, hy - 0.6, 0.1, WOOD);
    p.ell(hx + 0.25, hy - 0.66, 0.13, 0.17, DARKWOOD, true, 0.6f, 0.35);
}
void sword(Pen& p, double hx, double hy, Color blade) {
    p.poly({hx + 0.02, hy - 0.06, hx + 0.4, hy - 0.72, hx + 0.46, hy - 0.66, hx + 0.08, hy - 0.02}, blade, true, 0.5f);
    p.stick(hx - 0.08, hy - 0.08, hx + 0.12, hy + 0.04, 0.06, GOLD);  // guard
}
void roundShield(Pen& p, double x, double y, double r, Color rim, Color face) {
    p.ell(x, y, r * 0.62, r, rim, true, 0.55f);
    p.ell(x + 0.02, y, r * 0.48, r * 0.8, face, false, 0.6f);
    p.dotOut(x + 0.03, y, r * 0.18, rim);
}
void kiteShield(Pen& p, double x, double y, Color face) {
    p.poly({x - 0.16, y - 0.3, x + 0.2, y - 0.3, x + 0.18, y + 0.08, x + 0.02, y + 0.36, x - 0.14, y + 0.08}, IRON, true, 0.55f);
    p.poly({x - 0.1, y - 0.24, x + 0.14, y - 0.24, x + 0.12, y + 0.05, x + 0.02, y + 0.26, x - 0.08, y + 0.05}, face, false, 0.6f);
}
void bow(Pen& p, double hx, double hy, bool drawn) {
    p.arc(hx - 0.5, hy, 0.54, -0.95, 0.95, 0.06, WOOD);
    double tx = hx - 0.5 + std::cos(-0.95) * 0.54, ty = hy + std::sin(-0.95) * 0.54, bx2 = tx, by2 = hy - std::sin(-0.95) * 0.54;
    double sxp = drawn ? hx - 0.46 : tx;
    p.cv->line(p.X(tx), p.Y(ty), p.X(sxp), p.Y(hy), rgb(230, 220, 200), 1);
    p.cv->line(p.X(sxp), p.Y(hy), p.X(bx2), p.Y(by2), rgb(230, 220, 200), 1);
    if (drawn) {
        p.stick(sxp, hy, hx + 0.2, hy, 0.035, rgb(210, 190, 150), false);
        p.poly({hx + 0.28, hy, hx + 0.16, hy - 0.05, hx + 0.16, hy + 0.05}, STONE, false);
    }
}
void crossbow(Pen& p, double hx, double hy) {
    p.stick(hx - 0.5, hy + 0.04, hx + 0.32, hy - 0.04, 0.08, WOOD);
    p.arc(hx + 0.62, hy - 0.06, 0.42, 2.45, 3.85, 0.05, IRON);
    p.cv->line(p.X(hx + 0.62 + std::cos(2.45) * 0.42), p.Y(hy - 0.06 + std::sin(2.45) * 0.42), p.X(hx + 0.62 + std::cos(3.85) * 0.42),
               p.Y(hy - 0.06 + std::sin(3.85) * 0.42), rgb(230, 220, 200), 1);
}
// a long gun held at the shoulder and aimed forward
void rifle(Pen& p, double hx, double hy, Color stock, bool bayonet) {
    p.poly({hx - 0.62, hy + 0.06, hx - 0.28, hy - 0.04, hx - 0.26, hy + 0.06, hx - 0.6, hy + 0.2}, stock);
    p.stick(hx - 0.3, hy - 0.0, hx + 0.6, hy - 0.06, 0.06, BLACKMETAL);
    if (bayonet) p.stick(hx + 0.58, hy - 0.06, hx + 0.86, hy - 0.08, 0.03, IRON, false);
}
void helmetFor(Body& b, int era) {
    b.helmet = era <= 0 ? 0 : era == 1 ? 1 : era == 2 ? 2 : era <= 4 ? 3 : era == 5 ? 4 : era == 6 ? 5 : 6;
    b.clothes = era <= 0 ? 1 : era <= 4 ? 2 : era <= 6 ? 3 : 4;
}

// ---------------------------------------------------------------- beasts and machines
// a four-legged mount or pack beast, facing right; returns where a rider sits
void beast(Pen& p, Color hide, Color mane, Color armour, bool armoured, bool dog = false, double stride = 0) {
    double s = dog ? 0.8 : 1.0, foot = dog ? 0.62 : 0.82, lw = dog ? 0.11 : 0.09;
    Color hd = darken(hide, 0.7f);
    // far legs
    p.stick(-0.44 * s, 0.14, -0.5 * s - stride, foot - 0.02, lw, hd);
    p.stick(0.34 * s, 0.14, 0.4 * s + stride, foot - 0.02, lw, hd);
    // body
    p.ell(-0.04, 0.06, 0.6 * s, (dog ? 0.24 : 0.27) * s, hide, true, 0.55f);
    // neck and head
    if (dog) {
        p.poly({0.3, -0.06, 0.46, -0.32, 0.62, -0.3, 0.52, 0.04}, hide);
        p.poly({0.46, -0.4, 0.78, -0.28, 0.8, -0.18, 0.5, -0.16}, hide);
        p.poly({0.46, -0.38, 0.5, -0.56, 0.58, -0.36}, hd);  // ear
        p.stick(-0.46, -0.02, -0.66, -0.24, 0.06, hide);   // tail
    } else {
        p.poly({0.32, -0.08, 0.48, -0.46, 0.62, -0.5, 0.58, -0.08}, hide);
        p.poly({0.5, -0.56, 0.86, -0.36, 0.86, -0.24, 0.56, -0.32}, hide);
        p.stick(0.44, -0.4, 0.34, -0.1, 0.08, mane, false);  // mane
        p.poly({0.52, -0.54, 0.56, -0.72, 0.62, -0.54}, hd);  // ear
        p.stick(-0.62, -0.0, -0.78, 0.36, 0.07, mane);         // tail
    }
    if (armoured) p.poly({-0.46, -0.16, 0.36, -0.18, 0.44, 0.24, -0.5, 0.26}, armour, true, 0.5f);
    // near legs
    p.stick(-0.3 * s, 0.18, -0.26 * s + stride, foot, lw + 0.01, hide);
    p.stick(0.24 * s, 0.18, 0.2 * s - stride, foot, lw + 0.01, hide);
    if (!dog) {  // hooves
        p.ell(-0.26 * s + stride, foot + 0.02, 0.06, 0.035, INK, false);
        p.ell(0.2 * s - stride, foot + 0.02, 0.06, 0.035, INK, false);
    }
    if (p.s >= 11 && !dog) p.dot(0.72, -0.4, 0.025, rgb(16, 12, 10));
    if (p.s >= 11 && dog) p.dot(0.66, -0.3, 0.025, rgb(16, 12, 10));
}

void cartWheel(Pen& p, double x, double y, double r, Color c, bool spokes) {
    p.ell(x, y, r, r, c, true, 0.5f);
    p.ell(x, y, r * 0.55, r * 0.55, darken(c, 0.75f), false);
    if (spokes && p.s >= 10)
        for (int i = 0; i < 3; i++) {
            double a = i * 1.047;
            p.cv->line(p.X(x - std::cos(a) * r * 0.9), p.Y(y - std::sin(a) * r * 0.9), p.X(x + std::cos(a) * r * 0.9), p.Y(y + std::sin(a) * r * 0.9),
                       darken(c, 0.5f), 1);
        }
    p.dot(x, y, r * 0.2, INK);
}

void pennant(Pen& p, double x, double y, double h, Color cc) {
    p.stick(x, y, x, y - h, 0.04, DARKWOOD, false);
    p.poly({x, y - h, x + h * 0.55, y - h * 0.82, x, y - h * 0.64}, cc, true, 0.6f);
}

void tank(Pen& p, Color body, Color cc, bool rogue) {
    p.poly({-0.9, 0.36, 0.86, 0.36, 0.74, 0.76, -0.78, 0.76}, BLACKMETAL);  // tracks
    for (int i = 0; i < 5; i++) p.dotOut(-0.62 + i * 0.3, 0.56, 0.1, darken(body, 0.5f));
    p.poly({-0.86, 0.38, -0.7, 0.06, 0.66, 0.06, 0.9, 0.38}, body);   // hull
    p.poly({-0.42, 0.06, -0.32, -0.3, 0.28, -0.3, 0.4, 0.06}, body);  // turret
    p.stick(0.3, -0.14, 1.0, -0.2, 0.09, darken(body, 0.8f));         // gun
    if (rogue) {
        p.dotOut(0.1, -0.14, 0.09, rgb(255, 60, 40));
        p.stick(-0.2, -0.3, -0.34, -0.7, 0.03, BLACKMETAL, false);
        p.dot(-0.34, -0.72, 0.05, rgb(255, 80, 60));
        for (int i = 0; i < 4; i++) p.dot(-0.6 + i * 0.4, 0.14 + (i % 2) * 0.08, 0.05, rgb(150, 74, 40));  // rust
    } else p.poly({-0.24, -0.24, 0.16, -0.24, 0.16, -0.06, -0.24, -0.06}, cc, false);  // owner's mark on the turret
}

// ---------------------------------------------------------------- ships
void hull(Pen& p, double len, double h, Color c, bool steel) {
    double L2 = len / 2;
    if (steel) p.poly({-L2, 0.2, L2 + 0.1, 0.16, L2 - 0.12, 0.2 + h, -L2 + 0.06, 0.2 + h}, c, true, 0.55f);
    else p.poly({-L2, 0.12, -L2 + 0.12, 0.2, L2 - 0.1, 0.2, L2 + 0.08, 0.06, L2 - 0.1, 0.2 + h, -L2 + 0.16, 0.2 + h}, c, true, 0.55f);
    p.stick(-L2 + 0.06, 0.22, L2 - 0.02, 0.22, 0.03, lighten(c, 0.3f), false);
}
void sail(Pen& p, double x, double top, double bottom, double w, Color c, Color stripe) {
    p.poly({x - w * 0.5, top, x + w * 0.5, top, x + w * 0.58, bottom, x - w * 0.5, bottom}, c, true, 0.75f);
    p.poly({x - w * 0.52, (top * 0.62 + bottom * 0.38), x + w * 0.54, (top * 0.62 + bottom * 0.38), x + w * 0.56, (top * 0.4 + bottom * 0.6),
            x - w * 0.51, (top * 0.4 + bottom * 0.6)}, stripe, false);
}

// ---------------------------------------------------------------- which picture each unit gets
enum Art {
    A_BAND, A_ELDER, A_GATHERER, A_TINKERER, A_SCOUT, A_SONGSCOUT, A_WARBAND, A_SILVERBACK, A_STONE, A_SPEAR, A_HUNT, A_PHALANX, A_BOW,
    A_PEACEKEEP, A_BEASTRIDER, A_RAIDER, A_IRONBLADE, A_ONSEN, A_SLING, A_CROSSBOW, A_STALKER, A_ARMOURED, A_SALVRIFLE, A_BOMBARD,
    A_RIFLEMEN, A_FIELDGUN, A_INFANTRY, A_TANK, A_NUKE, A_CANOE, A_GALLEY, A_ARCHGALLEY, A_CARAVEL, A_FRIGATE, A_FREIGHTER, A_IRONCLAD,
    A_GUNBOAT, A_DESTROYER, A_BATTLESHIP, A_EXO, A_CARAVAN, A_THINKER, A_ARTISAN, A_MERCHANT, A_STORYTELLER, A_WARLEADER, A_MISSIONARY,
    A_HUMAN, A_DOGS, A_HOUNDS, A_MACHINES, A_GENERIC
};

int artOf(int type) {
    static std::vector<int> cache;
    if (cache.size() != g_units.size()) {
        static const std::map<std::string, int> M = {
            {"Roaming Band", A_BAND}, {"Elder Troop", A_ELDER}, {"Gatherer", A_GATHERER}, {"Tinkerer", A_TINKERER}, {"Scout", A_SCOUT},
            {"Song Scout", A_SONGSCOUT}, {"Warband", A_WARBAND}, {"Silverback Guard", A_SILVERBACK}, {"Stone Thrower", A_STONE},
            {"Spear Troop", A_SPEAR}, {"Hunting Party", A_HUNT}, {"Bronze Phalanx", A_PHALANX}, {"Bow Troop", A_BOW}, {"Peacekeepers", A_PEACEKEEP},
            {"Beast Riders", A_BEASTRIDER}, {"Baboon Raiders", A_RAIDER}, {"Iron Blades", A_IRONBLADE}, {"Onsen Guard", A_ONSEN},
            {"Siege Sling", A_SLING}, {"Crossbows", A_CROSSBOW}, {"Canopy Stalkers", A_STALKER}, {"Armoured Riders", A_ARMOURED},
            {"Salvaged Rifles", A_SALVRIFLE}, {"Bombard", A_BOMBARD}, {"Riflemen", A_RIFLEMEN}, {"Field Guns", A_FIELDGUN}, {"Infantry", A_INFANTRY},
            {"Rebuilt Tanks", A_TANK}, {"Nuclear Warhead", A_NUKE}, {"Dugout Canoe", A_CANOE}, {"War Galley", A_GALLEY},
            {"Archer Galley", A_ARCHGALLEY}, {"Caravel", A_CARAVEL}, {"Frigate", A_FRIGATE}, {"Salvaged Freighter", A_FREIGHTER},
            {"Ironclad", A_IRONCLAD}, {"River Gunboat", A_GUNBOAT}, {"Salvaged Destroyer", A_DESTROYER}, {"Salvaged Battleship", A_BATTLESHIP},
            {"Exo-Primates", A_EXO}, {"Caravan", A_CARAVAN}, {"Great Thinker", A_THINKER}, {"Great Artisan", A_ARTISAN},
            {"Great Merchant", A_MERCHANT}, {"Great Storyteller", A_STORYTELLER}, {"Great Warleader", A_WARLEADER}, {"Missionary", A_MISSIONARY},
            {"The Last Human", A_HUMAN}, {"Feral Dogs", A_DOGS}, {"Mutant Hounds", A_HOUNDS}, {"Rogue Machines", A_MACHINES}};
        cache.assign(g_units.size(), A_GENERIC);
        for (size_t i = 0; i < g_units.size(); i++) {
            auto it = M.find(g_units[i].name);
            if (it != M.end()) cache[i] = it->second;
        }
    }
    return type >= 0 && type < (int)cache.size() ? cache[type] : A_GENERIC;
}
}  // namespace

bool GameUI::unitHasFigure(int type) const { return artOf(type) != A_GENERIC; }

// The figure for one unit, centred on (sx, sy) and about 2r tall.
void GameUI::drawUnitFigure(int type, int owner, int sx, int sy, int r, int stride) {
    const UnitDef& d = g_units[type];
    Game& g = *game;
    Color cc = owner >= 0 && owner < (int)g.civs.size() ? civColor(owner) : rgb(160, 160, 160);
    Pen p{cv, (double)sx, (double)sy, (double)r};
    p.ow = r >= 26 ? 2 : 1;
    Species sp = speciesOf(g, owner);
    int art = artOf(type);
    int era = d.techId >= 0 ? g_techs[d.techId].era : 0;
    bool feral = d.primate == -2 && d.cls != UnitClass::Great;
    baseRing(p, feral ? rgb(120, 120, 120) : cc, d.naval());
    double st = stride * 0.08;
    Body b;
    b.cloth = cc;
    auto soldier = [&](int e) { helmetFor(b, e); };
    switch (art) {
        // -------------------------------------------------- civilians
        case A_BAND: {
            Body a = b, c2 = b, kid = b;
            a.x = -0.36; a.sc = 0.7; a.y = 0.22; a.fx = 0.36; a.fy = -0.02; a.clothes = 0;
            c2.x = 0.32; c2.sc = 0.72; c2.y = 0.2; c2.fx = 0.4; c2.fy = 0.0; c2.clothes = 0;
            kid.x = -0.02; kid.y = 0.42; kid.sc = 0.45; kid.fx = 0.3; kid.fy = -0.1;
            bodyBack(p, sp, a); bodyFront(p, sp, a);
            bodyBack(p, sp, c2);
            p.place(c2.x, c2.y, c2.sc * sp.size);
            p.stick(0.42, -0.0, 0.5, -1.0, 0.06, WOOD);  // walking staff
            bodyFront(p, sp, c2);
            bodyBack(p, sp, kid); bodyFront(p, sp, kid);
            break;
        }
        case A_ELDER: {
            b.old = true; b.robe = true; b.cloth = blend(CLOTH, cc, 0.35f); b.fx = 0.44; b.fy = -0.1; b.sc = 0.92;
            p.place(b.x, b.y, b.sc * sp.size);
            p.ell(-0.34, -0.2, 0.22, 0.26, rgb(150, 112, 70));  // bundle on the back
            p.stick(-0.48, -0.36, -0.2, -0.04, 0.04, DARKWOOD, false);
            bodyBack(p, sp, b);
            p.stick(0.5, -0.6, 0.46, 0.84, 0.06, WOOD);  // staff
            bodyFront(p, sp, b);
            break;
        }
        case A_GATHERER: case A_TINKERER: {
            b.clothes = 1; b.fx = 0.38; b.fy = 0.0;
            p.place(b.x, b.y, sp.size);
            p.ell(-0.36, -0.06, 0.2, 0.24, rgb(176, 136, 76));  // basket on the back
            p.stick(-0.54, -0.18, -0.18, -0.18, 0.03, DARKWOOD, false);
            bodyBack(p, sp, b);
            if (art == A_GATHERER) {  // a hoe
                p.stick(b.fx - 0.12, b.fy + 0.5, b.fx + 0.16, b.fy - 0.62, 0.06, WOOD);
                p.poly({b.fx + 0.1, b.fy - 0.66, b.fx + 0.42, b.fy - 0.54, b.fx + 0.4, b.fy - 0.46, b.fx + 0.14, b.fy - 0.52}, STONE);
            } else {  // a big spanner
                p.stick(b.fx - 0.06, b.fy + 0.08, b.fx + 0.26, b.fy - 0.5, 0.07, IRON);
                p.poly({b.fx + 0.18, b.fy - 0.5, b.fx + 0.26, b.fy - 0.66, b.fx + 0.36, b.fy - 0.62, b.fx + 0.32, b.fy - 0.52, b.fx + 0.4, b.fy - 0.46,
                        b.fx + 0.34, b.fy - 0.38}, IRON);
            }
            bodyFront(p, sp, b);
            break;
        }
        case A_CARAVAN: {
            p.place(0.08, -0.04, 0.82);
            beast(p, rgb(176, 150, 112), rgb(110, 86, 60), cc, false, false, st);
            p.ell(-0.08, -0.22, 0.3, 0.2, rgb(198, 162, 104));  // packs
            p.ell(0.18, -0.2, 0.2, 0.17, cc);
            p.stick(-0.3, -0.16, 0.34, -0.14, 0.03, DARKWOOD, false);
            b.x = -0.62; b.y = 0.2; b.sc = 0.62; b.clothes = 2; b.fx = 0.5; b.fy = -0.1;
            bodyBack(p, sp, b); bodyFront(p, sp, b);
            break;
        }
        case A_MISSIONARY: {
            b.robe = true; b.helmet = 7; b.cloth = rgb(230, 222, 204); b.fx = 0.44; b.fy = -0.14;
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            p.poly({-0.3, -0.25, 0.3, -0.25, 0.08, 0.84, -0.08, 0.84}, cc, false);  // a stole in the owner's colour
            p.stick(0.5, -0.86, 0.46, 0.84, 0.05, WOOD);
            p.dotOut(0.5, -0.92, 0.1, GOLD);
            bodyFront(p, sp, b);
            break;
        }
        case A_HUMAN: {  // tall, upright, hairless, in a coat
            p.place(0, 0, 1);
            Color skin = rgb(226, 186, 150), coat = rgb(96, 88, 80);
            p.poly({-0.14, 0.1, 0.02, 0.1, 0.0, 0.84, -0.16, 0.84}, rgb(60, 60, 72));
            p.poly({0.04, 0.1, 0.18, 0.1, 0.2, 0.84, 0.04, 0.84}, rgb(60, 60, 72));
            p.poly({-0.26, -0.5, 0.22, -0.5, 0.28, 0.4, -0.3, 0.4}, coat);
            p.stick(-0.22, -0.44, -0.3, 0.16, 0.11, coat);
            p.stick(0.2, -0.44, 0.34, 0.12, 0.11, coat);
            p.ell(-0.02, -0.76, 0.17, 0.2, skin);
            p.poly({-0.2, -0.82, -0.16, -0.98, 0.1, -0.98, 0.18, -0.86, 0.0, -0.9}, rgb(110, 70, 40));  // hair
            p.ell(0.36, 0.06, 0.1, 0.14, rgb(200, 180, 120));  // a book
            break;
        }
        case A_THINKER: case A_ARTISAN: case A_MERCHANT: case A_STORYTELLER: case A_WARLEADER: {
            p.place(0, -0.1, 1);
            p.cv->glowEllipse(p.X(0), p.Y(-0.2), p.L(0.95), p.L(1.05), rgb(255, 226, 120), 150);  // their renown
            b.robe = art != A_WARLEADER; b.clothes = art == A_WARLEADER ? 2 : 0; b.cloth = blend(cc, GOLD, 0.3f);
            if (art == A_WARLEADER) b.helmet = 2;
            b.fx = 0.4; b.fy = -0.2;
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            switch (art) {
                case A_THINKER: p.poly({0.26, -0.34, 0.6, -0.3, 0.58, -0.02, 0.24, -0.06}, rgb(236, 226, 196)); break;  // an open book
                case A_ARTISAN:
                    p.stick(0.36, -0.06, 0.62, -0.62, 0.06, WOOD);
                    p.poly({0.5, -0.72, 0.76, -0.6, 0.7, -0.48, 0.46, -0.6}, IRON);
                    break;
                case A_MERCHANT: p.ell(0.5, 0.02, 0.17, 0.19, rgb(170, 130, 70)); p.dot(0.5, -0.02, 0.06, GOLD); break;  // a purse
                case A_STORYTELLER: p.ell(0.52, -0.1, 0.16, 0.2, rgb(150, 100, 60)); p.ell(0.52, -0.1, 0.12, 0.16, rgb(230, 210, 170), false); break;  // a drum
                case A_WARLEADER:
                    p.stick(0.46, 0.6, 0.5, -1.04, 0.05, DARKWOOD);
                    p.poly({0.5, -1.02, 0.98, -0.9, 0.5, -0.64}, cc);
                    break;
            }
            bodyFront(p, sp, b);
            break;
        }
        // -------------------------------------------------- feral beasts
        case A_DOGS: case A_HOUNDS: {
            bool mut = art == A_HOUNDS;
            p.place(mut ? -0.04 : 0.12, mut ? 0.16 : 0.24, mut ? 1.0 : 0.82);
            beast(p, mut ? rgb(122, 138, 92) : rgb(150, 120, 86), rgb(80, 60, 40), cc, false, true, st);
            if (mut) for (int i = 0; i < 4; i++) p.poly({-0.36 + i * 0.18, -0.12, -0.3 + i * 0.18, -0.36, -0.24 + i * 0.18, -0.12}, rgb(200, 196, 170));
            if (!mut) {
                p.place(-0.42, 0.4, 0.62);
                beast(p, rgb(110, 92, 70), rgb(70, 50, 30), cc, false, true, -st);
            }
            break;
        }
        case A_MACHINES: p.place(0, 0.02, 0.9); tank(p, rgb(128, 96, 70), cc, true); break;
        // -------------------------------------------------- foot soldiers
        case A_WARBAND: case A_SILVERBACK: {
            b.clothes = 1; b.fx = 0.4; b.fy = -0.2;
            if (art == A_SILVERBACK) { b.sc = 1.12; b.clothes = 0; sp.fur = rgb(44, 44, 50); sp.face = rgb(70, 68, 74); sp.size = 1.0; }
            bodyBack(p, sp, b);
            if (art == A_SILVERBACK) { p.place(b.x, b.y, b.sc); p.ell(-0.12, -0.18, 0.24, 0.12, rgb(176, 176, 182), false); }  // the silver saddle
            p.place(b.x, b.y, b.sc * sp.size);
            club(p, b.fx, b.fy);
            bodyFront(p, sp, b);
            break;
        }
        case A_STONE: {
            b.clothes = 1; b.fx = 0.5; b.fy = -0.24; b.bx = -0.5; b.by = -0.78;
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            p.dotOut(-0.54, -0.86, 0.1, STONE);  // the stone, drawn back to throw
            p.ell(-0.3, 0.12, 0.12, 0.1, rgb(150, 110, 70));  // a pouch of more
            bodyFront(p, sp, b);
            break;
        }
        case A_SCOUT: case A_SONGSCOUT: {  // light-footed, shading their eyes to look far ahead
            b.clothes = 1; b.helmet = art == A_SCOUT ? 0 : 0;
            b.fx = 0.4; b.fy = -0.72; b.bx = -0.34; b.by = 0.1;
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            p.stick(-0.5, 0.84, -0.3, -0.5, 0.05, WOOD);  // a walking stick
            if (art == A_SONGSCOUT) {  // a horn to call across the canopy
                p.poly({-0.22, -0.06, -0.62, -0.28, -0.66, -0.12, -0.24, 0.04}, rgb(226, 210, 170));
                p.ell(-0.64, -0.2, 0.05, 0.1, rgb(190, 170, 130));
            }
            bodyFront(p, sp, b);
            break;
        }
        case A_SPEAR: case A_HUNT: case A_PHALANX: {
            soldier(era);
            if (art == A_HUNT) { b.clothes = 1; b.helmet = 0; }
            b.fx = 0.34; b.fy = -0.18;
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            spear(p, b.fx, b.fy, art == A_PHALANX ? BRONZE : STONE, art == A_PHALANX ? 1.12 : 1.0);
            if (art == A_HUNT) p.poly({0.52, -0.92, 0.6, -0.74, 0.48, -0.78}, rgb(230, 230, 220));  // a feather
            bodyFront(p, sp, b);
            if (art == A_PHALANX) { p.place(0, 0, sp.size); roundShield(p, 0.34, 0.06, 0.42, BRONZE, cc); }
            break;
        }
        case A_BOW: case A_PEACEKEEP: {
            soldier(era);
            if (art == A_PEACEKEEP) { b.helmet = 4; b.cloth = blend(cc, rgb(90, 150, 220), 0.5f); }
            b.fx = 0.5; b.fy = -0.3; b.bx = 0.1; b.by = -0.32;
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            p.poly({-0.42, -0.36, -0.28, -0.42, -0.2, 0.06, -0.34, 0.08}, rgb(130, 90, 54));  // quiver
            bow(p, b.fx, b.fy, true);
            bodyFront(p, sp, b);
            break;
        }
        case A_IRONBLADE: case A_ONSEN: {
            soldier(era);
            b.fx = 0.38; b.fy = -0.06;
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            sword(p, b.fx, b.fy, rgb(206, 212, 220));
            bodyFront(p, sp, b);
            p.place(0, 0, sp.size);
            if (art == A_ONSEN) roundShield(p, -0.06, 0.08, 0.36, DARKWOOD, cc);
            else kiteShield(p, -0.04, 0.08, cc);
            break;
        }
        case A_CROSSBOW: case A_STALKER: {
            soldier(era);
            if (art == A_STALKER) { b.helmet = 0; b.cloth = blend(cc, rgb(60, 110, 50), 0.5f); }
            b.fx = 0.42; b.fy = -0.28; b.bx = 0.0; b.by = -0.26;
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            crossbow(p, b.fx, b.fy);
            bodyFront(p, sp, b);
            if (art == A_STALKER) {  // leaves for camouflage
                p.place(0, 0, sp.size);
                for (int i = 0; i < 4; i++) p.ell(-0.24 + i * 0.12, -0.86 + (i % 2) * 0.06, 0.09, 0.05, rgb(70 + i * 10, 130, 60), false, 0.6f, 0.6 * i);
            }
            break;
        }
        case A_SALVRIFLE: case A_RIFLEMEN: case A_INFANTRY: case A_EXO: {
            soldier(era);
            if (art == A_SALVRIFLE) { b.helmet = 1; b.clothes = 2; }
            if (art == A_EXO) b.sc = 1.08;
            b.fx = 0.42; b.fy = -0.3; b.bx = 0.02; b.by = -0.28;
            bodyBack(p, sp, b);
            p.place(0, 0, b.sc * sp.size);
            if (art == A_EXO) {
                p.poly({-0.34, -0.42, -0.08, -0.46, -0.08, 0.1, -0.36, 0.08}, rgb(76, 82, 90));  // power pack
                p.dot(-0.22, -0.3, 0.05, rgb(90, 220, 240));
                p.stick(b.fx - 0.3, b.fy, b.fx + 0.66, b.fy - 0.04, 0.12, BLACKMETAL);
                p.stick(b.fx + 0.4, b.fy - 0.03, b.fx + 0.66, b.fy - 0.04, 0.05, rgb(90, 220, 240), false);
            } else rifle(p, b.fx, b.fy, art == A_SALVRIFLE ? rgb(120, 90, 70) : WOOD, art == A_RIFLEMEN);
            bodyFront(p, sp, b);
            break;
        }
        // -------------------------------------------------- riders
        case A_BEASTRIDER: case A_RAIDER: case A_ARMOURED: {
            bool arm = art == A_ARMOURED;
            p.place(0.02, 0.0, 0.95);
            beast(p, art == A_RAIDER ? rgb(150, 126, 90) : rgb(176, 134, 92), rgb(70, 50, 36), cc, arm, false, st);
            b.sitting = true; b.x = -0.1; b.y = -0.46; b.sc = 0.74;
            soldier(era);
            if (!arm) b.helmet = era >= 1 ? 1 : 0;
            b.fx = 0.4; b.fy = -0.1;
            bodyBack(p, sp, b);
            p.place(b.x, b.y, b.sc * sp.size);
            p.poly({-0.02, 0.2, 0.22, 0.2, 0.3, 0.72, 0.1, 0.74}, b.clothes >= 3 ? darken(b.cloth, 0.7f) : sp.fur);  // a leg astride
            p.ell(0.22, 0.76, 0.12, 0.05, darken(sp.face, 0.7f));
            if (arm) {
                p.stick(-0.3, 0.2, 1.3, -0.4, 0.07, WOOD);
                p.poly({1.3, -0.4, 1.12, -0.28, 1.1, -0.42}, IRON);
            } else spear(p, b.fx, b.fy, art == A_RAIDER ? STONE : BRONZE, 0.95);
            bodyFront(p, sp, b);
            break;
        }
        // -------------------------------------------------- siege and armour
        case A_SLING: {
            p.place(0, 0, 1);
            p.poly({-0.8, 0.36, 0.7, 0.36, 0.62, 0.52, -0.74, 0.52}, WOOD);
            p.poly({-0.2, 0.36, 0.0, -0.3, 0.12, -0.3, 0.2, 0.36}, DARKWOOD);
            p.stick(-0.7, 0.32, 0.6, -0.72, 0.08, WOOD);  // the throwing arm
            p.ell(0.62, -0.78, 0.13, 0.08, rgb(120, 90, 60));
            p.dotOut(0.62, -0.86, 0.08, STONE);
            cartWheel(p, -0.5, 0.62, 0.2, DARKWOOD, true);
            cartWheel(p, 0.4, 0.62, 0.2, DARKWOOD, true);
            pennant(p, -0.72, 0.3, 0.7, cc);
            break;
        }
        case A_BOMBARD: case A_FIELDGUN: {
            bool fg = art == A_FIELDGUN;
            p.place(0, 0, 1);
            p.poly({-0.82, 0.5, -0.1, 0.24, 0.12, 0.32, -0.7, 0.62}, fg ? OLIVE : WOOD);  // trail
            if (fg) {
                p.stick(-0.2, 0.12, 0.94, -0.32, 0.12, rgb(90, 96, 80));
                p.poly({-0.1, -0.24, 0.16, -0.3, 0.2, 0.3, -0.06, 0.36}, OLIVE);  // gun shield
            } else {
                p.poly({-0.3, 0.2, 0.8, -0.3, 0.9, -0.08, -0.2, 0.42}, rgb(70, 72, 78));
                p.ell(0.86, -0.19, 0.08, 0.13, BLACKMETAL, true, 0.6f, 0.4);
            }
            cartWheel(p, 0.0, 0.5, 0.34, fg ? rgb(80, 84, 70) : WOOD, true);
            pennant(p, -0.78, 0.46, 0.9, cc);
            break;
        }
        case A_TANK: p.place(0, 0.02, 0.92); tank(p, rgb(100, 112, 76), cc, false); break;
        case A_NUKE: {
            p.place(0, 0, 1);
            p.poly({-0.8, 0.42, 0.8, 0.42, 0.7, 0.62, -0.7, 0.62}, OLIVE);
            cartWheel(p, -0.5, 0.66, 0.14, BLACKMETAL, false);
            cartWheel(p, 0.5, 0.66, 0.14, BLACKMETAL, false);
            p.poly({-0.7, 0.3, 0.6, 0.06, 0.9, 0.0, 0.62, -0.08, -0.7, 0.08}, rgb(226, 228, 230), true, 0.6f);  // the missile, raised
            p.poly({-0.7, 0.08, -0.9, -0.12, -0.6, 0.06}, cc);
            p.poly({-0.7, 0.3, -0.9, 0.42, -0.62, 0.28}, cc);
            p.dotOut(0.2, 0.12, 0.08, rgb(250, 210, 40));
            break;
        }
        // -------------------------------------------------- boats and ships
        case A_CANOE: {
            p.place(0, 0.12, 1);
            p.poly({-0.9, 0.3, 0.9, 0.24, 0.66, 0.5, -0.7, 0.52}, rgb(120, 82, 46));
            b.x = 0.0; b.y = -0.12; b.sc = 0.62; b.sitting = true; b.clothes = 1; b.fx = 0.5; b.fy = 0.1; b.bx = 0.1; b.by = 0.1;
            bodyBack(p, sp, b);
            p.place(b.x, b.y, b.sc * sp.size);
            p.stick(0.0, -0.5, 0.7, 0.9, 0.06, WOOD);  // paddle
            p.ell(0.72, 0.94, 0.08, 0.14, WOOD, true, 0.6f, -0.5);
            bodyFront(p, sp, b);
            p.place(0, 0.12, 1);
            p.poly({-0.9, 0.3, 0.9, 0.24, 0.84, 0.32, -0.86, 0.38}, rgb(150, 106, 64), false);
            break;
        }
        case A_GALLEY: case A_ARCHGALLEY: {
            p.place(0, 0.1, 1);
            for (int i = 0; i < 4; i++) p.stick(-0.5 + i * 0.3, 0.36, -0.62 + i * 0.3, 0.7, 0.04, DARKWOOD, false);  // oars
            p.stick(0.0, 0.2, 0.0, -0.96, 0.06, DARKWOOD);
            sail(p, 0.02, -0.86, -0.08, 0.86, rgb(236, 226, 200), cc);
            hull(p, 1.8, 0.32, rgb(122, 84, 50), false);
            p.poly({0.88, 0.32, 1.06, 0.38, 0.86, 0.44}, BRONZE);  // ram
            if (art == A_ARCHGALLEY) { Body a; a.x = -0.6; a.y = -0.04; a.sc = 0.36; a.sitting = true; a.cloth = cc; a.clothes = 2; a.helmet = 1; a.fx = 0.5; a.fy = -0.3; a.bx = 0.1; a.by = -0.32; bodyBack(p, sp, a); p.place(a.x, a.y, a.sc * sp.size); bow(p, 0.5, -0.3, true); bodyFront(p, sp, a); }
            break;
        }
        case A_CARAVEL: case A_FRIGATE: {
            bool fr = art == A_FRIGATE;
            p.place(0, 0.08, 1);
            p.stick(-0.3, 0.2, -0.3, -0.98, 0.05, DARKWOOD);
            p.stick(0.3, 0.2, 0.3, -0.84, 0.05, DARKWOOD);
            if (fr) p.stick(-0.78, 0.2, -0.78, -0.6, 0.04, DARKWOOD);
            sail(p, -0.28, -0.9, -0.42, 0.56, rgb(240, 234, 214), cc);
            sail(p, -0.28, -0.38, 0.02, 0.62, rgb(240, 234, 214), rgb(240, 234, 214));
            sail(p, 0.32, -0.76, -0.04, 0.5, rgb(240, 234, 214), cc);
            if (fr) sail(p, -0.76, -0.54, -0.04, 0.34, rgb(240, 234, 214), rgb(240, 234, 214));
            hull(p, 1.9, 0.38, fr ? rgb(70, 52, 40) : rgb(128, 88, 52), false);
            if (fr) for (int i = 0; i < 5; i++) p.cv->fillRect(p.X(-0.62 + i * 0.28), p.Y(0.34), p.L(0.08), p.L(0.06), rgb(240, 210, 90));  // gun ports
            pennant(p, -0.3, -0.98, 0.3, cc);
            break;
        }
        case A_FREIGHTER: case A_IRONCLAD: case A_GUNBOAT: case A_DESTROYER: case A_BATTLESHIP: {
            p.place(0, 0.12, 1);
            Color hc = art == A_IRONCLAD ? rgb(58, 60, 66) : art == A_FREIGHTER ? rgb(150, 70, 50) : rgb(128, 136, 146);
            Color sup = art == A_FREIGHTER ? rgb(220, 214, 200) : lighten(hc, 0.15f);
            double len = art == A_GUNBOAT ? 1.5 : art == A_BATTLESHIP ? 2.0 : 1.9;
            if (art == A_FREIGHTER) {
                for (int i = 0; i < 4; i++)
                    p.poly({-0.7 + i * 0.3, -0.02, -0.44 + i * 0.3, -0.02, -0.44 + i * 0.3, 0.2, -0.7 + i * 0.3, 0.2},
                           i % 2 ? cc : rgb(70, 110, 150), true, 0.6f);
                p.poly({0.52, -0.3, 0.82, -0.3, 0.82, 0.2, 0.52, 0.2}, sup);
            } else {
                p.poly({-0.4, -0.12, 0.42, -0.12, 0.46, 0.2, -0.44, 0.2}, sup);
                p.poly({-0.14, -0.4, 0.2, -0.4, 0.24, -0.12, -0.16, -0.12}, sup);
                if (art == A_IRONCLAD || art == A_GUNBOAT) p.poly({-0.08, -0.74, 0.08, -0.74, 0.1, -0.4, -0.08, -0.4}, BLACKMETAL);  // funnel
                else p.stick(0.02, -0.4, 0.02, -0.86, 0.04, BLACKMETAL);  // mast
                int guns = art == A_BATTLESHIP ? 2 : 1;
                for (int i = 0; i < guns; i++) {
                    double gx = 0.58 + i * 0.0, gx2 = -0.62;
                    p.ell(gx, 0.08, 0.14, 0.08, sup);
                    p.stick(gx, 0.04, gx + 0.34, 0.0, 0.05, BLACKMETAL);
                    if (art == A_BATTLESHIP) { p.ell(gx2, 0.08, 0.14, 0.08, sup); p.stick(gx2, 0.04, gx2 - 0.34, 0.0, 0.05, BLACKMETAL); }
                }
            }
            hull(p, len, 0.34, hc, true);
            p.stick(-len / 2 + 0.06, 0.34, len / 2 - 0.08, 0.34, 0.05, cc, false);  // a stripe in the owner's colour
            if (art != A_FREIGHTER) pennant(p, art == A_IRONCLAD || art == A_GUNBOAT ? 0.0 : 0.02, art == A_IRONCLAD || art == A_GUNBOAT ? -0.74 : -0.86, 0.26, cc);
            if (animOn && (art == A_IRONCLAD || art == A_GUNBOAT || art == A_FREIGHTER)) {
                double t = animMs / 1000.0;
                for (int k = 0; k < 2; k++) {
                    double f = std::fmod(t * 0.5 + k * 0.5, 1.0);
                    double fx = art == A_FREIGHTER ? 0.66 : 0.0, fy = art == A_FREIGHTER ? -0.36 : -0.78;
                    p.cv->fillCircleA(p.X(fx - f * 0.5), p.Y(fy - f * 0.4), p.L(0.08 + f * 0.1), rgb(200, 200, 204), (int)(140 * (1 - f)));
                }
            }
            break;
        }
        default: {  // anything new: a soldier of its era with a spear
            soldier(era);
            bodyBack(p, sp, b);
            p.place(0, 0, sp.size);
            spear(p, b.fx, b.fy, IRON);
            bodyFront(p, sp, b);
            break;
        }
    }
}
