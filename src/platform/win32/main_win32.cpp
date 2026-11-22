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
#include <string>
#include <unordered_map>
#include <memory>
#include "../../ui/GameUI.h"

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
    int w = 0, h = 0;

    ~GdiCanvas() override {
        for (auto& f : fonts) delete f.second;
        delete fmt;
        for (auto& L : layers) freeLayer(L);
    }
    static void setup(Gdiplus::Graphics& gr) {
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
    void circle(int cx, int cy, int r, Color c, int t) override {
        Gdiplus::Pen p(gc(c, 255), (Gdiplus::REAL)std::max(1, t));
        g->DrawEllipse(&p, cx - r, cy - r, 2 * r, 2 * r);
    }
    void text(int x, int y, const std::string& s, int size, Color c, int align, bool bold) override {
        if (s.empty()) return;
        std::wstring ws = widen(s);
        Gdiplus::Font* f = font(size, bold);
        if (align != AL_LEFT) {
            int tw = measure(ws, f);
            x -= align == AL_CENTER ? tw / 2 : tw;
        }
        Gdiplus::SolidBrush b(gc(c, 255));
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
        if (L.valid && L.key == key && L.w == w && L.h == h) {
            g->Flush(Gdiplus::FlushIntentionSync);
            BitBlt(dc, 0, 0, w, h, L.dc, 0, 0, SRCCOPY);
            return true;
        }
        if (!L.dc || L.w != w || L.h != h) {
            freeLayer(L);
            L.dc = CreateCompatibleDC(dc);
            L.bmp = CreateCompatibleBitmap(dc, std::max(1, w), std::max(1, h));
            L.old = SelectObject(L.dc, L.bmp);
            L.w = w;
            L.h = h;
        }
        L.key = key;
        L.valid = true;
        saved = g;
        layerG = new Gdiplus::Graphics(L.dc);
        setup(*layerG);
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
        BitBlt(dc, 0, 0, w, h, layers[id].dc, 0, 0, SRCCOPY);
    }

private:
    struct Layer {
        HDC dc = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ old = nullptr;
        int w = 0, h = 0;
        uint64_t key = 0;
        bool valid = false;
    };
    Layer layers[2];
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
                {
                    Gdiplus::Graphics gfx(g_memDC);
                    GdiCanvas::setup(gfx);
                    g_canvas->dc = g_memDC;
                    g_canvas->g = &gfx;
                    g_canvas->w = w;
                    g_canvas->h = h;
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
            g_ui->mouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            g_ui->mouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), MOUSE_LEFT);
            afterInput(hwnd);
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            g_ui->mouseUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), MOUSE_LEFT);
            afterInput(hwnd);
            return 0;
        case WM_RBUTTONDOWN:
            g_ui->mouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), MOUSE_RIGHT);
            return 0;
        case WM_RBUTTONUP:
            g_ui->mouseUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), MOUSE_RIGHT);
            afterInput(hwnd);
            return 0;
        case WM_MOUSEWHEEL: {
            POINT p;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            ScreenToClient(hwnd, &p);
            int d = GET_WHEEL_DELTA_WPARAM(wp);
            g_ui->wheel(p.x, p.y, d > 0 ? 1 : -1);
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
            mm->ptMinTrackSize.x = 1024;
            mm->ptMinTrackSize.y = 640;
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

std::string savePathFor() {
    char buf[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableA("APPDATA", buf, (DWORD)sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
        std::string dir = std::string(buf) + "\\RebirthOfCivilisation";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\savegame.roc";
    }
    return "savegame.roc";
}

}  // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    SetProcessDPIAware();
    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken = 0;
    if (Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr) != Gdiplus::Ok) return 1;
    g_ui.reset(new GameUI());
    g_ui->savePath = savePathFor();
    g_canvas.reset(new GdiCanvas());

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
    int w = std::min(1600, sw * 9 / 10), h = std::min(900, sh * 85 / 100);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Rebirth of Civilisation", WS_OVERLAPPEDWINDOW, (sw - w) / 2, (sh - h) / 2, w, h,
                                nullptr, nullptr, inst, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, sw <= 1600 ? SW_MAXIMIZE : show);
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
