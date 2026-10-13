// Kinetik KDSP-16 — synthesis voice: one per track, many "machines"
#pragma once
#include "Dsp.hpp"
#include "Params.hpp"
#include "Sample.hpp"
#include <cstring>
#include <vector>
#include <algorithm>

namespace kn {

struct Voice {
    float sr = 48000.f, isr = 1.f / 48000.f;
    int machine = mOFF;
    Family fam = fOff;
    bool active = false;
    uint32_t t = 0;      // samples since trigger
    float vel = 1.f;
    float pitchMul = 1.f;   // MIDI melodic play: 2^(semitones/12), set at trigger
    bool fmQuant = false;   // snap FM ratios to musical values
    int oscShape = 0;       // tone machines: 0 sine, 1 triangle, 2 square
    Rng rng;

    // ---- shared generators ----
    float ph1 = 0, ph2 = 0, phm = 0;
    float f1 = 100, ratio2 = 1.5f, lvl2 = 0, amp2 = 0, coef2 = 0.999f;
    float amp = 0, ampCoef = 0.999f;
    float penv = 0, pCoef = 0.99f, pDepth = 0;
    float harm = 0, wave = 0;
    float nLvl = 0, nAmp = 0, nCoef = 0.999f;
    int nMode = 0; // 0 lp, 1 bp, 2 hp
    Svf nFilt, outFilt, outFilt2, clickFilt;
    int outMode = 0; // 0 none, 1 hp, 2 bp, 3 lp
    float clickLvl = 0, clickAmp = 0, clickCoef = 0.99f;
    float drv = 0;
    uint32_t attackS = 0, holdS = 0;
    float gainNorm = 1.f;

    // clap
    int bursts = 0, burstIdx = 0;
    uint32_t burstStart[8] = {};
    float clapSpacing = 400, clapJitter = 0;
    float burstAmp = 0, burstCoef = 0.99f, tailLvl = 0, tailAmp = 0, tailCoef = 0.999f;

    // metal
    float mph[6] = {}, mfreq[6] = {};
    float metl = 0, fastAmp = 0, fastCoef = 0.99f, shrt = 0;
    int metalCount = 6;
    float bellLvl = 0, bellAmp = 0, bellCoef = 0.999f, bph[3] = {};

    // fm
    float modIdx = 0, modEnv = 0, modCoef = 0.99f, fb = 0, mPrev = 0, mPrev2 = 0;
    float trem = 0, tremF = 10, tremPh = 0;
    bool fmClap = false;

    // shaker / noise
    float grain = 0, grainAmp = 0, grainCoef = 0.99f, grainRate = 100;
    float crush = 0, crushHold = 0; int crushCnt = 0;

    // modal
    static constexpr int kModes = 8;
    float rz1[kModes] = {}, rz2[kModes] = {}, ra1[kModes] = {}, ra2[kModes] = {}, rb0[kModes] = {}, rg[kModes] = {};
    int modes = 6;
    float excAmp = 0, excCoef = 0.9f, excNoise = 0, gainSumInv = 1.f; uint32_t excLen = 1; OnePole excLP;
    float tens = 0, tensEnv = 0;
    float snareLvl = 0, snareAmp = 0, snareCoef = 0.999f;
    float modalRing = 0;
    float modalBase = 100, modalT60 = 1, modalDamp = 0.5f;
    float modalRatio[kModes] = {}, modalGain[kModes] = {};

    // sample (the engine points this at the track's sample before each block)
    const SampleData* sample = nullptr;
    double sPos = 0, sStart = 0, sEnd = 0, sInc = 1;
    float sTune = 0, sAtk = 0, sDec = 1.f, sAmp = 1.f, sLofi = 0, sHoldV = 0; bool sRev = false; int sHoldCnt = 0;

    // string
    std::vector<float> ks;
    uint32_t ksW = 0;
    float ksDelay = 100, ksGain = 0.99f, ksDamp = 0.3f, ksLp = 0, ksApZ = 0, ksStif = 0;
    uint32_t ksExcLen = 0; float ksNoise = 0.5f, ksPick = 0.5f; OnePole ksExcLP;
    std::vector<float> ksExc;

    void init(float sampleRate)
    {
        sr = sampleRate; isr = 1.f / sr;
        ks.assign(16384, 0.f);
        ksExc.assign(16384, 0.f);
    }

    void setMachine(int m)
    {
        if (m < 0 || m >= kMachines) m = 0;
        if (m == machine) return;
        machine = m; fam = kMachineDefs[m].family;
        active = false;
        std::memset(rz1, 0, sizeof(rz1)); std::memset(rz2, 0, sizeof(rz2));
        std::fill(ks.begin(), ks.end(), 0.f);
        nFilt.reset(); outFilt.reset(); outFilt2.reset();
    }

