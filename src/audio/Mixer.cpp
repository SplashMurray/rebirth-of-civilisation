// Rebirth of Civilisation - portable software mixer
// SPDX-License-Identifier: MIT
#include "Mixer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#define STB_VORBIS_HEADER_ONLY
#include "../../third_party/stb_vorbis.c"

struct Mixer::Clip {
    std::vector<short> pcm;  // interleaved
    int ch = 1;
    size_t frames() const { return ch ? pcm.size() / (size_t)ch : 0; }
};

struct Mixer::Stream {
    std::vector<unsigned char> data;
    stb_vorbis* v = nullptr;
    float gain = 0, target = 0, step = 0;
    void close() {
        if (v) stb_vorbis_close(v);
        v = nullptr;
        data.clear();
    }
};

namespace {
bool readFile(const std::string& path, std::vector<unsigned char>& out) {
#ifdef _WIN32
    std::wstring w;  // paths are UTF-8; Windows wants UTF-16
    int n16 = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (n16 <= 0) return false;
    w.resize((size_t)n16);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &w[0], n16);
    FILE* f = _wfopen(w.c_str(), L"rb");
#else
    FILE* f = fopen(path.c_str(), "rb");
#endif
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return false; }
    out.resize((size_t)n);
    bool ok = fread(out.data(), 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    return ok;
}

bool loadClip(const std::string& path, Mixer::Clip& c) {
    std::vector<unsigned char> data;
    if (!readFile(path, data)) return false;
    int ch = 0, rate = 0;
    short* out = nullptr;
    int n = stb_vorbis_decode_memory(data.data(), (int)data.size(), &ch, &rate, &out);
    if (n <= 0 || !out || ch < 1 || ch > 2) { free(out); return false; }
    c.ch = ch;
    c.pcm.assign(out, out + (size_t)n * (size_t)ch);
    free(out);
    return true;
}

const std::vector<std::vector<const char*>>& moodTracks() {
    static const std::vector<std::vector<const char*>> T = {
        {},                                                                  // None
        {"title"},                                                           // Title
        {"wander_1", "wander_2", "wander_3", "wander_4", "wander_5"},       // Wander
        {"ancient_1", "ancient_2", "ancient_3", "ancient_4", "ancient_5"},  // Ancient
        {"later_1", "later_2", "later_3", "ancient_2", "ancient_5"},         // Later
        {"war_1", "war_2", "war_3"},                                         // War
        {"fall", "fall_2"},                                                  // Fall
        {"victory"},                                                         // Victory
        {"fall", "fall_2"},                                                  // Defeat
        {"industrial_1", "industrial_2", "industrial_3", "industrial_4"},   // Industrial
        {"modern_1", "modern_2", "modern_3", "industrial_4"},               // Modern
        {"modern_war_1", "modern_war_2", "modern_war_3", "modern_war_4", "modern_war_5"},  // ModernWar
    };
    return T;
}

// a soft limiter above 0.8, so many sounds at once round off instead of clipping harshly
float limit(float x) {
    float a = std::fabs(x);
    if (a <= 0.8f) return x;
    float y = 0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f);
    return x < 0 ? -y : y;
}
}  // namespace

Mixer::Mixer() : cur(new Stream), old(new Stream) {
    for (int a = 0; a < AMB_COUNT; a++) ambPos2[a] = -1;
}

Mixer::~Mixer() {
    cur->close();
    old->close();
    delete cur;
    delete old;
    for (auto* c : sfx) delete c;
    for (auto* c : amb) delete c;
    for (auto* c : voice) delete c;
}

