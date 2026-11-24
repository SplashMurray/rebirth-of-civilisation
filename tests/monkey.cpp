// Random-input stress test for the UI: clicks, drags, wheel and keys, rendering after every event.
// Best run under -fsanitize=address,undefined. Usage: rebirth_monkey [events] [seed]
// SPDX-License-Identifier: MIT
#include "SvgCanvas.h"
#include <map>
#include "../src/ui/GameUI.h"
#include <cstdio>
#include <cstdlib>

class NullCanvas : public Canvas {
public:
    int width() const override { return 1440; }
    int height() const override { return 860; }
    void fillRect(int, int, int, int, Color) override {}
    void rect(int, int, int, int, Color, int) override {}
    void line(int, int, int, int, Color, int) override {}
    void fillPoly(const int*, int, Color) override {}
    void fillCircle(int, int, int, Color) override {}
    void circle(int, int, int, Color, int) override {}
    void text(int, int, const std::string&, int, Color, int, bool) override {}
    int textWidth(const std::string& s, int size, bool) override { return (int)s.size() * size / 2; }
    void clip(int, int, int, int) override {}
    void unclip() override {}
    // a pretend chunk cache, so the chunked terrain path is exercised too
    float pixelScale() const override { return 1.5f; }
    int chunkSize() const override { return inChunk ? 0 : 256; }
    bool chunkDraw(uint64_t id, uint64_t key, int, int) override { auto it = keys.find(id); return it != keys.end() && it->second == key; }
    void chunkBegin(uint64_t, int, int, int, int) override { inChunk = true; }
    void chunkEnd(uint64_t id, uint64_t key, int, int) override { inChunk = false; keys[id] = key; if (keys.size() > 64) keys.clear(); }
    bool inChunk = false;
    std::map<uint64_t, uint64_t> keys;
};

int main(int argc, char** argv) {
    int events = argc > 1 ? std::atoi(argv[1]) : 20000;
    Rng r;
    r.seed(argc > 2 ? std::atoi(argv[2]) : 7);
    GameUI ui;
    ui.savePath = "/tmp/monkey_save.roc";
    NullCanvas c;
    ui.render(c);
    // start a game quickly, then go wild
    GameSettings s;
    s.primate = r.range(8);
    s.mapSize = r.range(2);
    s.seed = r.next();
    ui.startGame(s);
    int turnsEnded = 0;
    static const int KEYS[] = {KEY_ENTER, KEY_ESC, KEY_SPACE, KEY_TAB, KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_PLUS, KEY_MINUS,
                               KEY_DELETE, KEY_F1, 'B', 'F', 'S', 'W', 'E', 'X', 'M', 'C', 'T', 'P', 'D', 'L'};
    for (int i = 0; i < events; i++) {
        int x = r.range(c.width()), y = r.range(c.height());
        int what = r.range(100);
        if (what < 40) {
            ui.mouseMove(x, y);
            ui.mouseDown(x, y, MOUSE_LEFT);
            if (r.chance(0.2)) ui.mouseMove(x + r.between(-80, 80), y + r.between(-80, 80));
            ui.mouseUp(x, y, MOUSE_LEFT);
        } else if (what < 60) {
            ui.mouseMove(x, y);
            ui.mouseDown(x, y, MOUSE_RIGHT);
            ui.mouseUp(x, y, MOUSE_RIGHT);
        } else if (what < 66) {
            ui.wheel(x, y, r.chance(0.5) ? 1 : -1);
        } else if (what < 70) {
            ui.mouseMove(x, y);
        } else {
            int k = KEYS[r.range((int)(sizeof(KEYS) / sizeof(KEYS[0])))];
            if (k == KEY_ENTER) turnsEnded++;
            if (k == 'S' && r.chance(0.05)) { ui.key('S', false, true); }      // ctrl+S
            else if (k == 'L' && r.chance(0.05)) { ui.key('L', false, true); } // ctrl+L
            else ui.key(k, false, false);
        }
        ui.tick(r.range(400));
        ui.render(c);
        if (ui.screen != GameUI::SCR_GAME || !ui.game || (ui.game->gameOver && ui.game->winner < 0)) {
            // got kicked to the title or setup: start again
            GameSettings s2;
            s2.primate = r.range(8);
            s2.mapSize = r.range(2);
            s2.seed = r.next();
            ui.startGame(s2);
        }
        if (ui.wantsQuit()) break;
        if (i % 2000 == 0)
            std::printf("event %d: turn %d, units %zu, cities %zu\n", i, ui.game ? ui.game->turn : -1, ui.game ? ui.game->units.size() : 0,
                        ui.game ? ui.game->cities.size() : 0);
    }
    std::printf("monkey done: %d enter presses, final turn %d, gameOver=%d (%s), quit=%d\n", turnsEnded, ui.game ? ui.game->turn : -1, ui.game ? (int)ui.game->gameOver : -1, ui.game ? ui.game->victoryType.c_str() : "", (int)ui.wantsQuit());
    return 0;
}
