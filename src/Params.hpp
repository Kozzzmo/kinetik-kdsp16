// Kinetik KDSP-16 — shared definitions (DSP + UI)
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>

namespace kn {

static constexpr int kTracks = 16;
static constexpr int kSteps = 128;
static constexpr int kPatterns = 16;

// ---- per-track parameters (all 0..127 like the original hardware) ----
enum TrackParam {
    tpSyn0 = 0, // 0..7 synthesis params (depend on machine)
    tpAMD = 8, tpAMF, tpEQF, tpEQG, tpCUTOFF, tpFTYPE, tpRESO, tpSRR,
    tpDIST = 16, tpVOL, tpPAN, tpDEL, tpREV, tpFENV, tpFDEC, tpVEL,
    tpLFOS = 24, tpLFOD, tpLFOSH, tpLFOT, tpLFODST, tpLFOTRK,       // LFO 1
    tpLFO2S = 30, tpLFO2D, tpLFO2SH, tpLFO2T, tpLFO2DST, tpLFO2TRK,  // LFO 2
    tpLFOX = 36,             // SYNC, PHASE of LFO 1 then LFO 2
    kLockable = 40,          // every track parameter can be p-locked
    kTrackParams = 40        // machine, mute and the TRACK page are kit settings (stored in the Bank)
};
static constexpr int kLfoBase[2] = {tpLFOS, tpLFO2S};
static constexpr int kLfoDests = 24; // an LFO can move params 0..23
// parameter k (0..7) of LFO l: RATE DEPTH WAVE MODE DEST TARGET SYNC PHASE
inline int lfoParam(int l, int k) { return k < 6 ? kLfoBase[l] + k : tpLFOX + l * 2 + (k - 6); }

static const char* const kFxNames[kTrackParams - 8] = {
    "TREMOLO", "TREM RATE", "EQ FREQ", "EQ GAIN", "CUTOFF", "TYPE", "RESO", "CRUSH",
    "DRIVE", "LEVEL", "PAN", "DELAY", "REVERB", "ENV AMT", "ENV DEC", "VELOCITY",
    "RATE", "DEPTH", "WAVE", "MODE", "DEST", "TARGET",
    "RATE", "DEPTH", "WAVE", "MODE", "DEST", "TARGET",
    "SYNC", "PHASE", "SYNC", "PHASE"
};
static const uint8_t kFxDefaults[kTrackParams - 8] = {
    0, 64, 64, 64, 127, 0, 0, 0,
    0, 100, 64, 0, 0, 64, 40, 127,
    64, 0, 0, 0, 0, 0,
    40, 0, 0, 0, 0, 0,
    0, 0, 0, 0
};
// filter types (TYPE parameter)
static constexpr int kFilterTypes = 5;
static const char* const kFilterTypeNames[kFilterTypes] = {"LP 12", "LP 24", "BAND", "HIGH", "NOTCH"};
inline int filterTypeIndex(int v) { const int i = v * kFilterTypes / 128; return i < 0 ? 0 : (i >= kFilterTypes ? kFilterTypes - 1 : i); }
inline int filterTypeValue(int i) { return (i * 128 + kFilterTypes - 1) / kFilterTypes; }
// cutoff value (0..127) -> Hz (20 Hz .. 20 kHz)
inline float cutoffHz(float v) { return 20.f * std::pow(1000.f, (v < 0 ? 0 : (v > 127 ? 127 : v)) / 127.f); }

// ---- kit settings shown on the TRACK page (not host parameters) ----
enum KitField { kfChoke = 0, kfOut, kfMidi, kfFmq, kfOsc, kKitFields };
static const char* const kKitNames[kKitFields] = {"CHOKE", "OUTPUT", "MIDI CH", "FM SNAP", "OSC WAVE"};
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
    gECHO_MODEL, gECHO_MOD, gREV_MODEL, gREV_MOD,
    gOUT_STEP, gOUT_LEDS, gOUT_PATTERN,
    kGlobalParams
};

static constexpr int kGlobalBase = kTracks * kTrackParams;
static constexpr int kTotalParams = kGlobalBase + kGlobalParams;

