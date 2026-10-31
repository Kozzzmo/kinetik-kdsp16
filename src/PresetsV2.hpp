// Kinetik KDSP-16 — factory presets, second generation (0.20). All original material.
// Each preset is a full piece: 2 to 4 patterns of 32 or 64 steps (variations in the last bar, fills on FILL,
// probabilities, ratchets, micro-timing, per-step locks, LFOs, effects), a CHAIN when the patterns tell a story,
// and track levels balanced with the mix checker (kick about 2 LU under the mix, snare / clap 6-8, hats 13-17).
// Included by Presets.hpp after the PB builder.
#pragma once


// ---- builder helpers used by the presets below
inline std::string kBars(std::initializer_list<const char*> bs) { std::string s; for (auto x : bs) s += x; return s; }
inline std::string kRep(const char* bar, int n) { std::string s; for (int i = 0; i < n; ++i) s += bar; return s; }
struct PB2 {
    PB& p;
    explicit PB2(PB& b) : p(b) {}
    void row(int pat, int t, const std::string& s) { p.row(pat, t, s.c_str()); }
    // a per-step value cycling over the trigs of a track (any lockable param)
    void cyc(int pat, int t, int prm, std::initializer_list<int> vals)
    {
        const int n = (int)vals.size(); int i = 0; const int* a = vals.begin();
        for (int s = 0; s < kSteps; ++s) if (p.b.patterns[pat].flags[t][s] & sfTrig) p.lock(pat, t, s, prm, a[i++ % n]);
    }
    void cond(int pat, int t, std::initializer_list<int> steps, int c) { for (int s : steps) p.cond(pat, t, s, c); }
    void micro(int pat, int t, std::initializer_list<int> steps, int m) { for (int s : steps) p.micro(pat, t, s, m); }
    // LFO l of track t: synced division (index into kLfoSyncNames) or free rate, depth, shape (TRI SINE SQR SAW+ SAW- RND),
    // mode (FREE RETRIG HOLD), destination param (0..23)
    void lfo(int t, int l, int div, int depth, int shape, int mode, int dest, bool sync = true)
    {
        p.fx(t, lfoParam(l, 0), sync ? div * 8 + 4 : div);
        p.fx(t, lfoParam(l, 1), depth);
        p.fx(t, lfoParam(l, 2), shape * 128 / 6 + 10);
        p.fx(t, lfoParam(l, 3), mode * 43 + 10);
        p.fx(t, lfoParam(l, 4), lfoDestValue(dest));
        p.fx(t, lfoParam(l, 6), sync ? 100 : 0);
    }
    void copy(int src, int dst) { p.b.patterns[dst] = p.b.patterns[src]; }
    void chain(const char* spec) { p.b.chain.parse(spec, kPatterns); }
    void vol(std::initializer_list<std::pair<int, int>> tv) { for (auto& x : tv) p.fx(x.first, tpVOL, x.second); }
    // level correction per track in dB (from the mix checker): scales the VOL knob and the VOL locks of every pattern
    void mix(std::initializer_list<float> db)
    {
        int t = 0;
        for (float d : db) {
            const float k = std::pow(10.f, d / 40.f);
            float& v = tparam(p.v, t, tpVOL); v = std::min(127.f, std::round(v * k));
            for (auto& P : p.b.patterns) for (int s = 0; s < kSteps; ++s) { uint8_t& l = P.locks[t][s][tpVOL]; if (l != kNoLock) l = (uint8_t)std::min(127.f, std::round(l * k)); }
            if (++t >= kTracks) break;
        }
    }
    void pan(std::initializer_list<std::pair<int, int>> tv) { for (auto& x : tv) p.fx(x.first, tpPAN, x.second); }
    void send(int t, int del, int rev, int cho = 0) { p.fx(t, tpDEL, del); p.fx(t, tpREV, rev); p.g(gCHO_SEND + t, cho); }
    void chaos(int t, int amount) { p.g(gCHA_SEND + t, amount); }
    void filt(int t, int type, int cut, int reso) { p.fx(t, tpFTYPE, filterTypeValue(type)); p.fx(t, tpCUTOFF, cut); p.fx(t, tpRESO, reso); }
    void echo(int time, int fb, int model, int mod, int lev) { p.g(gECHO_TIME, time); p.g(gECHO_FB, fb); p.g(gECHO_MODEL, model); p.g(gECHO_MOD, mod); p.g(gECHO_LEV, lev); }
    void verb(int size, int damp, int model, int mod, int lev, int pre = 10) { p.g(gREV_SIZE, size); p.g(gREV_DAMP, damp); p.g(gREV_MODEL, model); p.g(gREV_MOD, mod); p.g(gREV_LEV, lev); p.g(gREV_PRE, pre); }
    void comp(int thr, int ratio) { p.g(gCMP_THR, thr); p.g(gCMP_RAT, ratio); }
    void pattern(int pat, int len, int swing, int accent = 70) { p.settings(pat, len, swing, 0, accent); }
};
// echo MODEL values (8 bins of 16): DIGITAL 0, TAPE 16, MULTI-TAP 32, BBD 48, PING-PONG 64, REVERSE 80, LO-FI 96, DIFFUSE 112
enum { emvDigital = 4, emvTape = 20, emvMulti = 36, emvBbd = 52, emvPing = 68, emvReverse = 84, emvLofi = 100, emvDiffuse = 116 };
// reverb MODEL values: ROOM PLATE HALL SPRING SHIMMER
enum { rmvRoom = 10, rmvPlate = 36, rmvHall = 62, rmvSpring = 88, rmvShimmer = 115 };
// echo TIME values for the divisions 1/16, 1/16D, 1/8, 1/8D, 1/4
enum { etv16 = 35, etv16D = 56, etv8 = 67, etv8D = 88, etv4 = 99 };

// ======================================================================== TECHNO
// Warehouse 132: rolling peak-time techno. A: 64 steps, B: the kick drops for a bar of rumble, C: fill bar.
static void v2Warehouse(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {48, 66, 55, 26, 72, 0, 8, 38}); b.g(gKATK + 0, 88);
    b.kit(1, mSINE, {24, 70, 20, 30, 40, 0, 30, 70}); q.filt(1, 1, 52, 30); // rumble
    b.kit(2, mCLAP, {66, 48, 52, 56, 30, 70, 25, 18});
    b.kit(3, mCLHAT, {72, 16, 92, 92, 30, 60, 0, 0});
    b.kit(4, mOPHAT, {70, 46, 88, 90, 30, 60, 0, 0}); b.choke(3, 1); b.choke(4, 1);
    b.kit(5, mFM_CYMBAL, {70, 72, 70, 30, 85, 88, 70, 72});
    b.kit(6, mPH_METAL, {58, 52, 82, 70, 40, 60, 64, 10});
    b.kit(7, mRIM, {62, 28, 50, 85, 30, 18, 50, 30});
    b.kit(8, mNOISE, {40, 30, 80, 40, 100, 40, 0, 0}); q.filt(8, 2, 60, 70);
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kRep("..x...x...x...xx", 4)); q.cyc(0, 1, 0, {24, 24, 24, 27});
    q.row(0, 2, kBars({"....x.......x...", "....x.......x...", "....x.......x...", "....x.......x.xx"}));
    q.row(0, 3, kRep("x.xxx.xxx.xxx.xx", 4)); q.cyc(0, 3, 2, {92, 80, 100, 86}); q.cond(0, 3, {3, 19, 35, 51}, 3);
    q.row(0, 4, kRep("..x...x...x...x.", 4));
    q.row(0, 5, kRep("x.x.x.x.x.x.x.x.", 4)); q.cyc(0, 5, tpVOL, {90, 60, 80, 60});
    q.row(0, 6, kBars({"......x.........", "...x......x.....", "......x.........", "...x....x.x...x."})); q.cyc(0, 6, 0, {58, 64, 51, 58, 70, 64});
    q.row(0, 7, kBars({".......x......x.", "...x...x........", ".......x......x.", "...x...x..x.x.x."}));
    q.row(0, 8, kBars({"................", "................", "................", "x..............."}));
    q.micro(0, 3, {2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 62}, 2);
    q.pattern(0, 64, 54);
    // B: breakdown, kick out, rumble and the noise rise
    q.copy(0, 1);
    q.row(1, 0, kBars({"x...x...x...x...", "x...x...x...x...", "................", "................"}));
    q.row(1, 8, kBars({"................", "................", "x...............", "................"}));
    b.lock(1, 8, 32, tpCUTOFF, 40);
    q.pattern(1, 64, 54);
    // C: fill (one bar)
    q.row(2, 0, "x...x...x...x.x.");
    q.row(2, 1, "..x...x...x...xx");
    q.row(2, 2, "....x.......x.xx"); b.ratch(2, 2, 14, 3);
    q.row(2, 3, "xxxxxxxxxxxxxxxx"); q.cyc(2, 3, tpVOL, {70, 80, 90, 100});
    q.row(2, 7, "x.x.x.x.xxxxxxxx"); q.cyc(2, 7, 0, {50, 54, 58, 62, 66, 70, 74, 78});
    q.pattern(2, 16, 54);
    q.lfo(5, 0, 3, 40, 1, 0, tpCUTOFF);              // ride opens and closes over 2 bars
    q.lfo(1, 0, 4, 30, 0, 1, 12);
    q.send(1, 45, 0); q.send(2, 0, 28); q.send(6, 40, 30); q.send(7, 30, 10); q.send(8, 30, 50);
    q.echo(etv8D, 62, emvDiffuse, 70, 70); q.verb(55, 60, rmvPlate, 40, 60);
    q.comp(105, 50); b.g(gDRIVE, 18);
    q.chain("1 4 0:3 1:1 0:2 2:1");
    b.g(gTEMPO, 132);
    q.vol({{0, 104}, {1, 92}, {2, 94}, {3, 74}, {4, 70}, {5, 64}, {6, 78}, {7, 72}, {8, 72}});
    q.mix({1.7f, -6.2f, -0.1f, 19.0f, 3.9f, -6.7f, 1.0f, 25.3f, 3.8f});
}

