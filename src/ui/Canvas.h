// Rebirth of Civilisation - drawing surface abstraction.
// The Windows build implements this with GDI; the test build renders to SVG.
// SPDX-License-Identifier: MIT
#pragma once
#include "../core/Types.h"
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
};

// Platform-independent key codes (letters and digits use their ASCII upper-case value)
enum KeyCode {
    KEY_BACK = 8, KEY_TAB = 9, KEY_ENTER = 13, KEY_ESC = 27, KEY_SPACE = 32,
    KEY_LEFT = 1000, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_DELETE, KEY_HOME, KEY_PGUP, KEY_PGDN,
    KEY_PLUS, KEY_MINUS,
    KEY_F1 = 1100, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
};

enum MouseButton { MOUSE_LEFT = 0, MOUSE_RIGHT = 1, MOUSE_MIDDLE = 2 };
