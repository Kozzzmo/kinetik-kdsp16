// Kinetik KDSP-16 — shared definitions (DSP + UI)
#pragma once
#include <cstdint>
#include <cstdio>

namespace kn {

static constexpr int kTracks = 16;
static constexpr int kSteps = 64;
static constexpr int kPatterns = 16;

// ---- per-track parameters (all 0..127 like the original hardware) ----
enum TrackParam {
    tpSyn0 = 0, // 0..7 synthesis params (depend on machine)
    tpAMD = 8, tpAMF, tpEQF, tpEQG, tpFLTF, tpFLTW, tpFLTQ, tpSRR,
    tpDIST = 16, tpVOL, tpPAN, tpDEL, tpREV, tpFENV, tpFDEC, tpVEL,
    tpLFOS = 24, tpLFOD, tpLFOSH, tpLFOT, tpLFODST, tpLFOTRK,       // LFO 1
    tpLFO2S = 30, tpLFO2D, tpLFO2SH, tpLFO2T, tpLFO2DST, tpLFO2TRK,  // LFO 2
    kLockable = 36,          // every track parameter can be p-locked
    kTrackParams = 36        // machine, mute and the TRACK page are kit settings (stored in the Bank)
};
static constexpr int kLfoBase[2] = {tpLFOS, tpLFO2S};
static constexpr int kLfoDests = 24; // an LFO can move params 0..23

static const char* const kFxNames[28] = {
    "AMD", "AMF", "EQF", "EQG", "FLTF", "FLTW", "FLTQ", "SRR",
    "DIST", "VOL", "PAN", "DEL", "REV", "FENV", "FDEC", "VEL",
    "SPD", "DEP", "SHP", "TYP", "DEST", "TRK",
    "SPD", "DEP", "SHP", "TYP", "DEST", "TRK"
};
static const uint8_t kFxDefaults[28] = {
    0, 64, 64, 64, 0, 127, 0, 0,
    0, 100, 64, 0, 0, 64, 40, 127,
    64, 0, 0, 0, 0, 0,
    40, 0, 0, 0, 0, 0
};

// ---- kit settings shown on the TRACK page (not host parameters) ----
enum KitField { kfChoke = 0, kfOut, kfMidi, kfFmq, kfOsc, kKitFields };
static const char* const kKitNames[kKitFields] = {"CHOKE", "OUT", "MIDI", "FM-Q", "OSC"};
static const int kKitMax[kKitFields] = {4, 3, 16, 1, 2};
inline const char* kitValueText(int field, int v, char* buf, int n)
{
    static const char* choke[5] = {"OFF", "A", "B", "C", "D"};
    static const char* out[4] = {"MAIN", "OUT 2", "OUT 3", "OUT 4"};
    static const char* osc[3] = {"SINE", "TRI", "SQR"};
    switch (field) {
    case kfChoke: return choke[v < 0 ? 0 : (v > 4 ? 4 : v)];
    case kfOut: return out[v < 0 ? 0 : (v > 3 ? 3 : v)];
    case kfMidi: if (v <= 0) return "OFF"; std::snprintf(buf, n, "CH %d", v); return buf;
    case kfFmq: return v ? "ON" : "OFF";
    default: return osc[v < 0 ? 0 : (v > 2 ? 2 : v)];
    }
}

// ---- per-step extras ----
static constexpr int kRatchCount = 6;
static const uint8_t kRatchValues[kRatchCount] = {1, 2, 3, 4, 6, 8};
enum CondKind { ckAlways, ckProb, ckFill, ckNotFill, ckPre, ckNotPre, ckFirst, ckNotFirst, ckRatio };
struct CondDef { const char* name; CondKind kind; int a, b; };
static const CondDef kConds[] = {
    {"", ckAlways, 0, 0},
    {"90%", ckProb, 90, 0}, {"75%", ckProb, 75, 0}, {"50%", ckProb, 50, 0}, {"33%", ckProb, 33, 0},
    {"25%", ckProb, 25, 0}, {"10%", ckProb, 10, 0},
    {"FILL", ckFill, 0, 0}, {"!FILL", ckNotFill, 0, 0}, {"PRE", ckPre, 0, 0}, {"!PRE", ckNotPre, 0, 0},
    {"1ST", ckFirst, 0, 0}, {"!1ST", ckNotFirst, 0, 0},
    {"1:2", ckRatio, 1, 2}, {"2:2", ckRatio, 2, 2}, {"1:3", ckRatio, 1, 3}, {"2:3", ckRatio, 2, 3},
    {"3:3", ckRatio, 3, 3}, {"1:4", ckRatio, 1, 4}, {"2:4", ckRatio, 2, 4}, {"3:4", ckRatio, 3, 4},
    {"4:4", ckRatio, 4, 4},
};
static constexpr int kNumConds = (int)(sizeof(kConds) / sizeof(kConds[0]));
static constexpr int kMicroMax = 11; // micro-timing in 1/24 of a step

// ---- global parameters ----
enum GlobalParam {
    gPATTERN = 0, gPLAY, gTEMPO,
    gECHO_TIME, gECHO_FB, gECHO_LPF, gECHO_HPF, gECHO_WIDTH, gECHO_LEV,
    gREV_SIZE, gREV_DAMP, gREV_GATE, gREV_HPF, gREV_PRE, gREV_LEV,
    gEQ_LOW, gEQ_HIGH, gCMP_THR, gCMP_RAT, gCMP_ATK, gCMP_REL, gDRIVE, gVOLUME,
    gOUT_STEP, gOUT_LEDS, gOUT_PATTERN,
    kGlobalParams
};

static constexpr int kGlobalBase = kTracks * kTrackParams;
static constexpr int kTotalParams = kGlobalBase + kGlobalParams;

static const char* const kMasterNames[20] = {
    "TIME", "FB", "LPF", "HPF", "WIDTH", "LEV",
    "DECAY", "DAMP", "GATE", "HPF", "PRE", "LEV",
    "LOW", "HIGH", "THRS", "RATIO", "ATK", "REL", "DRIVE", "VOL"
};
static const uint8_t kMasterDefaults[20] = {
    48, 50, 100, 10, 64, 64,
    60, 64, 0, 20, 10, 64,
    64, 64, 127, 40, 20, 50, 0, 100
};

// ---- machines ----
enum Family { fOff, fTone, fSnare, fClap, fMetal, fShaker, fFM, fModal, fString, fNoise };

struct MachineDef {
    const char* name;
    Family family;
    const char* p[8];
    uint8_t def[8];
};

enum MachineId {
    mOFF, mTRX_BD, mTRX_SD, mTRX_XT, mTRX_CP, mTRX_RS, mTRX_CB, mTRX_CH, mTRX_OH, mTRX_CY,
    mTRX_MA, mTRX_CL,
    mEFM_BD, mEFM_SD, mEFM_XT, mEFM_CP, mEFM_RS, mEFM_CB, mEFM_HH, mEFM_CY,
    mPI_BD, mPI_SD, mPI_MT, mPI_ML, mPI_KS,
    mGND_SN, mGND_NS,
    kMachines
};

static const MachineDef kMachineDefs[kMachines] = {
    {"OFF", fOff,   {"-","-","-","-","-","-","-","-"},                         {0,0,0,0,0,0,0,0}},
    {"BD", fTone,   {"PTCH","DEC","RAMP","RDEC","STRT","NOIS","HARM","CLIP"}, {52,60,50,30,60,0,0,20}},
    {"SD", fSnare,  {"PTCH","DEC","BUMP","BENV","SNAP","TONE","TUNE","NDEC"}, {50,40,40,20,80,64,50,50}},
    {"XT", fTone,   {"PTCH","DEC","RAMP","RDEC","STRT","NOIS","HARM","CLIP"}, {50,60,40,40,40,10,0,10}},
    {"CP", fClap,   {"CLPY","DEC","SPRD","TONE","RES","TAIL","RAND","DRIV"}, {64,50,50,60,30,80,20,10}},
    {"RS", fSnare,  {"PTCH","DEC","BODY","CLIK","NOIS","NDEC","HPF","CLIP"},  {60,20,50,80,40,20,40,20}},
    {"CB", fMetal,  {"PTCH","DEC","DTUN","BPF","RES","SHRT","ATAK","CLIP"},  {60,50,64,64,40,64,30,30}},
    {"CH", fMetal,  {"PTCH","DEC","HPF","METL","RES","SPRD","ATAK","CLIP"},  {64,30,80,80,30,64,0,0}},
    {"OH", fMetal,  {"PTCH","DEC","HPF","METL","RES","SPRD","ATAK","CLIP"},  {64,70,80,80,30,64,0,0}},
    {"CY", fMetal,  {"PTCH","DEC","HPF","METL","RES","SPRD","BELL","CLIP"},  {64,80,60,70,30,64,30,0}},
    {"MA", fShaker, {"ATK","HOLD","DEC","HPF","LPF","GRN","RATE","CLIP"},    {10,20,40,80,110,40,64,0}},
    {"CL", fTone,   {"PTCH","DEC","CLIK","RING","RLEV","NOIS","NDEC","CLIP"}, {64,30,60,64,40,10,20,0}},
    {"EFM-BD", fFM,     {"PTCH","DEC","RAMP","RDEC","MOD","MFRQ","MDEC","FDBK"}, {52,60,50,30,40,32,30,0}},
    {"EFM-SD", fFM,     {"PTCH","DEC","NOIS","NDEC","MOD","MFRQ","MDEC","HPF"},  {64,40,70,40,50,50,30,60}},
    {"EFM-XT", fFM,     {"PTCH","DEC","RAMP","RDEC","MOD","MFRQ","MDEC","CLIK"}, {60,60,40,40,40,40,30,30}},
    {"EFM-CP", fFM,     {"PTCH","DEC","CLPS","SPRD","MOD","MFRQ","MDEC","FDBK"}, {70,50,64,50,90,80,60,90}},
    {"EFM-RS", fFM,     {"PTCH","DEC","MOD","MFRQ","MDEC","NOIS","NDEC","FDBK"}, {70,20,60,60,20,30,15,20}},
    {"EFM-CB", fFM,     {"PTCH","DEC","SNAP","FDBK","MOD","MFRQ","MDEC","TONE"}, {64,50,40,10,50,70,50,64}},
    {"EFM-HH", fFM,     {"PTCH","DEC","TREM","TFRQ","MOD","MFRQ","MDEC","HPF"},  {80,30,0,64,90,100,60,90}},
    {"EFM-CY", fFM,     {"PTCH","DEC","FDBK","BELL","MOD","MFRQ","MDEC","HPF"},  {64,80,80,30,90,90,80,70}},
    {"PI-BD",  fModal,  {"PTCH","DEC","HARD","TENS","DAMP","MODE","NOIS","CLIP"}, {40,60,50,40,40,50,10,10}},
    {"PI-SD",  fModal,  {"PTCH","DEC","HARD","TENS","SNAR","SNDC","MODE","CLIP"}, {64,40,70,30,80,50,50,10}},
    {"PI-MT",  fModal,  {"PTCH","DEC","HARD","INHR","DAMP","MODE","SPRD","CLIP"}, {70,70,80,64,40,64,64,0}},
    {"PI-ML",  fModal,  {"PTCH","DEC","HARD","DAMP","MAT","POS","RING","CLIP"},   {64,50,60,40,40,64,30,0}},
    {"PI-KS",  fString, {"PTCH","DEC","BRIT","PICK","DAMP","STIF","NOIS","CLIP"}, {64,70,70,50,40,0,50,0}},
    {"SINE", fTone,   {"PTCH","DEC","RAMP","RDEC","WAVE","ATK","HOLD","CLIP"}, {64,60,0,30,0,0,0,0}},
    {"NOISE", fNoise,  {"ATK","HOLD","DEC","HPF","LPF","RES","CRSH","CLIP"},    {0,0,60,0,127,0,0,0}},
};

static const uint8_t kDefaultKit[kTracks] = {
    mTRX_BD, mTRX_SD, mTRX_XT, mTRX_XT, mTRX_CP, mTRX_RS, mTRX_CB, mTRX_CH,
    mTRX_OH, mTRX_CY, mEFM_BD, mEFM_SD, mEFM_HH, mPI_MT, mPI_ML, mGND_NS
};
// per-track synth overrides for the default kit (pitch for the toms etc.); 255 = machine default
static const uint8_t kDefaultKitPitch[kTracks] = {
    255, 255, 70, 45, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

inline uint8_t trackParamDefault(int track, int p)
{
    const int m = kDefaultKit[track];
    if (p < 8) {
        if (p == 0 && kDefaultKitPitch[track] != 255) return kDefaultKitPitch[track];
        return kMachineDefs[m].def[p];
    }
    if (p < kLockable) return kFxDefaults[p - 8];
    return 0;
}

inline const char* trackParamName(int machine, int p)
{
    if (p < 8) return kMachineDefs[machine].p[p];
    if (p < kLockable) return kFxNames[p - 8];
    return "?";
}

// LFO helpers shared by UI display
static const char* const kLfoShapes[6] = {"TRI", "SINE", "SQR", "SAW+", "SAW-", "RND"};
static const char* const kLfoTypes[3] = {"FREE", "TRIG", "HOLD"};
inline int lfoShapeIndex(int v) { return v * 6 / 128; }
inline int lfoTypeIndex(int v) { return v * 3 / 128; }
inline int lfoDestIndex(int v) { return v * kLfoDests / 128; } // destinations: params 0..23
inline int lfoDestValue(int p) { return (p * 128 + kLfoDests - 1) / kLfoDests; }  // inverse (smallest value)
inline int lfoTrackIndex(int v) { return v * 17 / 128; } // 0 = this track, 1..16 = track n

// sequencer step scales
static const float kScaleMult[7] = {1.f, 2.f, 0.75f, 1.5f, 0.5f, 0.25f, 0.125f};
static const char* const kScaleNames[7] = {"1x", "2x", "3/4x", "3/2x", "1/2x", "1/4x", "1/8x"};

// echo divisions (in beats)
static constexpr int kEchoDivs = 12;
static const float kEchoBeats[kEchoDivs] = {
    1.f/16, 1.f/8, 1.f/6, 1.f/4, 1.f/3, 3.f/8, 1.f/2, 2.f/3, 3.f/4, 1.f, 1.5f, 2.f
};
static const char* const kEchoNames[kEchoDivs] = {
    "1/64", "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/4D", "1/2"
};
inline int echoDivIndex(int v) { return v * kEchoDivs / 128; }

} // namespace kn
