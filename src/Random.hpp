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
    case mTRX_BD: case mEFM_BD: case mPI_BD: return roKick;
    case mTRX_SD: case mEFM_SD: case mPI_SD: case mTRX_CP: case mEFM_CP: return roSnare;
    case mTRX_CH: case mEFM_HH: case mTRX_MA: return roHat;
    case mTRX_OH: case mTRX_CY: case mEFM_CY: case mPI_MT: case mGND_NS: return roLong;
    default: return roPerc;
    }
}

// values: full parameter array (kTotalParams) as the UI holds it
inline float& tparam(float* values, int t, int p) { return values[t * kTrackParams + p]; }

// ---- sound of one track (machine kept) ----
inline void randomizeSound(float* values, int t, int m, RandGen& r)
{
    const Role role = machineRole(m);
    for (int p = 0; p < 8; ++p) {
        const int def = kMachineDefs[m].def[p];
        // stay in a musical neighbourhood of the machine defaults most of the time
        int v = r.chance(0.35f) ? r.range(5, 122) : clampi(def + r.range(-40, 40), 0, 127);
        tparam(values, t, p) = (float)v;
    }
    // decay: keep kicks/snares/hats from becoming endless drones
    if (role == roKick || role == roSnare) tparam(values, t, 1) = (float)r.range(25, 85);
    if (role == roHat) tparam(values, t, 1) = (float)r.range(5, 60);
    // effects page: occasional, tasteful
    tparam(values, t, tpAMD) = r.chance(0.2f) ? (float)r.range(10, 70) : 0.f;
    tparam(values, t, tpAMF) = (float)r.range(20, 110);
    tparam(values, t, tpEQF) = (float)r.range(20, 110);
    tparam(values, t, tpEQG) = (float)clampi(64 + r.range(-20, 20), 0, 127);
    tparam(values, t, tpFLTF) = r.chance(0.3f) ? (float)r.range(0, 50) : 0.f;
    tparam(values, t, tpFLTW) = (float)r.range(70, 127);
    tparam(values, t, tpFLTQ) = (float)r.range(0, 60);
    tparam(values, t, tpSRR) = r.chance(0.15f) ? (float)r.range(10, 70) : 0.f;
    tparam(values, t, tpDIST) = r.chance(0.3f) ? (float)r.range(5, 60) : 0.f;
    tparam(values, t, tpVOL) = (float)r.range(85, 112);
    tparam(values, t, tpPAN) = (float)clampi(64 + (r.chance(0.5f) ? r.range(-40, 40) : 0), 0, 127);
    tparam(values, t, tpDEL) = r.chance(0.25f) ? (float)r.range(10, 70) : 0.f;
    tparam(values, t, tpREV) = r.chance(0.3f) ? (float)r.range(10, 70) : 0.f;
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
        if (r.chance(0.06f)) pat.locks[t][s][tpFLTW] = (uint8_t)r.range(30, 100);
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
    static const int kicks[] = {mTRX_BD, mEFM_BD, mPI_BD};
    static const int snares[] = {mTRX_SD, mEFM_SD, mPI_SD, mTRX_CP, mEFM_CP};
    static const int hats[] = {mTRX_CH, mEFM_HH, mTRX_MA};
    static const int any[] = {mTRX_XT, mTRX_RS, mTRX_CB, mTRX_OH, mTRX_CY, mTRX_CL, mEFM_XT, mEFM_RS, mEFM_CB,
                              mEFM_CY, mPI_MT, mPI_ML, mPI_KS, mGND_SN, mGND_NS, mTRX_CP, mEFM_CP, mTRX_MA};
    for (int t = 0; t < kTracks; ++t) {
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
