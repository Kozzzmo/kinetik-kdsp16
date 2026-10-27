// Kinetik KDSP-16 — factory presets (kit + patterns). All original material.
#pragma once
#include "Random.hpp"
#include "FxPresets.hpp"
#include <cstring>
#include <algorithm>
#include <initializer_list>

namespace kn {

// Convert the parameters of one track saved before 0.7 (36 params per track, filter base/width/Q at 12..14).
inline void migrateFilterV1(float* trackParams)
{
    const float base = trackParams[12], width = trackParams[13];
    const float top = std::min(127.f, base + width);
    int type = 0; float cut = top;
    if (base > 5.f && top >= 122.f) { type = 3; cut = base; }          // only the high-pass was doing something
    else if (base > 5.f) { type = 2; cut = (base + top) * 0.5f; }      // band
    trackParams[tpCUTOFF] = cut; trackParams[tpFTYPE] = (float)filterTypeValue(type);
}
inline int presetIndexV1(int idx)
{
    constexpr int oldTP = 36, oldGB = kTracks * oldTP;
    if (idx < oldGB) return (idx / oldTP) * kTrackParams + idx % oldTP;
    return kGlobalBase + (idx - oldGB);
}

struct PB { // preset builder
    float* v; Bank& b;
    PB(float* values, Bank& bank) : v(values), b(bank)
    {
        for (int t = 0; t < kTracks; ++t) {
            for (int p = 0; p < kTrackParams; ++p) tparam(v, t, p) = p >= 8 ? kFxDefaults[p - 8] : 0.f;
            b.machine[t] = mOFF; b.mute[t] = 0;
        }
        for (int g = gECHO_TIME; g < gOUT_STEP; ++g) v[kGlobalBase + g] = globalDefault(g);
        for (auto& p : b.patterns) p.clear();
    }
    // machine + synth params (-1 keeps the machine default)
    void kit(int t, int m, std::initializer_list<int> syn = {})
    {
        b.machine[t] = (uint8_t)m;
        for (int p = 0; p < 8; ++p) tparam(v, t, p) = kMachineDefs[m].def[p];
        int p = 0;
        for (int x : syn) { if (x >= 0) tparam(v, t, p) = (float)x; if (++p >= 8) break; }
    }
    void fx(int t, int p, int val) { tparam(v, t, p) = (float)val; }
    void g(int gp, int val) { v[kGlobalBase + gp] = (float)val; }
    // "x" trig, "X" accent, "s" slide, "S" accent+slide, anything else = rest. Spaces ignored.
    void row(int pat, int t, const char* s)
    {
        int st = 0;
        for (const char* c = s; *c && st < kSteps; ++c) {
            if (*c == ' ' || *c == '|') continue;
            uint8_t f = 0;
            if (*c == 'x') f = sfTrig; else if (*c == 'X') f = sfTrig | sfAccent;
            else if (*c == 's') f = sfTrig | sfSlide; else if (*c == 'S') f = sfTrig | sfAccent | sfSlide;
            b.patterns[pat].flags[t][st++] = f;
        }
        b.patterns[pat].trackLen[t] = (uint8_t)std::max(st, 1);
    }
    void lock(int pat, int t, int step, int p, int val) { b.patterns[pat].locks[t][step][p] = (uint8_t)clampi(val, 0, 127); }
    // lock a melodic pitch on every trig of a track from a list of semitone offsets
    void melody(int pat, int t, int basePitch, float unitsPerSemi, std::initializer_list<int> semis)
    {
        const int n = (int)semis.size(); int i = 0;
        const int* arr = semis.begin();
        for (int s = 0; s < kSteps; ++s)
            if (b.patterns[pat].flags[t][s] & sfTrig) { lock(pat, t, s, 0, basePitch + (int)std::lround(arr[i % n] * unitsPerSemi)); ++i; }
    }
    void choke(int t, int group) { b.kit[t][kfChoke] = (uint8_t)group; }
    void ratch(int pat, int t, int step, int n) { b.patterns[pat].ratch[t][step] = (uint8_t)n; }
    void cond(int pat, int t, int step, int c) { b.patterns[pat].cond[t][step] = (uint8_t)c; }
    void micro(int pat, int t, int step, int m) { b.patterns[pat].micro[t][step] = (int8_t)m; }
    static int tpi(int t, int p) { return t * kTrackParams + p; }
    static int gpi(int g) { return kGlobalBase + g; }
    void speed(int t, int sp) { b.kit[t][kfSpeed] = (uint8_t)sp; }   // index into kTrackSpeedMult
    void settings(int pat, int len, int swing, int scale = 0, int accent = 64)
    {
        b.patterns[pat].length = (uint8_t)len; b.patterns[pat].swing = (uint8_t)swing;
        for (int t = 0; t < kTracks; ++t) {
            bool any = false;
            for (int st = 0; st < kSteps; ++st) any |= b.patterns[pat].flags[t][st] != 0;
            if (!any) b.patterns[pat].trackLen[t] = (uint8_t)len;
        }
        b.patterns[pat].scale = (uint8_t)scale; b.patterns[pat].accent = (uint8_t)accent;
    }
};

static constexpr float kMlSemi = 2.447f;  // PHYS MALLET pitch units per semitone
static constexpr float kKsSemi = 2.022f;  // PHYS STRING
static constexpr float kMtSemi = 3.08f;   // PHYS METAL
static constexpr float kSnSemi = 2.13f;   // SINE

// --------------------------------------------------------------------------------------------
static void presetInit(PB& b)
{
    b.b.initDefault();
    for (int t = 0; t < kTracks; ++t) {
        b.kit(t, kDefaultKit[t]);
        if (kDefaultKitPitch[t] != 255) b.fx(t, 0, kDefaultKitPitch[t]);
    }
    b.g(gTEMPO, 120);
}

static void presetDefault(PB& b)
{
    b.b.initDemo();
    for (int t = 0; t < kTracks; ++t) {
        b.kit(t, kDefaultKit[t]);
        if (kDefaultKitPitch[t] != 255) b.fx(t, 0, kDefaultKitPitch[t]);
    }
    b.g(gTEMPO, 120);
}

static void presetElectro(PB& b)
{
    b.kit(0, mKICK, {46, 70, 60, 35, 70, 0, 20, 30});
    b.kit(1, mSNARE, {58, 35, 30, 20, 90, 70, 40, 45});
    b.kit(2, mCLAP, {80, 45, 55, 55, 30, 70, 30, 15});
    b.kit(3, mCLHAT, {70, 20, 85, 90, 30, 64, 0, 0});
    b.kit(4, mOPHAT, {70, 55, 85, 90, 30, 64, 0, 0});
    b.kit(5, mCOWBELL, {62, 45, 64, 70, 40, 70, 30, 20});
    b.kit(6, mTOM, {40, 60, 50, 40, 40, 5, 0, 10});
    b.kit(7, mRIM, {66, 18, 50, 80, 30, 20, 50, 20});
    b.kit(8, mSINE, {30, 50, 30, 40, 70, 0, 0, 40}); b.fx(8, tpCUTOFF, 60); b.fx(8, tpRESO, 50);
    b.fx(2, tpREV, 35); b.fx(5, tpDEL, 30); b.fx(7, tpDEL, 25);
    b.choke(3, 1); b.choke(4, 1); // open hat cut by the closed hat
    b.g(gECHO_TIME, 64); b.g(gECHO_FB, 45); b.g(gREV_SIZE, 40);
    b.row(0, 0, "x.....x...x..x..");
    b.row(0, 1, "....X.......X...");
    b.row(0, 2, "....x.......x..x");
    b.row(0, 3, "x.x.x.x.x.x.x.x.");
    b.row(0, 4, "..............x.");
    b.row(0, 5, "...x.....x......");
    b.row(0, 6, "..........x...x.");
    b.lock(0, 6, 14, 0, 60);
    b.row(0, 8, "x.....x...x..x..");
    b.melody(0, 8, 30, kSnSemi, {0, 0, 3, -2});
    b.settings(0, 16, 54);
    b.row(1, 0, "x.....x...x...x.");
    b.row(1, 1, "....X.......X.xX");
    b.row(1, 3, "xxx.x.xxx.x.x.xx");
    b.row(1, 5, "...x.....x...x..");
    b.row(1, 7, ".x....x..x....x.");
    b.row(1, 8, "x.....x...x...x.");
    b.melody(1, 8, 30, kSnSemi, {0, 0, 5, 3});
    b.lock(1, 1, 14, 0, 70); b.lock(1, 1, 15, 0, 80);
    b.settings(1, 16, 54);
    b.g(gTEMPO, 122);
}

static void presetTechno(PB& b)
{
    b.kit(0, mKICK, {50, 75, 40, 30, 50, 5, 10, 45}); b.fx(0, tpDIST, 15);
    b.kit(1, mCLAP, {64, 55, 45, 50, 25, 90, 20, 10}); b.fx(1, tpREV, 45);
    b.kit(2, mCLHAT, {75, 18, 95, 95, 30, 60, 0, 0});
    b.kit(3, mOPHAT, {75, 50, 90, 90, 30, 60, 0, 0});
    b.kit(4, mRIM, {75, 15, 30, 90, 20, 15, 60, 10}); b.fx(4, tpDEL, 45);
    b.kit(5, mPH_MALLET, {70, 45, 75, 40, 85, 50, 20, 0}); b.fx(5, tpDEL, 50); b.fx(5, tpREV, 40);
    b.kit(6, mCYMBAL, {64, 70, 70, 70, 30, 70, 20, 0});
    b.kit(7, mFM_HAT, {90, 20, 0, 64, 90, 100, 60, 100});
    b.g(gECHO_TIME, 64); b.g(gECHO_FB, 60); b.g(gREV_SIZE, 70);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "..x...x...x...x.");
    b.row(0, 3, "..............x.");
    b.row(0, 4, "...x..x.....x...");
    b.row(0, 5, "x..x..x...x..x..");
    b.melody(0, 5, 60, kMlSemi, {0, 3, 7, 3, 10, 7});
    b.row(0, 7, "xxxxxxxxxxxxxxxx");
    for (int s = 0; s < 16; ++s) b.lock(0, 7, s, 1, s % 4 == 2 ? 45 : 18);
    b.settings(0, 16, 52);
    b.row(1, 0, "x...x...x...x...");
    b.row(1, 1, "....x.......x...");
    b.row(1, 2, "..x...x...x...x.");
    b.row(1, 3, "..x.......x.....");
    b.row(1, 4, ".x.x..x..x..x.x.");
    b.row(1, 5, "x.....x...x.....");
    b.melody(1, 5, 60, kMlSemi, {0, 5, 3});
    b.row(1, 6, "x...............");
    b.row(1, 7, "x.xxx.xxx.xxx.xx");
    b.settings(1, 16, 52);
    b.g(gTEMPO, 128);
}