// master parameters gECHO_TIME .. gREV_MOD (index g - gECHO_TIME)
static constexpr int kMasterCount = gREV_MOD - gECHO_TIME + 1;
static const char* const kMasterNames[kMasterCount] = {
    "TIME", "FEEDBACK", "HIGH CUT", "LOW CUT", "WIDTH", "LEVEL",
    "DECAY", "DAMPING", "GATE", "LOW CUT", "PRE-DELAY", "LEVEL",
    "LOW", "HIGH", "THRESH", "RATIO", "ATTACK", "RELEASE", "DRIVE", "VOLUME",
    "MODEL", "MOD", "MODEL", "MOD"
};
static const uint8_t kMasterDefaults[kMasterCount] = {
    48, 50, 100, 10, 64, 64,
    60, 64, 0, 20, 10, 64,
    64, 64, 127, 40, 20, 50, 0, 100,
    0, 30, 52, 30
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
    mOFF, mKICK, mSNARE, mTOM, mCLAP, mRIM, mCOWBELL, mCLHAT, mOPHAT, mCYMBAL,
    mSHAKER, mCLAVE,
    mFM_KICK, mFM_SNARE, mFM_TOM, mFM_CLAP, mFM_RIM, mFM_BELL, mFM_HAT, mFM_CYMBAL,
    mPH_KICK, mPH_SNARE, mPH_METAL, mPH_MALLET, mPH_STRING,
    mSINE, mNOISE,
    kMachines
};

static const MachineDef kMachineDefs[kMachines] = {
    {"OFF", fOff,   {"-","-","-","-","-","-","-","-"},                         {0,0,0,0,0,0,0,0}},
    {"KICK", fTone,     {"TUNE","DECAY","SWEEP","SWEEP TIME","CLICK","NOISE","TONE","DRIVE"},        {52,60,50,30,60,0,0,20}},
    {"SNARE", fSnare,   {"TUNE","DECAY","SWEEP","SWEEP TIME","NOISE","NOISE TONE","OVERTONE","NOISE DEC"}, {50,40,40,20,80,64,50,50}},
    {"TOM", fTone,      {"TUNE","DECAY","SWEEP","SWEEP TIME","CLICK","NOISE","TONE","DRIVE"},        {50,60,40,40,40,10,0,10}},
    {"CLAP", fClap,     {"CLAPS","DECAY","SPREAD","TONE","RESONANCE","TAIL","RANDOM","DRIVE"},       {64,50,50,60,30,80,20,10}},
    {"RIM", fSnare,     {"TUNE","DECAY","BODY","CLICK","NOISE","NOISE DEC","NOISE TONE","DRIVE"},    {60,20,50,80,40,20,40,20}},
    {"COWBELL", fMetal, {"TUNE","DECAY","DETUNE","TONE","RESONANCE","SHORT","CLICK","DRIVE"},       {60,50,64,64,40,64,30,30}},
    {"CL HAT", fMetal,  {"TUNE","DECAY","TONE","METAL","RESONANCE","SPREAD","ATTACK","DRIVE"},      {64,30,80,80,30,64,0,0}},
    {"OP HAT", fMetal,  {"TUNE","DECAY","TONE","METAL","RESONANCE","SPREAD","ATTACK","DRIVE"},      {64,70,80,80,30,64,0,0}},
    {"CYMBAL", fMetal,  {"TUNE","DECAY","TONE","METAL","RESONANCE","SPREAD","BELL","DRIVE"},        {64,80,60,70,30,64,30,0}},
    {"SHAKER", fShaker, {"ATTACK","HOLD","DECAY","LOW CUT","HIGH CUT","GRAIN","GRAIN RATE","DRIVE"}, {10,20,40,80,110,40,64,0}},
    {"CLAVE", fTone,    {"TUNE","DECAY","CLICK","OVERTONE","OVT LEVEL","NOISE","NOISE DEC","DRIVE"}, {64,30,60,64,40,10,20,0}},
    {"FM KICK", fFM,    {"TUNE","DECAY","SWEEP","SWEEP TIME","FM AMOUNT","FM RATIO","FM DECAY","FEEDBACK"}, {52,60,50,30,40,32,30,0}},
    {"FM SNARE", fFM,   {"TUNE","DECAY","NOISE","NOISE DEC","FM AMOUNT","FM RATIO","FM DECAY","LOW CUT"},   {64,40,70,40,50,50,30,60}},
    {"FM TOM", fFM,     {"TUNE","DECAY","SWEEP","SWEEP TIME","FM AMOUNT","FM RATIO","FM DECAY","CLICK"},    {60,60,40,40,40,40,30,30}},
    {"FM CLAP", fFM,    {"TUNE","DECAY","CLAPS","SPREAD","FM AMOUNT","FM RATIO","FM DECAY","FEEDBACK"},     {70,50,64,50,90,80,60,90}},
    {"FM RIM", fFM,     {"TUNE","DECAY","FM AMOUNT","FM RATIO","FM DECAY","NOISE","NOISE DEC","FEEDBACK"},  {70,20,60,60,20,30,15,20}},
    {"FM BELL", fFM,    {"TUNE","DECAY","SNAP","FEEDBACK","FM AMOUNT","FM RATIO","FM DECAY","TONE"},        {64,50,40,10,50,70,50,64}},
    {"FM HAT", fFM,     {"TUNE","DECAY","TREMOLO","TREM RATE","FM AMOUNT","FM RATIO","FM DECAY","LOW CUT"}, {80,30,0,64,90,100,60,90}},
    {"FM CYMBAL", fFM,  {"TUNE","DECAY","FEEDBACK","BELL","FM AMOUNT","FM RATIO","FM DECAY","LOW CUT"},     {64,80,80,30,90,90,80,70}},
    {"PH KICK", fModal,   {"TUNE","DECAY","HARDNESS","TENSION","DAMPING","MODES","NOISE","DRIVE"},       {40,60,50,40,40,50,10,10}},
    {"PH SNARE", fModal,  {"TUNE","DECAY","HARDNESS","TENSION","SNARES","SNARE DEC","MODES","DRIVE"},    {64,40,70,30,80,50,50,10}},
    {"PH METAL", fModal,  {"TUNE","DECAY","HARDNESS","INHARMONIC","DAMPING","MODES","SPREAD","DRIVE"},   {70,70,80,64,40,64,64,0}},
    {"PH MALLET", fModal, {"TUNE","DECAY","HARDNESS","DAMPING","MATERIAL","POSITION","RING","DRIVE"},    {64,50,60,40,40,64,30,0}},
    {"PH STRING", fString,{"TUNE","DECAY","BRIGHT","PICK POS","DAMPING","STIFFNESS","NOISE","DRIVE"},    {64,70,70,50,40,0,50,0}},
    {"SINE", fTone,     {"TUNE","DECAY","SWEEP","SWEEP TIME","WAVE","ATTACK","HOLD","DRIVE"},            {64,60,0,30,0,0,0,0}},
    {"NOISE", fNoise,   {"ATTACK","HOLD","DECAY","LOW CUT","HIGH CUT","RESONANCE","CRUSH","DRIVE"},      {0,0,60,0,127,0,0,0}},
};

