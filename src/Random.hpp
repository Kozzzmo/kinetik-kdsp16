// Kinetik KDSP-16 — randomizers (sound / sequence / pattern / everything)
#pragma once
#include "Bank.hpp"
#include <cmath>

namespace kn {

struct RandGen {
    uint32_t s;
    explicit RandGen(uint32_t seed) : s(seed ? seed : 0x1234567u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (next() & 0xFFFFFF) / 16777216.f; }
    int range(int lo, int hi) { return lo + (int)(uni() * (hi - lo + 1)); } // inclusive
    bool chance(float p) { return uni() < p; }
};

// rough role of a machine, used to make random sequences musical
enum Role { roKick, roSnare, roHat, roPerc, roLong };
inline Role machineRole(int m)
{
    switch (m) {
    case mKICK: case mFM_KICK: case mPH_KICK: return roKick;
    case mSNARE: case mFM_SNARE: case mPH_SNARE: case mCLAP: case mFM_CLAP: return roSnare;
    case mCLHAT: case mFM_HAT: case mSHAKER: return roHat;
    case mOPHAT: case mCYMBAL: case mFM_CYMBAL: case mPH_METAL: case mNOISE: return roLong;
    default: return roPerc;
    }
}

// values: full parameter array (kTotalParams) as the UI holds it
inline float& tparam(float* values, int t, int p) { return values[t * kTrackParams + p]; }

// ---- sound of one track (machine kept). LEVEL, PAN and the DELAY / REVERB sends are left as they are:
// randomizing a sound must not change the mix.
inline void randomizeSound(float* values, int t, int m, RandGen& r)
{
    const Role role = machineRole(m);
    for (int p = 0; p < 8; ++p) {
        const int def = kMachineDefs[m].def[p];
        // stay in a musical neighbourhood of the machine defaults most of the time
        int v = r.chance(0.35f) ? r.range(5, 122) : clampi(def + r.range(-40, 40), 0, 127);
        tparam(values, t, p) = (float)v;
    }
    if (m == mSAMPLE) {   // stay recognisable: near the original pitch, from near the start, mostly forwards
        tparam(values, t, 0) = (float)clampi(64 + r.range(-12, 12), 0, 127);
        tparam(values, t, 1) = r.chance(0.7f) ? 0.f : (float)r.range(0, 40);
        tparam(values, t, 2) = (float)r.range(60, 127);
        tparam(values, t, 3) = r.chance(0.8f) ? 0.f : (float)r.range(0, 40);
        tparam(values, t, 4) = r.chance(0.5f) ? 127.f : (float)r.range(50, 126);
        tparam(values, t, 5) = r.chance(0.15f) ? 127.f : 0.f;
        tparam(values, t, 6) = r.chance(0.2f) ? (float)r.range(10, 80) : 0.f;
        tparam(values, t, 7) = r.chance(0.3f) ? (float)r.range(0, 50) : 0.f;
    }
    // decay: keep kicks/snares/hats from becoming endless drones
    if (role == roKick || role == roSnare) tparam(values, t, 1) = (float)r.range(25, 85);
    if (role == roHat) tparam(values, t, 1) = (float)r.range(5, 60);
    // effects page: occasional, tasteful
    tparam(values, t, tpAMD) = r.chance(0.2f) ? (float)r.range(10, 70) : 0.f;
    tparam(values, t, tpAMF) = (float)r.range(20, 110);
    tparam(values, t, tpEQF) = (float)r.range(20, 110);
    tparam(values, t, tpEQG) = (float)clampi(64 + r.range(-20, 20), 0, 127);
    {
        const int ft = r.chance(0.7f) ? 0 : r.range(1, 4);
        tparam(values, t, tpFTYPE) = (float)filterTypeValue(ft);
        tparam(values, t, tpCUTOFF) = (float)(ft == 3 ? r.range(0, 60) : (ft == 0 ? r.range(70, 127) : r.range(40, 110)));
        tparam(values, t, tpRESO) = (float)r.range(0, 60);
    }
    tparam(values, t, tpSRR) = r.chance(0.15f) ? (float)r.range(10, 70) : 0.f;
    tparam(values, t, tpDIST) = r.chance(0.3f) ? (float)r.range(5, 60) : 0.f;
    // filter envelope (sometimes), velocity sensitivity
    tparam(values, t, tpFENV) = r.chance(0.25f) ? (float)clampi(64 + r.range(-40, 50), 0, 127) : 64.f;
    tparam(values, t, tpFDEC) = (float)r.range(15, 80);
    tparam(values, t, tpVEL) = 127.f;
    // lfos (LFO 1 sometimes, LFO 2 rarely; always on this track)
    for (int l = 0; l < 2; ++l) {
        const int b = kLfoBase[l];
        if (r.chance(l == 0 ? 0.3f : 0.1f)) {
            tparam(values, t, b) = (float)r.range(10, 110);
            tparam(values, t, b + 1) = (float)r.range(10, 70);
            tparam(values, t, b + 2) = (float)r.range(0, 127);
            tparam(values, t, b + 3) = (float)r.range(0, 127);
            tparam(values, t, b + 4) = (float)r.range(0, 127);
        } else {
            tparam(values, t, b + 1) = 0.f;
        }
        tparam(values, t, b + 5) = 0.f;
        tparam(values, t, lfoParam(l, 6)) = r.chance(0.4f) ? 127.f : 0.f;   // sometimes locked to the tempo
        tparam(values, t, lfoParam(l, 7)) = 0.f;
    }
}

// ---- sequence of one track ----
inline void randomizeSequence(Pattern& pat, int t, int machine, RandGen& r, float density = -1.f)
{
    pat.clearTrack(t);
    if (machine == mOFF) return;
    const Role role = machineRole(machine);
    const int len = pat.trackLen[t];
    float base;
    switch (role) {
    case roKick: base = 0.18f; break;
    case roSnare: base = 0.08f; break;
    case roHat: base = 0.45f; break;
    case roLong: base = 0.08f; break;
    default: base = 0.14f; break;
    }
    if (density >= 0.f) base *= density;
    for (int s = 0; s < len; ++s) {
        const int pos = s % 16;
        float p = base;
        if (role == roKick && pos % 4 == 0) p = 0.75f;
        if (role == roSnare && (pos == 4 || pos == 12)) p = 0.85f;
        if (role == roHat && pos % 2 == 0) p = std::min(0.9f, base * 1.8f);
        if (role == roLong && (pos == 2 || pos == 10)) p = 0.3f;
        if (!r.chance(p)) continue;
        uint8_t f = sfTrig;
        if (r.chance(0.15f)) f |= sfAccent;
        if (r.chance(0.04f)) f |= sfSlide;
        pat.flags[t][s] = f;
        // a few ratchets (mostly hats), conditional trigs and micro-timing
        if (r.chance(role == roHat ? 0.12f : 0.04f)) pat.ratch[t][s] = (uint8_t)kRatchValues[r.range(1, 3)];
        if (role != roKick && r.chance(0.1f)) pat.cond[t][s] = (uint8_t)r.range(1, 6);
        if (r.chance(0.08f)) pat.micro[t][s] = (int8_t)r.range(-4, 4);
        // a few parameter locks for movement
        if (r.chance(0.18f)) pat.locks[t][s][r.chance(0.6f) ? 0 : 1] = (uint8_t)r.range(15, 115);
        if (r.chance(0.06f)) pat.locks[t][s][tpCUTOFF] = (uint8_t)r.range(30, 100);
    }
}

inline void randomizePattern(Pattern& pat, const Bank& kit, RandGen& r)
{
    for (int t = 0; t < kTracks; ++t) randomizeSequence(pat, t, kit.machine[t], r);
    pat.swing = (uint8_t)(r.chance(0.5f) ? 50 : r.range(52, 64));
}

// ---- everything: kit, master effects, current pattern ----
inline void randomizeKit(float* values, Bank& kit, RandGen& r)
{
    static const int kicks[] = {mKICK, mFM_KICK, mPH_KICK};
    static const int snares[] = {mSNARE, mFM_SNARE, mPH_SNARE, mCLAP, mFM_CLAP};
    static const int hats[] = {mCLHAT, mFM_HAT, mSHAKER};
    static const int any[] = {mTOM, mRIM, mCOWBELL, mOPHAT, mCYMBAL, mCLAVE, mFM_TOM, mFM_RIM, mFM_BELL,
                              mFM_CYMBAL, mPH_METAL, mPH_MALLET, mPH_STRING, mSINE, mNOISE, mCLAP, mFM_CLAP, mSHAKER};
    for (int t = 0; t < kTracks; ++t) {
        if (kit.machine[t] == mSAMPLE) { randomizeSound(values, t, mSAMPLE, r); kit.mute[t] = 0; continue; }   // keep the user's samples
        int m;
        if (t == 0 || t == 10) m = kicks[r.range(0, 2)];
        else if (t == 1 || t == 4) m = snares[r.range(0, 4)];
        else if (t == 7 || t == 12) m = hats[r.range(0, 2)];
        else m = any[r.range(0, (int)(sizeof(any) / sizeof(any[0])) - 1)];
        kit.machine[t] = (uint8_t)m;
        for (int p = 0; p < 8; ++p) tparam(values, t, p) = kMachineDefs[m].def[p];
        randomizeSound(values, t, m, r);
        kit.mute[t] = 0;
    }
    float* g = values + kGlobalBase;
    g[gECHO_TIME] = (float)r.range(30, 100); g[gECHO_FB] = (float)r.range(20, 85);
    g[gECHO_LPF] = (float)r.range(60, 120); g[gECHO_HPF] = (float)r.range(0, 50);
    g[gECHO_WIDTH] = (float)r.range(20, 127); g[gECHO_LEV] = (float)r.range(40, 90);
    g[gREV_SIZE] = (float)r.range(20, 100); g[gREV_DAMP] = (float)r.range(20, 110);
    g[gREV_GATE] = r.chance(0.2f) ? (float)r.range(20, 90) : 0.f; g[gREV_HPF] = (float)r.range(0, 60);
    g[gREV_PRE] = (float)r.range(0, 50); g[gREV_LEV] = (float)r.range(40, 90);
}

} // namespace kn