bool Mixer::load(const std::string& d) {
    dir = d;
    std::vector<Clip*> s, a;
    int found = 0;
    for (int i = 0; i < (int)Sfx::COUNT; i++) {
        s.push_back(new Clip);
        if (loadClip(dir + "sfx/" + sfxName((Sfx)i) + ".ogg", *s.back())) found++;
    }
    for (int i = 0; i < AMB_COUNT; i++) {
        a.push_back(new Clip);
        loadClip(dir + "ambient/" + ambientName(i) + ".ogg", *a.back());
    }
    std::vector<Clip*> v;
    for (int p = 0; p < 8; p++)
        for (int k = 1; k <= VOICES_PER_PRIMATE; k++) {
            v.push_back(new Clip);
            loadClip(dir + "voice/" + primateVoiceName(p) + "_" + std::to_string(k) + ".ogg", *v.back());
        }
    {
        std::lock_guard<std::mutex> lk(m);
        sfx.swap(s);
        amb.swap(a);
        voice.swap(v);
    }
    for (auto* c : s) delete c;
    for (auto* c : a) delete c;
    for (auto* c : v) delete c;
    ready = found > 0;
    return ready;
}

bool Mixer::startTrack() {
    const auto& list = moodTracks()[(int)playing];
    if (list.empty()) return false;
    rng = rng * 1103515245u + 12345u;
    int pick = (int)((rng >> 8) % list.size());
    if (list.size() > 1 && pick == lastTrack) pick = (pick + 1) % (int)list.size();
    lastTrack = pick;
    cur->close();
    if (!readFile(dir + "music/" + list[(size_t)pick] + ".ogg", cur->data)) return false;
    int err = 0;
    cur->v = stb_vorbis_open_memory(cur->data.data(), (int)cur->data.size(), &err, nullptr);
    if (!cur->v) { cur->data.clear(); return false; }
    cur->gain = 0;
    cur->target = 1;
    cur->step = 1.0f / (1.5f * RATE);
    return true;
}

// decodes n frames of a stream into the mix; false when the stream has ended
bool Mixer::mixStream(Stream& s, int n) {
    if (!s.v) return false;
    tmp.resize((size_t)n * 2);
    int got = stb_vorbis_get_samples_short_interleaved(s.v, 2, tmp.data(), n * 2);
    for (int i = 0; i < got; i++) {
        if (s.gain < s.target) s.gain = std::min(s.target, s.gain + s.step);
        else if (s.gain > s.target) s.gain = std::max(s.target, s.gain - s.step);
        float g = s.gain * musicVol / 32768.0f;
        mixL[(size_t)i] += tmp[(size_t)i * 2] * g;
        mixR[(size_t)i] += tmp[(size_t)i * 2 + 1] * g;
    }
    return got == n;
}

void Mixer::fill(short* out, int frames) {
    mixL.assign((size_t)frames, 0.0f);
    mixR.assign((size_t)frames, 0.0f);
    {
        std::lock_guard<std::mutex> lk(m);
        // music: crossfade into a new mood, then play its tracks with a pause between them
        if (mood != playing) {
            if (cur->v) {
                old->close();
                std::swap(cur, old);
                old->target = 0;
                old->step = 1.0f / (2.5f * RATE);
            }
            playing = mood;
            gap = RATE / 2;
            lastTrack = -1;
        }
        if (!cur->v && gap <= 0 && playing != Mood::None && !startTrack()) gap = RATE * 5;
        if (gap > 0) gap -= frames;
        if (cur->v && !mixStream(*cur, frames)) {
            cur->close();
            gap = playing == Mood::Victory || playing == Mood::Defeat || playing == Mood::Fall ? RATE * 40 : RATE * 4;
        }
        if (old->v && (!mixStream(*old, frames) || old->gain <= 0.0f)) old->close();
        // ambient loops, overlapping each loop's fade-out with its fade-in at the seam
        const long long seam = (long long)(0.4 * RATE);
        for (int a = 0; a < AMB_COUNT && a < (int)amb.size(); a++) {
            const Clip& c = *amb[(size_t)a];
            long long len = (long long)c.frames();
            if (len <= seam * 2) continue;
            for (int i = 0; i < frames; i++) {
                ambLevel[a] += (ambTarget[a] - ambLevel[a]) * 0.00005f;
                if (ambLevel[a] < 0.001f && ambTarget[a] <= 0.0f) { ambLevel[a] = 0; break; }
                float s = c.pcm[(size_t)(ambPos[a] * c.ch)] / 32768.0f;
                if (ambPos2[a] >= 0) s += c.pcm[(size_t)(ambPos2[a] * c.ch)] / 32768.0f;
                float g = s * ambLevel[a] * ambVol;
                mixL[(size_t)i] += g;
                mixR[(size_t)i] += g;
                if (++ambPos[a] >= len - seam && ambPos2[a] < 0) ambPos2[a] = 0;
                else if (ambPos2[a] >= 0) ambPos2[a]++;
                if (ambPos[a] >= len) { ambPos[a] = ambPos2[a]; ambPos2[a] = -1; }
            }
        }
        // sound effects
        for (auto& v : voices) {
            size_t total = v.c->frames();
            for (int i = 0; i < frames && v.pos < total; i++) {
                if (v.delay > 0) { v.delay--; continue; }
                float l, r;
                if (v.c->ch == 2) { l = v.c->pcm[v.pos * 2] / 32768.0f; r = v.c->pcm[v.pos * 2 + 1] / 32768.0f; }
                else l = r = v.c->pcm[v.pos] / 32768.0f;
                mixL[(size_t)i] += l * v.gl * sfxVol;
                mixR[(size_t)i] += r * v.gr * sfxVol;
                v.pos++;
            }
        }
        voices.erase(std::remove_if(voices.begin(), voices.end(), [](const Voice& v) { return v.delay <= 0 && v.pos >= v.c->frames(); }), voices.end());
    }
    for (int i = 0; i < frames; i++) {
        out[i * 2] = (short)std::lround(limit(mixL[(size_t)i]) * 32000.0f);
        out[i * 2 + 1] = (short)std::lround(limit(mixR[(size_t)i]) * 32000.0f);
    }
}

