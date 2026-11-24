// Rebirth of Civilisation - drawing surface abstraction.
// The Windows build implements this with GDI; the test build renders to SVG.
// SPDX-License-Identifier: MIT
#pragma once
#include "../core/Types.h"
#include <cmath>
#include <string>

enum TextAlign { AL_LEFT = 0, AL_CENTER = 1, AL_RIGHT = 2 };

class Canvas {
public:
    virtual ~Canvas() {}
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void fillRect(int x, int y, int w, int h, Color c) = 0;
    virtual void rect(int x, int y, int w, int h, Color c, int thick = 1) = 0;
    virtual void line(int x1, int y1, int x2, int y2, Color c, int thick = 1) = 0;
    // xy holds n points as x0,y0,x1,y1,...
    virtual void fillPoly(const int* xy, int n, Color c) = 0;
    virtual void fillCircle(int cx, int cy, int r, Color c) = 0;
    virtual void circle(int cx, int cy, int r, Color c, int thick = 1) = 0;
    // y is the top of the text; size is the pixel height of the font
    virtual void text(int x, int y, const std::string& s, int size, Color c, int align = AL_LEFT, bool bold = false) = 0;
    virtual int textWidth(const std::string& s, int size, bool bold = false) = 0;
    virtual void clip(int x, int y, int w, int h) = 0;
    virtual void unclip() = 0;
    // translucent drawing (alpha 0..255). Backends without alpha fall back to opaque drawing.
    virtual void fillRectA(int x, int y, int w, int h, Color c, int alpha) { (void)alpha; fillRect(x, y, w, h, c); }
    virtual void fillPolyA(const int* xy, int n, Color c, int alpha) { (void)alpha; fillPoly(xy, n, c); }
    virtual void fillCircleA(int cx, int cy, int r, Color c, int alpha) { (void)alpha; fillCircle(cx, cy, r, c); }
    virtual void lineA(int x1, int y1, int x2, int y2, Color c, int thick, int alpha) { (void)alpha; line(x1, y1, x2, y2, c, thick); }
    // ---- richer primitives. Backends that cannot do them fall back to flat translucent shapes.
    // Linear gradient across a polygon: colour c1 at (x1,y1) to c2 at (x2,y2), each with its own alpha.
    virtual void fillPolyGrad(const int* xy, int n, Color c1, int a1, Color c2, int a2, int x1, int y1, int x2, int y2) {
        (void)x1; (void)y1; (void)x2; (void)y2;
        fillPolyA(xy, n, mixc(c1, c2), (a1 + a2) / 2);
    }
    // Vertical gradient rectangle (top colour to bottom colour).
    virtual void fillRectGrad(int x, int y, int w, int h, Color top, Color bottom, int alpha = 255) {
        int xy[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
        fillPolyGrad(xy, 4, top, alpha, bottom, alpha, x, y, x, y + h);
    }
    // Radial gradient disc: 'inner' at the focus (fx, fy), fading to 'outer' at the rim.
    virtual void fillCircleGrad(int cx, int cy, int r, Color inner, int ai, Color outer, int ao, int fx, int fy) {
        (void)fx; (void)fy;
        fillCircleA(cx, cy, r, mixc(inner, outer), (ai + ao) / 2);
    }
    void fillCircleGrad(int cx, int cy, int r, Color inner, int ai, Color outer, int ao) { fillCircleGrad(cx, cy, r, inner, ai, outer, ao, cx, cy); }
    virtual void fillEllipseA(int cx, int cy, int rx, int ry, Color c, int alpha) {
        int xy[48];
        for (int i = 0; i < 24; i++) {
            double a = i * 6.2831853 / 24;
            xy[i * 2] = cx + (int)(rx * cosApprox(a));
            xy[i * 2 + 1] = cy + (int)(ry * cosApprox(a - 1.5707963));
        }
        fillPolyA(xy, 24, c, alpha);
    }
    // Soft-edged ellipse (radial fade to transparent), for shadows and glows.
    virtual void glowEllipse(int cx, int cy, int rx, int ry, Color c, int alpha) { fillEllipseA(cx, cy, rx, ry, c, alpha / 2); }
    // Joined line strip (round joins), optionally closed.
    virtual void polylineA(const int* xy, int n, Color c, int thick, int alpha, bool closed = false) {
        for (int i = 0; i + 1 < n; i++) lineA(xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3], c, thick, alpha);
        if (closed && n > 2) lineA(xy[n * 2 - 2], xy[n * 2 - 1], xy[0], xy[1], c, thick, alpha);
    }
    // Translucent text (fades). Backends without alpha draw it opaque.
    virtual void textA(int x, int y, const std::string& s, int size, Color c, int align, bool bold, int alpha) {
        (void)alpha;
        text(x, y, s, size, c, align, bold);
    }
    // Rounded rectangles for panels and buttons.
    virtual void fillRoundRectGrad(int x, int y, int w, int h, int r, Color top, Color bottom, int alpha = 255) {
        (void)r;
        fillRectGrad(x, y, w, h, top, bottom, alpha);
    }
    virtual void roundRect(int x, int y, int w, int h, int r, Color c, int thick = 1, int alpha = 255) {
        (void)r; (void)alpha;
        rect(x, y, w, h, c, thick);
    }
    // ---- optional world chunk cache, used for the map terrain. The map is cut into square chunks of
    // chunkSize() physical pixels. chunkDraw blits a cached chunk (same id and key) to physical position
    // (px, py) and returns true. Otherwise the caller brackets drawing with chunkBegin/chunkEnd: in between,
    // logical coordinates are world coordinates and the physical point (ox, oy) is the chunk's top-left.
    virtual float pixelScale() const { return 1.0f; }
    virtual int chunkSize() const { return 0; }
    virtual bool chunkDraw(uint64_t id, uint64_t key, int px, int py) { (void)id; (void)key; (void)px; (void)py; return false; }
    virtual void chunkBegin(uint64_t id, int ox, int oy, int px, int py) { (void)id; (void)ox; (void)oy; (void)px; (void)py; }
    virtual void chunkEnd(uint64_t id, uint64_t key, int px, int py) { (void)id; (void)key; (void)px; (void)py; }
    // Optional cached layers. layerBegin returns true if an identical layer (same key and size) was cached
    // and has just been drawn - the caller then skips drawing it. Otherwise drawing is redirected into the
    // layer until layerEnd, which then composites it. Backends without caching just draw normally.
    virtual bool layerBegin(int id, uint64_t key) { (void)id; (void)key; return false; }
    virtual void layerEnd(int id) { (void)id; }

protected:
    static Color mixc(Color a, Color b) { return Color{(uint8_t)((a.r + b.r) / 2), (uint8_t)((a.g + b.g) / 2), (uint8_t)((a.b + b.b) / 2)}; }
    static double cosApprox(double a) { return std::cos(a); }
};

// Platform-independent key codes (letters and digits use their ASCII upper-case value)
enum KeyCode {
    KEY_BACK = 8, KEY_TAB = 9, KEY_ENTER = 13, KEY_ESC = 27, KEY_SPACE = 32,
    KEY_LEFT = 1000, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_DELETE, KEY_HOME, KEY_PGUP, KEY_PGDN,
    KEY_PLUS, KEY_MINUS,
    KEY_F1 = 1100, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
};

enum MouseButton { MOUSE_LEFT = 0, MOUSE_RIGHT = 1, MOUSE_MIDDLE = 2 };
