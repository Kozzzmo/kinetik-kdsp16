// Kinetik KDSP-16 — DPF plugin glue
#include "DistrhoPlugin.hpp"
#include "Engine.hpp"
#include "License.hpp"
#include <atomic>
#include <cmath>
#include <mutex>
#include <memory>

START_NAMESPACE_DISTRHO

using namespace kn;

static String paramSymbol(int track, int p)
{
    static const char* fx[32] = {"amd","amf","eqf","eqg","cutoff","ftype","reso","srr","dist","vol","pan","del","rev",
                                 "fenv","fdec","vel","lfos","lfod","lfosh","lfot","lfodst","lfotrk",
                                 "lfo2s","lfo2d","lfo2sh","lfo2t","lfo2dst","lfo2trk",
                                 "lfosync","lfophase","lfo2sync","lfo2phase"};
    static_assert(sizeof(fx) / sizeof(fx[0]) == kTrackParams - 8, "symbol table must cover every track parameter");
    char buf[32];
    if (p < 8) std::snprintf(buf, sizeof(buf), "t%02d_syn%d", track + 1, p + 1);
    else std::snprintf(buf, sizeof(buf), "t%02d_%s", track + 1, fx[p - 8]);
    return String(buf);
}

static constexpr uint32_t kKitBit = 1u << 16;