    // p: 8 synth params 0..127 (effective, including locks/LFO)
    void control(const float* p)
    {
        float x[8];
        for (int i = 0; i < 8; ++i) x[i] = clampf(p[i], 0.f, 127.f) / 127.f;
        attackS = 0; holdS = 0; outMode = 0; drv = 0; harm = 0; wave = 0;
        lvl2 = 0; nLvl = 0; clickLvl = 0; pDepth = 0; gainNorm = 1.f;

        switch (machine) {
        case mKICK:
            f1 = expmap(x[0], 25, 160); ampCoef = decayCoef(expmap(x[1], 0.05f, 3.f), sr);
            pDepth = x[2] * 4.f; pCoef = decayCoef(expmap(x[3], 0.004f, 0.5f), sr);
            clickLvl = x[4] * 0.9f; clickCoef = decayCoef(0.012f, sr); clickFilt.setup(2500, 0.8f, sr);
            nLvl = x[5] * 0.6f; nCoef = ampCoef; nMode = 0; nFilt.setup(3000, 0.7f, sr);
            harm = x[6]; drv = x[7]; gainNorm = 0.9f;
            break;
        case mTOM:
            f1 = expmap(x[0], 50, 600); ampCoef = decayCoef(expmap(x[1], 0.05f, 2.f), sr);
            pDepth = x[2] * 2.f; pCoef = decayCoef(expmap(x[3], 0.005f, 0.6f), sr);
            clickLvl = x[4] * 0.6f; clickCoef = decayCoef(0.01f, sr); clickFilt.setup(3500, 0.8f, sr);
            nLvl = x[5] * 0.5f; nCoef = decayCoef(expmap(x[1], 0.03f, 0.6f), sr); nMode = 1; nFilt.setup(f1 * 3.f, 1.f, sr);
            harm = x[6]; drv = x[7]; gainNorm = 0.85f;
            break;
        case mCLAVE:
            f1 = expmap(x[0], 800, 4000); ampCoef = decayCoef(expmap(x[1], 0.01f, 0.5f), sr);
            clickLvl = x[2] * 0.7f; clickCoef = decayCoef(0.004f, sr); clickFilt.setup(6000, 1.f, sr);
            ratio2 = 1.5f + x[3] * 2.5f; lvl2 = x[4] * 0.8f; coef2 = decayCoef(expmap(x[1], 0.005f, 0.25f), sr);
            nLvl = x[5] * 0.5f; nCoef = decayCoef(expmap(x[6], 0.005f, 0.2f), sr); nMode = 2; nFilt.setup(3000, 0.7f, sr);
            drv = x[7]; gainNorm = 0.6f;
            break;
        case mSINE:
            f1 = expmap(x[0], 30, 4000);
            ampCoef = x[1] >= 0.999f ? 1.f : decayCoef(expmap(x[1], 0.02f, 10.f), sr);
            pDepth = x[2] * 4.f; pCoef = decayCoef(expmap(x[3], 0.004f, 1.f), sr);
            wave = x[4]; attackS = (uint32_t)(x[5] * x[5] * 1.0f * sr); holdS = (uint32_t)(x[6] * 2.f * sr);
            drv = x[7]; gainNorm = 0.6f;
            break;
        case mSNARE:
            f1 = expmap(x[0], 100, 450); ampCoef = decayCoef(expmap(x[1], 0.02f, 0.8f), sr);
            pDepth = x[2]; pCoef = decayCoef(expmap(x[3], 0.005f, 0.25f), sr);
            nLvl = x[4] * 0.9f; nMode = 2; nFilt.setup(expmap(x[5], 300, 9000), 0.7f, sr);
            ratio2 = 1.2f + x[6] * 1.2f; lvl2 = 0.55f; coef2 = ampCoef;
            nCoef = decayCoef(expmap(x[7], 0.03f, 1.0f), sr);
            outMode = 3; outFilt.setup(13000, 0.7f, sr); gainNorm = 0.7f;
            break;
        case mRIM:
            f1 = expmap(x[0], 300, 2000); ampCoef = decayCoef(expmap(x[1], 0.005f, 0.3f), sr);
            ratio2 = 0.42f; lvl2 = x[2]; coef2 = ampCoef;
            clickLvl = x[3]; clickCoef = decayCoef(0.003f, sr); clickFilt.setup(5000, 1.2f, sr);
            nLvl = x[4] * 0.7f; nCoef = decayCoef(expmap(x[5], 0.005f, 0.3f), sr); nMode = 2; nFilt.setup(expmap(x[6], 500, 10000), 0.7f, sr);
            harm = 0.4f; drv = x[7]; gainNorm = 0.6f;
            break;
        case mCLAP:
            bursts = 1 + (int)(x[0] * 5.f + 0.5f);
            tailCoef = decayCoef(expmap(x[1], 0.05f, 1.5f), sr);
            clapSpacing = expmap(x[2], 0.003f, 0.03f) * sr; clapJitter = x[6] * 0.5f;
            burstCoef = decayCoef(0.018f, sr);
            outMode = 2; outFilt.setup(expmap(x[3], 500, 5000), 0.6f + x[4] * 8.f, sr);
            tailLvl = x[5]; drv = x[7]; gainNorm = 1.4f;
            break;
        case mCOWBELL:
            f1 = expmap(x[0], 250, 1200);
            ratio2 = 1.3f + x[2] * 0.5f;
            ampCoef = decayCoef(expmap(x[1], 0.05f, 1.5f), sr);
            fastCoef = decayCoef(0.06f, sr); shrt = x[5];
            outMode = 2; outFilt.setup(expmap(x[3], 500, 6000), 0.7f + x[4] * 6.f, sr);
            clickLvl = x[6]; clickCoef = decayCoef(0.004f, sr); clickFilt.setup(4000, 1.f, sr);
            drv = x[7]; metalCount = 2; gainNorm = 0.9f; bellLvl = 0;
            mfreq[0] = f1; mfreq[1] = f1 * ratio2;
            break;
        case mCLHAT: case mOPHAT: case mCYMBAL: {
            static const float R[6] = {1.f, 1.4827f, 1.8003f, 2.5461f, 2.6303f, 3.8968f};
            const float base = expmap(x[0], 100, 600);
            const float spread = 0.5f + x[5];
            for (int i = 0; i < 6; ++i) mfreq[i] = base * (1.f + (R[i] - 1.f) * spread);
            metalCount = 6;
            float dec;
            if (machine == mCLHAT) dec = expmap(x[1], 0.01f, 0.5f);
            else if (machine == mOPHAT) dec = expmap(x[1], 0.1f, 3.f);
            else dec = expmap(x[1], 0.3f, 6.f);
            ampCoef = decayCoef(dec, sr);
            outMode = 1; outFilt.setup(expmap(x[2], 1000, 12000), 0.5f + x[4] * 4.f, sr);
            outFilt2.setup(expmap(x[2], 1000, 12000) * 1.4f, 0.8f, sr);
            metl = x[3];
            if (machine == mCYMBAL) {
                bellLvl = x[6] * 0.5f; bellCoef = decayCoef(dec * 0.7f, sr);
                f1 = base * 2.f;
            } else {
                bellLvl = 0;
                attackS = (uint32_t)(x[6] * x[6] * 0.02f * sr);
            }
            drv = x[7]; gainNorm = 1.4f;
            break;
        }
        case mSHAKER:
            attackS = (uint32_t)(expmap(x[0], 0.0005f, 0.1f) * sr);
            holdS = (uint32_t)(x[1] * 0.5f * sr);
            ampCoef = decayCoef(expmap(x[2], 0.02f, 1.0f), sr);
            outMode = 1; outFilt.setup(expmap(x[3], 500, 12000), 0.7f, sr);
            outFilt2.setup(expmap(x[4], 1000, 20000), 0.7f, sr);
            grain = x[5]; grainRate = expmap(x[6], 20, 400); grainCoef = decayCoef(0.01f, sr);
            crush = 0; drv = x[7]; gainNorm = 1.2f;
            break;
        case mNOISE:
            attackS = x[0] <= 0.f ? 0 : (uint32_t)(expmap(x[0], 0.0005f, 1.0f) * sr);
            holdS = (uint32_t)(x[1] * 2.f * sr);
            ampCoef = x[2] >= 0.999f ? 1.f : decayCoef(expmap(x[2], 0.01f, 4.f), sr);
            outMode = x[3] > 0.f ? 1 : 0; outFilt.setup(expmap(x[3], 20, 15000), 0.5f + x[5] * 9.f, sr);
            outFilt2.setup(expmap(x[4], 100, 20000), 0.5f + x[5] * 9.f, sr);
            crush = x[6]; grain = 0; drv = x[7]; gainNorm = 0.5f;
            break;
        case mFM_KICK: case mFM_SNARE: case mFM_TOM: case mFM_CLAP: case mFM_RIM: case mFM_BELL: case mFM_HAT: case mFM_CYMBAL:
            controlFM(x);
            break;
        case mPH_KICK: case mPH_SNARE: case mPH_METAL: case mPH_MALLET:
            controlModal(x);
            break;
        case mSAMPLE: {
            sTune = clampf((x[0] * 127.f - 64.f) / 63.f, -1.f, 1.f) * 24.f;   // +-24 semitones, 64 = 0
            sAtk = x[3] * x[3] * 0.25f * sr;
            sDec = x[4] >= 0.995f ? 1.f : decayCoef(expmap(x[4], 0.02f, 8.f), sr);
            sRev = x[5] >= 0.5f;
            sLofi = x[6]; drv = x[7]; gainNorm = 1.f;
            const double frames = sample ? (double)sample->frames() : 0.0;
            sStart = std::floor(x[1] * 0.98 * frames);
            sEnd = sStart + std::max(16.0, (frames - sStart) * std::max(0.01f, x[2]));
            if (sEnd > frames) sEnd = frames;
            break;
        }
        case mPH_STRING:
            controlString(x);
            break;
        default: break;
        }
        if (pitchMul != 1.f && (fam == fTone || fam == fSnare || fam == fMetal || fam == fFM)) {
            f1 *= pitchMul;
            for (int i = 0; i < 6; ++i) mfreq[i] *= pitchMul;
        }
    }

