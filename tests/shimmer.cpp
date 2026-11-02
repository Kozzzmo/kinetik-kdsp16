// SHIMMER reverb (MasterFx): stays stable at every MOD (no build-up, no clipping), the tail dies away, and MOD really
// adds the octave above.   g++ -O2 -std=c++17 tests/shimmer.cpp -o /tmp/t && /tmp/t
#include "../src/Params.hpp"
#include "../src/MasterFx.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
#include <memory>
using namespace kn;
static int fails = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; }
struct Run { std::vector<float> L; float peak = 0; };
// a 220 Hz tone (0.3) for `inSec` seconds into the reverb send; MOD < 0 = swept 0 -> 127 over 10 s with the tone held
static Run render(int mod, float inSec, float sec)
{
    const float sr = 48000; auto fx = std::make_unique<MasterFx>(); fx->init(sr);
    float g[kGlobalParams]; for (int i = 0; i < kGlobalParams; ++i) g[i] = globalDefault(i);
    g[gREV_MODEL] = (float)reverbModelValue(rmShimmer); g[gREV_SIZE] = 110; g[gREV_LEV] = 100; g[gREV_MOD] = (float)std::max(0, mod);
    const int N = 256; std::vector<float> z(N, 0.f), s(N), oL(N), oR(N); Run r;
    for (int p = 0; p < (int)(sr * sec); p += N) {
        if (mod < 0) g[gREV_MOD] = std::min(127.f, p / sr / 10.f * 127.f);
        for (int i = 0; i < N; ++i) { const float t = (p + i) / sr; s[i] = (t < inSec || mod < 0) ? 0.3f * std::sin(6.2831853f * 220.f * t) : 0.f; }
        fx->process(g, 120.f, z.data(), z.data(), z.data(), z.data(), s.data(), s.data(), z.data(), z.data(), z.data(), z.data(), p / sr * 2.0, true, oL.data(), oR.data(), N);
        for (int i = 0; i < N; ++i) { r.L.push_back(oL[i]); r.peak = std::max(r.peak, std::fabs(oL[i])); }
    }
    return r;
}
static double rmsDb(const std::vector<float>& x, float a, float b)
{
    double s = 0; const size_t i0 = (size_t)(a * 48000), i1 = std::min(x.size(), (size_t)(b * 48000));
    for (size_t i = i0; i < i1; ++i) s += x[i] * x[i];
    return 10 * std::log10(s / std::max<size_t>(1, i1 - i0) + 1e-20);
}
static double bandDb(const std::vector<float>& x, float a, float b, float hz)   // Goertzel, level at one frequency
{
    const size_t i0 = (size_t)(a * 48000), i1 = (size_t)(b * 48000); const double w = 2 * M_PI * hz / 48000, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0; for (size_t i = i0; i < i1; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return 10 * std::log10((s1 * s1 + s2 * s2 - c * s1 * s2) / double(i1 - i0) + 1e-20);
}
int main()
{
    bool stable = true, dies = true;
    for (int mod : {0, 40, 64, 80, 100, 127}) {
        const Run r = render(mod, 1.5f, 14.f);
        const double early = rmsDb(r.L, 1.0f, 2.0f), late = rmsDb(r.L, 12.f, 14.f), mid = rmsDb(r.L, 4.f, 6.f);
        std::printf("     MOD %3d: peak %.2f  1-2 s %.1f dB  4-6 s %.1f dB  12-14 s %.1f dB\n", mod, r.peak, early, mid, late);
        if (r.peak > 1.f || mid > early) stable = false;
        if (late > early - 50.0) dies = false;
    }
    check(stable, "every MOD: no build-up after the input stops, no clipping");
    check(dies, "the tail dies away (50 dB down within 12 s)");
    const Run sw = render(-1, 0.f, 12.f);
    double worst = -200; for (float t = 1.f; t < 12.f; t += 1.f) worst = std::max(worst, rmsDb(sw.L, t, t + 1.f) - rmsDb(sw.L, 1.f, 2.f));
    std::printf("     sweep: peak %.2f, loudest second %+.1f dB over the first\n", sw.peak, worst);
    check(sw.peak <= 1.f && worst < 3.0, "MOD swept past the middle with the tone held: bounded (it used to saturate)");
    const Run a = render(0, 1.5f, 5.f), b = render(110, 1.5f, 5.f);
    const double oa = bandDb(a.L, 1.6f, 3.f, 440.f) - bandDb(a.L, 1.6f, 3.f, 220.f), ob = bandDb(b.L, 1.6f, 3.f, 440.f) - bandDb(b.L, 1.6f, 3.f, 220.f);
    std::printf("     octave vs fundamental in the tail: MOD 0 %.1f dB, MOD 110 %.1f dB\n", oa, ob);
    check(ob > oa + 6.0, "MOD adds the octave above");
    std::printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
