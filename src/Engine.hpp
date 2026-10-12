// Kinetik KDSP-16 — engine: 16 tracks, per-track FX + 2 LFOs, sequencer, master FX, 4 stereo outputs
#pragma once
#include "Bank.hpp"
#include "Voice.hpp"
#include "MasterFx.hpp"
#include <vector>
#include <algorithm>
#include <climits>
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif
// flush denormals to zero (avoids CPU spikes in decaying filters and reverbs)
inline void knDenormalsOff()
{
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
    _mm_setcsr(_mm_getcsr() | 0x8040);
#elif defined(__aarch64__)
    uint64_t fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#endif
}

namespace kn {

struct NoteEvent { uint32_t frame; int track; float velocity; int semis = 0; };

struct Transport {
    bool hostValid = false;   // host gave a musical position
    bool hostPlaying = false;
    double beat = 0.0;        // absolute position in quarter notes at block start
    double bpm = 120.0;
};

static constexpr int kOutBuses = 4;   // main + 3 stereo aux outputs

// ---------------------------------------------------------------------------
struct Track {
    Voice voice;
    float sr = 48000.f;
    // fx
    float amPh = 0;
    Biquad eq; float eqDb = 0;
    Svf hp, lp, lp2; bool hpOn = false, lpOn = false; int ftype = 0; float bpGain = 1.f;
    int srrN = 1, srrCnt = 0; float srrHold = 0;
    float vol = 0, panL = 0.707f, panR = 0.707f, del = 0, rev = 0; // targets
    float cVol = 0, cPanL = 0.707f, cPanR = 0.707f, cDel = 0, cRev = 0; // current (smoothed)
    float amd = 0, amf = 1, dist = 0;
    float fenv = 0;                       // filter envelope (1 at trig, decays)
    float chokeEnv = 1.f; bool choking = false;
    // lfos
    float lfoPh[2] = {0, 0}, lfoHeld[2] = {0, 0}, lfoRnd[2] = {0, 0}, lfoOut[2] = {0, 0};
    // trig-time locks
    uint8_t lockVal[kLockable] = {}; bool locked[kLockable] = {};
    float slideFrom[kLockable] = {}; bool sliding[kLockable] = {}; uint32_t slidePos = 0, slideLen = 1;
    float eff[kLockable] = {};
    float mod[kLockable] = {};            // LFO modulation received (from this or other tracks)
    uint32_t ledTimer = 0, idle = 0;
    // conditional trigs
    int64_t decK = INT64_MIN; bool decOk = true; bool pre = true;
    std::vector<float> buf;

    void init(float s)
    {
        sr = s; voice.init(s); buf.assign(64, 0.f);
        std::fill(locked, locked + kLockable, false);
        std::fill(sliding, sliding + kLockable, false);
    }
};

class Engine {
public:
    float trackParam[kTracks][kTrackParams];
    float global[kGlobalParams];
    Bank bank;               // audio-side copy (the plugin syncs it)
    bool fill = false;       // FILL condition state

    // outputs for the UI
    int outStep = -1, outPattern = 0; // outStep: absolute step counter modulo 2^20 (-1 = stopped)
    uint32_t outLeds = 0;
    uint32_t trigCount[kTracks] = {}; // diagnostics (tests)

    Engine()
    {
        for (int t = 0; t < kTracks; ++t)
            for (int p = 0; p < kTrackParams; ++p) trackParam[t][p] = trackParamDefault(t, p);
        for (int g = 0; g < kGlobalParams; ++g) global[g] = 0;
        for (int i = 0; i < kMasterCount; ++i) global[gECHO_TIME + i] = kMasterDefaults[i];
        global[gTEMPO] = 120;
        bank.initDefault();
    }

