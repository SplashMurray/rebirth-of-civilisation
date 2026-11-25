// Rebirth of Civilisation - Windows audio: feeds the mixer's output to waveOut
// SPDX-License-Identifier: MIT
#pragma once
#include "../../audio/Mixer.h"
#include <string>

class WinAudio {
public:
    WinAudio();
    ~WinAudio();
    // Opens the sound device and starts the audio thread, which loads the sounds from audioDir
    // (UTF-8, ending in a separator). Returns false if there is no device or no sounds.
    bool start(const std::string& audioDir);
    Mixer mixer;

private:
    struct Impl;
    Impl* p;
};