// Hard Groove 136: inspired by the owner's "Hard Groove": FM kick, snare rolls over 64 steps, hats in 32, metal hits.
static void v2HardGroove(PB& b)
{
    PB2 q(b);
    b.kit(0, mFM_KICK, {50, 62, 60, 24, 34, 30, 20, 10}); b.g(gKATK + 0, 80);
    b.kit(1, mSNARE, {56, 34, 30, 18, 88, 74, 40, 40});
    b.kit(2, mCLAP, {72, 40, 50, 60, 30, 60, 30, 30});
    b.kit(3, mCLHAT, {78, 14, 95, 95, 30, 60, 10, 20});
    b.kit(4, mOPHAT, {74, 40, 92, 92, 30, 60, 0, 10}); b.choke(3, 1); b.choke(4, 1);
    b.kit(5, mFM_HAT, {84, 24, 20, 70, 90, 100, 50, 90});
    b.kit(6, mPH_METAL, {64, 40, 90, 80, 30, 60, 70, 30});
    b.kit(7, mTOM, {44, 50, 50, 30, 50, 10, 10, 30});
    b.kit(8, mNOISE, {0, 10, 30, 70, 120, 20, 30, 20});
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kBars({"....x.......x...", "....x.......x..x", "....x.......x...", "....x...x.x.xxxx"}));
    q.cyc(0, 1, 0, {56, 56, 56, 56, 56, 56, 56, 58, 60, 62, 64, 66, 68});
    q.row(0, 2, kRep("....x.......x...", 4));
    q.row(0, 3, kRep("xxxxxxxxxxxxxxxx", 4)); q.cyc(0, 3, tpVOL, {100, 60, 84, 60}); q.cond(0, 3, {7, 23, 39, 55}, 2);
    q.row(0, 4, kRep("..x...x...x...x.", 4));
    q.row(0, 5, kRep("x..x..x..x..x..x..x..x..x..x..x.", 2));   // 3-against-4 FM hats
    q.row(0, 6, kBars({"..........x.....", "...x............", "..........x.....", "...x......x...x."})); q.cyc(0, 6, 0, {64, 70, 58, 76});
    q.row(0, 7, kBars({"................", "................", "................", "........x.x.x..."})); q.cyc(0, 7, 0, {60, 50, 40});
    q.row(0, 8, kRep("..x...x...x...x.", 4)); q.cyc(0, 8, tpCUTOFF, {90, 100, 110, 120});
    q.micro(0, 3, {1, 5, 9, 13}, 3);
    q.pattern(0, 64, 57);
    q.copy(0, 1);
    q.row(1, 1, kBars({"....x.......x...", "....x.......x...", "....x...x...x...", "x.x.x.x.xxxxxxxx"})); q.cyc(1, 1, 0, {56, 56, 56, 56, 56, 56, 58, 58, 60, 60, 62, 64, 66, 68, 70, 72, 74, 76});
    q.row(1, 6, kRep("x..x..x..x..x..x", 4)); q.cyc(1, 6, 0, {64, 66, 68, 70, 72, 74, 76});
    q.pattern(1, 64, 57);
    q.lfo(8, 0, 4, 60, 3, 0, tpCUTOFF); q.filt(8, 3, 70, 30);
    q.send(2, 10, 30); q.send(6, 50, 35); q.send(7, 20, 20); q.send(5, 30, 0);
    q.echo(etv16D, 48, emvBbd, 40, 60); q.verb(45, 70, rmvRoom, 50, 55);
    q.comp(100, 60); b.fx(0, tpDIST, 25); b.g(gDRIVE, 25);
    q.chain("1 2 0:3 1:1");
    b.g(gTEMPO, 136);
    q.vol({{0, 104}, {1, 82}, {2, 80}, {3, 78}, {4, 72}, {5, 66}, {6, 74}, {7, 84}, {8, 62}});
    q.mix({-5.2f, 3.2f, -6.4f, -6.3f, -7.3f, 4.6f, -5.9f, -13.8f, -12.3f});
}

// Basik Teck 125: inspired by the owner's "Basik Teck": a stripped tool, kick + clap + hats, a delayed tom figure.
static void v2BasikTeck(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {50, 60, 50, 30, 60, 0, 0, 22});
    b.kit(1, mCLAP, {64, 46, 50, 58, 28, 72, 22, 12});
    b.kit(2, mCLHAT, {70, 18, 90, 88, 30, 60, 0, 0});
    b.kit(3, mOPHAT, {70, 44, 86, 88, 30, 60, 0, 0}); b.choke(2, 1); b.choke(3, 1);
    b.kit(4, mTOM, {58, 40, 40, 30, 30, 10, 10, 10});
    b.kit(5, mFM_BELL, {64, 40, 40, 10, 50, 70, 50, 64});
    b.kit(6, mSHAKER, {10, 16, 30, 80, 112, 40, 64, 0});
    q.row(0, 0, kRep("x...x...x...x...", 4)); b.lock(0, 0, 60, tpCUTOFF, 70);
    q.row(0, 1, kRep("....x.......x...", 4));
    q.row(0, 2, kRep("..x...x...x...x.", 4));
    q.row(0, 3, kBars({"......x.........", "......x.........", "......x.........", "......x.....x..."}));
    q.row(0, 4, kRep("...x.....x..x...", 4)); q.cyc(0, 4, 0, {58, 52, 64}); q.cond(0, 4, {12, 44}, 13);
    q.row(0, 5, kBars({"................", "..........x.....", "................", "..x.......x....."})); q.cyc(0, 5, 0, {64, 60});
    q.row(0, 6, kRep("x.xxx.xxx.xxx.xx", 4)); q.cyc(0, 6, tpVOL, {80, 50, 64, 50});
    q.pattern(0, 64, 55);
    q.lfo(6, 0, 7, 40, 1, 0, tpPAN);
    q.lfo(2, 0, 2, 35, 0, 0, 2);
    q.send(4, 70, 40); q.send(5, 55, 50); q.send(1, 0, 22);
    q.echo(etv8D, 58, emvTape, 30, 65); q.verb(50, 60, rmvPlate, 30, 55);
    b.g(gTEMPO, 125);
    q.vol({{0, 102}, {1, 88}, {2, 76}, {3, 72}, {4, 74}, {5, 62}, {6, 62}});
    q.mix({0.2f, -1.0f, 11.2f, 0.2f, -1.4f, 7.9f, -12.1f});
}