    void init(double sampleRate)
    {
        sr = (float)sampleRate;
        for (auto& tr : tracks) tr.init(sr);
        for (int t = 0; t < kTracks; ++t) { tracks[t].voice.machine = -1; tracks[t].voice.setMachine(bank.machine[t]); }
        fx.init(sr);
        sendDL.assign(64, 0); sendDR.assign(64, 0); sendRL.assign(64, 0); sendRR.assign(64, 0);
        for (int b = 0; b < kOutBuses; ++b) { busL[b].assign(64, 0); busR[b].assign(64, 0); }
        events.reserve(4096);
        reset();
    }

    void reset()
    {
        for (auto& tr : tracks) { tr.voice.active = false; tr.choking = false; tr.chokeEnv = 1.f; }
        fx.reset();
        wasPlaying = false; internalBeat = 0; prevPlayParam = false; usingInternal = false;
    }

    void audition(int track, float vel = 1.f) { if (track >= 0 && track < kTracks) pendingAudition[track] = vel; }

    // convenience for tests: stereo main output only
    void process(float* outL, float* outR, uint32_t frames, const Transport& tp, const NoteEvent* notes, uint32_t nNotes)
    {
        if (dummy.size() < frames) dummy.assign(frames, 0.f);
        float* outs[2 * kOutBuses] = {outL, outR};
        for (int i = 2; i < 2 * kOutBuses; ++i) outs[i] = dummy.data();
        process(outs, frames, tp, notes, nNotes);
    }