static void presetDubTechno(PB& b)
{
    b.kit(0, mKICK, {48, 65, 30, 30, 30, 0, 0, 10}); b.fx(0, tpCUTOFF, 90);
    b.kit(1, mRIM, {55, 25, 70, 50, 20, 20, 40, 0}); b.fx(1, tpDEL, 80); b.fx(1, tpREV, 40);
    b.kit(2, mPH_METAL, {60, 55, 40, 30, 60, 60, 40, 0}); b.fx(2, tpDEL, 90); b.fx(2, tpREV, 70);
    b.fx(2, tpCUTOFF, 55); b.fx(2, tpFTYPE, filterTypeValue(2)); b.fx(2, tpRESO, 30);
    b.fx(2, tpLFOS, 15); b.fx(2, tpLFOD, 40); b.fx(2, tpLFOSH, 25); b.fx(2, tpLFODST, lfoDestValue(tpCUTOFF));
    b.kit(3, mCLHAT, {60, 25, 70, 60, 20, 64, 30, 0}); b.fx(3, tpVOL, 80);
    b.kit(4, mNOISE, {60, 0, 127, 70, 100, 0, 30, 0}); b.fx(4, tpVOL, 45);
    b.kit(5, mSNARE, {50, 30, 20, 20, 60, 50, 40, 60}); b.fx(5, tpREV, 70);
    b.g(gECHO_TIME, 76); b.g(gECHO_FB, 85); b.g(gECHO_LPF, 70); b.g(gECHO_HPF, 40); b.g(gECHO_WIDTH, 110);
    b.g(gREV_SIZE, 95); b.g(gREV_DAMP, 90); b.g(gREV_LEV, 80);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "......x.......x.");
    b.row(0, 2, "...x......x.....");
    b.lock(0, 2, 10, 0, 64);
    b.row(0, 3, "..x...x...x...x.");
    b.row(0, 4, "x...............");
    b.row(0, 5, "........x.......");
    b.settings(0, 16, 56);
    b.row(1, 0, "x...x...x...x...");
    b.row(1, 1, "...x..x....x..x.");
    b.row(1, 2, ".x.....x..x.....");
    b.lock(1, 2, 7, 0, 67); b.lock(1, 2, 10, 0, 56);
    b.row(1, 3, "..x...x...x...xx");
    b.row(1, 4, "x...............");
    b.settings(1, 16, 56);
    b.g(gTEMPO, 118);
}

static void presetBreakbeat(PB& b)
{
    b.kit(0, mKICK, {55, 55, 45, 25, 80, 5, 30, 50}); b.fx(0, tpSRR, 10);
    b.kit(1, mSNARE, {62, 38, 40, 15, 100, 60, 60, 40}); b.fx(1, tpDIST, 20);
    b.kit(2, mCLHAT, {70, 22, 75, 70, 30, 64, 0, 20});
    b.kit(3, mCYMBAL, {60, 85, 50, 60, 20, 70, 40, 10});
    b.kit(4, mRIM, {60, 20, 40, 70, 30, 20, 40, 20});
    b.kit(5, mFM_SNARE, {80, 25, 90, 25, 30, 50, 20, 80}); b.fx(5, tpVOL, 80);
    b.g(gCMP_THR, 90); b.g(gCMP_RAT, 50); b.g(gDRIVE, 25);
    b.row(0, 0, "X.x.......xx....");
    b.row(0, 1, "....X..x.x..X..x");
    b.lock(0, 1, 7, 2, 10); b.lock(0, 1, 9, 2, 10); b.lock(0, 1, 15, 2, 10);
    b.row(0, 2, "x.x.x.x.x.x.x.x.");
    b.row(0, 3, "x...............");
    b.row(0, 5, "......x.......x.");
    b.cond(0, 1, 15, 3); b.cond(0, 5, 14, 13);
    b.row(0, 4, "...............x"); b.cond(0, 4, 15, 7); // rimshot only when FILL is on
    b.micro(0, 1, 7, 3); b.micro(0, 1, 9, -2);
    b.settings(0, 16, 58);
    b.row(1, 0, "X.x.......x..x..");
    b.row(1, 1, "....X..x.x..Xx.x");
    b.row(1, 2, "x.x.x.x.x.xxx.x.");
    b.row(1, 4, ".......x......x.");
    b.row(1, 5, "..x.........x...");
    b.settings(1, 16, 58);
    b.g(gTEMPO, 136);
}

static void presetDrillBass(PB& b)
{
    b.kit(0, mFM_KICK, {50, 50, 70, 25, 30, 32, 20, 10});
    b.kit(1, mSNARE, {70, 30, 50, 15, 100, 75, 60, 35});
    b.kit(2, mFM_SNARE, {85, 20, 80, 20, 50, 60, 20, 90});
    b.kit(3, mFM_HAT, {95, 15, 0, 64, 90, 110, 40, 100});
    b.kit(4, mPH_STRING, {30, 60, 60, 40, 50, 0, 40, 30}); b.fx(4, tpCUTOFF, 80); b.fx(4, tpRESO, 40);
    b.kit(5, mCYMBAL, {64, 70, 60, 70, 30, 64, 30, 0});
    b.kit(6, mPH_SNARE, {80, 30, 80, 50, 70, 40, 50, 20});
    b.g(gCMP_THR, 85); b.g(gCMP_RAT, 60);
    b.row(0, 0, "x.......x.x..... ..x.....x......x");
    b.row(0, 1, "....X.......X... ....X..x....Xxxx");
    for (int s = 28; s < 32; ++s) b.lock(0, 1, s, 0, 50 + (s - 28) * 15);
    b.ratch(0, 1, 30, 2); b.ratch(0, 1, 31, 3);
    b.ratch(0, 3, 14, 2); b.ratch(0, 3, 30, 3); b.ratch(0, 3, 31, 4);
    b.row(0, 2, "......xx...x..xx ......x.x.xx..x.");
    for (int s = 0; s < 32; ++s) if (b.b.patterns[0].flags[2][s]) b.lock(0, 2, s, 0, 60 + (s * 7) % 50);
    b.row(0, 3, "x.xxx.xxx.xxx.x. x.xxx.xxx.xxxxxx");
    b.row(0, 4, "x.....x...x..... x.....x...x..x..");
    b.melody(0, 4, 30, kKsSemi, {0, 0, 3, 0, 7, 5, 3});
    b.row(0, 5, "x............... ................");
    b.row(0, 6, "...........x.... ...x.......x....");
    b.settings(0, 32, 50);
    b.row(1, 0, "x..x....x.x..x.. x.......x..x....");
    b.row(1, 1, "....X..x.xX.X..x ..X.X..xX.xXxXxX");
    for (int s = 16; s < 32; ++s) if (b.b.patterns[1].flags[1][s]) b.lock(1, 1, s, 0, 40 + (s - 16) * 5);
    b.row(1, 3, "xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx");
    for (int s = 0; s < 32; ++s) b.lock(1, 3, s, 1, 8 + (s * 5) % 40);
    b.row(1, 4, "x.....x...x...x. x.....x...x.x.x.");
    b.melody(1, 4, 30, kKsSemi, {0, 12, 3, 10, 0, 7, 5, 15});
    b.settings(1, 32, 50);
    b.g(gTEMPO, 172);
}

static void presetIdm(PB& b)
{
    b.kit(0, mFM_KICK, {45, 40, 90, 20, 60, 70, 15, 40}); b.fx(0, tpSRR, 20);
    b.kit(1, mFM_RIM, {80, 25, 80, 90, 15, 40, 10, 60}); b.fx(1, tpDEL, 40);
    b.kit(2, mPH_STRING, {60, 35, 90, 20, 30, 50, 80, 20}); b.fx(2, tpREV, 50);
    b.kit(3, mNOISE, {0, 0, 10, 90, 127, 40, 90, 0}); b.fx(3, tpPAN, 30);
    b.kit(4, mFM_HAT, {100, 10, 60, 100, 100, 120, 30, 110}); b.fx(4, tpPAN, 100);
    b.kit(5, mPH_METAL, {90, 40, 90, 110, 50, 90, 100, 10}); b.fx(5, tpSRR, 40); b.fx(5, tpDEL, 50);
    b.kit(6, mFM_BELL, {40, 30, 70, 70, 110, 90, 30, 40});
    b.kit(7, mSINE, {20, 30, 100, 15, 90, 0, 0, 60}); b.fx(7, tpDIST, 40);
    for (int t : {2, 5, 6}) { b.fx(t, tpLFOS, 90); b.fx(t, tpLFOD, 50); b.fx(t, tpLFOSH, 120); b.fx(t, tpLFODST, lfoDestValue(0)); }
    b.g(gECHO_TIME, 40); b.g(gECHO_FB, 70); b.g(gECHO_WIDTH, 127); b.g(gREV_SIZE, 50); b.g(gREV_GATE, 40);
    RandGen r(0xA17E);
    b.row(0, 0, "x..x.....x..x");
    b.row(0, 1, "..x..x.x....x");
    b.row(0, 2, "x.xx..x.x.xx.");
    b.row(0, 3, ".x...xx..x...");
    b.row(0, 4, "xx.x.xx");          // 7 steps against 13: the hats drift
    b.row(0, 5, "....x......");      // 11 steps
    b.row(0, 6, ".......x..x..");
    b.row(0, 7, "x.......x....");
    for (int t = 0; t < 8; ++t)
        for (int s = 0; s < 13; ++s)
            if (b.b.patterns[0].flags[t][s] && r.chance(0.5f)) {
                const int v = r.range(10, 117), p = r.range(0, 3);   // in this order on every compiler
                b.lock(0, t, s, p, v);
            }
    b.cond(0, 1, 5, 3); b.cond(0, 1, 7, 13); b.cond(0, 2, 6, 2); b.cond(0, 6, 10, 15);
    b.micro(0, 1, 2, 5); b.micro(0, 3, 5, -4); b.ratch(0, 4, 3, 3); b.ratch(0, 2, 10, 2);
    b.settings(0, 13, 50);
    b.row(1, 0, "x....x..x..x.x.");
    b.row(1, 1, ".x.x...x.x...xx");
    b.row(1, 2, "s.ss.s..s.ss.s.");
    b.row(1, 3, "..x...x....x..x");
    b.row(1, 4, "x.xx.xxx.xxx.xx");
    b.row(1, 5, "......x........");
    b.row(1, 7, "x......x.......");
    for (int t = 0; t < 8; ++t)
        for (int s = 0; s < 15; ++s)
            if (b.b.patterns[1].flags[t][s] && r.chance(0.6f)) {
                const int v = r.range(10, 117), p = r.range(0, 4);   // in this order on every compiler
                b.lock(1, t, s, p, v);
            }
    b.settings(1, 15, 50, 0, 90);
    b.g(gTEMPO, 104);
}

static void presetIndustrial(PB& b)
{
    b.kit(0, mPH_KICK, {35, 70, 80, 70, 30, 70, 40, 90}); b.fx(0, tpDIST, 60);
    b.kit(1, mFM_CLAP, {60, 60, 80, 40, 110, 90, 70, 110}); b.fx(1, tpREV, 50);
    b.kit(2, mPH_METAL, {40, 60, 110, 100, 30, 100, 90, 60}); b.fx(2, tpDIST, 40);
    b.kit(3, mNOISE, {0, 0, 30, 60, 90, 60, 100, 60});
    b.kit(4, mFM_HAT, {60, 20, 80, 30, 110, 70, 40, 60});
    b.kit(5, mTOM, {30, 50, 90, 30, 90, 40, 80, 100});
    b.g(gREV_SIZE, 80); b.g(gREV_GATE, 60); b.g(gCMP_THR, 80); b.g(gCMP_RAT, 60); b.g(gDRIVE, 30);
    b.row(0, 0, "X...x...X...x..x");
    b.row(0, 1, "....X.......X...");
    b.row(0, 2, "..x.....x.x.....");
    b.row(0, 3, "x.x.x.x.x.x.x.x.");
    b.row(0, 4, ".x.x.x.x.x.x.x.x");
    b.row(0, 5, "......x.......xx");
    b.lock(0, 5, 15, 0, 45);
    b.settings(0, 16, 50, 0, 100);
    b.row(1, 0, "X..xX...X..xX.xx");
    b.row(1, 1, "....X.......X.X.");
    b.row(1, 2, "x.x.....x.x...x.");
    b.row(1, 3, "xxxxxxxxxxxxxxxx");
    for (int s = 0; s < 16; ++s) b.lock(1, 3, s, 2, 10 + s * 4);
    b.row(1, 5, "...x...x...x..xx");
    b.settings(1, 16, 50, 0, 100);
    b.g(gTEMPO, 126);
}

