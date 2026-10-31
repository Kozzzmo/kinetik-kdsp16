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

#include "PresetsV2.hpp"

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
    // 909 folder: 0 BD 1 SD 2 CLAP 3 CH 4 OH 5 LT 6 MT 7 HT 8 RIM 9 RIDE 10 CRASH. 130 BPM, 4 bars, swing 59.
    sampleKit(b, kSmp909);
    PB2 q(b);
    b.g(gTEMPO, 130);
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kBars({"....x.......x...", "....x.......x..x", "....x.......x...", "....x..x.x..xxxx"}));
    q.cyc(0, 1, tpVOL, {84, 84, 84, 60, 84, 84, 84, 84, 70, 80, 90, 100});
    q.row(0, 2, kRep("....x.......x...", 4)); q.cond(0, 2, {4, 36}, 13);
    q.row(0, 3, kRep("x.xxx.xxx.xxx.xx", 4)); q.cyc(0, 3, tpVOL, {82, 56, 70, 56}); q.cond(0, 3, {3, 19, 35, 51}, 3);
    q.row(0, 4, kRep("..x...x...x...x.", 4));
    q.row(0, 5, kBars({"................", "................", "................", "..........x...x."}));
    q.row(0, 6, kBars({"................", "................", "................", "......x.....x..."}));
    q.row(0, 7, kBars({"................", "...........x....", "................", "..x.....x......."}));
    q.row(0, 8, kRep("...x......x.....", 4)); q.cond(0, 8, {10, 42}, 3);
    q.row(0, 9, kRep("x.x.x.x.x.x.x.x.", 4)); q.cyc(0, 9, tpVOL, {66, 50});
    q.row(0, 10, "x"); b.b.patterns[0].trackLen[10] = 64;
    q.pattern(0, 64, 59);
    q.copy(0, 1);                                   // B: no kick for 2 bars, snare roll
    q.row(1, 0, kBars({"x...x...x...x...", "x...x...x...x...", "................", "................"}));
    q.row(1, 1, kBars({"....x.......x...", "....x.......x...", "x...x...x...x...", "x.x.x.x.xxxxxxxx"})); b.ratch(1, 1, 63, 3);
    q.pattern(1, 64, 59);
    q.send(2, 0, 30); q.send(8, 30, 10); q.send(9, 0, 30); q.send(10, 0, 40); q.send(5, 20, 20); q.send(6, 20, 20); q.send(7, 20, 20);
    q.echo(etv8D, 45, emvTape, 30, 50); q.verb(55, 60, rmvPlate, 30, 55);
    q.vol({{0, 100}, {1, 84}, {2, 92}, {3, 76}, {4, 80}, {5, 92}, {6, 92}, {7, 92}, {8, 64}, {9, 58}, {10, 70}});
    q.pan({{3, 54}, {4, 74}, {5, 44}, {6, 64}, {7, 84}, {8, 52}, {9, 80}, {10, 50}});
    q.chain("1 2 0:3 1:1");
    q.mix({0.0f, 3.8f, 6.8f, -2.2f, -3.1f, -3.1f, -3.8f, -3.0f, 3.4f, -1.9f, -1.5f});
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
    {"Init",            "Factory state: default kit, every parameter at its base, empty sequencer", presetInit, true, pcBasics, 0.0f},
    {"Demo Groove",     "The default kit with a basic two-bar groove to start from",              v2Demo, true, pcBasics, 6.0f},
    {"Warehouse",       "132 BPM peak-time techno: rumble, ride LFO, breakdown and fill in a CHAIN", v2Warehouse, true, pcTechno, -5.5f},
    {"Hard Groove",     "136 BPM: FM kick, snare rolls over 4 bars, 3-against-4 FM hats, metal",  v2HardGroove, true, pcTechno, 0.0f},
    {"Basik Teck",      "125 BPM tool: kick, clap, hats, a tom figure in a tape echo",            v2BasikTeck, true, pcTechno, 6.9f},
    {"Dub Pressure",    "122 BPM dub techno: mallet chords and rims in tape echo, long hall, hiss", v2DubPressure, true, pcTechno, 1.4f},
    {"Industrial",      "140 BPM: distorted membrane kick, crushed snare, metal clangs, gated room", v2Industrial, true, pcTechno, -5.7f},
    {"Minimal Groove",  "126 BPM: clicks, claves, woody bass; 12- and 56-step tracks drift",      v2Minimal, true, pcTechno, 4.0f},
    {"K-House",         "128 BPM house: reverb clap, moving shaker, noise sweep, sine bass, toms", v2KHouse, true, pcHouse, -0.3f},
    {"Deep House",      "122 BPM: soft kick, swung shaker, mallet chords with a slow filter",     v2DeepHouse, true, pcHouse, 0.7f},
    {"Chicago Jack",    "126 BPM jacking 4/4: cowbell, rim shuffle, tom line, bouncing sine",     v2Chicago, true, pcHouse, -3.7f},
    {"UK Garage",       "132 BPM swung 2-step: shuffled hats, rim ghosts, bouncing bass",         v2Garage, true, pcHouse, 4.7f},
    {"Afro House",      "122 BPM: congas, 3-2 clave, shaker at 3/2x, bells in the chorus",        v2AfroHouse, true, pcPerc, 6.0f},
    {"Breakbeat",       "130 BPM break: ghost snares, ratchets, sub, tom fill",                   v2Breakbeat, true, pcBreaks, -1.1f},
    {"Jungle",          "170 BPM chopped break, rising snare roll, sub, dub stabs; CHAIN",        v2Jungle, true, pcBreaks, -2.4f},
    {"Liquid",          "174 BPM: soft kick, ghost snares, ride, mallets in a ducking MULTI-TAP", v2Liquid, true, pcBreaks, 4.4f},
    {"Footwork",        "160 BPM: triplet-ish kicks, hat rolls, sliding sine, plucked stabs",     v2Footwork, true, pcBreaks, -1.6f},
    {"Boom Bap",        "90 BPM dusty drums, late snare, plucked chords, TAPE echo with DRIVE",   v2BoomBap, true, pcHipHop, 5.4f},
    {"Trap",            "140 BPM: pitched sine boom, hat rolls up to 6, half-time clap",          v2Trap, true, pcHipHop, 1.2f},
    {"Lo-Fi Dust",      "82 BPM: crushed drums, wobbling mallets, vinyl hiss, round bass",        v2LofiDust, true, pcHipHop, 1.6f},
    {"Electro",         "128 BPM: syncopated kick, sine bass with an LFO filter, cowbell, toms",  v2Electro, true, pcElectro, 3.0f},
    {"Braindance",      "140 BPM all-FM kit, sliding 13-step acid sine, ratcheted hats",          v2Braindance, true, pcElectro, 9.1f},
    {"Polymeter",       "118 BPM: tracks of 16, 13, 11, 9, 7, 5 steps through a MULTI-TAP echo",  v2Polymeter, true, pcExperimental, 1.4f},
    {"Chaos Breaks",    "172 BPM break; CHAOS cuts up only the snares, hats and rims",            v2ChaosBreaks, true, pcExperimental, -2.5f},
    {"Ambient Metals",  "90 BPM: metals, mallets, strings and bells into a shimmer reverb",       v2AmbientMetals, true, pcAmbient, 8.2f},
    {"909 Kit",         "Sample kit (909 folder): 130 BPM, 4 bars, snare roll and breakdown in a CHAIN",  preset909, true, pcBasics, 4.5f, kSmp909},
    {"808 Kit",         "Sample kit (808 folder of the library): electro groove at 104 BPM", preset808, true, pcBasics, 3.5f, kSmp808},
    {"R-8 MkII Kit",    "Sample kit (R-8 MkII folder): kick, snares, hats, toms, percussion", presetR8, true, pcBasics, 4.5f, kSmpR8},
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

inline void keepKickLevel(float* values, Bank& bank);
inline void buildPreset(int i, float* values, Bank& bank)
{
    PB b(values, bank);
    b.g(gTEMPO, 120);
    kPresets[i].build(b);
    if (!kPresets[i].expReso) convertResoV010(values, bank);
    widenIfMono(values, bank);
    applyLevelTrim(values, kPresets[i].trimDb);
    if (i != 0) keepKickLevel(values, bank);
}
// the KICK machine is 3.5 dB louder since 0.19.1: the presets voiced before keep their balance (VOL = v^2: v x 10^(-dB/40))
inline void keepKickLevel(float* values, Bank& bank)
{
    const float k = std::pow(10.f, -kKickBoostDb / 40.f);
    for (int t = 0; t < kTracks; ++t) {
        if (bank.machine[t] != mKICK) continue;
        values[tpIndex(t, tpVOL)] *= k;
        for (auto& pat : bank.patterns)
            for (int st = 0; st < kSteps; ++st)
                if (pat.locks[t][st][tpVOL] != kNoLock) pat.locks[t][st][tpVOL] = (uint8_t)std::lround(pat.locks[t][st][tpVOL] * k);
    }
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