// Dub Pressure 122: dub techno. Chord-like mallets and rims through tape echo and a long hall; noise hiss.
static void v2DubPressure(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {44, 74, 40, 40, 40, 0, 0, 10});
    b.kit(1, mRIM, {60, 28, 60, 70, 20, 20, 40, 30});
    b.kit(2, mPH_MALLET, {60, 44, 50, 50, 30, 64, 40, 0}); q.filt(2, 1, 72, 40);
    b.kit(3, mCLHAT, {66, 22, 76, 80, 20, 64, 0, 0});
    b.kit(4, mSHAKER, {20, 20, 50, 70, 100, 50, 60, 0});
    b.kit(5, mNOISE, {0, 127, 127, 80, 100, 0, 0, 0}); q.filt(5, 2, 90, 20);
    b.kit(6, mSINE, {22, 80, 10, 30, 10, 0, 40, 30});
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kBars({"......x.....x...", "...x......x.....", "......x.....x...", "...x..x...x..x.."})); q.cond(0, 1, {58, 61}, 3);
    q.row(0, 2, kBars({"..x.....x.......", "..x.......x.....", "..x.....x.......", "..x.....x.....x."}));
    q.cyc(0, 2, 0, {60, 60, 67, 60, 63, 72, 60});
    q.row(0, 3, kRep("..x...x...x...x.", 4)); q.cyc(0, 3, tpVOL, {70, 55});
    q.row(0, 4, kRep("x.xxx.xxx.xxx.xx", 4));
    q.row(0, 5, "x"); b.b.patterns[0].trackLen[5] = 64;
    q.row(0, 6, kRep("x.......x.......", 4)); q.cyc(0, 6, 0, {22, 22, 27, 25});
    q.pattern(0, 64, 53);
    q.lfo(2, 0, 2, 60, 1, 0, tpCUTOFF);
    q.lfo(1, 0, 1, 30, 1, 0, tpDEL);
    q.send(1, 90, 40); q.send(2, 85, 60); q.send(3, 20, 10); q.send(4, 10, 20); q.send(5, 0, 50);
    q.echo(etv8D, 82, emvTape, 70, 80); b.g(gECHO_LPF, 70); q.verb(95, 75, rmvHall, 60, 75, 30);
    b.g(gTEMPO, 122);
    q.vol({{0, 100}, {1, 70}, {2, 70}, {3, 66}, {4, 58}, {5, 42}, {6, 88}});
    q.mix({7.7f, 27.4f, 6.1f, 29.8f, -3.9f, 8.9f, -1.0f});
}

// Industrial 140: distorted membrane kick, crushed snare, metal clangs, gated room.
static void v2Industrial(PB& b)
{
    PB2 q(b);
    b.kit(0, mPH_KICK, {40, 56, 70, 50, 30, 50, 20, 80}); b.fx(0, tpDIST, 50);
    b.kit(1, mSNARE, {50, 30, 50, 20, 100, 60, 60, 40}); b.fx(1, tpSRR, 50);
    b.kit(2, mPH_METAL, {50, 60, 100, 90, 20, 80, 90, 60});
    b.kit(3, mNOISE, {0, 0, 20, 90, 127, 30, 70, 50});
    b.kit(4, mFM_RIM, {80, 18, 70, 70, 20, 40, 15, 60});
    b.kit(5, mTOM, {30, 60, 70, 30, 60, 30, 20, 70});
    q.row(0, 0, kBars({"x...x...x...x...", "x...x...x...x..x", "x...x...x...x...", "x...x...x.x.x.xx"}));
    q.row(0, 1, kRep("....x.......x...", 4)); q.cond(0, 1, {60}, 0);
    q.row(0, 2, kBars({"...x......x.....", "......x.....x...", "...x......x.....", "...x..x...x.x..x"})); q.cyc(0, 2, 0, {50, 44, 60, 38});
    q.row(0, 3, kRep("xxxxxxxxxxxxxxxx", 4)); q.cyc(0, 3, tpVOL, {90, 50, 70, 50}); q.cond(0, 3, {5, 13, 21, 29, 37, 45, 53, 61}, 3);
    q.row(0, 4, kRep(".x..x..x.x..x..x", 4));
    q.row(0, 5, kBars({"................", "................", "................", "x..x..x..x..x.x."})); q.cyc(0, 5, 0, {50, 44, 38, 32, 28, 24});
    q.pattern(0, 64, 50);
    q.lfo(3, 0, 5, 50, 4, 1, tpCUTOFF); q.filt(3, 3, 80, 40);
    q.send(1, 0, 40); q.send(2, 30, 40); q.send(4, 40, 0);
    q.echo(etv16D, 50, emvLofi, 60, 50); q.verb(30, 40, rmvRoom, 80, 60); b.g(gREV_GATE, 70);
    q.comp(90, 80); b.g(gDRIVE, 45);
    b.g(gTEMPO, 140);
    q.vol({{0, 100}, {1, 84}, {2, 70}, {3, 56}, {4, 66}, {5, 80}});
    q.mix({-10.4f, 25.5f, -6.7f, 2.8f, 22.3f, -13.5f});
}

// Minimal Groove 126: clicks, claves and a woody bass, everything moves a little each loop.
static void v2Minimal(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {54, 52, 40, 24, 80, 0, 0, 10});
    b.kit(1, mRIM, {70, 28, 40, 90, 20, 10, 60, 30});
    b.kit(2, mCLAVE, {70, 20, 70, 64, 30, 0, 20, 0});
    b.kit(3, mCLHAT, {80, 10, 100, 100, 20, 60, 20, 0});
    b.kit(4, mPH_MALLET, {50, 30, 40, 70, 20, 60, 10, 0});
    b.kit(5, mSINE, {26, 30, 30, 20, 0, 0, 10, 20});
    b.kit(6, mSHAKER, {0, 10, 20, 90, 120, 30, 80, 0});
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kRep("...x..x....x..x.", 4)); q.cond(0, 1, {14, 30, 46, 62}, 3); q.micro(0, 1, {3, 19, 35, 51}, -3);
    q.row(0, 2, kRep(".x.....x..x.....", 4)); q.cyc(0, 2, 0, {70, 76, 64});
    q.row(0, 3, kRep("..x...x...x...x.", 4));
    q.row(0, 4, kBars({"..........x.....", "......x.........", "..........x..x..", "......x.....x..."})); q.cyc(0, 4, 0, {50, 57, 55, 62});
    q.row(0, 5, kRep("..x..x.x..x...x.", 4)); q.cyc(0, 5, 0, {26, 26, 31, 26, 29, 26, 24}); q.cond(0, 5, {7, 23, 39, 55}, 2);
    q.row(0, 6, kRep("xxxxxxxxxxxxxxxx", 4)); q.cyc(0, 6, tpVOL, {70, 40, 55, 40});
    b.b.patterns[0].trackLen[2] = 12; b.b.patterns[0].trackLen[4] = 56;
    q.pattern(0, 64, 56);
    q.lfo(4, 0, 2, 40, 1, 0, tpCUTOFF); q.filt(4, 1, 80, 40);
    q.lfo(1, 0, 3, 50, 5, 0, tpPAN);
    q.send(1, 40, 20); q.send(2, 30, 10); q.send(4, 50, 40);
    q.echo(etv16D, 50, emvPing, 60, 60); q.verb(40, 70, rmvRoom, 40, 50);
    b.g(gTEMPO, 126);
    q.vol({{0, 100}, {1, 70}, {2, 70}, {3, 70}, {4, 70}, {5, 86}, {6, 56}});
    q.mix({6.6f, 9.2f, 13.4f, 35.9f, 8.5f, -1.4f, -7.7f});
}