    // -----------------------------------------------------------------------
    void process(float* const* outs, uint32_t frames, const Transport& tp, const NoteEvent* notes, uint32_t nNotes)
    {
        knDenormalsOff();
        if (frames == 0) return;
        for (int t = 0; t < kTracks; ++t) {
            Voice& v = tracks[t].voice;
            v.setMachine(bank.machine[t]);
            v.fmQuant = bank.kit[t][kfFmq] != 0;
            v.oscShape = bank.kit[t][kfOsc];
        }

        // ---- transport ----
        const bool playParam = global[gPLAY] > 0.5f;
        double bpm = tp.hostValid && tp.bpm > 1.0 ? tp.bpm : (double)global[gTEMPO];
        bpm = std::max(20.0, std::min(400.0, bpm));
        curBpm = bpm;
        bool playing = false, restarted = false;
        double b0 = 0;
        if (tp.hostValid && tp.hostPlaying) {
            playing = true; b0 = tp.beat;
            if (!wasPlaying || usingInternal || std::fabs(b0 - prevB1) > 0.01) restarted = true;
            usingInternal = false;
        } else if (playParam) {
            if (!prevPlayParam || !wasPlaying || !usingInternal) { internalBeat = 0; restarted = true; }
            playing = true; b0 = internalBeat; usingInternal = true;
        }
        prevPlayParam = playParam;
        const double beatsPerFrame = bpm / (60.0 * sr);
        const double b1 = b0 + beatsPerFrame * frames;
        blockBpf = beatsPerFrame;
        if (playing) lfoClock = b0;     // synced LFOs follow the song position; stopped, they keep the tempo
        if (!playing) {
            wasPlaying = false; usingInternal = false; outStep = -1;
            curPattern = clampi((int)global[gPATTERN], 0, kPatterns - 1);
        }

        // ---- collect events ----
        events.clear();
        for (uint32_t i = 0; i < nNotes; ++i)
            events.push_back({std::min(notes[i].frame, frames - 1), 0, notes[i].track, notes[i].velocity, 0, 0, notes[i].semis});
        for (int t = 0; t < kTracks; ++t)
            if (pendingAudition[t] > 0.f) { events.push_back({0, 0, t, pendingAudition[t], 0, 0, 0}); pendingAudition[t] = 0.f; }

        if (playing) {
            const double sb = stepBeats();
            const double winStart = restarted ? b0 - 1e-9 : std::max(b0, prevB1);
            if (restarted) {
                lastBoundaryK = (int64_t)std::ceil(b0 / sb - 1e-9) - 1;
                playStartK = lastBoundaryK + 1;
                for (auto& tr : tracks) { tr.decK = INT64_MIN; tr.pre = true; }
            }
            const Pattern& pat0 = bank.patterns[curPattern];
            const double swingOff = (pat0.swing / 100.0 * 2.0 - 1.0) * sb;
            const double maxMicro = (double)kMicroMax / 24.0 * sb;
            const int64_t kFirst = (int64_t)std::floor((b0 - swingOff - maxMicro) / sb) - 2;
            const int64_t kLast = (int64_t)std::floor((b1 + maxMicro) / sb) + 1;
            auto toFrame = [&](double tk) { return (uint32_t)std::min<double>(frames - 1, std::max(0.0, (tk - b0) / beatsPerFrame)); };
            // which pattern plays step k (a queued pattern change happens on the next master-length boundary)
            const int want = clampi((int)global[gPATTERN], 0, kPatterns - 1);
            const int master = pat0.length ? pat0.length : 16;
            const int64_t nextSwitch = ((lastBoundaryK + 1 + master - 1) / master) * master;
            auto patFor = [&](int64_t k) -> const Pattern& {
                return (want != curPattern && k >= nextSwitch) ? bank.patterns[want] : bank.patterns[curPattern];
            };
            // boundaries (pattern change + playhead)
            for (int64_t k = std::max<int64_t>(kFirst, lastBoundaryK + 1); k <= kLast; ++k) {
                const double tb = k * sb;
                if (tb < winStart || tb >= b1 || tb < -1e-9) continue;
                events.push_back({toFrame(tb), 1, -1, 1.f, k, 0, 0});
                lastBoundaryK = k;
            }
            // track trigs (swing, micro-timing, ratchets)
            for (int t = 0; t < kTracks; ++t) {
                for (int64_t k = kFirst; k <= kLast; ++k) {
                    if (k < 0) continue;
                    const Pattern& P = patFor(k);
                    const int s = P.trackStep(t, k);
                    if (!(P.flags[t][s] & sfTrig)) continue;
                    const int n = std::max<int>(1, P.ratch[t][s]);
                    const double base = k * sb + ((k & 1) ? (P.swing / 100.0 * 2.0 - 1.0) * sb : 0.0) + P.micro[t][s] / 24.0 * sb;
                    for (int j = 0; j < n; ++j) {
                        const double tk = base + j * sb / n;
                        if (tk < winStart || tk >= b1 || tk < -1e-9) continue;
                        events.push_back({toFrame(tk), 2, t, 1.f, k, j, 0});
                    }
                }
            }
            wasPlaying = true;
            prevB1 = b1;
            if (usingInternal) internalBeat = b1;
        }
        std::stable_sort(events.begin(), events.end(), [](const Ev& a, const Ev& b) {
            return a.frame != b.frame ? a.frame < b.frame : a.kind < b.kind; });

        // ---- render ----
        uint32_t pos = 0; size_t ei = 0;
        while (pos < frames) {
            while (ei < events.size() && events[ei].frame <= pos) { fire(events[ei]); ++ei; }
            uint32_t end = std::min<uint32_t>(frames, pos + kCtl);
            if (ei < events.size() && events[ei].frame < end) end = events[ei].frame;
            if (end <= pos) end = pos + 1;
            float* seg[2 * kOutBuses];
            for (int i = 0; i < 2 * kOutBuses; ++i) seg[i] = outs[i] + pos;
            renderSegment(seg, end - pos);
            pos = end;
        }

        uint32_t leds = 0;
        for (int t = 0; t < kTracks; ++t) {
            if (tracks[t].ledTimer > 0) {
                leds |= 1u << t;
                tracks[t].ledTimer = tracks[t].ledTimer > frames ? tracks[t].ledTimer - frames : 0;
            }
        }
        outLeds = leds;
        outPattern = curPattern;
    }

    double stepBeats() const { return 0.25 / kScaleMult[bank.patterns[curPattern].scale % 7]; }
    int currentPattern() const { return curPattern; }

private:
    struct Ev { uint32_t frame; int kind; int track; float vel; int64_t k; int sub; int semis; }; // kind: 0 note, 1 boundary, 2 step
    static constexpr uint32_t kCtl = 32;