static void presetMinimal(PB& b)
{
    b.kit(0, mKICK, {52, 45, 30, 20, 40, 0, 0, 10});
    b.kit(1, mSINE, {110, 5, 60, 5, 0, 0, 0, 30}); b.fx(1, tpPAN, 40);
    b.kit(2, mCLAVE, {75, 15, 40, 80, 40, 0, 10, 0}); b.fx(2, tpPAN, 90); b.fx(2, tpDEL, 40);
    b.kit(3, mSHAKER, {5, 5, 25, 90, 110, 60, 80, 0});
    b.kit(4, mRIM, {70, 12, 30, 90, 10, 10, 70, 0}); b.fx(4, tpDEL, 30);
    b.kit(5, mPH_MALLET, {75, 30, 90, 60, 100, 30, 0, 0}); b.fx(5, tpREV, 40);
    b.kit(6, mCLAP, {50, 30, 40, 70, 30, 40, 30, 0}); b.fx(6, tpVOL, 80);
    b.g(gECHO_TIME, 48); b.g(gECHO_FB, 55);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "..x..x.....x..x.");
    for (int s : {2, 5, 11, 14}) b.lock(0, 1, s, 0, 90 + s * 2);
    b.row(0, 2, ".......x.......x");
    b.row(0, 3, "..x...x...x...x.");
    b.row(0, 4, "...x......x.....");
    b.row(0, 5, "......x.........");
    b.lock(0, 5, 6, 0, 82);
    b.row(0, 6, "....x.......x...");
    b.settings(0, 16, 56);
    b.row(1, 0, "x...x...x...x...");
    b.row(1, 1, "..x..x.x...x..x.");
    b.row(1, 3, "xxx.xxx.xxx.xxx.");
    b.row(1, 4, "...x..x...x..x..");
    b.row(1, 5, "......x.......x.");
    b.melody(1, 5, 75, kMlSemi, {0, -5});
    b.settings(1, 16, 56);
    b.g(gTEMPO, 124);
}

static void presetLofi(PB& b)
{
    b.kit(0, mKICK, {50, 50, 30, 30, 50, 10, 20, 30}); b.fx(0, tpSRR, 35); b.fx(0, tpCUTOFF, 80);
    b.kit(1, mSNARE, {48, 35, 20, 20, 80, 45, 50, 40}); b.fx(1, tpSRR, 35); b.fx(1, tpCUTOFF, 85); b.fx(1, tpREV, 30);
    b.kit(2, mCLHAT, {55, 20, 60, 50, 20, 64, 10, 0}); b.fx(2, tpSRR, 30); b.fx(2, tpCUTOFF, 90);
    b.kit(3, mPH_STRING, {25, 50, 40, 60, 60, 0, 10, 10}); b.fx(3, tpCUTOFF, 70);
    b.kit(4, mNOISE, {0, 0, 127, 50, 70, 0, 60, 0}); b.fx(4, tpVOL, 35);
    b.kit(5, mRIM, {50, 15, 70, 50, 20, 20, 30, 0});
    b.g(gEQ_HIGH, 50); b.g(gDRIVE, 30);
    b.row(0, 0, "x.......x.x.....");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "x.x.x.x.x.x.x.xx");
    b.row(0, 3, "x.....x...x.....");
    b.melody(0, 3, 25, kKsSemi, {0, 3, -2});
    b.row(0, 4, "x...............");
    b.row(0, 5, "...........x....");
    b.settings(0, 16, 64);
    b.row(1, 0, "x......xx.x..x..");
    b.row(1, 1, "....x.......x..x");
    b.row(1, 2, "x.x.x.x.x.x.x.x.");
    b.row(1, 3, "x.....x...x...x.");
    b.melody(1, 3, 25, kKsSemi, {0, 3, 5, 7});
    b.row(1, 4, "x...............");
    b.settings(1, 16, 64);
    b.g(gTEMPO, 86);
}

static void presetBraindance(PB& b)
{
    b.kit(0, mFM_KICK, {48, 55, 60, 30, 50, 40, 25, 30});
    b.kit(1, mFM_SNARE, {70, 35, 80, 35, 60, 55, 25, 70});
    b.kit(2, mFM_HAT, {85, 20, 30, 80, 90, 100, 50, 90});
    b.kit(3, mFM_BELL, {60, 45, 50, 30, 60, 70, 40, 70});
    b.kit(4, mSINE, {35, 45, 20, 30, 100, 0, 0, 50}); b.fx(4, tpCUTOFF, 55); b.fx(4, tpRESO, 70);
    b.kit(5, mFM_TOM, {50, 50, 50, 40, 50, 40, 30, 40});
    b.kit(6, mFM_CYMBAL, {64, 70, 80, 30, 90, 90, 80, 70});
    b.g(gECHO_TIME, 56); b.g(gECHO_FB, 50); b.g(gREV_SIZE, 50);
    b.row(0, 0, "x..x..x...x.x...");
    b.row(0, 1, "....X..x....X.x.");
    b.row(0, 2, "x.xxx.xxx.xxx.xx");
    b.row(0, 3, "..x....x..x.....");
    b.row(0, 4, "x.sS..x.s.xS..x.");
    b.melody(0, 4, 40, kSnSemi, {0, 12, 7, 10, 0, 3, 15, 12});
    b.row(0, 5, "...........x.x..");
    b.lock(0, 5, 11, 0, 70); b.lock(0, 5, 13, 0, 50);
    b.settings(0, 16, 50, 0, 80);
    b.row(1, 0, "x..x..x...x.x..x");
    b.row(1, 1, "....X..x.x..X.xx");
    b.row(1, 2, "xxxxxxxxxxxxxxxx");
    for (int s = 0; s < 16; ++s) b.lock(1, 2, s, 0, 60 + (s % 3) * 15);
    b.row(1, 4, "x.sS.sx.s.xS.sx.");
    b.melody(1, 4, 40, kSnSemi, {0, 7, 12, 3, 10, 0});
    b.row(1, 6, "x...............");
    b.settings(1, 16, 50, 0, 80);
    b.g(gTEMPO, 140);
}

static void presetPercussion(PB& b)
{
    b.kit(0, mPH_KICK, {45, 50, 40, 30, 40, 50, 0, 0});
    b.kit(1, mPH_MALLET, {70, 55, 60, 30, 20, 60, 50, 0}); b.fx(1, tpREV, 40);
    b.kit(2, mTOM, {80, 40, 20, 30, 50, 15, 0, 0});  // conga hi
    b.kit(3, mTOM, {62, 45, 20, 30, 50, 15, 0, 0});  // conga lo
    b.kit(4, mCLAVE, {70, 25, 50, 70, 40, 5, 10, 0});
    b.kit(5, mSHAKER, {15, 10, 30, 80, 110, 70, 90, 0});
    b.kit(6, mCOWBELL, {70, 40, 60, 75, 50, 60, 20, 0});
    b.kit(7, mPH_METAL, {75, 60, 60, 50, 40, 60, 50, 0}); b.fx(7, tpREV, 60);
    b.fx(0, tpVOL, 90); b.fx(1, tpVOL, 92);
    b.g(gREV_SIZE, 60); b.g(gVOLUME, 92);
    b.row(0, 0, "x.....x.x.......");
    b.row(0, 1, "x..x..x...x.x...");
    b.melody(0, 1, 65, kMlSemi, {0, 3, 7, 10, 7});
    b.row(0, 2, "..x..x....x..x..");
    b.row(0, 3, "x.....x.......x.");
    b.row(0, 4, "x..x..x...x.x...");
    b.row(0, 5, "xxxxxxxxxxxxxxxx");
    for (int s = 0; s < 16; s += 2) b.lock(0, 5, s, 2, 45);
    b.row(0, 6, "......x.......x.");
    b.row(0, 7, "x...............");
    b.settings(0, 16, 58);
    b.row(1, 0, "x.....x.x.....x.");
    b.row(1, 1, "x.xx.x..x.xx.x..");
    b.melody(1, 1, 65, kMlSemi, {0, 7, 3, 10, 12, 7, 5});
    b.row(1, 2, ".xx..x.x..x..xx.");
    b.row(1, 3, "x.....x...x...x.");
    b.row(1, 5, "x.xxx.xxx.xxx.xx");
    b.settings(1, 16, 58);
    b.g(gTEMPO, 104);
}


static void presetHouse(PB& b)
{
    b.kit(0, mKICK, {48, 62, 35, 30, 45, 0, 5, 25});
    b.kit(1, mCLAP, {64, 45, 50, 60, 30, 70, 30, 10}); b.fx(1, tpREV, 30);
    b.kit(2, mCLHAT, {72, 15, 90, 85, 25, 64, 0, 0});
    b.kit(3, mOPHAT, {72, 40, 88, 85, 25, 64, 0, 0});
    b.kit(4, mRIM, {70, 15, 40, 80, 20, 15, 50, 10});
    b.kit(5, mSHAKER, {8, 5, 25, 85, 110, 40, 80, 0}); b.fx(5, tpVOL, 80);
    b.kit(6, mPH_MALLET, {66, 40, 70, 45, 75, 55, 30, 0}); b.fx(6, tpDEL, 45); b.fx(6, tpREV, 35);
    b.kit(7, mSINE, {34, 45, 15, 30, 40, 0, 0, 30}); b.fx(7, tpCUTOFF, 70);
    b.g(gECHO_TIME, 68); b.g(gECHO_FB, 45); b.g(gREV_SIZE, 50);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "x.x.x.x.x.x.x.x.");
    b.row(0, 3, "..x...x...x...x.");
    b.row(0, 4, "...x.....x....x.");
    b.row(0, 5, "xxxxxxxxxxxxxxxx");
    for (int s = 0; s < 16; ++s) b.lock(0, 5, s, 2, s % 2 ? 35 : 20);
    for (int s = 1; s < 16; s += 2) b.cond(0, 5, s, 2);
    b.choke(2, 1); b.choke(3, 1);
    b.row(0, 6, "...x..x.....x...");
    b.melody(0, 6, 66, kMlSemi, {0, 3, 7});
    b.row(0, 7, "..x...x...x..xx.");
    b.melody(0, 7, 34, kSnSemi, {0, 0, 0, 3, 5});
    b.settings(0, 16, 54);
    b.row(1, 0, "x...x...x...x...");
    b.row(1, 1, "....x.......x..x");
    b.row(1, 2, "x.x.x.x.x.x.x.x.");
    b.row(1, 3, "..x...x...x...x.");
    b.row(1, 5, "xxxxxxxxxxxxxxxx");
    b.row(1, 6, "x..x..x...x..x..");
    b.melody(1, 6, 66, kMlSemi, {0, 5, 3, 7, 10});
    b.row(1, 7, "..x...x...x...x.");
    b.melody(1, 7, 34, kSnSemi, {0, 0, 5, 3});
    b.settings(1, 16, 54);
    b.g(gTEMPO, 124);
}

