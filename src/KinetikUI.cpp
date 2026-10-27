// Kinetik KDSP-16 — user interface (NanoVG)
#include "DistrhoUI.hpp"
#include "DistrhoPlugin.hpp"
#include "Bank.hpp"
#include "Dsp.hpp"
#include "Presets.hpp"
#include "Help.hpp"
#include "Skins.hpp"
#include "LogoMark.hpp"
#include "Fonts.hpp"
#include "License.hpp"
#include "Updater.hpp"
#include "MasterFx.hpp"
#include "Voice.hpp"
#include "Sample.hpp"
#include "FileDrop.hpp"
#include "MidiFile.hpp"
#include "FxPresets.hpp"
#include "DrumShared.hpp"
#define DR_WAV_IMPLEMENTATION
#include "third_party/dr_libs/dr_wav.h"
#include <cstdlib>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <vector>
#include <cmath>
#include <string>
#include <memory>
#include <deque>
#include <chrono>

START_NAMESPACE_DISTRHO

using namespace kn;
using DGL_NAMESPACE::Color;

static constexpr float W = 1180.f, H = 720.f;

struct R {
    float x, y, w, h;
    bool in(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
};

enum Page { pgSynth, pgFilter, pgFx, pgMix, pgLfo1, pgLfo2, pgTrack, kPages };
static const char* const kPageNames[kPages] = {"SYNTH", "FILTER", "FX", "MIX", "LFO 1", "LFO 2", "TRACK"};
enum MPage { mpEcho, mpReverb, mpChorus, mpChaos, mpFilter, mpMaster, kMPages };
enum Mode { mdTrig, mdAccent, mdSlide, mdRatch, mdCond, mdTime, kModes };

enum DragKind { dkNone, dkTrackKnob, dkMasterKnob, dkTempo, dkSwing, dkAccent, dkKit, dkEuclid, dkSend, dkGlHold };
enum Overlay { ovNone, ovPicker, ovPresets, ovHelp, ovSkins, ovSave, ovLicense, ovDest , ovUpdate, ovFx, ovLib};
enum DragExtra { dkResize = 100 };

class KinetikUI : public UI
{
public:
    KinetikUI()
        : UI(W, H)
    {
        loadSharedResources();
        const double s = getScaleFactor();
        if (s > 1.01) setSize((uint)(W * s), (uint)(H * s));
        setGeometryConstraints((uint)(W * 0.6 * (s > 1.01 ? s : 1.0)), (uint)(H * 0.6 * (s > 1.01 ? s : 1.0)), true, false);
        for (int i = 0; i < kTotalParams; ++i) values[i] = 0.f;
        for (int t = 0; t < kTracks; ++t)
            for (int p = 0; p < kTrackParams; ++p) values[t * kTrackParams + p] = trackParamDefault(t, p);
        for (int g = gECHO_TIME; g < gOUT_STEP; ++g) values[kGlobalBase + g] = globalDefault(g);
        values[kGlobalBase + gTEMPO] = 120;
        values[kGlobalBase + gOUT_STEP] = -1;
        bank.reset(new Bank());
        bank->initDefault();
        fonts[fnSerif] = createFontFromMemory("kn-serif", kFontSerif, sizeof(kFontSerif), false);
        fonts[fnSerifItalic] = createFontFromMemory("kn-serif-i", kFontSerifItalic, sizeof(kFontSerifItalic), false);
        fonts[fnMono] = createFontFromMemory("kn-mono", kFontMono, sizeof(kFontMono), false);
        fonts[fnBlack] = createFontFromMemory("kn-black", kFontBlack, sizeof(kFontBlack), false);
        fonts[fnLogo] = createFontFromMemory("kn-logo", kFontUnbounded, sizeof(kFontUnbounded), false);
        int sk, zm;
        readDefaultView(sk, zm, lfoPal, clickOn);
        setSkin(sk, false);
        if (zm != 100) setSize((uint)std::lround(W * baseScale() * zm / 100.0), (uint)std::lround(H * baseScale() * zm / 100.0));
        viewDirty = false;
        updater.reset(new upd::Checker("KDSP-16", KN_VERSION, "https://github.com/Kozzzmo/kinetik-kdsp16/releases/latest/download/update.txt"));
        updater->check(false);
        if (void* p = getPluginInstancePointer()) direct = dynamic_cast<DrumDirect*>(static_cast<Plugin*>(p));
        if (direct) direct->setRecording(false);
        // files dropped on the window: samples (SAMPLE machine) or a licence file
        dropToken = fileDropAttach(getWindow().getNativeWindowHandle(),
                                   [this](const std::string& path, double fx, double fy) { fileDropped(path, fx * W, fy * H); });
    }
    ~KinetikUI() override { fileDropDetach(dropToken); if (direct) direct->setRecording(false); }