    float fmRatio(float x) const
    {
        const float r = 0.5f + x * 11.5f;
        if (!fmQuant) return r;
        static const float q[] = {0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f};
        float best = q[0];
        for (float v : q) if (std::fabs(v - r) < std::fabs(best - r)) best = v;
        return best;
    }
    static float fmIndex(float x) { return x * x * 12.f; }

    void controlFM(const float* x)
    {
        fb = 0; trem = 0; bellLvl = 0; fmClap = false; ratio2 = 1;
        switch (machine) {
        case mFM_KICK:
            f1 = expmap(x[0], 25, 150); ampCoef = decayCoef(expmap(x[1], 0.05f, 3.f), sr);
            pDepth = x[2] * 4.f; pCoef = decayCoef(expmap(x[3], 0.004f, 0.4f), sr);
            modIdx = fmIndex(x[4]); ratio2 = fmRatio(x[5]); modCoef = decayCoef(expmap(x[6], 0.003f, 1.f), sr);
            fb = x[7] * 1.2f; gainNorm = 0.9f;
            break;
        case mFM_SNARE:
            f1 = expmap(x[0], 100, 600); ampCoef = decayCoef(expmap(x[1], 0.02f, 0.8f), sr);
            nLvl = x[2] * 0.8f; nCoef = decayCoef(expmap(x[3], 0.02f, 0.8f), sr); nMode = 2;
            modIdx = fmIndex(x[4]); ratio2 = fmRatio(x[5]); modCoef = decayCoef(expmap(x[6], 0.003f, 1.f), sr);
            nFilt.setup(expmap(x[7], 200, 8000), 0.7f, sr); pDepth = 0.3f; pCoef = decayCoef(0.03f, sr);
            gainNorm = 0.7f;
            break;
        case mFM_TOM:
            f1 = expmap(x[0], 50, 700); ampCoef = decayCoef(expmap(x[1], 0.05f, 2.f), sr);
            pDepth = x[2] * 2.f; pCoef = decayCoef(expmap(x[3], 0.005f, 0.5f), sr);
            modIdx = fmIndex(x[4]); ratio2 = fmRatio(x[5]); modCoef = decayCoef(expmap(x[6], 0.003f, 1.f), sr);
            clickLvl = x[7] * 0.7f; clickCoef = decayCoef(0.006f, sr); clickFilt.setup(4000, 1.f, sr);
            gainNorm = 0.85f;
            break;
        case mFM_CLAP:
            f1 = expmap(x[0], 200, 3000); tailCoef = decayCoef(expmap(x[1], 0.05f, 1.5f), sr);
            bursts = 1 + (int)(x[2] * 5.f + 0.5f);
            clapSpacing = expmap(x[3], 0.003f, 0.03f) * sr; clapJitter = 0.1f;
            burstCoef = decayCoef(0.018f, sr); tailLvl = 0.7f; fmClap = true;
            modIdx = fmIndex(x[4]); ratio2 = fmRatio(x[5]); modCoef = decayCoef(expmap(x[6], 0.01f, 1.5f), sr);
            fb = x[7] * 1.5f; outMode = 1; outFilt.setup(400, 0.7f, sr); gainNorm = 0.6f;
            break;
        case mFM_RIM:
            f1 = expmap(x[0], 300, 2500); ampCoef = decayCoef(expmap(x[1], 0.005f, 0.25f), sr);
            modIdx = fmIndex(x[2]); ratio2 = fmRatio(x[3]); modCoef = decayCoef(expmap(x[4], 0.002f, 0.5f), sr);
            nLvl = x[5] * 0.6f; nCoef = decayCoef(expmap(x[6], 0.005f, 0.3f), sr); nMode = 2; nFilt.setup(2000, 0.7f, sr);
            fb = x[7] * 1.2f; gainNorm = 0.6f;
            break;
        case mFM_BELL:
            f1 = expmap(x[0], 250, 1500); ampCoef = decayCoef(expmap(x[1], 0.05f, 1.5f), sr);
            clickLvl = x[2] * 0.8f; clickCoef = decayCoef(0.005f, sr); clickFilt.setup(5000, 1.f, sr);
            fb = x[3] * 1.2f;
            modIdx = fmIndex(x[4]); ratio2 = fmRatio(x[5]); modCoef = decayCoef(expmap(x[6], 0.005f, 1.5f), sr);
            outMode = 3; outFilt.setup(expmap(x[7], 500, 15000), 0.8f, sr); gainNorm = 1.0f;
            break;
        case mFM_HAT:
            f1 = expmap(x[0], 1000, 10000); ampCoef = decayCoef(expmap(x[1], 0.01f, 1.5f), sr);
            trem = x[2]; tremF = expmap(x[3], 2, 200);
            modIdx = fmIndex(x[4]); ratio2 = fmRatio(x[5]) * 1.4142f; modCoef = decayCoef(expmap(x[6], 0.005f, 1.5f), sr);
            fb = 0.7f; outMode = 1; outFilt.setup(expmap(x[7], 500, 12000), 0.7f, sr); gainNorm = 0.6f;
            break;
        case mFM_CYMBAL:
            f1 = expmap(x[0], 500, 6000); ampCoef = decayCoef(expmap(x[1], 0.2f, 6.f), sr);
            fb = x[2] * 1.5f; bellLvl = x[3] * 0.6f; bellCoef = decayCoef(expmap(x[1], 0.1f, 3.f), sr);
            modIdx = fmIndex(x[4]); ratio2 = fmRatio(x[5]) * 1.4142f; modCoef = decayCoef(expmap(x[6], 0.01f, 4.f), sr);
            outMode = 1; outFilt.setup(expmap(x[7], 200, 10000), 0.7f, sr); gainNorm = 0.6f;
            break;
        }
    }