static void presetGarage(PB& b)
{
    b.kit(0, mKICK, {50, 55, 40, 25, 60, 0, 10, 20});
    b.kit(1, mSNARE, {60, 30, 30, 15, 90, 80, 50, 30}); b.fx(1, tpREV, 25);
    b.kit(2, mCLHAT, {78, 12, 95, 90, 20, 60, 0, 0});
    b.kit(3, mRIM, {72, 14, 40, 85, 25, 15, 55, 10});
    b.kit(4, mSINE, {28, 70, 25, 30, 60, 0, 0, 40}); b.fx(4, tpCUTOFF, 60); b.fx(4, tpRESO, 40);
    b.kit(5, mFM_BELL, {75, 25, 40, 20, 40, 60, 30, 90}); b.fx(5, tpDEL, 40);
    b.row(0, 0, "x.......x.x.....");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "..x...x...x...x.");
    b.row(0, 3, ".x.....x.x....x.");
    b.row(0, 4, "x.....x...x..x..");
    b.melody(0, 4, 28, kSnSemi, {0, 7, 5, 3});
    b.row(0, 5, "...x.......x....");
    b.settings(0, 16, 66);
    b.row(1, 0, "x.......x.x...x.");
    b.row(1, 1, "....x.......x..x");
    b.row(1, 2, "..x.x.x...x.x.x.");
    b.row(1, 3, ".x...x...x...x..");
    b.row(1, 4, "x.....x.x...x...");
    b.melody(1, 4, 28, kSnSemi, {0, 3, 5, 7});
    b.settings(1, 16, 66);
    b.g(gTEMPO, 132);
}

static void presetFootwork(PB& b)
{
    b.kit(0, mKICK, {44, 80, 60, 30, 50, 0, 20, 40});
    b.kit(1, mCLAP, {50, 35, 35, 65, 30, 40, 20, 20});
    b.kit(2, mCLHAT, {80, 10, 95, 90, 20, 60, 0, 0});
    b.kit(3, mTOM, {55, 45, 40, 30, 50, 10, 0, 10});
    b.kit(4, mRIM, {75, 12, 40, 90, 20, 15, 50, 10});
    b.kit(5, mPH_STRING, {45, 45, 70, 30, 40, 0, 30, 20}); b.fx(5, tpDEL, 30);
    b.row(0, 0, "x..x..x...x..x..x..x..x...x..x..");
    b.row(0, 1, "........x...............x.......");
    b.row(0, 2, "x.xxx.x.x.xxx.x.x.xxx.x.x.xxx.xx");
    b.row(0, 3, "..........x..x..........x.x..x..");
    b.lock(0, 3, 13, 0, 40); b.lock(0, 3, 29, 0, 35);
    b.row(0, 4, ".x.....x.....x...x.....x....x...");
    b.row(0, 5, "x.....x...x.....x.....x...x.x...");
    b.melody(0, 5, 45, kKsSemi, {0, 0, 3, 0, 0, 5, 7});
    b.settings(0, 32, 50);
    b.row(1, 0, "x..x..x..x..x..xx..x..x..x..x..x");
    b.row(1, 1, "........x...............x...x...");
    b.row(1, 2, "xxxxx.x.xxxxx.x.xxxxx.x.xxxxxxxx");
    b.row(1, 3, "...x.....x.x.......x.....x.x.xx.");
    b.row(1, 5, "x..x..x...x..x..x..x..x...x..x..");
    b.melody(1, 5, 45, kKsSemi, {0, 3, 0, 7, 5});
    b.settings(1, 32, 50);
    b.g(gTEMPO, 160);
}

static void presetTrap(PB& b)
{
    b.kit(0, mSINE, {14, 100, 40, 40, 5, 0, 20, 50}); b.fx(0, tpDIST, 20); // 808 boom
    b.kit(1, mSNARE, {66, 30, 30, 15, 100, 70, 50, 30}); b.fx(1, tpREV, 30);
    b.kit(2, mCLAP, {80, 30, 30, 60, 30, 40, 20, 20});
    b.kit(3, mCLHAT, {85, 10, 95, 95, 20, 60, 0, 0});
    b.kit(4, mOPHAT, {80, 35, 90, 90, 20, 60, 0, 0});
    b.kit(5, mKICK, {55, 25, 50, 15, 80, 0, 20, 30});
    b.g(gECHO_TIME, 48); b.g(gREV_SIZE, 50);
    b.row(0, 0, "x......x..x.....x.........x.....");
    b.melody(0, 0, 14, kSnSemi, {0, 0, 3, 0, -2});
    b.row(0, 1, "........x...............x.......");
    b.row(0, 2, "........x...............x.......");
    b.row(0, 3, "x.x.x.x.x.xxx.x.x.x.x.x.xxxxx.x.");
    for (int s : {10, 11, 24, 25, 26, 27}) b.lock(0, 3, s, 1, 6);
    b.ratch(0, 3, 10, 2); b.ratch(0, 3, 11, 3); b.ratch(0, 3, 26, 4); b.ratch(0, 3, 27, 6);
    b.choke(3, 1); b.choke(4, 1);
    b.row(0, 4, "..............x...............x.");
    b.row(0, 5, "x..............x..x.............");
    b.settings(0, 32, 50);
    b.row(1, 0, "x.....x...x.x...x......x..x.....");
    b.melody(1, 0, 14, kSnSemi, {0, 5, 3, 0, 7, 0});
    b.row(1, 1, "........x...............x.....x.");
    b.row(1, 2, "........x...............x.......");
    b.row(1, 3, "xxx.x.x.xxx.x.x.xxxxxxx.x.xxx.x.");
    b.row(1, 5, "x.....x.........x.x.............");
    b.settings(1, 32, 50);
    b.g(gTEMPO, 140);
}

static void presetDembow(PB& b)
{
    b.kit(0, mKICK, {50, 60, 40, 25, 60, 0, 10, 35});
    b.kit(1, mSNARE, {62, 30, 25, 15, 95, 75, 45, 25});
    b.kit(2, mRIM, {70, 15, 50, 80, 20, 15, 50, 10});
    b.kit(3, mCLHAT, {75, 15, 90, 85, 25, 64, 0, 0});
    b.kit(4, mTOM, {70, 35, 30, 30, 40, 10, 0, 0});
    b.kit(5, mSHAKER, {8, 5, 20, 85, 110, 50, 90, 0});
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "...x..x....x..x.");
    b.row(0, 2, "...x..x....x..x.");
    b.row(0, 3, "x.x.x.x.x.x.x.x.");
    b.row(0, 4, "..........x...x.");
    b.row(0, 5, "xxxxxxxxxxxxxxxx");
    b.settings(0, 16, 52);
    b.row(1, 0, "x...x...x...x...");
    b.row(1, 1, "...x..x....x..xx");
    b.row(1, 3, "x.xxx.x.x.xxx.x.");
    b.row(1, 4, "......x...x.x.x.");
    b.lock(1, 4, 12, 0, 60); b.lock(1, 4, 14, 0, 50);
    b.row(1, 5, "xxxxxxxxxxxxxxxx");
    b.settings(1, 16, 52);
    b.g(gTEMPO, 96);
}

static void presetJungle(PB& b)
{
    b.kit(0, mKICK, {52, 45, 45, 25, 80, 5, 30, 40}); b.fx(0, tpSRR, 15);
    b.kit(1, mSNARE, {68, 32, 45, 15, 100, 65, 60, 35}); b.fx(1, tpSRR, 15);
    b.kit(2, mCLHAT, {72, 15, 85, 70, 20, 60, 0, 10}); b.fx(2, tpSRR, 15);
    b.kit(3, mCYMBAL, {64, 80, 60, 60, 20, 70, 40, 0}); b.fx(3, tpSRR, 15);
    b.kit(4, mSINE, {14, 110, 30, 50, 0, 0, 30, 30}); b.fx(4, tpVOL, 105);
    b.kit(5, mPH_METAL, {55, 70, 50, 80, 40, 70, 70, 0}); b.fx(5, tpDEL, 60); b.fx(5, tpREV, 60);
    b.g(gECHO_TIME, 72); b.g(gECHO_FB, 65); b.g(gREV_SIZE, 70); b.g(gCMP_THR, 90); b.g(gCMP_RAT, 50);
    b.row(0, 0, "x.x.......x..... ..x.......x.....");
    b.row(0, 1, "....x..x.x..x..x ....x..x.x..x.x.");
    for (int s : {7, 9, 15, 23, 25, 30}) b.lock(0, 1, s, 2, 15);
    b.row(0, 2, "x.x.x.x.x.x.x.xx x.x.x.x.x.x.x.x.");
    b.row(0, 3, "x............... ................");
    b.row(0, 4, "x............... ..........x.....");
    b.melody(0, 4, 14, kSnSemi, {0, -2});
    b.row(0, 5, "........x....... ........x.......");
    b.settings(0, 32, 50);
    b.row(1, 0, "x.x.......xx.... ..x....x..x.....");
    b.row(1, 1, "....x..x.x..x.xx ....x..xx...xxxx");
    for (int s = 28; s < 32; ++s) b.lock(1, 1, s, 0, 55 + (s - 28) * 12);
    b.row(1, 2, "xxx.x.x.xxx.x.x. x.x.xxx.x.x.xxx.");
    b.row(1, 4, "x............... x...............");
    b.melody(1, 4, 14, kSnSemi, {0, 3});
    b.settings(1, 32, 50);
    b.g(gTEMPO, 168);
}

static void presetPolyLab(PB& b)
{
    // every track has its own length: the pattern keeps evolving for a long time
    b.kit(0, mPH_KICK, {42, 55, 50, 40, 40, 50, 0, 10});
    b.kit(1, mPH_MALLET, {70, 45, 60, 35, 30, 60, 40, 0}); b.fx(1, tpDEL, 35); b.fx(1, tpPAN, 40);
    b.kit(2, mPH_MALLET, {58, 50, 50, 35, 70, 30, 30, 0}); b.fx(2, tpDEL, 35); b.fx(2, tpPAN, 88);
    b.kit(3, mCLAVE, {72, 25, 50, 70, 40, 5, 10, 0});
    b.kit(4, mSHAKER, {10, 8, 25, 85, 110, 60, 80, 0});
    b.kit(5, mPH_METAL, {80, 60, 70, 60, 40, 70, 50, 0}); b.fx(5, tpREV, 60);
    b.kit(6, mFM_RIM, {75, 18, 50, 70, 15, 20, 10, 30});
    b.g(gECHO_TIME, 56); b.g(gECHO_FB, 50); b.g(gREV_SIZE, 60);
    b.row(0, 0, "x...x...x...x...");          // 16
    b.row(0, 1, "x..x.x..x.x.");              // 12
    b.melody(0, 1, 70, kMlSemi, {0, 3, 7, 10, 7});
    b.row(0, 2, "x...x.x...");                // 10
    b.melody(0, 2, 58, kMlSemi, {0, 5, 3});
    b.row(0, 3, "x..x..x");                   // 7
    b.row(0, 4, "xx.x.");                     // 5
    b.row(0, 5, "x........");                 // 9
    b.melody(0, 5, 80, kMtSemi, {0, 5});
    b.row(0, 6, "..x...x.x..");               // 11
    b.settings(0, 16, 54);
    b.row(1, 0, "x..x..x...x..x..");          // 16
    b.row(1, 1, "x.x..x.x..x..x.");           // 15
    b.melody(1, 1, 70, kMlSemi, {0, 7, 3, 12, 10});
    b.row(1, 2, "x...x.x.");                  // 8
    b.melody(1, 2, 58, kMlSemi, {0, 3});
    b.row(1, 3, "x.x..x");                    // 6
    b.row(1, 4, "xxx.x.xx.");                 // 9
    b.row(1, 6, "..x.x..x.x.x.");             // 13
    b.settings(1, 16, 54);
    b.g(gTEMPO, 112);
}

