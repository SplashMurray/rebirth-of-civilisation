// Rebirth of Civilisation - native Windows front end (Win32 + GDI+, no dependencies beyond Windows itself)
// SPDX-License-Identifier: MIT
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <algorithm>
// gdiplus.h expects min/max; NOMINMAX hides the macros, so provide them inside its namespace
namespace Gdiplus {
using std::max;
using std::min;
}
#include <gdiplus.h>
#include <cstdio>
#include <cmath>
#include <string>
#include <unordered_map>
#include <memory>
#include "../../ui/GameUI.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

// Anti-aliased renderer using GDI+ (part of Windows since XP, no extra dependencies).
class GdiCanvas : public Canvas {
public:
    HDC dc = nullptr;                 // back buffer DC
    Gdiplus::Graphics* g = nullptr;   // current target (back buffer or a cached layer)
    int w = 0, h = 0;                 // logical size seen by the UI
    int pw = 0, ph = 0;               // physical pixel size of the back buffer
    float scale = 1.0f;               // physical pixels per logical pixel

    ~GdiCanvas() override {
        for (auto& f : fonts) delete f.second;
        delete fmt;
        for (auto& L : layers) freeLayer(L);
        for (auto& c : chunks) freeLayer(c.second.L);
    }
    static void setup(Gdiplus::Graphics& gr, float scale = 1.0f) {
        if (scale != 1.0f) gr.ScaleTransform(scale, scale);
        gr.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        gr.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        gr.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        gr.SetCompositingQuality(Gdiplus::CompositingQualityHighSpeed);
    }
    int width() const override { return w; }
    int height() const override { return h; }