    void setMode(int i, float freq, float t60, float gain)
    {
        freq = clampf(freq, 20.f, sr * 0.45f);
        rg[i] = gain;
        const float r = std::exp(-6.9077553f / (t60 * sr));
        const float w = kTwoPi * freq / sr;
        ra1[i] = -2.f * r * std::cos(w);
        ra2[i] = r * r;
        rb0[i] = std::sin(w);
    }

    void controlModal(const float* x)
    {
        static const float membrane[kModes] = {1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f};
        const float hard = x[2];
        nLvl = 0; snareLvl = 0; tens = 0; modalRing = 0;
        switch (machine) {
        case mPH_KICK:
            modalBase = expmap(x[0], 30, 200); modalT60 = expmap(x[1], 0.05f, 3.f);
            tens = x[3] * 0.8f; modalDamp = x[4];
            modes = 2 + (int)(x[5] * 6.f);
            for (int i = 0; i < kModes; ++i) { modalRatio[i] = membrane[i]; modalGain[i] = 1.f / (1.f + i * (1.5f - x[5])); }
            nLvl = x[6] * 0.4f; nCoef = decayCoef(modalT60 * 0.3f, sr); nMode = 0; nFilt.setup(2000, 0.7f, sr);
            drv = x[7]; gainNorm = 0.7f;
            break;
        case mPH_SNARE:
            modalBase = expmap(x[0], 100, 500); modalT60 = expmap(x[1], 0.05f, 1.5f);
            tens = x[3] * 0.5f; modalDamp = 0.5f;
            snareLvl = x[4] * 0.8f; snareCoef = decayCoef(expmap(x[5], 0.05f, 0.8f), sr);
            nMode = 1; nFilt.setup(4500, 0.6f, sr);
            modes = 2 + (int)(x[6] * 6.f);
            for (int i = 0; i < kModes; ++i) { modalRatio[i] = membrane[i]; modalGain[i] = 1.f / (1.f + i * 0.6f); }
            drv = x[7]; gainNorm = 0.8f;
            break;
        case mPH_METAL: {
            modalBase = expmap(x[0], 80, 2000); modalT60 = expmap(x[1], 0.1f, 8.f);
            const float inh = 1.f + x[3];
            modalDamp = x[4]; modes = 2 + (int)(x[5] * 6.f);
            static const float jit[kModes] = {0.f, 0.031f, -0.022f, 0.044f, -0.037f, 0.018f, -0.051f, 0.027f};
            for (int i = 0; i < kModes; ++i) {
                modalRatio[i] = i == 0 ? 1.f : std::pow((float)(i + 1), inh) * (1.f + jit[i] * x[6] * 2.f);
                modalGain[i] = 1.f / (1.f + i * 0.4f);
            }
            drv = x[7]; gainNorm = 0.35f;
            break;
        }
        case mPH_MALLET: {
            modalBase = expmap(x[0], 100, 2000); modalT60 = expmap(x[1], 0.05f, 4.f);
            modalDamp = x[3];
            static const float wood[4] = {1.f, 3.99f, 10.65f, 21.f};
            static const float metal[4] = {1.f, 2.76f, 5.40f, 8.93f};
            const float mat = x[4], pos = 0.05f + x[5] * 0.45f;
            modes = 4;
            for (int i = 0; i < 4; ++i) {
                modalRatio[i] = wood[i] + (metal[i] - wood[i]) * mat;
                modalGain[i] = std::fabs(std::sin(kPi * (i + 1) * pos)) * (i == 0 ? 1.f : 0.6f) + 0.05f;
            }
            modalRing = x[6]; // RING: resonator tube sustains the fundamental
            drv = x[7]; gainNorm = 0.4f;
            break;
        }
        }
        modalBase *= pitchMul;
        // mallet: raised-cosine pulse of unit area (short = hard/bright, long = soft/dark) + a little noise
        excLen = std::max<uint32_t>(2, (uint32_t)(expmap(1.f - hard, 0.0002f, 0.006f) * sr));
        excNoise = hard * 0.12f;
        excCoef = decayCoef(0.002f, sr);
        float gs = 0.f;
        for (int i = 0; i < modes; ++i) gs += modalGain[i];
        gainSumInv = gs > 0.f ? 1.f / gs : 1.f;
        updateModes();
    }