static void presetAmbient(PB& b)
{
    b.kit(0, mPH_KICK, {38, 80, 20, 30, 70, 30, 0, 0}); b.fx(0, tpVOL, 90);
    b.kit(1, mPH_METAL, {60, 110, 30, 50, 50, 70, 60, 0}); b.fx(1, tpREV, 100); b.fx(1, tpDEL, 70);
    b.kit(2, mPH_MALLET, {75, 100, 20, 20, 90, 40, 80, 0}); b.fx(2, tpREV, 90); b.fx(2, tpDEL, 60);
    b.kit(3, mNOISE, {90, 60, 110, 70, 90, 40, 0, 0}); b.fx(3, tpVOL, 60); b.fx(3, tpREV, 80);
    b.fx(3, tpLFOS, 10); b.fx(3, tpLFOD, 60); b.fx(3, tpLFOSH, 25); b.fx(3, tpLFODST, lfoDestValue(tpCUTOFF));
    b.kit(4, mPH_STRING, {50, 100, 40, 50, 30, 10, 10, 0}); b.fx(4, tpREV, 70); b.fx(4, tpDEL, 50);
    b.g(gECHO_TIME, 100); b.g(gECHO_FB, 80); b.g(gECHO_LPF, 70); b.g(gECHO_WIDTH, 127);
    b.g(gREV_SIZE, 120); b.g(gREV_DAMP, 80); b.g(gREV_LEV, 90);
    b.row(0, 0, "x...............x..........");     // 27
    b.row(0, 1, "x.......................");        // 24
    b.melody(0, 1, 60, kMtSemi, {0, 5, -2});
    b.row(0, 2, "....x.....x......x.....");         // 23
    b.melody(0, 2, 75, kMlSemi, {0, 7, 12, 3, 10});
    b.row(0, 3, "x...............................");// 32
    b.row(0, 4, "..x.........x.......x....");       // 25
    b.melody(0, 4, 50, kKsSemi, {0, 7, 5, 12});
    b.settings(0, 32, 50);
    b.g(gTEMPO, 80);
}


// ---- 0.12: presets that show CHAOS, chorus and per-track SPEED (voiced with the exponential RESONANCE)
static void presetMalletTechno(PB& b)
{
    b.kit(0, mKICK, {50, 70, 40, 30, 55, 5, 10, 40}); b.fx(0, tpDIST, 15);
    b.kit(1, mCLAP, {64, 50, 45, 50, 25, 85, 20, 10}); b.fx(1, tpREV, 30);
    b.kit(2, mCLHAT, {75, 18, 95, 95, 30, 60, 0, 0});
    b.kit(3, mOPHAT, {75, 45, 90, 90, 30, 60, 0, 0});
    b.kit(4, mPH_MALLET, {68, 40, 75, 40, 85, 50, 20, 0}); b.fx(4, tpDEL, 40);
    b.kit(5, mFM_HAT, {90, 20, 0, 64, 90, 100, 60, 100});
    for (int t = 0; t < 6; ++t) { b.fx(t, tpCUTOFF, 100); b.fx(t, tpRESO, 20); }
    b.g(gECHO_TIME, 77); b.g(gECHO_FB, 55); b.g(gREV_SIZE, 60);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "x.x.x.x.x.x.x.x.");
    b.row(0, 3, "..x...x...x...x.");
    b.row(0, 4, "x..x..x...x..x..");
    b.melody(0, 4, 60, kMlSemi, {0, 3, 7, 10, 7, 3});
    b.row(0, 5, "xxxxxxxxxxxxxxxx");
    b.settings(0, 16, 52);
    b.g(gTEMPO, 126);
    // a slow LFO opens and closes the mallets
    b.fx(4, tpCUTOFF, 80); b.fx(4, tpRESO, 50); b.fx(0, tpDIST, 30); b.fx(5, tpVOL, 75);
    b.fx(4, tpLFOS, 18); b.fx(4, tpLFOD, 90); b.fx(4, tpLFODST, lfoDestValue(tpCUTOFF));
    b.g(gREV_LEV, 60); b.g(gECHO_LEV, 60);
}
static void presetChaosBreaks(PB& b)
{
    b.kit(0, mKICK, {55, 55, 45, 25, 80, 5, 30, 50});
    b.kit(1, mSNARE, {62, 38, 40, 15, 100, 60, 60, 40}); b.fx(1, tpDIST, 20);
    b.kit(2, mCLHAT, {70, 22, 75, 70, 30, 64, 0, 20});
    b.kit(3, mRIM, {60, 20, 40, 70, 30, 20, 40, 20});
    b.kit(4, mFM_SNARE, {80, 25, 90, 25, 30, 50, 20, 80}); b.fx(4, tpVOL, 80);
    b.kit(5, mSINE, {28, 70, 20, 30, 90, 0, 0, 50});
    b.g(gCMP_THR, 90); b.g(gCMP_RAT, 50); b.g(gDRIVE, 20);
    // CHAOS on the snares and hats only: the kick and the bass keep the groove
    for (int t = 0; t < kTracks; ++t) b.g(gCHA_SEND + t, 0);
    b.g(gCHA_SEND + 1, 127); b.g(gCHA_SEND + 2, 127); b.g(gCHA_SEND + 4, 110);
    b.g(gGL_CHANCE, 45); b.g(gGL_SIZE, 70); b.g(gGL_LEN, 30); b.g(gGL_PITCH, 80); b.g(gGL_FADE, 25); b.g(gGL_REV, 30); b.g(gGL_MIX, 127);
    b.row(0, 0, "X.x.......xx....");
    b.row(0, 1, "....X..x.x..X..x");
    b.row(0, 2, "x.x.x.x.x.x.x.x.");
    b.row(0, 3, "...x..........x.");
    b.row(0, 4, "......x.......x.");
    b.row(0, 5, "x.........x.....");
    b.melody(0, 5, 28, kSnSemi, {0, 3});
    b.micro(0, 1, 7, 3); b.micro(0, 1, 9, -2);
    b.settings(0, 16, 58);
    b.g(gTEMPO, 172);
}
static void presetPolySpeed(PB& b)
{
    b.kit(0, mKICK, {48, 65, 40, 30, 60, 0, 10, 30});
    b.kit(1, mCLAP, {66, 50, 45, 50, 25, 85, 20, 10}); b.fx(1, tpREV, 40);
    b.kit(2, mCLHAT, {78, 15, 95, 95, 30, 60, 0, 0});
    b.kit(3, mSHAKER, {15, 10, 30, 80, 110, 70, 90, 0});
    b.kit(4, mCOWBELL, {66, 35, 60, 75, 50, 60, 20, 0}); b.fx(4, tpDEL, 30);
    b.kit(5, mTOM, {70, 45, 20, 30, 50, 15, 0, 0}); b.g(gCHO_SEND + 5, 70);
    b.kit(6, mPH_MALLET, {70, 50, 70, 40, 60, 50, 30, 0}); b.fx(6, tpREV, 50); b.g(gCHO_SEND + 6, 90);
    b.g(gCHO_MODEL, chorusModelValue(cmEnsemble)); b.g(gCHO_LEV, 70);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "x.x.x.x.x.x.x.x."); b.speed(2, 1);   // hats at 2x
    b.row(0, 3, "x.xx.x"); b.speed(3, 3);             // shaker: 6 steps at 3/2x
    b.row(0, 4, "x..x..x."); b.speed(4, 4);           // cowbell at 3/4x
    b.row(0, 5, "x...x.x."); b.speed(5, 5);           // toms at half speed
    b.row(0, 6, "x..x..x..x"); b.speed(6, 4);
    b.melody(0, 6, 64, kMlSemi, {0, 7, 3, 10, 5});
    b.settings(0, 16, 54);
    b.g(gTEMPO, 118);
}
static void presetChorusDub(PB& b)
{
    b.kit(0, mKICK, {44, 85, 30, 40, 30, 0, 0, 20});
    b.kit(1, mRIM, {70, 25, 30, 90, 20, 15, 60, 10}); b.fx(1, tpDEL, 80);
    b.kit(2, mFM_BELL, {45, 40, 60, 70, 100, 80, 30, 40}); b.fx(2, tpDEL, 60); b.fx(2, tpREV, 60);
    b.kit(3, mCLHAT, {70, 18, 80, 80, 30, 60, 0, 0});
    b.kit(4, mNOISE, {0, 0, 10, 80, 127, 30, 90, 0}); b.fx(4, tpVOL, 40); b.fx(4, tpREV, 70);
    for (int t : {1, 2}) b.g(gCHO_SEND + t, 110);
    b.g(gCHO_MODEL, chorusModelValue(cmEnsemble)); b.g(gCHO_RATE, 30); b.g(gCHO_DEPTH, 80); b.g(gCHO_LEV, 72);
    b.g(gECHO_TIME, fxDiv(d8D)); b.g(gECHO_FB, 85); b.g(gECHO_MODEL, echoModelValue(emTape)); b.g(gECHO_MOD, 60); b.g(gECHO_LEV, 72);
    b.g(gREV_SIZE, 85); b.g(gREV_MODEL, reverbModelValue(rmHall));
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "..x.......x..x..");
    b.row(0, 2, "...x......x.....");
    b.melody(0, 2, 45, 2.f, {0, 5});
    b.row(0, 3, "..x...x...x...x.");
    b.row(0, 4, "x...............");
    b.settings(0, 16, 54);
    b.g(gTEMPO, 118);
}
static void presetLofiTape(PB& b)
{
    b.kit(0, mKICK, {50, 55, 30, 30, 50, 10, 20, 40}); b.fx(0, tpSRR, 30);
    b.kit(1, mSNARE, {60, 35, 30, 20, 80, 50, 40, 40}); b.fx(1, tpSRR, 40);
    b.kit(2, mCLHAT, {65, 20, 70, 70, 40, 60, 0, 10}); b.fx(2, tpSRR, 30);
    b.kit(3, mPH_STRING, {50, 50, 40, 40, 50, 40, 30, 20}); b.fx(3, tpREV, 40);
    b.kit(4, mSHAKER, {20, 10, 30, 70, 100, 60, 80, 0}); b.fx(4, tpVOL, 60);
    b.g(gDRIVE, 25); b.g(gEQ_HIGH, 52);
    // worn tape: crushed hits, drive and a darker top
    b.fx(0, tpSRR, 30); b.fx(1, tpSRR, 40); b.fx(2, tpSRR, 35);
    b.row(0, 0, "x......x..x.....");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "x.xx.x.xx.x.x.xx");
    b.row(0, 3, "x.......x.......");
    b.melody(0, 3, 50, kKsSemi, {0, 3, 7, 5});
    b.row(0, 4, "..x...x...x...x.");
    b.settings(0, 16, 62);
    b.g(gTEMPO, 86);
    b.g(gDRIVE, 30); b.g(gEQ_HIGH, 50); b.g(gMF_LP, 100);
}
static void presetGlitchGarage(PB& b)
{
    b.kit(0, mKICK, {48, 60, 40, 30, 60, 0, 10, 30});
    b.kit(1, mRIM, {65, 20, 35, 85, 25, 15, 55, 10});
    b.kit(2, mCLHAT, {75, 15, 90, 90, 30, 60, 0, 0});
    b.kit(3, mCLAP, {66, 50, 45, 50, 25, 85, 20, 10}); b.fx(3, tpREV, 40);
    b.kit(4, mSINE, {30, 60, 20, 30, 80, 0, 0, 40});
    for (int t = 0; t < kTracks; ++t) b.g(gCHA_SEND + t, 0);
    b.g(gCHA_SEND + 1, 127); b.g(gCHA_SEND + 3, 127);
    b.g(gGL_CHANCE, 30); b.g(gGL_SIZE, 90); b.g(gGL_LEN, 30); b.g(gGL_PITCH, 64); b.g(gGL_FADE, 40); b.g(gGL_REV, 60); b.g(gGL_MIX, 120);
    b.row(0, 0, "x.....x...x.....");
    b.row(0, 1, "..x..x.....x.x..");
    b.row(0, 2, "..x...x...x...x.");
    b.row(0, 3, "....x.......x...");
    b.row(0, 4, "x.....x...x..x..");
    b.melody(0, 4, 30, kSnSemi, {0, 0, 5, 3});
    b.settings(0, 16, 66);
    b.g(gTEMPO, 132);
}