    float sr = 48000.f;
    Track tracks[kTracks];
    std::vector<Ev> events;
    std::vector<float> dummy;
    float pendingAudition[kTracks] = {};
    int curPattern = 0;
    int64_t lastBoundaryK = -1, playStartK = 0;
    bool wasPlaying = false, prevPlayParam = false, usingInternal = false;
    double internalBeat = 0, prevB1 = 0, curBpm = 120;
    Rng condRng;

    // master
    MasterFx fx;
    double lfoClock = 0, blockBpf = 0;   // beat position used by tempo-synced LFOs
    std::vector<float> sendDL, sendDR, sendRL, sendRR;
    std::vector<float> busL[kOutBuses], busR[kOutBuses];

    // -----------------------------------------------------------------------
    bool evalCond(Track& tr, int c, int64_t k, int len)
    {
        if (c <= 0 || c >= kNumConds) return true;
        const CondDef& cd = kConds[c];
        bool ok = true, updates = true;
        switch (cd.kind) {
        case ckProb: ok = condRng.uni() * 100.f < (float)cd.a; break;
        case ckFill: ok = fill; break;
        case ckNotFill: ok = !fill; break;
        case ckPre: ok = tr.pre; updates = false; break;
        case ckNotPre: ok = !tr.pre; updates = false; break;
        case ckFirst: ok = (k - playStartK) / std::max(1, len) == 0; break;
        case ckNotFirst: ok = (k - playStartK) / std::max(1, len) != 0; break;
        case ckRatio: { const int64_t it = (k >= 0 ? k : 0) / std::max(1, len); ok = (it % cd.b) == cd.a - 1; break; }
        default: break;
        }
        if (updates) tr.pre = ok;
        return ok;
    }

    void fire(const Ev& e)
    {
        if (e.kind == 0) { triggerTrack(e.track, e.vel, nullptr, nullptr, 0, e.semis); return; }
        if (e.kind == 1) {
            const Pattern& pat = bank.patterns[curPattern];
            const int master = pat.length ? pat.length : 16;
            if ((((e.k % master) + master) % master) == 0) {
                const int want = clampi((int)global[gPATTERN], 0, kPatterns - 1);
                if (want != curPattern) curPattern = want;
            }
            outStep = (int)(((e.k % 1048576) + 1048576) % 1048576);
            return;
        }
        // sequencer trig
        Track& tr = tracks[e.track];
        const Pattern& P = bank.patterns[curPattern];
        const int t = e.track;
        const int s = P.trackStep(t, e.k);
        const uint8_t f = P.flags[t][s];
        if (!(f & sfTrig)) return;
        if (e.sub == 0) {
            tr.decK = e.k;
            tr.decOk = evalCond(tr, P.cond[t][s], e.k, P.trackLen[t]);
        } else if (tr.decK != e.k) return;
        if (!tr.decOk || bank.mute[t]) return;
        const float acc = 1.f + P.accent / 127.f * 0.8f;
        const int n = std::max<int>(1, P.ratch[t][s]);
        const uint32_t stepLen = (uint32_t)(stepBeats() * 60.0 / curBpm * sr / n);
        triggerTrack(t, (f & sfAccent) ? acc : 1.f, P.locks[t][s], &f, stepLen, 0);
    }