    void updateModes()
    {
        const float pitchMul = 1.f + tens * tensEnv;
        for (int i = 0; i < modes; ++i) {
            float t60 = modalT60 / (1.f + i * modalDamp * 2.f);
            if (machine == mPH_MALLET && i == 0) t60 = modalT60 * (1.f + 2.f * modalRing);
            setMode(i, modalBase * modalRatio[i] * pitchMul, t60, modalGain[i]);
        }
    }

    void controlString(const float* x)
    {
        f1 = expmap(x[0], 40, 1500) * pitchMul;
        ksDelay = clampf(sr / f1, 2.f, 16000.f);
        const float t60 = expmap(x[1], 0.1f, 10.f);
        ksGain = std::pow(10.f, -3.f / (t60 * f1));
        ksExcLP.setLP(expmap(x[2], 300, 16000), sr);
        ksPick = 0.05f + x[3] * 0.45f;
        ksDamp = x[4] * 0.85f;
        ksStif = x[5] * 0.6f;
        ksNoise = x[6];
        drv = x[7]; gainNorm = 1.4f;
    }

    void trigger(float velocity)
    {
        vel = velocity;
        if (fam == fOff) { active = false; return; }
        active = true; t = 0;
        ph1 = ph2 = phm = 0; mPrev = mPrev2 = 0; tremPh = 0;
        amp = 1.f; amp2 = 1.f; penv = 1.f; nAmp = 1.f; clickAmp = 1.f;
        burstIdx = 0; burstAmp = 0; tailAmp = 0; fastAmp = 1.f; bellAmp = 1.f; modEnv = 1.f;
        grainAmp = 0; excAmp = 1.f; tensEnv = 1.f; snareAmp = 1.f;
        for (int i = 0; i < 6; ++i) mph[i] = rng.uni();
        for (int i = 0; i < 3; ++i) bph[i] = 0;
        burstStart[0] = 0;
        for (int i = 1; i < 8; ++i) {
            const float j = 1.f + rng.next() * clapJitter;
            burstStart[i] = burstStart[i - 1] + (uint32_t)(clapSpacing * j);
        }
        if (fam == fString) startString();
        if (fam == fSample) {
            if (!sample || sEnd - sStart < 2.0) { active = false; return; }
            sPos = sRev ? sEnd - 1.0 : sStart; sAmp = 1.f; sHoldCnt = 0;
        }
        if (fam == fModal) updateModes();
    }