// ---- 0.16: new presets (kick ATTACK, echo models / extras, chorus, CHAOS, SPEED; voiced with the exponential RESONANCE)
static void presetPeakTechno(PB& b)
{
    b.kit(0, mKICK, {46, 58, 50, 28, 70, 0, 10, 45}); b.g(gKATK + 0, 100);                 // punchy kick
    b.kit(1, mSINE, {30, 70, 10, 30, 0, 0, 20, 40}); b.fx(1, tpVOL, 70); b.fx(1, tpDEL, 70);  // rumble into a diffuse echo
    b.kit(2, mCLAP, {64, 55, 60, 60, 30, 85, 25, 20}); b.fx(2, tpREV, 45);
    b.kit(3, mCLHAT, {74, 14, 92, 88, 25, 64, 0, 10}); b.fx(3, tpPAN, 50);
    b.kit(4, mOPHAT, {72, 45, 90, 85, 25, 64, 0, 10}); b.fx(4, tpPAN, 78);
    b.kit(5, mFM_CYMBAL, {70, 55, 70, 40, 85, 95, 60, 85}); b.fx(5, tpVOL, 78); b.fx(5, tpPAN, 84);
    b.kit(6, mPH_METAL, {58, 45, 70, 80, 40, 60, 70, 10}); b.fx(6, tpDEL, 50); b.fx(6, tpPAN, 40);
    b.choke(3, 1); b.choke(4, 1);
    b.g(gECHO_TIME, fxDiv(d16D)); b.g(gECHO_FB, 70); b.g(gECHO_MODEL, echoModelValue(emDiffuse)); b.g(gECHO_MOD, 80); b.g(gECHO_LPF, 70);
    b.g(gREV_SIZE, 60); b.g(gCMP_THR, 95); b.g(gCMP_RAT, 50); b.g(gDRIVE, 15);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "..x...x...x...x.");
    b.row(0, 2, "....x.......x...");
    b.row(0, 3, "x.xxx.xxx.xxx.xx");
    b.row(0, 4, "..x...x...x...x.");
    b.row(0, 5, "x...x...x...x...");
    b.row(0, 6, "......x.....x..x");
    b.settings(0, 16, 50);
    b.row(1, 0, "x...x...x...x...");
    b.row(1, 1, "..x...x...x.x.x.");
    b.row(1, 2, "....x.......x.xx");
    b.row(1, 3, "xxxxxxxxxxxxxxxx");
    b.row(1, 4, "..x...x...x...x.");
    b.row(1, 5, "x...x...x...x...");
    b.row(1, 6, "...x..x...x..x..");
    b.settings(1, 16, 50);
    b.g(gTEMPO, 132);
}
static void presetChicagoHouse(PB& b)
{
    b.kit(0, mKICK, {50, 64, 30, 30, 40, 0, 10, 30});
    b.kit(1, mCLAP, {60, 50, 55, 55, 30, 75, 30, 10}); b.fx(1, tpREV, 35);
    b.kit(2, mCLHAT, {70, 18, 85, 80, 25, 64, 0, 0}); b.fx(2, tpPAN, 48);
    b.kit(3, mOPHAT, {70, 50, 85, 80, 25, 64, 0, 0}); b.fx(3, tpPAN, 80);
    b.kit(4, mCOWBELL, {62, 40, 64, 70, 40, 70, 30, 20}); b.fx(4, tpVOL, 70); b.fx(4, tpPAN, 36); b.fx(4, tpDEL, 35);
    b.kit(5, mRIM, {66, 18, 45, 80, 20, 15, 50, 10}); b.fx(5, tpPAN, 92);
    b.kit(6, mSINE, {32, 50, 15, 30, 40, 0, 0, 35});
    b.choke(2, 1); b.choke(3, 1);
    b.g(gECHO_TIME, fxDiv(d8D)); b.g(gECHO_FB, 40); b.g(gECHO_MODEL, echoModelValue(emTape)); b.g(gECHO_MOD, 40);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "x.x.x.x.x.x.x.x.");
    b.row(0, 3, "..x...x...x...x.");
    b.row(0, 4, "...x......x.....");
    b.row(0, 5, ".x..x.x..x..x.x.");
    b.row(0, 6, "..x..x..x...x.x.");
    b.melody(0, 6, 32, kSnSemi, {0, 0, 7, 5, 0});
    b.settings(0, 16, 58);
    b.g(gTEMPO, 122);
}
static void presetAfroHouse(PB& b)
{
    b.kit(0, mKICK, {44, 60, 35, 30, 40, 0, 10, 20}); b.g(gKATK + 0, 40);              // a softer kick
    b.kit(1, mTOM, {82, 38, 20, 30, 50, 15, 0, 0}); b.fx(1, tpPAN, 40);                  // conga high
    b.kit(2, mTOM, {64, 42, 20, 30, 50, 15, 0, 0}); b.fx(2, tpPAN, 88);                  // conga low
    b.kit(3, mSHAKER, {8, 5, 25, 85, 110, 40, 80, 0}); b.fx(3, tpVOL, 78); b.speed(3, 3); // shaker at 3/2x
    b.kit(4, mCLAVE, {78, 20, 50, 80, 40, 0, 10, 0}); b.fx(4, tpPAN, 30);
    b.kit(5, mFM_BELL, {60, 40, 50, 20, 55, 70, 40, 70}); b.fx(5, tpDEL, 45); b.fx(5, tpPAN, 96);
    b.kit(6, mRIM, {64, 18, 45, 80, 20, 15, 50, 10}); b.fx(6, tpPAN, 76);
    b.kit(7, mPH_MALLET, {60, 45, 65, 40, 70, 55, 30, 0}); b.fx(7, tpDEL, 40); b.fx(7, tpREV, 40);
    b.g(choSendG(5), 70); b.g(choSendG(7), 60); b.g(gCHO_MODEL, chorusModelValue(cmEnsemble)); b.g(gCHO_LEV, 60);
    b.g(gECHO_TIME, fxDiv(d8T)); b.g(gECHO_FB, 45); b.g(gECHO_MODEL, echoModelValue(emPingPong)); b.g(gREV_SIZE, 55);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "..x..x....x..x.x");
    b.row(0, 2, "x.....x..x....x.");
    b.row(0, 3, "x.xxx.xxx.xxx.xx");
    b.row(0, 4, "x..x..x...x.x...");   // 3-2 clave
    b.row(0, 5, ".......x.......x");
    b.row(0, 6, "....x.......x...");
    b.row(0, 7, "x..x..x...x..x..");
    b.melody(0, 7, 62, kMlSemi, {0, 3, 7, 10, 7, 3});
    b.settings(0, 16, 54);
    b.g(gTEMPO, 120);
}
static void presetLiquidDnb(PB& b)
{
    b.kit(0, mKICK, {50, 50, 40, 25, 50, 0, 10, 20}); b.g(gKATK + 0, 45);
    b.kit(1, mSNARE, {58, 38, 40, 18, 75, 70, 60, 45}); b.fx(1, tpREV, 30);
    b.kit(2, mCLHAT, {76, 14, 92, 88, 25, 64, 0, 0}); b.fx(2, tpPAN, 52);
    b.kit(3, mCYMBAL, {66, 60, 60, 60, 25, 70, 40, 0}); b.fx(3, tpVOL, 60); b.fx(3, tpPAN, 84);   // ride
    b.kit(4, mRIM, {66, 18, 45, 80, 20, 15, 50, 10}); b.fx(4, tpDEL, 60); b.fx(4, tpPAN, 36);
    b.kit(5, mFM_BELL, {64, 55, 40, 10, 45, 70, 50, 60}); b.fx(5, tpDEL, 45); b.fx(5, tpREV, 50);
    b.kit(6, mSINE, {28, 65, 10, 30, 30, 0, 20, 30});
    b.g(choSendG(5), 80); b.g(gCHO_MODEL, chorusModelValue(cmChorus)); b.g(gCHO_LEV, 60);
    b.g(gECHO_TIME, fxDiv(d8D)); b.g(gECHO_FB, 55); b.g(gECHO_MODEL, echoModelValue(emMulti)); b.g(gECHO_MOD, 60); b.g(gECHO_DUCK, 60);
    b.g(gREV_SIZE, 70); b.g(gREV_LEV, 60);
    b.row(0, 0, "x.........x.....x.x.......x.....");
    b.row(0, 1, "....x.......x.......x.......x...");
    b.row(0, 2, "x.x.x.xxx.x.x.x.x.x.x.xxx.x.x.xx");
    b.row(0, 3, "..x...x...x...x...x...x...x...x.");
    b.row(0, 4, ".......x.....x.........x...x....");
    b.row(0, 5, "x...............x.......x.......");
    b.melody(0, 5, 64, 2.f, {0, 7, 5});
    b.row(0, 6, "x.........x.....x.x.......x.....");
    b.melody(0, 6, 28, kSnSemi, {0, 0, 0, 3, 5});
    b.settings(0, 32, 52);
    for (int s : {6, 14, 22, 30}) b.lock(0, 1, s, tpVOL, 45);   // ghost snares
    b.row(0, 1, "....x.x.....x.x.....x.x.....x..x");
    b.g(gTEMPO, 174);
}
static void presetBoomBap(PB& b)
{
    b.kit(0, mKICK, {50, 55, 30, 25, 60, 10, 20, 50});
    b.kit(1, mSNARE, {56, 45, 35, 20, 85, 55, 55, 55}); b.fx(1, tpDIST, 25);
    b.kit(2, mCLHAT, {66, 20, 70, 75, 30, 64, 10, 20}); b.fx(2, tpPAN, 46);
    b.kit(3, mRIM, {62, 20, 50, 70, 30, 20, 40, 20}); b.fx(3, tpPAN, 84);
    b.kit(4, mPH_STRING, {48, 60, 45, 50, 50, 0, 30, 10}); b.fx(4, tpDEL, 40);   // plucked sample-like stab
    b.kit(5, mSINE, {30, 55, 10, 30, 20, 0, 10, 40});
    for (int t = 0; t < 4; ++t) b.fx(t, tpSRR, 25);
    b.g(gECHO_TIME, fxDiv(d4)); b.g(gECHO_FB, 35); b.g(gECHO_MODEL, echoModelValue(emTape)); b.g(gECHO_MOD, 70); b.g(gECHO_DRIVE, 40);
    b.g(gDRIVE, 25); b.g(gEQ_HIGH, 52); b.g(gCMP_THR, 90); b.g(gCMP_RAT, 60);
    b.row(0, 0, "x......x..x.....");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "x.x.x.x.x.x.x.x.");
    b.row(0, 3, ".......x......x.");
    b.row(0, 4, "x.........x.....");
    b.melody(0, 4, 48, kKsSemi, {0, -2});
    b.row(0, 5, "x......x..x.....");
    b.melody(0, 5, 30, kSnSemi, {0, 0, -2});
    b.settings(0, 16, 62);
    b.row(1, 0, "x......x.xx...x.");
    b.row(1, 1, "....x.......x..x");
    b.row(1, 2, "x.x.x.x.x.x.x.x.");
    b.row(1, 4, "x.....x...x.....");
    b.melody(1, 4, 48, kKsSemi, {0, 3, -2});
    b.row(1, 5, "x......x.xx...x.");
    b.melody(1, 5, 30, kSnSemi, {0, 0, 3, 3, -2});
    b.settings(1, 16, 62);
    b.g(gTEMPO, 90);
}
static void presetElectroBass(PB& b)
{
    b.kit(0, mKICK, {44, 70, 45, 35, 50, 0, 15, 40}); b.g(gKATK + 0, 90);
    b.kit(1, mCLAP, {66, 45, 50, 60, 35, 70, 25, 20});
    b.kit(2, mCLHAT, {76, 12, 90, 90, 25, 64, 0, 0}); b.fx(2, tpPAN, 50);
    b.kit(3, mCOWBELL, {66, 35, 64, 70, 40, 70, 30, 20}); b.fx(3, tpVOL, 72); b.fx(3, tpPAN, 82); b.fx(3, tpDEL, 50);
    b.kit(4, mSINE, {30, 70, 20, 30, 40, 0, 20, 60}); b.fx(4, tpVOL, 110);   // 808 bass
    b.kit(5, mFM_TOM, {50, 45, 40, 40, 50, 40, 30, 30}); b.fx(5, tpPAN, 34);
    b.g(gECHO_TIME, fxDiv(d8D)); b.g(gECHO_FB, 50); b.g(gECHO_MODEL, echoModelValue(emPingPong)); b.g(gECHO_RTIME, 80);
    b.row(0, 0, "x......x..x.....");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "xxxxxxxxxxxxxxxx");
    for (int s = 1; s < 16; s += 2) b.lock(0, 2, s, tpVOL, 70);
    b.row(0, 3, "..x.....x.x...x.");
    b.row(0, 4, "x..x..x...x..x..");
    b.melody(0, 4, 30, kSnSemi, {0, 0, 3, 0, 5, 7});
    b.row(0, 5, "...........x.x.x");
    b.settings(0, 16, 54);
    b.g(gTEMPO, 128);
}
static void presetReverseDub(PB& b)
{
    b.kit(0, mKICK, {42, 70, 30, 30, 30, 0, 10, 20}); b.g(gKATK + 0, 30);
    b.kit(1, mRIM, {60, 22, 50, 75, 30, 20, 40, 10}); b.fx(1, tpDEL, 90); b.fx(1, tpPAN, 40);
    b.kit(2, mFM_BELL, {58, 60, 40, 10, 50, 70, 50, 60}); b.fx(2, tpDEL, 80); b.fx(2, tpREV, 60); b.fx(2, tpPAN, 88);
    b.kit(3, mSHAKER, {10, 10, 30, 80, 100, 50, 70, 0}); b.fx(3, tpVOL, 65);
    b.kit(4, mNOISE, {0, 0, 90, 60, 90, 20, 0, 0}); b.fx(4, tpVOL, 85); b.fx(4, tpREV, 70);
    b.kit(5, mSINE, {26, 80, 5, 30, 20, 10, 30, 20});
    b.g(gECHO_TIME, fxDiv(d4)); b.g(gECHO_FB, 70); b.g(gECHO_MODEL, echoModelValue(emReverse)); b.g(gECHO_MOD, 30); b.g(gECHO_LEV, 80);
    b.g(gREV_SIZE, 100); b.g(gREV_LEV, 70);
    b.row(0, 0, "x.........x.....");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "......x.........");
    b.melody(0, 2, 58, 2.f, {0});
    b.row(0, 3, "..x...x...x...x.");
    b.row(0, 4, "x...............");
    b.row(0, 5, "x.........x.....");
    b.melody(0, 5, 26, kSnSemi, {0, -2});
    b.settings(0, 16, 50);
    b.g(gTEMPO, 72);
}
static void presetHalftime(PB& b)
{
    b.kit(0, mKICK, {46, 60, 45, 30, 60, 0, 10, 40}); b.g(gKATK + 0, 80);
    b.kit(1, mSNARE, {54, 50, 40, 20, 90, 60, 60, 60}); b.fx(1, tpREV, 40); b.fx(1, tpDIST, 20);
    b.kit(2, mCLHAT, {78, 12, 92, 90, 25, 64, 0, 0}); b.fx(2, tpPAN, 50); b.speed(2, 1);   // hats at 2x
    b.kit(3, mFM_HAT, {84, 40, 40, 90, 85, 100, 55, 80}); b.fx(3, tpPAN, 82); b.fx(3, tpVOL, 110);
    b.kit(4, mSINE, {26, 75, 25, 30, 60, 0, 20, 70}); b.fx(4, tpVOL, 105);
    b.kit(5, mPH_METAL, {62, 50, 70, 80, 40, 60, 70, 10}); b.fx(5, tpDEL, 60); b.fx(5, tpPAN, 34);
    b.g(gECHO_TIME, fxDiv(d8T)); b.g(gECHO_FB, 55); b.g(gECHO_MODEL, echoModelValue(emBbd)); b.g(gREV_SIZE, 70);
    b.row(0, 0, "x.....x...x.....");
    b.row(0, 1, "........x.......");
    b.row(0, 2, "x.x.x.x.x.x.xxxx");
    b.ratch(0, 2, 14, 3); b.ratch(0, 2, 15, 4);
    b.row(0, 3, "...x.......x....");
    b.row(0, 4, "x.....x...x.....");
    b.melody(0, 4, 26, kSnSemi, {0, 0, 3});
    b.row(0, 5, "............x...");
    b.settings(0, 16, 50);
    b.g(gTEMPO, 85);
}
static void presetPolymeter(PB& b)
{
    b.kit(0, mFM_KICK, {50, 55, 50, 30, 30, 32, 30, 0});
    b.kit(1, mCLAVE, {80, 20, 50, 80, 40, 0, 10, 0}); b.fx(1, tpPAN, 34); b.fx(1, tpDEL, 40);
    b.kit(2, mFM_RIM, {72, 20, 60, 60, 20, 30, 15, 20}); b.fx(2, tpPAN, 92);
    b.kit(3, mPH_MALLET, {62, 45, 65, 40, 70, 55, 30, 0}); b.fx(3, tpDEL, 55); b.fx(3, tpPAN, 50);
    b.kit(4, mFM_BELL, {70, 35, 50, 20, 55, 70, 40, 70}); b.fx(4, tpPAN, 80); b.fx(4, tpREV, 40);
    b.kit(5, mSHAKER, {8, 5, 25, 85, 110, 40, 80, 0}); b.fx(5, tpVOL, 70);
    b.g(gECHO_TIME, fxDiv(d16D)); b.g(gECHO_FB, 60); b.g(gECHO_MODEL, echoModelValue(emMulti)); b.g(gECHO_MOD, 90);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 1, "x..x..x..x..x");   b.b.patterns[0].trackLen[1] = 13;
    b.row(0, 2, "..x...x..x.");      b.b.patterns[0].trackLen[2] = 11;
    b.row(0, 3, "x..x.x.x.");        b.b.patterns[0].trackLen[3] = 9;
    b.melody(0, 3, 62, kMlSemi, {0, 7, 3, 10, 5});
    b.row(0, 4, "....x..");          b.b.patterns[0].trackLen[4] = 7;
    b.row(0, 5, "x.xxx");            b.b.patterns[0].trackLen[5] = 5;
    b.settings(0, 16, 50);
    b.g(gTEMPO, 118);
}