class KinetikPlugin : public Plugin
{
public:
    KinetikPlugin()
        : Plugin(kTotalParams, 0, 5)
    {
        engine.reset(new Engine());
        master.reset(new Bank());
        master->initDefault();
        engine->bank = *master;
        engine->init(getSampleRate());
        for (int i = 0; i < kTotalParams; ++i) { uiVal[i].store(0.f); uiHold[i].store(0); }
        notes.reserve(256);
        licensed.store(lic::findInstalled(kProductCode).valid);
    }

protected:
    const char* getLabel() const override { return "KinetikKDSP16"; }
    const char* getDescription() const override { return "16-track drum synthesizer with step sequencer and parameter locks"; }
    const char* getMaker() const override { return "Kinetik"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(KN_VER_MAJOR, KN_VER_MINOR, KN_VER_MICRO); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        p.hints = kParameterIsAutomatable;
        if ((int)index < kGlobalBase) {
            const int t = (int)index / kTrackParams, k = (int)index % kTrackParams;
            char name[48];
            p.symbol = paramSymbol(t, k);
            p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = trackParamDefault(t, k);
            if (k < 8) std::snprintf(name, sizeof(name), "T%02d Synth %d", t + 1, k + 1);
            else if (k >= tpLFOX) std::snprintf(name, sizeof(name), "T%02d LFO%d %s", t + 1, (k - tpLFOX) / 2 + 1, kFxNames[k - 8]);
            else if (k >= tpLFO2S) std::snprintf(name, sizeof(name), "T%02d LFO2 %s", t + 1, kFxNames[k - 8]);
            else if (k >= tpLFOS) std::snprintf(name, sizeof(name), "T%02d LFO1 %s", t + 1, kFxNames[k - 8]);
            else std::snprintf(name, sizeof(name), "T%02d %s", t + 1, kFxNames[k - 8]);
            p.name = name;
            p.shortName = name;
            return;
        }
        const int g = (int)index - kGlobalBase;
        switch (g) {
        case gPATTERN:
            p.name = "Pattern"; p.symbol = "pattern"; p.ranges.min = 0; p.ranges.max = kPatterns - 1; p.ranges.def = 0;
            p.hints |= kParameterIsInteger; return;
        case gPLAY:
            p.name = "Internal Play"; p.symbol = "play"; p.ranges.min = 0; p.ranges.max = 1; p.ranges.def = 0;
            p.hints |= kParameterIsBoolean | kParameterIsInteger; return;
        case gTEMPO:
            p.name = "Internal Tempo"; p.symbol = "tempo"; p.ranges.min = 40; p.ranges.max = 240; p.ranges.def = 120;
            p.unit = "BPM"; return;
        case gOUT_STEP:
            p.name = "Play Step"; p.symbol = "out_step"; p.hints = kParameterIsOutput | kParameterIsInteger;
            p.ranges.min = -1; p.ranges.max = 1048575; p.ranges.def = -1; return;
        case gOUT_LEDS:
            p.name = "Trig LEDs"; p.symbol = "out_leds"; p.hints = kParameterIsOutput | kParameterIsInteger;
            p.ranges.min = 0; p.ranges.max = 65535; p.ranges.def = 0; return;
        case gOUT_PATTERN:
            p.name = "Playing Pattern"; p.symbol = "out_pattern"; p.hints = kParameterIsOutput | kParameterIsInteger;
            p.ranges.min = 0; p.ranges.max = kPatterns - 1; p.ranges.def = 0; return;
        default: break;
        }
        if (g >= gCHO_SEND && g < gCHO_SEND + kTracks) {   // chorus send of each track
            static char names[kTracks][32], symbols[kTracks][16];
            const int t = g - gCHO_SEND;
            std::snprintf(names[t], sizeof(names[t]), "T%02d Chorus Send", t + 1);
            std::snprintf(symbols[t], sizeof(symbols[t]), "t%02d_cho", t + 1);
            p.name = names[t]; p.symbol = symbols[t];
            p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = 0;
            return;
        }
        static const char* section[] = {"Echo","Echo","Echo","Echo","Echo","Echo","Reverb","Reverb","Reverb","Reverb",
                                          "Reverb","Reverb","Master","Master","Comp","Comp","Comp","Comp","Master","Master",
                                          "Echo","Echo","Reverb","Reverb","Filter","Filter","Filter",
                                          "Chorus","Chorus","Chorus","Chorus","Chorus","Chorus","Chorus","Chorus","Glitch","Glitch","Glitch","Glitch","Glitch","Glitch","Glitch"};
        static const char* syms[] = {"echo_time","echo_fb","echo_lpf","echo_hpf","echo_width","echo_lev",
                                       "rev_decay","rev_damp","rev_gate","rev_hpf","rev_pre","rev_lev",
                                       "eq_low","eq_high","cmp_thr","cmp_ratio","cmp_atk","cmp_rel","drive","volume",
                                       "echo_model","echo_mod","rev_model","rev_mod","mfilter_hp","mfilter_lp","mfilter_reso",
                                       "cho_model","cho_rate","cho_depth","cho_time","cho_fb","cho_width","cho_tone","cho_lev","gl_chance","gl_size","gl_len","gl_pitch","gl_fade","gl_rev","gl_mix"};
        static_assert(sizeof(section) / sizeof(section[0]) == kMasterCount && sizeof(syms) / sizeof(syms[0]) == kMasterCount, "master tables");
        const int i = g - gECHO_TIME;
        if (i >= 0 && i < kMasterCount) {
            char name[48];
            std::snprintf(name, sizeof(name), "%s %s", section[i], kMasterNames[i]);
            p.name = name; p.symbol = syms[i];
            p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = kMasterDefaults[i];
        }
    }