    void startString()
    {
        const uint32_t L = (uint32_t)ksDelay;
        ksExcLen = L < 16000 ? L : 16000;
        ksExcLP.y = 0;
        const uint32_t pickD = (uint32_t)(ksPick * L);
        for (uint32_t i = 0; i < ksExcLen; ++i) {
            const float tri = 1.f - std::fabs(2.f * i / (float)ksExcLen - 1.f);
            ksExc[i] = ksNoise * rng.next() + (1.f - ksNoise) * (tri * 2.f - 1.f) * 0.8f;
        }
        for (uint32_t i = ksExcLen; i-- > 0;)
            if (i >= pickD && pickD > 0) ksExc[i] -= ksExc[i - pickD] * 0.9f;
        for (uint32_t i = 0; i < ksExcLen; ++i) ksExc[i] = ksExcLP.lp(ksExc[i]);
    }

    inline float env(float& a, float coef)
    {
        // attack / hold / exponential decay
        if (t < attackS) return (float)t / (float)attackS;
        if (t < attackS + holdS) return 1.f;
        const float v = a; a *= coef; return v;
    }

    // ------------------------------------------------------------------
    void render(float* out, int n)
    {
        if (!active) { std::memset(out, 0, sizeof(float) * n); return; }
        switch (fam) {
        case fTone: case fSnare: renderTone(out, n); break;
        case fClap: renderClap(out, n); break;
        case fMetal: renderMetal(out, n); break;
        case fShaker: case fNoise: renderNoise(out, n); break;
        case fFM: renderFM(out, n); break;
        case fModal: renderModal(out, n); break;
        case fString: renderString(out, n); break;
        case fSample: renderSample(out, n); break;
        default: std::memset(out, 0, sizeof(float) * n); active = false; return;
        }
        const float g = vel * gainNorm;
        for (int i = 0; i < n; ++i) out[i] *= g;
    }

    inline float applyOut(float s)
    {
        switch (outMode) {
        case 1: outFilt.tick(s); s = outFilt.hp; break;
        case 2: outFilt.tick(s); s = outFilt.bp; break;
        case 3: outFilt.tick(s); s = outFilt.lp; break;
        }
        return s;
    }

    inline float noiseSample()
    {
        nFilt.tick(rng.next());
        return nMode == 0 ? nFilt.lp : (nMode == 1 ? nFilt.bp : nFilt.hp);
    }

    inline float clickSample()
    {
        if (clickLvl <= 0.f || clickAmp < 1e-4f) return 0.f;
        clickFilt.tick(rng.next() + (t == 0 ? 6.f : 0.f));
        const float v = clickFilt.bp * clickAmp * clickLvl * 2.f;
        clickAmp *= clickCoef;
        return v;
    }

    void renderSample(float* out, int n)
    {
        const SampleData* s = sample;
        const double frames = s ? (double)s->frames() : 0.0;
        const double inc = s ? s->sr / sr * std::exp2((sTune) / 12.0) * pitchMul : 1.0;
        const int hold = 1 + (int)(sLofi * sLofi * 15.f);
        const float q = sLofi > 0.01f ? std::exp2(12.f - sLofi * 8.f) : 0.f;   // bits: 12 down to 4
        for (int i = 0; i < n; ++i, ++t) {
            if (!s || sPos < sStart || sPos >= sEnd || sPos >= frames - 1.0) {
                std::memset(out + i, 0, sizeof(float) * (n - i)); active = false; return;
            }
            const size_t i0 = (size_t)sPos; const float fr = (float)(sPos - (double)i0);
            float v = s->pcm[i0] + (s->pcm[i0 + 1] - s->pcm[i0]) * fr;
            sPos += sRev ? -inc : inc;
            if (hold > 1 || q > 0.f) {
                if (--sHoldCnt <= 0) { sHoldCnt = hold; sHoldV = q > 0.f ? std::round(v * q) / q : v; }
                v = sHoldV;
            }
            float e = 1.f;
            if (t < (uint32_t)sAtk) e = (float)t / sAtk;
            else { e = sAmp; sAmp *= sDec; if (sAmp < 1e-4f) { std::memset(out + i, 0, sizeof(float) * (n - i)); active = false; return; } }
            out[i] = drive(v * e, drv);
        }
    }

