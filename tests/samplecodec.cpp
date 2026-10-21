// Sample codec: lossless round trip, old format still read, damaged data refused, size.
// g++ -O2 -std=c++17 tests/samplecodec.cpp -o /tmp/t && /tmp/t
#include "../src/Sample.hpp"
#include <cstdio>
#include <random>
using namespace kn;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); ++fails; } } while (0)

static SampleData make(int kind, size_t n, std::mt19937& rng)
{
    SampleData s; s.sr = 44100; s.name = "test\tname"; s.pcm.resize(n);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    for (size_t i = 0; i < n; ++i) {
        const float t = (float)i / 44100.f;
        float v = 0.f;
        switch (kind) {
        case 0: v = 0.f; break;                                                        // silence
        case 1: v = u(rng); break;                                                     // full-scale noise
        case 2: v = std::exp(-t * 9.f) * std::sin(6.2831853f * 55.f * t * (1.f + 2.f * std::exp(-t * 30.f))); break; // kick
        case 3: v = std::exp(-t * 25.f) * (0.6f * u(rng) + 0.4f * std::sin(6.2831853f * 190.f * t)); break;            // snare
        case 4: v = (i & 1) ? 1.f : -1.f; break;                                       // worst case for prediction
        default: v = (i % 7 < 3) ? 1.2f : -3.f; break;                                 // clipped
        }
        s.pcm[i] = v;
    }
    return s;
}

int main()
{
    std::mt19937 rng(7);
    const char* names[] = {"silence", "noise", "kick", "snare", "alternating", "clipped"};
    for (int kind = 0; kind < 6; ++kind)
        for (size_t n : {1u, 2u, 3u, 4u, 4095u, 4096u, 4097u, 44100u}) {
            const SampleData s = make(kind, n, rng);
            const std::string enc = smp::encode(s);
            const SamplePtr d = smp::decode(enc.data(), enc.size(), true);
            CHECK(d && d->frames() == n, "%s n=%zu decode", names[kind], n);
            if (!d) continue;
            CHECK(d->name == "test name" && d->sr == 44100.0, "%s header", names[kind]);
            bool same = true;
            for (size_t i = 0; i < n; ++i) same &= smp::toPcm16(d->pcm[i]) == smp::toPcm16(s.pcm[i]);
            CHECK(same, "%s n=%zu not lossless", names[kind], n);
            // a second save gives the same text (no level drift from save to save)
            CHECK(smp::encode(*d) == enc, "%s n=%zu re-encode differs", names[kind], n);
            if (n == 44100u) {
                const std::string raw = smp::encodeRaw16(s);
                std::printf("%-12s 1 s: old %7zu bytes, new %7zu bytes (%.0f %%)\n", names[kind], raw.size(), enc.size(), 100.0 * enc.size() / raw.size());
                const SamplePtr o = smp::decode(raw.data(), raw.size(), false);
                CHECK(o && o->frames() == n, "%s old format", names[kind]);
            }
        }

    // decodeAll: old numeric lines and new z-lines, removal line
    {
        SampleData a = make(2, 1000, rng), b = make(3, 500, rng);
        const std::string text = "3 " + smp::encodeRaw16(a) + "\n" + smp::encodeLine(5, b) + "\n7\n";
        int got[16] = {}; size_t frames[16] = {};
        smp::decodeAll(text.c_str(), [&](int t, SamplePtr d) { if (t >= 0 && t < 16) { got[t]++; frames[t] = d ? d->frames() : 0; } });
        CHECK(got[3] == 1 && frames[3] == 1000, "old line");
        CHECK(got[5] == 1 && frames[5] == 500, "z line");
        CHECK(got[7] == 1 && frames[7] == 0, "removal line");
        // what versions before 0.10 do with a z-line: their parser wants a number first, so the line is skipped
        const std::string z = smp::encodeLine(2, b);
        char* end = nullptr; std::strtol(z.c_str(), &end, 10);
        CHECK(end == z.c_str(), "old parser would read the z-line");
    }

    // damaged packed data: never crashes, never returns a sample longer than the limit
    {
        const SampleData s = make(3, 20000, rng);
        const std::string enc = smp::encode(s);
        const size_t t2 = enc.find('\t', enc.find('\t') + 1) + 1;
        std::uniform_int_distribution<int> pick(0, 63);
        static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        int accepted = 0;
        for (int it = 0; it < 20000; ++it) {
            std::string m = enc;
            const int edits = 1 + it % 6;
            for (int e = 0; e < edits; ++e) m[t2 + rng() % (m.size() - t2)] = T[pick(rng)];
            if (it % 5 == 0) m.resize(t2 + rng() % (m.size() - t2));
            const SamplePtr d = smp::decode(m.data(), m.size(), true);
            if (d) { ++accepted; CHECK(d->frames() <= smp::kMaxPackFrames, "too long"); for (float v : d->pcm) if (!(v >= -1.01f && v <= 1.01f)) { CHECK(false, "out of range"); break; } }
        }
        // random bytes as a packed stream
        for (int it = 0; it < 20000; ++it) {
            std::vector<uint8_t> b(rng() % 64);
            for (auto& c : b) c = (uint8_t)rng();
            std::vector<int32_t> x;
            if (smp::unpack(b.data(), b.size(), x)) CHECK(x.size() <= b.size() * 8, "random stream too long");
        }
        std::printf("damaged inputs: %d of 20000 still decode (as some other sound), none crashed\n", accepted);
    }
    if (fails) { std::printf("%d failures\n", fails); return 1; }
    std::printf("all passed\n");
    return 0;
}