    // ------------------------------------------------------------------ skins
    static std::string viewFile()
    {
       #ifdef _WIN32
        const std::string base = fs::env("APPDATA");
        return !base.empty() ? base + "\\KinetikKDSP16-view.txt" : std::string();
       #else
        const std::string base = fs::env("HOME");
       #ifdef __APPLE__
        return !base.empty() ? base + "/Library/Preferences/KinetikKDSP16-view.txt" : std::string();
       #else
        return !base.empty() ? base + "/.config/KinetikKDSP16-view.txt" : std::string();
       #endif
       #endif
    }
    static void readDefaultView(int& skinOut, int& zoomOut, int* pal, bool& click)
    {
        skinOut = kDefaultSkin; zoomOut = 100;
        const std::string f = viewFile();
        if (f.empty()) return;
        if (FILE* fp = fs::open(f, "r")) {
            int a = 0, b = 100, c[3] = {4, 5, 7}, k = 1;
            const int n = std::fscanf(fp, "%d %d %d %d %d %d", &a, &b, &c[0], &c[1], &c[2], &k); std::fclose(fp);
            if (n >= 6) click = k != 0;   // metronome while recording (0.17 / 0.13)
            if (n >= 1 && a >= 0 && a < kNumThemes) skinOut = a;
            if (n >= 2 && b >= 60 && b <= 300) zoomOut = b;
            for (int l = 0; l < 3; ++l) if (n >= 3 + l && c[l] >= 0 && c[l] < kLfoPal) pal[l] = c[l];   // LFO colours (0.16 / 0.12)
        }
    }
    static void writeDefaultView(int s, int z, const int* pal, bool click)
    {
        const std::string f = viewFile();
        if (f.empty()) return;
        if (FILE* fp = fs::open(f, "w")) { std::fprintf(fp, "%d %d %d %d %d %d\n", s, z, pal[0], pal[1], pal[2], click ? 1 : 0); std::fclose(fp); }
    }
    void setSkin(int i, bool save)
    {
        skin = clampi(i, 0, kNumThemes - 1);
        th = &kThemes[skin];
        if (save) saveView();
        repaint();
    }

protected:
    // ------------------------------------------------------------------ DSP -> UI
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= (uint32_t)kTotalParams) return;
        if (values[index] == value) return;
        values[index] = value;
        if (index == (uint32_t)(kGlobalBase + gOUT_STEP) && recording() && drag == dkTrackKnob && recDirty)
            recordLock(pageParam(page, dragIndex), (int)std::lround(recVal));
        repaint();
    }

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "samples") == 0) {
            for (auto& sp : uiSmp) sp.reset();
            smp::decodeAll(value ? value : "", [this](int t, SamplePtr d) { if (t >= 0 && t < kTracks) uiSmp[t] = std::move(d); });
            repaint();
            return;
        }
        if (std::strcmp(key, "bank") == 0) { deserializeBank(*bank, value); repaint(); }
        if (std::strcmp(key, "skin") == 0 && value && *value) {
            int sk = kDefaultSkin, zm = 0;
            const int n = std::sscanf(value, "%d %d", &sk, &zm);
            if (n >= 1) setSkin(sk, false);
            if (n >= 2 && zm >= 60 && zm <= 300 && std::abs(zm - zoomPercent()) > 1) setZoom(zm / 100.0);
            viewDirty = false;
        }
    }

    // ------------------------------------------------------------------ helpers
    float& tp(int t, int p) { return values[t * kTrackParams + p]; }
    float gp(int g) const { return values[kGlobalBase + g]; }
    int editPattern() const { return clampi((int)std::lround(gp(gPATTERN)), 0, kPatterns - 1); }
    int machineOf(int t) const { return clampi(bank->machine[t], 0, kMachines - 1); }
    int64_t fineStep() const { return (int64_t)std::lround(gp(gOUT_STEP)); }               // 1/24 steps, -1 when stopped
    int64_t absStep() const { const int64_t f = fineStep(); return f < 0 ? -1 : f / kFineSteps; } // pattern step
    int64_t trackK(int t) const { const int64_t f = fineStep(); return f < 0 ? -1 : (int64_t)std::floor((double)f * trackSpeedMult(bank->kit[t][kfSpeed]) / kFineSteps + 1e-9); }
    int trackStepNow(int t) { const int64_t k = trackK(t); return k < 0 ? -1 : pat().trackStep(t, k); }
    int tlen(int t) { return pat().trackLen[t] ? pat().trackLen[t] : 16; }
    Pattern& pat() { return bank->patterns[editPattern()]; }

    void setParam(uint32_t idx, float v)
    {
        if (values[idx] != v) autoSnap("sound", 1.2);
        values[idx] = v;
        setParameterValue(idx, v);
        // also sent through the cmd state (flushed in uiIdle): the DSP applies it even if the host loses or
        // reverts the parameter change (see uiEdit in the plugin)
        char b[40]; std::snprintf(b, sizeof(b), "pv %u %.4f\n", (unsigned)idx, v); pvQueue += b;
    }
    std::string pvQueue;
    void flushPv()
    {
        if (pvQueue.empty()) return;
        pvQueue.pop_back();
        setState("cmd", pvQueue.c_str());
        pvQueue.clear();
    }
    void setParamGesture(uint32_t idx, float v)
    {
        editParameter(idx, true);
        setParam(idx, v);
        editParameter(idx, false);
    }

    void sendCmd(const std::string& c)
    {
        if (undoable(c)) autoSnap("edit", 0.5);
        if (c.rfind("aud ", 0) != 0) applyCommand(*bank, c.c_str());
        setState("cmd", c.c_str());
    }
    static bool undoable(const std::string& c)
    {
        static const char* skip[] = {"aud ", "fill ", "sel ", "rec ", "non ", "noff ", "panic", "lic", "pv ", "gh "};
        for (const char* k : skip) if (c.rfind(k, 0) == 0) return false;
        return true;
    }
    void sendLines(const std::string& c)
    {
        if (undoable(c)) autoSnap("edit", 0.5);
        size_t st = 0;
        while (st < c.size()) {
            size_t e = c.find('\n', st); if (e == std::string::npos) e = c.size();
            applyCommand(*bank, c.substr(st, e - st).c_str());
            st = e + 1;
        }
        setState("cmd", c.c_str());
    }
    void sendPatternSettings()
    {
        const Pattern& p = pat();
        char b[64];
        std::snprintf(b, sizeof(b), "P %d %d %d %d %d", editPattern(), p.length, p.scale, p.swing, p.accent);
        sendCmd(b);
    }

    int pageParam(int pg, int k) const
    {
        if (k < 0 || k >= 8) return -1;
        switch (pg) {
        case pgSynth: return k;
        case pgFilter: { static const int f[8] = {tpCUTOFF, tpRESO, tpFTYPE, tpFENV, tpFDEC, tpEQF, tpEQG, -1}; return f[k & 7]; }
        case pgFx: { static const int f[8] = {tpAMD, tpAMF, tpSRR, tpDIST, -1, -1, -1, -1}; return f[k & 7]; }   // 5-6: CHORUS / CHAOS sends
        case pgMix: { static const int f[8] = {tpVOL, tpPAN, tpVEL, tpDEL, tpREV, -1, -1, -1}; return f[k & 7]; }
        case pgLfo1: return lfoParam(0, k);
        case pgLfo2: return lfoParam(1, k);
        default: return -1; // TRACK page: kit settings, see kitField()
        }
    }
    int kitField(int k) const { return page == pgTrack && k < kKitFields ? k : -1; }
    int masterParam(int mp, int k) const
    {
        if (mp == mpEcho) { static const int e[12] = {gECHO_TIME, gECHO_FB, gECHO_LPF, gECHO_HPF, gECHO_WIDTH, gECHO_LEV, gECHO_MODEL, gECHO_MOD, gECHO_RTIME, gECHO_DRIVE, gECHO_DUCK, -1}; return e[k % 12]; }   // 12 = FREEZE pad
        if (mp == mpReverb) return k < 6 ? gREV_SIZE + k : (k == 6 ? gREV_MODEL : gREV_MOD);
        if (mp == mpFilter) return k < 3 ? gMF_HP + k : -1;   // knob 4 = ON / OFF pad
        if (mp == mpChorus) return gCHO_MODEL + k;
        if (mp == mpChaos) return k < 7 ? gGL_CHANCE + k : -1;   // knob 8 = HOLD pad
        return gEQ_LOW + k;
    }

    static int lfoOf(int p, int& k)
    {
        for (int l = 0; l < 2; ++l) for (int i = 0; i < 8; ++i) if (lfoParam(l, i) == p) { k = i; return l; }
        return -1;
    }
    static std::string hzText(float hz)
    {
        char b[24];
        if (hz >= 1000.f) std::snprintf(b, sizeof(b), "%.1fk", hz / 1000.f);
        else std::snprintf(b, sizeof(b), "%d", (int)std::lround(hz));
        return b;
    }
    std::string lfoDestName(int t, int l, int d) const
    {
        const int trk = lfoTrackIndex((int)std::lround(values[t * kTrackParams + kLfoBase[l] + 5]));
        const int tt = trk == 0 ? t : trk - 1;
        if (d >= kLfoDests) return vdestName(d);
        return trackParamName(machineOf(tt), d);
    }
    static const char* vdestName(int d)
    {
        static const char* n[] = {"LFO1 RATE", "LFO1 DEPTH", "LFO2 RATE", "LFO2 DEPTH", "-"};   // MORPH (removed): shown as none
        return d >= kLfoDests && d < kDestCount ? n[d - kLfoDests] : "-";
    }
    static int destAt(int c, int r) { return c < 3 ? c * 8 + r : (r < kDestMorph - kLfoDests ? kLfoDests + r : -1); }   // MORPH no longer offered

    std::string valueText(int t, int p, float v) const
    {
        char b[32];
        const int iv = (int)std::lround(v);
        if (p == tpPAN) {
            if (iv == 64) return "C";
            std::snprintf(b, sizeof(b), "%c%d", iv < 64 ? 'L' : 'R', std::abs(iv - 64)); return b;
        }
        if (p == tpEQG || p == tpFENV) { std::snprintf(b, sizeof(b), "%+d", iv - 64); return b; }
        if (p < 8 && machineOf(t) == mSAMPLE) {
            switch (p) {
            case 0: { const int st = (int)std::lround(clampf((v - 64.f) / 63.f, -1.f, 1.f) * 24.f); if (st == 0) return "0 ST"; std::snprintf(b, sizeof(b), "%+d ST", st); return b; }
            case 1: case 2: std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
            case 4: if (iv >= 127) return "FULL"; break;
            case 5: return iv >= 64 ? "ON" : "OFF";
            default: break;
            }
        }
        if (p == tpCUTOFF) return hzText(cutoffHz(v));
        if (p == tpEQF) return hzText(expmap(v / 127.f, 40.f, 16000.f));
        if (p == tpAMF) return hzText(expmap(v / 127.f, 0.5f, 2000.f));
        if (p == tpFTYPE) return kFilterTypeNames[filterTypeIndex(iv)];
        if (p == tpVEL) { std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b; }
        int lk = 0; const int l = lfoOf(p, lk);
        if (l >= 0) {
            switch (lk) {
            case 0:
                if (lfoSynced(values[t * kTrackParams + lfoParam(l, 6)])) return kLfoSyncNames[lfoSyncDivIndex(iv)];
                { const float hz = lfoFreeHz(v); std::snprintf(b, sizeof(b), hz < 10.f ? "%.2f" : "%.1f", hz); return b; }
            case 2: return kLfoShapes[lfoShapeIndex(iv)];
            case 3: return kLfoTypes[lfoTypeIndex(iv)];
            case 4: return lfoDestName(t, l, lfoDestIndex(iv));
            case 5: { const int tt = lfoTrackIndex(iv); if (tt == 0) return "SELF"; std::snprintf(b, sizeof(b), "T%02d", tt); return b; }
            case 6: return lfoSynced(v) ? "TEMPO" : "FREE";
            case 7: std::snprintf(b, sizeof(b), "%d\xC2\xB0", (int)std::lround(v / 128.f * 360.f)); return b;
            default: break;
            }
        }
        std::snprintf(b, sizeof(b), "%d", iv); return b;
    }
    // knob label: MOD is named after what it does in the selected echo / reverb model
    const char* masterName(int g) const
    {
        if (g == gECHO_MOD) return kEchoModLabel[echoModelIndex((int)gp(gECHO_MODEL))];
        if (g == gREV_MOD) return kReverbModLabel[reverbModelIndex((int)gp(gREV_MODEL))];
        if (g == gECHO_RTIME) return "R TIME";
        if (g == gECHO_DRIVE) return "DRIVE";
        if (g == gECHO_DUCK) return "DUCK";
        return kMasterNames[g - gECHO_TIME];
    }
    std::string masterText(int g, float v) const
    {
        char b[32];
        const int iv = (int)std::lround(v);
        auto pct = [&](float x) { std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(x / 127.f * 100.f)); return std::string(b); };
        auto hz = [&](float h) { if (h >= 1000.f) std::snprintf(b, sizeof(b), "%.1fk", h / 1000.f); else std::snprintf(b, sizeof(b), "%d", (int)std::lround(h)); return std::string(b); };
        if (g == gMF_HP) return iv <= 0 ? std::string("OFF") : hz(cutoffHz(v));
        if (g == gMF_LP) return iv >= 127 ? std::string("OFF") : hz(cutoffHz(v));
        if (g == gMF_RESO || g == gCHO_DEPTH || g == gCHO_FB || g == gCHO_WIDTH || g == gGL_CHANCE || g == gGL_FADE || g == gGL_REV || g == gGL_MIX)
            return (g == gGL_CHANCE && iv == 0) ? std::string("OFF") : pct(v);
        if (g == gCHO_MODEL) return kChorusModelNames[chorusModelIndex(iv)];
        if (g == gCHO_RATE) { std::snprintf(b, sizeof(b), "%.2fHz", expmap(v / 127.f, 0.05f, 8.f)); return b; }
        if (g == gCHO_TIME) {
            const bool fl = chorusModelIndex((int)gp(gCHO_MODEL)) == cmFlanger;
            std::snprintf(b, sizeof(b), "%.1fms", fl ? expmap(v / 127.f, 0.3f, 6.f) : expmap(v / 127.f, 4.f, 30.f)); return b;
        }
        if (g == gCHO_TONE) return iv >= 127 ? std::string("OPEN") : hz(expmap(v / 127.f, 1200.f, 20000.f));
        if (g == gCHO_LEV) { const float gn = fxReturnGain(v); if (gn < 0.001f) return "OFF"; std::snprintf(b, sizeof(b), "%+.1fdB", 20.f * std::log10(gn)); return b; }
        if (g == gGL_SIZE) return kGlSizeNames[glSizeIndex(iv)];
        if (g == gGL_LEN) return kGlLenNames[glLenIndex(iv)];
        if (g == gGL_PITCH) { const int st = (int)glPitchSemis(v); if (st == 0) return "0 ST"; std::snprintf(b, sizeof(b), "%+d ST", st); return b; }
        if (g == gECHO_TIME) return kEchoNames[echoDivIndex(iv)];
        if (g == gECHO_MODEL) return kEchoModelNames[echoModelIndex(iv)];
        if (g == gREV_MODEL) return kReverbModelNames[reverbModelIndex(iv)];
        if (g == gECHO_LEV || g == gREV_LEV) {
            const float gn = fxReturnGain(v);
            if (gn < 0.001f) return "OFF";
            std::snprintf(b, sizeof(b), "%+.1fdB", 20.f * std::log10(gn)); return b;
        }
        if (g == gREV_GATE && iv == 0) return "OFF";
        if (g == gECHO_RTIME) { if (iv == 64) return "= L"; std::snprintf(b, sizeof(b), "x%.2f", 1.f + (v - 64.f) / 128.f); return b; }
        if (g == gECHO_DRIVE || g == gECHO_DUCK) return iv == 0 ? std::string("OFF") : pct(v);
        if (g == gEQ_LOW || g == gEQ_HIGH) { std::snprintf(b, sizeof(b), "%+d", iv - 64); return b; }
        std::snprintf(b, sizeof(b), "%d", iv); return b;
    }

    // ------------------------------------------------------------------ layout
    R trackBtn(int t) const { return {20.f + t * 71.25f, 66.f, 66.f, 64.f}; }
    R muteBtn(int t) const { R r = trackBtn(t); return {r.x + r.w - 28.f, r.y + r.h - 26.f, 28.f, 26.f}; } // generous hit area
    R soloBtn(int t) const { const R m = muteBtn(t); return {m.x - 26.f, m.y, 26.f, m.h}; }
    R lcd() const { return {20.f, 146.f, 680.f, 150.f}; }
    R machPrev() const { return {150.f, 156.f, 26.f, 26.f}; }
    R machName() const { return {178.f, 156.f, 130.f, 26.f}; }
    R machNext() const { return {310.f, 156.f, 26.f, 26.f}; }
    R smpLoadBtn() const { return {596.f, 156.f, 94.f, 24.f}; }
    R libBtn() const { return {496.f, 156.f, 94.f, 24.f}; }
    bool showSmpLoad() const { return page == pgSynth && lockStep < 0 && machineOf(selTrack) == mSAMPLE; }
    R lockClear() const { return {596.f, 156.f, 94.f, 24.f}; }
    R pageTab(int i) const { return {20.f + i * 88.f, 304.f, 85.f, 28.f}; }   // ALL button at the end of the row
    // track knobs: 8, or 9 on the SYNTH page of a kick (9th = ATTACK, a global host parameter per track)
    bool kickAttackSlot() const { const int m = machineOf(selTrack); return page == pgSynth && (m == mKICK || m == mFM_KICK || m == mPH_KICK); }
    int slots() const { return kickAttackSlot() ? 9 : 8; }
    R knob(int k) const { const float w = 680.f / slots(); return {20.f + k * w, 340.f, w, 120.f}; }
    R mTab(int i) const { const float w = (448.f - (kMPages - 1) * 4.f) / kMPages; return {716.f + i * (w + 4.f), 146.f, w, 28.f}; }
    // master knobs: 4 per row (8), the ECHO tab has 6 per row (12)
    int mSlots() const { return mpage == mpEcho ? 12 : 8; }
    R mKnob(int k) const
    {
        if (mpage == mpEcho) return {718.f + (k % 6) * 74.f, 178.f + (k / 6) * 102.f, 74.f, 100.f};
        return {720.f + (k % 4) * 110.f, 178.f + (k / 4) * 102.f, 110.f, 100.f};
    }
    R patBtn(int i) const { return {720.f + (i % 8) * 55.f, 432.f + (i / 8) * 30.f, 51.f, 26.f}; }
    R patCopy() const { return {946.f, 400.f, 68.f, 24.f}; }
    R patPaste() const { return {1018.f, 400.f, 68.f, 24.f}; }
    R patClear() const { return {1090.f, 400.f, 68.f, 24.f}; }
    R patMidi() const { return {874.f, 400.f, 68.f, 24.f}; }
    R modeBtn(int i) const { return {20.f + i * 62.f, 504.f, 58.f, 26.f}; }
    R stepPage(int i) const { return {396.f + i * 27.f, 504.f, 25.f, 26.f}; }   // 8 pages of 16 steps
    R lenMinus() const { return {612.f, 504.f, 22.f, 26.f}; }
    R lenBox() const { return {636.f, 504.f, 62.f, 26.f}; }
    R lenPlus() const { return {700.f, 504.f, 22.f, 26.f}; }
    R fillBtn() const { return {730.f, 504.f, 60.f, 26.f}; }
    // pattern settings live in the header
    R patLenBox() const { return {396.f, 9.f, 70.f, 40.f}; }
    R scaleBox() const { return {470.f, 9.f, 56.f, 40.f}; }
    R swingBox() const { return {530.f, 9.f, 78.f, 40.f}; }
    R accBox() const { return {612.f, 9.f, 80.f, 40.f}; }
    R helpBtn() const { return {340.f, 156.f, 26.f, 26.f}; }
    R trkCopy() const { return {800.f, 504.f, 62.f, 26.f}; }
    R trkPaste() const { return {866.f, 504.f, 62.f, 26.f}; }
    R trkClear() const { return {932.f, 504.f, 62.f, 26.f}; }
    R lockClrAll() const { return {998.f, 504.f, 66.f, 26.f}; }
    R shiftL() const { return {1068.f, 504.f, 26.f, 26.f}; }
    R shiftR() const { return {1096.f, 504.f, 26.f, 26.f}; }
    R stepBtn(int i) const { return {20.f + i * 71.25f, 544.f, 66.f, 72.f}; }
    R playBtn() const { return {840.f, 9.f, 84.f, 40.f}; }
    R tempoBox() const { return {932.f, 9.f, 110.f, 40.f}; }
    R skinBtn() const { return {1050.f, 9.f, 116.f, 40.f}; }
    R viewBox() const { return {240.f, 96.f, 700.f, 520.f}; }
    R skinRow(int i) const { return {264.f + (i % 2) * 330.f, 150.f + (i / 2) * 148.f, 316.f, 136.f}; }
    R zoomBtn(int i) const { return {264.f + i * 109.f, 486.f, 101.f, 36.f}; }
    R gripR() const { return {W - 22.f, H - 22.f, 22.f, 22.f}; }
    R recBtn() const { return {20.f, 628.f, 96.f, 30.f}; }
    R rndGroup() const { return {124.f, 628.f, 584.f, 52.f}; }
    R rndBtn(int i) const { return {250.f + i * 91.f, 634.f, 87.f, 40.f}; }
    R undoBtn() const { return {712.f, 634.f, 86.f, 40.f}; }
    R presetBtn() const { return {1010.f, 624.f, 150.f, 34.f}; }
    R saveBtn() const { return {1010.f, 664.f, 150.f, 34.f}; }
    // presets window
    R presetBox() const { return {80.f, 60.f, 1020.f, 610.f}; }
    R presetTab(int i) const { return {300.f + i * 134.f, 76.f, 128.f, 32.f}; }
    R presetRow(int i) const { return {100.f + (i % 2) * 490.f, 154.f + (i / 2) * 43.f, 480.f, 40.f}; }
    static constexpr int kPresetRows = 10;   // 2 columns
    // style chips above the list: 0 = ALL, 1.. = kPresetCats
    R presetCatChip(int i) const { const float w = (980.f - kPresetCatCount * 4.f) / (kPresetCatCount + 1); return {100.f + i * (w + 4.f), 120.f, w, 28.f}; }
    int presetCat = -1;
    R presetSaveNew() const { return {100.f, 620.f, 150.f, 36.f}; }
    R presetDelete() const { return {256.f, 620.f, 80.f, 36.f}; }
    R presetLoadAll() const { return {574.f, 620.f, 130.f, 36.f}; }
    R presetLoadKit() const { return {710.f, 620.f, 130.f, 36.f}; }
    R presetLoadPat() const { return {846.f, 620.f, 144.f, 36.f}; }
    R presetCancel() const { return {996.f, 620.f, 84.f, 36.f}; }
    R presetSearchBox() const { return {716.f, 76.f, 364.f, 32.f}; }
    // save window
    R saveBox() const { return {290.f, 200.f, 600.f, 270.f}; }
    R saveField() const { return {320.f, 290.f, 540.f, 52.f}; }
    R saveOk() const { return {620.f, 400.f, 240.f, 40.f}; }
    // licence window
    R logoArea() const { return {12.f, 4.f, 372.f, 50.f}; }
    R licBox() const { return {290.f, 170.f, 600.f, 330.f}; }
    R licLoad() const { return {320.f, 420.f, 190.f, 40.f}; }
    R licPaste() const { return {520.f, 420.f, 190.f, 40.f}; }
    R licClose() const { return {720.f, 420.f, 140.f, 40.f}; }
    R saveCancel() const { return {320.f, 400.f, 140.f, 40.f}; }

    // ------------------------------------------------------------------ drawing primitives (themed)
    void useFont(int f)
    {
        switch (f) {
        case fnSerif: fontFaceId(fonts[fnSerif]); break;
        case fnSerifItalic: fontFaceId(fonts[fnSerifItalic]); break;
        case fnMono: fontFaceId(fonts[fnMono]); break;
        case fnBlack: fontFaceId(fonts[fnBlack]); break;
        case fnLogo: fontFaceId(fonts[fnLogo]); break;
        default: fontFace(NANOVG_DEJAVU_SANS_TTF); break;
        }
    }
    void box(const R& r, const Color& fillC, float rad = -1.f)
    {
        beginPath(); roundedRect(r.x, r.y, r.w, r.h, rad < 0 ? th->radius : rad); fillColor(fillC); fill();
    }
    void frame(const R& r, const Color& c, float rad = -1.f, float wdt = 1.f)
    {
        beginPath(); roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, rad < 0 ? th->radius : rad); strokeColor(c); strokeWidth(wdt); stroke();
    }
    // readable label size: every small label is enlarged, with a floor so nothing is tiny
    static float textSize(float size, int font)
    {
        const float s = std::max(size, 9.5f);
        return font == fnMono ? s * 1.13f * 1.08f : s * 1.08f;   // labels: SemiBold, 8 % larger (owner's choice D)
    }
    // mono / label text (letter-spaced in the retro skins)
    void label(float x, float y, const char* s, float size, const Color& c, int align = ALIGN_CENTER | ALIGN_MIDDLE, int font = -1)
    {
        if (s == nullptr || *s == '\0') return;
        const int f = font < 0 ? th->fontLabel : font;
        useFont(f);
        fontSize(textSize(size, f));
        textLetterSpacing(f == fnMono ? th->labelSpacing * (size / 12.f) : 0.f);
        fillColor(c); textAlign(align); text(x, y, s, nullptr);
        textLetterSpacing(0.f);
    }
    enum Icon { icNone, icPlay, icStop, icRec, icNext };
    // icon (drawn as a shape, independent of the font) followed by a label, centred on (cx, cy)
    void iconLabel(float cx, float cy, Icon ic, const char* s, float size, const Color& c, int font = -1, int align = ALIGN_CENTER)
    {
        const int f = font < 0 ? th->fontButton : font;
        useFont(f);
        fontSize(textSize(size, f));
        textLetterSpacing(f == fnMono ? th->labelSpacing * (size / 12.f) : 0.f);
        DGL_NAMESPACE::Rectangle<float> bnd; textBounds(0, 0, s, nullptr, bnd);
        textLetterSpacing(0.f);
        const float isz = size * 0.62f, gap = size * 0.55f;
        const float total = isz + gap + bnd.getWidth();
        float x = align == ALIGN_RIGHT ? cx - total : (align == ALIGN_LEFT ? cx : cx - total * 0.5f);
        beginPath();
        switch (ic) {
        case icPlay: case icNext:
            moveTo(x, cy - isz * 0.55f); lineTo(x + isz * 0.9f, cy); lineTo(x, cy + isz * 0.55f); closePath(); break;
        case icStop: rect(x, cy - isz * 0.45f, isz * 0.9f, isz * 0.9f); break;
        case icRec: circle(x + isz * 0.45f, cy, isz * 0.48f); break;
        default: break;
        }
        fillColor(c); fill();
        label(x + isz + gap, cy, s, size, c, ALIGN_LEFT | ALIGN_MIDDLE, f);
    }

    // big numbers / names (serif, black grotesk or sans depending on the skin), optional glow
    void number(float x, float y, const char* s, float size, const Color& c, int align = ALIGN_CENTER | ALIGN_MIDDLE, int font = -1)
    {
        if (s == nullptr || *s == '\0') return;
        const int f = font < 0 ? th->fontNum : font;
        useFont(f);
        float sz = f == fnSerif || f == fnSerifItalic ? size * 1.25f : (f == fnBlack ? size * 0.95f : size);
        if (size <= 16.f) sz *= 1.15f;   // small values (knobs, steps) 15 % larger; big displays unchanged
        fontSize(sz); textAlign(align);
        if (th->glow) {
            fontBlur(sz * 0.18f); fillColor(Color(c, Color(0, 0, 0, 0.f), 0.35f)); text(x, y, s, nullptr);
            fontBlur(0.f);
        }
        fillColor(c); text(x, y, s, nullptr);
    }
    void dotted(const R& r, const Color& c)
    {
        if (!dotImage.isValid()) { box(r, c, 1.f); return; }
        Paint p = imagePattern(r.x, r.y, 4.f, 4.f, 0.f, dotImage, 1.f);
        p.innerColor = c; p.outerColor = c;
        beginPath(); rect(r.x, r.y, r.w, r.h); fillPaint(p); fill();
    }
    void panelBox(const R& r)
    {
        box(r, th->panel, th->radius + 2.f);
        frame(r, th->panelEdge, th->radius + 2.f, th->panelEdgeWidth);
    }
    void button(const R& r, const char* s, bool on, float size = 11.5f)
    {
        if (on) box(r, th->selFill);
        else { box(r, th->btnFill); frame(r, th->btnEdge, -1.f, 1.f); }
        label(r.cx(), r.cy() + 1, s, size, on ? th->selText : th->btnText, ALIGN_CENTER | ALIGN_MIDDLE, th->fontButton);
    }
    void drawKnob(const R& r, float v01, const char* name, const std::string& val, bool locked, bool highlight, bool enabled,
                  float mod01 = -1.f, Color modCol = Color())
    {
        // mod01 >= 0: an LFO moves this knob; the arc / ticks follow the modulated value in the LFO's colour,
        // the pointer stays on the set value
        const float cx = r.cx(), cy = r.y + 44.f, rad = 26.f;
        const float a0 = kPi * 0.75f, a1 = kPi * 2.25f;
        v01 = clampf(v01, 0.f, 1.f);
        const Color hi = locked ? th->lock : th->knobArc;
        if (!enabled) {
            beginPath(); circle(cx, cy, rad - 6); fillColor(Color(th->knobBody, th->panel, 0.6f)); fill();
            for (int i = 0; i < 24; ++i) {
                const float a = kTwoPi * i / 24.f;
                beginPath(); circle(cx + std::cos(a) * (rad - 6), cy + std::sin(a) * (rad - 6), 0.8f); fillColor(th->knobTrack); fill();
            }
            label(cx, r.y + 90.f, "-", 12.f, th->faint);
            return;
        }
        const float a = a0 + (a1 - a0) * v01;
        const bool modOn = mod01 >= 0.f;
        const float e01 = modOn ? clampf(mod01, 0.f, 1.f) : v01;   // where the arc ends
        const Color ac = modOn ? modCol : hi;                     // arc colour
        switch (th->knob) {
        case ksTicks: {
            // ring of tick dots around a dark knob (lit up to the value)
            const int n = 25;
            for (int i = 0; i < n; ++i) {
                const float t = (float)i / (n - 1);
                const float ang = a0 + (a1 - a0) * t;
                const bool lit = t <= e01 + 1e-4f;
                beginPath(); circle(cx + std::cos(ang) * (rad + 3), cy + std::sin(ang) * (rad + 3), lit ? (modOn ? 1.9f : 1.6f) : 1.1f);
                fillColor(lit ? ac : th->knobTrack); fill();
            }
            if (th->glow) {
                beginPath(); circle(cx, cy, rad - 3);
                fillPaint(radialGradient(cx, cy, rad - 8, rad + 8, Color(ac, Color(0, 0, 0, 0.f), 0.75f), Color(0, 0, 0, 0.f))); fill();
            }
            beginPath(); circle(cx, cy, rad - 5); fillColor(th->knobBody); fill();
            strokeColor(locked ? th->lock : th->knobRing); strokeWidth(locked ? 2.f : 1.2f); stroke();
            beginPath(); moveTo(cx + std::cos(a) * 4, cy + std::sin(a) * 4); lineTo(cx + std::cos(a) * (rad - 9), cy + std::sin(a) * (rad - 9));
            strokeColor(locked ? th->lock : th->knobPointer); strokeWidth(2.4f); lineCap(ROUND); stroke();
            break;
        }
        case ksArcThick: {
            beginPath(); arc(cx, cy, rad, a0, a1, CW); strokeColor(th->knobTrack); strokeWidth(6.f); lineCap(BUTT); stroke();
            beginPath(); arc(cx, cy, rad, a0, a0 + (a1 - a0) * std::max(0.001f, e01), CW); strokeColor(ac); strokeWidth(6.f); stroke();
            beginPath(); circle(cx, cy, rad - 6); fillColor(locked ? th->lock : th->knobBody); fill();
            strokeColor(th->knobRing); strokeWidth(1.2f); stroke();
            beginPath(); moveTo(cx + std::cos(a) * 6, cy + std::sin(a) * 6); lineTo(cx + std::cos(a) * (rad - 9), cy + std::sin(a) * (rad - 9));
            strokeColor(th->knobPointer); strokeWidth(2.4f); lineCap(ROUND); stroke();
            break;
        }
        default: {
            beginPath(); circle(cx, cy, rad + 4); fillColor(th->inner); fill();
            beginPath(); arc(cx, cy, rad, a0, a1, CW); strokeColor(th->knobTrack); strokeWidth(5.f); stroke();
            beginPath(); arc(cx, cy, rad, a0, a0 + (a1 - a0) * std::max(0.001f, e01), CW); strokeColor(ac); strokeWidth(5.f); stroke();
            if (modOn && locked) { beginPath(); circle(cx, cy, rad + 4); strokeColor(th->lock); strokeWidth(1.5f); stroke(); }
            beginPath(); circle(cx, cy, rad - 7); fillColor(highlight ? Color(th->knobBody, Color(255, 255, 255), 0.1f) : th->knobBody); fill();
            beginPath(); moveTo(cx + std::cos(a) * (rad - 18), cy + std::sin(a) * (rad - 18));
            lineTo(cx + std::cos(a) * (rad - 8), cy + std::sin(a) * (rad - 8));
            strokeColor(th->knobPointer); strokeWidth(3.f); stroke();
            break;
        }
        }
        label(cx, r.y + 88.f, name, 12.f, locked ? th->lock : th->text);
        number(cx, r.y + 106.f, val.c_str(), 14.f, locked ? th->lock : (th->fontNum == fnSans ? th->dim : th->text));
    }

    void drawSparkle(float cx, float cy, float s, const Color& c)
    {
        beginPath();
        moveTo(cx, cy - s);
        quadTo(cx + s * 0.18f, cy - s * 0.18f, cx + s, cy);
        quadTo(cx + s * 0.18f, cy + s * 0.18f, cx, cy + s);
        quadTo(cx - s * 0.18f, cy + s * 0.18f, cx - s, cy);
        quadTo(cx - s * 0.18f, cy - s * 0.18f, cx, cy - s);
        closePath(); fillColor(c); fill();
    }

    // the same logo in every skin: the Kinetik "Orbit" mark (LogoMark.hpp, colours per skin) + the product name
    void drawLogo()
    {
        drawOrbitMark(*this, 14.f, 7.f, 46.f, th->logo);
        const char* nm = "KDSP-16";
        useFont(fnLogo); fontSize(30.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        const float nx = 68.f, ny = 23.f;
        if (th->glow) { fontBlur(10.f); fillColor(Color(th->accent, Color(0, 0, 0, 0.f), 0.45f)); text(nx, ny, nm, nullptr); fontBlur(0.f); }
        fillColor(th->glow ? Color(232, 236, 255) : th->text); text(nx, ny, nm, nullptr);
        const float sy = 45.f;
        const int ust = updater ? (int)updater->getState() : -1;
        if (ust == upd::Checker::stAvailable || ust == upd::Checker::stInstalling || ust == upd::Checker::stInstalled) {
            const std::string v = updater->manifest().version;
            const std::string t = ust == upd::Checker::stAvailable ? "UPDATE " + v + " AVAILABLE - CLICK"
                                : ust == upd::Checker::stInstalling ? "INSTALLING UPDATE " + v + "..."
                                : "UPDATED TO " + v + " - REOPEN TO USE IT";
            label(nx + 1, sy, t.c_str(), 9.5f, th->logo == lgRiso ? th->text : th->accent, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        }
        else if (licInfo.valid || !lic::kDemoModeEnabled) label(nx + 1, sy, "KINETIK  DRUM SYNTH PROGRAM", 9.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        else label(nx + 1, sy, "DEMO - CLICK TO REGISTER", 9.5f, th->logo == lgRiso ? th->text : th->accent, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
    }

    // ------------------------------------------------------------------ main draw
    void onNanoDisplay() override
    {
        sx = getWidth() / W;
        scale(sx, sx);
        if (!dotImage.isValid()) makeTextures();

        beginPath(); rect(0, 0, W, H); fillColor(th->bg); fill();
        if (th->halftone && haloImage.isValid()) {
            Paint p = imagePattern(0, 0, 6.f, 6.f, 0.f, haloImage, 1.f);
            p.innerColor = th->dots; p.outerColor = th->dots;
            beginPath(); rect(0, 0, W, H); fillPaint(p); fill();
        }
        drawHeader();
        drawTracks();
        drawLcdSection();
        drawMaster();
        drawSequencer();
        if (overlay == ovPicker) drawPicker();
        if (overlay == ovPresets) drawPresets();
        if (overlay == ovLib) drawLibrary();
        if (overlay == ovFx) drawFxMenu();
        if (overlay == ovHelp) drawHelp();
        if (overlay == ovSkins) drawSkins();
        if (overlay == ovSave) drawSave();
        if (overlay == ovLicense) drawLicense();
        if (overlay == ovUpdate) drawUpdate();
        if (overlay == ovDest) drawDestMenu();
        if (tipShown && overlay == ovNone) drawTip();
        drawGrip();
    }

    void makeTextures()
    {
        // 4x4 tile with one dot (tinted at draw time) and a 6x6 halftone tile
        uint8_t d[4 * 4 * 4] = {};
        for (int i : {5}) { d[i * 4] = 255; d[i * 4 + 1] = 255; d[i * 4 + 2] = 255; d[i * 4 + 3] = 255; }
        dotImage = createImageFromRGBA(4, 4, d, IMAGE_REPEAT_X | IMAGE_REPEAT_Y | (1 << 5));
        uint8_t h[6 * 6 * 4] = {};
        for (int i : {7, 8, 13, 14}) { h[i * 4] = 255; h[i * 4 + 1] = 255; h[i * 4 + 2] = 255; h[i * 4 + 3] = 140; }
        haloImage = createImageFromRGBA(6, 6, h, IMAGE_REPEAT_X | IMAGE_REPEAT_Y | (1 << 5));
    }

    void drawHeader()
    {
        drawLogo();
        drawPatternSettings();

        const int ep = editPattern(), pp = (int)std::lround(gp(gOUT_PATTERN));
        const bool running = gp(gOUT_STEP) >= 0.f;
        char b[48];
        const R pr{700, 9, 130, 40};
        box(pr, th->lcdBg); frame(pr, th->panelEdge, -1.f, th->panelEdgeWidth);
        label(pr.x + 10, pr.cy() + 1, "PAT", 10.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
        if (running && pp != ep) std::snprintf(b, sizeof(b), "A%02d>A%02d", pp + 1, ep + 1);
        else std::snprintf(b, sizeof(b), "A%02d", ep + 1);
        number(pr.x + pr.w - 10, pr.cy() + 1, b, running && pp != ep ? 16.f : 20.f, th->lcdText, ALIGN_RIGHT | ALIGN_MIDDLE);

        const bool play = gp(gPLAY) > 0.5f;
        {
            const R pb = playBtn();
            if (play) box(pb, th->selFill); else { box(pb, th->btnFill); frame(pb, th->btnEdge); }
            iconLabel(pb.cx(), pb.cy() + 1, play ? icStop : icPlay, play ? "STOP" : "PLAY", 12.f, play ? th->selText : th->btnText);
        }
        const R tb = tempoBox();
        box(tb, th->lcdBg); frame(tb, th->panelEdge, -1.f, th->panelEdgeWidth);
        std::snprintf(b, sizeof(b), "%.1f", gp(gTEMPO));
        number(tb.x + 70, tb.cy() + 1, b, 20.f, th->lcdText, ALIGN_RIGHT | ALIGN_MIDDLE);
        label(tb.x + 75, tb.cy() + 2, "BPM", 10.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
        const R sk = skinBtn();
        box(sk, th->btnFill); frame(sk, th->btnEdge);
        label(sk.cx(), sk.y + 12, "SKIN / SIZE", 9.f, th->dim);
        std::snprintf(b, sizeof(b), "%s %d%%", th->name, zoomPercent());
        label(sk.cx(), sk.y + 28, b, 10.5f, th->text);
    }

    void drawTracks()
    {
        const uint32_t leds = (uint32_t)gp(gOUT_LEDS);
        for (int t = 0; t < kTracks; ++t) {
            const R r = trackBtn(t);
            const bool sel = t == selTrack;
            const bool off = machineOf(t) == mOFF;
            const Color fg = sel ? th->selText : (off ? th->faint : th->text);
            if (sel) box(r, th->selFill);
            else { box(r, th->btnFill); frame(r, off ? Color(th->btnEdge, th->bg, 0.5f) : th->btnEdge); }
            if (allShown() && !off) frame(r, th->logo == lgRiso ? th->text : th->accent, -1.f, 2.f);   // ALL: every track follows
            char b[8]; std::snprintf(b, sizeof(b), "%02d", t + 1);
            number(r.x + 7, r.y + 17, b, 17.f, fg, ALIGN_LEFT | ALIGN_MIDDLE);
            const bool lit = (leds >> t) & 1;
            beginPath(); circle(r.x + r.w - 11, r.y + 13, 4);
            const Color ledOn = th->logo == lgNeon ? Color(125, 255, 90) : (sel && th->selFill.red > 0.5f && th->accent.red < 0.2f ? th->selText : th->accent);   // NEON: green LEDs
            fillColor(lit ? ledOn
                          : Color(fg, Color(0, 0, 0, 0.f), 0.25f)); fill();
            if (lit && th->glow) { beginPath(); circle(r.x + r.w - 11, r.y + 13, 9); fillPaint(radialGradient(r.x + r.w - 11, r.y + 13, 2, 9, Color(ledOn, Color(0, 0, 0, 0.f), 0.5f), Color(0, 0, 0, 0.f))); fill(); }
            label(r.x + 7, r.y + 36, kMachineDefs[machineOf(t)].name, 10.f, fg, ALIGN_LEFT | ALIGN_MIDDLE);
            const bool muted = bank->mute[t] != 0;
            const R mh = muteBtn(t);
            const R m{mh.x + 6, mh.y + 9, mh.w - 10, mh.h - 13};
            if (muted) box(m, th->lock, 2.f); else frame(m, Color(fg, Color(0, 0, 0, 0.f), 0.6f), 2.f);
            label(m.cx(), m.cy() + 1, "M", 9.f, muted ? th->selText : fg, ALIGN_CENTER | ALIGN_MIDDLE, fnMono);
            {   // solo
                const bool solo = bank->solo[t] != 0;
                const R sh = soloBtn(t);
                const R so{sh.x + (m.x - mh.x), m.y, m.w, m.h};
                if (solo) box(so, th->accent, 2.f); else frame(so, Color(fg, Color(0, 0, 0, 0.f), 0.6f), 2.f);
                label(so.cx(), so.cy() + 1, "S", 9.f, solo ? th->selText : fg, ALIGN_CENTER | ALIGN_MIDDLE, fnMono);
            }
            if (muted) { beginPath(); rect(r.x + 4, r.y + r.h - 4, r.w - 8, 2); fillColor(th->lock); fill(); }
            else if (trackSilenced(*bank, t)) { beginPath(); rect(r.x + 4, r.y + r.h - 4, r.w - 8, 2); fillColor(th->faint); fill(); }   // silent: another track is soloed
        }
    }

    // ------------------------------------------------------------------ LCD graphics
    // The upper display draws what the knobs of the page do (waveform, envelope, filter curve...)
    // instead of repeating their values.
    static Color withAlpha(Color c, float a) { c.alpha *= a; return c; }

    // value shown for track parameter pi: lock of the edited step, value being recorded, else the base value
    float shownValue(int pi)
    {
        if (pi < 0) return 0.f;
        if (lockStep >= 0) { const uint8_t l = pat().locks[selTrack][lockStep][pi]; if (l != kNoLock) return l; }
        if (recording() && drag == dkTrackKnob && recDirty && pageParam(page, dragIndex) == pi) return recVal;
        return tp(selTrack, pi);
    }
    bool shownLocked(int pi) { return lockStep >= 0 && pi >= 0 && pat().locks[selTrack][lockStep][pi] != kNoLock; }
    Color ink(int pi) { return shownLocked(pi) ? th->lock : th->lcdText; }

    void lcdGrid(const R& g, int nx, int ny)
    {
        const Color c = withAlpha(th->lcdDim, 0.22f);
        for (int i = 1; i < nx; ++i) { beginPath(); rect(g.x + g.w * i / nx, g.y, 1.f, g.h); fillColor(c); fill(); }
        for (int i = 1; i < ny; ++i) { beginPath(); rect(g.x, g.y + g.h * i / ny, g.w, 1.f); fillColor(c); fill(); }
    }
    // polyline of n points (xs, ys), optional fill down to baseY
    void lcdCurve(const float* xs, const float* ys, int n, const Color& c, float width, float baseY = -1.f)
    {
        if (n < 2) return;
        if (baseY >= 0.f) {
            beginPath(); moveTo(xs[0], baseY);
            for (int i = 0; i < n; ++i) lineTo(xs[i], ys[i]);
            lineTo(xs[n - 1], baseY); closePath();
            fillPaint(linearGradient(0, baseY - 90.f, 0, baseY, withAlpha(c, 0.30f), withAlpha(c, 0.04f))); fill();
        }
        if (th->glow) {
            beginPath(); moveTo(xs[0], ys[0]); for (int i = 1; i < n; ++i) lineTo(xs[i], ys[i]);
            strokeColor(withAlpha(c, 0.25f)); strokeWidth(width + 4.f); lineJoin(ROUND); lineCap(ROUND); stroke();
        }
        beginPath(); moveTo(xs[0], ys[0]); for (int i = 1; i < n; ++i) lineTo(xs[i], ys[i]);
        strokeColor(c); strokeWidth(width); lineJoin(ROUND); lineCap(ROUND); stroke();
    }
    void lcdTag(float x, float y, const char* name, const std::string& val, const Color& c, int align = ALIGN_LEFT)
    {
        label(x, y, name, 9.5f, th->lcdDim, align | ALIGN_MIDDLE, fnMono);
        number(x, y + 15.f, val.c_str(), 13.f, c, align | ALIGN_MIDDLE, fnMono);
    }

    void drawPageGraphic(const R& g)
    {
        switch (page) {
        case pgSynth: drawHit(g); break;
        case pgFilter: drawFilterGraphic(g); break;
        case pgFx: drawFxGraphic(g); break;
        case pgMix: drawMixGraphic(g); break;
        case pgLfo1: case pgLfo2: drawLfoGraphic(g, page - pgLfo1); break;
        default: drawTrackGraphic(g); break;
        }
    }

    // ---- SYNTH: the sound of one hit, rendered by a private copy of the voice
    Voice hitVoice; bool hitReady = false; float hitKey[11] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    const SampleData* hitSample = nullptr;
    std::vector<float> hitMin, hitMax; float hitSeconds = 0.5f;
    void updateHit(int cols)
    {
        float key[11];
        for (int i = 0; i < 8; ++i) key[i] = std::round(shownValue(i));
        key[8] = (float)machineOf(selTrack); key[9] = (float)bank->kit[selTrack][kfFmq]; key[10] = (float)bank->kit[selTrack][kfOsc];
        const SampleData* hs = uiSmp[selTrack].get();
        if (hitReady && std::equal(key, key + 11, hitKey) && (int)hitMin.size() == cols && hs == hitSample) return;
        std::copy(key, key + 11, hitKey);
        hitSample = hs;
        if (!hitReady) { hitVoice.init(48000.f); hitReady = true; }
        hitVoice.machine = -1;
        hitVoice.setMachine((int)key[8]);
        hitVoice.fmQuant = key[9] > 0.5f; hitVoice.oscShape = (int)key[10];
        hitVoice.sample = hs;
        hitVoice.rng.s = 0x1234567u;
        float eff[kLockable];
        for (int p = 0; p < kLockable; ++p) eff[p] = p < 8 ? key[p] : 0.f;
        hitVoice.control(eff);
        hitVoice.trigger(1.f);
        // length shown: up to 2 s, cut when the hit has died away
        const int N = 96000;
        std::vector<float> buf(N, 0.f);
        int last = 2400;
        for (int i = 0; i < N; i += 64) {
            hitVoice.render(buf.data() + i, 64);
            for (int k = i; k < i + 64; ++k) if (std::fabs(buf[k]) > 0.003f) last = k;
            if (!hitVoice.active || i > last + 9600) break;
        }
        const int len = std::min(N, std::max(2400, (int)(last * 1.1f)));
        hitSeconds = len / 48000.f;
        hitMin.assign(cols, 0.f); hitMax.assign(cols, 0.f);
        for (int c = 0; c < cols; ++c) {
            const int a = c * len / cols, b = std::max(a + 1, (c + 1) * len / cols);
            float mn = 0.f, mx = 0.f;
            for (int k = a; k < b; ++k) { mn = std::min(mn, buf[k]); mx = std::max(mx, buf[k]); }
            hitMin[c] = mn; hitMax[c] = mx;
        }
    }
    void drawHit(const R& g)
    {
        lcdGrid(g, 8, 4);
        beginPath(); rect(g.x, g.cy(), g.w, 1.f); fillColor(withAlpha(th->lcdDim, 0.5f)); fill();
        if (machineOf(selTrack) == mOFF) { label(g.cx(), g.cy(), "TRACK OFF", 14.f, th->lcdDim); return; }
        if (machineOf(selTrack) == mSAMPLE && !uiSmp[selTrack]) {
            useFont(fnSerifItalic);
            label(g.cx(), g.cy() - 10, "Drop a sample here", 24.f, th->lcdText, ALIGN_CENTER | ALIGN_MIDDLE, fnSerifItalic);
            label(g.cx(), g.cy() + 20, "OR CLICK LOAD  -  WAV / AIFF", 9.5f, th->lcdDim, ALIGN_CENTER | ALIGN_MIDDLE, fnMono);
            return;
        }
        const int cols = (int)g.w / 2;
        updateHit(cols);
        float pk = 0.02f;
        for (int c = 0; c < cols; ++c) pk = std::max(pk, std::max(-hitMin[c], hitMax[c]));
        bool anyLock = false; for (int i = 0; i < 8; ++i) anyLock = anyLock || shownLocked(i);
        const Color c0 = anyLock ? th->lock : th->lcdText;
        beginPath();
        moveTo(g.x, g.cy());
        for (int c = 0; c < cols; ++c) lineTo(g.x + c * 2.f, g.cy() - hitMax[c] / pk * g.h * 0.46f);
        for (int c = cols - 1; c >= 0; --c) lineTo(g.x + c * 2.f, g.cy() - hitMin[c] / pk * g.h * 0.46f);
        closePath();
        fillColor(withAlpha(c0, 0.75f)); fill();
        if (th->glow) { strokeColor(withAlpha(c0, 0.3f)); strokeWidth(2.f); stroke(); }
        char b[24];
        if (hitSeconds < 1.f) std::snprintf(b, sizeof(b), "%d ms", (int)std::lround(hitSeconds * 1000.f));
        else std::snprintf(b, sizeof(b), "%.1f s", hitSeconds);
        label(g.x + g.w - 4, g.y + g.h - 8, b, 9.5f, th->lcdDim, ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
        if (machineOf(selTrack) == mSAMPLE && uiSmp[selTrack]) {
            const SampleData& sd = *uiSmp[selTrack];
            char nb[96]; std::snprintf(nb, sizeof(nb), "%s  -  %.2f s", sd.name.c_str(), sd.frames() / sd.sr);
            label(g.x + 4, g.y + 8, nb, 10.f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        }
    }

    static float svfMag(int type, float f, float fc, float q)
    {
        const float w = f / fc, k = 1.f / q;
        const float re = 1.f - w * w, im = k * w, den = std::sqrt(re * re + im * im);
        switch (type) {
        case 0: return 1.f / den;
        case 1: { const float k2 = 1.f / 0.6f, re2 = 1.f - w * w, im2 = k2 * w; return 1.f / den / std::sqrt(re2 * re2 + im2 * im2); }
        case 2: return w / den * std::sqrt(k) * 1.4f;
        case 3: return w * w / den;
        default: return std::fabs(1.f - w * w) / den;
        }
    }
    float filterDb(float f, float cutHz, int type, float q, float lowHz) const
    {
        float m = svfMag(type, f, std::min(cutHz, 20000.f), q);
        (void)lowHz;
        return 20.f * std::log10(std::max(m, 1e-6f));
    }
    void drawFilterGraphic(const R& g)
    {
        const R pl{g.x, g.y + 2, g.w - 150, g.h - 4};
        lcdGrid(pl, 3, 4);
        const float cut = shownValue(tpCUTOFF), q = resoQ(shownValue(tpRESO), 0.5f, 12.5f);
        const int type = filterTypeIndex((int)shownValue(tpFTYPE));
        const float cutHz = cutoffHz(cut);
        const float envOct = (shownValue(tpFENV) - 64.f) / 64.f * 6.f;
        const float envHz = std::min(20000.f, cutHz * std::exp2(envOct));
        auto fx = [&](float f) { return pl.x + pl.w * std::log10(f / 20.f) / 3.f; };
        auto dy = [&](float db) { return pl.y + pl.h * (1.f - (clampf(db, -36.f, 24.f) + 36.f) / 60.f); };
        for (float f : {100.f, 1000.f, 10000.f}) {
            label(fx(f) + 3, pl.y + pl.h - 7, f >= 1000.f ? (f >= 10000.f ? "10k" : "1k") : "100", 8.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
            beginPath(); rect(fx(f), pl.y, 1.f, pl.h); fillColor(withAlpha(th->lcdDim, 0.18f)); fill();
        }
        beginPath(); rect(pl.x, dy(0.f), pl.w, 1.f); fillColor(withAlpha(th->lcdDim, 0.45f)); fill();
        const int n = 160;
        float X[n], Y[n], Ye[n];
        for (int i = 0; i < n; ++i) {
            const float f = 20.f * std::pow(1000.f, (float)i / (n - 1));
            X[i] = fx(f);
            Y[i] = dy(filterDb(f, cutHz, type, q, 0.f));
            Ye[i] = dy(filterDb(f, envHz, type, q, 0.f));
        }
        if (std::fabs(envOct) > 0.1f) {
            beginPath(); moveTo(X[0], Ye[0]); for (int i = 1; i < n; ++i) lineTo(X[i], Ye[i]);
            strokeColor(withAlpha(ink(tpFENV), 0.45f)); strokeWidth(1.2f); stroke();
            const float ex0 = fx(std::min(cutHz, envHz)), ex1 = fx(std::max(cutHz, envHz));
            beginPath(); rect(ex0, pl.y, ex1 - ex0, pl.h); fillColor(withAlpha(ink(tpFENV), 0.07f)); fill();
        }
        const bool lk = shownLocked(tpCUTOFF) || shownLocked(tpRESO) || shownLocked(tpFTYPE);
        lcdCurve(X, Y, n, lk ? th->lock : th->lcdText, 2.2f, pl.y + pl.h);
        const float cx = clampf(fx(cutHz), pl.x, pl.x + pl.w);
        beginPath(); rect(cx, pl.y, 1.f, pl.h); fillColor(withAlpha(ink(tpCUTOFF), 0.7f)); fill();
        box({cx > pl.x + pl.w - 90 ? cx - 62 : cx + 2, pl.y + 17, 60, 14}, withAlpha(th->lcdBg, 0.85f), 2.f);
        if (cx > pl.x + pl.w - 90) label(cx - 4, pl.y + 24, (hzText(cutHz) + " Hz").c_str(), 10.f, ink(tpCUTOFF), ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
        else label(cx + 4, pl.y + 24, (hzText(cutHz) + " Hz").c_str(), 10.f, ink(tpCUTOFF), ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        label(pl.x + 4, pl.y + 9, kFilterTypeNames[type], 10.f, ink(tpFTYPE), ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        char b[32];
        std::snprintf(b, sizeof(b), "RESO %d", (int)std::lround(shownValue(tpRESO)));
        label(pl.x + 60, pl.y + 9, b, 10.f, ink(tpRESO), ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        // right: envelope (starts at full, decays)
        const R ev{g.x + g.w - 138, g.y + 4, 138, g.h - 30};
        frame(ev, withAlpha(th->lcdDim, 0.35f), 2.f);
        const float fd = expmap(shownValue(tpFDEC) / 127.f, 0.005f, 3.f);
        const float amt = (shownValue(tpFENV) - 64.f) / 64.f;
        const float base = amt >= 0 ? ev.y + ev.h - 6 : ev.y + 6, peak = base - (ev.h - 12) * amt;
        const float span = std::min(1.f, std::log1p(fd * 10.f) / std::log1p(30.f));
        float EX[33], EY[33];
        for (int i = 0; i < 33; ++i) {
            const float u = i / 32.f;
            EX[i] = ev.x + 6 + (ev.w - 12) * u;
            EY[i] = base + (peak - base) * std::exp(-5.f * u / std::max(0.05f, span));
        }
        lcdCurve(EX, EY, 33, ink(tpFENV), 1.6f);
        std::snprintf(b, sizeof(b), "ENV %+d", (int)std::lround(shownValue(tpFENV) - 64.f));
        label(ev.x, g.y + g.h - 14, b, 9.5f, ink(tpFENV), ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        if (fd < 1.f) std::snprintf(b, sizeof(b), "DEC %dms", (int)std::lround(fd * 1000.f)); else std::snprintf(b, sizeof(b), "DEC %.1fs", fd);
        label(ev.x + ev.w, g.y + g.h - 14, b, 9.5f, ink(tpFDEC), ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
    }

    void drawFxGraphic(const R& g)
    {
        const float cw = (g.w - 20) / 3.f;
        auto cell = [&](int i) { return R{g.x + i * (cw + 10), g.y + 2, cw, g.h - 20}; };
        char b[32];
        // tremolo
        {
            const R c = cell(0); frame(c, withAlpha(th->lcdDim, 0.35f), 2.f);
            const float d = shownValue(tpAMD) / 127.f, hz = expmap(shownValue(tpAMF) / 127.f, 0.5f, 2000.f);
            const float cyc = clampf(1.f + std::log2(hz + 1.f), 1.f, 9.f);
            float X[90], Y[90];
            for (int i = 0; i < 90; ++i) {
                const float u = i / 89.f;
                X[i] = c.x + 4 + (c.w - 8) * u;
                const float a = 1.f - d * (0.5f - 0.5f * std::sin(kTwoPi * cyc * u));
                Y[i] = c.y + c.h - 4 - (c.h - 8) * a;
            }
            lcdCurve(X, Y, 90, ink(tpAMD), 1.6f, c.y + c.h - 2);
            std::snprintf(b, sizeof(b), d > 0.004f ? "%s Hz" : "OFF", hzText(hz).c_str());
            label(c.x, g.y + g.h - 8, "TREMOLO", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
            label(c.x + c.w, g.y + g.h - 8, b, 9.5f, ink(tpAMF), ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
        }
        // EQ bell
        {
            const R c = cell(1); frame(c, withAlpha(th->lcdDim, 0.35f), 2.f);
            const float f0 = expmap(shownValue(tpEQF) / 127.f, 40.f, 16000.f), db = (shownValue(tpEQG) - 64.f) / 64.f * 18.f;
            beginPath(); rect(c.x + 2, c.cy(), c.w - 4, 1.f); fillColor(withAlpha(th->lcdDim, 0.5f)); fill();
            float X[80], Y[80];
            for (int i = 0; i < 80; ++i) {
                const float u = i / 79.f, f = 20.f * std::exp2(10.f * u);
                const float o = std::log2(f / f0);
                X[i] = c.x + 4 + (c.w - 8) * u;
                Y[i] = c.cy() - db * std::exp(-o * o * 1.6f) / 18.f * (c.h * 0.42f);
            }
            lcdCurve(X, Y, 80, ink(tpEQG), 1.8f);
            std::snprintf(b, sizeof(b), "%+.0fdB %s", db, hzText(f0).c_str());
            label(c.x, g.y + g.h - 8, "EQ", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
            label(c.x + c.w, g.y + g.h - 8, b, 9.5f, ink(tpEQF), ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
        }
        // crush (steps) + drive (soft clipping) on one sine cycle
        {
            const R c = cell(2); frame(c, withAlpha(th->lcdDim, 0.35f), 2.f);
            const float dr = shownValue(tpDIST) / 127.f, cr = shownValue(tpSRR);
            const int hold = cr > 0.5f ? (int)expmap(cr / 127.f, 1.f, 64.f) : 1;
            const int n = 96;
            float X[n], Y[n], Xs[n], Ys[n];
            float held = 0.f;
            for (int i = 0; i < n; ++i) {
                const float u = i / (n - 1.f);
                const float s = std::sin(kTwoPi * u) * 1.15f;
                Xs[i] = X[i] = c.x + 4 + (c.w - 8) * u;
                Ys[i] = c.cy() - s * c.h * 0.36f;
                float y = drive(s * 0.8f, dr);
                if (hold > 1) { if (i % std::max(1, hold / 4 + 1) == 0) held = y; y = held; }
                Y[i] = c.cy() - y * c.h * 0.42f;
            }
            beginPath(); moveTo(Xs[0], Ys[0]); for (int i = 1; i < n; ++i) lineTo(Xs[i], Ys[i]);
            strokeColor(withAlpha(th->lcdDim, 0.4f)); strokeWidth(1.f); stroke();
            lcdCurve(X, Y, n, shownLocked(tpDIST) || shownLocked(tpSRR) ? th->lock : th->lcdText, 1.8f);
            std::snprintf(b, sizeof(b), "%d / %d", (int)std::lround(dr * 100.f), (int)std::lround(cr));
            label(c.x, g.y + g.h - 8, "DRIVE/CRUSH", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
            label(c.x + c.w, g.y + g.h - 8, b, 9.5f, th->lcdText, ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
        }
    }

    // ---- MIX: level, pan, velocity response, sends
    void drawMixGraphic(const R& g)
    {
        char b[48];
        // level meter
        const float lv = shownValue(tpVOL) / 127.f;
        const R mtr{g.x + 4, g.y + 6, 16, g.h - 12};
        frame(mtr, withAlpha(th->lcdDim, 0.6f), 1.f);
        box({mtr.x + 2, mtr.y + 2 + (mtr.h - 4) * (1.f - lv), mtr.w - 4, (mtr.h - 4) * lv}, ink(tpVOL), 1.f);
        const float db = lv > 0.001f ? 20.f * std::log10(lv * lv / 0.62f) : -99.f;
        std::snprintf(b, sizeof(b), lv > 0.001f ? "%+.1fdB" : "OFF", db);
        lcdTag(g.x + 30, g.y + 12, "LEVEL", b, ink(tpVOL));
        // pan
        const float pan = shownValue(tpPAN) / 127.f, py = g.y + 62, px0 = g.x + 40, px1 = g.x + 200;
        beginPath(); rect(px0, py, px1 - px0, 1.f); fillColor(withAlpha(th->lcdDim, 0.6f)); fill();
        beginPath(); rect((px0 + px1) * 0.5f, py - 4, 1.f, 9.f); fillColor(withAlpha(th->lcdDim, 0.6f)); fill();
        label(px0 - 6, py, "L", 9.f, th->lcdDim, ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
        label(px1 + 4, py, "R", 9.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        beginPath(); circle(px0 + (px1 - px0) * pan, py, 4.5f); fillColor(ink(tpPAN)); fill();
        label(g.x + 30, g.y + g.h - 8, ("PAN " + valueText(selTrack, tpPAN, shownValue(tpPAN))).c_str(), 9.5f, ink(tpPAN), ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        // velocity response: output level for an input velocity (accent = 1.6)
        const R vr{g.x + 240, g.y + 4, 180, g.h - 24};
        frame(vr, withAlpha(th->lcdDim, 0.35f), 2.f);
        const float sens = shownValue(tpVEL) / 127.f;
        float X[2], Y[2];
        for (int i = 0; i < 2; ++i) {
            const float vel = i == 0 ? 0.f : 1.6f;
            const float gain = std::max(0.f, 1.f + (vel - 1.f) * sens);
            X[i] = vr.x + 4 + (vr.w - 8) * i;
            Y[i] = vr.y + vr.h - 4 - (vr.h - 8) * std::min(1.f, gain / 1.6f);
        }
        lcdCurve(X, Y, 2, ink(tpVEL), 2.f, vr.y + vr.h - 2);
        std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(sens * 100.f));
        label(vr.x, g.y + g.h - 8, "VELOCITY", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        label(vr.x + vr.w, g.y + g.h - 8, b, 9.5f, ink(tpVEL), ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
        // sends
        const R c{g.x + 450, g.y + 4, 210, g.h - 24};
        frame(c, withAlpha(th->lcdDim, 0.35f), 2.f);
        const float sd = shownValue(tpDEL) / 127.f, sr2 = shownValue(tpREV) / 127.f, sc = gp(gCHO_SEND + selTrack) / 127.f;
        const R bars[3] = {{c.x + 10, c.y + 14, c.w - 20, 10}, {c.x + 10, c.y + 40, c.w - 20, 10}, {c.x + 10, c.y + 66, c.w - 20, 10}};
        const float sl[3] = {sd, sr2, sc};
        const Color col[3] = {ink(tpDEL), ink(tpREV), th->lcdText};
        char nm[3][40];
        std::snprintf(nm[0], sizeof(nm[0]), "DELAY  -  %s", kEchoModelNames[echoModelIndex((int)gp(gECHO_MODEL))]);
        std::snprintf(nm[1], sizeof(nm[1]), "REVERB  -  %s", kReverbModelNames[reverbModelIndex((int)gp(gREV_MODEL))]);
        std::snprintf(nm[2], sizeof(nm[2]), "CHORUS  -  %s", kChorusModelNames[chorusModelIndex((int)gp(gCHO_MODEL))]);
        for (int i = 0; i < 3; ++i) {
            frame(bars[i], withAlpha(th->lcdDim, 0.5f), 1.f);
            box({bars[i].x + 2, bars[i].y + 2, (bars[i].w - 4) * sl[i], bars[i].h - 4}, col[i], 1.f);
            label(bars[i].x, bars[i].y - 6, nm[i], 9.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        }
        label(c.x, g.y + g.h - 8, "SENDS TO THE MASTER", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
    }

    static float lfoWave(int shape, float ph, float rnd0, float rnd1)
    {
        switch (shape) {
        case 0: return 1.f - 4.f * std::fabs(ph - 0.5f);
        case 1: return std::sin(kTwoPi * ph);
        case 2: return ph < 0.5f ? 1.f : -1.f;
        case 3: return 2.f * ph - 1.f;
        case 4: return 1.f - 2.f * ph;
        default: return ph < 0.5f ? rnd0 : rnd1;
        }
    }
    R lfoDestBox() const { return {448.f, 228.f, 236.f, 42.f}; }
    void drawLfoGraphic(const R& g, int l)
    {
        const R w{g.x, g.y + 4, 400, g.h - 10};
        lcdGrid(w, 8, 2);
        const float dep = shownValue(lfoParam(l, 1)) / 127.f;
        const int shape = lfoShapeIndex((int)shownValue(lfoParam(l, 2)));
        const float phase = shownValue(lfoParam(l, 7)) / 128.f;
        const int n = 200;
        float X[n], Y[n];
        static const float rnd[5] = {0.6f, -0.3f, 0.9f, -0.8f, 0.2f};
        for (int i = 0; i < n; ++i) {
            const float u = (float)i / (n - 1) * 2.f;      // two cycles
            float ph = u + phase; const int cyc = (int)ph; ph -= (float)cyc;
            const int ci = (int)(u + phase);
            const float v = lfoWave(shape, ph, rnd[ci % 5], rnd[(ci + 2) % 5]);
            X[i] = w.x + w.w * i / (n - 1);
            Y[i] = w.cy() - v * (w.h * 0.46f) * std::max(0.08f, dep);
        }
        beginPath(); rect(w.x, w.cy(), w.w, 1.f); fillColor(withAlpha(th->lcdDim, 0.5f)); fill();
        bool lk = false; for (int k = 0; k < 8; ++k) lk = lk || shownLocked(lfoParam(l, k));
        lcdCurve(X, Y, n, dep < 0.004f ? withAlpha(th->lcdDim, 0.7f) : (lk ? th->lock : lfoInk(l)), 2.f);
        const int type = lfoTypeIndex((int)shownValue(lfoParam(l, 3)));
        if (type == 1) { // RETRIG: the cycle restarts on every note
            for (int c = 0; c < 2; ++c) { beginPath(); rect(w.x + w.w * c / 2.f, w.y, 1.5f, w.h); fillColor(withAlpha(th->accent, 0.8f)); fill(); }
        }
        // right: rate, mode and destination
        const float rx = g.x + 418;
        const bool sync = lfoSynced(shownValue(lfoParam(l, 6)));
        lcdTag(rx, g.y + 10, sync ? "RATE  (TEMPO)" : "RATE", sync ? std::string(kLfoSyncNames[lfoSyncDivIndex((int)shownValue(lfoParam(l, 0)))])
                                                      : valueText(selTrack, lfoParam(l, 0), shownValue(lfoParam(l, 0))) + " Hz", ink(lfoParam(l, 0)));
        lcdTag(rx + 150, g.y + 10, "MODE", kLfoTypes[type], ink(lfoParam(l, 3)));
        const int d = lfoDestIndex((int)shownValue(lfoParam(l, 4)));
        const int trk = lfoTrackIndex((int)shownValue(lfoParam(l, 5)));
        char b[64];
        std::snprintf(b, sizeof(b), "%s %s", trk == 0 ? "THIS TRACK" : (std::string("TRACK ") + std::to_string(trk)).c_str(), "\xE2\x96\xBE");
        const R db = lfoDestBox();
        box(db, withAlpha(th->lcdText, 0.10f), 3.f);
        frame(db, withAlpha(ink(lfoParam(l, 4)), 0.8f), 3.f);
        label(db.x + 8, db.y + 12, "MODULATES", 9.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        label(db.x + db.w - 8, db.y + 12, b, 9.f, th->lcdDim, ALIGN_RIGHT | ALIGN_MIDDLE, fnMono);
        const std::string dn = lfoDestName(selTrack, l, d);
        number(db.x + 8, db.y + 31, dn.c_str(), 15.f, dep < 0.004f ? th->lcdDim : ink(lfoParam(l, 4)), ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        if (dep < 0.004f) label(rx, g.y + g.h - 8, "DEPTH 0: no effect", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        else { std::snprintf(b, sizeof(b), "DEPTH %d%%   PHASE %d\xC2\xB0", (int)std::lround(dep * 100.f), (int)std::lround(phase * 360.f));
               label(rx, g.y + g.h - 8, b, 9.5f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE, fnMono); }
        drawLfoColor(l);
    }

    // ---- TRACK: choke group, output, MIDI, oscillator wave
    void drawTrackGraphic(const R& g)
    {
        static const char* choke[5] = {"-", "A", "B", "C", "D"};
        const int ck = bank->kit[selTrack][kfChoke];
        label(g.x + 4, g.y + 10, "CHOKE GROUP", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        number(g.x + 4, g.y + 48, choke[ck % 5], 40.f, ck ? th->lcdText : th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
        // the other tracks of the group
        std::string others;
        if (ck) for (int u = 0; u < kTracks; ++u) if (u != selTrack && bank->kit[u][kfChoke] == ck) others += (others.empty() ? "" : " ") + std::to_string(u + 1);
        label(g.x + 4, g.y + g.h - 8, ck ? (others.empty() ? "no other track in this group" : ("cuts tracks " + others).c_str()) : "plays freely",
              9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        // output routing as a little block diagram
        const int out = bank->kit[selTrack][kfOut];
        static const char* outs[4] = {"MAIN", "OUT 2", "OUT 3", "OUT 4"};
        for (int i = 0; i < 4; ++i) {
            const R o{g.x + 200 + i * 62.f, g.y + 50, 56, 26};
            if (i == out) box(o, th->lcdText, 3.f); else frame(o, withAlpha(th->lcdDim, 0.5f), 3.f);
            label(o.cx(), o.cy() + 1, outs[i], 9.5f, i == out ? th->lcdBg : th->lcdDim, ALIGN_CENTER | ALIGN_MIDDLE, fnMono);
        }
        char b[32];
        std::snprintf(b, sizeof(b), "T%02d", selTrack + 1);
        const R tb{g.x + 200 + out * 62.f + 8, g.y + 6, 40, 22};
        box(tb, withAlpha(th->lcdText, 0.15f), 3.f);
        label(tb.cx(), tb.cy() + 1, b, 10.f, th->lcdText, ALIGN_CENTER | ALIGN_MIDDLE, fnMono);
        beginPath(); rect(tb.cx(), tb.y + tb.h, 1.5f, g.y + 50 - tb.y - tb.h); fillColor(th->lcdText); fill();
        label(g.x + 200, g.y + g.h - 8, "OUTPUT", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        // MIDI channel and oscillator shape
        char kb[16];
        lcdTag(g.x + 470, g.y + 10, "MIDI", kitValueText(kfMidi, bank->kit[selTrack][kfMidi], kb, sizeof(kb)), th->lcdText);
        lcdTag(g.x + 560, g.y + 10, "FM SNAP", bank->kit[selTrack][kfFmq] ? "ON" : "OFF", th->lcdText);
        const int osc = bank->kit[selTrack][kfOsc];
        const R ow{g.x + 470, g.y + 50, 180, 34};
        float X[60], Y[60];
        for (int i = 0; i < 60; ++i) {
            const float u = i / 59.f, ph = u * 2.f - std::floor(u * 2.f);
            const float v = osc == 0 ? std::sin(kTwoPi * ph) : (osc == 1 ? 1.f - 4.f * std::fabs(ph - 0.5f) : std::tanh(std::sin(kTwoPi * ph) * 6.f));
            X[i] = ow.x + ow.w * u; Y[i] = ow.cy() - v * ow.h * 0.45f;
        }
        lcdCurve(X, Y, 60, th->lcdText, 1.6f);
        label(ow.x, g.y + g.h - 8, "OSC WAVE (tone machines)", 9.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
    }

    // ---- LFO destination menu
    int destLfo = 0;
    R destBox() const { return {130.f, 110.f, 920.f, 500.f}; }
    R destTrackBtn(int i) const { return {156.f + (i % 9) * 96.f, 160.f + (i / 9) * 36.f, 90.f, 30.f}; }
    R destItem(int col, int row) const { return {156.f + col * 222.f, 280.f + row * 36.f, 212.f, 32.f}; }
    void openDestMenu(int l) { destLfo = l; overlay = ovDest; repaint(); }
    void drawDestMenu()
    {
        overlayBox(destBox());
        const R bx = destBox();
        const int l = destLfo;
        char b[64]; std::snprintf(b, sizeof(b), "LFO %d  -  WHAT SHOULD IT MOVE?", l + 1);
        label(bx.x + 26, bx.y + 24, b, 15.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        const int trk = lfoTrackIndex((int)std::lround(knobValueOf(lfoParam(l, 5))));
        const int curD = lfoDestIndex((int)std::lround(knobValueOf(lfoParam(l, 4))));
        for (int i = 0; i <= kTracks; ++i) {
            char tb[16]; if (i == 0) std::snprintf(tb, sizeof(tb), "THIS"); else std::snprintf(tb, sizeof(tb), "T%02d", i);
            button(destTrackBtn(i), tb, trk == i, 11.f);
        }
        const int tt = trk == 0 ? selTrack : trk - 1;
        static const char* cols[4] = {"SYNTH", "FX + FILTER", "MIX + FILTER ENV", "LFOS"};
        for (int c = 0; c < 4; ++c) {
            label(destItem(c, 0).x, 264, cols[c], 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            for (int r = 0; r < 8; ++r) {
                const int d = destAt(c, r);
                if (d < 0) continue;
                const char* nm = d >= kLfoDests ? vdestName(d) : trackParamName(machineOf(tt), d);
                if (!std::strcmp(nm, "-")) continue;
                button(destItem(c, r), nm, d == curD, 10.5f);
            }
        }
    }
    float knobValueOf(int pi)
    {
        if (lockStep >= 0) { const uint8_t lv = pat().locks[selTrack][lockStep][pi]; if (lv != kNoLock) return lv; }
        return tp(selTrack, pi);
    }
    void setTrackParamOrLock(int pi, float v)
    {
        if (lockStep >= 0) writeLock(pi, (int)std::lround(v));
        else setParamGesture(selTrack * kTrackParams + pi, v);
    }
    bool destClick(float x, float y)
    {
        const int l = destLfo;
        for (int i = 0; i <= kTracks; ++i)
            if (destTrackBtn(i).in(x, y)) { setTrackParamOrLock(lfoParam(l, 5), (float)lfoTrackValue(i)); repaint(); return true; }
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 8; ++r) {
                const int d = destAt(c, r);
                if (d < 0 || !destItem(c, r).in(x, y)) continue;
                setTrackParamOrLock(lfoParam(l, 4), (float)lfoDestValue(d));
                if (knobValueOf(lfoParam(l, 1)) < 0.5f) setTrackParamOrLock(lfoParam(l, 1), 40.f);
                overlay = ovNone; repaint(); return true;
            }
        if (!destBox().in(x, y)) { overlay = ovNone; repaint(); }
        return true;
    }

    void drawLcdSection()
    {
        panelBox({14, 140, 690, 344});
        const R L = lcd();
        box(L, th->lcdBg, th->radius + 1.f);
        const int m = machineOf(selTrack);
        char b[64];
        std::snprintf(b, sizeof(b), "T%02d", selTrack + 1);
        number(32, 170, b, 22.f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
        frame(machPrev(), th->lcdText); label(machPrev().cx(), machPrev().cy() + 1, "\xE2\x97\x80", 10.f, th->lcdText, ALIGN_CENTER | ALIGN_MIDDLE, fnSans);
        frame(machNext(), th->lcdText); label(machNext().cx(), machNext().cy() + 1, "\xE2\x96\xB6", 10.f, th->lcdText, ALIGN_CENTER | ALIGN_MIDDLE, fnSans);
        box(machName(), th->nameFill); label(machName().cx(), machName().cy() + 1, kMachineDefs[m].name, 13.f, th->nameText);
        frame(helpBtn(), th->lcdDim); label(helpBtn().cx(), helpBtn().cy() + 1, "?", 13.f, th->lcdText);
        if (!(showSmpLoad() && lockStep < 0 && smpMsgTicks <= 0)) label(378, 169, kPageNames[page], 11.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);   // hidden behind "or drop a file"

        if (lockStep >= 0) {
            const int nsel = selCount();
            if (nsel > 1) std::snprintf(b, sizeof(b), "LOCK %d STEPS", nsel);
            else std::snprintf(b, sizeof(b), "LOCK STEP %02d", lockStep + 1);
            label(586, 169, b, 11.5f, th->lock, ALIGN_RIGHT | ALIGN_MIDDLE);
            frame(lockClear(), th->lock); label(lockClear().cx(), lockClear().cy() + 1, "CLEAR", 10.f, th->lock);
        } else if (smpMsgTicks > 0) {
            label(688, 169, smpMsg.c_str(), 10.f, th->lock, ALIGN_RIGHT | ALIGN_MIDDLE);
        } else if (showSmpLoad()) {
            box(smpLoadBtn(), th->lcdText, 3.f);
            label(smpLoadBtn().cx(), smpLoadBtn().cy() + 1, "LOAD", 10.5f, th->lcdBg);
            frame(libBtn(), th->lcdText, 3.f);
            label(libBtn().cx(), libBtn().cy() + 1, "LIBRARY", 10.5f, th->lcdText);
            label(486, 169, "or drop a file", 10.f, th->lcdDim, ALIGN_RIGHT | ALIGN_MIDDLE);
        } else if (rec) {
            iconLabel(688, 169, icRec, recording() ? "REC: turn knobs to record" : "REC armed: start playback", 10.f,
                      th->logo == lgRiso ? th->text : th->accent, th->fontLabel, ALIGN_RIGHT);
        } else {
            label(688, 169, "right-click a step to lock", 10.f, th->lcdDim, ALIGN_RIGHT | ALIGN_MIDDLE);
        }

        const Pattern& p = pat();
        drawPageGraphic({30.f, 190.f, 660.f, 100.f});

        for (int i = 0; i < kPages; ++i) button(pageTab(i), kPageNames[i], page == i, 11.5f);
        button(allBtn(), "ALL", allShown(), 11.5f);

        for (int k = 0; k < slots(); ++k) {
            const int pi = pageParam(page, k);
            if (sendG(k) >= 0) {
                const float cv = sendValue(k);
                std::string vt = cv < 0.5f ? std::string("OFF") : std::to_string((int)std::lround(cv));
                if (sendG(k) == gKATK) {
                    const int a = (int)std::lround((cv - 64.f) / 63.f * 100.f);
                    vt = a == 0 ? std::string("0") : (a < 0 ? "SOFT " + std::to_string(-a) : "PUNCH " + std::to_string(a));
                }
                drawKnob(knob(k), cv / 127.f, sendG(k) == gCHO_SEND ? "CHORUS" : (sendG(k) == gKATK ? "ATTACK" : "CHAOS"), vt, false,
                         drag == dkSend && dragIndex == k, true);
                if (allShown()) drawAllBadge(knob(k));
                continue;
            }
            if (euclidSlot(k)) {
                const int n = tlen(selTrack);
                char eb[16];
                if (k == 6) std::snprintf(eb, sizeof(eb), eucHits[selTrack] > 0 ? "%d / %d" : "OFF", eucHits[selTrack], n);
                else std::snprintf(eb, sizeof(eb), "%+d", eucRot[selTrack]);
                drawKnob(knob(k), k == 6 ? (float)eucHits[selTrack] / n : 0.5f + (float)eucRot[selTrack] / (2.f * n),
                         k == 6 ? "EUCLID" : "ROTATE", eb, false, drag == dkEuclid && dragIndex == k, true);
                continue;
            }
            if (kitField(k) >= 0) {
                const int f = kitField(k), kv = bank->kit[selTrack][f];
                char kb[16];
                drawKnob(knob(k), (float)kv / kKitMax[f], kKitNames[f], kitValueText(f, kv, kb, sizeof(kb)), false,
                         drag == dkKit && dragIndex == k, true);
                continue;
            }
            if (pi < 0) { drawKnob(knob(k), 0, "", "", false, false, false); continue; }
            const bool locked = lockStep >= 0 && p.locks[selTrack][lockStep][pi] != kNoLock;
            float v = locked ? p.locks[selTrack][lockStep][pi] : tp(selTrack, pi);
            const bool recNow = recording() && drag == dkTrackKnob && dragIndex == k && recDirty;
            if (recNow) v = recVal;
            LfoHit hits[8]; const int nh = lfoHits(selTrack, pi, hits);
            drawKnob(knob(k), v / 127.f, trackParamName(m, pi), valueText(selTrack, pi, v), locked || recNow,
                     drag == dkTrackKnob && dragIndex == k, true, nh > 0 ? lfoLive(v, hits, nh) / 127.f : -1.f,
                     nh > 0 ? lfoInk(hits[0].l) : Color());
            if (nh > 0) drawLfoBadge(knob(k), hits, nh);
            if (allShown() && lockStep < 0) drawAllBadge(knob(k));
        }
    }

    void drawMaster()
    {
        panelBox({710, 140, 460, 344});
        static const char* tabs[kMPages] = {"ECHO", "REVERB", "CHORUS", "CHAOS", "FILTER", "MASTER"};
        for (int i = 0; i < kMPages; ++i) button(mTab(i), tabs[i], mpage == i, 10.f);
        fxTabDots();
        if (fxTabOf(mpage) >= 0) { const R t = mTab(mpage); label(t.x + t.w - 8, t.cy() + 1, "\xE2\x96\xBE", 9.f, th->selText, ALIGN_CENTER | ALIGN_MIDDLE, fnSans); }
        for (int k = 0; k < mSlots(); ++k) {
            const int g = masterParam(mpage, k);
            if (mpage == mpChaos && k == 7) { drawHoldPad(); continue; }
            if (mpage == mpFilter && k == 3) { drawOnPad(); continue; }
            if (mpage == mpEcho && k == 11) { drawFreezePad(); continue; }
            if (g < 0) { drawKnob(mKnob(k), 0, "", "", false, false, false); continue; }
            const float v = gp(g);
            drawKnob(mKnob(k), v / 127.f, masterName(g), masterText(g, v), false,
                     drag == dkMasterKnob && dragIndex == k, true);
        }
        const R in{716, 394, 448, 88};
        box(in, th->inner, th->radius);
        if (!patMsg.empty()) label(728, 412, patMsg.c_str(), 10.5f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        else label(728, 412, "PATTERNS", 10.5f, th->logo == lgRiso ? th->text : th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        button(patMidi(), "MIDI", false, 10.5f);
        button(patCopy(), patClip >= 0 ? "COPIED" : "COPY", false, 10.5f);
        button(patPaste(), "PASTE", false, 10.5f);
        button(patClear(), "CLEAR", false, 10.5f);
        const int ep = editPattern(), pp = (int)std::lround(gp(gOUT_PATTERN));
        const bool running = gp(gOUT_STEP) >= 0.f;
        for (int i = 0; i < kPatterns; ++i) {
            const R r = patBtn(i);
            char b[8]; std::snprintf(b, sizeof(b), "A%02d", i + 1);
            bool hasContent = false;
            for (int t = 0; t < kTracks && !hasContent; ++t)
                for (int s = 0; s < kSteps; ++s) if (bank->patterns[i].flags[t][s]) { hasContent = true; break; }
            const bool sel = i == ep;
            if (sel) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge); }
            number(r.cx(), r.cy() + 1, b, 14.f, sel ? th->selText : th->btnText);
            if (running && i == pp) { beginPath(); rect(r.x + 4, r.y + r.h - 3, r.w - 8, 2); fillColor(th->accent); fill(); }
            if (hasContent && !sel) { beginPath(); circle(r.x + 5, r.y + 5, 1.8f); fillColor(th->dim); fill(); }
        }
    }

    void drawSequencer()
    {
        panelBox({14, 494, 1156, 220});
        static const char* modes[kModes] = {"TRIG", "ACCENT", "SLIDE", "RATCH", "COND", "TIME"};
        for (int i = 0; i < kModes; ++i) button(modeBtn(i), modes[i], mode == i, 10.5f);
        button(fillBtn(), "FILL", fillOn, 10.5f);
        Pattern& p = pat();
        const int len = tlen(selTrack);
        const int pages = (len + 15) / 16;
        if (stepPg >= pages) stepPg = pages - 1;
        const bool samePat = (int)std::lround(gp(gOUT_PATTERN)) == editPattern();
        const bool running = absStep() >= 0;
        const int step = running && samePat ? trackStepNow(selTrack) : -1;
        for (int i = 0; i < kSteps / 16; ++i) {
            char b[16]; std::snprintf(b, sizeof(b), "%d", i + 1);
            const R r = stepPage(i);
            if (i < pages) {
                button(r, b, stepPg == i, 10.5f);
                if (step >= 0 && step / 16 == i) { beginPath(); rect(r.x + 4, r.y + r.h - 3, r.w - 8, 2); fillColor(th->accent); fill(); }
            } else label(r.cx(), r.cy() + 1, b, 10.5f, th->faint);
        }
        char b[32];
        button(lenMinus(), "-", false); button(lenPlus(), "+", false);
        box(lenBox(), th->lcdBg); std::snprintf(b, sizeof(b), "L:%d", len); label(lenBox().cx(), lenBox().cy() + 1, b, 11.f, th->lcdText);

        button(trkCopy(), trackClip.empty() ? "COPY" : "COPIED", false, 10.5f);
        button(trkPaste(), "PASTE", false, 10.5f); button(trkClear(), "CLEAR", false, 10.5f);
        button(lockClrAll(), "-LOCKS", false, 10.5f);
        for (const R& r : {shiftL(), shiftR()}) {   // move the track's steps one step earlier / later
            button(r, "", false, 10.f);
            label(r.cx(), r.cy() + 1, r.x == shiftL().x ? "\xE2\x97\x80" : "\xE2\x96\xB6", 9.f, th->btnText, ALIGN_CENTER | ALIGN_MIDDLE, fnSans);
        }

        for (int i = 0; i < 16; ++i) {
            const int s = stepPg * 16 + i;
            const R r = stepBtn(i);
            const bool inLen = s < len;
            const uint8_t f = p.flags[selTrack][s];
            const bool playing = s == step;
            Color bg = inLen ? ((i / 4) % 2 ? th->stepAlt : th->stepFill) : th->stepOff;
            box(r, bg);
            if (playing) frame(r, th->logo == lgRiso ? th->text : th->selFill, -1.f, 2.5f);
            else if (isSel(s)) frame(r, th->lock, -1.f, s == lockStep ? 3.f : 2.f);
            else frame(r, Color(th->btnEdge, bg, 0.5f));
            const R led{r.x + 8, r.y + 9, r.w - 16, 12};
            if (f & sfTrig && inLen) {
                // NEON: active steps in red (owner's choice), accents in a lighter red
                const bool neon = th->logo == lgNeon;
                const Color lc = (f & sfAccent) ? (neon ? Color(255, 140, 120) : th->lock) : (neon ? Color(236, 36, 50) : th->accent);
                if (th->glow) { beginPath(); rect(led.x - 4, led.y - 4, led.w + 8, led.h + 8); fillPaint(boxGradient(led.x, led.y, led.w, led.h, 2, 8, Color(lc, Color(0, 0, 0, 0.f), 0.5f), Color(0, 0, 0, 0.f))); fill(); }
                box(led, lc, 1.5f);
            } else if (th->dottedLeds) {
                box(led, th->ledOff, 1.5f); dotted({led.x + 1, led.y + 1, led.w - 2, led.h - 2}, Color(th->dots, th->ledOff, inLen ? 0.f : 0.6f));
            } else box(led, inLen ? th->ledOff : Color(th->ledOff, th->bg, 0.5f), 2.f);
            if (f & sfSlide) box({led.x, r.y + 25, led.w, 3}, th->lock, 1.f);
            char nb[8]; std::snprintf(nb, sizeof(nb), "%d", s + 1);
            number(r.cx(), r.y + 45, nb, 18.f, inLen ? th->stepText : th->faint);
            if (p.hasLocks(selTrack, s)) { beginPath(); circle(r.cx(), r.y + 62, 2.8f); fillColor(th->lock); fill(); }
            if (inLen && (f & sfTrig)) {
                std::string ex;
                char eb[16];
                if (p.ratch[selTrack][s] > 1) { std::snprintf(eb, sizeof(eb), "x%d ", p.ratch[selTrack][s]); ex += eb; }
                if (p.cond[selTrack][s]) { ex += kConds[p.cond[selTrack][s]].name; ex += " "; }
                if (p.micro[selTrack][s]) { std::snprintf(eb, sizeof(eb), "%+d", p.micro[selTrack][s]); ex += eb; }
                if (!ex.empty()) label(r.cx(), r.y + 63, ex.c_str(), 9.f, th->extra);
            }
        }
        drawBottomBar();
    }

    R pickerCell(int m) const
    {
        static const int col[kMachines] = {3, 0,0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, 2,2,2,2,2, 3,3,3};
        int row = 0;
        for (int i = 0; i < m; ++i) if (col[i] == col[m]) ++row;
        return {120.f + col[m] * 240.f, 160.f + row * 34.f, 220.f, 30.f};
    }

    void overlayBox(const R& r)
    {
        beginPath(); rect(0, 0, W, H); fillColor(th->overlay); fill();
        box(r, th->panel, th->radius + 3.f); frame(r, th->panelEdge, th->radius + 3.f, th->panelEdgeWidth + 0.5f);
    }

    void drawPicker()
    {
        overlayBox({100, 100, 980, 480});
        char b[64]; std::snprintf(b, sizeof(b), "Machine for track %02d", selTrack + 1);
        number(120, 128, b, 17.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(1060, 128, "Esc or click outside = cancel", 10.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        static const char* heads[4] = {"ANALOG", "FM", "PHYSICAL (PH)", "GENERATORS"};
        for (int c = 0; c < 4; ++c) label(120 + c * 240.f, 150, heads[c], 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        const int cur = machineOf(selTrack);
        for (int m = 0; m < kMachines; ++m) button(pickerCell(m), kMachineDefs[m].name, m == cur, 12.f);
    }

    // ------------------------------------------------------------------ bottom bar / overlays
    void drawBottomBar()
    {
        const R rb = recBtn();
        const bool live = recording();
        if (rec) {
            box(rb, th->accent);
            iconLabel(rb.cx(), rb.cy() + 1, icRec, "REC", 13.f, th->accentText);
            if (live && th->glow) { beginPath(); rect(rb.x - 6, rb.y - 6, rb.w + 12, rb.h + 12); fillPaint(boxGradient(rb.x, rb.y, rb.w, rb.h, 4, 10, Color(th->accent, Color(0, 0, 0, 0.f), 0.45f), Color(0, 0, 0, 0.f))); fill(); }
        } else {
            box(rb, th->btnFill); frame(rb, th->btnEdge, -1.f, 1.5f);
            iconLabel(rb.cx(), rb.cy() + 1, icRec, "REC", 13.f, th->logo == lgRiso ? th->text : th->accent);
        }
        {   // metronome on / off (heard while REC records)
            const R cb = clickBtn();
            if (clickOn) box(cb, Color(th->accent, th->btnFill, 0.55f)); else { box(cb, th->btnFill); frame(cb, th->btnEdge, -1.f, 1.f); }
            label(cb.cx(), cb.cy() + 1, clickOn ? "CLICK ON" : "CLICK OFF", 9.f, clickOn ? th->text : th->dim, ALIGN_CENTER | ALIGN_MIDDLE);
        }
        const R grp = rndGroup();   // ends before the UNDO button
        box(grp, th->inner);
        const Color gt = th->logo == lgRiso ? th->text : th->dim;
        label(134, 645, "RANDOMIZE", 10.f, gt, ALIGN_LEFT | ALIGN_MIDDLE);
        label(134, 663, "undo restores", 9.f, gt, ALIGN_LEFT | ALIGN_MIDDLE);
        static const char* rn[5] = {"SOUND", "ALL SOUNDS", "SEQUENCE", "PATTERN", "EVERYTHING"};
        static const char* rs[5] = {"this track", "no steps", "this track", "all tracks", "kit+fx+steps"};
        for (int i = 0; i < 5; ++i) {
            const R r = rndBtn(i);
            box(r, th->btnFill); frame(r, th->btnEdge);
            label(r.cx(), r.y + 14, rn[i], 10.5f, th->btnText);
            label(r.cx(), r.y + 30, rs[i], 9.f, th->dim);
        }
        const R u = undoBtn();
        const bool undoValid = !undoStack.empty(), redoValid = !redoStack.empty();
        if (undoValid) { box(u, th->btnFill); frame(u, th->lock, -1.f, 1.5f); }
        else { box(u, th->btnFill); frame(u, th->btnEdge); }
        label(u.cx(), u.y + 14, "UNDO", 10.5f, undoValid ? th->lock : th->faint);
        label(u.cx(), u.y + 30, undoValid ? undoStack.back().what.c_str() : (redoValid ? "right-click: redo" : ""), 9.f, th->dim);
        label(996, 640, "PRESET", 9.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        if (th->fontNum == fnSerif) number(996, 666, presetName.c_str(), 19.f, th->text, ALIGN_RIGHT | ALIGN_MIDDLE, fnSerifItalic);
        else number(996, 666, presetName.c_str(), 16.f, th->text, ALIGN_RIGHT | ALIGN_MIDDLE);
        const R pb = presetBtn();
        box(pb, th->btnFill); frame(pb, th->btnEdge, -1.f, 1.5f);
        label(pb.cx() - 8, pb.cy() + 1, "PRESETS", 11.5f, th->btnText);
        beginPath(); moveTo(pb.x + pb.w - 28, pb.cy() - 5); lineTo(pb.x + pb.w - 21, pb.cy() + 1); lineTo(pb.x + pb.w - 28, pb.cy() + 7);
        closePath(); fillColor(th->btnText); fill();
        const R sv = saveBtn();
        box(sv, th->btnFill); frame(sv, th->btnEdge, -1.f, 1.f);
        label(sv.cx(), sv.cy() + 1, "SAVE AS PRESET", 10.5f, th->btnText);
    }

    // ------------------------------------------------------------------ preset browser: tabs, search, favourites
    struct PEntry { bool user; int idx; std::string name; };
    int presetCount() const { return (int)presetView.size(); }
    static std::string lowerStr(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
    static std::string favKey(bool user, const std::string& name) { return std::string(user ? "U " : "F ") + name; }
    bool isFav(bool user, const std::string& name) const { return std::find(favs.begin(), favs.end(), favKey(user, name)) != favs.end(); }
    static std::string favPath() { const std::string d = userDir(); return d.empty() ? d : d + "favourites.txt"; }
    void loadFavs()
    {
        favs.clear();
        FILE* f = fs::open(favPath(), "rb");
        if (!f) return;
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            std::string l(line);
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            if (l.size() > 2 && (l[0] == 'F' || l[0] == 'U') && l[1] == ' ' && !isFav(l[0] == 'U', l.substr(2))) favs.push_back(l);
        }
        std::fclose(f);
    }
    void saveFavs() const
    {
        FILE* f = fs::open(favPath(), "wb");
        if (!f) return;
        for (const auto& l : favs) std::fprintf(f, "%s\n", l.c_str());
        std::fclose(f);
    }
    void toggleFav(const PEntry& e)
    {
        auto it = std::find(favs.begin(), favs.end(), favKey(e.user, e.name));
        if (it != favs.end()) favs.erase(it); else favs.push_back(favKey(e.user, e.name));
        saveFavs();
    }
    int favCount() const
    {
        int n = 0;
        for (int i = 0; i < kNumPresets; ++i) n += isFav(false, kPresets[i].name);
        for (const auto& u : userPresets) n += isFav(true, u);
        return n;
    }
    void rebuildPresetView()
    {
        presetView.clear();
        const std::string q = lowerStr(presetSearch);
        auto match = [&](const std::string& a, const char* d) {
            return q.empty() || lowerStr(a).find(q) != std::string::npos || (d && lowerStr(d).find(q) != std::string::npos); };
        if (presetPage != 1)
            for (int i = 0; i < kNumPresets; ++i)
                if ((presetPage == 0 || isFav(false, kPresets[i].name)) && (presetCat < 0 || kPresets[i].cat == presetCat)
                    && (match(kPresets[i].name, kPresets[i].desc) || match(kPresetCats[kPresets[i].cat], nullptr)))
                    presetView.push_back({false, i, kPresets[i].name});
        std::stable_sort(presetView.begin(), presetView.end(), [](const PEntry& x, const PEntry& y) { return kPresets[x.idx].cat < kPresets[y.idx].cat; });   // grouped by style
        if (presetPage != 0 && presetCat < 0)   // user presets have no style: shown with ALL
            for (int i = 0; i < (int)userPresets.size(); ++i)
                if ((presetPage == 1 || isFav(true, userPresets[i])) && match(userPresets[i], nullptr))
                    presetView.push_back({true, i, userPresets[i]});
        if (presetSel >= (int)presetView.size()) presetSel = -1;
        presetScroll = clampi(presetScroll, 0, std::max(0, ((int)presetView.size() + 1) / 2 - kPresetRows));
    }

    void drawPresets()
    {
        const R bx = presetBox();
        overlayBox(bx);
        number(100, 92, "Presets", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        char b[96];
        button(presetTab(0), "FACTORY", presetPage == 0, 11.f);
        std::snprintf(b, sizeof(b), "USER (%d)", (int)userPresets.size());
        button(presetTab(1), b, presetPage == 1, 11.f);
        std::snprintf(b, sizeof(b), "FAVOURITES (%d)", favCount());
        button(presetTab(2), b, presetPage == 2, 11.f);
        for (int c = -1; c < kPresetCatCount; ++c)
            button(presetCatChip(c + 1), c < 0 ? "ALL" : kPresetCats[c], presetCat == c, 9.5f);
        const R sr = presetSearchBox();
        box(sr, th->lcdBg); frame(sr, presetSearch.empty() ? th->btnEdge : th->lcdText, -1.f, 1.f);
        if (presetSearch.empty()) label(sr.x + 12, sr.cy() + 1, "TYPE TO SEARCH", 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        else {
            const std::string t = presetSearch + "_";
            label(sr.x + 12, sr.cy() + 1, t.c_str(), 11.5f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
            label(sr.x + sr.w - 12, sr.cy() + 1, "ESC = CLEAR", 9.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        }
        const int n = presetCount();
        for (int k = 0; k < kPresetRows * 2; ++k) {
            const int i = k + presetScroll * 2;
            if (i >= n) break;
            const PEntry& e = presetView[i];
            const R r = presetRow(k);
            const bool sel = i == presetSel;
            if (sel) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge); }
            const Color fg = sel ? th->selText : th->text, fd = sel ? th->selText : th->dim;
            if (e.user && presetPage == 1) {
                number(r.x + 14, r.cy() + 1, e.name.c_str(), 15.f, fg, ALIGN_LEFT | ALIGN_MIDDLE);
            } else {
                number(r.x + 12, r.y + 13, e.name.c_str(), 14.f, fg, ALIGN_LEFT | ALIGN_MIDDLE);
                if (e.user) label(r.x + 12, r.y + 29, "user preset", 9.f, fd, ALIGN_LEFT | ALIGN_MIDDLE);
                else {
                    std::string d = std::string(kPresetCats[kPresets[e.idx].cat]) + "  -  " + kPresets[e.idx].desc;
                    const int f = th->fontLabel;   // fit the line inside the row (before the star)
                    useFont(f); fontSize(textSize(9.f, f)); textLetterSpacing(f == fnMono ? th->labelSpacing * (9.f / 12.f) : 0.f);
                    DGL_NAMESPACE::Rectangle<float> bb; bool cut = false;
                    while (d.size() > 8) { textBounds(0, 0, d.c_str(), nullptr, bb); if (bb.getWidth() <= r.w - 60.f) break; d.pop_back(); cut = true; }
                    textLetterSpacing(0.f);
                    if (cut) { while (!d.empty() && d.back() == ' ') d.pop_back(); d += "..."; }
                    label(r.x + 12, r.y + 29, d.c_str(), 9.f, fd, ALIGN_LEFT | ALIGN_MIDDLE);
                }
            }
            const bool fav = isFav(e.user, e.name);
            label(r.x + r.w - 20, r.cy() + 1, fav ? "\xE2\x98\x85" : "\xE2\x98\x86", 16.f, fav && !sel ? th->accent : fd,
                  ALIGN_CENTER | ALIGN_MIDDLE, fnSans);
        }
        if (n == 0) {
            if (!presetSearch.empty()) label(bx.cx(), 300, "Nothing matches the search.", 13.f, th->text);
            else if (presetPage == 1) {
                label(bx.cx(), 300, "No user presets yet.", 13.f, th->text);
                label(bx.cx(), 330, "Use SAVE CURRENT AS... (or SAVE AS PRESET on the main panel).", 11.f, th->dim);
            } else if (presetPage == 2) {
                label(bx.cx(), 300, "No favourites yet.", 13.f, th->text);
                label(bx.cx(), 330, "Click the star of a preset to add it here.", 11.f, th->dim);
            }
        }
        if (n > kPresetRows * 2) std::snprintf(b, sizeof(b), "double-click = load everything   scroll for more (%d-%d of %d)", presetScroll * 2 + 1, std::min(n, presetScroll * 2 + kPresetRows * 2), n);
        else std::snprintf(b, sizeof(b), "double-click = load everything");
        label(1080, 600, b, 9.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        label(100, 600, presetMsg.c_str(), 10.f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        const bool ok = presetSel >= 0 && presetSel < n;
        button(presetSaveNew(), "SAVE CURRENT AS...", false, 10.5f);
        if (ok && presetView[presetSel].user) button(presetDelete(), deleteArmed ? "CONFIRM?" : "DELETE", deleteArmed, 10.5f);
        button(presetLoadAll(), "LOAD ALL", ok, 10.5f);
        button(presetLoadKit(), "LOAD SOUND", false, 10.5f);
        button(presetLoadPat(), "LOAD PATTERNS", false, 10.5f);
        button(presetCancel(), "CLOSE", false, 10.5f);
    }

    void openPresets()
    {
        refreshUserPresets(); loadFavs();
        overlay = ovPresets; presetSel = -1; presetScroll = 0; deleteArmed = false; presetMsg.clear();
        presetSearch.clear(); rebuildPresetView();
        repaint();
    }

    void presetClick(float x, float y, bool dbl)
    {
        for (int t = 0; t < 3; ++t)
            if (presetTab(t).in(x, y)) {
                presetPage = t; presetSel = -1; presetScroll = 0; deleteArmed = false; presetMsg.clear();
                if (t != 0) refreshUserPresets();
                rebuildPresetView(); repaint(); return;
            }
        for (int c = -1; c < kPresetCatCount; ++c)
            if (presetCatChip(c + 1).in(x, y)) { presetCat = presetCat == c ? -1 : c; presetSel = -1; presetScroll = 0; rebuildPresetView(); repaint(); return; }
        if (presetSearchBox().in(x, y)) {   // a click clears the search
            presetSearch.clear(); presetSel = -1; rebuildPresetView(); repaint(); return;
        }
        const int n = presetCount();
        for (int k = 0; k < kPresetRows * 2; ++k) {
            const int i = k + presetScroll * 2;
            const R r = presetRow(k);
            if (i < n && r.in(x, y)) {
                deleteArmed = false;
                if (x > r.x + r.w - 40) {   // the star: favourite on / off
                    toggleFav(presetView[i]);
                    if (presetPage == 2) { presetSel = -1; rebuildPresetView(); }
                    repaint(); return;
                }
                if (dbl && presetSel == i) { loadAnyPreset(i, true); overlay = ovNone; }
                else { if (presetSel != i) presetSel = i; }
                repaint(); return;
            }
        }
        if (presetSaveNew().in(x, y)) { openSave(); return; }
        const bool ok = presetSel >= 0 && presetSel < n;
        if (ok && presetView[presetSel].user && presetDelete().in(x, y)) {
            if (!deleteArmed) { deleteArmed = true; repaint(); return; }
            const PEntry e = presetView[presetSel];
            fs::remove(userPresetPath(e.name));
            if (isFav(true, e.name)) toggleFav(e);
            presetMsg = "Deleted \"" + e.name + "\"";
            deleteArmed = false; presetSel = -1; refreshUserPresets(); rebuildPresetView(); repaint(); return;
        }
        deleteArmed = false;
        if (ok && presetLoadAll().in(x, y)) { loadAnyPreset(presetSel, true); overlay = ovNone; }
        else if (ok && presetLoadKit().in(x, y)) { loadAnyPreset(presetSel, false); overlay = ovNone; }
        else if (ok && presetLoadPat().in(x, y)) { loadPresetPatterns(presetSel); overlay = ovNone; }
        else if (presetCancel().in(x, y) || !presetBox().in(x, y)) overlay = ovNone;
        repaint();
    }

    void loadAnyPreset(int i, bool withPatterns)
    {
        if (i < 0 || i >= presetCount()) return;
        const PEntry e = presetView[i];
        if (e.user) loadUserPreset(e.name, withPatterns);
        else loadPreset(e.idx, withPatterns);
       
    }

    // ------------------------------------------------------------------ preset helpers
    bool readPresetSound(const PEntry& e, std::vector<float>& nv, Bank& mb)
    {
        nv.assign(values, values + kTotalParams);
        if (!e.user) { std::unique_ptr<Bank> pb(new Bank()); buildPreset(e.idx, nv.data(), *pb); std::memcpy(mb.machine, pb->machine, sizeof(mb.machine)); return true; }
        FILE* fp = fs::open(userPresetPath(e.name), "rb");
        if (!fp) return false;
        std::string all; char buf[4096]; size_t rd;
        while ((rd = std::fread(buf, 1, sizeof(buf), fp)) > 0) all.append(buf, rd);
        std::fclose(fp);
        if (all.rfind("KDSP16-PRESET 2", 0) != 0) return false;   // old layout: load and save it once first
        const size_t bankPos = all.find("\nBANK\n");
        const std::string head = all.substr(0, bankPos == std::string::npos ? all.size() : bankPos);
        size_t st = 0;
        while (st < head.size()) {
            size_t en = head.find('\n', st); if (en == std::string::npos) en = head.size();
            int idx; float v;
            if (std::sscanf(head.c_str() + st, "V %d %f", &idx, &v) == 2 && idx >= 0 && idx < kGlobalBase + gOUT_STEP && std::isfinite(v))
                nv[idx] = idx == kGlobalBase + gTEMPO ? clampf(v, 40.f, 240.f) : clampf(v, 0.f, 127.f);
            st = en + 1;
        }
        if (bankPos != std::string::npos) {
            std::unique_ptr<Bank> fb(new Bank());
            if (deserializeBank(*fb, all.c_str() + bankPos + 6)) std::memcpy(mb.machine, fb->machine, sizeof(mb.machine));
        }
        return true;
    }
    // patterns of a preset only (the sound stays)
    void loadPresetPatterns(int i)
    {
        if (i < 0 || i >= presetCount()) return;
        const PEntry e = presetView[i];
        std::unique_ptr<Bank> pb(new Bank());
        int patIdx = 0;
        if (!e.user) { std::vector<float> tmp(kTotalParams); buildPreset(e.idx, tmp.data(), *pb); }
        else {
            FILE* fp = fs::open(userPresetPath(e.name), "rb");
            if (!fp) { presetMsg = "Could not open " + e.name; return; }
            std::string all; char buf[4096]; size_t rd;
            while ((rd = std::fread(buf, 1, sizeof(buf), fp)) > 0) all.append(buf, rd);
            std::fclose(fp);
            const size_t bankPos = all.find("\nBANK\n");
            if (bankPos == std::string::npos || !deserializeBank(*pb, all.c_str() + bankPos + 6)) { presetMsg = "No patterns in " + e.name; return; }
            const size_t pp = all.find("\nPATTERN ");
            if (pp != std::string::npos && pp < bankPos) patIdx = std::atoi(all.c_str() + pp + 9);
        }
        snapshot("patterns");
        std::unique_ptr<Bank> nb(new Bank(*bank));
        for (int p = 0; p < kPatterns; ++p) nb->patterns[p] = pb->patterns[p];
        applyState(values, nb.get());
        setParamGesture(kGlobalBase + gPATTERN, (float)clampi(patIdx, 0, kPatterns - 1)); stepPg = 0;
        clearSel();
    }


    // ------------------------------------------------------------------ user presets (files)
    static std::string userDir()
    {
       #ifdef _WIN32
        const std::string base = fs::env("APPDATA");
        if (base.empty()) return std::string();
        std::string d = base + "\\Kinetik"; fs::makeDir(d); d += "\\KDSP-16"; fs::makeDir(d); d += "\\Presets"; fs::makeDir(d);
        return d + "\\";
       #else
        const std::string base = fs::env("HOME");
        if (base.empty()) return std::string();
       #ifdef __APPLE__
        std::string d = base + "/Library/Application Support";
       #else
        std::string d = base + "/.config";
       #endif
        fs::makeDir(d); d += "/Kinetik"; fs::makeDir(d); d += "/KDSP-16"; fs::makeDir(d);
        d += "/Presets"; fs::makeDir(d);
        return d + "/";
       #endif
    }
    static std::string cleanName(const std::string& n)
    {
        std::string o;
        for (char c : n) {
            const bool okc = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
            o.push_back(okc ? c : '_');
        }
        while (!o.empty() && o.back() == ' ') o.pop_back();
        while (!o.empty() && o.front() == ' ') o.erase(o.begin());
        return o;
    }
    static std::string userPresetPath(const std::string& name) { return userDir() + name + ".kdsp"; }

    void refreshUserPresets()
    {
        userPresets.clear();
        const std::string d = userDir();
        if (d.empty()) return;
        for (const std::string& file : fs::list(d, ".kdsp")) userPresets.push_back(file.substr(0, file.size() - 5));
        std::sort(userPresets.begin(), userPresets.end(), [](const std::string& a, const std::string& b) {
            std::string la = a, lb = b;
            for (auto& c : la) c = (char)std::tolower((unsigned char)c);
            for (auto& c : lb) c = (char)std::tolower((unsigned char)c);
            return la < lb; });
    }

    bool saveUserPreset(const std::string& name)
    {
        const std::string path = userPresetPath(name);
        FILE* fp = fs::open(path, "wb");
        if (!fp) return false;
        std::fprintf(fp, "KDSP16-PRESET 2\nNAME %s\nPATTERN %d\n", name.c_str(), editPattern());
        for (int i = 0; i < kGlobalBase + gOUT_STEP; ++i) {
            if (i == kGlobalBase + gPLAY || i == kGlobalBase + gPATTERN) continue;
            std::fprintf(fp, "V %d %.4f\n", i, values[i]);
        }
        for (int t = 0; t < kTracks; ++t)
            if (uiSmp[t]) std::fprintf(fp, "SMP %s\n", smp::encodeLine(t, *uiSmp[t]).c_str());
        std::fprintf(fp, "BANK\n%s", serializeBank(*bank).c_str());
        std::fclose(fp);
        return true;
    }

    void loadUserPreset(const std::string& name, bool withPatterns)
    {
        FILE* fp = fs::open(userPresetPath(name), "rb");
        if (!fp) { presetMsg = "Could not open " + name; return; }
        std::string all; char buf[4096]; size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) all.append(buf, n);
        std::fclose(fp);
        if (all.rfind("KDSP16-PRESET", 0) != 0) { presetMsg = "Not a KDSP-16 preset: " + name; return; }
        snapshot("preset");
        std::vector<float> nv(values, values + kTotalParams);
        std::unique_ptr<Bank> nb(new Bank(*bank));
        int patIdx = 0;
        const bool v1 = all.rfind("KDSP16-PRESET 1", 0) == 0;   // before 0.7: 36 params per track, old filter
        std::string smpText;
        const size_t bankPos = all.find("\nBANK\n");
        const std::string head = all.substr(0, bankPos == std::string::npos ? all.size() : bankPos);
        size_t st = 0;
        while (st < head.size()) {
            size_t e = head.find('\n', st); if (e == std::string::npos) e = head.size();
            const std::string line = head.substr(st, e - st);
            int idx; float v;
            if (std::sscanf(line.c_str(), "V %d %f", &idx, &v) == 2 && idx >= 0) {
                if (v1) idx = presetIndexV1(idx);
                // a preset file can come from anywhere: keep only finite values inside the parameter's range
                if (idx < kGlobalBase + gOUT_STEP && std::isfinite(v))
                    nv[idx] = idx == kGlobalBase + gTEMPO ? clampf(v, 40.f, 240.f) : clampf(v, 0.f, 127.f);
            }
            std::sscanf(line.c_str(), "PATTERN %d", &patIdx);
            if (line.rfind("SMP ", 0) == 0) { smpText += line.substr(4); smpText += "\n"; }
            st = e + 1;
        }
        if (v1) for (int t = 0; t < kTracks; ++t) migrateFilterV1(&nv[t * kTrackParams]);
        if (bankPos != std::string::npos) {
            std::unique_ptr<Bank> fileBank(new Bank());
            if (deserializeBank(*fileBank, all.c_str() + bankPos + 6)) {
                std::memcpy(nb->machine, fileBank->machine, sizeof(nb->machine));
                std::memcpy(nb->mute, fileBank->mute, sizeof(nb->mute));
                std::memcpy(nb->solo, fileBank->solo, sizeof(nb->solo));   // no solo left over from before
                std::memcpy(nb->kit, fileBank->kit, sizeof(nb->kit));
                if (withPatterns) for (int p = 0; p < kPatterns; ++p) nb->patterns[p] = fileBank->patterns[p];
            }
        }
        applyState(nv.data(), nb.get());
        if (withPatterns) { setParamGesture(kGlobalBase + gPATTERN, (float)clampi(patIdx, 0, kPatterns - 1)); stepPg = 0; }
        if (!smpText.empty()) {     // the preset carries samples: they replace the current ones
            for (auto& sp : uiSmp) sp.reset();
            smp::decodeAll(smpText.c_str(), [this](int t, SamplePtr d) { if (t >= 0 && t < kTracks) uiSmp[t] = std::move(d); });
            setState("samples", smp::encodeAll(uiSmp, kTracks).c_str());
        }
        clearSel();
        presetName = name;
    }

    // ------------------------------------------------------------------ save-as window (text entry)
    void openSave()
    {
        refreshUserPresets();
        saveName = cleanName(presetName);
        if (saveName.empty() || saveName == "Init" || saveName == "(random)") {
            int k = 1;
            char b[32];
            do { std::snprintf(b, sizeof(b), "My preset %d", k++); } while (std::find(userPresets.begin(), userPresets.end(), std::string(b)) != userPresets.end());
            saveName = b;
        }
        saveConfirm = false;
        overlay = ovSave; repaint();
    }

    bool saveExists() const { return std::find(userPresets.begin(), userPresets.end(), saveName) != userPresets.end(); }

    void commitSave()
    {
        saveName = cleanName(saveName);
        if (saveName.empty()) return;
        if (saveExists() && !saveConfirm) { saveConfirm = true; repaint(); return; }
        if (saveUserPreset(saveName)) {
            presetName = saveName;
            presetMsg = "Saved \"" + saveName + "\"";
            refreshUserPresets();
            presetPage = 1;
        } else presetMsg = "Could not write the preset file";
        overlay = ovNone; repaint();
    }

    void drawSave()
    {
        const R bx = saveBox();
        overlayBox(bx);
        number(320, 236, "Save as preset", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(320, 270, "Name (type on the keyboard, Enter = save, Esc = cancel)", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        const R f = saveField();
        box(f, th->lcdBg); frame(f, th->lcdText, -1.f, 1.5f);
        number(f.x + 16, f.cy() + 1, saveName.c_str(), 20.f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
        // caret
        useFont(th->fontNum); fontSize(th->fontNum == fnSerif ? 25.f : (th->fontNum == fnBlack ? 19.f : 20.f));
        DGL_NAMESPACE::Rectangle<float> bnd; float cx = f.x + 16;
        if (!saveName.empty()) { textBounds(f.x + 16, f.cy(), saveName.c_str(), nullptr, bnd); cx = bnd.getX() + bnd.getWidth() + 3; }
        beginPath(); rect(cx, f.y + 12, 2, f.h - 24); fillColor(th->lcdText); fill();
        if (saveExists())
            label(320, 366, saveConfirm ? "Click SAVE again to replace it." : "A user preset with this name exists: it will be replaced.", 10.5f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        else
            label(320, 366, "Saves the kit, all 16 patterns, effects and tempo.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        button(saveCancel(), "CANCEL", false, 11.f);
        button(saveOk(), saveConfirm ? "REPLACE" : "SAVE", true, 12.f);
    }

    bool presetType(uint c)
    {
        if (c >= 32 && c < 127 && presetSearch.size() < 24 && !(presetSearch.empty() && c == ' ')) {
            presetSearch.push_back((char)c); presetSel = -1; presetScroll = 0; rebuildPresetView(); repaint();
        }
        return true;
    }
    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (overlay == ovPresets) return presetType(ev.character);
        if (overlay != ovSave) return false;
        const uint c = ev.character;
        if (c >= 32 && c < 127 && saveName.size() < 28) {
            const char ch = (char)c;
            const bool okc = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' || ch == '_';
            if (okc) { saveName.push_back(ch); saveConfirm = false; repaint(); }
        }
        return true;
    }

    // ------------------------------------------------------------------ view (skin + size)
    double baseScale() const { const double s = getScaleFactor(); return s > 0.1 ? s : 1.0; }
    int zoomPercent() const { return (int)std::lround(getWidth() / (W * baseScale()) * 100.0); }
    void setZoom(double z)
    {
        z = std::max(0.6, std::min(3.0, z));
        const uint w = (uint)std::lround(W * baseScale() * z), h = (uint)std::lround(H * baseScale() * z);
        if (std::abs((int)w - (int)getWidth()) > 1) setSize(w, h);
        viewDirty = true;
        repaint();
    }
    void saveView()
    {
        char b[32]; std::snprintf(b, sizeof(b), "%d %d", skin, zoomPercent());
        setState("skin", b);      // saved with the project
        writeDefaultView(skin, zoomPercent(), lfoPal, clickOn); // and used by new instances
        viewDirty = false;
    }
    void onResize(const ResizeEvent& ev) override
    {
        UI::onResize(ev);
        viewDirty = true; viewDirtyTicks = 0;
    }

    // ------------------------------------------------------------------ licence (registration)
    // ------------------------------------------------------------------ About / updates window (logo click)
    std::unique_ptr<upd::Checker> updater;
    int updLastState = -1;
    R updBox() const { return {290.f, 150.f, 600.f, 380.f}; }
    R updAuto() const { return {320.f, 400.f, 170.f, 34.f}; }
    R updAutoInst() const { return {496.f, 400.f, 190.f, 34.f}; }
    R updLic() const { return {692.f, 400.f, 168.f, 34.f}; }
    R updMain() const { return {320.f, 456.f, 250.f, 40.f}; }
    R updSkip() const { return {580.f, 456.f, 150.f, 40.f}; }
    R updClose() const { return {740.f, 456.f, 120.f, 40.f}; }
    void drawUpdate()
    {
        const R bx = updBox();
        overlayBox(bx);
        using C = upd::Checker;
        const C::State st = updater->getState();
        const upd::Manifest m = updater->manifest();
        number(320, bx.y + 36, "Kinetik KDSP-16", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(860, bx.y + 36, (std::string("version ") + KN_VERSION).c_str(), 11.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        std::string l1, l2, l3;
        switch (st) {
        case C::stChecking: l1 = "Checking for updates..."; break;
        case C::stAvailable:
            l1 = "Version " + m.version + " is available.";
            l2 = m.notes;
            l3 = m.url.empty() ? "Opens the download page." : "Downloads the installer, checks its signature and installs it (one system prompt).";
            break;
        case C::stUpToDate: l1 = "You have the latest version."; break;
        case C::stDownloading: l1 = "Downloading and checking the installer..."; break;
        case C::stInstalling:
            l1 = "Installing version " + m.version + "...";
            l2 = "Accept the system prompt (administrator password on macOS, Yes on Windows). Keep working: the music does not stop.";
            break;
        case C::stInstalled:
            l1 = "Version " + m.version + " is installed.";
            l2 = "This window still runs the old version: it switches the next time the plug-in loads (reopen the project or restart your music software).";
            break;
        case C::stOpened:
            l1 = "The installer is open.";
            l2 = "Close your music software before you finish the installation,";
            l3 = "then open it again: the new version is loaded.";
            break;
        case C::stFailed: l1 = updater->message(); l2 = "Check your internet connection and try again."; break;
        default: l1 = "Click CHECK NOW to look for a new version."; break;
        }
        // an old copy in the per-user plug-in folder would be loaded instead of the installed one
        if (st == C::stOpened || st == C::stUpToDate || st == C::stIdle) {
            const std::string stray = upd::strayCopy("KinetikKDSP16");
            if (!stray.empty()) {
                l2 = "An older copy is also installed here: " + stray + " - delete it (your music software may load it "
                     "instead of the new version).";
                if (st != C::stOpened) l3.clear();
            }
        }
        label(320, bx.y + 86, l1.c_str(), 13.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        if (!l2.empty()) {
            fontFace(NANOVG_DEJAVU_SANS_TTF); fontSize(12.5f); fillColor(th->dim); textAlign(ALIGN_LEFT | ALIGN_TOP);
            textBox(320, bx.y + 104, 540, l2.c_str(), nullptr);
        }
        if (!l3.empty()) label(320, bx.y + 196, l3.c_str(), 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        const bool autoOn = updater->autoCheck();
        button(updAuto(), autoOn ? "AUTO CHECK: ON" : "AUTO CHECK: OFF", autoOn, 10.5f);
        const bool instOn = updater->autoInstall();
        button(updAutoInst(), instOn ? "AUTO INSTALL: ON" : "AUTO INSTALL: OFF", instOn, 10.5f);
        button(updLic(), licInfo.valid ? "LICENCE: REGISTERED" : "ENTER A LICENCE", false, 10.5f);
        const bool avail = st == C::stAvailable;
        const bool busy = st == C::stChecking || st == C::stDownloading || st == C::stInstalling;
        button(updMain(), busy ? "PLEASE WAIT..." : (avail ? (m.url.empty() ? "OPEN DOWNLOAD PAGE" : "DOWNLOAD & INSTALL") : "CHECK NOW"), avail, 11.f);
        if (avail) button(updSkip(), "SKIP THIS ONE", false, 10.5f);
        button(updClose(), "CLOSE", false, 11.f);
    }
    bool updateClick(float x, float y)
    {
        using C = upd::Checker;
        const C::State st = updater->getState();
        if (updMain().in(x, y)) {
            if (st == C::stAvailable) updater->install();
            else if (st != C::stChecking && st != C::stDownloading && st != C::stInstalling) updater->check(true);
        } else if (st == C::stAvailable && updSkip().in(x, y)) {
            updater->skipThisVersion();
        } else if (updAuto().in(x, y)) {
            updater->setAutoCheck(!updater->autoCheck());
        } else if (updAutoInst().in(x, y)) {
            updater->setAutoInstall(!updater->autoInstall());
        } else if (updLic().in(x, y)) {
            overlay = ovLicense; licMsg.clear();
        } else if (updClose().in(x, y) || !updBox().in(x, y)) {
            overlay = ovNone;
        }
        repaint();
        return true;
    }

    void drawLicense()
    {
        const R bx = licBox();
        overlayBox(bx);
        number(320, bx.y + 36, licInfo.valid ? "Registered" : "Register KDSP-16", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        if (licInfo.valid) {
            label(320, bx.y + 82, ("Licensed to " + licInfo.name).c_str(), 13.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 108, licInfo.email.c_str(), 11.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 134, ("Licence " + licInfo.product + "  -  order " + licInfo.order + "  -  " + licInfo.issued).c_str(), 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 176, "Thank you for supporting Kinetik.", 11.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        } else {
            label(320, bx.y + 80, "This copy runs in demo mode: the sound drops out for 3 seconds every 45 seconds.", 10.5f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 106, "After your purchase you receive a licence file by e-mail.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 130, "Load that file, or copy its text and press PASTE LICENSE (or Ctrl/Cmd+V).", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        }
        label(320, bx.y + 210, licMsg.c_str(), 10.5f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        button(licLoad(), "LOAD LICENSE FILE", false, 10.5f);
        button(licPaste(), "PASTE LICENSE", false, 10.5f);
        button(licClose(), "CLOSE", true, 11.f);
    }

    void applyLicenseText(const std::string& text)
    {
        if (text.empty()) { licMsg = "Nothing to read."; repaint(); return; }
        const lic::Info i = lic::install(text, "KDSP-16");
        if (i.valid) {
            licInfo = i;
            licMsg = "Licence installed. The demo limits are removed.";
            setState("cmd", "lic");
        } else licMsg = i.error;
        repaint();
    }

    void pasteLicense()
    {
        size_t n = 0;
        const void* d = getWindow().getClipboard(n);
        if (d == nullptr || n == 0) { licMsg = "The clipboard is empty. Copy the licence text first."; repaint(); return; }
        std::string t((const char*)d, n);
        while (!t.empty() && t.back() == '\0') t.pop_back();
        applyLicenseText(t);
    }

    // ------------------------------------------------------------------ SAMPLE machine: loading files
    static bool readWholeFile(const std::string& path, std::vector<uint8_t>& out)
    {
#ifdef _WIN32
        // UTF-8 -> UTF-16 (file names with accents)
        std::wstring w;
        for (size_t i = 0; i < path.size();) {
            const unsigned char c = (unsigned char)path[i];
            uint32_t cp = c; size_t n = 1;
            if (c >= 0xF0 && i + 3 < path.size() + 0) { cp = (c & 7u) << 18 | ((unsigned char)path[i + 1] & 63u) << 12 | ((unsigned char)path[i + 2] & 63u) << 6 | ((unsigned char)path[i + 3] & 63u); n = 4; }
            else if (c >= 0xE0 && i + 2 < path.size()) { cp = (c & 15u) << 12 | ((unsigned char)path[i + 1] & 63u) << 6 | ((unsigned char)path[i + 2] & 63u); n = 3; }
            else if (c >= 0xC0 && i + 1 < path.size()) { cp = (c & 31u) << 6 | ((unsigned char)path[i + 1] & 63u); n = 2; }
            if (cp >= 0x10000) { cp -= 0x10000; w += (wchar_t)(0xD800 + (cp >> 10)); w += (wchar_t)(0xDC00 + (cp & 0x3FF)); }
            else w += (wchar_t)cp;
            i += n;
        }
        FILE* fp = _wfopen(w.c_str(), L"rb");
#else
        FILE* fp = std::fopen(path.c_str(), "rb");
#endif
        if (!fp) return false;
        std::fseek(fp, 0, SEEK_END);
        const long n = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        if (n <= 0 || n > 200L * 1024 * 1024) { std::fclose(fp); return false; }
        out.resize((size_t)n);
        const size_t got = std::fread(out.data(), 1, (size_t)n, fp);
        std::fclose(fp);
        return got == (size_t)n;
    }
    static std::string baseName(const std::string& p)
    {
        const size_t a = p.find_last_of("/\\");
        std::string n = a == std::string::npos ? p : p.substr(a + 1);
        const size_t d = n.find_last_of('.');
        return d == std::string::npos ? n : n.substr(0, d);
    }
    bool loadSampleFile(const std::string& path, int t)
    {
        std::vector<uint8_t> bytes;
        unsigned int ch = 0, rate = 0; drwav_uint64 frames = 0;
        float* pcm = nullptr;
        if (readWholeFile(path, bytes))
            pcm = drwav_open_memory_and_read_pcm_frames_f32(bytes.data(), bytes.size(), &ch, &rate, &frames, nullptr);
        if (pcm == nullptr || ch == 0 || rate == 0 || frames == 0) {
            if (pcm) drwav_free(pcm, nullptr);
            smpMsg = "Could not read " + baseName(path) + " (WAV or AIFF only)"; smpMsgTicks = 150; repaint();
            return false;
        }
        auto d = std::make_shared<SampleData>();
        d->sr = rate;
        d->name = baseName(path);
        const drwav_uint64 n = std::min<drwav_uint64>(frames, (drwav_uint64)(kMaxSampleSeconds * rate));
        d->pcm.resize((size_t)n);
        float peak = 0.f;
        for (drwav_uint64 i = 0; i < n; ++i) {
            float v = 0.f;
            for (unsigned int c = 0; c < ch; ++c) v += pcm[i * ch + c];
            v /= (float)ch;
            d->pcm[(size_t)i] = v;
            peak = std::max(peak, std::fabs(v));
        }
        drwav_free(pcm, nullptr);
        // normalize to -1 dBFS so every sample plays as loud as the synthesized drums
        if (peak > 1e-4f) { const float g = 0.89f / peak; for (float& v : d->pcm) v *= g; }
        uiSmp[t] = d;
        setState("smp", smp::encodeLine(t, *d).c_str());
        if (machineOf(t) != mSAMPLE) {
            setMachine(t, mSAMPLE);
            // a fresh SAMPLE track: neutral tone shaping (the previous machine's filter / envelope / crush made
            // the sample sound odd or almost silent); level, pan and sends are kept
            for (int p : {tpAMD, tpAMF, tpEQF, tpEQG, tpCUTOFF, tpFTYPE, tpRESO, tpSRR, tpDIST, tpFENV, tpFDEC, tpVEL})
                setParamGesture(t * kTrackParams + p, kFxDefaults[p - 8]);
        } else {
            // new sample on a SAMPLE track: start, length and reverse refer to the old file, reset them
            for (int k : {1, 2, 5}) setParamGesture(t * kTrackParams + k, kMachineDefs[mSAMPLE].def[k]);
        }
        selTrack = t;
        page = pgSynth;
        smpMsg.clear();
        repaint();
        return true;
    }
    void fileDropped(const std::string& path, double x, double y)
    {
        std::string ext = path.substr(path.find_last_of('.') == std::string::npos ? path.size() : path.find_last_of('.'));
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".kinetik-license" || ext == ".txt") {
            std::string text;
            if (lic::readFile(path, text)) { overlay = ovLicense; applyLicenseText(text); }
            return;
        }
        int t = selTrack;                                  // dropped on a track button: that track
        for (int i = 0; i < kTracks; ++i) if (trackBtn(i).in((float)x, (float)y)) t = i;
        overlay = ovNone;
        loadSampleFile(path, t);
    }
    // ------------------------------------------------------------------ sample LIBRARY (built-in + user folders)
    // FACTORY = the samples installed with the plug-in, one sub-folder per category; USER = Documents/Kinetik/KDSP-16
    // Samples (the user's own, sub-folders too). Click = load on the selected track and play it, double-click = load + close.
    static std::string factorySampleDir()
    {
       #ifdef _WIN32
        std::string pf = fs::env("ProgramW6432"); if (pf.empty()) pf = fs::env("ProgramFiles");
        return pf.empty() ? std::string() : pf + "\\Kinetik\\KinetikKDSP16\\Samples\\";
       #elif defined(__APPLE__)
        return "/Library/Application Support/Kinetik/KDSP-16/Samples/";
       #else
        const std::string h = fs::env("HOME");
        return h.empty() ? std::string() : h + "/.local/share/Kinetik/KDSP-16/Samples/";
       #endif
    }
    static std::string userSampleDir() { return fs::exportDir("KDSP-16 Samples"); }
    static char pathSep() {
       #ifdef _WIN32
        return '\\';
       #else
        return '/';
       #endif
    }
    struct LibCat { std::string name, dir; bool user; };
    std::vector<LibCat> libCats;
    std::vector<std::string> libFiles;          // file names in the selected category
    int libCat = 0, libScroll = 0, libSel = -1;
    std::string libMsg;
    static constexpr int kLibRows = 11;
    R libBox() const { return {80.f, 60.f, 1020.f, 610.f}; }
    R libCatBtn(int i) const { return {100.f, 120.f + i * 34.f, 200.f, 30.f}; }
    R libRow(int k) const { return {320.f + (k % 2) * 380.f, 120.f + (k / 2) * 43.f, 370.f, 38.f}; }
    R libOpenBtn() const { return {720.f, 74.f, 180.f, 30.f}; }
    R libCloseBtn() const { return {910.f, 74.f, 170.f, 30.f}; }
    static bool audioExt(const std::string& f)
    {
        std::string e = f.substr(f.find_last_of('.') == std::string::npos ? f.size() : f.find_last_of('.'));
        for (auto& c : e) c = (char)std::tolower((unsigned char)c);
        return e == ".wav" || e == ".aif" || e == ".aiff";
    }
    static std::vector<std::string> audioFiles(const std::string& dir)
    {
        std::vector<std::string> v;
        for (const auto& f : fs::list(dir, "")) if (audioExt(f) && f[0] != '.') v.push_back(f);
        std::sort(v.begin(), v.end());
        return v;
    }
    void scanLibrary()
    {
        libCats.clear();
        auto addTree = [&](const std::string& root, bool user, const std::string& prefix) {
            if (root.empty()) return;
            std::vector<std::string> subs = fs::listDirs(root);
            std::sort(subs.begin(), subs.end());
            for (const auto& d : subs)
                if (!audioFiles(root + d + pathSep()).empty()) libCats.push_back({prefix + d, root + d + pathSep(), user});
            if (!audioFiles(root).empty()) libCats.push_back({prefix + (user ? "MY SAMPLES" : "OTHER"), root, user});
        };
        addTree(factorySampleDir(), false, "");
        addTree(userSampleDir(), true, "USER: ");
        libCat = clampi(libCat, 0, std::max(0, (int)libCats.size() - 1));
        selectLibCat(libCat);
    }
    void selectLibCat(int i)
    {
        libCat = i; libScroll = 0; libSel = -1;
        libFiles = i >= 0 && i < (int)libCats.size() ? audioFiles(libCats[(size_t)i].dir) : std::vector<std::string>();
    }
    void openLibrary() { overlay = ovLib; libMsg.clear(); scanLibrary(); repaint(); }
    void drawLibrary()
    {
        const R bx = libBox();
        overlayBox(bx);
        number(100, 92, "Sample library", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        char b[96]; std::snprintf(b, sizeof(b), "LOADS ON TRACK %d", selTrack + 1);
        label(420, 92, b, 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        button(libOpenBtn(), "OPEN MY FOLDER", false, 10.5f);
        button(libCloseBtn(), "CLOSE", false, 10.5f);
        for (int i = 0; i < (int)libCats.size() && i < 15; ++i) {
            std::string n = libCats[(size_t)i].name;
            for (auto& c : n) c = (char)std::toupper((unsigned char)c);
            button(libCatBtn(i), n.c_str(), i == libCat, 10.f);
        }
        if (libCats.empty()) {
            label(bx.cx(), 300, "No samples yet.", 14.f, th->text);
            label(bx.cx(), 326, "Put WAV / AIFF files (or folders of them) in Documents / Kinetik / KDSP-16 Samples.", 11.f, th->dim);
            return;
        }
        for (int k = 0; k < kLibRows * 2; ++k) {
            const int i = k + libScroll * 2;
            if (i >= (int)libFiles.size()) break;
            const R r = libRow(k);
            const bool sel = i == libSel;
            if (sel) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge); }
            std::string n = libFiles[(size_t)i];
            const size_t dot = n.find_last_of('.'); if (dot != std::string::npos) n = n.substr(0, dot);
            number(r.x + 12, r.cy() + 1, n.c_str(), 13.f, sel ? th->selText : th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        }
        const int rows = ((int)libFiles.size() + 1) / 2;
        if (rows > kLibRows) {
            std::snprintf(b, sizeof(b), "%d SAMPLES - SCROLL FOR MORE", (int)libFiles.size());
            label(1080, 92, b, 9.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        }
        if (!libMsg.empty()) label(bx.cx(), 652, libMsg.c_str(), 10.5f, th->lock);
    }
    void libClick(float x, float y, bool dbl)
    {
        if (libCloseBtn().in(x, y) || !libBox().in(x, y)) { overlay = ovNone; repaint(); return; }
        if (libOpenBtn().in(x, y)) { upd::openFolder(userSampleDir()); return; }
        for (int i = 0; i < (int)libCats.size() && i < 15; ++i)
            if (libCatBtn(i).in(x, y)) { selectLibCat(i); repaint(); return; }
        for (int k = 0; k < kLibRows * 2; ++k) {
            const int i = k + libScroll * 2;
            if (i >= (int)libFiles.size() || !libRow(k).in(x, y)) continue;
            libSel = i;
            const std::string path = libCats[(size_t)libCat].dir + libFiles[(size_t)i];
            if (loadSampleFile(path, selTrack)) {
                char b[16]; std::snprintf(b, sizeof(b), "aud %d", selTrack); sendCmd(b);   // hear it
                libMsg.clear();
                if (dbl) overlay = ovNone;
            } else libMsg = smpMsg;
            repaint(); return;
        }
    }

    void openSampleBrowser()
    {
        FileBrowserOptions o;
        o.title = "Load a sample (WAV or AIFF)";
        browseSample = true;
        if (!openFileBrowser(o)) { browseSample = false; smpMsg = "File browser not available: drop a file on the window"; smpMsgTicks = 150; }
        repaint();
    }

    void uiFileBrowserSelected(const char* filename) override
    {
        if (browseSample) { browseSample = false; if (filename != nullptr) loadSampleFile(filename, selTrack); return; }
        if (filename == nullptr) return;
        std::string text;
        if (!lic::readFile(filename, text)) { licMsg = "Could not read that file."; repaint(); return; }
        applyLicenseText(text);
    }

    bool licenseClick(float x, float y)
    {
        if (licLoad().in(x, y)) {
            FileBrowserOptions o;
            o.title = "Load your Kinetik licence";
            if (!openFileBrowser(o)) licMsg = "The file browser is not available here: use PASTE LICENSE.";
            repaint(); return true;
        }
        if (licPaste().in(x, y)) { pasteLicense(); return true; }
        if (licClose().in(x, y) || !licBox().in(x, y)) { overlay = ovNone; licMsg.clear(); repaint(); }
        return true;
    }

    void uiIdle() override
    {
        if (!clickSent) { clickSent = true; sendClick(); }
        pollRecorded();
        if (ringsShown && overlay == ovNone && nowSec() - lastRingPaint > 0.033) { lastRingPaint = nowSec(); ringsShown = false; repaint(); }
        if (!tipShown && !tipNone && drag == dkNone && overlay == ovNone && hovX >= 0.f && nowSec() - hovSince > 0.8 && nowSec() > tipBlockUntil) {
            tipText = tipAt(hovX, hovY);
            if (!tipText.empty()) { tipShown = true; repaint(); }
            else tipNone = true;   // nothing here: do not test again until the mouse moves
        }
        flushPv();
        if (updater && (int)updater->getState() != updLastState) { updLastState = (int)updater->getState(); repaint(); }
        if (smpMsgTicks > 0 && --smpMsgTicks == 0) repaint();
        if (!patMsg.empty() && nowSec() > patMsgUntil) { patMsg.clear(); repaint(); }
        // persist the window size a moment after the user stops resizing
        if (viewDirty && drag != (DragKind)dkResize && ++viewDirtyTicks > 30) saveView();
    }

    void drawSkins()
    {
        const R vb = viewBox();
        overlayBox(vb);
        number(264, 124, "Skin and size", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(916, 124, "saved with the project and for new instances", 10.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        for (int i = 0; i < kNumThemes; ++i) {
            const R r = skinRow(i);
            const Theme& t = kThemes[i];
            box(r, t.bg, 4.f); frame(r, i == skin ? th->accent : th->btnEdge, 4.f, i == skin ? 3.f : 1.f);
            const Theme* keep = th; th = &t;
            number(r.x + 14, r.y + 22, t.name, 16.f, t.text, ALIGN_LEFT | ALIGN_MIDDLE);
            // miniature: an LCD value, a knob and a few steps drawn with the skin itself
            box({r.x + 14, r.y + 40, 120, 52}, t.lcdBg);
            label(r.x + 22, r.y + 52, "DEC", 8.f, t.lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
            number(r.x + 22, r.y + 74, "75", 20.f, t.lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
            drawKnob({r.x + 140, r.y + 26, 80, 110}, 0.6f, "", "", false, false, true);
            for (int k = 0; k < 4; ++k) {
                const R st{r.x + 224 + (k % 2) * 40.f, r.y + 40 + (k / 2) * 44.f, 36, 40};
                box(st, t.stepFill);
                box({st.x + 5, st.y + 6, st.w - 10, 7}, k == 0 || k == 3 ? t.accent : t.ledOff, 1.f);
                char nb[4]; std::snprintf(nb, sizeof(nb), "%d", k + 1);
                number(st.cx(), st.y + 27, nb, 12.f, t.stepText);
            }
            th = keep;
        }
        label(264, 468, "WINDOW SIZE", 11.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        static const int zooms[6] = {80, 100, 120, 140, 160, 200};
        const int zp = zoomPercent();
        for (int i = 0; i < 6; ++i) {
            char b[16]; std::snprintf(b, sizeof(b), "%d%%", zooms[i]);
            button(zoomBtn(i), b, std::abs(zp - zooms[i]) <= 2, 12.f);
        }
        label(264, 546, "You can also drag the corner at the bottom right of the window to any size.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        label(264, 572, "Right-click the SKIN / SIZE button to switch skin directly.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
    }

    void drawGrip()
    {
        const R g = gripR();
        const Color c = drag == (DragKind)dkResize ? th->accent : Color(th->dim, th->bg, 0.2f);
        for (int i = 0; i < 3; ++i) {
            const float o = 6.f + i * 5.f;
            beginPath(); moveTo(W - 3, H - o); lineTo(W - o, H - 3);
            strokeColor(c); strokeWidth(1.6f); lineCap(ROUND); stroke();
        }
        (void)g;
    }

    void loadPreset(int i, bool withPatterns)
    {
        snapshot("preset");
        std::vector<float> nv(values, values + kTotalParams);
        std::unique_ptr<Bank> nb(new Bank());
        buildPreset(i, nv.data(), *nb);
        if (!withPatterns) // keep the current patterns, take only the kit
            for (int p = 0; p < kPatterns; ++p) nb->patterns[p] = bank->patterns[p];
        applyState(nv.data(), nb.get());
        if (withPatterns) { setParamGesture(kGlobalBase + gPATTERN, 0.f); stepPg = 0; }
        clearSel();
        presetName = kPresets[i].name;
    }

    // ------------------------------------------------------------------ whole-state helpers (presets, random, undo)
    void applyState(const float* nv, const Bank* nb)
    {
        const bool wasRestoring = restoring; restoring = true;   // the changes below are one undo step, not many
        for (int i = 0; i < kGlobalBase + gOUT_STEP; ++i) {
            if (i == kGlobalBase + gPLAY || i == kGlobalBase + gPATTERN) continue;
            if (std::fabs(values[i] - nv[i]) > 1e-4f) setParamGesture((uint32_t)i, nv[i]);
        }
        if (nb) {
            *bank = *nb;
            setState("bank", serializeBank(*bank).c_str());
        }
        restoring = wasRestoring;
        repaint();
    }

    struct Snap { std::vector<float> values; std::string bank; std::string what; };
    static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
    Snap capture(const char* what) const { Snap s; s.values.assign(values, values + kTotalParams); s.bank = serializeBank(*bank); s.what = what; return s; }
    void snapshot(const char* what)
    {
        if (restoring) return;
        undoStack.push_back(capture(what));
        if (undoStack.size() > kUndoDepth) undoStack.pop_front();
        redoStack.clear();
        lastSnapAt = nowSec(); lastSnapWhat = what;
    }
    // one snapshot per gesture: nothing within 0.15 s of the last one, and the same kind of edit merges for `window` s
    void autoSnap(const char* what, double window)
    {
        if (restoring) return;
        const double t = nowSec();
        if (t - lastSnapAt < 0.15 || (lastSnapWhat == what && t - lastSnapAt < window)) { if (lastSnapWhat == what) lastSnapAt = t; return; }
        snapshot(what);
    }
    void restoreSnap(const Snap& s)
    {
        std::unique_ptr<Bank> b(new Bank());
        deserializeBank(*b, s.bank.c_str());
        std::vector<float> v = s.values;
        applyState(v.data(), b.get());
        lastSnapAt = -10.0; lastSnapWhat.clear();
    }
    void redo()
    {
        if (redoStack.empty()) return;
        Snap s = std::move(redoStack.back()); redoStack.pop_back();
        undoStack.push_back(capture(s.what.c_str()));
        restoreSnap(s);
        repaint();
    }

    void undo()
    {
        if (undoStack.empty()) return;
        Snap s = std::move(undoStack.back()); undoStack.pop_back();
        redoStack.push_back(capture(s.what.c_str()));
        restoreSnap(s);
        repaint();
    }

    void randomize(int what)
    {
        RandGen r(++rndCounter * 2654435761u ^ lastClickTime);
        static const char* names[5] = {"sound", "sequence", "pattern", "everything", "all sounds"};
        snapshot(names[what]);
        std::vector<float> nv(values, values + kTotalParams);
        if (what == 0) {
            randomizeSound(nv.data(), selTrack, machineOf(selTrack), r);
            applyState(nv.data(), nullptr);
        } else if (what == 4) {
            // every track's sound (machines kept, OFF tracks left alone), the sequencer is not touched
            for (int t = 0; t < kTracks; ++t) if (machineOf(t) != mOFF) randomizeSound(nv.data(), t, machineOf(t), r);
            applyState(nv.data(), nullptr);
        } else if (what == 1) {
            std::unique_ptr<Pattern> tmp(new Pattern(pat()));
            randomizeSequence(*tmp, selTrack, machineOf(selTrack), r);
            sendCmd("tr " + std::to_string(editPattern()) + " " + std::to_string(selTrack) + " " + trackToHex(*tmp, selTrack));
        } else {
            std::unique_ptr<Bank> nb(new Bank(*bank));
            if (what == 3) randomizeKit(nv.data(), *nb, r);
            randomizePattern(nb->patterns[editPattern()], *nb, r);
            applyState(nv.data(), nb.get());
        }
        if (what == 2 || what == 3) clearSel();
        presetName = what == 3 ? "(random)" : presetName;
        repaint();
    }

    // ------------------------------------------------------------------ live recording (REC + playing)
    bool recording() const
    {
        return rec && gp(gOUT_STEP) >= 0.f && (int)std::lround(gp(gOUT_PATTERN)) == editPattern();
    }
    void recordLock(int pi, int v)
    {
        const int s = trackStepNow(selTrack);
        if (pi < 0 || s < 0 || s >= kSteps) return;
        Pattern& p = pat();
        if (!(p.flags[selTrack][s] & sfTrig)) return; // like the hardware: locks are recorded onto trigs
        if (p.locks[selTrack][s][pi] == v) return;
        char b[64];
        std::snprintf(b, sizeof(b), "l %d %d %d %d %d", editPattern(), selTrack, s, pi, clampi(v, 0, 127));
        sendCmd(b);
    }
    void recordTrig(int t)
    {
        const int s = trackStepNow(t);
        if (s < 0 || s >= kSteps) return;
        char b[64];
        std::snprintf(b, sizeof(b), "f %d %d %d %d", editPattern(), t, s, pat().flags[t][s] | sfTrig);
        sendCmd(b);
    }

    void setKit(int t, int f, int v)
    {
        char b[48]; std::snprintf(b, sizeof(b), "K %d %d %d", t, f, clampi(v, 0, kKitMax[f])); sendCmd(b); repaint();
    }

    // RATCH / COND / TIME modes: left click = next value, right click = previous, double-click = reset
    void stepExtra(int s, bool right, bool dbl, bool shift)
    {
        // the value is computed from the clicked step, then written to every selected step
        Pattern& p = pat();
        const int pi = editPattern();
        const std::vector<int> targets = editTargets(s);
        std::string c;
        char b[64];
        for (int st : targets) if (!(p.flags[selTrack][st] & sfTrig)) { std::snprintf(b, sizeof(b), "f %d %d %d %d\n", pi, selTrack, st, sfTrig); c += b; }
        char op = 'm'; int v = 0;
        if (mode == mdRatch) {
            int idx = 0;
            for (int i = 0; i < kRatchCount; ++i) if (kRatchValues[i] == std::max<int>(1, p.ratch[selTrack][s])) idx = i;
            idx = dbl ? 0 : (idx + (right ? kRatchCount - 1 : 1)) % kRatchCount;
            op = 'r'; v = kRatchValues[idx];
        } else if (mode == mdCond) {
            op = 'c'; v = dbl ? 0 : (p.cond[selTrack][s] + (right ? kNumConds - 1 : 1)) % kNumConds;
        } else {
            const int st = shift ? 4 : 1;
            v = dbl ? 0 : clampi(p.micro[selTrack][s] + (right ? -st : st), -kMicroMax, kMicroMax);
        }
        for (int st : targets) { std::snprintf(b, sizeof(b), "%c %d %d %d %d\n", op, pi, selTrack, st, v); c += b; }
        if (!c.empty()) { c.pop_back(); sendLines(c); }
    }

    void drawPatternSettings()
    {
        Pattern& p = pat();
        char b[32];
        auto cell = [&](const R& r, const char* top, const char* val) {
            box(r, th->lcdBg); frame(r, th->panelEdge, -1.f, th->panelEdgeWidth);
            label(r.cx(), r.y + 11, top, 9.f, th->lcdDim); number(r.cx(), r.y + 28, val, 15.f, th->lcdText);
        };
        std::snprintf(b, sizeof(b), "%d", p.length); cell(patLenBox(), "PAT LEN", b);
        cell(scaleBox(), "SPEED", kScaleNames[p.scale % 7]);
        std::snprintf(b, sizeof(b), "%d%%", p.swing); cell(swingBox(), "SWING", b);
        std::snprintf(b, sizeof(b), "%d", p.accent); cell(accBox(), "ACCENT", b);
    }

    // ------------------------------------------------------------------ per-track length
    // extend=true copies the existing steps (trigs + locks) into the new steps, like duplicating a page.
    // allTracks (Shift) applies the change to every track of the pattern.
    void setTrackLen(int t, int newLen, bool extend, bool allTracks)
    {
        newLen = clampi(newLen, 1, kSteps);
        std::string cmds;
        for (int tt = 0; tt < kTracks; ++tt) {
            if (!allTracks && tt != t) continue;
            std::unique_ptr<Pattern> tmp(new Pattern(pat()));
            const int old = tmp->trackLen[tt] ? tmp->trackLen[tt] : 16;
            if (extend && newLen > old)
                for (int s = old; s < newLen; ++s) {
                    tmp->flags[tt][s] = tmp->flags[tt][s % old];
                    std::memcpy(tmp->locks[tt][s], tmp->locks[tt][s % old], kLockable);
                }
            tmp->trackLen[tt] = (uint8_t)newLen;
            if (!cmds.empty()) cmds += "\n";
            cmds += "tr " + std::to_string(editPattern()) + " " + std::to_string(tt) + " " + trackToHex(*tmp, tt);
        }
        // keep the master length at least as long as the longest track, so short patterns don't cut long tracks
        int maxLen = 1;
        for (int tt = 0; tt < kTracks; ++tt) maxLen = std::max(maxLen, allTracks || tt == t ? newLen : tlen(tt));
        if (maxLen > pat().length || allTracks) {
            const Pattern& p = pat();
            char b[64]; std::snprintf(b, sizeof(b), "\nP %d %d %d %d %d", editPattern(), allTracks ? newLen : maxLen, p.scale, p.swing, p.accent);
            cmds += b;
        }
        sendLines(cmds);
        dropSelFrom(newLen);
    }

    void drawHelp()
    {
        const R box0{80, 70, 1020, 580};
        overlayBox(box0);
        const int m = machineOf(selTrack);
        char b[96];
        std::snprintf(b, sizeof(b), "%s  -  track %02d  -  %s page", kMachineDefs[m].name, selTrack + 1, kPageNames[page]);
        number(104, 100, b, 19.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(1076, 100, "click anywhere to close", 10.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        const char* intro = page == pgSynth ? kMachineHelp[m].summary
                          : page == pgTrack ? "Track settings saved with the kit (not automatable, not lockable)."
                          : page >= pgLfo1 ? "Each track has two LFOs. Click DEST to choose what it moves (a parameter of this track or of any other track). SYNC = TEMPO locks its rate to the song."
                          : "These parameters exist on every track, whatever the machine. All of them can be locked per step.";
        fontFace(NANOVG_DEJAVU_SANS_TTF); fontSize(14.f); fillColor(th->lock); textAlign(ALIGN_LEFT | ALIGN_TOP);
        textBox(104, 120, 970, intro, nullptr);
        for (int k = 0; k < 8; ++k) {
            const int pi = pageParam(page, k);
            const float y = 178 + k * 52.f;
            if (pi < 0 && kitField(k) < 0) continue;
            const char* nm = kitField(k) >= 0 ? kKitNames[kitField(k)] : trackParamName(m, pi);
            const char* tx = kitField(k) >= 0 ? kKitHelp[kitField(k)] : (pi < 8 ? kMachineHelp[m].p[pi] : kFxHelp[pi - 8]);
            box({104, y, 84, 40}, th->lcdBg);
            label(146, y + 21, nm, 10.5f, th->lcdText);
            fontFace(NANOVG_DEJAVU_SANS_TTF); fontSize(14.f); fillColor(th->text); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            textBox(194, y + 12, 870, tx, nullptr);
            std::snprintf(b, sizeof(b), "KNOB %c", 'A' + k);
            label(194, y + 34, b, 9.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        }
        label(104, 612, "RATCH / COND / TIME step modes: left click = next, right click = previous, double-click = reset.",
              10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
    }

    // ------------------------------------------------------------------ interaction
    void setMachine(int t, int m)
    {
        m = clampi(m, 0, kMachines - 1);
        char b[32]; std::snprintf(b, sizeof(b), "M %d %d", t, m); sendCmd(b);
        for (int k = 0; k < 8; ++k) setParamGesture(t * kTrackParams + k, kMachineDefs[m].def[k]);
        repaint();
    }

    // ---- multi-step selection: each right-click adds a step (or removes it); the knobs then lock every
    // selected step. lockStep = the last selected step (shown in the display).
    std::vector<uint8_t> selMask = std::vector<uint8_t>(kSteps, 0);
    int selCount() const { int n = 0; for (uint8_t v : selMask) n += v; return n; }
    bool isSel(int s) const { return s >= 0 && s < kSteps && selMask[(size_t)s]; }
    void clearSel() { std::fill(selMask.begin(), selMask.end(), (uint8_t)0); lockStep = -1; }
    void toggleSel(int s)
    {
        if (s < 0 || s >= kSteps) return;
        if (selMask[(size_t)s]) {
            selMask[(size_t)s] = 0;
            if (lockStep == s) { lockStep = -1; for (int i = kSteps - 1; i >= 0; --i) if (selMask[(size_t)i]) { lockStep = i; break; } }
        } else { selMask[(size_t)s] = 1; lockStep = s; }
    }
    void dropSelFrom(int len) { for (int i = len; i < kSteps; ++i) selMask[(size_t)i] = 0; if (lockStep >= len) { lockStep = -1; for (int i = len - 1; i >= 0; --i) if (selMask[(size_t)i]) { lockStep = i; break; } } }
    // the steps an edit applies to: the whole selection when s is part of it, else s alone
    std::vector<int> editTargets(int s) const
    {
        std::vector<int> v;
        if (isSel(s)) { for (int i = 0; i < kSteps; ++i) if (selMask[(size_t)i]) v.push_back(i); }
        else v.push_back(s);
        return v;
    }
    bool selLockDiffers(int pi, int iv)
    {
        for (int i = 0; i < kSteps; ++i) if (selMask[(size_t)i] && pat().locks[selTrack][i][pi] != iv) return true;
        return false;
    }

    void writeLock(int pi, int v)
    {
        char b[64];
        Pattern& p = pat();
        std::string c;
        for (int s : editTargets(lockStep)) {
            if (!(p.flags[selTrack][s] & sfTrig) && v != kNoLock) {
                std::snprintf(b, sizeof(b), "f %d %d %d %d\n", editPattern(), selTrack, s, p.flags[selTrack][s] | sfTrig); c += b;
            }
            std::snprintf(b, sizeof(b), "l %d %d %d %d %d\n", editPattern(), selTrack, s, pi, v); c += b;
        }
        if (!c.empty()) { c.pop_back(); sendLines(c); }
    }

    float knobValue(int k)
    {
        const int pi = pageParam(page, k);
        if (pi < 0) return 0;
        if (lockStep >= 0) {
            const uint8_t l = pat().locks[selTrack][lockStep][pi];
            if (l != kNoLock) return l;
        }
        return tp(selTrack, pi);
    }

    void setKnob(int k, float v)
    {
        const int pi = pageParam(page, k);
        if (pi < 0) return;
        v = clampf(v, 0.f, 127.f);
        if (lockStep < 0 && recording()) {
            recVal = v; recDirty = true;
            recordLock(pi, (int)std::lround(v));
        } else if (lockStep >= 0) {
            const int iv = (int)std::lround(v);
            if (selLockDiffers(pi, iv)) writeLock(pi, iv);
        } else {
            setParam(selTrack * kTrackParams + pi, v);
            if (!mIdx.empty()) multiFollow(v);
        }
        repaint();
    }

    void setMaster(int k, float v, bool gesture = false)
    {
        const int g = masterParam(mpage, k);
        if (g < 0) return;
        v = clampf(v, 0.f, 127.f);
        if (gesture) setParamGesture(kGlobalBase + g, v); else setParam(kGlobalBase + g, v);
        repaint();
    }

    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (overlay == ovLicense) {
            if (ev.press && (ev.key == 'v' || ev.key == 'V') && (ev.mod & (kModifierControl | kModifierSuper))) pasteLicense();
            else if (ev.press && ev.key == kKeyEscape) { overlay = ovNone; licMsg.clear(); repaint(); }
            return true;
        }
        if (overlay == ovSave) {
            // the text field takes every key so the host does not react to them
            if (ev.press) {
                if (ev.key == kKeyEscape) { overlay = ovNone; repaint(); }
                else if (ev.key == kKeyEnter || ev.key == '\n') commitSave();
                else if (ev.key == kKeyBackspace || ev.key == kKeyDelete) {
                    if (!saveName.empty()) { saveName.pop_back(); saveConfirm = false; repaint(); }
                }
            }
            return true;
        }
        if (overlay == ovPresets && ev.press) {
            if (ev.key == kKeyBackspace || ev.key == kKeyDelete) {
                if (!presetSearch.empty()) { presetSearch.pop_back(); presetSel = -1; rebuildPresetView(); repaint(); }
                return true;
            }
            if (ev.key == kKeyEscape && !presetSearch.empty()) { presetSearch.clear(); presetSel = -1; rebuildPresetView(); repaint(); return true; }
            if ((ev.key == kKeyEnter || ev.key == '\n') && presetSel >= 0) { loadAnyPreset(presetSel, true); overlay = ovNone; repaint(); return true; }
        }
        if (ev.press && (ev.mod & (kModifierControl | kModifierSuper)) && overlay == ovNone) {
            if (ev.key == 'z' || ev.key == 'Z') { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (ev.key == 'y' || ev.key == 'Y') { redo(); return true; }
        }
        if (ev.press && ev.key == kKeyEscape) {
            if (overlay != ovNone) overlay = ovNone; else clearSel();
            repaint(); return true;
        }
        return false;
    }


    // ------------------------------------------------------------------ tooltips (after a short hover)
    float hovX = -1.f, hovY = -1.f; double hovSince = 0.0, tipBlockUntil = 0.0; bool tipShown = false, tipNone = false; std::string tipText;
    std::string tipAt(float x, float y)
    {
        for (int k = 0; k < slots(); ++k)
            if (knob(k).in(x, y)) {
                if (sendG(k) == gCHO_SEND) return "CHORUS - how much of this track goes to the master chorus (CHORUS tab). Double-click: off.";
                if (sendG(k) == gKATK) return "ATTACK - shape of the kick's start: left = softer (the beat fades in, up to 25 ms), right = punchier (the first milliseconds pushed). Centre = as designed. Double-click: centre.";
                if (sendG(k) == gCHA_SEND) return "CHAOS - how much of this track the CHAOS effect may grab and repeat (CHAOS tab). 0 = never repeated.";
                if (euclidSlot(k)) return k == 6 ? "EUCLID - spreads this many hits evenly over the track. Replaces the trigs of the track (undo restores)."
                                                 : "ROTATE - moves the Euclidean rhythm later or earlier.";
                if (kitField(k) >= 0) return std::string(kKitNames[kitField(k)]) + " - " + kKitHelp[kitField(k)];
                const int pi = pageParam(page, k);
                if (pi < 0) return "";
                const int m = machineOf(selTrack);
                std::string t = std::string(trackParamName(m, pi)) + " - " + (pi < 8 ? kMachineHelp[m].p[pi] : kFxHelp[pi - 8]);
                return t + " Double-click: default. Shift-drag: fine.";
            }
        for (int k = 0; k < mSlots(); ++k)
            if (mKnob(k).in(x, y) && masterParam(mpage, k) >= 0) {
                const int g = masterParam(mpage, k);
                if (g == gECHO_MODEL || g == gECHO_MOD) return std::string("Echo ") + kEchoModelNames[echoModelIndex((int)gp(gECHO_MODEL))] + ": " + kEchoModelInfo[echoModelIndex((int)gp(gECHO_MODEL))];
                if (g == gREV_MODEL || g == gREV_MOD) return std::string("Reverb ") + kReverbModelNames[reverbModelIndex((int)gp(gREV_MODEL))] + ": " + kReverbModelInfo[reverbModelIndex((int)gp(gREV_MODEL))];
                return "";
            }
        if (mpage == mpFilter && mfOnPad().in(x, y)) return "Master filter on / off.";
        if (mpage == mpEcho && freezePad().in(x, y)) return "FREEZE: the echo keeps repeating what it holds, nothing new goes in.";
        for (int i = 0; i < kMPages; ++i) if (mTab(i).in(x, y) && i == mpage && fxTabOf(i) >= 0) return "Click again: presets for this effect.";
        if (mpage == mpChaos && glHoldPad().in(x, y)) return "HOLD: CHAOS repeats the current slice while pressed. Right-click: latch on / off.";
        for (int k = 0; k < mSlots(); ++k)
            if (mKnob(k).in(x, y) && masterParam(mpage, k) >= 0) {
                const int g = masterParam(mpage, k);
                if (g >= gCHO_MODEL && g <= gCHO_LEV) return std::string("Chorus ") + kChorusModelNames[chorusModelIndex((int)gp(gCHO_MODEL))] + ": " + kChorusModelInfo[chorusModelIndex((int)gp(gCHO_MODEL))] + ". Each track sends to it with its CHORUS knob.";
                if (g == gECHO_RTIME) return "R TIME - the right side repeats shorter (left) or longer (right) than the left side. Centre = same time.";
                if (g == gECHO_DRIVE) return "DRIVE - saturation inside the echo loop: the repeats get warmer and denser each time round.";
                if (g == gECHO_DUCK) return "DUCK - the echo backs off while the sound sent to it plays and swells back in the gaps.";
                if (g == gGL_CHANCE) return "CHANCE - how often per bar the glitch grabs a slice of the mix and repeats it (on the beats).";
                if (g == gGL_SIZE) return "SIZE - length of the repeated slice.";
                if (g == gGL_LEN) return "LENGTH - how long one glitch lasts.";
                if (g == gGL_PITCH) return "PITCH - the repeats glide up or down by this many semitones over the glitch.";
                if (g == gGL_FADE) return "FADE - each repeat gets quieter and darker.";
                if (g == gGL_REV) return "REVERSE - chance that a repeat plays backwards.";
                if (g == gGL_MIX) return "MIX - repeats over the music (100% = the music stops while it repeats).";
                if (g >= gMF_HP && g <= gMF_RESO) return "Master filter on the main output: HIGH PASS removes lows, LOW PASS removes highs, RESONANCE peaks at the cutoff.";
            }
        if (undoBtn().in(x, y)) return "Click: undo (Ctrl+Z). Right-click: redo (Ctrl+Shift+Z). Up to 40 steps back.";
        if (trkCopy().in(x, y)) return "Copy this track: its steps and its sound.";
        if (trkPaste().in(x, y)) return "Click: paste the copied steps. Right-click: paste the copied sound.";
        if (trkClear().in(x, y)) return "Remove every step of this track in this pattern.";
        if (patMidi().in(x, y)) return "Export this pattern as a MIDI file (Documents / Kinetik / KDSP-16 MIDI): channel 10, notes 36-51 = tracks 1-16, accents at velocity 127. Drag it from the folder onto a MIDI track.";
        if (lockClrAll().in(x, y)) return "Remove every parameter lock of this track in this pattern.";
        if (shiftL().in(x, y)) return "Move the steps of this track one step earlier.";
        if (shiftR().in(x, y)) return "Move the steps of this track one step later.";
        if (fillBtn().in(x, y)) return "Hold the FILL condition on: steps set to FILL play.";
        if (recBtn().in(x, y)) return "REC: while playing, knob moves are recorded as parameter locks, and the pads you play on a MIDI keyboard (or right-click a track) as trigs.";
        if (clickBtn().in(x, y)) return "CLICK: a metronome on every beat while REC records (main output). Click to turn it on or off.";
        if (allBtn().in(x, y)) return "ALL: knobs move every track at once, each keeping its own offset (or hold Alt / Option while turning). Synth knobs: tracks with the same machine only.";
        { static const char* rt[5] = {"New random sound for this track", "New random sounds on every track (machines and steps unchanged)", "New random steps for this track", "New random pattern on every track", "Random kit, effects and pattern"}; for (int i = 0; i < 5; ++i) if (rndBtn(i).in(x, y)) return std::string(rt[i]) + " (undo restores).";  }
        for (int t = 0; t < kTracks; ++t) {
            if (soloBtn(t).in(x, y)) return "Solo: only the soloed tracks play.";
            if (muteBtn(t).in(x, y)) return "Mute this track.";
            if (trackBtn(t).in(x, y)) return "Click: edit this track. MIDI notes 36-51 play tracks 1-16.";
        }
        for (int i = 0; i < 16; ++i)
            if (stepBtn(i).in(x, y)) return "Click: trig on / off. Right-click: select the step to lock parameters (several with more right-clicks).";
        if (presetBtn().in(x, y)) return "Factory and user presets. Double-click a preset to load sound and patterns.";
        return "";
    }
    void drawTip()
    {
        if (tipText.empty()) return;
        const float wMax = 300.f;
        fontFace(NANOVG_DEJAVU_SANS_TTF); fontSize(12.f); textAlign(ALIGN_LEFT | ALIGN_TOP); textLineHeight(1.25f);
        float b[4]; textBoxBounds(0, 0, wMax, tipText.c_str(), nullptr, b);
        const float w = b[2] - b[0] + 20.f, h = b[3] - b[1] + 14.f;
        float x = hovX + 14.f, y = hovY + 18.f;
        if (x + w > W - 6) x = W - 6 - w;
        if (y + h > H - 6) y = hovY - h - 10.f;
        beginPath(); roundedRect(x + 2, y + 3, w, h, 6); fillColor(Color(0, 0, 0, 0.25f)); fill();
        box({x, y, w, h}, th->panel, 6.f); frame({x, y, w, h}, th->text, 6.f);
        fillColor(th->text); textBox(x + 10, y + 7, wMax, tipText.c_str(), nullptr);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        if (tipShown) { tipShown = false; repaint(); }
        tipBlockUntil = nowSec() + 1.0;   // no tooltip right after a click
        altHeld = (ev.mod & kModifierAlt) != 0;
        if (ev.press) allNow = allLatch || altHeld;   // ALL for the gesture that starts here
        if (!ev.press) {
            if (drag == dkTrackKnob && std::fabs(y - dragY) < 3.f && (page == pgLfo1 || page == pgLfo2) && (dragIndex == 4 || dragIndex == 5)) {
                editParameter(selTrack * kTrackParams + pageParam(page, dragIndex), false);
                multiEnd();
                drag = dkNone; openDestMenu(page - pgLfo1);    // a click (no drag) on DEST / TARGET opens the list
                return true;
            }
            if (drag == dkTrackKnob && lockStep < 0 && !recording()) editParameter(selTrack * kTrackParams + pageParam(page, dragIndex), false);
            multiEnd();
            recDirty = false;
            if (drag == dkMasterKnob) editParameter(kGlobalBase + masterParam(mpage, dragIndex), false);
            if (drag == dkTempo) editParameter(kGlobalBase + gTEMPO, false);
            if (drag == dkSwing || drag == dkAccent) sendPatternSettings();
            if (drag == (DragKind)dkResize) saveView();
            if (drag == dkSend && sendG(dragIndex) >= 0) editParameter(kGlobalBase + sendG(dragIndex) + selTrack, false);
            if (drag == dkSend) multiEnd();
            if (drag == dkGlHold && !glLatch) glHoldSet(false);
            if (drag != dkNone) { drag = dkNone; repaint(); }
            return true;
        }
        const bool dbl = (ev.time - lastClickTime) < 350 && std::fabs(x - lastX) < 4 && std::fabs(y - lastY) < 4;
        lastClickTime = ev.time; lastX = x; lastY = y;
        const bool right = ev.button == kMouseButtonRight;

        if (overlay == ovPicker) {
            for (int m = 0; m < kMachines; ++m)
                if (pickerCell(m).in(x, y)) { setMachine(selTrack, m); break; }
            overlay = ovNone; repaint(); return true;
        }
        if (gripR().in(x, y) && overlay == ovNone) {
            drag = (DragKind)dkResize; resizeStartW = getWidth();
            grabX = ev.pos.getX(); grabY = ev.pos.getY();
            repaint(); return true;
        }
        if (overlay == ovPresets) { presetClick(x, y, dbl); return true; }
        if (overlay == ovLib) { libClick(x, y, dbl); return true; }
        if (overlay == ovFx) { fxMenuClick(x, y); return true; }
        if (overlay == ovDest) return destClick(x, y);
        if (overlay == ovNone && (page == pgLfo1 || page == pgLfo2) && lfoColorClick(page - pgLfo1, x, y)) return true;
        if (overlay == ovNone && (page == pgLfo1 || page == pgLfo2) && lfoDestBox().in(x, y)) { openDestMenu(page - pgLfo1); return true; }
        if (overlay == ovHelp) { overlay = ovNone; repaint(); return true; }
        if (overlay == ovLicense) return licenseClick(x, y);
        if (overlay == ovUpdate) return updateClick(x, y);
        if (overlay == ovNone && logoArea().in(x, y)) { overlay = ovUpdate; repaint(); return true; }
        if (overlay == ovSave) {
            if (saveOk().in(x, y)) commitSave();
            else if (saveCancel().in(x, y) || !saveBox().in(x, y)) { overlay = ovNone; repaint(); }
            return true;
        }
        if (overlay == ovSkins) {
            static const int zooms[6] = {80, 100, 120, 140, 160, 200};
            for (int i = 0; i < kNumThemes; ++i) if (skinRow(i).in(x, y)) { setSkin(i, true); repaint(); return true; }
            for (int i = 0; i < 6; ++i) if (zoomBtn(i).in(x, y)) { setZoom(zooms[i] / 100.0); saveView(); return true; }
            if (!viewBox().in(x, y)) { overlay = ovNone; repaint(); }
            return true;
        }
        if (skinBtn().in(x, y)) {
            if (right) setSkin((skin + 1) % kNumThemes, true); else overlay = ovSkins;
            repaint(); return true;
        }
        if (helpBtn().in(x, y)) { overlay = ovHelp; repaint(); return true; }

        for (int t = 0; t < kTracks; ++t) {
            if (soloBtn(t).in(x, y)) {
                char b[32]; std::snprintf(b, sizeof(b), "S %d %d", t, bank->solo[t] ? 0 : 1); sendCmd(b);
                repaint(); return true;
            }
            if (muteBtn(t).in(x, y)) {
                char b[32]; std::snprintf(b, sizeof(b), "U %d %d", t, bank->mute[t] ? 0 : 1); sendCmd(b);
                repaint(); return true;
            }
            if (trackBtn(t).in(x, y)) {
                if (right) {
                    char b[16]; std::snprintf(b, sizeof(b), "aud %d", t); sendCmd(b);
                    if (recording()) recordTrig(t);
                }
                selTrack = t;
                repaint(); return true;
            }
        }
        if (recBtn().in(x, y)) { setRec(!rec); repaint(); return true; }
        if (clickBtn().in(x, y)) { clickOn = !clickOn; sendClick(); saveView(); repaint(); return true; }
        for (int i = 0; i < 5; ++i) if (rndBtn(i).in(x, y)) { static const int what[5] = {0, 4, 1, 2, 3}; randomize(what[i]); return true; }
        if (undoBtn().in(x, y)) { if (right) redo(); else undo(); return true; }   // right-click = redo
        if (presetBtn().in(x, y)) { openPresets(); return true; }
        if (saveBtn().in(x, y)) { openSave(); return true; }
        if (machPrev().in(x, y)) { setMachine(selTrack, machineOf(selTrack) - 1); return true; }
        if (machNext().in(x, y)) { setMachine(selTrack, machineOf(selTrack) + 1); return true; }
        if (machName().in(x, y)) { overlay = ovPicker; repaint(); return true; }
        if (showSmpLoad() && smpLoadBtn().in(x, y)) { openSampleBrowser(); return true; }
        if (showSmpLoad() && libBtn().in(x, y)) { openLibrary(); return true; }
        if (lockStep >= 0 && lockClear().in(x, y)) {
            std::string c; char b[48];
            for (int st : editTargets(lockStep)) { std::snprintf(b, sizeof(b), "cl %d %d %d\n", editPattern(), selTrack, st); c += b; }
            if (!c.empty()) { c.pop_back(); sendLines(c); }
            repaint(); return true;
        }
        for (int i = 0; i < kPages; ++i) if (pageTab(i).in(x, y)) { page = i; repaint(); return true; }
        if (allBtn().in(x, y)) { allLatch = !allLatch; repaint(); return true; }
        for (int i = 0; i < kMPages; ++i)
            if (mTab(i).in(x, y)) {
                if (mpage == i && fxTabOf(i) >= 0) overlay = ovFx;   // click the active tab: its presets
                else mpage = i;
                repaint(); return true;
            }

        for (int k = 0; k < slots(); ++k)
            if (sendG(k) >= 0 && knob(k).in(x, y)) {
                const uint32_t idx = kGlobalBase + sendG(k) + selTrack;
                if (dbl) {
                    setParamGesture(idx, globalDefault(sendG(k) + selTrack));
                    for (uint32_t o : sendOthers(k)) setParamGesture(o, globalDefault((int)(o - kGlobalBase)));
                    repaint(); return true;
                }
                drag = dkSend; dragIndex = k; dragY = y; dragStart = sendValue(k);
                editParameter(idx, true);
                multiBegin(idx, sendOthers(k));
                repaint(); return true;
            }
        if (mpage == mpFilter && mfOnPad().in(x, y)) { setParamGesture(kGlobalBase + gMF_ON, gp(gMF_ON) > 0.5f ? 0.f : 1.f); repaint(); return true; }
        if (mpage == mpEcho && freezePad().in(x, y)) { setParamGesture(kGlobalBase + gECHO_FREEZE, gp(gECHO_FREEZE) > 0.5f ? 0.f : 1.f); repaint(); return true; }
        if (mpage == mpChaos && glHoldPad().in(x, y)) {
            if (right) { glLatch = !glLatch; glHoldSet(glLatch); return true; }
            glLatch = false; glHoldSet(true); drag = dkGlHold; return true;
        }
        for (int k = 6; k < 8; ++k)
            if (euclidSlot(k) && knob(k).in(x, y)) {
                if (dbl) { (k == 6 ? eucHits : eucRot)[selTrack] = 0; applyEuclid(selTrack); repaint(); return true; }
                drag = dkEuclid; dragIndex = k; dragY = y; dragStart = (float)(k == 6 ? eucHits : eucRot)[selTrack];
                repaint(); return true;
            }
        for (int k = 0; k < 8; ++k) {
            if (!knob(k).in(x, y) || kitField(k) < 0) continue;
            if (dbl) { setKit(selTrack, kitField(k), 0); return true; }
            drag = dkKit; dragIndex = k; dragY = y; dragStart = bank->kit[selTrack][kitField(k)];
            repaint(); return true;
        }
        for (int k = 0; k < slots(); ++k) {
            if (!knob(k).in(x, y) || pageParam(page, k) < 0) continue;
            const int pi = pageParam(page, k);
            if (dbl) {
                if (lockStep >= 0) writeLock(pi, kNoLock);
                else {
                    const float def = pi < 8 ? kMachineDefs[machineOf(selTrack)].def[pi] : kFxDefaults[pi - 8];
                    setParamGesture(selTrack * kTrackParams + pi, def);
                    for (int t = 0; t < kTracks; ++t)
                        if (knobTarget(t, pi)) setParamGesture(tpIndex(t, pi), pi < 8 ? kMachineDefs[machineOf(t)].def[pi] : kFxDefaults[pi - 8]);
                }
                repaint(); return true;
            }
            drag = dkTrackKnob; dragIndex = k; dragY = y; dragStart = knobValue(k); recDirty = false;
            if (lockStep < 0 && !recording()) { editParameter(selTrack * kTrackParams + pi, true); multiBegin(tpIndex(selTrack, pi), knobOthers(pi)); }
            else if (lockStep >= 0 && pat().locks[selTrack][lockStep][pi] == kNoLock) setKnob(k, dragStart);
            repaint(); return true;
        }
        for (int k = 0; k < mSlots(); ++k) {
            const int g = masterParam(mpage, k);
            if (!mKnob(k).in(x, y) || g < 0) continue;
            if (dbl) { setMaster(k, kMasterDefaults[g - gECHO_TIME], true); return true; }
            drag = dkMasterKnob; dragIndex = k; dragY = y; dragStart = gp(g);
            editParameter(kGlobalBase + g, true);
            repaint(); return true;
        }
        if (playBtn().in(x, y)) { setParamGesture(kGlobalBase + gPLAY, gp(gPLAY) > 0.5f ? 0.f : 1.f); repaint(); return true; }
        if (tempoBox().in(x, y)) {
            if (dbl) { setParamGesture(kGlobalBase + gTEMPO, 120.f); repaint(); return true; }
            drag = dkTempo; dragY = y; dragStart = gp(gTEMPO); editParameter(kGlobalBase + gTEMPO, true); return true;
        }
        for (int i = 0; i < kPatterns; ++i)
            if (patBtn(i).in(x, y)) { setParamGesture(kGlobalBase + gPATTERN, (float)i); clearSel(); stepPg = 0; repaint(); return true; }
        if (patCopy().in(x, y)) { patClip = editPattern(); repaint(); return true; }
        if (patPaste().in(x, y) && patClip >= 0) {
            char b[32]; std::snprintf(b, sizeof(b), "pc %d %d", editPattern(), patClip); sendCmd(b); repaint(); return true;
        }
        if (patMidi().in(x, y)) { exportMidi(); repaint(); return true; }
        if (patClear().in(x, y)) { char b[32]; std::snprintf(b, sizeof(b), "cp %d", editPattern()); sendCmd(b); repaint(); return true; }

        for (int i = 0; i < kModes; ++i) if (modeBtn(i).in(x, y)) { mode = i; repaint(); return true; }
        if (fillBtn().in(x, y)) { fillOn = !fillOn; sendCmd(fillOn ? "fill 1" : "fill 0"); repaint(); return true; }
        Pattern& p = pat();
        for (int i = 0; i < kSteps / 16; ++i)
            if (stepPage(i).in(x, y)) {
                const int want = (i + 1) * 16;
                if (dbl) setTrackLen(selTrack, want, false, (bool)(ev.mod & kModifierShift));   // double-click: exact length (can shrink)
                else if (want > tlen(selTrack)) setTrackLen(selTrack, want, true, (bool)(ev.mod & kModifierShift)); // extend, copying the steps
                stepPg = i; repaint(); return true;
            }
        if (lenMinus().in(x, y) || lenPlus().in(x, y)) {
            setTrackLen(selTrack, tlen(selTrack) + (lenPlus().in(x, y) ? 1 : -1), false, (bool)(ev.mod & kModifierShift));
            repaint(); return true;
        }
        if (lenBox().in(x, y)) { // click cycles 16/32/48/64
            setTrackLen(selTrack, ((tlen(selTrack) - 1) / 16 + 1) % (kSteps / 16) * 16 + 16, true, (bool)(ev.mod & kModifierShift));
            repaint(); return true;
        }
        if (patLenBox().in(x, y)) { // master length: when pattern changes happen
            p.length = (uint8_t)(((p.length - 1) / 16 + 1) % (kSteps / 16) * 16 + 16);
            sendPatternSettings(); repaint(); return true;
        }
        if (scaleBox().in(x, y)) { p.scale = (uint8_t)((p.scale + (right ? 6 : 1)) % 7); sendPatternSettings(); repaint(); return true; }
        if (swingBox().in(x, y)) {
            if (dbl) { p.swing = 50; sendPatternSettings(); repaint(); return true; }
            drag = dkSwing; dragY = y; dragStart = p.swing; return true;
        }
        if (accBox().in(x, y)) {
            if (dbl) { p.accent = 64; sendPatternSettings(); repaint(); return true; }
            drag = dkAccent; dragY = y; dragStart = p.accent; return true;
        }
        if (trkCopy().in(x, y)) {   // the steps and the sound of the track
            trackClip = trackToHex(p, selTrack);
            soundClip.assign(&values[selTrack * kTrackParams], &values[selTrack * kTrackParams] + kTrackParams);
            soundClipMachine = machineOf(selTrack);
            repaint(); return true;
        }
        if (trkPaste().in(x, y) && right && !soundClip.empty()) {   // right-click PASTE = the copied sound
            snapshot("paste sound");
            const bool wasR = restoring; restoring = true;
            if (machineOf(selTrack) != soundClipMachine) setMachine(selTrack, soundClipMachine);
            for (int k = 0; k < kTrackParams; ++k) setParamGesture(selTrack * kTrackParams + k, soundClip[(size_t)k]);
            restoring = wasR;
            repaint(); return true;
        }
        for (int dir : {-1, 1})
            if ((dir < 0 ? shiftL() : shiftR()).in(x, y)) {
                std::unique_ptr<Pattern> tmp(new Pattern(p));
                tmp->rotateTrack(selTrack, dir);
                sendCmd("tr " + std::to_string(editPattern()) + " " + std::to_string(selTrack) + " " + trackToHex(*tmp, selTrack));
                repaint(); return true;
            }
        if (trkPaste().in(x, y) && !trackClip.empty()) {
            sendCmd("tr " + std::to_string(editPattern()) + " " + std::to_string(selTrack) + " " + trackClip); repaint(); return true;
        }
        if (trkClear().in(x, y)) { char b[32]; std::snprintf(b, sizeof(b), "ct %d %d", editPattern(), selTrack); sendCmd(b); repaint(); return true; }
        if (lockClrAll().in(x, y)) {
            std::string c;
            for (int s = 0; s < kSteps; ++s)
                if (p.hasLocks(selTrack, s)) { char b[48]; std::snprintf(b, sizeof(b), "cl %d %d %d\n", editPattern(), selTrack, s); c += b; }
            if (!c.empty()) { c.pop_back(); sendLines(c); }
            repaint(); return true;
        }

        for (int i = 0; i < 16; ++i) {
            if (!stepBtn(i).in(x, y)) continue;
            const int s = stepPg * 16 + i;
            if (mode >= mdRatch) { stepExtra(s, right, dbl, (ev.mod & kModifierShift) != 0); repaint(); return true; }
            if (right) { toggleSel(s); repaint(); return true; }
            uint8_t f = p.flags[selTrack][s];
            if (mode == mdTrig) f = (f & sfTrig) ? 0 : sfTrig;
            else if (mode == mdAccent) f = (f & sfAccent) ? (uint8_t)(f & ~sfAccent) : (uint8_t)(f | sfAccent | sfTrig);
            else f = (f & sfSlide) ? (uint8_t)(f & ~sfSlide) : (uint8_t)(f | sfSlide | sfTrig);
            char b[48]; std::snprintf(b, sizeof(b), "f %d %d %d %d", editPattern(), selTrack, s, f);
            sendCmd(b);
            repaint(); return true;
        }
        // a left click on an empty spot ends the step selection
        if (!right && selCount() > 0) { clearSel(); repaint(); }
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (((ev.mod & kModifierAlt) != 0) != altHeld) { altHeld = !altHeld; repaint(); }   // ALL shown while Alt is held
        {   // hover tracking for tooltips
            const float hx = ev.pos.getX() / sx, hy = ev.pos.getY() / sx;
            if (std::fabs(hx - hovX) > 3.f || std::fabs(hy - hovY) > 3.f) {
                hovX = hx; hovY = hy; hovSince = nowSec(); tipNone = false;
                if (tipShown) { tipShown = false; repaint(); }
            }
        }
        if (drag == dkNone) return false;
        if (drag == (DragKind)dkResize) {
            // window pixels: grow along the diagonal, keep the proportions
            const double dx = ev.pos.getX() - grabX, dy = (ev.pos.getY() - grabY) * (double)W / H;
            const double nw = resizeStartW + (dx + dy) * 0.5;
            const double z = std::max(0.6, std::min(3.0, nw / (W * baseScale())));
            const uint w = (uint)std::lround(W * baseScale() * z), h = (uint)std::lround(H * baseScale() * z);
            if (std::abs((int)w - (int)getWidth()) >= 3) setSize(w, h);
            return true;
        }
        const float y = ev.pos.getY() / sx;
        const float sens = (ev.mod & kModifierShift) ? 0.12f : 0.6f;
        const float v = dragStart + (dragY - y) * sens;
        switch (drag) {
        case dkTrackKnob: setKnob(dragIndex, v); break;
        case dkSend:
            if (sendG(dragIndex) >= 0) { setParam(kGlobalBase + sendG(dragIndex) + selTrack, clampf(v, 0.f, 127.f)); if (!mIdx.empty()) multiFollow(clampf(v, 0.f, 127.f)); }
            repaint(); break;
        case dkEuclid: {
            const int n = tlen(selTrack);
            int nv = (int)std::lround(dragStart + (dragY - y) * 0.08f);
            nv = dragIndex == 6 ? clampi(nv, 0, n) : clampi(nv, -n + 1, n - 1);
            int& cur = (dragIndex == 6 ? eucHits : eucRot)[selTrack];
            if (nv != cur) { cur = nv; applyEuclid(selTrack); repaint(); }
            break;
        }
        case dkKit: {
            const int f = kitField(dragIndex);
            if (f >= 0) {
                const int nv = clampi((int)std::lround(dragStart + (dragY - y) * 0.05f), 0, kKitMax[f]);
                if (nv != bank->kit[selTrack][f]) setKit(selTrack, f, nv);
            }
            break;
        }
        case dkMasterKnob: setMaster(dragIndex, v); break;
        case dkTempo: {
            const float tv = clampf(std::round((dragStart + (dragY - y) * ((ev.mod & kModifierShift) ? 0.05f : 0.5f)) * 10.f) / 10.f, 40.f, 240.f);
            setParam(kGlobalBase + gTEMPO, tv); repaint(); break;
        }
        case dkSwing: pat().swing = (uint8_t)clampi((int)std::lround(dragStart + (dragY - y) * 0.2f), 50, 80); repaint(); break;
        case dkAccent: pat().accent = (uint8_t)clampi((int)std::lround(v), 0, 127); repaint(); break;
        default: break;
        }
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        const float d = ev.delta.getY() > 0 ? 1.f : (ev.delta.getY() < 0 ? -1.f : 0.f);
        if (overlay == ovLib && d != 0.f) {
            const int rows = ((int)libFiles.size() + 1) / 2;
            libScroll = clampi(libScroll - (int)d, 0, std::max(0, rows - kLibRows));
            repaint(); return true;
        }
        if (overlay == ovPresets && d != 0.f) {
            const int rows = (presetCount() + 1) / 2;
            presetScroll = clampi(presetScroll - (int)d, 0, std::max(0, rows - kPresetRows));
            repaint(); return true;
        }
        if (d == 0.f || overlay != ovNone) return false;
        allNow = allLatch || (ev.mod & kModifierAlt) != 0;
        for (int k = 8; k < mSlots(); ++k)   // ECHO tab knobs 9-12
            if (mKnob(k).in(x, y) && masterParam(mpage, k) >= 0) { setMaster(k, gp(masterParam(mpage, k)) + d, true); return true; }
        for (int k = 0; k < slots(); ++k) {
            if (knob(k).in(x, y) && sendG(k) >= 0) {
                for (uint32_t o : sendOthers(k)) setParamGesture(o, clampf(values[o] + d, 0.f, 127.f));
                setParamGesture(kGlobalBase + sendG(k) + selTrack, clampf(sendValue(k) + d, 0.f, 127.f)); repaint(); return true;
            }
            if (knob(k).in(x, y) && euclidSlot(k)) {
                const int n = tlen(selTrack);
                int& cur = (k == 6 ? eucHits : eucRot)[selTrack];
                cur = k == 6 ? clampi(cur + (int)d, 0, n) : clampi(cur + (int)d, -n + 1, n - 1);
                applyEuclid(selTrack); repaint(); return true;
            }
            if (knob(k).in(x, y) && kitField(k) >= 0) {
                const int f = kitField(k);
                setKit(selTrack, f, clampi(bank->kit[selTrack][f] + (int)d, 0, kKitMax[f])); return true;
            }
            if (knob(k).in(x, y) && pageParam(page, k) >= 0) {
                const uint32_t idx = selTrack * kTrackParams + pageParam(page, k);
                const bool gesture = lockStep < 0 && !recording();
                if (gesture) { editParameter(idx, true); multiBegin(idx, knobOthers(pageParam(page, k))); }
                setKnob(k, knobValue(k) + d);
                if (gesture) { editParameter(idx, false); multiEnd(); }
                recDirty = false;
                return true;
            }
            if (mKnob(k).in(x, y) && masterParam(mpage, k) >= 0) {
                setMaster(k, gp(masterParam(mpage, k)) + d, true); return true;
            }
        }
        if (tempoBox().in(x, y)) { setParamGesture(kGlobalBase + gTEMPO, clampf(gp(gTEMPO) + d, 40.f, 240.f)); repaint(); return true; }
        if (swingBox().in(x, y)) { pat().swing = (uint8_t)clampi(pat().swing + (int)d, 50, 80); sendPatternSettings(); repaint(); return true; }
        if (lenBox().in(x, y)) { setTrackLen(selTrack, tlen(selTrack) + (int)d, false, false); repaint(); return true; }
        if (patLenBox().in(x, y)) { pat().length = (uint8_t)clampi(pat().length + (int)d, 1, kSteps); sendPatternSettings(); repaint(); return true; }
        if (accBox().in(x, y)) { pat().accent = (uint8_t)clampi(pat().accent + (int)d * 4, 0, 127); sendPatternSettings(); repaint(); return true; }
        return false;
    }

private:
    float values[kTotalParams];
    std::unique_ptr<Bank> bank;
    float sx = 1.f;
    SamplePtr uiSmp[kTracks];
    void* dropToken = nullptr;
    bool browseSample = false;
    std::string smpMsg; int smpMsgTicks = 0;
    int selTrack = 0, page = pgSynth, mpage = mpEcho, mode = mdTrig, stepPg = 0, lockStep = -1;
    Overlay overlay = ovNone;
    DragKind drag = dkNone; int dragIndex = 0; float dragY = 0, dragStart = 0;
    uint lastClickTime = 0; float lastX = -100, lastY = -100;
    int patClip = -1;
    std::string trackClip;
    std::vector<float> soundClip; int soundClipMachine = 0;
    // Euclidean rhythm on the TRACK page (knobs 7 and 8): hits spread evenly over the track length, then rotated
    int eucHits[kTracks] = {}, eucRot[kTracks] = {};

    // ------------------------------------------------------------------ chorus send knob, glitch HOLD pad, tab dots
    bool glHoldUi = false, glLatch = false;
    R glHoldPad() const { const R k = mKnob(7); return {k.x + 14.f, k.y + 14.f, k.w - 28.f, k.h - 28.f}; }
    R mfOnPad() const { const R k = mKnob(3); return {k.x + 14.f, k.y + 14.f, k.w - 28.f, k.h - 28.f}; }
    R freezePad() const { const R k = mKnob(11); return {k.x + 6.f, k.y + 14.f, k.w - 12.f, k.h - 28.f}; }
    void drawFreezePad()
    {
        const R p = freezePad();
        const bool on = gp(gECHO_FREEZE) > 0.5f;
        if (on) box(p, th->selFill); else { box(p, th->btnFill); frame(p, th->btnEdge, -1.f, 1.5f); }
        label(p.cx(), p.cy() - 6, "FREEZE", 11.f, on ? th->selText : th->btnText);
        label(p.cx(), p.cy() + 13, on ? "HOLDING" : "off", 9.f, on ? th->selText : th->dim);
    }
    void glHoldSet(bool on) { if (on == glHoldUi) return; glHoldUi = on; sendCmd(on ? "gh 1" : "gh 0"); repaint(); }
    void fxTabDots()
    {
        bool cho = false;
        for (int t = 0; t < kTracks; ++t) cho |= gp(gCHO_SEND + t) > 0.5f;
        const bool on[kMPages] = {false, false, cho, gp(gGL_CHANCE) > 0.5f || glHoldUi, gp(gMF_ON) > 0.5f && (gp(gMF_HP) > 0.5f || gp(gMF_LP) < 126.5f), false};
        for (int i = 0; i < kMPages; ++i)
            if (on[i] && mpage != i) { const R t = mTab(i); beginPath(); circle(t.x + t.w - 8, t.cy(), 3.f); fillColor(th->accent); fill(); }
    }
    void drawOnPad()
    {
        const R p = mfOnPad();
        const bool on = gp(gMF_ON) > 0.5f;
        if (on) box(p, th->selFill); else { box(p, th->btnFill); frame(p, th->btnEdge, -1.f, 1.5f); }
        label(p.cx(), p.cy() - 6, on ? "ON" : "OFF", 13.f, on ? th->selText : th->btnText);
        label(p.cx(), p.cy() + 13, "filter", 9.f, on ? th->selText : th->dim);
    }
    void drawHoldPad()
    {
        const R p = glHoldPad();
        if (glHoldUi) box(p, th->selFill); else { box(p, th->btnFill); frame(p, th->btnEdge, -1.f, 1.5f); }
        label(p.cx(), p.cy() - 6, "HOLD", 13.f, glHoldUi ? th->selText : th->btnText);
        label(p.cx(), p.cy() + 13, glLatch ? "LATCHED" : "press", 9.f, glHoldUi ? th->selText : th->dim);
    }
    // per-track send knobs that are global host parameters (gCHO_SEND + t, gCHA_SEND + t): base index or -1
    int sendG(int k) const
    {
        if (page == pgSynth) return k == 8 && kickAttackSlot() ? gKATK : -1;
        if (page != pgFx) return -1;
        return k == 4 ? gCHO_SEND : (k == 5 ? gCHA_SEND : -1);
    }
    float sendValue(int k) const { return gp(sendG(k) + selTrack); }

    // ------------------------------------------------------------------ effect presets (click the active tab)
    static int fxTabOf(int mp)
    {
        switch (mp) {
        case mpEcho: return fxEcho; case mpReverb: return fxReverb; case mpChorus: return fxChorus;
        case mpChaos: return fxChaos; case mpFilter: return fxFilter; case mpMaster: return fxMaster;
        default: return -1;
        }
    }
    std::vector<int> fxList() const
    {
        std::vector<int> v; const int tab = fxTabOf(mpage);
        const auto& all = fxPresets();
        for (int i = 0; i < (int)all.size(); ++i) if (all[i].tab == tab) v.push_back(i);
        return v;
    }
    R fxBox() const { const int rows = ((int)fxList().size() + 1) / 2; return {716.f, 178.f, 448.f, 50.f + rows * 40.f}; }
    R fxItem(int i) const { return {728.f + (i % 2) * 214.f, 222.f + (i / 2) * 40.f, 206.f, 34.f}; }
    void drawFxMenu()
    {
        const R bx = fxBox();
        box(bx, th->panel, th->radius); frame(bx, th->lcdText, th->radius, 1.5f);
        static const char* names[kFxTabs] = {"ECHO", "REVERB", "CHORUS", "CHAOS", "FILTER", "MASTER"};
        char b[48]; std::snprintf(b, sizeof(b), "%s PRESETS", names[std::max(0, fxTabOf(mpage))]);
        label(bx.x + 14, bx.y + 22, b, 12.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(bx.x + bx.w - 14, bx.y + 22, "sets this effect only", 9.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        const auto v = fxList(); const auto& all = fxPresets();
        for (int i = 0; i < (int)v.size(); ++i) button(fxItem(i), all[v[i]].name, false, 10.5f);
    }
    void fxMenuClick(float x, float y)
    {
        const auto v = fxList(); const auto& all = fxPresets();
        for (int i = 0; i < (int)v.size(); ++i)
            if (fxItem(i).in(x, y)) {
                snapshot(all[v[i]].name);
                const bool wasRestoring = restoring; restoring = true;
                if (all[v[i]].tab == fxEcho)   // echo extras the preset does not set go back to their defaults
                    for (int g : {(int)gECHO_RTIME, (int)gECHO_DRIVE, (int)gECHO_DUCK, (int)gECHO_FREEZE}) setParamGesture(kGlobalBase + g, globalDefault(g));
                for (const FxSet& fs : all[v[i]].set) setParamGesture(kGlobalBase + fs.g, (float)fs.v);
                restoring = wasRestoring;
                break;
            }
        overlay = ovNone; repaint();
    }

    // ------------------------------------------------------------------ LFO rings: which knobs an LFO moves
    struct LfoHit { int s, l; };   // LFO l of track s
    static constexpr int kUiLfos = (int)(sizeof(kLfoBase) / sizeof(kLfoBase[0]));
    int lfoHits(int t, int p, LfoHit* out) const
    {
        int n = 0;
        for (int s = 0; s < kTracks && n < 8; ++s)
            for (int l = 0; l < kUiLfos && n < 8; ++l) {
                const float* v = &values[s * kTrackParams];
                if (v[kLfoBase[l] + 1] < 0.5f) continue;                           // depth 0
                const int trk = lfoTrackIndex((int)std::lround(v[kLfoBase[l] + 5]));
                if ((trk == 0 ? s : trk - 1) != t) continue;
                const int d = lfoDestIndex((int)std::lround(v[kLfoBase[l] + 4]));
                int dp = -1;
                if (d < kLfoDests) dp = d;
                else if (d >= kDestL1Rate && d < kDestMorph) dp = lfoParam((d - kDestL1Rate) / 2, (d - kDestL1Rate) & 1);
                if (dp == p) out[n++] = {s, l};
            }
        return n;
    }
    // the LFO as the UI imagines it (same rate, wave and phase; not sample-exact): -1..1
    float lfoUiValue(int s, int l) const
    {
        const float* v = &values[s * kTrackParams];
        const int b = kLfoBase[l];
        double ph;
        if (lfoSynced(v[lfoParam(l, 6)])) {
            const double cyc = kLfoSyncBeats[lfoSyncDivIndex((int)v[b])];
            const double beats = fineStep() >= 0 ? (double)fineStep() / kFineSteps * 0.25 : nowSec() * gp(gTEMPO) / 60.0;
            ph = beats / cyc;
        } else ph = nowSec() * lfoFreeHz(v[b]);
        ph += v[lfoParam(l, 7)] / 128.0;
        ph -= std::floor(ph);
        switch (lfoShapeIndex((int)v[b + 2])) {
        case 0: return (float)(1.0 - 4.0 * std::fabs(ph - 0.5));
        case 1: return (float)std::sin(kTwoPi * ph);
        case 2: return ph < 0.5 ? 1.f : -1.f;
        case 3: return (float)(2.0 * ph - 1.0);
        case 4: return (float)(1.0 - 2.0 * ph);
        default: { const uint32_t x = (uint32_t)(std::floor(nowSec() * lfoFreeHz(v[b])) * 2654435761.0); return (float)((x >> 8) & 0xFFFF) / 32767.5f - 1.f; }
        }
    }
    // LFO colours: chosen per LFO on its page (COLOR), saved per user with the view (not in the project)
    static constexpr int kLfoPal = 8;
    static Color lfoPalColor(int i)
    {
        static const uint8_t c[kLfoPal][3] = {{64, 206, 255}, {255, 150, 50}, {176, 136, 255}, {242, 46, 58},
                                              {125, 255, 90}, {255, 214, 64}, {80, 124, 255}, {236, 238, 248}};
        i = clampi(i, 0, kLfoPal - 1);
        return Color(c[i][0], c[i][1], c[i][2]);
    }
    int lfoPal[3] = {4, 5, 7};    // palette index per LFO (defaults: green, yellow, white)
    int lfoPick = -1;             // LFO whose colour row is open on its page
    Color lfoInk(int l) const
    {
        const int i = lfoPal[clampi(l, 0, 2)];
        if (i == kLfoPal - 1 && th->logo == lgRiso) return th->text;   // WHITE on paper = ink
        return lfoPalColor(i);
    }
    // the knob value the LFOs give right now (UI estimate, same scaling as the engine: value + out * depth / 2)
    float lfoLive(float base, const LfoHit* h, int n) const
    {
        float v = base;
        for (int i = 0; i < n; ++i) v += lfoUiValue(h[i].s, h[i].l) * values[tpIndex(h[i].s, kLfoBase[h[i].l] + 1)] * 0.5f;
        return clampf(v, 0.f, 127.f);
    }
    bool ringsShown = false; double lastRingPaint = 0.0;
    void drawLfoBadge(const R& r, const LfoHit* h, int n)
    {
        ringsShown = true;
        char b[24];
        if (h[0].s == selTrack) std::snprintf(b, sizeof(b), n > 1 ? "L%d+" : "L%d", h[0].l + 1);
        else std::snprintf(b, sizeof(b), n > 1 ? "T%d L%d+" : "T%d L%d", h[0].s + 1, h[0].l + 1);
        label(r.x + r.w - 6, r.y + 8, b, 8.5f, lfoInk(h[0].l), ALIGN_RIGHT | ALIGN_MIDDLE);
    }
    // COLOR chip on the LFO page (bottom right of the screen) and the row of swatches it opens
    R lfoColChip() const { return {620.f, 274.f, 64.f, 16.f}; }
    R lfoSwatch(int i) const { return {448.f + i * 29.5f, 230.f, 24.f, 24.f}; }
    void drawLfoColor(int l)
    {
        const R c = lfoColChip();
        label(c.x + 2, c.cy(), "COLOR", 9.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        beginPath(); roundedRect(c.x + c.w - 22, c.y + 1, 22, c.h - 2, 3.f); fillColor(lfoInk(l)); fill();
        if (lfoPick != l) return;
        const R bg{lfoSwatch(0).x - 6, lfoSwatch(0).y - 6, kLfoPal * 29.5f + 6.5f, 36.f};
        box(bg, th->lcdBg, 4.f); frame(bg, withAlpha(th->lcdText, 0.6f), 4.f);
        for (int i = 0; i < kLfoPal; ++i) {
            const R s = lfoSwatch(i);
            beginPath(); circle(s.cx(), s.cy(), 9.f);
            fillColor(i == kLfoPal - 1 && th->logo == lgRiso ? th->text : lfoPalColor(i)); fill();
            if (lfoPal[l] == i) { beginPath(); circle(s.cx(), s.cy(), 11.5f); strokeColor(th->lcdText); strokeWidth(2.f); stroke(); }
        }
    }
    // true when the click was used
    bool lfoColorClick(int l, float x, float y)
    {
        if (lfoPick == l) {
            for (int i = 0; i < kLfoPal; ++i)
                if (lfoSwatch(i).in(x, y)) { lfoPal[l] = i; lfoPick = -1; saveView(); repaint(); return true; }
            lfoPick = -1; repaint();
            return lfoColChip().in(x, y) || R{442.f, 224.f, 248.f, 36.f}.in(x, y);
        }
        if (lfoColChip().in(x, y)) { lfoPick = l; repaint(); return true; }
        return false;
    }
    // ------------------------------------------------------------------ ALL: one knob moves every track
    // Alt (Option) + knob, or the ALL button: the same parameter of every track moves by the same amount (each track
    // keeps its own offset). Synth knobs only move the tracks that use the same machine.
    bool allLatch = false, altHeld = false, allNow = false;
    bool allShown() const { return allLatch || altHeld; }
    R allBtn() const { return {640.f, 304.f, 58.f, 28.f}; }
    std::vector<uint32_t> mIdx; std::vector<float> mBase; float mBase0 = 0.f;
    // track t follows the selected track's knob pi
    bool knobTarget(int t, int pi) const
    {
        if (t == selTrack) return false;
        return (allNow && (pi >= 8 || machineOf(t) == machineOf(selTrack))) || false;
    }
    std::vector<uint32_t> knobOthers(int pi) const
    {
        std::vector<uint32_t> v;
        for (int t = 0; t < kTracks; ++t) if (knobTarget(t, pi)) v.push_back(tpIndex(t, pi));
        return v;
    }
    uint32_t sendIdxFor(int k, int t) const { return kGlobalBase + sendG(k) + t; }
    std::vector<uint32_t> sendOthers(int k) const
    {
        std::vector<uint32_t> v;
        if (sendG(k) < 0) return v;
        for (int t = 0; t < kTracks; ++t)
            if (t != selTrack && ((allNow && (!(sendG(k) == gKATK) || machineOf(t) == machineOf(selTrack))) || false)) v.push_back(sendIdxFor(k, t));
        return v;
    }
    void multiBegin(uint32_t sel, const std::vector<uint32_t>& others)
    {
        multiEnd();
        mIdx = others; mBase0 = values[sel];
        for (uint32_t i : mIdx) { mBase.push_back(values[i]); editParameter(i, true); }
    }
    void multiFollow(float v)
    {
        const float d = v - mBase0;
        for (size_t j = 0; j < mIdx.size(); ++j) setParam(mIdx[j], clampf(mBase[j] + d, 0.f, 127.f));
    }
    void multiEnd() { for (uint32_t i : mIdx) editParameter(i, false); mIdx.clear(); mBase.clear(); }
    void drawAllBadge(const R& r) { label(r.x + 6, r.y + 8, "ALL", 8.5f, th->logo == lgRiso ? th->text : th->accent, ALIGN_LEFT | ALIGN_MIDDLE); }

    // ------------------------------------------------------------------ metronome while recording (per user)
    bool clickOn = true;
    R clickBtn() const { return {20.f, 662.f, 96.f, 18.f}; }
    void sendClick()
    {
        if (direct) direct->setClick(clickOn);
        else setState("cmd", clickOn ? "click 1" : "click 0");
    }
    bool euclidSlot(int k) const { return page == pgTrack && (k == 6 || k == 7); }
    void applyEuclid(int t)
    {
        const int n = tlen(t);
        const int k = clampi(eucHits[t], 0, n), r = ((eucRot[t] % n) + n) % n;
        std::unique_ptr<Pattern> tmp(new Pattern(pat()));
        for (int i = 0; i < n; ++i) {
            const int src = ((i - r) % n + n) % n;
            const bool on = k > 0 && (src * k) % n < k;   // Bresenham form of the Euclidean (Bjorklund) rhythm
            tmp->flags[t][i] = on ? (uint8_t)(tmp->flags[t][i] | sfTrig) : (uint8_t)(tmp->flags[t][i] & ~sfTrig);
        }
        sendCmd("tr " + std::to_string(editPattern()) + " " + std::to_string(t) + " " + trackToHex(*tmp, t));
    }
    bool rec = false, recDirty = false; float recVal = 0;
    DrumDirect* direct = nullptr;   // the plug-in (same process): live-recorded trigs, REC, click
    bool clickSent = false;
    void setRec(bool on)
    {
        rec = on;
        if (direct) direct->setRecording(on);
        else setState("cmd", on ? "rec 1" : "rec 0");
    }
    // trigs played on a MIDI keyboard while recording are written into the pattern here
    void pollRecorded()
    {
        if (!direct) return;
        std::string c;
        int pt, t, s; bool acc;
        char b[64];
        while (direct->popRecorded(pt, t, s, acc)) {
            if (pt < 0 || pt >= kPatterns || t < 0 || t >= kTracks || s < 0 || s >= kSteps) continue;
            const uint8_t f = (uint8_t)(bank->patterns[pt].flags[t][s] | sfTrig | (acc ? sfAccent : 0));
            std::snprintf(b, sizeof(b), "f %d %d %d %d\n", pt, t, s, f); c += b;
        }
        if (!c.empty()) { c.pop_back(); sendLines(c); repaint(); }
    }
    bool fillOn = false;
    int skin = kDefaultSkin;
    const Theme* th = &kThemes[kDefaultSkin];
    FontId fonts[kFontCount] = {};
    NanoImage dotImage, haloImage;
    int presetSel = -1, presetScroll = 0;
    bool deleteArmed = false, saveConfirm = false;
    std::vector<PEntry> presetView; std::vector<std::string> favs; std::string presetSearch; int presetPage = 0;
    std::vector<std::string> userPresets;
    std::string presetMsg, saveName;
    bool viewDirty = false; int viewDirtyTicks = 0;
    double resizeStartW = 0, grabX = 0, grabY = 0;
    std::string presetName = "Init";
    std::string patMsg; double patMsgUntil = 0;

    // ------------------------------------------------------------------ MIDI export of the edited pattern
    // Every trig of the pattern over its master length, with swing, micro-timing and ratchets; conditions and
    // probabilities are not applied (each trig is written once). Notes 36-51 on channel 10 play the tracks back.
    void exportMidi()
    {
        const int pi = editPattern();
        const Pattern& P = bank->patterns[pi];
        const double sb = 0.25 / kScaleMult[P.scale % 7];
        const int len = P.length ? P.length : 16;
        std::vector<std::string> names; std::vector<midi::Note> notes;
        for (int t = 0; t < kTracks; ++t) {
            bool any = false;
            const double mult = trackSpeedMult(bank->kit[t][kfSpeed]), sbt = sb / mult;   // the track's SPEED
            const int steps = (int)std::floor(len * mult + 1e-9);
            for (int k = 0; k < steps; ++k) {
                const int s = P.trackStep(t, k);
                if (!(P.flags[t][s] & sfTrig)) continue;
                if (!any) {
                    char nb[48]; std::snprintf(nb, sizeof(nb), "%d %s", t + 1, kMachineDefs[machineOf(t)].name);
                    names.push_back(nb); any = true;
                }
                const int n = std::max<int>(1, P.ratch[t][s]);
                const double base = k * sbt + ((k & 1) ? (P.swing / 100.0 * 2.0 - 1.0) * sbt : 0.0) + P.micro[t][s] / 24.0 * sbt;
                for (int j = 0; j < n; ++j) {
                    midi::Note m;
                    m.track = (int)names.size() - 1; m.channel = 9; m.note = 36 + t;
                    m.velocity = (P.flags[t][s] & sfAccent) ? 127 : 100;
                    m.start = std::max(0.0, base + j * sbt / n); m.length = sbt / n * 0.5;
                    notes.push_back(m);
                }
            }
        }
        if (notes.empty()) { showPatMsg("PATTERN IS EMPTY"); return; }
        char fn[32]; std::snprintf(fn, sizeof(fn), "A%02d", pi + 1);
        std::string title = cleanName(presetName); if (title.empty()) title = "KDSP-16";
        const std::string dir = fs::exportDir("KDSP-16 MIDI");
        if (dir.empty()) { showPatMsg("NO DOCUMENTS FOLDER"); return; }
        const std::string path = dir + title + " " + fn + ".mid";
        const std::string data = midi::build(title + " " + fn, std::clamp(gp(gTEMPO), 20.f, 300.f), len * sb, names, notes);
        FILE* f = fs::open(path, "wb");
        if (!f) { showPatMsg("EXPORT FAILED"); return; }
        const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
        std::fclose(f);
        if (!ok) { showPatMsg("EXPORT FAILED"); return; }
        showPatMsg(std::string(fn) + ".MID SAVED");
        upd::openFolder(dir);
    }
    void showPatMsg(const std::string& m) { patMsg = m; patMsgUntil = nowSec() + 4.0; }
    lic::Info licInfo = lic::findInstalled("KDSP-16");
    std::string licMsg;
    uint32_t rndCounter = 0x51ED;
    // ---- undo / redo: a stack of whole states (parameters + bank). Random, presets and undo itself push one each;
    // knob moves and sequencer edits push one per gesture (edits closer together than a short window merge).
    std::deque<Snap> undoStack, redoStack;
    bool restoring = false;
    double lastSnapAt = -10.0; std::string lastSnapWhat;
    static constexpr size_t kUndoDepth = 40;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KinetikUI)
};

UI* createUI() { return new KinetikUI(); }

END_NAMESPACE_DISTRHO