// ======================================================================== HOUSE
// K-House 128: inspired by the owner's "K-House": kick, clap in the reverb, shaker with a moving tone, noise sweep
// into the echo, sine bass, toms and rims.
static void v2KHouse(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {50, 62, 48, 28, 66, 0, 6, 24});
    b.kit(1, mCLAP, {68, 50, 54, 56, 30, 76, 24, 14});
    b.kit(2, mCLHAT, {72, 16, 90, 90, 30, 60, 0, 0});
    b.kit(3, mOPHAT, {72, 46, 88, 88, 30, 60, 0, 0}); b.choke(2, 1); b.choke(3, 1);
    b.kit(4, mRIM, {64, 28, 52, 84, 30, 20, 50, 30});
    b.kit(5, mSHAKER, {8, 18, 34, 80, 110, 40, 64, 0});
    b.kit(6, mNOISE, {60, 40, 70, 30, 100, 50, 0, 0}); q.filt(6, 2, 70, 60);
    b.kit(7, mSINE, {28, 50, 10, 30, 30, 0, 20, 40});
    b.kit(8, mTOM, {52, 46, 40, 30, 30, 10, 10, 10});
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kRep("....x.......x...", 4)); q.cond(0, 1, {63}, 0);
    q.row(0, 2, kBars({"x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.xxxx"}));
    q.row(0, 3, kRep("..x...x...x...x.", 4));
    q.row(0, 4, kBars({".......x......x.", "...x...x........", ".......x......x.", "...x...x...x..x."}));
    q.row(0, 5, kRep("xxxxxxxxxxxxxxxx", 4)); q.cyc(0, 5, 4, {80, 110, 95, 120, 100}); q.cyc(0, 5, tpVOL, {80, 50, 64, 50});
    q.row(0, 6, kBars({"................", "......x.........", "................", "......x.....x..."})); q.cyc(0, 6, tpCUTOFF, {60, 80, 100});
    q.row(0, 7, kBars({"..x..x....x..x..", "..x..x....x..x.x", "..x..x....x..x..", "..x..x..x.x...x."}));
    q.cyc(0, 7, 0, {28, 28, 35, 28, 33, 28, 28, 35, 28, 33, 39, 28, 28, 35, 28, 33, 28, 28, 33, 37, 30});
    q.row(0, 8, kBars({"................", "................", "..........x.....", "............x.x."})); q.cyc(0, 8, 0, {56, 50, 44});
    q.pattern(0, 64, 57);
    q.copy(0, 1);
    q.row(1, 0, kBars({"x...x...x...x...", "x...x...x...x...", "x...............", "................"}));
    q.row(1, 6, kBars({"................", "................", "x...............", "................"})); b.lock(1, 6, 32, tpCUTOFF, 30);
    q.pattern(1, 64, 57);
    q.lfo(6, 0, 1, 90, 3, 1, tpCUTOFF);     // the noise sweep rises over 8 bars
    q.lfo(7, 0, 4, 30, 1, 0, tpCUTOFF); q.filt(7, 1, 60, 50);
    q.send(1, 0, 32); q.send(4, 30, 10); q.send(5, 16, 10); q.send(6, 50, 40); q.send(8, 20, 20);
    q.echo(etv8D, 55, emvPing, 50, 65); q.verb(55, 55, rmvPlate, 40, 60);
    q.chain("1 2 0:3 1:1");
    b.g(gTEMPO, 128);
    q.vol({{0, 102}, {1, 88}, {2, 72}, {3, 70}, {4, 70}, {5, 54}, {6, 56}, {7, 92}, {8, 72}});
    q.mix({7.9f, 5.3f, 31.4f, 7.4f, 31.2f, -5.6f, 28.3f, -0.8f, 4.1f});
}

// Deep House 122: soft kick, swung shaker, mallet chords with a slow filter, round sine bass.
static void v2DeepHouse(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {48, 64, 40, 30, 40, 0, 0, 10});
    b.kit(1, mCLAP, {66, 52, 60, 54, 30, 80, 30, 24});
    b.kit(2, mCLHAT, {66, 20, 80, 84, 20, 64, 0, 0});
    b.kit(3, mOPHAT, {66, 50, 80, 84, 20, 64, 0, 0}); b.choke(2, 1); b.choke(3, 1);
    b.kit(4, mSHAKER, {14, 20, 40, 70, 100, 40, 60, 0});
    b.kit(5, mPH_MALLET, {64, 50, 40, 40, 40, 64, 40, 0}); q.filt(5, 1, 70, 30);
    b.kit(6, mSINE, {30, 60, 6, 30, 10, 0, 30, 20});
    b.kit(7, mRIM, {58, 28, 60, 60, 20, 30, 40, 30});
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kRep("....x.......x...", 4));
    q.row(0, 2, kRep("x...x...x...x...", 4)); q.micro(0, 2, {0}, 0);
    q.row(0, 3, kRep("..x...x...x...x.", 4));
    q.row(0, 4, kRep(".xx..xx..xx..xxx", 4)); q.cyc(0, 4, tpVOL, {60, 45, 55, 45});
    q.row(0, 5, kBars({"..x....x...x....", "..x....x........", "..x....x...x....", "..x....x....x.x."}));
    q.cyc(0, 5, 0, {64, 71, 67, 64, 71, 62, 69, 66, 64, 71, 67, 64, 71, 74});
    q.row(0, 6, kBars({"x.....x...x.....", "x.....x...x...x.", "x.....x...x.....", "x.....x.x...x..."}));
    q.cyc(0, 6, 0, {30, 30, 37, 30, 30, 35, 33, 30, 30, 37, 28, 30, 33, 37});
    q.row(0, 7, kRep("...x..x.......x.", 4)); q.cond(0, 7, {14, 46}, 3);
    q.pattern(0, 64, 58);
    q.lfo(5, 0, 1, 70, 1, 0, tpCUTOFF);
    q.send(5, 50, 50, 60); q.send(1, 0, 40); q.send(7, 40, 20);
    q.echo(etv8D, 50, emvBbd, 50, 60); q.verb(70, 70, rmvHall, 40, 60);
    b.g(gCHO_MODEL, 50); b.g(gCHO_LEV, 60);
    b.g(gTEMPO, 122);
    q.vol({{0, 100}, {1, 84}, {2, 70}, {3, 68}, {4, 56}, {5, 74}, {6, 90}, {7, 66}});
    q.mix({11.7f, 14.1f, 31.5f, 5.9f, -1.7f, 8.9f, -1.7f, 29.4f});
}

// Chicago Jack 126: jacking 4/4, cowbell and rim shuffle, open hats, a tom line and a bouncing sine.
static void v2Chicago(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {52, 58, 50, 26, 64, 0, 0, 30});
    b.kit(1, mCLAP, {70, 44, 50, 60, 30, 66, 30, 20});
    b.kit(2, mCLHAT, {72, 16, 88, 90, 30, 60, 0, 0});
    b.kit(3, mOPHAT, {72, 40, 86, 90, 30, 60, 0, 0}); b.choke(2, 1); b.choke(3, 1);
    b.kit(4, mCOWBELL, {60, 40, 64, 70, 40, 70, 30, 20});
    b.kit(5, mRIM, {64, 28, 50, 84, 30, 20, 50, 30});
    b.kit(6, mTOM, {50, 40, 40, 30, 40, 10, 10, 20});
    b.kit(7, mSINE, {30, 40, 20, 30, 40, 0, 10, 50});
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kBars({"....x.......x...", "....x.......x...", "....x.......x...", "....x..x....x.x."}));
    q.row(0, 2, kRep("x.xxx.xxx.xxx.xx", 4)); q.cyc(0, 2, tpVOL, {80, 55, 70, 55});
    q.row(0, 3, kRep("..x...x...x...x.", 4));
    q.row(0, 4, kRep("...x..x..x....x.", 4)); q.cyc(0, 4, 0, {60, 60, 66, 60}); q.cond(0, 4, {9, 41}, 3);
    q.row(0, 5, kRep(".x.x...x.x.x...x", 4)); q.cyc(0, 5, tpVOL, {70, 50, 60});
    q.row(0, 6, kBars({"..........x.....", "..........x...x.", "..........x.....", "......x...x.x.x."})); q.cyc(0, 6, 0, {50, 44, 56, 40});
    q.row(0, 7, kRep("x..x..x.x..x..x.", 4)); q.cyc(0, 7, 0, {30, 30, 37, 30, 35, 30, 33});
    q.pattern(0, 64, 58);
    q.send(1, 0, 26); q.send(4, 45, 10); q.send(5, 20, 0); q.send(6, 20, 15);
    q.echo(etv8D, 45, emvPing, 40, 55); q.verb(40, 60, rmvRoom, 50, 50);
    q.comp(108, 45); b.g(gDRIVE, 14);
    b.g(gTEMPO, 126);
    q.vol({{0, 100}, {1, 88}, {2, 72}, {3, 70}, {4, 66}, {5, 64}, {6, 70}, {7, 90}});
    q.mix({3.6f, -0.2f, 14.6f, 3.8f, 0.5f, 13.4f, -1.0f, -4.4f});
}

