// Kinetik KDSP-16 — user interface (NanoVG)
#include "DistrhoUI.hpp"
#include "Bank.hpp"
#include "Dsp.hpp"
#include "Presets.hpp"
#include "Help.hpp"
#include "Skins.hpp"
#include "Fonts.hpp"
#include "License.hpp"
#include "MasterFx.hpp"
#include "Voice.hpp"
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
enum MPage { mpEcho, mpReverb, mpMaster };
enum Mode { mdTrig, mdAccent, mdSlide, mdRatch, mdCond, mdTime, kModes };

enum DragKind { dkNone, dkTrackKnob, dkMasterKnob, dkTempo, dkSwing, dkAccent, dkKit };
enum Overlay { ovNone, ovPicker, ovPresets, ovHelp, ovSkins, ovSave, ovLicense, ovDest };
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
        for (int i = 0; i < kMasterCount; ++i) values[kGlobalBase + gECHO_TIME + i] = kMasterDefaults[i];
        values[kGlobalBase + gTEMPO] = 120;
        values[kGlobalBase + gOUT_STEP] = -1;
        bank.reset(new Bank());
        bank->initDefault();
        fonts[fnSerif] = createFontFromMemory("kn-serif", kFontSerif, sizeof(kFontSerif), false);
        fonts[fnSerifItalic] = createFontFromMemory("kn-serif-i", kFontSerifItalic, sizeof(kFontSerifItalic), false);
        fonts[fnMono] = createFontFromMemory("kn-mono", kFontMono, sizeof(kFontMono), false);
        fonts[fnBlack] = createFontFromMemory("kn-black", kFontBlack, sizeof(kFontBlack), false);
        int sk, zm;
        readDefaultView(sk, zm);
        setSkin(sk, false);
        if (zm != 100) setSize((uint)std::lround(W * baseScale() * zm / 100.0), (uint)std::lround(H * baseScale() * zm / 100.0));
        viewDirty = false;
    }

    // ------------------------------------------------------------------ skins
    static std::string viewFile()
    {
       #ifdef _WIN32
        const char* base = std::getenv("APPDATA");
        return base ? std::string(base) + "\\KinetikKDSP16-view.txt" : std::string();
       #else
        const char* base = std::getenv("HOME");
       #ifdef __APPLE__
        return base ? std::string(base) + "/Library/Preferences/KinetikKDSP16-view.txt" : std::string();
       #else
        return base ? std::string(base) + "/.config/KinetikKDSP16-view.txt" : std::string();
       #endif
       #endif
    }
    static void readDefaultView(int& skinOut, int& zoomOut)
    {
        skinOut = kDefaultSkin; zoomOut = 100;
        const std::string f = viewFile();
        if (f.empty()) return;
        if (FILE* fp = std::fopen(f.c_str(), "r")) {
            int a = 0, b = 100; const int n = std::fscanf(fp, "%d %d", &a, &b); std::fclose(fp);
            if (n >= 1 && a >= 0 && a < kNumThemes) skinOut = a;
            if (n >= 2 && b >= 60 && b <= 300) zoomOut = b;
        }
    }
    static void writeDefaultView(int s, int z)
    {
        const std::string f = viewFile();
        if (f.empty()) return;
        if (FILE* fp = std::fopen(f.c_str(), "w")) { std::fprintf(fp, "%d %d\n", s, z); std::fclose(fp); }
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
    int64_t absStep() const { return (int64_t)std::lround(gp(gOUT_STEP)); } // -1 when stopped
    int trackStepNow(int t) { const int64_t k = absStep(); return k < 0 ? -1 : pat().trackStep(t, k); }
    int tlen(int t) { return pat().trackLen[t] ? pat().trackLen[t] : 16; }
    Pattern& pat() { return bank->patterns[editPattern()]; }

    void setParam(uint32_t idx, float v)
    {
        values[idx] = v;
        setParameterValue(idx, v);
    }
    void setParamGesture(uint32_t idx, float v)
    {
        editParameter(idx, true);
        setParam(idx, v);
        editParameter(idx, false);
    }

    void sendCmd(const std::string& c)
    {
        if (c.rfind("aud ", 0) != 0) applyCommand(*bank, c.c_str());
        setState("cmd", c.c_str());
    }
    void sendLines(const std::string& c)
    {
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
        switch (pg) {
        case pgSynth: return k;
        case pgFilter: { static const int f[8] = {tpCUTOFF, tpRESO, tpFTYPE, tpFENV, tpFDEC, -1, -1, -1}; return f[k & 7]; }
        case pgFx: { static const int f[8] = {tpAMD, tpAMF, tpEQF, tpEQG, tpSRR, tpDIST, -1, -1}; return f[k & 7]; }
        case pgMix: { static const int f[8] = {tpVOL, tpPAN, tpVEL, tpDEL, tpREV, -1, -1, -1}; return f[k & 7]; }
        case pgLfo1: return lfoParam(0, k);
        case pgLfo2: return lfoParam(1, k);
        default: return -1; // TRACK page: kit settings, see kitField()
        }
    }
    int kitField(int k) const { return page == pgTrack && k < kKitFields ? k : -1; }
    int masterParam(int mp, int k) const
    {
        if (mp == mpEcho) return k < 6 ? gECHO_TIME + k : (k == 6 ? gECHO_MODEL : gECHO_MOD);
        if (mp == mpReverb) return k < 6 ? gREV_SIZE + k : (k == 6 ? gREV_MODEL : gREV_MOD);
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
        return trackParamName(machineOf(tt), std::min(kLfoDests - 1, d));
    }

    std::string valueText(int t, int p, float v) const
    {
        char b[32];
        const int iv = (int)std::lround(v);
        if (p == tpPAN) {
            if (iv == 64) return "C";
            std::snprintf(b, sizeof(b), "%c%d", iv < 64 ? 'L' : 'R', std::abs(iv - 64)); return b;
        }
        if (p == tpEQG || p == tpFENV) { std::snprintf(b, sizeof(b), "%+d", iv - 64); return b; }
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
    std::string masterText(int g, float v) const
    {
        char b[32];
        const int iv = (int)std::lround(v);
        if (g == gECHO_TIME) return kEchoNames[echoDivIndex(iv)];
        if (g == gECHO_MODEL) return kEchoModelNames[echoModelIndex(iv)];
        if (g == gREV_MODEL) return kReverbModelNames[reverbModelIndex(iv)];
        if (g == gECHO_LEV || g == gREV_LEV) {
            const float gn = fxReturnGain(v);
            if (gn < 0.001f) return "OFF";
            std::snprintf(b, sizeof(b), "%+.1fdB", 20.f * std::log10(gn)); return b;
        }
        if (g == gREV_GATE && iv == 0) return "OFF";
        if (g == gEQ_LOW || g == gEQ_HIGH) { std::snprintf(b, sizeof(b), "%+d", iv - 64); return b; }
        std::snprintf(b, sizeof(b), "%d", iv); return b;
    }

    // ------------------------------------------------------------------ layout
    R trackBtn(int t) const { return {20.f + t * 71.25f, 66.f, 66.f, 64.f}; }
    R muteBtn(int t) const { R r = trackBtn(t); return {r.x + r.w - 28.f, r.y + r.h - 26.f, 28.f, 26.f}; } // generous hit area
    R lcd() const { return {20.f, 146.f, 680.f, 150.f}; }
    R machPrev() const { return {150.f, 156.f, 26.f, 26.f}; }
    R machName() const { return {178.f, 156.f, 130.f, 26.f}; }
    R machNext() const { return {310.f, 156.f, 26.f, 26.f}; }
    R lockClear() const { return {596.f, 156.f, 94.f, 24.f}; }
    R pageTab(int i) const { return {20.f + i * 97.4f, 304.f, 94.f, 28.f}; }
    R knob(int k) const { return {20.f + k * 85.f, 340.f, 85.f, 120.f}; }
    R mTab(int i) const { return {720.f + i * 148.f, 146.f, 144.f, 28.f}; }
    R mKnob(int k) const { return {720.f + (k % 4) * 110.f, 178.f + (k / 4) * 102.f, 110.f, 100.f}; }
    R patBtn(int i) const { return {720.f + (i % 8) * 55.f, 432.f + (i / 8) * 30.f, 51.f, 26.f}; }
    R patCopy() const { return {946.f, 400.f, 68.f, 24.f}; }
    R patPaste() const { return {1018.f, 400.f, 68.f, 24.f}; }
    R patClear() const { return {1090.f, 400.f, 68.f, 24.f}; }
    R modeBtn(int i) const { return {20.f + i * 62.f, 504.f, 58.f, 26.f}; }
    R stepPage(int i) const { return {396.f + i * 53.f, 504.f, 50.f, 26.f}; }
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
    R stepBtn(int i) const { return {20.f + i * 71.25f, 544.f, 66.f, 72.f}; }
    R playBtn() const { return {840.f, 9.f, 84.f, 40.f}; }
    R tempoBox() const { return {932.f, 9.f, 110.f, 40.f}; }
    R skinBtn() const { return {1050.f, 9.f, 116.f, 40.f}; }
    R viewBox() const { return {240.f, 96.f, 700.f, 520.f}; }
    R skinRow(int i) const { return {264.f + (i % 2) * 330.f, 150.f + (i / 2) * 148.f, 316.f, 136.f}; }
    R zoomBtn(int i) const { return {264.f + i * 109.f, 486.f, 101.f, 36.f}; }
    R gripR() const { return {W - 22.f, H - 22.f, 22.f, 22.f}; }
    R recBtn() const { return {20.f, 634.f, 96.f, 40.f}; }
    R rndGroup() const { return {124.f, 628.f, 574.f, 52.f}; }
    R rndBtn(int i) const { return {250.f + i * 111.f, 634.f, 105.f, 40.f}; }
    R undoBtn() const { return {712.f, 634.f, 86.f, 40.f}; }
    R presetBtn() const { return {1010.f, 624.f, 150.f, 34.f}; }
    R saveBtn() const { return {1010.f, 664.f, 150.f, 34.f}; }
    // presets window
    R presetBox() const { return {80.f, 60.f, 1020.f, 610.f}; }
    R presetTab(int i) const { return {300.f + i * 134.f, 76.f, 128.f, 32.f}; }
    R presetRow(int i) const { return {100.f + (i % 2) * 490.f, 120.f + (i / 2) * 43.f, 480.f, 40.f}; }
    R presetSaveNew() const { return {100.f, 620.f, 190.f, 36.f}; }
    R presetDelete() const { return {300.f, 620.f, 110.f, 36.f}; }
    R presetLoadAll() const { return {560.f, 620.f, 250.f, 36.f}; }
    R presetLoadKit() const { return {820.f, 620.f, 170.f, 36.f}; }
    R presetCancel() const { return {1000.f, 620.f, 80.f, 36.f}; }
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
        return font == fnMono ? s * 1.13f : s * 1.08f;
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
        const float sz = f == fnSerif || f == fnSerifItalic ? size * 1.25f : (f == fnBlack ? size * 0.95f : size);
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
    void drawKnob(const R& r, float v01, const char* name, const std::string& val, bool locked, bool highlight, bool enabled)
    {
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
        switch (th->knob) {
        case ksTicks: {
            // ring of tick dots around a dark knob (lit up to the value)
            const int n = 25;
            for (int i = 0; i < n; ++i) {
                const float t = (float)i / (n - 1);
                const float ang = a0 + (a1 - a0) * t;
                const bool lit = t <= v01 + 1e-4f;
                beginPath(); circle(cx + std::cos(ang) * (rad + 3), cy + std::sin(ang) * (rad + 3), lit ? 1.6f : 1.1f);
                fillColor(lit ? hi : th->knobTrack); fill();
            }
            if (th->glow) {
                beginPath(); circle(cx, cy, rad - 3);
                fillPaint(radialGradient(cx, cy, rad - 8, rad + 8, Color(hi, Color(0, 0, 0, 0.f), 0.75f), Color(0, 0, 0, 0.f))); fill();
            }
            beginPath(); circle(cx, cy, rad - 5); fillColor(th->knobBody); fill();
            strokeColor(locked ? th->lock : th->knobRing); strokeWidth(locked ? 2.f : 1.2f); stroke();
            beginPath(); moveTo(cx + std::cos(a) * 4, cy + std::sin(a) * 4); lineTo(cx + std::cos(a) * (rad - 9), cy + std::sin(a) * (rad - 9));
            strokeColor(locked ? th->lock : th->knobPointer); strokeWidth(2.4f); lineCap(ROUND); stroke();
            break;
        }
        case ksArcThick: {
            beginPath(); arc(cx, cy, rad, a0, a1, CW); strokeColor(th->knobTrack); strokeWidth(6.f); lineCap(BUTT); stroke();
            beginPath(); arc(cx, cy, rad, a0, a0 + (a1 - a0) * std::max(0.001f, v01), CW); strokeColor(hi); strokeWidth(6.f); stroke();
            beginPath(); circle(cx, cy, rad - 6); fillColor(locked ? th->lock : th->knobBody); fill();
            strokeColor(th->knobRing); strokeWidth(1.2f); stroke();
            beginPath(); moveTo(cx + std::cos(a) * 6, cy + std::sin(a) * 6); lineTo(cx + std::cos(a) * (rad - 9), cy + std::sin(a) * (rad - 9));
            strokeColor(th->knobPointer); strokeWidth(2.4f); lineCap(ROUND); stroke();
            break;
        }
        default: {
            beginPath(); circle(cx, cy, rad + 4); fillColor(th->inner); fill();
            beginPath(); arc(cx, cy, rad, a0, a1, CW); strokeColor(th->knobTrack); strokeWidth(5.f); stroke();
            beginPath(); arc(cx, cy, rad, a0, a0 + (a1 - a0) * std::max(0.001f, v01), CW); strokeColor(hi); strokeWidth(5.f); stroke();
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

    // the same logo in every skin: "Kinetik" in serif italic + a four-point star, in the skin's colours
    void drawLogo()
    {
        useFont(fnSerifItalic); fontSize(54.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        DGL_NAMESPACE::Rectangle<float> bnd; textBounds(18, 30, "Kinetik", nullptr, bnd);
        const float right = bnd.getX() + bnd.getWidth();
        const Color star = th->logo == lgRiso ? th->lock : (th->logo == lgClassic ? th->knobArc : th->accent);
        if (th->glow) {
            for (int k = 0; k < 2; ++k) { fontBlur(24.f); fillColor(Color(th->accent, Color(0, 0, 0, 0.f), 0.55f)); text(18, 30, "Kinetik", nullptr); }
            fontBlur(6.f); fillColor(Color(th->accent, Color(0, 0, 0, 0.f), 0.7f)); text(18, 30, "Kinetik", nullptr);
            fontBlur(0.f); fillColor(Color(230, 234, 255)); text(18, 30, "Kinetik", nullptr);
            beginPath(); circle(right + 10, 19, 14);
            fillPaint(radialGradient(right + 10, 19, 2, 14, Color(star, Color(0, 0, 0, 0.f), 0.55f), Color(0, 0, 0, 0.f))); fill();
        } else {
            fillColor(th->text); text(18, 30, "Kinetik", nullptr);
        }
        drawSparkle(right + 10, 19, 9.f, star);
        label(right + 30, 21, "KDSP-16", 13.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        if (licInfo.valid) label(right + 30, 39, "DRUM SYNTH PROGRAM", 9.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        else label(right + 30, 39, "DEMO - CLICK TO REGISTER", 9.5f, th->logo == lgRiso ? th->text : th->accent, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
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
        if (overlay == ovHelp) drawHelp();
        if (overlay == ovSkins) drawSkins();
        if (overlay == ovSave) drawSave();
        if (overlay == ovLicense) drawLicense();
        if (overlay == ovDest) drawDestMenu();
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
            char b[8]; std::snprintf(b, sizeof(b), "%02d", t + 1);
            number(r.x + 7, r.y + 17, b, 17.f, fg, ALIGN_LEFT | ALIGN_MIDDLE);
            const bool lit = (leds >> t) & 1;
            beginPath(); circle(r.x + r.w - 11, r.y + 13, 4);
            fillColor(lit ? (sel && th->selFill.red > 0.5f && th->accent.red < 0.2f ? th->selText : th->accent)
                          : Color(fg, Color(0, 0, 0, 0.f), 0.25f)); fill();
            if (lit && th->glow) { beginPath(); circle(r.x + r.w - 11, r.y + 13, 9); fillPaint(radialGradient(r.x + r.w - 11, r.y + 13, 2, 9, Color(th->accent, Color(0, 0, 0, 0.f), 0.5f), Color(0, 0, 0, 0.f))); fill(); }
            label(r.x + 7, r.y + 36, kMachineDefs[machineOf(t)].name, 10.f, fg, ALIGN_LEFT | ALIGN_MIDDLE);
            const bool muted = bank->mute[t] != 0;
            const R mh = muteBtn(t);
            const R m{mh.x + 6, mh.y + 9, mh.w - 10, mh.h - 13};
            if (muted) box(m, th->lock, 2.f); else frame(m, Color(fg, Color(0, 0, 0, 0.f), 0.6f), 2.f);
            label(m.cx(), m.cy() + 1, "M", 9.f, muted ? th->selText : fg, ALIGN_CENTER | ALIGN_MIDDLE, fnMono);
            if (muted) { beginPath(); rect(r.x + 4, r.y + r.h - 4, r.w - 8, 2); fillColor(th->lock); fill(); }
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
    std::vector<float> hitMin, hitMax; float hitSeconds = 0.5f;
    void updateHit(int cols)
    {
        float key[11];
        for (int i = 0; i < 8; ++i) key[i] = std::round(shownValue(i));
        key[8] = (float)machineOf(selTrack); key[9] = (float)bank->kit[selTrack][kfFmq]; key[10] = (float)bank->kit[selTrack][kfOsc];
        if (hitReady && std::equal(key, key + 11, hitKey) && (int)hitMin.size() == cols) return;
        std::copy(key, key + 11, hitKey);
        if (!hitReady) { hitVoice.init(48000.f); hitReady = true; }
        hitVoice.machine = -1;
        hitVoice.setMachine((int)key[8]);
        hitVoice.fmQuant = key[9] > 0.5f; hitVoice.oscShape = (int)key[10];
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
        const float cut = shownValue(tpCUTOFF), q = 0.5f + shownValue(tpRESO) / 127.f * 12.f;
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
        const float sd = shownValue(tpDEL) / 127.f, sr2 = shownValue(tpREV) / 127.f;
        const R b1{c.x + 10, c.y + 18, c.w - 20, 14}, b2{c.x + 10, c.y + 50, c.w - 20, 14};
        frame(b1, withAlpha(th->lcdDim, 0.5f), 1.f); frame(b2, withAlpha(th->lcdDim, 0.5f), 1.f);
        box({b1.x + 2, b1.y + 2, (b1.w - 4) * sd, b1.h - 4}, ink(tpDEL), 1.f);
        box({b2.x + 2, b2.y + 2, (b2.w - 4) * sr2, b2.h - 4}, ink(tpREV), 1.f);
        std::snprintf(b, sizeof(b), "DELAY  -  %s", kEchoModelNames[echoModelIndex((int)gp(gECHO_MODEL))]);
        label(b1.x, b1.y - 7, b, 9.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        std::snprintf(b, sizeof(b), "REVERB  -  %s", kReverbModelNames[reverbModelIndex((int)gp(gREV_MODEL))]);
        label(b2.x, b2.y - 7, b, 9.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
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
        lcdCurve(X, Y, n, dep < 0.004f ? withAlpha(th->lcdDim, 0.7f) : (lk ? th->lock : th->lcdText), 2.f);
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
        const int d = std::min(kLfoDests - 1, lfoDestIndex((int)shownValue(lfoParam(l, 4))));
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
    R destItem(int col, int row) const { return {156.f + col * 220.f, 280.f + row * 36.f, 210.f, 32.f}; }
    void openDestMenu(int l) { destLfo = l; overlay = ovDest; repaint(); }
    void drawDestMenu()
    {
        overlayBox(destBox());
        const R bx = destBox();
        const int l = destLfo;
        char b[64]; std::snprintf(b, sizeof(b), "LFO %d  -  WHAT SHOULD IT MOVE?", l + 1);
        label(bx.x + 26, bx.y + 24, b, 15.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        const int trk = lfoTrackIndex((int)std::lround(knobValueOf(lfoParam(l, 5))));
        const int curD = std::min(kLfoDests - 1, lfoDestIndex((int)std::lround(knobValueOf(lfoParam(l, 4)))));
        for (int i = 0; i <= kTracks; ++i) {
            char tb[16]; if (i == 0) std::snprintf(tb, sizeof(tb), "THIS"); else std::snprintf(tb, sizeof(tb), "T%02d", i);
            button(destTrackBtn(i), tb, trk == i, 11.f);
        }
        const int tt = trk == 0 ? selTrack : trk - 1;
        static const char* cols[3] = {"SYNTH", "FX + FILTER", "MIX + FILTER ENV"};
        for (int c = 0; c < 3; ++c) {
            label(destItem(c, 0).x, 264, cols[c], 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            for (int r = 0; r < 8; ++r) {
                const int d = c * 8 + r;
                const char* nm = trackParamName(machineOf(tt), d);
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
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 8; ++r) {
                const int d = c * 8 + r;
                if (!destItem(c, r).in(x, y)) continue;
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
        label(378, 169, kPageNames[page], 11.5f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);

        if (lockStep >= 0) {
            std::snprintf(b, sizeof(b), "LOCK STEP %02d", lockStep + 1);
            label(586, 169, b, 11.5f, th->lock, ALIGN_RIGHT | ALIGN_MIDDLE);
            frame(lockClear(), th->lock); label(lockClear().cx(), lockClear().cy() + 1, "CLEAR", 10.f, th->lock);
        } else if (rec) {
            iconLabel(688, 169, icRec, recording() ? "REC: turn knobs to record" : "REC armed: start playback", 10.f,
                      th->logo == lgRiso ? th->text : th->accent, th->fontLabel, ALIGN_RIGHT);
        } else {
            label(688, 169, "right-click a step to lock", 10.f, th->lcdDim, ALIGN_RIGHT | ALIGN_MIDDLE);
        }

        const Pattern& p = pat();
        drawPageGraphic({30.f, 190.f, 660.f, 100.f});

        for (int i = 0; i < kPages; ++i) button(pageTab(i), kPageNames[i], page == i, 11.5f);

        for (int k = 0; k < 8; ++k) {
            const int pi = pageParam(page, k);
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
            drawKnob(knob(k), v / 127.f, trackParamName(m, pi), valueText(selTrack, pi, v), locked || recNow,
                     drag == dkTrackKnob && dragIndex == k, true);
        }
    }

    void drawMaster()
    {
        panelBox({710, 140, 460, 344});
        static const char* tabs[3] = {"ECHO", "REVERB", "MASTER"};
        for (int i = 0; i < 3; ++i) button(mTab(i), tabs[i], mpage == i, 11.5f);
        for (int k = 0; k < 8; ++k) {
            const int g = masterParam(mpage, k);
            if (g < 0) { drawKnob(mKnob(k), 0, "", "", false, false, false); continue; }
            const float v = gp(g);
            drawKnob(mKnob(k), v / 127.f, kMasterNames[g - gECHO_TIME], masterText(g, v), false,
                     drag == dkMasterKnob && dragIndex == k, true);
        }
        const R in{716, 394, 448, 88};
        box(in, th->inner, th->radius);
        label(728, 412, "PATTERNS", 10.5f, th->logo == lgRiso ? th->text : th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
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
        for (int i = 0; i < 4; ++i) {
            char b[16]; std::snprintf(b, sizeof(b), "%d-%d", i * 16 + 1, i * 16 + 16);
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

        for (int i = 0; i < 16; ++i) {
            const int s = stepPg * 16 + i;
            const R r = stepBtn(i);
            const bool inLen = s < len;
            const uint8_t f = p.flags[selTrack][s];
            const bool playing = s == step;
            Color bg = inLen ? ((i / 4) % 2 ? th->stepAlt : th->stepFill) : th->stepOff;
            box(r, bg);
            if (playing) frame(r, th->logo == lgRiso ? th->text : th->selFill, -1.f, 2.5f);
            else if (s == lockStep) frame(r, th->lock, -1.f, 2.5f);
            else frame(r, Color(th->btnEdge, bg, 0.5f));
            const R led{r.x + 8, r.y + 9, r.w - 16, 12};
            if (f & sfTrig && inLen) {
                const Color lc = (f & sfAccent) ? th->lock : th->accent;
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
        static const int col[kMachines] = {3, 0,0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, 2,2,2,2,2, 3,3};
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
        const R grp = rndGroup();   // ends before the UNDO button
        box(grp, th->inner);
        const Color gt = th->logo == lgRiso ? th->text : th->dim;
        label(134, 645, "RANDOMIZE", 10.f, gt, ALIGN_LEFT | ALIGN_MIDDLE);
        label(134, 663, "undo restores", 9.f, gt, ALIGN_LEFT | ALIGN_MIDDLE);
        static const char* rn[4] = {"SOUND", "SEQUENCE", "PATTERN", "EVERYTHING"};
        static const char* rs[4] = {"this track", "this track", "all tracks", "kit+fx+pattern"};
        for (int i = 0; i < 4; ++i) {
            const R r = rndBtn(i);
            box(r, th->btnFill); frame(r, th->btnEdge);
            label(r.cx(), r.y + 14, rn[i], 10.5f, th->btnText);
            label(r.cx(), r.y + 30, rs[i], 9.f, th->dim);
        }
        const R u = undoBtn();
        if (undoValid) { box(u, th->btnFill); frame(u, th->lock, -1.f, 1.5f); }
        else { box(u, th->btnFill); frame(u, th->btnEdge); }
        label(u.cx(), u.y + 14, "UNDO", 10.5f, undoValid ? th->lock : th->faint);
        label(u.cx(), u.y + 30, undoValid ? undoWhat : "", 9.f, th->dim);
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

    int presetCount() const { return presetTabUser ? (int)userPresets.size() : kNumPresets; }

    void drawPresets()
    {
        const R bx = presetBox();
        overlayBox(bx);
        number(100, 92, "Presets", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        button(presetTab(0), "FACTORY", !presetTabUser, 11.f);
        char b[64]; std::snprintf(b, sizeof(b), "USER (%d)", (int)userPresets.size());
        button(presetTab(1), b, presetTabUser, 11.f);
        label(1080, 92, "double-click = load everything", 10.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        const int n = presetCount();
        for (int k = 0; k < 22; ++k) {
            const int i = k + presetScroll * 2;
            if (i >= n) break;
            const R r = presetRow(k);
            const bool sel = i == presetSel;
            if (sel) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge); }
            if (presetTabUser) {
                number(r.x + 14, r.cy() + 1, userPresets[i].c_str(), 15.f, sel ? th->selText : th->text, ALIGN_LEFT | ALIGN_MIDDLE);
            } else {
                number(r.x + 12, r.y + 13, kPresets[i].name, 14.f, sel ? th->selText : th->text, ALIGN_LEFT | ALIGN_MIDDLE);
                label(r.x + 12, r.y + 29, kPresets[i].desc, 9.f, sel ? th->selText : th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            }
        }
        if (presetTabUser && n == 0) {
            label(bx.cx(), 300, "No user presets yet.", 13.f, th->text);
            label(bx.cx(), 330, "Use SAVE CURRENT AS... (or SAVE AS PRESET on the main panel).", 11.f, th->dim);
        }
        if (n > 22) {
            std::snprintf(b, sizeof(b), "scroll for more (%d-%d of %d)", presetScroll * 2 + 1, std::min(n, presetScroll * 2 + 22), n);
            label(1080, 600, b, 9.5f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        }
        label(100, 600, presetMsg.c_str(), 10.f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        const bool ok = presetSel >= 0 && presetSel < n;
        button(presetSaveNew(), "SAVE CURRENT AS...", false, 10.5f);
        if (presetTabUser) button(presetDelete(), deleteArmed ? "CONFIRM?" : "DELETE", deleteArmed, 10.5f);
        button(presetLoadAll(), "LOAD SOUND + PATTERNS", ok, 10.5f);
        button(presetLoadKit(), "LOAD SOUND ONLY", false, 10.5f);
        button(presetCancel(), "CLOSE", false, 10.5f);
    }

    void openPresets()
    {
        refreshUserPresets();
        overlay = ovPresets; presetSel = -1; presetScroll = 0; deleteArmed = false; presetMsg.clear();
        repaint();
    }

    void presetClick(float x, float y, bool dbl)
    {
        for (int t = 0; t < 2; ++t)
            if (presetTab(t).in(x, y)) {
                presetTabUser = t == 1; presetSel = -1; presetScroll = 0; deleteArmed = false; presetMsg.clear();
                if (presetTabUser) refreshUserPresets();
                repaint(); return;
            }
        const int n = presetCount();
        for (int k = 0; k < 22; ++k) {
            const int i = k + presetScroll * 2;
            if (i < n && presetRow(k).in(x, y)) {
                deleteArmed = false;
                if (dbl && presetSel == i) { loadAnyPreset(i, true); overlay = ovNone; }
                else presetSel = i;
                repaint(); return;
            }
        }
        if (presetSaveNew().in(x, y)) { openSave(); return; }
        if (presetTabUser && presetDelete().in(x, y) && presetSel >= 0 && presetSel < n) {
            if (!deleteArmed) { deleteArmed = true; repaint(); return; }
            std::remove(userPresetPath(userPresets[presetSel]).c_str());
            presetMsg = "Deleted \"" + userPresets[presetSel] + "\"";
            deleteArmed = false; presetSel = -1; refreshUserPresets(); repaint(); return;
        }
        deleteArmed = false;
        if (presetSel >= 0 && presetSel < n && presetLoadAll().in(x, y)) { loadAnyPreset(presetSel, true); overlay = ovNone; }
        else if (presetSel >= 0 && presetSel < n && presetLoadKit().in(x, y)) { loadAnyPreset(presetSel, false); overlay = ovNone; }
        else if (presetCancel().in(x, y) || !presetBox().in(x, y)) overlay = ovNone;
        repaint();
    }

    void loadAnyPreset(int i, bool withPatterns)
    {
        if (presetTabUser) loadUserPreset(userPresets[i], withPatterns);
        else loadPreset(i, withPatterns);
    }

    // ------------------------------------------------------------------ user presets (files)
    static std::string userDir()
    {
       #ifdef _WIN32
        const char* base = std::getenv("APPDATA");
        if (!base) return std::string();
        std::string d = std::string(base) + "\\Kinetik";
        _mkdir(d.c_str()); d += "\\KDSP-16"; _mkdir(d.c_str()); d += "\\Presets"; _mkdir(d.c_str());
        return d + "\\";
       #else
        const char* base = std::getenv("HOME");
        if (!base) return std::string();
       #ifdef __APPLE__
        std::string d = std::string(base) + "/Library/Application Support";
       #else
        std::string d = std::string(base) + "/.config";
       #endif
        mkdir(d.c_str(), 0755); d += "/Kinetik"; mkdir(d.c_str(), 0755); d += "/KDSP-16"; mkdir(d.c_str(), 0755);
        d += "/Presets"; mkdir(d.c_str(), 0755);
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
        if (DIR* dir = opendir(d.c_str())) {
            while (dirent* e = readdir(dir)) {
                std::string f = e->d_name;
                if (f.size() > 5 && f.compare(f.size() - 5, 5, ".kdsp") == 0) userPresets.push_back(f.substr(0, f.size() - 5));
            }
            closedir(dir);
        }
        std::sort(userPresets.begin(), userPresets.end(), [](const std::string& a, const std::string& b) {
            std::string la = a, lb = b;
            for (auto& c : la) c = (char)std::tolower((unsigned char)c);
            for (auto& c : lb) c = (char)std::tolower((unsigned char)c);
            return la < lb; });
    }

    bool saveUserPreset(const std::string& name)
    {
        const std::string path = userPresetPath(name);
        FILE* fp = std::fopen(path.c_str(), "wb");
        if (!fp) return false;
        std::fprintf(fp, "KDSP16-PRESET 2\nNAME %s\nPATTERN %d\n", name.c_str(), editPattern());
        for (int i = 0; i < kGlobalBase + gOUT_STEP; ++i) {
            if (i == kGlobalBase + gPLAY || i == kGlobalBase + gPATTERN) continue;
            std::fprintf(fp, "V %d %.4f\n", i, values[i]);
        }
        std::fprintf(fp, "BANK\n%s", serializeBank(*bank).c_str());
        std::fclose(fp);
        return true;
    }

    void loadUserPreset(const std::string& name, bool withPatterns)
    {
        FILE* fp = std::fopen(userPresetPath(name).c_str(), "rb");
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
        const size_t bankPos = all.find("\nBANK\n");
        const std::string head = all.substr(0, bankPos == std::string::npos ? all.size() : bankPos);
        size_t st = 0;
        while (st < head.size()) {
            size_t e = head.find('\n', st); if (e == std::string::npos) e = head.size();
            const std::string line = head.substr(st, e - st);
            int idx; float v;
            if (std::sscanf(line.c_str(), "V %d %f", &idx, &v) == 2 && idx >= 0) {
                if (v1) idx = presetIndexV1(idx);
                if (idx < kGlobalBase + gOUT_STEP) nv[idx] = v;
            }
            std::sscanf(line.c_str(), "PATTERN %d", &patIdx);
            st = e + 1;
        }
        if (v1) for (int t = 0; t < kTracks; ++t) migrateFilterV1(&nv[t * kTrackParams]);
        if (bankPos != std::string::npos) {
            std::unique_ptr<Bank> fileBank(new Bank());
            if (deserializeBank(*fileBank, all.c_str() + bankPos + 6)) {
                std::memcpy(nb->machine, fileBank->machine, sizeof(nb->machine));
                std::memcpy(nb->mute, fileBank->mute, sizeof(nb->mute));
                std::memcpy(nb->kit, fileBank->kit, sizeof(nb->kit));
                if (withPatterns) for (int p = 0; p < kPatterns; ++p) nb->patterns[p] = fileBank->patterns[p];
            }
        }
        applyState(nv.data(), nb.get());
        if (withPatterns) { setParamGesture(kGlobalBase + gPATTERN, (float)clampi(patIdx, 0, kPatterns - 1)); stepPg = 0; }
        lockStep = -1;
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
            presetTabUser = true;
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

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
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
        writeDefaultView(skin, zoomPercent()); // and used by new instances
        viewDirty = false;
    }
    void onResize(const ResizeEvent& ev) override
    {
        UI::onResize(ev);
        viewDirty = true; viewDirtyTicks = 0;
    }

    // ------------------------------------------------------------------ licence (registration)
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

    void uiFileBrowserSelected(const char* filename) override
    {
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
        lockStep = -1;
        presetName = kPresets[i].name;
    }

    // ------------------------------------------------------------------ whole-state helpers (presets, random, undo)
    void applyState(const float* nv, const Bank* nb)
    {
        for (int i = 0; i < kGlobalBase + gOUT_STEP; ++i) {
            if (i == kGlobalBase + gPLAY || i == kGlobalBase + gPATTERN) continue;
            if (std::fabs(values[i] - nv[i]) > 1e-4f) setParamGesture((uint32_t)i, nv[i]);
        }
        if (nb) {
            *bank = *nb;
            setState("bank", serializeBank(*bank).c_str());
        }
        repaint();
    }

    void snapshot(const char* what)
    {
        undoValues.assign(values, values + kTotalParams);
        undoBank = serializeBank(*bank);
        undoValid = true; undoWhat = what;
    }

    void undo()
    {
        if (!undoValid) return;
        std::unique_ptr<Bank> b(new Bank());
        deserializeBank(*b, undoBank.c_str());
        std::vector<float> v = undoValues;
        undoValid = false;
        applyState(v.data(), b.get());
    }

    void randomize(int what)
    {
        RandGen r(++rndCounter * 2654435761u ^ lastClickTime);
        static const char* names[4] = {"sound", "sequence", "pattern", "everything"};
        snapshot(names[what]);
        std::vector<float> nv(values, values + kTotalParams);
        if (what == 0) {
            randomizeSound(nv.data(), selTrack, machineOf(selTrack), r);
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
        if (what >= 2) lockStep = -1;
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
        Pattern& p = pat();
        const int pi = editPattern();
        std::string c;
        char b[64];
        if (!(p.flags[selTrack][s] & sfTrig)) { std::snprintf(b, sizeof(b), "f %d %d %d %d\n", pi, selTrack, s, sfTrig); c += b; }
        if (mode == mdRatch) {
            int idx = 0;
            for (int i = 0; i < kRatchCount; ++i) if (kRatchValues[i] == std::max<int>(1, p.ratch[selTrack][s])) idx = i;
            idx = dbl ? 0 : (idx + (right ? kRatchCount - 1 : 1)) % kRatchCount;
            std::snprintf(b, sizeof(b), "r %d %d %d %d", pi, selTrack, s, kRatchValues[idx]);
        } else if (mode == mdCond) {
            const int v = dbl ? 0 : (p.cond[selTrack][s] + (right ? kNumConds - 1 : 1)) % kNumConds;
            std::snprintf(b, sizeof(b), "c %d %d %d %d", pi, selTrack, s, v);
        } else {
            const int st = shift ? 4 : 1;
            const int v = dbl ? 0 : clampi(p.micro[selTrack][s] + (right ? -st : st), -kMicroMax, kMicroMax);
            std::snprintf(b, sizeof(b), "m %d %d %d %d", pi, selTrack, s, v);
        }
        c += b;
        sendLines(c);
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
        if (lockStep >= newLen) lockStep = -1;
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

    void writeLock(int pi, int v)
    {
        char b[64];
        Pattern& p = pat();
        if (!(p.flags[selTrack][lockStep] & sfTrig) && v != kNoLock) {
            std::snprintf(b, sizeof(b), "f %d %d %d %d", editPattern(), selTrack, lockStep, p.flags[selTrack][lockStep] | sfTrig);
            sendCmd(b);
        }
        std::snprintf(b, sizeof(b), "l %d %d %d %d %d", editPattern(), selTrack, lockStep, pi, v);
        sendCmd(b);
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
            if (pat().locks[selTrack][lockStep][pi] != iv) writeLock(pi, iv);
        } else {
            setParam(selTrack * kTrackParams + pi, v);
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
        if (ev.press && ev.key == kKeyEscape) {
            if (overlay != ovNone) overlay = ovNone; else lockStep = -1;
            repaint(); return true;
        }
        return false;
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        if (!ev.press) {
            if (drag == dkTrackKnob && std::fabs(y - dragY) < 3.f && (page == pgLfo1 || page == pgLfo2) && (dragIndex == 4 || dragIndex == 5)) {
                editParameter(selTrack * kTrackParams + pageParam(page, dragIndex), false);
                drag = dkNone; openDestMenu(page - pgLfo1);    // a click (no drag) on DEST / TARGET opens the list
                return true;
            }
            if (drag == dkTrackKnob && lockStep < 0 && !recording()) editParameter(selTrack * kTrackParams + pageParam(page, dragIndex), false);
            recDirty = false;
            if (drag == dkMasterKnob) editParameter(kGlobalBase + masterParam(mpage, dragIndex), false);
            if (drag == dkTempo) editParameter(kGlobalBase + gTEMPO, false);
            if (drag == dkSwing || drag == dkAccent) sendPatternSettings();
            if (drag == (DragKind)dkResize) saveView();
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
        if (overlay == ovDest) return destClick(x, y);
        if (overlay == ovNone && (page == pgLfo1 || page == pgLfo2) && lfoDestBox().in(x, y)) { openDestMenu(page - pgLfo1); return true; }
        if (overlay == ovHelp) { overlay = ovNone; repaint(); return true; }
        if (overlay == ovLicense) return licenseClick(x, y);
        if (overlay == ovNone && logoArea().in(x, y)) { overlay = ovLicense; licMsg.clear(); repaint(); return true; }
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
        if (recBtn().in(x, y)) { rec = !rec; repaint(); return true; }
        for (int i = 0; i < 4; ++i) if (rndBtn(i).in(x, y)) { randomize(i); return true; }
        if (undoBtn().in(x, y)) { undo(); return true; }
        if (presetBtn().in(x, y)) { openPresets(); return true; }
        if (saveBtn().in(x, y)) { openSave(); return true; }
        if (machPrev().in(x, y)) { setMachine(selTrack, machineOf(selTrack) - 1); return true; }
        if (machNext().in(x, y)) { setMachine(selTrack, machineOf(selTrack) + 1); return true; }
        if (machName().in(x, y)) { overlay = ovPicker; repaint(); return true; }
        if (lockStep >= 0 && lockClear().in(x, y)) {
            char b[48]; std::snprintf(b, sizeof(b), "cl %d %d %d", editPattern(), selTrack, lockStep); sendCmd(b);
            repaint(); return true;
        }
        for (int i = 0; i < kPages; ++i) if (pageTab(i).in(x, y)) { page = i; repaint(); return true; }
        for (int i = 0; i < 3; ++i) if (mTab(i).in(x, y)) { mpage = i; repaint(); return true; }

        for (int k = 0; k < 8; ++k) {
            if (!knob(k).in(x, y) || kitField(k) < 0) continue;
            if (dbl) { setKit(selTrack, kitField(k), 0); return true; }
            drag = dkKit; dragIndex = k; dragY = y; dragStart = bank->kit[selTrack][kitField(k)];
            repaint(); return true;
        }
        for (int k = 0; k < 8; ++k) {
            if (!knob(k).in(x, y) || pageParam(page, k) < 0) continue;
            const int pi = pageParam(page, k);
            if (dbl) {
                if (lockStep >= 0) writeLock(pi, kNoLock);
                else {
                    const float def = pi < 8 ? kMachineDefs[machineOf(selTrack)].def[pi] : kFxDefaults[pi - 8];
                    setParamGesture(selTrack * kTrackParams + pi, def);
                }
                repaint(); return true;
            }
            drag = dkTrackKnob; dragIndex = k; dragY = y; dragStart = knobValue(k); recDirty = false;
            if (lockStep < 0 && !recording()) editParameter(selTrack * kTrackParams + pi, true);
            else if (pat().locks[selTrack][lockStep][pi] == kNoLock) setKnob(k, dragStart);
            repaint(); return true;
        }
        for (int k = 0; k < 8; ++k) {
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
            if (patBtn(i).in(x, y)) { setParamGesture(kGlobalBase + gPATTERN, (float)i); lockStep = -1; stepPg = 0; repaint(); return true; }
        if (patCopy().in(x, y)) { patClip = editPattern(); repaint(); return true; }
        if (patPaste().in(x, y) && patClip >= 0) {
            char b[32]; std::snprintf(b, sizeof(b), "pc %d %d", editPattern(), patClip); sendCmd(b); repaint(); return true;
        }
        if (patClear().in(x, y)) { char b[32]; std::snprintf(b, sizeof(b), "cp %d", editPattern()); sendCmd(b); repaint(); return true; }

        for (int i = 0; i < kModes; ++i) if (modeBtn(i).in(x, y)) { mode = i; repaint(); return true; }
        if (fillBtn().in(x, y)) { fillOn = !fillOn; sendCmd(fillOn ? "fill 1" : "fill 0"); repaint(); return true; }
        Pattern& p = pat();
        for (int i = 0; i < 4; ++i)
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
            setTrackLen(selTrack, ((tlen(selTrack) - 1) / 16 + 1) % 4 * 16 + 16, true, (bool)(ev.mod & kModifierShift));
            repaint(); return true;
        }
        if (patLenBox().in(x, y)) { // master length: when pattern changes happen
            p.length = (uint8_t)(((p.length - 1) / 16 + 1) % 4 * 16 + 16);
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
        if (trkCopy().in(x, y)) { trackClip = trackToHex(p, selTrack); repaint(); return true; }
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
            if (right) { lockStep = (lockStep == s) ? -1 : s; repaint(); return true; }
            uint8_t f = p.flags[selTrack][s];
            if (mode == mdTrig) f = (f & sfTrig) ? 0 : sfTrig;
            else if (mode == mdAccent) f = (f & sfAccent) ? (uint8_t)(f & ~sfAccent) : (uint8_t)(f | sfAccent | sfTrig);
            else f = (f & sfSlide) ? (uint8_t)(f & ~sfSlide) : (uint8_t)(f | sfSlide | sfTrig);
            char b[48]; std::snprintf(b, sizeof(b), "f %d %d %d %d", editPattern(), selTrack, s, f);
            sendCmd(b);
            repaint(); return true;
        }
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
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
        if (overlay == ovPresets && d != 0.f) {
            const int rows = (presetCount() + 1) / 2;
            presetScroll = clampi(presetScroll - (int)d, 0, std::max(0, rows - 11));
            repaint(); return true;
        }
        if (d == 0.f || overlay != ovNone) return false;
        for (int k = 0; k < 8; ++k) {
            if (knob(k).in(x, y) && kitField(k) >= 0) {
                const int f = kitField(k);
                setKit(selTrack, f, clampi(bank->kit[selTrack][f] + (int)d, 0, kKitMax[f])); return true;
            }
            if (knob(k).in(x, y) && pageParam(page, k) >= 0) {
                const uint32_t idx = selTrack * kTrackParams + pageParam(page, k);
                const bool gesture = lockStep < 0 && !recording();
                if (gesture) editParameter(idx, true);
                setKnob(k, knobValue(k) + d);
                if (gesture) editParameter(idx, false);
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
    int selTrack = 0, page = pgSynth, mpage = mpEcho, mode = mdTrig, stepPg = 0, lockStep = -1;
    Overlay overlay = ovNone;
    DragKind drag = dkNone; int dragIndex = 0; float dragY = 0, dragStart = 0;
    uint lastClickTime = 0; float lastX = -100, lastY = -100;
    int patClip = -1;
    std::string trackClip;
    bool rec = false, recDirty = false; float recVal = 0;
    bool fillOn = false;
    int skin = kDefaultSkin;
    const Theme* th = &kThemes[kDefaultSkin];
    FontId fonts[kFontCount] = {};
    NanoImage dotImage, haloImage;
    int presetSel = -1, presetScroll = 0;
    bool presetTabUser = false, deleteArmed = false, saveConfirm = false;
    std::vector<std::string> userPresets;
    std::string presetMsg, saveName;
    bool viewDirty = false; int viewDirtyTicks = 0;
    double resizeStartW = 0, grabX = 0, grabY = 0;
    std::string presetName = "Init";
    lic::Info licInfo = lic::findInstalled("KDSP-16");
    std::string licMsg;
    uint32_t rndCounter = 0x51ED;
    bool undoValid = false; std::vector<float> undoValues; std::string undoBank; const char* undoWhat = "";

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KinetikUI)
};

UI* createUI() { return new KinetikUI(); }

END_NAMESPACE_DISTRHO