    void renderTone(float* out, int n)
    {
        const float hg = 1.f + harm * 8.f, hn = 1.f / std::tanh(hg);
        for (int i = 0; i < n; ++i, ++t) {
            const float f = f1 * std::exp2(pDepth * penv);
            penv *= pCoef;
            ph1 += f * isr; if (ph1 >= 1.f) ph1 -= 1.f;
            float s;
            if (oscShape == 1 && machine != mSINE) { float q = ph1 + 0.25f; if (q >= 1.f) q -= 1.f; s = 1.f - 4.f * std::fabs(q - 0.5f); }
            else if (oscShape == 2 && machine != mSINE) s = std::tanh(std::sin(kTwoPi * ph1) * 6.f);
            else s = std::sin(kTwoPi * ph1);
            if (harm > 0.f) s = std::tanh(s * hg) * hn;
            if (wave > 0.f) {
                // morph sine -> triangle -> saw -> square
                const float tri = 1.f - 4.f * std::fabs(ph1 - 0.5f);
                const float saw = 2.f * ph1 - 1.f;
                const float sq = ph1 < 0.5f ? 1.f : -1.f;
                const float w = wave * 3.f;
                if (w < 1.f) s = s + (tri - s) * w;
                else if (w < 2.f) s = tri + (saw - tri) * (w - 1.f);
                else s = saw + (sq - saw) * (w - 2.f);
                s *= 0.8f;
            }
            const float e = env(amp, ampCoef);
            float v = s * e;
            if (lvl2 > 0.f) {
                ph2 += f * ratio2 * isr; if (ph2 >= 1.f) ph2 -= 1.f;
                v += std::sin(kTwoPi * ph2) * lvl2 * amp2;
                amp2 *= coef2;
            }
            if (nLvl > 0.f) { v += noiseSample() * nLvl * nAmp; nAmp *= nCoef; }
            v += clickSample();
            out[i] = drive(applyOut(v), drv);
        }
        if (amp < 1e-4f && nAmp < 1e-4f && amp2 < 1e-4f && t > attackS + holdS && ampCoef < 1.f) active = false;
    }

    void renderClap(float* out, int n)
    {
        for (int i = 0; i < n; ++i, ++t) {
            if (burstIdx < bursts && t >= burstStart[burstIdx]) {
                burstAmp = 1.f; ++burstIdx;
                if (burstIdx == bursts) tailAmp = tailLvl;
            }
            const float e = burstAmp + tailAmp;
            burstAmp *= burstCoef; tailAmp *= tailCoef;
            out[i] = drive(applyOut(rng.next() * e), drv);
        }
        if (burstIdx >= bursts && burstAmp < 1e-4f && tailAmp < 1e-4f) active = false;
    }

    void renderMetal(float* out, int n)
    {
        for (int i = 0; i < n; ++i, ++t) {
            float m = 0.f;
            for (int k = 0; k < metalCount; ++k) {
                const float dt = mfreq[k] * isr;
                mph[k] += dt; if (mph[k] >= 1.f) mph[k] -= 1.f;
                m += blepSquare(mph[k], dt);
            }
            float v;
            if (machine == mCOWBELL) {
                v = applyOut(m * 0.5f);
                const float e = (shrt * fastAmp + (1.f - shrt) * amp);
                fastAmp *= fastCoef; amp *= ampCoef;
                v = v * e + clickSample();
            } else {
                m *= (1.f / 6.f);
                const float s = metl * m + (1.f - metl) * rng.next();
                outFilt.tick(s); outFilt2.tick(outFilt.hp);
                v = outFilt.hp * 0.6f + outFilt2.bp * 0.6f;
                v *= env(amp, ampCoef);
                if (bellLvl > 0.f) {
                    static const float br[3] = {1.f, 1.38f, 2.7f};
                    float b = 0.f;
                    for (int k = 0; k < 3; ++k) {
                        bph[k] += f1 * br[k] * isr; if (bph[k] >= 1.f) bph[k] -= 1.f;
                        b += std::sin(kTwoPi * bph[k]) / (k + 1);
                    }
                    v += b * bellLvl * bellAmp * 0.5f; bellAmp *= bellCoef;
                }
            }
            out[i] = drive(v, drv);
        }
        if (amp < 1e-4f && (machine != mCOWBELL || fastAmp < 1e-4f) && bellAmp * bellLvl < 1e-4f && t > attackS) active = false;
    }

    void renderNoise(float* out, int n)
    {
        const bool shaker = (machine == mSHAKER);
        const float grainProb = grainRate * isr;
        const int crushHoldN = crush > 0.f ? 1 + (int)(crush * crush * 40.f) : 1;
        const float bits = crush > 0.f ? std::exp2(12.f - crush * 10.f) : 0.f;
        for (int i = 0; i < n; ++i, ++t) {
            float v = rng.next();
            if (outMode == 1) { outFilt.tick(v); v = outFilt.hp; }
            outFilt2.tick(v); v = outFilt2.lp;
            if (shaker) {
                if (rng.uni() < grainProb) grainAmp = 0.5f + 0.5f * rng.uni();
                v *= (1.f - grain) + grain * grainAmp * 1.6f;
                grainAmp *= grainCoef;
            }
            if (bits > 0.f) {
                if (--crushCnt <= 0) { crushCnt = crushHoldN; crushHold = std::round(v * bits) / bits; }
                v = crushHold;
            }
            out[i] = drive(v * env(amp, ampCoef), drv);
        }
        if (amp < 1e-4f && t > attackS + holdS && ampCoef < 1.f) active = false;
    }