// UK Garage 132: swung 2-step, shuffled hats, rim ghosts, a bass that bounces.
static void v2Garage(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {52, 54, 50, 24, 60, 0, 0, 20});
    b.kit(1, mSNARE, {60, 30, 30, 18, 80, 80, 40, 40});
    b.kit(2, mCLAP, {70, 40, 50, 60, 30, 70, 30, 10});
    b.kit(3, mCLHAT, {74, 14, 90, 90, 30, 60, 0, 0});
    b.kit(4, mRIM, {64, 28, 50, 84, 30, 20, 50, 30});
    b.kit(5, mSHAKER, {10, 14, 30, 80, 110, 40, 64, 0});
    b.kit(6, mSINE, {30, 40, 30, 20, 50, 0, 10, 50});
    b.kit(7, mFM_BELL, {70, 30, 50, 10, 40, 60, 40, 64});
    q.row(0, 0, kBars({"x.........x.....", "x.......x.x.....", "x.........x.....", "x.......x...x.x."}));
    q.row(0, 1, kRep("....x.......x...", 4));
    q.row(0, 2, kRep("....x.......x...", 4));
    q.row(0, 3, kRep("..x.xx.x..x.xx.x", 4)); q.cyc(0, 3, tpVOL, {80, 55, 70, 55, 65});
    q.row(0, 4, kRep(".x....x..x....x.", 4)); q.cond(0, 4, {9, 25, 41, 57}, 3);
    q.row(0, 5, kRep("x.xxx.xxx.xxx.xx", 4));
    q.row(0, 6, kBars({"x.....x...x..x..", "x.....x...x.....", "x.....x...x..x..", "x.....x.x...x.x."})); q.cyc(0, 6, 0, {30, 30, 37, 35, 30, 33, 30, 28});
    q.row(0, 7, kBars({"................", "...........x....", "................", "...x.......x...."})); q.cyc(0, 7, 0, {70, 77});
    q.pattern(0, 64, 64);
    q.send(2, 0, 30); q.send(4, 30, 10); q.send(7, 60, 40);
    q.echo(etv8D, 50, emvPing, 50, 60); q.verb(45, 60, rmvPlate, 40, 55);
    b.g(gTEMPO, 132);
    q.vol({{0, 100}, {1, 80}, {2, 82}, {3, 70}, {4, 66}, {5, 54}, {6, 92}, {7, 60}});
    q.mix({2.5f, 21.4f, 2.4f, 18.2f, 11.0f, -6.4f, -3.3f, 9.5f});
}

// Afro House 122: congas (toms), 3-2 clave, shaker at 3/2x, bells in the chorus.
static void v2AfroHouse(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {50, 60, 40, 30, 50, 0, 0, 20});
    b.kit(1, mTOM, {66, 30, 20, 20, 30, 20, 30, 0});
    b.kit(2, mTOM, {80, 24, 20, 20, 40, 20, 30, 0});
    b.kit(3, mCLAVE, {72, 20, 70, 64, 30, 0, 20, 0});
    b.kit(4, mSHAKER, {10, 16, 30, 80, 110, 40, 64, 0}); b.speed(4, 3);
    b.kit(5, mFM_BELL, {72, 40, 50, 10, 50, 64, 40, 64});
    b.kit(6, mCLAP, {64, 50, 60, 50, 30, 80, 40, 0});
    b.kit(7, mOPHAT, {70, 40, 86, 88, 30, 60, 0, 0});
    q.row(0, 0, kRep("x...x...x...x...", 4));
    q.row(0, 1, kRep("..x..x.x...x..x.", 4)); q.cyc(0, 1, tpVOL, {80, 70, 60, 75, 65});
    q.row(0, 2, kRep("...x......x..x..", 4)); q.cond(0, 2, {13, 45}, 2);
    q.row(0, 3, kRep("x..x..x...x.x...", 4));
    q.row(0, 4, kRep("xxxxxxxxxxxx", 1)); b.b.patterns[0].trackLen[4] = 12;
    q.row(0, 5, kBars({"......x.....x...", "......x...x.....", "......x.....x...", "..x...x...x...x."})); q.cyc(0, 5, 0, {72, 79, 76, 72, 84});
    q.row(0, 6, kRep("....x.......x...", 4));
    q.row(0, 7, kRep("..x...x...x...x.", 4));
    q.pattern(0, 64, 56);
    q.send(5, 50, 40, 80); q.send(3, 20, 10); q.send(6, 0, 40);
    b.g(gCHO_MODEL, 50); b.g(gCHO_LEV, 70);
    q.echo(etv8D, 50, emvTape, 40, 55); q.verb(55, 65, rmvHall, 40, 50);
    b.g(gTEMPO, 122);
    q.pan({{1, 50}, {2, 80}, {3, 70}, {4, 58}, {5, 76}});
    q.vol({{0, 100}, {1, 76}, {2, 72}, {3, 66}, {4, 56}, {5, 62}, {6, 80}, {7, 66}});
    q.mix({1.2f, 1.3f, 2.2f, 9.3f, -12.0f, 3.9f, 6.4f, 2.4f});
}

// ======================================================================== BREAKS
static void v2Breakbeat(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {50, 50, 50, 24, 70, 0, 10, 40}); b.g(gKATK + 0, 80);
    b.kit(1, mSNARE, {58, 32, 40, 20, 90, 70, 50, 40});
    b.kit(2, mCLHAT, {74, 16, 90, 90, 30, 60, 0, 10});
    b.kit(3, mOPHAT, {72, 40, 86, 90, 30, 60, 0, 0}); b.choke(2, 1); b.choke(3, 1);
    b.kit(4, mCYMBAL, {64, 90, 60, 70, 30, 64, 30, 0});
    b.kit(5, mSINE, {26, 60, 20, 30, 40, 0, 10, 60});
    b.kit(6, mFM_TOM, {50, 40, 50, 40, 40, 40, 30, 30});
    q.row(0, 0, kBars({"x.........x.....", "x.x.......x.....", "x.........x..x..", "x.x.......x....."}));
    q.row(0, 1, kBars({"....x..x.x..x...", "....x..x....x..x", "....x..x.x..x...", "....x..x..x.xxxx"}));
    q.cyc(0, 1, tpVOL, {110, 60, 70, 110}); b.ratch(0, 1, 60, 2);
    q.row(0, 2, kRep("x.xxx.xxx.xxx.xx", 4)); q.cyc(0, 2, tpVOL, {90, 55, 75, 55});
    q.row(0, 3, kRep("..............x.", 4));
    q.row(0, 4, "x"); b.b.patterns[0].trackLen[4] = 64;
    q.row(0, 5, kBars({"x.........x.....", "x.x.......x.....", "x.........x..x..", "x.x.......x....."})); q.cyc(0, 5, 0, {26, 26, 33, 26, 31, 26, 26, 24});
    q.row(0, 6, kBars({"................", "................", "................", "........x.x.x..."})); q.cyc(0, 6, 0, {60, 52, 44});
    q.micro(0, 1, {7, 23, 39, 55}, 3);
    q.pattern(0, 64, 56);
    q.send(1, 0, 22); q.send(4, 0, 30); q.send(6, 30, 20);
    q.echo(etv16D, 40, emvTape, 30, 50); q.verb(40, 60, rmvRoom, 50, 55);
    q.comp(95, 75); b.fx(1, tpDIST, 20); b.g(gDRIVE, 25);
    b.g(gTEMPO, 130);
    q.vol({{0, 100}, {1, 86}, {2, 74}, {3, 70}, {4, 56}, {5, 88}, {6, 74}});
    q.mix({-4.3f, -3.8f, -0.7f, -1.8f, -5.8f, -8.2f, -3.7f});
}

// Jungle 170: chopped break feel with ghost snares and ratchets, sub bass, dub stabs into the echo.
static void v2Jungle(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {50, 46, 50, 22, 74, 0, 10, 40});
    b.kit(1, mSNARE, {62, 28, 40, 20, 90, 76, 50, 30});
    b.kit(2, mCLHAT, {78, 12, 95, 95, 30, 60, 10, 20});
    b.kit(3, mCYMBAL, {70, 70, 70, 70, 30, 64, 40, 0});
    b.kit(4, mSINE, {20, 90, 10, 30, 10, 0, 20, 20});
    b.kit(5, mPH_METAL, {60, 40, 80, 70, 30, 64, 60, 10});
    b.kit(6, mRIM, {66, 28, 50, 84, 30, 20, 50, 30});
    q.row(0, 0, kBars({"x.x.......x.....", "x.........xx....", "x.x.......x.....", "x.........x..x.."}));
    q.row(0, 1, kBars({"....x..x.x..x..x", "..x.x..x....x...", "....x..x.x..x..x", "..x.x..x..x.x.xx"}));
    q.cyc(0, 1, tpVOL, {110, 55, 60, 110, 60}); b.ratch(0, 1, 62, 3); b.ratch(0, 1, 63, 2);
    q.row(0, 2, kRep("x.x.x.x.x.x.x.x.", 4)); q.cyc(0, 2, tpVOL, {80, 60});
    q.row(0, 3, kRep("x...............", 4)); q.cond(0, 3, {16, 48}, 3);
    q.row(0, 4, kBars({"x...............", "x.........x.....", "x...............", "x.....x.....x..."})); q.cyc(0, 4, 0, {20, 20, 25, 20, 27, 23});
    q.row(0, 5, kBars({"......x.........", "..........x.....", "......x.........", "..x.......x....."})); q.cyc(0, 5, 0, {60, 66, 53});
    q.row(0, 6, kRep("...x......x.....", 4)); q.cond(0, 6, {10, 42}, 3);
    q.micro(0, 1, {7, 23, 39, 55}, 2);
    q.pattern(0, 64, 52);
    q.copy(0, 1); q.row(1, 0, kRep("x...............", 4)); q.row(1, 1, kBars({"................", "................", "................", "x.x.x.x.xxxxxxxx"})); q.cyc(1, 1, 0, {50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72});
    q.pattern(1, 64, 52);
    q.send(1, 0, 25); q.send(5, 70, 40); q.send(6, 50, 10); q.send(3, 0, 30);
    q.echo(etv8D, 70, emvTape, 50, 65); q.verb(50, 60, rmvSpring, 50, 50);
    q.comp(98, 65); b.g(gDRIVE, 20);
    q.chain("1 2 0:3 1:1");
    b.g(gTEMPO, 170);
    q.vol({{0, 96}, {1, 84}, {2, 66}, {3, 56}, {4, 98}, {5, 62}, {6, 60}});
    q.mix({-1.6f, 9.5f, 1.0f, -3.8f, -8.2f, 3.0f, 13.0f});
}