// factory preset: expReso = voiced with the 0.11 RESONANCE curve; cat = style (kPresetCats); trimDb = level trim so every
// preset plays at about the same loudness (-16 LUFS, measured with tests/audit.cpp --fit)
// smp: per track, a file of the built-in sample library (path relative to its folder, "" = none) that the UI loads
// onto the track's SAMPLE machine when the preset is loaded; such kits are listed first in the browser
struct PresetDef { const char* name; const char* desc; void (*build)(PB&); bool expReso = false; int cat = 0; float trimDb = 0.f;
                   const char* const* smp = nullptr; };
enum PresetCat { pcBasics, pcTechno, pcHouse, pcBreaks, pcHipHop, pcElectro, pcPerc, pcExperimental, pcAmbient, kPresetCatCount };
static const char* const kPresetCats[kPresetCatCount] = {"BASICS", "TECHNO", "HOUSE", "BREAKS", "HIP-HOP", "ELECTRO", "PERCUSSION", "EXPERIMENTAL", "AMBIENT"};


// ---- sample kits: every track is a SAMPLE machine playing a file of the built-in library (samples/, installed with
// the plug-in; redistributed with written permission, see THIRD_PARTY_NOTICES.txt). Without the library the tracks are silent.
static const char* const kSmp909[kTracks] = {"909/909BDRUM.wav", "909/909SNARE.wav", "909/909CLAP.wav", "909/909CLHAT.wav",
    "909/909OPHAT.wav", "909/909LTOM.wav", "909/909MTOM.wav", "909/909HTOM.wav", "909/909RIM.wav", "909/909RIDE.wav",
    "909/909CRASH.wav", "", "", "", "", ""};
static const char* const kSmp808[kTracks] = {"808/808BDRUM.wav", "808/808SNARE.wav", "808/808CLAP.wav", "808/808CLHAT.wav",
    "808/808OPHAT.wav", "808/808RIM.wav", "808/808CLAVE.wav", "808/808COW.wav", "808/808SHAKE.wav", "808/808CRASH.wav",
    "", "", "", "", "", ""};
static const char* const kSmpR8[kTracks] = {"R-8 MkII/PUNCHK.wav", "R-8 MkII/FATS1.wav", "R-8 MkII/DRYCLAP.wav",
    "R-8 MkII/78CHH.wav", "R-8 MkII/78OHH.wav", "R-8 MkII/VIDEOK.wav", "R-8 MkII/VIDEOS.wav", "R-8 MkII/808T_B.wav",
    "R-8 MkII/909T.wav", "R-8 MkII/909SIDE.wav", "R-8 MkII/808COW.wav", "R-8 MkII/78TAMB.wav", "R-8 MkII/CABASA.wav",
    "R-8 MkII/808CNG_A.wav", "R-8 MkII/FINGSNAP_A.wav", "R-8 MkII/BRRDC1.wav"};