    void fillRect(int x, int y, int ww, int hh, Color c) override { fillRectA(x, y, ww, hh, c, 255); }
    void fillRectA(int x, int y, int ww, int hh, Color c, int a) override {
        if (ww <= 0 || hh <= 0) return;
        Gdiplus::SolidBrush b(gc(c, a));
        g->FillRectangle(&b, x, y, ww, hh);
    }
    void rect(int x, int y, int ww, int hh, Color c, int t) override {
        fillRect(x, y, ww, t, c);
        fillRect(x, y + hh - t, ww, t, c);
        fillRect(x, y, t, hh, c);
        fillRect(x + ww - t, y, t, hh, c);
    }
    void line(int x1, int y1, int x2, int y2, Color c, int t) override { lineA(x1, y1, x2, y2, c, t, 255); }
    void lineA(int x1, int y1, int x2, int y2, Color c, int t, int a) override {
        Gdiplus::Pen p(gc(c, a), (Gdiplus::REAL)std::max(1, t));
        p.SetStartCap(Gdiplus::LineCapRound);
        p.SetEndCap(Gdiplus::LineCapRound);
        g->DrawLine(&p, x1, y1, x2, y2);
    }
    void fillPoly(const int* xy, int n, Color c) override { fillPolyA(xy, n, c, 255); }
    void fillPolyA(const int* xy, int n, Color c, int a) override {
        if (n < 3) return;
        Gdiplus::Point pts[64];
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++) pts[i] = Gdiplus::Point(xy[i * 2], xy[i * 2 + 1]);
        Gdiplus::SolidBrush b(gc(c, a));
        g->FillPolygon(&b, pts, n);
    }
    void fillCircle(int cx, int cy, int r, Color c) override { fillCircleA(cx, cy, r, c, 255); }
    void fillCircleA(int cx, int cy, int r, Color c, int a) override {
        Gdiplus::SolidBrush b(gc(c, a));
        g->FillEllipse(&b, cx - r, cy - r, 2 * r, 2 * r);
    }
    // ---- gradients and rounded shapes
    void fillPolyGrad(const int* xy, int n, Color c1, int a1, Color c2, int a2, int x1, int y1, int x2, int y2) override {
        if (n < 3) return;
        if (x1 == x2 && y1 == y2) { fillPolyA(xy, n, c1, a1); return; }
        Gdiplus::Point pts[64];
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++) pts[i] = Gdiplus::Point(xy[i * 2], xy[i * 2 + 1]);
        // stretch the gradient a pixel past both ends: GDI+ wraps (tiles) colours just beyond them
        float dx = (float)(x2 - x1), dy = (float)(y2 - y1), len = std::sqrt(dx * dx + dy * dy);
        float ex = dx / len, ey = dy / len;
        Gdiplus::PointF p1(x1 - ex, y1 - ey), p2(x2 + ex, y2 + ey);
        Gdiplus::LinearGradientBrush b(p1, p2, gc(c1, a1), gc(c2, a2));
        b.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        g->FillPolygon(&b, pts, n);
    }
    void fillCircleGrad(int cx, int cy, int r, Color inner, int ai, Color outer, int ao, int fx, int fy) override {
        if (r <= 0) return;
        Gdiplus::GraphicsPath path;
        path.AddEllipse(cx - r, cy - r, 2 * r, 2 * r);
        Gdiplus::PathGradientBrush b(&path);
        b.SetCenterPoint(Gdiplus::PointF((Gdiplus::REAL)fx, (Gdiplus::REAL)fy));
        b.SetCenterColor(gc(inner, ai));
        Gdiplus::Color sc = gc(outer, ao);
        INT cnt = 1;
        b.SetSurroundColors(&sc, &cnt);
        g->FillPath(&b, &path);
    }
    void fillEllipseA(int cx, int cy, int rx, int ry, Color c, int a) override {
        Gdiplus::SolidBrush b(gc(c, a));
        g->FillEllipse(&b, cx - rx, cy - ry, 2 * rx, 2 * ry);
    }
    void glowEllipse(int cx, int cy, int rx, int ry, Color c, int a) override {
        if (rx <= 0 || ry <= 0) return;
        Gdiplus::GraphicsPath path;
        path.AddEllipse(cx - rx, cy - ry, 2 * rx, 2 * ry);
        Gdiplus::PathGradientBrush b(&path);
        b.SetCenterColor(gc(c, a));
        Gdiplus::Color sc = gc(c, 0);
        INT cnt = 1;
        b.SetSurroundColors(&sc, &cnt);
        g->FillPath(&b, &path);
    }
    void polylineA(const int* xy, int n, Color c, int t, int a, bool closed) override {
        if (n < 2) return;
        Gdiplus::Point pts[128];
        if (n > 128) n = 128;
        for (int i = 0; i < n; i++) pts[i] = Gdiplus::Point(xy[i * 2], xy[i * 2 + 1]);
        Gdiplus::Pen p(gc(c, a), (Gdiplus::REAL)std::max(1, t));
        p.SetLineJoin(Gdiplus::LineJoinRound);
        p.SetStartCap(Gdiplus::LineCapRound);
        p.SetEndCap(Gdiplus::LineCapRound);
        if (closed) g->DrawPolygon(&p, pts, n);
        else g->DrawLines(&p, pts, n);
    }
    static void roundPath(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float r) {
        r = std::min(r, std::min(w, h) / 2);
        if (r <= 0.5f) { path.AddRectangle(Gdiplus::RectF(x, y, w, h)); return; }
        float d = r * 2;
        path.AddArc(x, y, d, d, 180, 90);
        path.AddArc(x + w - d, y, d, d, 270, 90);
        path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
        path.AddArc(x, y + h - d, d, d, 90, 90);
        path.CloseFigure();
    }
    void fillRoundRectGrad(int x, int y, int ww, int hh, int r, Color top, Color bottom, int a) override {
        if (ww <= 0 || hh <= 0) return;
        Gdiplus::GraphicsPath path;
        roundPath(path, (float)x, (float)y, (float)ww, (float)hh, (float)r);
        Gdiplus::LinearGradientBrush b(Gdiplus::PointF((Gdiplus::REAL)x, (Gdiplus::REAL)y - 1), Gdiplus::PointF((Gdiplus::REAL)x, (Gdiplus::REAL)(y + hh + 1)),
                                       gc(top, a), gc(bottom, a));
        b.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        g->FillPath(&b, &path);
    }
    void roundRect(int x, int y, int ww, int hh, int r, Color c, int t, int a) override {
        if (ww <= 0 || hh <= 0) return;
        Gdiplus::GraphicsPath path;
        float h2 = t / 2.0f;
        roundPath(path, x + h2, y + h2, ww - (float)t, hh - (float)t, (float)r);
        Gdiplus::Pen p(gc(c, a), (Gdiplus::REAL)std::max(1, t));
        g->DrawPath(&p, &path);
    }
    void circle(int cx, int cy, int r, Color c, int t) override {
        Gdiplus::Pen p(gc(c, 255), (Gdiplus::REAL)std::max(1, t));
        g->DrawEllipse(&p, cx - r, cy - r, 2 * r, 2 * r);
    }
    void text(int x, int y, const std::string& s, int size, Color c, int align, bool bold) override { textA(x, y, s, size, c, align, bold, 255); }
    void textA(int x, int y, const std::string& s, int size, Color c, int align, bool bold, int a) override {
        if (s.empty() || a <= 0) return;
        std::wstring ws = widen(s);
        Gdiplus::Font* f = font(size, bold);
        if (align != AL_LEFT) {
            int tw = measure(ws, f);
            x -= align == AL_CENTER ? tw / 2 : tw;
        }
        Gdiplus::SolidBrush b(gc(c, a));
        g->DrawString(ws.c_str(), (INT)ws.size(), f, Gdiplus::PointF((Gdiplus::REAL)x, (Gdiplus::REAL)y), format(), &b);
    }
    int textWidth(const std::string& s, int size, bool bold) override {
        if (s.empty()) return 0;
        return measure(widen(s), font(size, bold));
    }
    void clip(int x, int y, int ww, int hh) override { g->SetClip(Gdiplus::Rect(x, y, ww, hh)); }
    void unclip() override { g->ResetClip(); }

    // ---- cached layers: plain GDI bitmaps copied with BitBlt (very fast)
    bool layerBegin(int id, uint64_t key) override {
        if (id < 0 || id >= 2 || layerG) return false;
        Layer& L = layers[id];
        if (L.valid && L.key == key && L.w == pw && L.h == ph && L.scale == scale) {
            g->Flush(Gdiplus::FlushIntentionSync);
            BitBlt(dc, 0, 0, pw, ph, L.dc, 0, 0, SRCCOPY);
            return true;
        }
        if (!L.dc || L.w != pw || L.h != ph) {
            freeLayer(L);
            L.dc = CreateCompatibleDC(dc);
            L.bmp = CreateCompatibleBitmap(dc, std::max(1, pw), std::max(1, ph));
            L.old = SelectObject(L.dc, L.bmp);
            L.w = pw;
            L.h = ph;
        }
        L.scale = scale;
        L.key = key;
        L.valid = true;
        saved = g;
        layerG = new Gdiplus::Graphics(L.dc);
        setup(*layerG, scale);
        g = layerG;
        return false;
    }
    void layerEnd(int id) override {
        if (!layerG || id < 0 || id >= 2) return;
        delete layerG;
        layerG = nullptr;
        g = saved;
        saved = nullptr;
        g->Flush(Gdiplus::FlushIntentionSync);
        BitBlt(dc, 0, 0, pw, ph, layers[id].dc, 0, 0, SRCCOPY);
    }

    // ---- world chunk cache: fixed-size physical-pixel tiles of the map, reused while panning
    float pixelScale() const override { return scale; }
    int chunkSize() const override { return layerG || chunkG ? 0 : CHUNK; }
    bool chunkDraw(uint64_t id, uint64_t key, int px, int py) override {
        auto it = chunks.find(id);
        if (it == chunks.end() || !it->second.valid || it->second.key != key) return false;
        it->second.used = ++chunkClock;
        g->Flush(Gdiplus::FlushIntentionSync);
        BitBlt(dc, px, py, CHUNK, CHUNK, it->second.dc, 0, 0, SRCCOPY);
        return true;
    }
    void chunkBegin(uint64_t id, int ox, int oy, int px, int py) override {
        (void)px; (void)py;
        if (chunkG) return;
        if (chunks.size() >= MAX_CHUNKS && !chunks.count(id)) {  // evict the least recently used
            auto victim = chunks.begin();
            for (auto it = chunks.begin(); it != chunks.end(); ++it)
                if (it->second.used < victim->second.used) victim = it;
            freeLayer(victim->second.L);
            chunks.erase(victim);
        }
        Chunk& C = chunks[id];
        if (!C.L.dc) {
            C.L.dc = CreateCompatibleDC(dc);
            C.L.bmp = CreateCompatibleBitmap(dc, CHUNK, CHUNK);
            C.L.old = SelectObject(C.L.dc, C.L.bmp);
            C.L.w = C.L.h = CHUNK;
        }
        C.valid = false;
        chunkSaved = g;
        chunkG = new Gdiplus::Graphics(C.L.dc);
        chunkG->TranslateTransform((Gdiplus::REAL)-ox, (Gdiplus::REAL)-oy);
        setup(*chunkG, scale);
        g = chunkG;
    }
    void chunkEnd(uint64_t id, uint64_t key, int px, int py) override {
        if (!chunkG) return;
        delete chunkG;
        chunkG = nullptr;
        g = chunkSaved;
        chunkSaved = nullptr;
        Chunk& C = chunks[id];
        C.valid = true;
        C.key = key;
        C.used = ++chunkClock;
        g->Flush(Gdiplus::FlushIntentionSync);
        BitBlt(dc, px, py, CHUNK, CHUNK, C.L.dc, 0, 0, SRCCOPY);
    }