void Mixer::addVoice(const Clip& c, float vol, float pan, int delayMs) {  // m held
    if (c.pcm.empty() || voices.size() > 24) return;
    pan = std::max(-1.0f, std::min(1.0f, pan));
    float gl = vol * std::sqrt(0.5f * (1.0f - pan)) * 1.41f, gr = vol * std::sqrt(0.5f * (1.0f + pan)) * 1.41f;
    voices.push_back({&c, 0, std::min(gl, 1.2f), std::min(gr, 1.2f), delayMs * (RATE / 1000)});
}

void Mixer::play(Sfx s, float vol, float pan, int delayMs) {
    if (!ready || (int)s < 0 || (int)s >= (int)Sfx::COUNT) return;
    std::lock_guard<std::mutex> lk(m);
    if ((size_t)s < sfx.size()) addVoice(*sfx[(size_t)s], vol, pan, delayMs);
}

void Mixer::call(int primate, float vol, float pan, int delayMs) {
    if (!ready || primate < 0 || primate >= 8) return;
    std::lock_guard<std::mutex> lk(m);
    std::vector<const Clip*> have;
    for (int k = 0; k < VOICES_PER_PRIMATE; k++) {
        size_t i = (size_t)(primate * VOICES_PER_PRIMATE + k);
        if (i < voice.size() && !voice[i]->pcm.empty()) have.push_back(voice[i]);
    }
    if (have.empty()) return;
    rng = rng * 1103515245u + 12345u;
    addVoice(*have[(rng >> 8) % have.size()], vol, pan, delayMs);
}

void Mixer::setMood(Mood md) {
    std::lock_guard<std::mutex> lk(m);
    mood = md;
}

void Mixer::setAmbient(int a, float level) {
    if (a < 0 || a >= AMB_COUNT) return;
    std::lock_guard<std::mutex> lk(m);
    ambTarget[a] = std::max(0.0f, std::min(1.0f, level));
}

void Mixer::setVolumes(int music, int sfxV, int ambient) {
    auto curve = [](int v) { float f = std::max(0, std::min(100, v)) / 100.0f; return f * f; };
    std::lock_guard<std::mutex> lk(m);
    musicVol = curve(music);
    sfxVol = curve(sfxV);
    ambVol = curve(ambient);
}

int Mixer::voicesPlaying() {
    std::lock_guard<std::mutex> lk(m);
    return (int)voices.size();
}

int Mixer::callsLoaded() {
    std::lock_guard<std::mutex> lk(m);
    int n = 0;
    for (auto* c : voice) if (!c->pcm.empty()) n++;
    return n;
}
