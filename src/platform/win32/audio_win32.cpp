// Rebirth of Civilisation - Windows audio: feeds the mixer's output to waveOut
// SPDX-License-Identifier: MIT
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "audio_win32.h"
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <thread>
#include <vector>

namespace {
const int FRAMES = 1024;  // per buffer: 23 ms
const int NBUF = 4;
}  // namespace

struct WinAudio::Impl {
    HWAVEOUT wo = nullptr;
    HANDLE evt = nullptr;
    WAVEHDR hdr[NBUF] = {};
    std::vector<short> buf[NBUF];
    std::thread th;
    std::atomic<bool> quit{false};
};

WinAudio::WinAudio() : p(new Impl) {}

WinAudio::~WinAudio() {
    if (p->th.joinable()) {
        p->quit = true;
        if (p->evt) SetEvent(p->evt);
        p->th.join();
    }
    if (p->wo) {
        waveOutReset(p->wo);
        for (auto& h : p->hdr)
            if (h.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(p->wo, &h, sizeof(WAVEHDR));
        waveOutClose(p->wo);
    }
    if (p->evt) CloseHandle(p->evt);
    delete p;
}

bool WinAudio::start(const std::string& dir) {
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = Mixer::RATE;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = Mixer::RATE * 4;
    p->evt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!p->evt || waveOutOpen(&p->wo, WAVE_MAPPER, &fmt, (DWORD_PTR)p->evt, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        p->wo = nullptr;
        return false;
    }
    for (int i = 0; i < NBUF; i++) {
        p->buf[i].assign(FRAMES * 2, 0);
        p->hdr[i].lpData = (LPSTR)p->buf[i].data();
        p->hdr[i].dwBufferLength = FRAMES * 4;
        waveOutPrepareHeader(p->wo, &p->hdr[i], sizeof(WAVEHDR));
    }
    p->th = std::thread([this, dir] {
        if (!mixer.load(dir)) return;  // no sounds shipped: stay silent
        while (!p->quit) {
            for (int i = 0; i < NBUF; i++) {
                if (p->hdr[i].dwFlags & WHDR_INQUEUE) continue;
                mixer.fill(p->buf[i].data(), FRAMES);
                waveOutWrite(p->wo, &p->hdr[i], sizeof(WAVEHDR));
            }
            WaitForSingleObject(p->evt, 40);
        }
    });
    return true;
}
