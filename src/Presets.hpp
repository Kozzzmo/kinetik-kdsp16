// Kinetik KDSP-16 — factory presets (kit + patterns). All original material.
#pragma once
#include "Random.hpp"
#include <cstring>
#include <initializer_list>

namespace kn {

struct PB { // preset builder
    float* v; Bank& b;
    PB(float* values, Bank& bank) : v(values), b(bank)
    {
        for (int t = 0; t < kTracks; ++t) {
            for (int p = 0; p < kTrackParams; ++p) tparam(v, t, p) = p >= 8 ? kFxDefaults[p - 8] : 0.f;
            b.machine[t] = mOFF; b.mute[t] = 0;
        }
        for (int i = 0; i < 20; ++i) v[kGlobalBase + gECHO_TIME + i] = kMasterDefaults[i];
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

static constexpr float kMlSemi = 2.447f;  // PI-ML pitch units per semitone
static constexpr float kKsSemi = 2.022f;  // PI-KS
static constexpr float kMtSemi = 3.08f;   // PI-MT
static constexpr float kSnSemi = 2.13f;   // GND-SN

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
    b.kit(0, mTRX_BD, {46, 70, 60, 35, 70, 0, 20, 30});
    b.kit(1, mTRX_SD, {58, 35, 30, 20, 90, 70, 40, 45});
    b.kit(2, mTRX_CP, {80, 45, 55, 55, 30, 70, 30, 15});
    b.kit(3, mTRX_CH, {70, 20, 85, 90, 30, 64, 0, 0});
    b.kit(4, mTRX_OH, {70, 55, 85, 90, 30, 64, 0, 0});
    b.kit(5, mTRX_CB, {62, 45, 64, 70, 40, 70, 30, 20});
    b.kit(6, mTRX_XT, {40, 60, 50, 40, 40, 5, 0, 10});
    b.kit(7, mTRX_RS, {66, 18, 50, 80, 30, 20, 50, 20});
    b.kit(8, mGND_SN, {30, 50, 30, 40, 70, 0, 0, 40}); b.fx(8, tpFLTW, 60); b.fx(8, tpFLTQ, 50);
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
    b.kit(0, mTRX_BD, {50, 75, 40, 30, 50, 5, 10, 45}); b.fx(0, tpDIST, 15);
    b.kit(1, mTRX_CP, {64, 55, 45, 50, 25, 90, 20, 10}); b.fx(1, tpREV, 45);
    b.kit(2, mTRX_CH, {75, 18, 95, 95, 30, 60, 0, 0});
    b.kit(3, mTRX_OH, {75, 50, 90, 90, 30, 60, 0, 0});
    b.kit(4, mTRX_RS, {75, 15, 30, 90, 20, 15, 60, 10}); b.fx(4, tpDEL, 45);
    b.kit(5, mPI_ML, {70, 45, 75, 40, 85, 50, 20, 0}); b.fx(5, tpDEL, 50); b.fx(5, tpREV, 40);
    b.kit(6, mTRX_CY, {64, 70, 70, 70, 30, 70, 20, 0});
    b.kit(7, mEFM_HH, {90, 20, 0, 64, 90, 100, 60, 100});
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
    b.kit(0, mTRX_BD, {48, 65, 30, 30, 30, 0, 0, 10}); b.fx(0, tpFLTW, 90);
    b.kit(1, mTRX_RS, {55, 25, 70, 50, 20, 20, 40, 0}); b.fx(1, tpDEL, 80); b.fx(1, tpREV, 40);
    b.kit(2, mPI_MT, {60, 55, 40, 30, 60, 60, 40, 0}); b.fx(2, tpDEL, 90); b.fx(2, tpREV, 70);
    b.fx(2, tpFLTF, 30); b.fx(2, tpFLTW, 50); b.fx(2, tpFLTQ, 30);
    b.fx(2, tpLFOS, 15); b.fx(2, tpLFOD, 40); b.fx(2, tpLFOSH, 25); b.fx(2, tpLFODST, lfoDestValue(tpFLTW));
    b.kit(3, mTRX_CH, {60, 25, 70, 60, 20, 64, 30, 0}); b.fx(3, tpVOL, 80);
    b.kit(4, mGND_NS, {60, 0, 127, 70, 100, 0, 30, 0}); b.fx(4, tpVOL, 45);
    b.kit(5, mTRX_SD, {50, 30, 20, 20, 60, 50, 40, 60}); b.fx(5, tpREV, 70);
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
    b.kit(0, mTRX_BD, {55, 55, 45, 25, 80, 5, 30, 50}); b.fx(0, tpSRR, 10);
    b.kit(1, mTRX_SD, {62, 38, 40, 15, 100, 60, 60, 40}); b.fx(1, tpDIST, 20);
    b.kit(2, mTRX_CH, {70, 22, 75, 70, 30, 64, 0, 20});
    b.kit(3, mTRX_CY, {60, 85, 50, 60, 20, 70, 40, 10});
    b.kit(4, mTRX_RS, {60, 20, 40, 70, 30, 20, 40, 20});
    b.kit(5, mEFM_SD, {80, 25, 90, 25, 30, 50, 20, 80}); b.fx(5, tpVOL, 80);
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
    b.kit(0, mEFM_BD, {50, 50, 70, 25, 30, 32, 20, 10});
    b.kit(1, mTRX_SD, {70, 30, 50, 15, 100, 75, 60, 35});
    b.kit(2, mEFM_SD, {85, 20, 80, 20, 50, 60, 20, 90});
    b.kit(3, mEFM_HH, {95, 15, 0, 64, 90, 110, 40, 100});
    b.kit(4, mPI_KS, {30, 60, 60, 40, 50, 0, 40, 30}); b.fx(4, tpFLTW, 80); b.fx(4, tpFLTQ, 40);
    b.kit(5, mTRX_CY, {64, 70, 60, 70, 30, 64, 30, 0});
    b.kit(6, mPI_SD, {80, 30, 80, 50, 70, 40, 50, 20});
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
    b.kit(0, mEFM_BD, {45, 40, 90, 20, 60, 70, 15, 40}); b.fx(0, tpSRR, 20);
    b.kit(1, mEFM_RS, {80, 25, 80, 90, 15, 40, 10, 60}); b.fx(1, tpDEL, 40);
    b.kit(2, mPI_KS, {60, 35, 90, 20, 30, 50, 80, 20}); b.fx(2, tpREV, 50);
    b.kit(3, mGND_NS, {0, 0, 10, 90, 127, 40, 90, 0}); b.fx(3, tpPAN, 30);
    b.kit(4, mEFM_HH, {100, 10, 60, 100, 100, 120, 30, 110}); b.fx(4, tpPAN, 100);
    b.kit(5, mPI_MT, {90, 40, 90, 110, 50, 90, 100, 10}); b.fx(5, tpSRR, 40); b.fx(5, tpDEL, 50);
    b.kit(6, mEFM_CB, {40, 30, 70, 70, 110, 90, 30, 40});
    b.kit(7, mGND_SN, {20, 30, 100, 15, 90, 0, 0, 60}); b.fx(7, tpDIST, 40);
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
            if (b.b.patterns[0].flags[t][s] && r.chance(0.5f)) b.lock(0, t, s, r.range(0, 3), r.range(10, 117));
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
            if (b.b.patterns[1].flags[t][s] && r.chance(0.6f)) b.lock(1, t, s, r.range(0, 4), r.range(10, 117));
    b.settings(1, 15, 50, 0, 90);
    b.g(gTEMPO, 104);
}

static void presetIndustrial(PB& b)
{
    b.kit(0, mPI_BD, {35, 70, 80, 70, 30, 70, 40, 90}); b.fx(0, tpDIST, 60);
    b.kit(1, mEFM_CP, {60, 60, 80, 40, 110, 90, 70, 110}); b.fx(1, tpREV, 50);
    b.kit(2, mPI_MT, {40, 60, 110, 100, 30, 100, 90, 60}); b.fx(2, tpDIST, 40);
    b.kit(3, mGND_NS, {0, 0, 30, 60, 90, 60, 100, 60});
    b.kit(4, mEFM_HH, {60, 20, 80, 30, 110, 70, 40, 60});
    b.kit(5, mTRX_XT, {30, 50, 90, 30, 90, 40, 80, 100});
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
    b.kit(0, mTRX_BD, {52, 45, 30, 20, 40, 0, 0, 10});
    b.kit(1, mGND_SN, {110, 5, 60, 5, 0, 0, 0, 30}); b.fx(1, tpPAN, 40);
    b.kit(2, mTRX_CL, {75, 15, 40, 80, 40, 0, 10, 0}); b.fx(2, tpPAN, 90); b.fx(2, tpDEL, 40);
    b.kit(3, mTRX_MA, {5, 5, 25, 90, 110, 60, 80, 0});
    b.kit(4, mTRX_RS, {70, 12, 30, 90, 10, 10, 70, 0}); b.fx(4, tpDEL, 30);
    b.kit(5, mPI_ML, {75, 30, 90, 60, 100, 30, 0, 0}); b.fx(5, tpREV, 40);
    b.kit(6, mTRX_CP, {50, 30, 40, 70, 30, 40, 30, 0}); b.fx(6, tpVOL, 80);
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
    b.kit(0, mTRX_BD, {50, 50, 30, 30, 50, 10, 20, 30}); b.fx(0, tpSRR, 35); b.fx(0, tpFLTW, 80);
    b.kit(1, mTRX_SD, {48, 35, 20, 20, 80, 45, 50, 40}); b.fx(1, tpSRR, 35); b.fx(1, tpFLTW, 85); b.fx(1, tpREV, 30);
    b.kit(2, mTRX_CH, {55, 20, 60, 50, 20, 64, 10, 0}); b.fx(2, tpSRR, 30); b.fx(2, tpFLTW, 90);
    b.kit(3, mPI_KS, {25, 50, 40, 60, 60, 0, 10, 10}); b.fx(3, tpFLTW, 70);
    b.kit(4, mGND_NS, {0, 0, 127, 50, 70, 0, 60, 0}); b.fx(4, tpVOL, 35);
    b.kit(5, mTRX_RS, {50, 15, 70, 50, 20, 20, 30, 0});
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
    b.kit(0, mEFM_BD, {48, 55, 60, 30, 50, 40, 25, 30});
    b.kit(1, mEFM_SD, {70, 35, 80, 35, 60, 55, 25, 70});
    b.kit(2, mEFM_HH, {85, 20, 30, 80, 90, 100, 50, 90});
    b.kit(3, mEFM_CB, {60, 45, 50, 30, 60, 70, 40, 70});
    b.kit(4, mGND_SN, {35, 45, 20, 30, 100, 0, 0, 50}); b.fx(4, tpFLTW, 55); b.fx(4, tpFLTQ, 70);
    b.kit(5, mEFM_XT, {50, 50, 50, 40, 50, 40, 30, 40});
    b.kit(6, mEFM_CY, {64, 70, 80, 30, 90, 90, 80, 70});
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
    b.kit(0, mPI_BD, {45, 50, 40, 30, 40, 50, 0, 0});
    b.kit(1, mPI_ML, {70, 55, 60, 30, 20, 60, 50, 0}); b.fx(1, tpREV, 40);
    b.kit(2, mTRX_XT, {80, 40, 20, 30, 50, 15, 0, 0});  // conga hi
    b.kit(3, mTRX_XT, {62, 45, 20, 30, 50, 15, 0, 0});  // conga lo
    b.kit(4, mTRX_CL, {70, 25, 50, 70, 40, 5, 10, 0});
    b.kit(5, mTRX_MA, {15, 10, 30, 80, 110, 70, 90, 0});
    b.kit(6, mTRX_CB, {70, 40, 60, 75, 50, 60, 20, 0});
    b.kit(7, mPI_MT, {75, 60, 60, 50, 40, 60, 50, 0}); b.fx(7, tpREV, 60);
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
    b.kit(0, mTRX_BD, {48, 62, 35, 30, 45, 0, 5, 25});
    b.kit(1, mTRX_CP, {64, 45, 50, 60, 30, 70, 30, 10}); b.fx(1, tpREV, 30);
    b.kit(2, mTRX_CH, {72, 15, 90, 85, 25, 64, 0, 0});
    b.kit(3, mTRX_OH, {72, 40, 88, 85, 25, 64, 0, 0});
    b.kit(4, mTRX_RS, {70, 15, 40, 80, 20, 15, 50, 10});
    b.kit(5, mTRX_MA, {8, 5, 25, 85, 110, 40, 80, 0}); b.fx(5, tpVOL, 80);
    b.kit(6, mPI_ML, {66, 40, 70, 45, 75, 55, 30, 0}); b.fx(6, tpDEL, 45); b.fx(6, tpREV, 35);
    b.kit(7, mGND_SN, {34, 45, 15, 30, 40, 0, 0, 30}); b.fx(7, tpFLTW, 70);
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
    b.kit(0, mTRX_BD, {50, 55, 40, 25, 60, 0, 10, 20});
    b.kit(1, mTRX_SD, {60, 30, 30, 15, 90, 80, 50, 30}); b.fx(1, tpREV, 25);
    b.kit(2, mTRX_CH, {78, 12, 95, 90, 20, 60, 0, 0});
    b.kit(3, mTRX_RS, {72, 14, 40, 85, 25, 15, 55, 10});
    b.kit(4, mGND_SN, {28, 70, 25, 30, 60, 0, 0, 40}); b.fx(4, tpFLTW, 60); b.fx(4, tpFLTQ, 40);
    b.kit(5, mEFM_CB, {75, 25, 40, 20, 40, 60, 30, 90}); b.fx(5, tpDEL, 40);
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
    b.kit(0, mTRX_BD, {44, 80, 60, 30, 50, 0, 20, 40});
    b.kit(1, mTRX_CP, {50, 35, 35, 65, 30, 40, 20, 20});
    b.kit(2, mTRX_CH, {80, 10, 95, 90, 20, 60, 0, 0});
    b.kit(3, mTRX_XT, {55, 45, 40, 30, 50, 10, 0, 10});
    b.kit(4, mTRX_RS, {75, 12, 40, 90, 20, 15, 50, 10});
    b.kit(5, mPI_KS, {45, 45, 70, 30, 40, 0, 30, 20}); b.fx(5, tpDEL, 30);
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
    b.kit(0, mGND_SN, {14, 100, 40, 40, 5, 0, 20, 50}); b.fx(0, tpDIST, 20); // 808 boom
    b.kit(1, mTRX_SD, {66, 30, 30, 15, 100, 70, 50, 30}); b.fx(1, tpREV, 30);
    b.kit(2, mTRX_CP, {80, 30, 30, 60, 30, 40, 20, 20});
    b.kit(3, mTRX_CH, {85, 10, 95, 95, 20, 60, 0, 0});
    b.kit(4, mTRX_OH, {80, 35, 90, 90, 20, 60, 0, 0});
    b.kit(5, mTRX_BD, {55, 25, 50, 15, 80, 0, 20, 30});
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
    b.kit(0, mTRX_BD, {50, 60, 40, 25, 60, 0, 10, 35});
    b.kit(1, mTRX_SD, {62, 30, 25, 15, 95, 75, 45, 25});
    b.kit(2, mTRX_RS, {70, 15, 50, 80, 20, 15, 50, 10});
    b.kit(3, mTRX_CH, {75, 15, 90, 85, 25, 64, 0, 0});
    b.kit(4, mTRX_XT, {70, 35, 30, 30, 40, 10, 0, 0});
    b.kit(5, mTRX_MA, {8, 5, 20, 85, 110, 50, 90, 0});
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
    b.kit(0, mTRX_BD, {52, 45, 45, 25, 80, 5, 30, 40}); b.fx(0, tpSRR, 15);
    b.kit(1, mTRX_SD, {68, 32, 45, 15, 100, 65, 60, 35}); b.fx(1, tpSRR, 15);
    b.kit(2, mTRX_CH, {72, 15, 85, 70, 20, 60, 0, 10}); b.fx(2, tpSRR, 15);
    b.kit(3, mTRX_CY, {64, 80, 60, 60, 20, 70, 40, 0}); b.fx(3, tpSRR, 15);
    b.kit(4, mGND_SN, {14, 110, 30, 50, 0, 0, 30, 30}); b.fx(4, tpVOL, 105);
    b.kit(5, mPI_MT, {55, 70, 50, 80, 40, 70, 70, 0}); b.fx(5, tpDEL, 60); b.fx(5, tpREV, 60);
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
    b.kit(0, mPI_BD, {42, 55, 50, 40, 40, 50, 0, 10});
    b.kit(1, mPI_ML, {70, 45, 60, 35, 30, 60, 40, 0}); b.fx(1, tpDEL, 35); b.fx(1, tpPAN, 40);
    b.kit(2, mPI_ML, {58, 50, 50, 35, 70, 30, 30, 0}); b.fx(2, tpDEL, 35); b.fx(2, tpPAN, 88);
    b.kit(3, mTRX_CL, {72, 25, 50, 70, 40, 5, 10, 0});
    b.kit(4, mTRX_MA, {10, 8, 25, 85, 110, 60, 80, 0});
    b.kit(5, mPI_MT, {80, 60, 70, 60, 40, 70, 50, 0}); b.fx(5, tpREV, 60);
    b.kit(6, mEFM_RS, {75, 18, 50, 70, 15, 20, 10, 30});
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
    b.kit(0, mPI_BD, {38, 80, 20, 30, 70, 30, 0, 0}); b.fx(0, tpVOL, 90);
    b.kit(1, mPI_MT, {60, 110, 30, 50, 50, 70, 60, 0}); b.fx(1, tpREV, 100); b.fx(1, tpDEL, 70);
    b.kit(2, mPI_ML, {75, 100, 20, 20, 90, 40, 80, 0}); b.fx(2, tpREV, 90); b.fx(2, tpDEL, 60);
    b.kit(3, mGND_NS, {90, 60, 110, 70, 90, 40, 0, 0}); b.fx(3, tpVOL, 60); b.fx(3, tpREV, 80);
    b.fx(3, tpLFOS, 10); b.fx(3, tpLFOD, 60); b.fx(3, tpLFOSH, 25); b.fx(3, tpLFODST, lfoDestValue(tpFLTW));
    b.kit(4, mPI_KS, {50, 100, 40, 50, 30, 10, 10, 0}); b.fx(4, tpREV, 70); b.fx(4, tpDEL, 50);
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

struct PresetDef { const char* name; const char* desc; void (*build)(PB&); };

static const PresetDef kPresets[] = {
    {"Init",               "Factory state: default kit, every parameter at its base, empty sequencer", presetInit},
    {"Demo Groove",        "The default kit with a basic groove to start from",            presetDefault},
    {"Electro Funk",       "808-style electro: syncopated kicks, cowbell, sine bass",       presetElectro},
    {"Detroit Techno",     "Driving 4/4, clap, ride-like FM hats, mallet chord stabs",       presetTechno},
    {"Dub Techno",         "Deep kick, echoing rims and bell chords, long reverb, hiss",     presetDubTechno},
    {"Breakbeat",          "Punchy break with ghost snares, compressed and saturated",       presetBreakbeat},
    {"Drill & Bass",       "172 BPM, 32 steps, snare rolls with rising pitch, plucked bass", presetDrillBass},
    {"IDM Glitch",         "Odd lengths (13 / 15 steps), FM and physical machines, locks",   presetIdm},
    {"Industrial",         "Distorted membrane kick, metal hits, crushed noise, gated verb", presetIndustrial},
    {"Minimal Click",      "Tiny clicks, claves, shaker, sparse mallets",                    presetMinimal},
    {"Lo-Fi Hip-Hop",      "Swung, bit-reduced drums, plucked bassline, vinyl hiss",         presetLofi},
    {"Braindance",         "All-FM kit, acid-like sliding sine line, busy hats",             presetBraindance},
    {"Percussion Ensemble","Marimba, congas, claves, shaker, bells",                         presetPercussion},
    {"Deep House",         "Classic 4/4, offbeat hats, shaker, mallet chords, sine bass",    presetHouse},
    {"UK Garage",          "Swung 2-step, rim shots, bouncing sine bass",                    presetGarage},
    {"Footwork",           "160 BPM, triplet-ish kicks, rolling hats, plucked stabs",        presetFootwork},
    {"Trap",               "Pitched sine 808 boom, hi-hat rolls, half-time snare",           presetTrap},
    {"Dembow",             "Reggaeton / dembow groove with toms and shaker",                 presetDembow},
    {"Jungle",             "168 BPM chopped break feel, sub bass, dubby metal stabs",        presetJungle},
    {"Polyrhythm Lab",     "Every track has its own length (5 to 16 steps): ever-changing", presetPolyLab},
    {"Ambient Bells",      "Slow, long track lengths, bells, plucks, washed-out echo",       presetAmbient},
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// Build preset `i` into a full parameter array + bank.
inline void buildPreset(int i, float* values, Bank& bank)
{
    PB b(values, bank);
    b.g(gTEMPO, 120);
    kPresets[i].build(b);
}

} // namespace kn