private:
    struct Layer {
        HDC dc = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ old = nullptr;
        int w = 0, h = 0;
        float scale = 1.0f;
        uint64_t key = 0;
        bool valid = false;
    };
    Layer layers[2];
    struct Chunk {
        Layer L;
        uint64_t key = 0, used = 0;
        bool valid = false;
    };
    static const int CHUNK = 512;
    static const size_t MAX_CHUNKS = 72;
    std::unordered_map<uint64_t, Chunk> chunks;
    uint64_t chunkClock = 0;
    Gdiplus::Graphics* chunkG = nullptr;
    Gdiplus::Graphics* chunkSaved = nullptr;
    Gdiplus::Graphics* layerG = nullptr;
    Gdiplus::Graphics* saved = nullptr;
    std::unordered_map<int, Gdiplus::Font*> fonts;
    Gdiplus::StringFormat* fmt = nullptr;

    static void freeLayer(Layer& L) {
        if (L.dc) {
            SelectObject(L.dc, L.old);
            DeleteObject(L.bmp);
            DeleteDC(L.dc);
        }
        L = Layer();
    }
    static Gdiplus::Color gc(Color c, int a) { return Gdiplus::Color((BYTE)a, c.r, c.g, c.b); }
    Gdiplus::StringFormat* format() {
        if (!fmt) {
            fmt = Gdiplus::StringFormat::GenericTypographic()->Clone();
            fmt->SetFormatFlags(fmt->GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces | Gdiplus::StringFormatFlagsNoClip);
        }
        return fmt;
    }
    Gdiplus::Font* font(int size, bool bold) {
        int key = size * 2 + (bold ? 1 : 0);
        auto it = fonts.find(key);
        if (it != fonts.end()) return it->second;
        Gdiplus::Font* f = new Gdiplus::Font(L"Segoe UI", (Gdiplus::REAL)size, bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        fonts[key] = f;
        return f;
    }
    int measure(const std::wstring& ws, Gdiplus::Font* f) {
        Gdiplus::RectF box;
        g->MeasureString(ws.c_str(), (INT)ws.size(), f, Gdiplus::PointF(0, 0), format(), &box);
        return (int)(box.Width + 0.5f);
    }
};