// Liquid 174: soft kick, ghost snares, ride, mallet chords in a MULTI-TAP echo that ducks.
static void v2Liquid(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {50, 50, 40, 24, 50, 0, 0, 10});
    b.kit(1, mSNARE, {64, 30, 30, 20, 80, 90, 40, 30});
    b.kit(2, mFM_CYMBAL, {74, 60, 70, 40, 80, 90, 60, 80});
    b.kit(3, mCLHAT, {76, 12, 90, 90, 20, 64, 0, 0});
    b.kit(4, mPH_MALLET, {64, 60, 40, 30, 40, 64, 50, 0}); q.filt(4, 1, 76, 30);
    b.kit(5, mSINE, {24, 90, 4, 30, 0, 0, 20, 10});
    q.row(0, 0, kBars({"x.........x.....", "x.........x.x...", "x.........x.....", "x.......x.x....."}));
    q.row(0, 1, kBars({"....x......x.x..", "....x..x....x...", "....x......x.x..", "....x..x....x.xx"})); q.cyc(0, 1, tpVOL, {104, 55, 60, 104, 55});
    q.row(0, 2, kRep("x.x.x.x.x.x.x.x.", 4)); q.cyc(0, 2, tpVOL, {74, 56});
    q.row(0, 3, kRep(".x.x.x.x.x.x.x.x", 4)); q.cond(0, 3, {3, 11, 19, 27, 35, 43, 51, 59}, 3);
    q.row(0, 4, kBars({"x...............", "......x.........", "x...............", "......x...x....."})); q.cyc(0, 4, 0, {64, 71, 60, 67, 69});
    q.row(0, 5, kBars({"x...............", "x.........x.....", "x...............", "x.....x...x....."})); q.cyc(0, 5, 0, {24, 24, 31, 24, 29, 27});
    q.pattern(0, 64, 52);
    q.send(4, 70, 60); q.send(1, 0, 30); q.send(2, 10, 20);
    q.echo(etv8D, 55, emvMulti, 60, 60); b.g(gECHO_DUCK, 70); q.verb(70, 60, rmvHall, 50, 60);
    q.lfo(4, 0, 2, 50, 1, 0, tpCUTOFF);
    b.g(gTEMPO, 174);
    q.vol({{0, 94}, {1, 82}, {2, 62}, {3, 58}, {4, 72}, {5, 98}});
    q.mix({7.7f, 24.4f, -7.5f, 26.6f, 5.5f, -5.3f});
}

// Footwork 160: triplet-ish kicks, rolling hats with ratchets, chopped claps, a sliding sine.
static void v2Footwork(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {44, 70, 60, 30, 50, 0, 10, 40});
    b.kit(1, mCLAP, {70, 40, 40, 60, 30, 60, 30, 20});
    b.kit(2, mCLHAT, {80, 10, 95, 95, 30, 60, 10, 20});
    b.kit(3, mRIM, {70, 28, 50, 84, 30, 20, 50, 30});
    b.kit(4, mSINE, {26, 70, 30, 30, 40, 0, 20, 50});
    b.kit(5, mPH_STRING, {64, 40, 70, 50, 40, 0, 50, 0});
    q.row(0, 0, kBars({"x..x..x...x..x..", "x..x..x...x.....", "x..x..x...x..x..", "x..x..x.x.x.x..."}));
    q.row(0, 1, kRep("....x.......x...", 4)); q.cond(0, 1, {28, 60}, 7);
    q.row(0, 2, kRep("x.xxx.xxx.xxx.xx", 4)); b.ratch(0, 2, 15, 3); b.ratch(0, 2, 31, 4); b.ratch(0, 2, 47, 3); b.ratch(0, 2, 63, 4);
    q.row(0, 3, kRep("..x..x.....x..x.", 4)); q.cond(0, 3, {5, 37}, 3);
    q.row(0, 4, kBars({"x..x..x...x..x..", "x..x..x...x.....", "x..x..x...x..x..", "x..x..x.x.x.x..."})); q.cyc(0, 4, 0, {26, 26, 33, 31, 28, 26});
    q.row(0, 5, kBars({"......x.....x...", "......x.........", "......x.....x...", "..x...x...x....."})); q.cyc(0, 5, 0, {64, 69, 67, 74});
    q.pattern(0, 64, 50);
    q.send(1, 0, 30); q.send(3, 40, 10); q.send(5, 60, 40);
    q.echo(etv16D, 50, emvPing, 50, 55); q.verb(40, 60, rmvPlate, 40, 50);
    b.g(gTEMPO, 160);
    q.vol({{0, 98}, {1, 84}, {2, 66}, {3, 62}, {4, 90}, {5, 66}});
    q.mix({2.4f, 9.9f, 8.1f, 35.0f, -1.4f, 12.6f});
}

// ======================================================================== HIP-HOP
static void v2BoomBap(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {48, 60, 40, 30, 60, 10, 30, 50});
    b.kit(1, mSNARE, {54, 40, 30, 20, 100, 60, 50, 50}); b.fx(1, tpSRR, 30);
    b.kit(2, mCLHAT, {66, 18, 70, 80, 20, 64, 0, 0}); b.fx(2, tpSRR, 40);
    b.kit(3, mPH_STRING, {60, 50, 50, 40, 50, 10, 60, 30});
    b.kit(4, mNOISE, {0, 127, 127, 40, 70, 0, 80, 0});
    b.kit(5, mRIM, {60, 28, 50, 70, 30, 30, 40, 30});
    q.row(0, 0, kBars({"x.........x.x...", "x......x..x.....", "x.........x.x...", "x......x...x...."}));
    q.row(0, 1, kRep("....x.......x...", 4)); q.micro(0, 1, {4, 12, 20, 28, 36, 44, 52, 60}, 3);
    q.row(0, 2, kRep("x.x.x.x.x.x.x.x.", 4)); q.cyc(0, 2, tpVOL, {80, 55, 70, 50});
    q.row(0, 3, kBars({"x.....x...x.....", "x.....x.........", "x.....x...x.....", "x.....x.....x..."})); q.cyc(0, 3, 0, {60, 63, 67, 58, 60, 63, 55});
    q.row(0, 4, "x"); b.b.patterns[0].trackLen[4] = 64;
    q.row(0, 5, kRep("...x......x.....", 4)); q.cond(0, 5, {10, 42}, 3);
    q.pattern(0, 64, 62);
    q.send(1, 0, 20); q.send(3, 40, 20); q.send(5, 30, 0);
    q.echo(etv8, 45, emvTape, 40, 50); b.g(gECHO_DRIVE, 50); q.verb(30, 70, rmvRoom, 50, 45);
    q.comp(92, 70); b.g(gEQ_HIGH, 52); b.g(gDRIVE, 25);
    b.g(gTEMPO, 90);
    q.vol({{0, 100}, {1, 90}, {2, 70}, {3, 78}, {4, 40}, {5, 64}});
    q.mix({-11.6f, -3.6f, 3.2f, -16.3f, 2.1f, -0.6f});
}

