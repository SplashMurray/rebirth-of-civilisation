// Rebirth of Civilisation - sound interface
// The interface asks for sounds by name; a platform audio backend plays them. Without one, all
// calls do nothing (tests, headless builds).
// SPDX-License-Identifier: MIT
#pragma once

enum class Sfx {
    Click, Notify, Invalid, Tech, Evolve, Achievement, War, CityAttacked, Victory, Defeat,
    CityFounded, Built, Trade, TradeSea, Culture,
    Step, StepSand, StepSnow, Horse, Ship, ShipMove, SiegeSelect,
    Sword, SwordHit, Spear, Bow, ArrowHit, Sling, ShieldHit, Musket, Catapult, Ballista, SiegeHit, Cannon,
    Death, HorseDeath, ShipSinks, Beast, Nuke, Collapse, Thunder, Wood, Hoe, Dig,
    COUNT
};

// file names under assets/audio/sfx, in the order of Sfx
inline const char* sfxName(Sfx s) {
    static const char* N[] = {"click", "notify", "invalid", "tech", "evolve", "achievement", "war", "city_attacked", "victory", "defeat",
                              "city_founded", "built", "trade", "trade_sea", "culture",
                              "step", "step_sand", "step_snow", "horse", "ship", "ship_move", "siege_select",
                              "sword", "sword_hit", "spear", "bow", "arrow_hit", "sling", "shield_hit", "musket", "catapult", "ballista", "siege_hit", "cannon",
                              "death", "horse_death", "ship_sinks", "beast", "nuke", "collapse", "thunder", "wood", "hoe", "dig"};
    static_assert(sizeof(N) / sizeof(N[0]) == (int)Sfx::COUNT, "sound names out of step");
    return N[(int)s];
}

// The music to play: each mood has a few tracks that play in turn, crossfading when the mood changes.
enum class Mood { None, Title, Wander, Ancient, Later, War, Fall, Victory, Defeat, Industrial, Modern, ModernWar };

// Looping background sounds, mixed by how much of each is on screen.
enum Ambient { AMB_WAVES, AMB_JUNGLE, AMB_BIRDS, AMB_WIND, AMB_COUNT };
// file name stems under assets/audio/voice, in g_primates order
inline const char* primateVoiceName(int p) {
    static const char* N[] = {"chimpanzee", "bonobo", "orangutan", "gorilla", "baboon", "capuchin", "gibbon", "macaque"};
    return p >= 0 && p < 8 ? N[p] : "";
}
const int VOICES_PER_PRIMATE = 3;

inline const char* ambientName(int a) {
    static const char* N[] = {"waves", "jungle", "birds", "wind"};
    return N[a];
}

class SoundSink {
public:
    virtual ~SoundSink() {}
    // vol 0..1, pan -1 (left) .. 1 (right), delayMs: start later (e.g. an impact after the shot)
    virtual void play(Sfx s, float vol = 1.0f, float pan = 0.0f, int delayMs = 0) { (void)s; (void)vol; (void)pan; (void)delayMs; }
    virtual void setMood(Mood m) { (void)m; }
    // a call of one primate species (index into g_primates); a few recordings per species
    virtual void call(int primate, float vol = 1.0f, float pan = 0.0f, int delayMs = 0) { (void)primate; (void)vol; (void)pan; (void)delayMs; }
    virtual void setAmbient(int amb, float level) { (void)amb; (void)level; }
    virtual void setVolumes(int music, int sfx, int ambient) { (void)music; (void)sfx; (void)ambient; }  // 0..100
};
