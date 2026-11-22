// Rebirth of Civilisation - native Windows front end (Win32 + GDI, no dependencies)
// SPDX-License-Identifier: MIT
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
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

class GdiCanvas : public Canvas {
public:
    HDC dc = nullptr;
    int w = 0, h = 0;

    ~GdiCanvas() override {
        for (auto& f : fonts) DeleteObject(f.second);
        for (auto& p : pens) DeleteObject(p.second);
    }
    int width() const override { return w; }
    int height() const override { return h; }

    void fillRect(int x, int y, int ww, int hh, Color c) override {
        if (ww <= 0 || hh <= 0) return;
        RECT r = {x, y, x + ww, y + hh};
        SetDCBrushColor(dc, cr(c));
        FillRect(dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
    }
    void rect(int x, int y, int ww, int hh, Color c, int t) override {
        fillRect(x, y, ww, t, c);
        fillRect(x, y + hh - t, ww, t, c);
        fillRect(x, y, t, hh, c);
        fillRect(x + ww - t, y, t, hh, c);
    }
    void line(int x1, int y1, int x2, int y2, Color c, int t) override {
        HGDIOBJ old = SelectObject(dc, pen(c, t));
        MoveToEx(dc, x1, y1, nullptr);
        LineTo(dc, x2, y2);
        SelectObject(dc, old);
    }
    void fillPoly(const int* xy, int n, Color c) override {
        if (n < 3) return;
        POINT pts[64];
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++) { pts[i].x = xy[i * 2]; pts[i].y = xy[i * 2 + 1]; }
        HGDIOBJ ob = SelectObject(dc, GetStockObject(DC_BRUSH));
        HGDIOBJ op = SelectObject(dc, GetStockObject(DC_PEN));
        SetDCBrushColor(dc, cr(c));
        SetDCPenColor(dc, cr(c));
        Polygon(dc, pts, n);
        SelectObject(dc, ob);
        SelectObject(dc, op);
    }
    void fillCircle(int cx, int cy, int r, Color c) override {
        HGDIOBJ ob = SelectObject(dc, GetStockObject(DC_BRUSH));
        HGDIOBJ op = SelectObject(dc, GetStockObject(DC_PEN));
        SetDCBrushColor(dc, cr(c));
        SetDCPenColor(dc, cr(c));
        Ellipse(dc, cx - r, cy - r, cx + r + 1, cy + r + 1);
        SelectObject(dc, ob);
        SelectObject(dc, op);
    }
    void circle(int cx, int cy, int r, Color c, int t) override {
        HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        HGDIOBJ op = SelectObject(dc, pen(c, t));
        Ellipse(dc, cx - r, cy - r, cx + r + 1, cy + r + 1);
        SelectObject(dc, ob);
        SelectObject(dc, op);
    }
    void text(int x, int y, const std::string& s, int size, Color c, int align, bool bold) override {
        if (s.empty()) return;
        std::wstring ws = widen(s);
        HGDIOBJ of = SelectObject(dc, font(size, bold));
        SetTextColor(dc, cr(c));
        SetBkMode(dc, TRANSPARENT);
        if (align != AL_LEFT) {
            SIZE sz;
            GetTextExtentPoint32W(dc, ws.c_str(), (int)ws.size(), &sz);
            x -= align == AL_CENTER ? sz.cx / 2 : sz.cx;
        }
        TextOutW(dc, x, y, ws.c_str(), (int)ws.size());
        SelectObject(dc, of);
    }
    int textWidth(const std::string& s, int size, bool bold) override {
        if (s.empty()) return 0;
        std::wstring ws = widen(s);
        HGDIOBJ of = SelectObject(dc, font(size, bold));
        SIZE sz;
        GetTextExtentPoint32W(dc, ws.c_str(), (int)ws.size(), &sz);
        SelectObject(dc, of);
        return sz.cx;
    }
    void clip(int x, int y, int ww, int hh) override {
        HRGN rgn = CreateRectRgn(x, y, x + ww, y + hh);
        SelectClipRgn(dc, rgn);
        DeleteObject(rgn);
    }
    void unclip() override { SelectClipRgn(dc, nullptr); }

private:
    std::unordered_map<int, HFONT> fonts;
    std::unordered_map<uint64_t, HPEN> pens;
    static COLORREF cr(Color c) { return RGB(c.r, c.g, c.b); }
    HFONT font(int size, bool bold) {
        int key = size * 2 + (bold ? 1 : 0);
        auto it = fonts.find(key);
        if (it != fonts.end()) return it->second;
        HFONT f = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        fonts[key] = f;
        return f;
    }
    HPEN pen(Color c, int t) {
        uint64_t key = ((uint64_t)RGB(c.r, c.g, c.b) << 8) | (uint64_t)(t & 0xff);
        auto it = pens.find(key);
        if (it != pens.end()) return it->second;
        LOGBRUSH lb;
        lb.lbStyle = BS_SOLID;
        lb.lbColor = cr(c);
        lb.lbHatch = 0;
        HPEN p = t <= 1 ? CreatePen(PS_SOLID, 1, cr(c))
                        : ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, (DWORD)t, &lb, 0, nullptr);
        if (pens.size() > 2000) {
            for (auto& q : pens) DeleteObject(q.second);
            pens.clear();
        }
        pens[key] = p;
        return p;
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
                g_canvas->dc = g_memDC;
                g_canvas->w = w;
                g_canvas->h = h;
                g_ui->render(*g_canvas);
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
    g_canvas.reset();
    g_ui.reset();
    return 0;
}