std::unique_ptr<GameUI> g_ui;
std::unique_ptr<GdiCanvas> g_canvas;
HDC g_memDC = nullptr;
HBITMAP g_bmp = nullptr;
int g_bw = 0, g_bh = 0;
UINT g_dpi = 96;            // DPI of the monitor the window is on
int g_appliedPct = -1;      // uiScalePct last saved to the settings file
const int MIN_LOGICAL_W = 1000, MIN_LOGICAL_H = 620;

// Effective scale: Windows display scaling x the in-game interface size, limited so the
// logical canvas never drops below the size the UI was laid out for.
float scaleFor(int physW, int physH) {
    float s = (float)g_dpi / 96.0f * (float)g_ui->uiScalePct / 100.0f;
    if (physW > 0 && physH > 0) {
        s = std::min(s, (float)physW / MIN_LOGICAL_W);
        s = std::min(s, (float)physH / MIN_LOGICAL_H);
    }
    return std::max(0.5f, s);
}
float currentScale(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    return scaleFor(rc.right - rc.left, rc.bottom - rc.top);
}
int lx(HWND hwnd, int x) { return (int)((float)x / currentScale(hwnd)); }

UINT dpiForWindow(HWND hwnd) {
    typedef UINT(WINAPI * GetDpiForWindowFn)(HWND);
    static GetDpiForWindowFn fn = (GetDpiForWindowFn)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn && hwnd) return fn(hwnd);
    HDC sdc = GetDC(nullptr);
    UINT d = (UINT)GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(nullptr, sdc);
    return d ? d : 96;
}