static void v2Trap(PB& b)
{
    PB2 q(b);
    b.kit(0, mSINE, {22, 110, 40, 20, 30, 0, 0, 50}); // 808 boom
    b.kit(1, mKICK, {54, 30, 60, 20, 80, 0, 0, 40});
    b.kit(2, mCLAP, {70, 50, 60, 50, 30, 70, 30, 10});
    b.kit(3, mCLHAT, {80, 10, 95, 95, 30, 60, 10, 10});
    b.kit(4, mOPHAT, {76, 40, 92, 92, 30, 60, 0, 0}); b.choke(3, 1); b.choke(4, 1);
    b.kit(5, mPH_MALLET, {70, 40, 70, 30, 60, 64, 40, 0});
    q.row(0, 0, kBars({"x......x..x.....", "x.........x...x.", "x......x..x.....", "x.......x.x..x.."})); q.cyc(0, 0, 0, {22, 22, 27, 22, 22, 20, 25, 22, 22, 27, 22, 29, 25});
    q.row(0, 1, kBars({"x......x..x.....", "x.........x...x.", "x......x..x.....", "x.......x.x..x.."}));
    q.row(0, 2, kRep("........x.......", 4));
    q.row(0, 3, kRep("x.x.x.x.x.x.x.x.", 4)); b.ratch(0, 3, 6, 3); b.ratch(0, 3, 14, 4); b.ratch(0, 3, 30, 3); b.ratch(0, 3, 38, 2); b.ratch(0, 3, 46, 4); b.ratch(0, 3, 62, 6);
    q.cyc(0, 3, tpVOL, {84, 60, 70, 60});
    q.row(0, 4, kRep("......x.........", 4)); q.cond(0, 4, {22, 54}, 3);
    q.row(0, 5, kBars({"x.....x...x.....", "x.....x.........", "x.....x...x.....", "x.....x...x...x."})); q.cyc(0, 5, 0, {70, 73, 77, 68, 70, 75});
    q.pattern(0, 64, 50);
    q.send(2, 0, 30); q.send(5, 40, 50);
    q.echo(etv8, 40, emvPing, 40, 50); q.verb(60, 60, rmvHall, 40, 55);
    b.fx(0, tpDIST, 30);
    b.g(gTEMPO, 140);
    q.vol({{0, 100}, {1, 70}, {2, 86}, {3, 64}, {4, 62}, {5, 68}});
    q.mix({-7.3f, 19.7f, 11.9f, 13.9f, 10.5f, 7.4f});
}

static void v2LofiDust(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {46, 60, 30, 30, 30, 10, 20, 30}); b.fx(0, tpSRR, 30);
    b.kit(1, mSNARE, {52, 38, 20, 20, 90, 50, 40, 60}); b.fx(1, tpSRR, 50);
    b.kit(2, mCLHAT, {60, 20, 60, 70, 20, 64, 0, 0}); b.fx(2, tpSRR, 60);
    b.kit(3, mPH_MALLET, {58, 60, 30, 40, 40, 64, 50, 10}); q.filt(3, 1, 64, 20);
    b.kit(4, mNOISE, {0, 127, 127, 50, 60, 0, 90, 0});
    b.kit(5, mSINE, {28, 80, 6, 30, 0, 0, 30, 20});
    q.row(0, 0, kBars({"x.......x.x.....", "x.........x.....", "x.......x.x.....", "x.....x...x....."}));
    q.row(0, 1, kRep("....x.......x...", 4)); q.micro(0, 1, {4, 12, 20, 28, 36, 44, 52, 60}, 4);
    q.row(0, 2, kRep("x.xx.xx.x.xx.xx.", 4)); q.cyc(0, 2, tpVOL, {70, 50, 60, 45});
    q.row(0, 3, kBars({"x.......x.......", "x.......x.......", "x.......x.......", "x.......x...x..."})); q.cyc(0, 3, 0, {58, 63, 56, 61, 58, 65, 60});
    q.row(0, 4, "x"); b.b.patterns[0].trackLen[4] = 64;
    q.row(0, 5, kBars({"x.......x.......", "x.........x.....", "x.......x.......", "x.....x...x....."})); q.cyc(0, 5, 0, {28, 28, 26, 31, 28, 33});
    q.pattern(0, 64, 64);
    q.lfo(3, 0, 2, 40, 1, 0, 0);    // detuned, wow-like mallets
    q.send(3, 40, 40, 50); q.send(1, 0, 20);
    q.echo(etv8, 40, emvLofi, 30, 45); q.verb(40, 80, rmvRoom, 50, 45);
    b.g(gEQ_HIGH, 44); b.g(gDRIVE, 20);
    b.g(gTEMPO, 82);
    q.vol({{0, 100}, {1, 88}, {2, 66}, {3, 76}, {4, 42}, {5, 90}});
    q.mix({-2.2f, 5.1f, 12.7f, -0.1f, 12.2f, -7.8f});
}

// ======================================================================== ELECTRO
static void v2Electro(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {46, 74, 60, 34, 66, 0, 20, 30});
    b.kit(1, mSNARE, {58, 34, 30, 20, 90, 70, 40, 44});
    b.kit(2, mCLAP, {80, 44, 54, 56, 30, 70, 30, 14});
    b.kit(3, mCLHAT, {70, 18, 86, 90, 30, 64, 0, 0});
    b.kit(4, mCOWBELL, {62, 44, 64, 70, 40, 70, 30, 20});
    b.kit(5, mSINE, {30, 50, 30, 40, 70, 0, 0, 40}); q.filt(5, 1, 60, 50);
    b.kit(6, mTOM, {40, 60, 50, 40, 40, 5, 0, 10});
    b.kit(7, mFM_RIM, {80, 16, 60, 64, 20, 30, 15, 30});
    q.row(0, 0, kBars({"x.....x...x..x..", "x.....x...x.....", "x.....x...x..x..", "x.....x...x.x.x."}));
    q.row(0, 1, kRep("....X.......X...", 4));
    q.row(0, 2, kRep("....x.......x..x", 4)); q.cond(0, 2, {15, 47}, 3);
    q.row(0, 3, kRep("x.xxx.xxx.xxx.xx", 4)); q.cyc(0, 3, tpVOL, {82, 56, 70, 56});
    q.row(0, 4, kRep("...x.....x......", 4)); q.cyc(0, 4, 0, {62, 66});
    q.row(0, 5, kBars({"x.....x...x..x..", "x.....x...x.....", "x.....x...x..x..", "x.....x...x.x.x."})); q.cyc(0, 5, 0, {30, 30, 36, 28, 30, 30, 33, 30, 30, 36, 28, 30, 33, 37});
    q.row(0, 6, kBars({"................", "..........x...x.", "................", "........x.x.x.x."})); q.cyc(0, 6, 0, {50, 44, 38, 32});
    q.row(0, 7, kRep(".x....x..x....x.", 4)); q.cond(0, 7, {25, 57}, 7);
    q.pattern(0, 64, 54);
    q.lfo(5, 0, 3, 50, 0, 0, tpCUTOFF);
    q.send(2, 0, 35); q.send(4, 40, 0); q.send(7, 45, 10);
    q.echo(etv8D, 50, emvPing, 50, 60); q.verb(40, 60, rmvPlate, 40, 50);
    b.g(gTEMPO, 128);
    q.vol({{0, 100}, {1, 84}, {2, 80}, {3, 70}, {4, 64}, {5, 92}, {6, 74}, {7, 62}});
    q.mix({-0.6f, 7.1f, 3.4f, 15.3f, 4.2f, 14.0f, -1.2f, 32.5f});
}