static const uint8_t kDefaultKit[kTracks] = {
    mKICK, mSNARE, mTOM, mTOM, mCLAP, mRIM, mCOWBELL, mCLHAT,
    mOPHAT, mCYMBAL, mFM_KICK, mFM_SNARE, mFM_HAT, mPH_METAL, mPH_MALLET, mNOISE
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
static const char* const kLfoTypes[3] = {"FREE", "RETRIG", "HOLD"};
// tempo-synced LFO: length of one cycle, in beats
static constexpr int kLfoSyncDivs = 16;
static const float kLfoSyncBeats[kLfoSyncDivs] = {64.f, 32.f, 16.f, 8.f, 4.f, 2.f, 1.5f, 1.f, 2.f / 3.f, 0.75f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f, 0.0625f};
static const char* const kLfoSyncNames[kLfoSyncDivs] = {"16 BARS", "8 BARS", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64"};
inline int lfoSyncDivIndex(int v) { const int i = v * kLfoSyncDivs / 128; return i < 0 ? 0 : (i >= kLfoSyncDivs ? kLfoSyncDivs - 1 : i); }
inline bool lfoSynced(float syncParam) { return syncParam >= 64.f; }
inline float lfoFreeHz(float rate) { return 0.03f * std::pow(1000.f, (rate < 0 ? 0 : (rate > 127 ? 127 : rate)) / 127.f); }
inline int lfoTrackValue(int t) { return (t * 128 + 16) / 17; }
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
