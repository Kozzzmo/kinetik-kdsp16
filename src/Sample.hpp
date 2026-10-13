// Kinetik KDSP-16 — user samples for the SAMPLE machine (one per track).
// Stored mono, float, at the file's sample rate; serialized as 16-bit PCM in base64 (plugin state, presets).
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace kn {

static constexpr float kMaxSampleSeconds = 20.f;

struct SampleData {
    std::vector<float> pcm;     // mono
    double sr = 48000.0;
    std::string name;           // file name, for the display
    uint32_t frames() const { return (uint32_t)pcm.size(); }
};
using SamplePtr = std::shared_ptr<const SampleData>;

namespace smp {

inline std::string base64(const uint8_t* d, size_t n)
{
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; o.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        o += T[v >> 18 & 63]; o += T[v >> 12 & 63];
        o += i + 1 < n ? T[v >> 6 & 63] : '='; o += i + 2 < n ? T[v & 63] : '=';
    }
    return o;
}
inline std::vector<uint8_t> unbase64(const char* s, size_t n)
{
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<uint8_t> o; o.reserve(n / 4 * 3);
    uint32_t acc = 0; int bits = 0;
    for (size_t i = 0; i < n; ++i) {
        const int v = val(s[i]);
        if (v < 0) continue;
        acc = acc << 6 | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; o.push_back((uint8_t)(acc >> bits & 0xFF)); }
    }
    return o;
}

// "name<TAB>samplerate<TAB>base64"   (name without tabs / newlines)
inline std::string encode(const SampleData& s)
{
    std::vector<uint8_t> b(s.pcm.size() * 2);
    for (size_t i = 0; i < s.pcm.size(); ++i) {
        const float c = s.pcm[i] < -1.f ? -1.f : (s.pcm[i] > 1.f ? 1.f : s.pcm[i]);
        const int16_t v = (int16_t)std::lround(c * 32767.f);
        b[i * 2] = (uint8_t)(v & 0xFF); b[i * 2 + 1] = (uint8_t)((uint16_t)v >> 8);
    }
    std::string name;
    for (char c : s.name) name += (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
    char srb[32]; std::snprintf(srb, sizeof(srb), "%.0f", s.sr);
    return name + "\t" + srb + "\t" + base64(b.data(), b.size());
}
inline SamplePtr decode(const char* s, size_t n)
{
    const char* t1 = (const char*)std::memchr(s, '\t', n);
    if (!t1) return nullptr;
    const char* t2 = (const char*)std::memchr(t1 + 1, '\t', n - (size_t)(t1 + 1 - s));
    if (!t2) return nullptr;
    auto d = std::make_shared<SampleData>();
    d->name.assign(s, t1);
    d->sr = std::atof(std::string(t1 + 1, t2).c_str());
    if (!(d->sr >= 4000.0 && d->sr <= 384000.0)) return nullptr;
    const std::vector<uint8_t> b = unbase64(t2 + 1, n - (size_t)(t2 + 1 - s));
    d->pcm.resize(b.size() / 2);
    for (size_t i = 0; i < d->pcm.size(); ++i) d->pcm[i] = (int16_t)(b[i * 2] | b[i * 2 + 1] << 8) / 32768.f;
    if (d->pcm.empty()) return nullptr;
    return d;
}

// All tracks: lines "<track> <encoded>"
template <class Arr>
std::string encodeAll(const Arr& samples, int tracks)
{
    std::string o;
    for (int t = 0; t < tracks; ++t)
        if (samples[t]) { o += std::to_string(t) + " " + encode(*samples[t]) + "\n"; }
    return o;
}
// calls fn(track, SamplePtr) for each line of encodeAll's output (also for a single "<track> <encoded>" command;
// "<track>" alone = remove the sample)
template <class Fn>
void decodeAll(const char* text, Fn fn)
{
    const char* s = text;
    while (s && *s) {
        const char* e = std::strchr(s, '\n');
        const size_t len = e ? (size_t)(e - s) : std::strlen(s);
        if (len > 0) {
            char* end = nullptr;
            const long t = std::strtol(s, &end, 10);
            if (end && end != s) {
                const char* rest = end; while (*rest == ' ') ++rest;
                const size_t rl = len - (size_t)(rest - s);
                fn((int)t, rl > 0 ? decode(rest, rl) : SamplePtr());
            }
        }
        s = e ? e + 1 : nullptr;
    }
}

} // namespace smp
} // namespace kn
