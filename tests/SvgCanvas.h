// SVG implementation of Canvas, used by the screenshot tool to check the UI without Windows.
// SPDX-License-Identifier: MIT
#pragma once
#include "../src/ui/Canvas.h"
#include <cstdio>
#include <fstream>
#include <sstream>

class SvgCanvas : public Canvas {
public:
    SvgCanvas(int w, int h) : w_(w), h_(h) {}
    int width() const override { return w_; }
    int height() const override { return h_; }
    void fillRect(int x, int y, int w, int h, Color c) override {
        if (w <= 0 || h <= 0) return;
        o << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' fill='" << col(c) << "'/>\n";
    }
    void rect(int x, int y, int w, int h, Color c, int t) override {
        o << "<rect x='" << x + 0.5 << "' y='" << y + 0.5 << "' width='" << w - 1 << "' height='" << h - 1 << "' fill='none' stroke='" << col(c)
          << "' stroke-width='" << t << "'/>\n";
    }
    void line(int x1, int y1, int x2, int y2, Color c, int t) override {
        o << "<line x1='" << x1 << "' y1='" << y1 << "' x2='" << x2 << "' y2='" << y2 << "' stroke='" << col(c) << "' stroke-width='" << t
          << "' stroke-linecap='round'/>\n";
    }
    void fillPoly(const int* xy, int n, Color c) override {
        o << "<polygon points='";
        for (int i = 0; i < n; i++) o << xy[i * 2] << "," << xy[i * 2 + 1] << " ";
        o << "' fill='" << col(c) << "'/>\n";
    }
    void fillCircle(int cx, int cy, int r, Color c) override {
        o << "<circle cx='" << cx << "' cy='" << cy << "' r='" << r << "' fill='" << col(c) << "'/>\n";
    }
    void circle(int cx, int cy, int r, Color c, int t) override {
        o << "<circle cx='" << cx << "' cy='" << cy << "' r='" << r << "' fill='none' stroke='" << col(c) << "' stroke-width='" << t << "'/>\n";
    }
    void text(int x, int y, const std::string& s, int size, Color c, int align, bool bold) override {
        if (s.empty()) return;
        const char* anchor = align == AL_CENTER ? "middle" : align == AL_RIGHT ? "end" : "start";
        o << "<text x='" << x << "' y='" << y + (int)(size * 0.86) << "' font-family='DejaVu Sans, Arial' font-size='" << size * 0.92
          << "' fill='" << col(c) << "' text-anchor='" << anchor << "'" << (bold ? " font-weight='bold'" : "") << ">" << esc(s) << "</text>\n";
    }
    void textA(int x, int y, const std::string& s, int size, Color c, int align, bool bold, int a) override {
        if (s.empty()) return;
        const char* anchor = align == AL_CENTER ? "middle" : align == AL_RIGHT ? "end" : "start";
        o << "<text x='" << x << "' y='" << y + (int)(size * 0.86) << "' font-family='DejaVu Sans, Arial' font-size='" << size * 0.92
          << "' fill='" << col(c) << "' fill-opacity='" << a / 255.0 << "' text-anchor='" << anchor << "'" << (bold ? " font-weight='bold'" : "") << ">" << esc(s)
          << "</text>\n";
    }
    int textWidth(const std::string& s, int size, bool bold) override { return (int)(s.size() * size * (bold ? 0.58 : 0.53)); }
    void clip(int x, int y, int w, int h) override {
        unclip();
        int id = ++clipId;
        o << "<clipPath id='c" << id << "'><rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "'/></clipPath><g clip-path='url(#c" << id << ")'>\n";
        clipped = true;
    }
    void unclip() override {
        if (clipped) o << "</g>\n";
        clipped = false;
    }
    void fillRectA(int x, int y, int w, int h, Color c, int a) override {
        if (w <= 0 || h <= 0) return;
        o << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' fill='" << col(c) << "' fill-opacity='" << a / 255.0 << "'/>\n";
    }
    void fillPolyA(const int* xy, int n, Color c, int a) override {
        o << "<polygon points='";
        for (int i = 0; i < n; i++) o << xy[i * 2] << "," << xy[i * 2 + 1] << " ";
        o << "' fill='" << col(c) << "' fill-opacity='" << a / 255.0 << "'/>\n";
    }
    void fillCircleA(int cx, int cy, int r, Color c, int a) override {
        o << "<circle cx='" << cx << "' cy='" << cy << "' r='" << r << "' fill='" << col(c) << "' fill-opacity='" << a / 255.0 << "'/>\n";
    }
    void lineA(int x1, int y1, int x2, int y2, Color c, int t, int a) override {
        o << "<line x1='" << x1 << "' y1='" << y1 << "' x2='" << x2 << "' y2='" << y2 << "' stroke='" << col(c) << "' stroke-width='" << t
          << "' stroke-opacity='" << a / 255.0 << "' stroke-linecap='round'/>\n";
    }
    void fillPolyGrad(const int* xy, int n, Color c1, int a1, Color c2, int a2, int x1, int y1, int x2, int y2) override {
        if (n < 3) return;
        int id = ++gradId;
        o << "<linearGradient id='g" << id << "' gradientUnits='userSpaceOnUse' x1='" << x1 << "' y1='" << y1 << "' x2='" << x2 << "' y2='" << y2
          << "'><stop offset='0' stop-color='" << col(c1) << "' stop-opacity='" << a1 / 255.0 << "'/><stop offset='1' stop-color='" << col(c2)
          << "' stop-opacity='" << a2 / 255.0 << "'/></linearGradient>";
        o << "<polygon points='";
        for (int i = 0; i < n; i++) o << xy[i * 2] << "," << xy[i * 2 + 1] << " ";
        o << "' fill='url(#g" << id << ")'/>\n";
    }
    void fillCircleGrad(int cx, int cy, int r, Color inner, int ai, Color outer, int ao, int fx, int fy) override {
        if (r <= 0) return;
        int id = ++gradId;
        o << "<radialGradient id='g" << id << "' gradientUnits='userSpaceOnUse' cx='" << cx << "' cy='" << cy << "' r='" << r << "' fx='" << fx
          << "' fy='" << fy << "'><stop offset='0' stop-color='" << col(inner) << "' stop-opacity='" << ai / 255.0 << "'/><stop offset='1' stop-color='"
          << col(outer) << "' stop-opacity='" << ao / 255.0 << "'/></radialGradient>";
        o << "<circle cx='" << cx << "' cy='" << cy << "' r='" << r << "' fill='url(#g" << id << ")'/>\n";
    }
    void fillEllipseA(int cx, int cy, int rx, int ry, Color c, int a) override {
        o << "<ellipse cx='" << cx << "' cy='" << cy << "' rx='" << rx << "' ry='" << ry << "' fill='" << col(c) << "' fill-opacity='" << a / 255.0 << "'/>\n";
    }
    void glowEllipse(int cx, int cy, int rx, int ry, Color c, int a) override {
        if (rx <= 0 || ry <= 0) return;
        int id = ++gradId;
        o << "<radialGradient id='g" << id << "'><stop offset='0' stop-color='" << col(c) << "' stop-opacity='" << a / 255.0
          << "'/><stop offset='1' stop-color='" << col(c) << "' stop-opacity='0'/></radialGradient>";
        o << "<ellipse cx='" << cx << "' cy='" << cy << "' rx='" << rx << "' ry='" << ry << "' fill='url(#g" << id << ")'/>\n";
    }
    void polylineA(const int* xy, int n, Color c, int t, int a, bool closed) override {
        o << (closed ? "<polygon" : "<polyline") << " points='";
        for (int i = 0; i < n; i++) o << xy[i * 2] << "," << xy[i * 2 + 1] << " ";
        o << "' fill='none' stroke='" << col(c) << "' stroke-width='" << t << "' stroke-opacity='" << a / 255.0
          << "' stroke-linejoin='round' stroke-linecap='round'/>\n";
    }
    void fillRoundRectGrad(int x, int y, int w, int h, int r, Color top, Color bottom, int a) override {
        if (w <= 0 || h <= 0) return;
        int id = ++gradId;
        o << "<linearGradient id='g" << id << "' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='" << col(top) << "'/><stop offset='1' stop-color='"
          << col(bottom) << "'/></linearGradient>";
        o << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' rx='" << r << "' fill='url(#g" << id << ")' fill-opacity='"
          << a / 255.0 << "'/>\n";
    }
    void roundRect(int x, int y, int w, int h, int r, Color c, int t, int a) override {
        o << "<rect x='" << x + t / 2.0 << "' y='" << y + t / 2.0 << "' width='" << w - t << "' height='" << h - t << "' rx='" << r
          << "' fill='none' stroke='" << col(c) << "' stroke-width='" << t << "' stroke-opacity='" << a / 255.0 << "'/>\n";
    }
    // chunk mode (enabled by setting chunk > 0) exercises the map's chunked terrain path in screenshots
    int chunk = 0;
    int chunkSize() const override { return chunk; }
    void chunkBegin(uint64_t, int ox, int oy, int px, int py) override {
        o << "<svg x='" << px << "' y='" << py << "' width='" << chunk << "' height='" << chunk << "' overflow='hidden'><g transform='translate(" << -ox << ","
          << -oy << ")'>\n";
    }
    void chunkEnd(uint64_t, uint64_t, int, int) override { o << "</g></svg>\n"; }
    bool save(const std::string& path) {
        unclip();
        std::ofstream f(path);
        f << "<svg xmlns='http://www.w3.org/2000/svg' width='" << w_ << "' height='" << h_ << "'>\n" << o.str() << "</svg>\n";
        return (bool)f;
    }

private:
    int w_, h_;
    int clipId = 0, gradId = 0;
    bool clipped = false;
    std::ostringstream o;
    static std::string col(Color c) {
        char b[8];
        std::snprintf(b, sizeof b, "#%02x%02x%02x", c.r, c.g, c.b);
        return b;
    }
    static std::string esc(const std::string& s) {
        std::string r;
        for (char ch : s) {
            if (ch == '&') r += "&amp;";
            else if (ch == '<') r += "&lt;";
            else if (ch == '>') r += "&gt;";
            else r += ch;
        }
        return r;
    }
};
