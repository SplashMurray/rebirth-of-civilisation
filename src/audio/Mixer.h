// Rebirth of Civilisation - portable software mixer
// Mixes sound effects, looping ambience and streamed music into 16-bit stereo at 44.1 kHz.
// A platform backend calls fill() from its audio thread; the interface calls the SoundSink methods.
// SPDX-License-Identifier: MIT
#pragma once
#include "../ui/Sound.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class Mixer : public SoundSink {
public:
    static const int RATE = 44100;
    Mixer();
    ~Mixer() override;
    // Loads the sound effects and ambience from dir (UTF-8, ending in a path separator).
    // Returns false if they are not there. Safe to call from the audio thread.
    bool load(const std::string& dir);
    bool loaded() const { return ready; }
    void fill(short* out, int frames);  // interleaved stereo
    void play(Sfx s, float vol, float pan, int delayMs) override;
    void setMood(Mood m) override;
    void setAmbient(int amb, float level) override;
    void setVolumes(int music, int sfx, int ambient) override;
    int voicesPlaying();
    struct Clip;
    struct Stream;

private:
    std::string dir;
    std::atomic<bool> ready{false};
    std::mutex m;
    std::vector<Clip*> sfx, amb;
    struct Voice { const Clip* c; size_t pos; float gl, gr; int delay; };
    std::vector<Voice> voices;
    float ambTarget[AMB_COUNT] = {}, ambLevel[AMB_COUNT] = {};
    long long ambPos[AMB_COUNT] = {}, ambPos2[AMB_COUNT] = {};
    Mood mood = Mood::None, playing = Mood::None;
    float musicVol = 0.36f, sfxVol = 0.64f, ambVol = 0.36f;
    Stream *cur, *old;
    int gap = 0, lastTrack = -1;
    unsigned rng = 12345;
    std::vector<float> mixL, mixR;
    std::vector<short> tmp;
    bool startTrack();
    bool mixStream(Stream& s, int n);
};
