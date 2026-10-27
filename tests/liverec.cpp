// KDSP-16 live recording: MIDI notes while REC + playing are queued as trigs on the nearest step, the sequencer does not
// replay the hit right after; the metronome clicks on beats only while recording (and when enabled).
//   g++ -O2 -std=c++17 tests/liverec.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
#include <cmath>
#include <memory>
using namespace kn;

static std::unique_ptr<Engine> makeEngine(Bank& b)
{
    std::vector<float> v(kTotalParams); buildPreset(0, v.data(), b);   // Init: empty sequencer
    std::unique_ptr<Engine> e(new Engine()); e->init(48000);
    for (int t = 0; t < kTracks; t++) for (int p = 0; p < kTrackParams; p++) e->trackParam[t][p] = v[tpIndex(t, p)];
    for (int g = 0; g < gOUT_STEP; g++) e->global[g] = v[kGlobalBase + g];
    e->bank = b;
    e->global[gPLAY] = 1.f; e->global[gTEMPO] = 120.f;   // a 16th = 6000 frames
    return e;
}

int main()
{
    int fails = 0;
    auto check = [&](bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; };
    std::vector<float> L(64), R(64);
    {
        std::unique_ptr<Bank> b(new Bank()); auto e = makeEngine(*b);
        e->recording = true; e->click.enabled = false;
        // a note on track 3 (index 2) 900 frames before step 5 (index 4 = frame 24000), velocity 120 -> accent
        const int at = 24000 - 900;
        int pos = 0;
        for (; pos + 64 <= at; pos += 64) e->process(L.data(), R.data(), 64, Transport(), nullptr, 0);
        NoteEvent n{(uint32_t)(at - pos), 2, 120 / 100.f, 0};
        e->process(L.data(), R.data(), 64, Transport(), &n, 1); pos += 64;
        RecEvent r{};
        const bool got = e->popRec(r);
        check(got && r.pattern == 0 && r.track == 2 && r.step == 4 && r.accent, "MIDI note recorded on the nearest step (5) with accent");
        check(!e->popRec(r), "one event only");
        // the UI writes the trig at once: the sequencer must not play it again on this pass
        e->bank.patterns[0].flags[2][4] = sfTrig | sfAccent;
        const uint32_t before = e->trigCount[2];
        for (; pos < 48000; pos += 64) e->process(L.data(), R.data(), 64, Transport(), nullptr, 0);
        check(e->trigCount[2] == before, "the live hit is not doubled by the new trig");
        // next loop (16 steps = 96000 frames): the recorded trig plays
        for (; pos < 96000 + 30000; pos += 64) e->process(L.data(), R.data(), 64, Transport(), nullptr, 0);
        check(e->trigCount[2] == before + 1, "the recorded trig plays on the next loop");
    }
    {
        std::unique_ptr<Bank> b(new Bank()); auto e = makeEngine(*b);
        e->recording = false;
        NoteEvent n{10, 0, 1.f, 0};
        e->process(L.data(), R.data(), 64, Transport(), &n, 1);
        RecEvent r{};
        check(!e->popRec(r), "nothing recorded when REC is off");
    }
    // metronome: empty Init pattern, so the output is the click alone
    auto clickPeak = [&](bool rec, bool enabled, double& firstBeatPeak, double& otherPeak, double& offBeat) {
        std::unique_ptr<Bank> b(new Bank()); auto e = makeEngine(*b);
        e->recording = rec; e->click.enabled = enabled;
        firstBeatPeak = otherPeak = offBeat = 0;
        for (int pos = 0; pos < 48000 * 2; pos += 64) {    // 2 s = 4 beats at 120 BPM
            e->process(L.data(), R.data(), 64, Transport(), nullptr, 0);
            for (int i = 0; i < 64; ++i) {
                const int f = pos + i, inBeat = f % 24000;
                const double a = std::fabs(L[i]);
                if (inBeat < 2400) { if (f < 24000) firstBeatPeak = std::max(firstBeatPeak, a); else otherPeak = std::max(otherPeak, a); }
                else if (inBeat > 9000) offBeat = std::max(offBeat, a);
            }
        }
    };
    double p1, p2, off;
    clickPeak(true, true, p1, p2, off);
    check(p1 > 0.2 && p2 > 0.1 && p1 > p2 && off < 1e-3, "click on each beat while recording, accented on beat 1, silent between");
    std::printf("     peaks: bar %.3f, beat %.3f, between %.5f\n", p1, p2, off);
    clickPeak(true, false, p1, p2, off);
    check(p1 < 1e-6 && p2 < 1e-6, "CLICK OFF: silent");
    clickPeak(false, true, p1, p2, off);
    check(p1 < 1e-6 && p2 < 1e-6, "not recording: silent");
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
