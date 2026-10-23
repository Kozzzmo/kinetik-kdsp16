// KDSP-16 kick ATTACK: centre = unchanged, left softer, right punchier, on the three kick machines.
//   g++ -O2 -std=c++17 tests/kickattack.cpp -o /tmp/t && /tmp/t
#include "../src/Voice.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
using namespace kn;

static std::vector<float> hit(int m, float attack)
{
    Voice v; v.init(48000.f);
    v.setMachine(m);
    float p[kLockable] = {};
    for (int i = 0; i < 8; ++i) p[i] = kMachineDefs[m].def[i];
    for (int i = 8; i < kLockable; ++i) p[i] = kFxDefaults[i - 8];
    v.control(p);
    v.attack = attack;
    v.trigger(1.f);
    std::vector<float> o(4800);
    for (int i = 0; i < 4800; i += 64) v.render(o.data() + i, 64);
    return o;
}
static double energy(const std::vector<float>& x, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += x[i] * x[i]; return s; }

int main()
{
    int fails = 0;
    for (int m : {mKICK, mFM_KICK, mPH_KICK}) {
        const auto n = hit(m, 0.f), soft = hit(m, -1.f), punch = hit(m, 1.f);
        const double e0 = energy(n, 0, 240), es = energy(soft, 0, 240), ep = energy(punch, 0, 240);
        const double tail0 = energy(n, 2400, 4800), tailp = energy(punch, 2400, 4800);
        const bool ok = es < e0 * 0.7 && ep > e0 * 1.3 && std::fabs(tailp - tail0) < 1e-6 * (tail0 + 1e-9) + 1e-9;
        std::printf("%s %-8s first 5 ms: soft %.3g < centre %.3g < punch %.3g, tail unchanged\n", ok ? "ok  " : "FAIL", kMachineDefs[m].name, es, e0, ep);
        if (!ok) ++fails;
    }
    // other machines ignore it
    const auto a = hit(mSNARE, 0.f), b = hit(mSNARE, 1.f);
    const bool same = energy(a, 0, 4800) == energy(b, 0, 4800);
    std::printf("%s SNARE ignores ATTACK\n", same ? "ok  " : "FAIL");
    if (!same) ++fails;
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