    void initState(uint32_t index, State& state) override
    {
        if (index == 0) {
            state.key = "bank";
            state.defaultValue = String(serializeBank(*master).c_str());
            state.label = "Pattern bank";
        } else if (index == 1) {
            state.key = "cmd";
            state.defaultValue = "";
            state.label = "Edit command";
        } else if (index == 2) {
            state.key = "skin";          // interface skin, saved with the project
            state.defaultValue = "";
            state.label = "Interface skin";
        } else if (index == 3) {
            state.key = "samples";       // every track's SAMPLE machine data, saved with the project
            state.defaultValue = "";
            state.label = "Samples";
        } else {
            state.key = "smp";           // the UI loaded (or removed) one track's sample
            state.defaultValue = "";
            state.label = "Sample command";
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        if ((int)index < kGlobalBase) return engine->trackParam[index / kTrackParams][index % kTrackParams];
        const int g = (int)index - kGlobalBase;
        switch (g) {
        case gOUT_STEP: return (float)engine->outStep;
        case gOUT_LEDS: return (float)engine->outLeds;
        case gOUT_PATTERN: return (float)engine->outPattern;
        default: return g < kGlobalParams ? engine->global[g] : 0.f;
        }
    }

    // ---- UI edits win over stale host values
    // The UI sends every parameter it changes twice: as a host parameter and as a "pv <index> <value>" line of the
    // cmd state. Some hosts (Ableton Live) send the processor the value a parameter had before a UI edit, late, or
    // lose part of a bulk edit (randomize, undo, preset): for 1.5 s after a UI edit, host values that disagree with
    // it are ignored, and the "pv" line applies the value even if the host never does.
    std::atomic<float> uiVal[kTotalParams];
    std::atomic<int32_t> uiHold[kTotalParams];   // samples left
    void uiEdit(uint32_t index, float value)
    {
        if (index >= (uint32_t)kTotalParams) return;
        uiVal[index].store(value);
        uiHold[index].store((int32_t)(getSampleRate() * 1.5));
        applyParameter(index, value);
    }
    void setParameterValue(uint32_t index, float value) override
    {
        if (index < (uint32_t)kTotalParams && uiHold[index].load(std::memory_order_relaxed) > 0) {
            if (std::fabs(value - uiVal[index].load(std::memory_order_relaxed)) > 0.01f) return;   // stale host value
            uiHold[index].store(0);                                                                   // host caught up
        }
        applyParameter(index, value);
    }
    void applyParameter(uint32_t index, float value)
    {
        if ((int)index < kGlobalBase) { engine->trackParam[index / kTrackParams][index % kTrackParams] = value; return; }
        const int g = (int)index - kGlobalBase;
        if (g < 0 || g >= gOUT_STEP) return;
        engine->global[g] = value;
    }

    String getState(const char* key) const override
    {
        if (std::strcmp(key, "bank") == 0) {
            std::lock_guard<std::mutex> lk(masterMutex);
            return String(serializeBank(*master).c_str());
        }
        if (std::strcmp(key, "skin") == 0) return skinState;
        if (std::strcmp(key, "samples") == 0) {
            std::lock_guard<std::mutex> lk(masterMutex);
            SamplePtr cur[kTracks];
            for (int t = 0; t < kTracks; ++t) cur[t] = smpPend[t] ? smpPend[t] : smpLive[t];
            return String(smp::encodeAll(cur, kTracks).c_str());
        }
        return String("");
    }

    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "skin") == 0) { skinState = value ? value : ""; return; }
        if (std::strcmp(key, "samples") == 0 || std::strcmp(key, "smp") == 0) {
            const bool all = key[1] == 'a';
            if (!all && !(value && *value)) return;
            std::lock_guard<std::mutex> lk(masterMutex);
            for (int t = 0; t < kTracks; ++t) smpGrave[t].reset();   // free what the audio thread released
            if (all) for (int t = 0; t < kTracks; ++t) { smpPend[t].reset(); smpSet[t] = true; }   // a full set replaces everything
            smp::decodeAll(value ? value : "", [this](int t, SamplePtr d) {
                if (t < 0 || t >= kTracks) return;
                smpPend[t] = std::move(d); smpSet[t] = true;
            });
            smpDirty.store(true);
            return;
        }
        if (std::strcmp(key, "bank") == 0) {
            std::lock_guard<std::mutex> lk(masterMutex);
            if (deserializeBank(*master, value)) dirty.store(0xFFFFu | kKitBit);
            return;
        }
        if (std::strcmp(key, "cmd") == 0 && value && *value) {
            std::lock_guard<std::mutex> lk(masterMutex);
            uint32_t d = 0;
            const char* s = value;
            std::string line;
            while (*s) {
                const char* e = std::strchr(s, '\n');
                const size_t len = e ? (size_t)(e - s) : std::strlen(s);
                line.assign(s, len);
                if (line.rfind("pv ", 0) == 0) {                     // UI parameter edit (see uiEdit)
                    unsigned i = 0; float v = 0.f;
                    if (std::sscanf(line.c_str() + 3, "%u %f", &i, &v) == 2) uiEdit(i, v);
                } else if (line == "lic") {                    // the UI installed a licence: check again
                    licensed.store(lic::findInstalled(kProductCode).valid);
                } else if (line.rfind("gh ", 0) == 0) {                  // glitch HOLD button
                    engine->setGlitchHold(std::atoi(line.c_str() + 3) != 0);
                } else if (line.rfind("fill ", 0) == 0) {
                    fillFlag.store(std::atoi(line.c_str() + 5) != 0);
                } else if (line.rfind("aud ", 0) == 0) {
                    const int t = std::atoi(line.c_str() + 4);
                    if (t >= 0 && t < kTracks) auditionMask.fetch_or(1u << t);
                } else {
                    const int p = applyCommand(*master, line.c_str());
                    if (p == kKitTouched) d |= kKitBit;
                    else if (p >= 0) d |= 1u << p;
                }
                s += len; if (*s == '\n') ++s;
            }
            if (d) dirty.fetch_or(d);
        }
    }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input) return;
        static const char* names[8] = {"Main L", "Main R", "Out 2 L", "Out 2 R", "Out 3 L", "Out 3 R", "Out 4 L", "Out 4 R"};
        static const char* syms[8] = {"main_l", "main_r", "out2_l", "out2_r", "out3_l", "out3_r", "out4_l", "out4_r"};
        port.name = names[index];
        port.symbol = syms[index];
        port.groupId = index < 2 ? kPortGroupStereo : 1 + (index - 2) / 2;
    }

    void initPortGroup(uint32_t groupId, PortGroup& group) override
    {
        static const char* names[4] = {"", "Out 2", "Out 3", "Out 4"};
        static const char* syms[4] = {"", "out2", "out3", "out4"};
        if (groupId >= 1 && groupId <= 3) { group.name = names[groupId]; group.symbol = syms[groupId]; }
    }

    void activate() override { engine->reset(); }

    void sampleRateChanged(double sr) override { engine->init(sr); }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        for (int i = 0; i < kTotalParams; ++i) {
            const int32_t h = uiHold[i].load(std::memory_order_relaxed);
            if (h > 0) uiHold[i].store(h > (int32_t)frames ? h - (int32_t)frames : 0, std::memory_order_relaxed);
        }
        // pull pattern edits from the UI/host thread without ever blocking
        if (dirty.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk(masterMutex, std::try_to_lock);
            if (lk.owns_lock()) {
                const uint32_t d = dirty.exchange(0);
                for (int p = 0; p < kPatterns; ++p)
                    if (d & (1u << p)) engine->bank.patterns[p] = master->patterns[p];
                if (d & kKitBit) {
                    std::memcpy(engine->bank.machine, master->machine, sizeof(master->machine));
                    std::memcpy(engine->bank.mute, master->mute, sizeof(master->mute)); std::memcpy(engine->bank.solo, master->solo, sizeof(master->solo));
                    std::memcpy(engine->bank.kit, master->kit, sizeof(master->kit));
                }
            }
        }
        // install new samples (the old ones are freed later, outside the audio thread)
        if (smpDirty.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk(masterMutex, std::try_to_lock);
            if (lk.owns_lock()) {
                bool left = false;
                for (int t = 0; t < kTracks; ++t) {
                    if (!smpSet[t]) continue;
                    if (smpGrave[t]) { left = true; continue; }        // wait until the previous one is freed
                    smpGrave[t] = std::move(smpLive[t]);
                    smpLive[t] = std::move(smpPend[t]);
                    smpSet[t] = false;
                    engine->samples[t] = smpLive[t].get();
                }
                smpDirty.store(left);
            }
        }
        engine->fill = fillFlag.load();
        const uint32_t aud = auditionMask.exchange(0);
        for (int t = 0; t < kTracks; ++t) if (aud & (1u << t)) engine->audition(t, 1.f);

        Transport tp;
        const TimePosition& pos = getTimePosition();
        if (pos.bbt.valid) {
            tp.hostValid = true;
            tp.hostPlaying = pos.playing;
            const double bt = pos.bbt.beatType > 0 ? pos.bbt.beatType : 4.0;
            const double beatsFromStart = (pos.bbt.bar - 1) * (double)pos.bbt.beatsPerBar + (pos.bbt.beat - 1)
                                        + (pos.bbt.ticksPerBeat > 0 ? pos.bbt.tick / pos.bbt.ticksPerBeat : 0.0);
            tp.beat = beatsFromStart * (4.0 / bt);   // in quarter notes
            tp.bpm = pos.bbt.beatsPerMinute > 0 ? pos.bbt.beatsPerMinute : 120.0;
        }

        notes.clear();
        for (uint32_t i = 0; i < midiCount && notes.size() < 256; ++i) {
            const MidiEvent& ev = midi[i];
            if (ev.size < 3) continue;
            const uint8_t st = ev.data[0] & 0xF0;
            if (st == 0x90 && ev.data[2] > 0) {
                const int ch = (ev.data[0] & 0x0F) + 1;
                // tracks listening to this MIDI channel play chromatically (C3 = 60 = original pitch)
                bool melodic = false;
                for (int t = 0; t < kTracks && notes.size() < 256; ++t)
                    if (engine->bank.kit[t][kfMidi] == ch) {
                        notes.push_back({ev.frame, t, ev.data[2] / 100.f, (int)ev.data[1] - 60});
                        melodic = true;
                    }
                if (melodic) continue;
                // otherwise: drum map, notes 36..51 = tracks 1..16
                const int tr = (int)ev.data[1] - 36;
                if (tr >= 0 && tr < kTracks)
                    notes.push_back({ev.frame, tr, ev.data[2] / 100.f, 0});
            }
        }
        engine->process(outputs, frames, tp, notes.data(), (uint32_t)notes.size());
        if (lic::kDemoModeEnabled && !licensed.load(std::memory_order_relaxed)) demoGate(outputs, frames);
    }

    // Demo mode (no licence installed): the sound drops out for 3 seconds every 45 seconds.
    void demoGate(float** outputs, uint32_t frames)
    {
        const double sr = getSampleRate();
        const uint64_t period = (uint64_t)(45.0 * sr), mute = (uint64_t)(3.0 * sr);
        const float coef = 1.f - std::exp(-1.f / (0.01f * (float)sr));
        for (uint32_t i = 0; i < frames; ++i) {
            const float target = (demoPos % period) >= period - mute ? 0.f : 1.f;
            demoGain += (target - demoGain) * coef;
            ++demoPos;
            if (demoGain < 0.9999f)
                for (uint32_t c = 0; c < DISTRHO_PLUGIN_NUM_OUTPUTS; ++c) outputs[c][i] *= demoGain;
        }
    }

private:
    std::unique_ptr<Engine> engine;
    std::unique_ptr<Bank> master;
    mutable std::mutex masterMutex;
    std::atomic<uint32_t> dirty{0};
    std::atomic<uint32_t> auditionMask{0};
    std::atomic<bool> fillFlag{false};
    std::atomic<bool> licensed{false};
    uint64_t demoPos = 0; float demoGain = 1.f;
    static constexpr const char* kProductCode = "KDSP-16";
    String skinState;
    // SAMPLE machine data: pending (from setState), live (used by the engine), grave (freed later)
    SamplePtr smpPend[kTracks], smpLive[kTracks], smpGrave[kTracks];
    bool smpSet[kTracks] = {};
    std::atomic<bool> smpDirty{false};
    std::vector<NoteEvent> notes;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KinetikPlugin)
};

Plugin* createPlugin() { return new KinetikPlugin(); }

END_NAMESPACE_DISTRHO