void enableDpiAwareness() {
    // Per-monitor v2 (Windows 10 1703+) gives crisp rendering and DPI change notifications.
    typedef BOOL(WINAPI * SetCtxFn)(HANDLE);
    SetCtxFn fn = (SetCtxFn)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (fn && fn((HANDLE)-4 /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */)) return;
    SetProcessDPIAware();
}

std::string settingsPath();
int g_appliedAnim = -1;     // animOn last saved
void loadSettings() {
    FILE* f = fopen(settingsPath().c_str(), "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        int v = 0;
        if (sscanf(line, "uiscale=%d", &v) == 1 && v >= GameUI::UI_SCALE_MIN && v <= GameUI::UI_SCALE_MAX) g_ui->uiScalePct = v;
        if (sscanf(line, "animations=%d", &v) == 1) g_ui->animOn = v != 0;
    }
    fclose(f);
}
void saveSettingsIfChanged() {
    if (g_ui->uiScalePct == g_appliedPct && (int)g_ui->animOn == g_appliedAnim) return;
    g_appliedPct = g_ui->uiScalePct;
    g_appliedAnim = (int)g_ui->animOn;
    FILE* f = fopen(settingsPath().c_str(), "w");
    if (!f) return;
    fprintf(f, "uiscale=%d\nanimations=%d\n", g_appliedPct, g_appliedAnim);
    fclose(f);
}

void ensureBackBuffer(HDC wdc, int w, int h) {
    if (g_memDC && w == g_bw && h == g_bh) return;
    if (g_bmp) DeleteObject(g_bmp);
    if (!g_memDC) g_memDC = CreateCompatibleDC(wdc);
    g_bmp = CreateCompatibleBitmap(wdc, std::max(1, w), std::max(1, h));
    SelectObject(g_memDC, g_bmp);
    g_bw = w;
    g_bh = h;
}

int mapKey(WPARAM vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return (int)vk;
    switch (vk) {
        case VK_RETURN: return KEY_ENTER;
        case VK_ESCAPE: return KEY_ESC;
        case VK_SPACE: return KEY_SPACE;
        case VK_TAB: return KEY_TAB;
        case VK_BACK: return KEY_BACK;
        case VK_LEFT: return KEY_LEFT;
        case VK_RIGHT: return KEY_RIGHT;
        case VK_UP: return KEY_UP;
        case VK_DOWN: return KEY_DOWN;
        case VK_DELETE: return KEY_DELETE;
        case VK_HOME: return KEY_HOME;
        case VK_PRIOR: return KEY_PGUP;
        case VK_NEXT: return KEY_PGDN;
        case VK_ADD: case VK_OEM_PLUS: return KEY_PLUS;
        case VK_SUBTRACT: case VK_OEM_MINUS: return KEY_MINUS;
        default:
            if (vk >= VK_F1 && vk <= VK_F12) return KEY_F1 + (int)(vk - VK_F1);
            return 0;
    }
}

