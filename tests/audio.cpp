// Rebirth of Civilisation - mixer test: loads the shipped sounds, mixes 40 seconds of music, ambience
// and effects (with a mood change), and checks the output is sane. Optionally writes a WAV to listen to.
// usage: rebirth_audio <assets/audio/ dir> [out.wav]
// SPDX-License-Identifier: MIT
#include "audio/Mixer.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: rebirth_audio <audio dir> [out.wav]\n"); return 2; }
    Mixer mx;
    if (!mx.load(argv[1])) { printf("FAIL: could not load the sounds from %s\n", argv[1]); return 1; }
    mx.setVolumes(60, 80, 60);
    mx.setMood(Mood::Wander);
    mx.setAmbient(AMB_WAVES, 0.8f);
    mx.setAmbient(AMB_JUNGLE, 0.6f);
    std::vector<short> all, buf(2048);
    double sum = 0, peak = 0, musicSum = 0;
    int blocks = 40 * Mixer::RATE / 1024;
    for (int f = 0; f < blocks; f++) {
        if (f % 30 == 0) mx.play((Sfx)(f / 30 % (int)Sfx::COUNT), 1.0f, (f % 3) - 1.0f, 0);
        if (f == blocks / 2) mx.setMood(Mood::War);
        if (f == blocks * 3 / 4) mx.setAmbient(AMB_WAVES, 0.0f);
        mx.fill(buf.data(), 1024);
        for (short s : buf) {
            double a = std::abs(s) / 32768.0;
            peak = std::max(peak, a);
            sum += a * a;
            if (f > 200 && f < 300) musicSum += a * a;
        }
        all.insert(all.end(), buf.begin(), buf.end());
    }
    double rms = std::sqrt(sum / all.size());
    printf("rms %.3f peak %.3f\n", rms, peak);
    if (argc > 2) {
        FILE* fo = fopen(argv[2], "wb");
        if (fo) {
            uint32_t data = (uint32_t)all.size() * 2, riff = 36 + data, rate = Mixer::RATE, br = Mixer::RATE * 4, fl = 16;
            uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
            fwrite("RIFF", 1, 4, fo); fwrite(&riff, 4, 1, fo); fwrite("WAVEfmt ", 1, 8, fo); fwrite(&fl, 4, 1, fo);
            fwrite(&pcm, 2, 1, fo); fwrite(&ch, 2, 1, fo); fwrite(&rate, 4, 1, fo); fwrite(&br, 4, 1, fo); fwrite(&ba, 2, 1, fo); fwrite(&bits, 2, 1, fo);
            fwrite("data", 1, 4, fo); fwrite(&data, 4, 1, fo); fwrite(all.data(), 2, all.size(), fo);
            fclose(fo);
        }
    }
    if (!(rms > 0.01 && rms < 0.6) || musicSum <= 0) { printf("FAIL: output level out of range\n"); return 1; }
    printf("OK\n");
    return 0;
}