    void triggerTrack(int t, float vel, const uint8_t* locks, const uint8_t* flags, uint32_t stepLen, int semis)
    {
        if (t < 0 || t >= kTracks) return;
        Track& tr = tracks[t];
        const bool slide = flags && (*flags & sfSlide);
        for (int p = 0; p < kLockable; ++p) {
            const bool l = locks && locks[p] != kNoLock;
            if (slide && l) { tr.slideFrom[p] = tr.eff[p]; tr.sliding[p] = true; }
            else tr.sliding[p] = false;
            tr.locked[p] = l;
            if (l) tr.lockVal[p] = locks[p];
        }
        tr.slidePos = 0; tr.slideLen = std::max<uint32_t>(1, stepLen);
        // lfo trig / hold modes
        for (int l = 0; l < 2; ++l) {
            const int type = lfoTypeIndex((int)effParam(t, kLfoBase[l] + 3));
            if (type == 1) tr.lfoPh[l] = 0;
            if (type == 2) tr.lfoHeld[l] = lfoShape(t, l);
        }
        // choke group: silence the other tracks of the same group
        const int grp = bank.kit[t][kfChoke];
        if (grp)
            for (int u = 0; u < kTracks; ++u)
                if (u != t && bank.kit[u][kfChoke] == grp && tracks[u].voice.active) tracks[u].choking = true;
        tr.choking = false; tr.chokeEnv = 1.f;
        tr.fenv = 1.f;
        computeEffective(t);
        const float sens = tr.eff[tpVEL] / 127.f;
        const float g = std::max(0.f, 1.f + (vel - 1.f) * sens);
        tr.voice.pitchMul = semis ? std::exp2(semis / 12.f) : 1.f;
        tr.voice.control(tr.eff);
        tr.voice.trigger(g);
        ++trigCount[t];
        tr.idle = 0;
        tr.ledTimer = (uint32_t)(0.08f * sr);
    }

    float effParam(int t, int p) const
    {
        const Track& tr = tracks[t];
        return tr.locked[p] ? (float)tr.lockVal[p] : trackParam[t][p];
    }

    float lfoShape(int t, int l)
    {
        Track& tr = tracks[t];
        const int shp = lfoShapeIndex((int)effParam(t, kLfoBase[l] + 2));
        float ph = tr.lfoPh[l] + effParam(t, lfoParam(l, 7)) / 128.f;
        if (ph >= 1.f) ph -= 1.f;
        switch (shp) {
        case 0: return 1.f - 4.f * std::fabs(ph - 0.5f);
        case 1: return std::sin(kTwoPi * ph);
        case 2: return ph < 0.5f ? 1.f : -1.f;
        case 3: return 2.f * ph - 1.f;
        case 4: return 1.f - 2.f * ph;
        default: return tr.lfoRnd[l];
        }
    }

    void computeEffective(int t)
    {
        Track& tr = tracks[t];
        const float sl = tr.slideLen > 0 ? std::min(1.f, (float)tr.slidePos / (float)tr.slideLen) : 1.f;
        for (int p = 0; p < kLockable; ++p) {
            float v = effParam(t, p);
            if (tr.sliding[p]) v = tr.slideFrom[p] + (v - tr.slideFrom[p]) * sl;
            if (tr.mod[p] != 0.f) v = clampf(v + tr.mod[p], 0.f, 127.f);
            tr.eff[p] = v;
        }
    }

    // advance every LFO and route its output (to its own track or another one)
    void runLfos(uint32_t n)
    {
        for (auto& tr : tracks) std::fill(tr.mod, tr.mod + kLockable, 0.f);
        for (int t = 0; t < kTracks; ++t) {
            Track& tr = tracks[t];
            for (int l = 0; l < 2; ++l) {
                const int b = kLfoBase[l];
                const int type = lfoTypeIndex((int)effParam(t, b + 3));
                if (lfoSynced(effParam(t, lfoParam(l, 6)))) {
                    const double cyc = kLfoSyncBeats[lfoSyncDivIndex((int)effParam(t, b))];
                    if (type == 0) {
                        // FREE + SYNC: locked to the song position
                        const double ph = lfoClock / cyc - std::floor(lfoClock / cyc);
                        if ((float)ph < tr.lfoPh[l]) tr.lfoRnd[l] = tr.voice.rng.next();
                        tr.lfoPh[l] = (float)ph;
                    } else {
                        tr.lfoPh[l] += (float)(blockBpf * n / cyc);
                        while (tr.lfoPh[l] >= 1.f) { tr.lfoPh[l] -= 1.f; tr.lfoRnd[l] = tr.voice.rng.next(); }
                    }
                } else {
                    tr.lfoPh[l] += lfoFreeHz(effParam(t, b)) * n / sr;
                    while (tr.lfoPh[l] >= 1.f) { tr.lfoPh[l] -= 1.f; tr.lfoRnd[l] = tr.voice.rng.next(); }
                }
                const float dep = effParam(t, b + 1);
                if (dep < 0.5f) continue;
                const float out = type == 2 ? tr.lfoHeld[l] : lfoShape(t, l);
                tr.lfoOut[l] = out;
                const int dst = std::min(kLfoDests - 1, lfoDestIndex((int)effParam(t, b + 4)));
                const int trk = lfoTrackIndex((int)effParam(t, b + 5));
                const int target = trk == 0 ? t : trk - 1;
                tracks[target].mod[dst] += out * dep * 0.5f;
            }
        }
    }