static void sampleKit(PB& b, const char* const* smp)
{
    for (int t = 0; t < kTracks; ++t) if (smp[t][0]) b.kit(t, mSAMPLE);
    b.choke(3, 1); b.choke(4, 1);   // the closed hat cuts the open hat
}
static void preset909(PB& b)
{
    sampleKit(b, kSmp909);
    b.g(gTEMPO, 126);
    b.fx(2, tpREV, 20); b.fx(9, tpPAN, 76); b.fx(5, tpPAN, 50); b.fx(7, tpPAN, 78);
    b.row(0, 0, "x...x...x...x...");
    b.row(0, 2, "....x.......x...");
    b.row(0, 3, "x.x.x.x.x.x.x.x.");
    b.row(0, 4, "..x...x...x...x.");
    b.row(0, 9, "..........x...x.");
    b.row(0, 10, "x...............");
}
static void preset808(PB& b)
{
    sampleKit(b, kSmp808);
    b.g(gTEMPO, 104);
    b.fx(6, tpPAN, 80); b.fx(7, tpPAN, 48); b.fx(8, tpPAN, 76);
    b.row(0, 0, "x.....x...x.....");
    b.row(0, 1, "....x.......x...");
    b.row(0, 2, "............x...");
    b.row(0, 3, "x.xxx.x.x.xxx.x.");
    b.row(0, 4, "..............x.");
    b.row(0, 6, "...x......x.....");
    b.row(0, 7, ".......x........");
    b.row(0, 9, "x...............");
}
static void presetR8(PB& b)
{
    sampleKit(b, kSmpR8);
    b.g(gTEMPO, 112);
    b.fx(11, tpPAN, 80); b.fx(12, tpPAN, 48); b.fx(13, tpPAN, 72);
    b.row(0, 0, "x......x..x.....");
    b.row(0, 1, "....x.......x...");
    b.row(0, 3, "xxxxxxxxxxxxxxx.");
    b.row(0, 4, "...............x");
    b.row(0, 11, "....x.......x...");
    b.row(0, 12, "x.xxx.xxx.xxx.xx");
}

static const PresetDef kPresets[] = {
    {"Init",               "Factory state: default kit, every parameter at its base, empty sequencer", presetInit, false, pcBasics, 0.0f},
    {"Demo Groove",        "The default kit with a basic groove to start from",            presetDefault, false, pcBasics, 7.3f},
    {"Electro Funk",       "808-style electro: syncopated kicks, cowbell, sine bass",       presetElectro, false, pcElectro, 3.7f},
    {"Detroit Techno",     "Driving 4/4, clap, ride-like FM hats, mallet chord stabs",       presetTechno, false, pcTechno, 1.6f},
    {"Dub Techno",         "Deep kick, echoing rims and bell chords, long reverb, hiss",     presetDubTechno, false, pcTechno, 9.8f},
    {"Breakbeat",          "Punchy break with ghost snares, compressed and saturated",       presetBreakbeat, false, pcBreaks, -4.8f},
    {"Drill & Bass",       "172 BPM, 32 steps, snare rolls with rising pitch, plucked bass", presetDrillBass, false, pcBreaks, 1.3f},
    {"IDM Glitch",         "Odd lengths (13 / 15 steps), FM and physical machines, locks",   presetIdm, false, pcExperimental, 2.2f},
    {"Industrial",         "Distorted membrane kick, metal hits, crushed noise, gated verb", presetIndustrial, false, pcTechno, -7.8f},
    {"Minimal Click",      "Tiny clicks, claves, shaker, sparse mallets",                    presetMinimal, false, pcTechno, 10.0f},
    {"Lo-Fi Hip-Hop",      "Swung, bit-reduced drums, plucked bassline, vinyl hiss",         presetLofi, false, pcHipHop, -2.5f},
    {"Braindance",         "All-FM kit, acid-like sliding sine line, busy hats",             presetBraindance, false, pcElectro, 3.8f},
    {"Percussion Ensemble","Marimba, congas, claves, shaker, bells",                         presetPercussion, false, pcPerc, 5.3f},
    {"Deep House",         "Classic 4/4, offbeat hats, shaker, mallet chords, sine bass",    presetHouse, false, pcHouse, 4.8f},
    {"UK Garage",          "Swung 2-step, rim shots, bouncing sine bass",                    presetGarage, false, pcHouse, 6.6f},
    {"Footwork",           "160 BPM, triplet-ish kicks, rolling hats, plucked stabs",        presetFootwork, false, pcBreaks, 0.7f},
    {"Trap",               "Pitched sine 808 boom, hi-hat rolls, half-time snare",           presetTrap, false, pcHipHop, -0.7f},
    {"Dembow",             "Reggaeton / dembow groove with toms and shaker",                 presetDembow, false, pcPerc, 5.7f},
    {"Jungle",             "168 BPM chopped break feel, sub bass, dubby metal stabs",        presetJungle, false, pcBreaks, 1.3f},
    {"Polyrhythm Lab",     "Every track has its own length (5 to 16 steps): ever-changing", presetPolyLab, false, pcExperimental, 4.7f},
    {"Ambient Bells",      "Slow, long track lengths, bells, plucks, washed-out echo",       presetAmbient, false, pcAmbient, 1.0f},
    {"Mallet Techno",      "Techno with mallets that a slow LFO opens and closes",            presetMalletTechno, true, pcTechno, 2.0f},
    {"Chaos Breaks",       "172 BPM break; CHAOS repeats only the snares and hats",           presetChaosBreaks, true, pcBreaks, -3.6f},
    {"Speed Lab",          "Hats 2x, shaker 3/2x, cowbell 3/4x, toms 1/2x; ensemble chorus", presetPolySpeed, true, pcExperimental, 2.4f},
    {"Chorus Dub",         "Ensemble chorus on rims and bells, dub tape echo, hall",          presetChorusDub, true, pcAmbient, 6.4f},
    {"Lo-Fi Tape",         "Worn tape drums: crushed hits, drive and a dark top",             presetLofiTape, true, pcHipHop, -2.0f},
    {"Glitch Garage",      "Swung 2-step; CHAOS reverses and stutters the rims and claps",    presetGlitchGarage, true, pcHouse, 6.2f},
    {"Peak Time Techno",   "132 BPM, punchy kick (ATTACK), rumble in a DIFFUSE echo, ride and metal", presetPeakTechno, true, pcTechno, -4.5f},
    {"Chicago House",      "Jacking 4/4 with cowbell, rim shuffle, open hats and a sine bass",  presetChicagoHouse, true, pcHouse, 5.3f},
    {"Afro House",         "Congas, 3-2 clave, shaker at 3/2x, bells in ensemble chorus",       presetAfroHouse, true, pcPerc, 4.4f},
    {"Liquid DnB",         "174 BPM, soft kick, ghost snares, ride, MULTI-TAP echo with DUCK",  presetLiquidDnb, true, pcBreaks, 2.9f},
    {"Boom Bap",           "90 BPM dusty drums, plucked stab, TAPE echo with DRIVE",            presetBoomBap, true, pcHipHop, -2.6f},
    {"Electro Bass",       "808 bass line, rolling hats, cowbell in PING-PONG echo",            presetElectroBass, true, pcElectro, -1.6f},
    {"Reverse Dub",        "Slow dub: rims and bells into a REVERSE echo, long reverb",         presetReverseDub, true, pcAmbient, 6.1f},
    {"Halftime",           "85 BPM halftime: hats at 2x with rolls, deep sine bass",            presetHalftime, true, pcHipHop, 2.9f},
    {"Polymeter",          "Tracks of 16, 13, 11, 9, 7 and 5 steps through a MULTI-TAP echo",   presetPolymeter, true, pcExperimental, 7.3f},
    {"909 Kit",            "Sample kit (909 folder of the library): house groove at 126 BPM",  preset909, true, pcBasics, 0.f, kSmp909},
    {"808 Kit",            "Sample kit (808 folder of the library): electro groove at 104 BPM", preset808, true, pcBasics, 0.f, kSmp808},
    {"R-8 MkII Kit",       "Sample kit (R-8 MkII folder): kick, snares, hats, toms, percussion", presetR8, true, pcBasics, 0.f, kSmpR8},
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// Build preset `i` into a full parameter array + bank.
inline void convertResoV010(float* values, Bank& bank);
// presets whose tracks are all panned to the centre: the percussion (hats, shakers, rims, toms, bells...) is spread
// a little left and right, the kicks, snares, claps and basses stay in the middle
inline void widenIfMono(float* values, const Bank& bank)
{
    bool trigs = false;
    for (const auto& pat : bank.patterns) for (int t = 0; t < kTracks && !trigs; ++t) for (int st = 0; st < kSteps; ++st) if (pat.flags[t][st]) { trigs = true; break; }
    if (!trigs) return;   // Init: base parameters
    for (int t = 0; t < kTracks; ++t) if (bank.machine[t] != mOFF && std::fabs(values[tpIndex(t, tpPAN)] - 64.f) > 0.5f) return;
    static const int spread[] = {-18, 16, -24, 22, -12, 26, -20, 14};
    int k = 0;
    for (int t = 0; t < kTracks; ++t) {
        const int m = bank.machine[t];
        const bool perc = m == mCLHAT || m == mOPHAT || m == mCYMBAL || m == mSHAKER || m == mCLAVE || m == mCOWBELL || m == mRIM ||
                          m == mTOM || m == mFM_HAT || m == mFM_CYMBAL || m == mFM_BELL || m == mFM_RIM || m == mFM_TOM ||
                          m == mPH_METAL || m == mPH_MALLET;
        if (perc) values[tpIndex(t, tpPAN)] = 64.f + (float)spread[k++ % 8];
    }
}
// level trim in dB: master VOLUME first (gain = 1.2 (v / 127)^2), what it cannot give goes to the track levels
inline void applyLevelTrim(float* values, float db)
{
    if (std::fabs(db) < 0.05f) return;
    float& mv = values[kGlobalBase + gVOLUME];
    const float gain = 1.2f * (mv / 127.f) * (mv / 127.f) * std::pow(10.f, db / 20.f);
    float v = 127.f * std::sqrt(gain / 1.2f);
    if (v <= 127.f) { mv = std::round(v); return; }
    mv = 127.f;
    const float extra = std::sqrt(gain / 1.2f);   // track level scale (track gain = (v / 127)^2)
    for (int t = 0; t < kTracks; ++t) { float& tv = values[tpIndex(t, tpVOL)]; tv = std::round(std::min(127.f, tv * extra)); }
}

inline void buildPreset(int i, float* values, Bank& bank)
{
    PB b(values, bank);
    b.g(gTEMPO, 120);
    kPresets[i].build(b);
    if (!kPresets[i].expReso) convertResoV010(values, bank);
    widenIfMono(values, bank);
    applyLevelTrim(values, kPresets[i].trimDb);
}
// the presets voiced with the linear RESONANCE curve used before 0.11: their values (knobs and locks) converted to the
// exponential curve (resoQ) so they keep the same resonance
inline void convertResoV010(float* values, Bank& bank)
{
    auto conv = [](float v, float qMin, float span) {
        const float q = qMin + clampf(v, 0.f, 127.f) / 127.f * span;
        const float e = std::log(q / qMin) / std::log((qMin + span) / qMin);
        return std::round(127.f * std::pow(clampf(e, 0.f, 1.f), 0.8f));
    };
    for (int t = 0; t < kTracks; ++t) values[tpIndex(t, tpRESO)] = conv(values[tpIndex(t, tpRESO)], 0.5f, 12.f);
    values[kGlobalBase + gMF_RESO] = conv(values[kGlobalBase + gMF_RESO], 0.707f, 9.f);
    for (auto& pat : bank.patterns)
        for (int t = 0; t < kTracks; ++t)
            for (int st = 0; st < kSteps; ++st)
                if (pat.locks[t][st][tpRESO] != kNoLock) pat.locks[t][st][tpRESO] = (uint8_t)conv(pat.locks[t][st][tpRESO], 0.5f, 12.f);
}

} // namespace kn