static void v2Braindance(PB& b)
{
    PB2 q(b);
    b.kit(0, mFM_KICK, {50, 50, 60, 20, 50, 40, 30, 30});
    b.kit(1, mFM_SNARE, {64, 34, 70, 40, 60, 60, 30, 60});
    b.kit(2, mFM_HAT, {80, 20, 0, 64, 90, 100, 60, 90});
    b.kit(3, mSINE, {30, 40, 30, 20, 60, 0, 0, 60}); q.filt(3, 1, 70, 80);
    b.kit(4, mFM_BELL, {70, 30, 60, 30, 60, 90, 40, 64});
    b.kit(5, mFM_RIM, {80, 14, 60, 70, 20, 30, 15, 40});
    q.row(0, 0, kBars({"x.....x...x.....", "x..x..x...x.....", "x.....x...x..x..", "x..x..x.x.x....."}));
    q.row(0, 1, kBars({"....x.......x...", "....x.......x..x", "....x.......x...", "....x...x.x.x.xx"})); b.ratch(0, 1, 63, 4);
    q.row(0, 2, kRep("xxxxxxxxxxxxxxxx", 4)); q.cyc(0, 2, tpVOL, {90, 50, 70, 50, 60}); b.ratch(0, 2, 30, 3); b.ratch(0, 2, 62, 4);
    q.row(0, 3, kRep("ssxsxsxxsxsxssxs", 4)); q.cyc(0, 3, 0, {30, 32, 37, 30, 42, 35, 30, 44, 39, 30, 37, 32, 47, 30});
    q.row(0, 4, kBars({"...x............", ".......x........", "...x.......x....", ".......x...x...x"})); q.cyc(0, 4, 0, {70, 77, 74, 82});
    q.row(0, 5, kRep(".x..x..x..x..x..", 4)); q.cond(0, 5, {4, 20, 36, 52}, 3);
    b.b.patterns[0].trackLen[3] = 13; b.b.patterns[0].trackLen[5] = 11;
    q.pattern(0, 64, 50);
    q.lfo(3, 0, 4, 80, 0, 0, tpCUTOFF); q.lfo(3, 1, 9, 40, 5, 0, tpRESO);
    q.send(4, 60, 40); q.send(1, 10, 20); q.send(5, 50, 0);
    q.echo(etv16D, 55, emvPing, 60, 60); q.verb(40, 50, rmvPlate, 50, 50);
    b.g(gTEMPO, 140);
    q.vol({{0, 100}, {1, 84}, {2, 64}, {3, 80}, {4, 62}, {5, 60}});
    q.mix({7.1f, 4.5f, -0.5f, -1.3f, 5.6f, 13.0f});
}

// ======================================================================== EXPERIMENTAL / AMBIENT
static void v2Polymeter(PB& b)
{
    PB2 q(b);
    b.kit(0, mKICK, {50, 56, 40, 30, 60, 0, 0, 20});
    b.kit(1, mPH_MALLET, {64, 40, 60, 40, 50, 64, 30, 0});
    b.kit(2, mPH_METAL, {70, 36, 80, 64, 40, 64, 64, 0});
    b.kit(3, mCLAVE, {72, 20, 70, 64, 30, 0, 20, 0});
    b.kit(4, mFM_BELL, {64, 40, 40, 20, 50, 70, 50, 64});
    b.kit(5, mSHAKER, {10, 16, 30, 80, 110, 40, 64, 0});
    b.kit(6, mSINE, {28, 60, 10, 30, 20, 0, 20, 30});
    q.row(0, 0, "x...x...x...x...");
    q.row(0, 1, "x..x.x..x.x.x"); q.cyc(0, 1, 0, {64, 71, 67, 74, 69, 62});
    q.row(0, 2, "..x...x..x."); q.cyc(0, 2, 0, {70, 64, 76});
    q.row(0, 3, "x..x..x..");
    q.row(0, 4, "....x.."); q.cyc(0, 4, 0, {64, 69, 71});
    q.row(0, 5, "xxxxx");
    q.row(0, 6, "x.....x...x.....x.....x...x...x."); q.cyc(0, 6, 0, {28, 28, 35, 33});
    q.pattern(0, 64, 52);
    q.send(1, 50, 40); q.send(2, 60, 40); q.send(4, 70, 50); q.send(3, 20, 10);
    q.echo(etv16D, 60, emvMulti, 60, 60); q.verb(60, 60, rmvHall, 50, 55);
    q.lfo(5, 0, 6, 40, 1, 0, tpPAN);
    b.g(gTEMPO, 118);
    q.vol({{0, 98}, {1, 68}, {2, 62}, {3, 64}, {4, 60}, {5, 54}, {6, 86}});
    q.mix({9.1f, 4.5f, 8.5f, 23.3f, 11.0f, -4.7f, 0.3f});
}

static void v2AmbientMetals(PB& b)
{
    PB2 q(b);
    b.kit(0, mPH_METAL, {60, 100, 60, 60, 20, 80, 80, 0});
    b.kit(1, mPH_MALLET, {64, 90, 40, 20, 40, 64, 80, 0});
    b.kit(2, mPH_STRING, {64, 100, 50, 50, 20, 0, 40, 0});
    b.kit(3, mFM_BELL, {64, 90, 30, 10, 40, 60, 60, 64});
    b.kit(4, mNOISE, {60, 60, 100, 40, 80, 20, 0, 0}); q.filt(4, 2, 70, 50);
    b.kit(5, mKICK, {40, 90, 20, 40, 10, 0, 0, 0});
    q.row(0, 0, kBars({"x...............", "..........x.....", "................", "......x........."})); q.cyc(0, 0, 0, {60, 67, 55});
    q.row(0, 1, kBars({"....x.......x...", "......x.........", "....x.......x...", "..x.......x....."})); q.cyc(0, 1, 0, {64, 71, 67, 76, 69, 62, 74});
    q.row(0, 2, kBars({"..x.............", "........x.......", "..x.............", "........x...x..."})); q.cyc(0, 2, 0, {64, 69, 71, 76});
    q.row(0, 3, kBars({"................", "x...............", "................", "x.......x......."})); q.cyc(0, 3, 0, {64, 71, 76});
    q.row(0, 4, "x"); b.b.patterns[0].trackLen[4] = 64;
    q.row(0, 5, kBars({"x...............", "................", "x...............", "................"}));
    for (int t = 1; t < 4; ++t) q.cond(0, t, {12, 40, 58}, 3);
    b.b.patterns[0].trackLen[1] = 60; b.b.patterns[0].trackLen[2] = 52;
    q.pattern(0, 64, 50);
    q.lfo(4, 0, 0, 70, 1, 0, tpCUTOFF); q.lfo(1, 0, 3, 40, 1, 0, tpPAN); q.lfo(2, 0, 4, 40, 1, 0, tpPAN);
    q.send(0, 70, 80); q.send(1, 70, 70, 60); q.send(2, 60, 80, 50); q.send(3, 80, 80); q.send(4, 30, 60); q.send(5, 0, 40);
    b.g(gCHO_MODEL, 50); b.g(gCHO_LEV, 60);
    q.echo(etv4, 70, emvDiffuse, 80, 70); q.verb(110, 60, rmvShimmer, 60, 80, 40);
    b.g(gTEMPO, 90);
    q.vol({{0, 66}, {1, 70}, {2, 66}, {3, 58}, {4, 50}, {5, 90}});
    q.mix({-7.1f, -4.0f, -4.2f, 1.6f, 2.9f, 9.3f});
}

// Chaos Breaks 172: a break that CHAOS keeps cutting up (snares and hats only), sub and stabs.
static void v2ChaosBreaks(PB& b)
{
    v2Jungle(b);
    PB2 q(b);
    b.g(gGL_CHANCE, 70); b.g(gGL_SIZE, 70); b.g(gGL_LEN, 40); b.g(gGL_PITCH, 80); b.g(gGL_REV, 40); b.g(gGL_MIX, 110);
    for (int t = 0; t < kTracks; ++t) q.chaos(t, 0);
    q.chaos(1, 127); q.chaos(2, 127); q.chaos(6, 127);
    b.b.chain = Chain();
    b.g(gTEMPO, 172);
    q.mix({1.0f, 9.3f, 1.1f, 1.6f, -1.9f, 1.6f, 16.2f});
}

// ======================================================================== BASICS
static void v2Demo(PB& b)
{
    PB2 q(b);
    for (int t = 0; t < kTracks; ++t) { b.kit(t, kDefaultKit[t]); if (kDefaultKitPitch[t] != 255) b.fx(t, 0, kDefaultKitPitch[t]); }
    q.row(0, 0, kRep("x...x...x...x...", 2));
    q.row(0, 1, kBars({"....x.......x...", "....x.......x.xx"}));
    q.row(0, 4, kRep("....x.......x...", 2));
    q.row(0, 7, kRep("..x...x...x...x.", 2));
    q.row(0, 8, kRep("..............x.", 2));
    q.row(0, 6, kRep("...x......x.....", 2));
    q.row(0, 2, kBars({"................", "..........x.x..."}));
    q.pattern(0, 32, 54);
    q.send(4, 0, 30); q.send(6, 40, 0);
    b.g(gTEMPO, 124);
    for (int t = 0; t < kTracks; ++t) b.b.mute[t] = 0;
    q.mix({1.3f, 3.3f, -9.5f, 0.0f, -2.9f, 0.0f, -8.8f, 3.0f, -8.3f});
}