    void controlTrack(int t, uint32_t n)
    {
        Track& tr = tracks[t];
        if (tr.slidePos < tr.slideLen) tr.slidePos += n;
        computeEffective(t);
        const float* e = tr.eff;
        tr.voice.control(e);
        tr.amd = e[tpAMD] / 127.f;
        tr.amf = expmap(e[tpAMF] / 127.f, 0.5f, 2000.f);
        const float db = (e[tpEQG] - 64.f) / 64.f * 18.f;
        tr.eqDb = db;
        if (std::fabs(db) > 0.05f) tr.eq.setPeak(expmap(e[tpEQF] / 127.f, 40.f, 16000.f), 1.0f, db, sr);
        // filter + filter envelope
        tr.fenv *= std::pow(decayCoef(expmap(e[tpFDEC] / 127.f, 0.005f, 3.f), sr), (float)n);
        const float envOct = (e[tpFENV] - 64.f) / 64.f * 6.f * tr.fenv;
        const float envMul = std::fabs(envOct) > 0.001f ? std::exp2(envOct) : 1.f;
        const float q = 0.5f + e[tpRESO] / 127.f * 12.f;
        const float cutHz = std::min(20000.f, cutoffHz(e[tpCUTOFF]) * envMul);
        tr.ftype = filterTypeIndex((int)e[tpFTYPE]);
        tr.hpOn = false;
        tr.lpOn = tr.ftype != 0 || cutHz < 19000.f || q > 0.6f;
        if (tr.lpOn) {
            tr.lp.setup(cutHz, q, sr);
            if (tr.ftype == 1) tr.lp2.setup(cutHz, 0.6f, sr);
            tr.bpGain = std::sqrt(tr.lp.k) * 1.4f;
        }
        tr.srrN = e[tpSRR] > 0.5f ? (int)expmap(e[tpSRR] / 127.f, 1.f, 64.f) : 1;
        tr.dist = e[tpDIST] / 127.f;
        const float v = e[tpVOL] / 127.f;
        tr.vol = bank.mute[t] ? 0.f : v * v * 0.8f; // mute silences the track at once
        const float pan = e[tpPAN] / 127.f;
        tr.panL = std::cos(pan * kPi * 0.5f); tr.panR = std::sin(pan * kPi * 0.5f);
        tr.del = e[tpDEL] / 127.f * 1.4f; tr.rev = e[tpREV] / 127.f * 1.4f;   // sends go above unity
    }