void afterInput(HWND hwnd) {
    saveSettingsIfChanged();
    if (g_ui->wantsQuit()) DestroyWindow(hwnd);
    else InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hwnd, 1, 33, nullptr);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left, h = rc.bottom - rc.top;
            if (w > 0 && h > 0) {
                ensureBackBuffer(dc, w, h);
                float s = scaleFor(w, h);
                {
                    Gdiplus::Graphics gfx(g_memDC);
                    GdiCanvas::setup(gfx, s);
                    g_canvas->dc = g_memDC;
                    g_canvas->g = &gfx;
                    g_canvas->scale = s;
                    g_canvas->pw = w;
                    g_canvas->ph = h;
                    g_canvas->w = (int)((float)w / s + 0.999f);
                    g_canvas->h = (int)((float)h / s + 0.999f);
                    g_ui->render(*g_canvas);
                    gfx.Flush(Gdiplus::FlushIntentionSync);
                    g_canvas->g = nullptr;
                }
                BitBlt(dc, 0, 0, w, h, g_memDC, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (g_ui->tick(33)) InvalidateRect(hwnd, nullptr, FALSE);
            if (g_ui->wantsQuit()) DestroyWindow(hwnd);
            return 0;
        case WM_MOUSEMOVE:
            g_ui->mouseMove(lx(hwnd, GET_X_LPARAM(lp)), lx(hwnd, GET_Y_LPARAM(lp)));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            g_ui->mouseDown(lx(hwnd, GET_X_LPARAM(lp)), lx(hwnd, GET_Y_LPARAM(lp)), MOUSE_LEFT);
            afterInput(hwnd);
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            g_ui->mouseUp(lx(hwnd, GET_X_LPARAM(lp)), lx(hwnd, GET_Y_LPARAM(lp)), MOUSE_LEFT);
            afterInput(hwnd);
            return 0;
        case WM_DPICHANGED: {
            g_dpi = HIWORD(wp);
            RECT* r = (RECT*)lp;
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_RBUTTONDOWN:
            g_ui->mouseDown(lx(hwnd, GET_X_LPARAM(lp)), lx(hwnd, GET_Y_LPARAM(lp)), MOUSE_RIGHT);
            return 0;
        case WM_RBUTTONUP:
            g_ui->mouseUp(lx(hwnd, GET_X_LPARAM(lp)), lx(hwnd, GET_Y_LPARAM(lp)), MOUSE_RIGHT);
            afterInput(hwnd);
            return 0;
        case WM_MOUSEWHEEL: {
            POINT p;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            ScreenToClient(hwnd, &p);
            int d = GET_WHEEL_DELTA_WPARAM(wp);
            g_ui->wheel(lx(hwnd, p.x), lx(hwnd, p.y), d > 0 ? 1 : -1);
            afterInput(hwnd);
            return 0;
        }
        case WM_KEYDOWN: {
            int k = mapKey(wp);
            if (k) {
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                g_ui->key(k, shift, ctrl);
                afterInput(hwnd);
            }
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mm = (MINMAXINFO*)lp;
            mm->ptMinTrackSize.x = MulDiv(1024, (int)g_dpi, 96);
            mm->ptMinTrackSize.y = MulDiv(640, (int)g_dpi, 96);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

std::string dataDir() {
    char buf[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableA("APPDATA", buf, (DWORD)sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
        std::string dir = std::string(buf) + "\\RebirthOfCivilisation";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\";
    }
    return "";
}
std::string savePathFor() { return dataDir() + "savegame.roc"; }
std::string settingsPath() { return dataDir() + "settings.ini"; }

}  // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    enableDpiAwareness();
    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken = 0;
    if (Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr) != Gdiplus::Ok) return 1;
    g_ui.reset(new GameUI());
    g_ui->savePath = savePathFor();
    g_ui->achievementsPath = dataDir() + "achievements.ini";
    g_ui->loadAchievements();
    g_canvas.reset(new GdiCanvas());
    loadSettings();
    g_appliedPct = g_ui->uiScalePct;

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"RebirthOfCivilisation";
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_dpi = dpiForWindow(nullptr);
    // 1600x900 logical, in physical pixels for this display, capped to the screen
    int w = std::min(MulDiv(1600, (int)g_dpi, 96), sw * 9 / 10), h = std::min(MulDiv(900, (int)g_dpi, 96), sh * 85 / 100);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Rebirth of Civilisation", WS_OVERLAPPEDWINDOW, (sw - w) / 2, (sh - h) / 2, w, h,
                                nullptr, nullptr, inst, nullptr);
    if (!hwnd) return 1;
    g_dpi = dpiForWindow(hwnd);
    ShowWindow(hwnd, MulDiv(sw, 96, (int)g_dpi) <= 1600 ? SW_MAXIMIZE : show);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (g_bmp) DeleteObject(g_bmp);
    if (g_memDC) DeleteDC(g_memDC);
    g_canvas.reset();  // fonts and layers must go before GDI+ shuts down
    g_ui.reset();
    Gdiplus::GdiplusShutdown(gdipToken);
    return 0;
}