    void renderFM(float* out, int n)
    {
        for (int i = 0; i < n; ++i, ++t) {
            const float f = f1 * std::exp2(pDepth * penv);
            penv *= pCoef;
            phm += f * ratio2 * isr; if (phm >= 1.f) phm -= std::floor(phm);
            const float m = std::sin(kTwoPi * phm + fb * (mPrev + mPrev2) * 0.5f * kPi);
            mPrev2 = mPrev; mPrev = m;
            ph1 += f * isr; if (ph1 >= 1.f) ph1 -= 1.f;
            const float c = std::sin(kTwoPi * ph1 + modIdx * modEnv * m);
            modEnv *= modCoef;
            float e;
            if (fmClap) {
                if (burstIdx < bursts && t >= burstStart[burstIdx]) {
                    burstAmp = 1.f; ++burstIdx;
                    if (burstIdx == bursts) tailAmp = tailLvl;
                }
                e = burstAmp + tailAmp; burstAmp *= burstCoef; tailAmp *= tailCoef;
            } else {
                e = amp; amp *= ampCoef;
            }
            float v = c * e;
            if (trem > 0.f) {
                tremPh += tremF * isr; if (tremPh >= 1.f) tremPh -= 1.f;
                v *= 1.f - trem * (0.5f - 0.5f * std::sin(kTwoPi * tremPh));
            }
            if (bellLvl > 0.f) {
                ph2 += f * 0.37f * isr; if (ph2 >= 1.f) ph2 -= 1.f;
                v += std::sin(kTwoPi * ph2 + 2.f * modEnv * m) * bellLvl * bellAmp; bellAmp *= bellCoef;
            }
            if (nLvl > 0.f) { v += noiseSample() * nLvl * nAmp; nAmp *= nCoef; }
            v += clickSample();
            out[i] = applyOut(v);
        }
        if (fmClap) { if (burstIdx >= bursts && burstAmp < 1e-4f && tailAmp < 1e-4f) active = false; }
        else if (amp < 1e-4f && nAmp * nLvl < 1e-4f && bellAmp * bellLvl < 1e-4f) active = false;
    }

    void renderModal(float* out, int n)
    {
        if (tens > 0.f) { tensEnv *= std::pow(decayCoef(0.08f, sr), (float)n); updateModes(); }
        float pk = 0.f;
        for (int i = 0; i < n; ++i, ++t) {
            float x = 0.f;
            if (t < excLen) x = (1.f - std::cos(kTwoPi * (t + 0.5f) / excLen)) / excLen;
            if (excAmp > 1e-4f) { x += rng.next() * excNoise * excAmp; excAmp *= excCoef; }
            float y = 0.f;
            for (int k = 0; k < modes; ++k) {
                const float r = rb0[k] * x - ra1[k] * rz1[k] - ra2[k] * rz2[k];
                rz2[k] = rz1[k]; rz1[k] = r;
                y += r * rg[k];
            }
            y *= gainSumInv * 6.f;
            if (nLvl > 0.f) { y += noiseSample() * nLvl * nAmp; nAmp *= nCoef; }
            if (snareLvl > 0.f) { y += noiseSample() * snareLvl * snareAmp * (0.3f + std::fabs(y)); snareAmp *= snareCoef; }
            y = drive(y, drv);
            out[i] = y;
            const float a = std::fabs(y); if (a > pk) pk = a;
        }
        if (t > excLen && excAmp < 1e-4f && pk < 1e-5f) active = false;
    }

    void renderString(float* out, int n)
    {
        const uint32_t mask = (uint32_t)ks.size() - 1;
        float pk = 0.f;
        for (int i = 0; i < n; ++i, ++t) {
            float rp = (float)ksW - ksDelay;
            if (rp < 0.f) rp += (float)ks.size();
            const float rpf = std::floor(rp);
            const float fr = rp - rpf;
            const uint32_t i0 = (uint32_t)((int32_t)rpf) & mask;
            const uint32_t i1 = (i0 + 1) & mask;
            const float y = ks[i0] + (ks[i1] - ks[i0]) * fr;
            // loop filter: lowpass damping + dispersion allpass
            ksLp = y + ksDamp * (ksLp - y);
            float z = ksLp;
            if (ksStif > 0.f) {
                const float ap = -ksStif * z + ksApZ;
                ksApZ = z + ksStif * ap;
                z = ap;
            }
            float in = z * ksGain;
            if (t < ksExcLen) in += ksExc[t];
            ks[ksW] = in;
            ksW = (ksW + 1) & mask;
            const float o = drive(y * 0.9f, drv);
            out[i] = o;
            const float a = std::fabs(o); if (a > pk) pk = a;
        }
        if (t > ksExcLen && pk < 1e-5f) active = false;
    }
};

} // namespace kn