    void renderSegment(float* const* outs, uint32_t n)
    {
        for (int b = 0; b < kOutBuses; ++b) { std::fill(busL[b].begin(), busL[b].begin() + n, 0.f); std::fill(busR[b].begin(), busR[b].begin() + n, 0.f); }
        std::fill(sendDL.begin(), sendDL.begin() + n, 0.f); std::fill(sendDR.begin(), sendDR.begin() + n, 0.f);
        std::fill(sendRL.begin(), sendRL.begin() + n, 0.f); std::fill(sendRR.begin(), sendRR.begin() + n, 0.f);
        runLfos(n);
        lfoClock += blockBpf * n;
        const float inv = 1.f / n;
        const float chokeCoef = decayCoef(0.012f, sr);
        for (int t = 0; t < kTracks; ++t) {
            Track& tr = tracks[t];
            if (tr.voice.active) tr.idle = 0; else tr.idle += n;
            if (tr.idle > (uint32_t)(sr * 0.5f)) continue;
            controlTrack(t, n);
            if (tr.idle == 0 && tr.cVol == 0.f) { tr.cVol = tr.vol; tr.cPanL = tr.panL; tr.cPanR = tr.panR; tr.cDel = tr.del; tr.cRev = tr.rev; }
            float* b = tr.buf.data();
            tr.voice.render(b, (int)n);
            if (tr.choking) {
                for (uint32_t i = 0; i < n; ++i) { b[i] *= tr.chokeEnv; tr.chokeEnv *= chokeCoef; }
                if (tr.chokeEnv < 1e-4f) { tr.voice.active = false; tr.choking = false; tr.chokeEnv = 1.f; }
            }
            const int bus = std::min<int>(kOutBuses - 1, bank.kit[t][kfOut]);
            float* mL = busL[bus].data(); float* mR = busR[bus].data();
            const float dVol = (tr.vol - tr.cVol) * inv, dPL = (tr.panL - tr.cPanL) * inv, dPR = (tr.panR - tr.cPanR) * inv;
            const float dDel = (tr.del - tr.cDel) * inv, dRev = (tr.rev - tr.cRev) * inv;
            const float amInc = tr.amf / sr;
            const bool eqOn = std::fabs(tr.eqDb) > 0.05f;
            for (uint32_t i = 0; i < n; ++i) {
                float s = drive(b[i], tr.dist);
                if (tr.amd > 0.f) {
                    tr.amPh += amInc; if (tr.amPh >= 1.f) tr.amPh -= 1.f;
                    s *= 1.f - tr.amd * (0.5f - 0.5f * std::sin(kTwoPi * tr.amPh));
                }
                if (eqOn) s = tr.eq.tick(s);
                if (tr.hpOn) { tr.hp.tick(s); s = tr.hp.hp; }
                if (tr.lpOn) {
                    tr.lp.tick(s);
                    switch (tr.ftype) {
                    case 0: s = tr.lp.lp; break;
                    case 1: tr.lp2.tick(tr.lp.lp); s = tr.lp2.lp; break;
                    case 2: s = tr.lp.bp * tr.bpGain; break;
                    case 3: s = tr.lp.hp; break;
                    default: s = tr.lp.lp + tr.lp.hp; break;
                    }
                }
                if (tr.srrN > 1) { if (--tr.srrCnt <= 0) { tr.srrCnt = tr.srrN; tr.srrHold = s; } s = tr.srrHold; }
                tr.cVol += dVol; tr.cPanL += dPL; tr.cPanR += dPR; tr.cDel += dDel; tr.cRev += dRev;
                s *= tr.cVol;
                const float l = s * tr.cPanL, r = s * tr.cPanR;
                mL[i] += l; mR[i] += r;
                sendDL[i] += l * tr.cDel; sendDR[i] += r * tr.cDel;
                sendRL[i] += l * tr.cRev; sendRR[i] += r * tr.cRev;
            }
        }
        fx.process(global, (float)curBpm, busL[0].data(), busR[0].data(), sendDL.data(), sendDR.data(), sendRL.data(), sendRR.data(),
                   outs[0], outs[1], n);
        const float mv = global[gVOLUME] / 127.f; const float vol = mv * mv * 1.2f;
        for (int b = 1; b < kOutBuses; ++b)
            for (uint32_t i = 0; i < n; ++i) { outs[2 * b][i] = safety(busL[b][i] * vol); outs[2 * b + 1][i] = safety(busR[b][i] * vol); }
    }

    static inline float safety(float x) { return MasterFx::safety(x); }
};

} // namespace kn
