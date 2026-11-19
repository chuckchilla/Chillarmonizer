// Small DSP building blocks shared by the Chillarmonizer engine. JUCE-free.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace charm
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float frac (float x) { return x - std::floor (x); }

struct Rng
{
    uint32_t s;
    Rng() : s (0x9E3779B9u) {}
    explicit Rng (uint32_t seed) : s (seed ? seed : 1u) {}
    float next() noexcept   // -1 .. 1
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
    float next01() noexcept { return 0.5f + 0.5f * next(); }
};

struct SmoothRandom   // cosine-interpolated sample & hold
{
    float phase = 1.0f, a = 0.0f, b = 0.0f;
    Rng rng;
    SmoothRandom() : rng (1234u) {}
    explicit SmoothRandom (uint32_t seed) : rng (seed) {}
    float next (float inc) noexcept
    {
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            a = b;
            b = rng.next();
        }
        const float t = 0.5f - 0.5f * std::cos (kPi * phase);
        return a + (b - a) * t;
    }
};

struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int minSize)
    {
        int n = 1;
        while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    int size() const { return mask + 1; }
    void write (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float readInt (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }

    // Hermite-interpolated read, d samples in the past (d >= 3).
    float read (float d) const noexcept
    {
        const int di = (int) d;
        const float f = 1.0f - (d - (float) di);
        const int i = w - di - 1;
        const float xm1 = buf[(size_t) ((i - 1) & mask)];
        const float x0  = buf[(size_t) (i & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)];
        const float x2  = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

struct Svf   // Cytomic TPT state-variable filter
{
    float k = 1.414f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set (float fc, float q, float sr) noexcept
    {
        fc = std::clamp (fc, 10.0f, 0.45f * sr);
        const float g = std::tan (kPi * fc / sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    void tick (float x, float& lpOut, float& bpOut, float& hpOut) noexcept
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lpOut = v2; bpOut = v1; hpOut = x - k * v1 - v2;
    }
    float lp (float x) noexcept { float l, b, h; tick (x, l, b, h); return l; }
    float hp (float x) noexcept { float l, b, h; tick (x, l, b, h); return h; }
    float bpNorm (float x) noexcept { float l, b, h; tick (x, l, b, h); return b * k; }   // unity peak
};

struct OnePole
{
    float a = 1.0f, y = 0.0f;
    void set (float fc, float sr) noexcept { a = 1.0f - std::exp (-kTwoPi * std::min (fc, 0.49f * sr) / sr); }
    void reset() { y = 0; }
    float process (float x) noexcept { y += a * (x - y); return y; }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setPeak (float fc, float q, float gainDb, float sr) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = kTwoPi * fc / sr, cw = std::cos (w0), alpha = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0; b1 = -2.0f * cw / a0; b2 = (1.0f - alpha * A) / a0;
        a1 = -2.0f * cw / a0;         a2 = (1.0f - alpha / A) / a0;
    }
    void reset() { z1 = z2 = 0; }
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Schroeder allpass (delay-line based).
struct Allpass
{
    std::vector<float> buf;
    int pos = 0, len = 1;
    float g = 0.62f;
    void prepare (int maxLen) { buf.assign ((size_t) std::max (1, maxLen), 0.0f); len = (int) buf.size(); pos = 0; }
    void setLength (int l) { len = std::clamp (l, 1, (int) buf.size()); if (pos >= len) pos = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    float process (float x) noexcept
    {
        const float d = buf[(size_t) pos];
        const float y = d - g * x;
        buf[(size_t) pos] = x + g * y;
        if (++pos >= len) pos = 0;
        return y;
    }
};

// First-order allpass (phasers, spring dispersion).
struct Allpass1
{
    float a = 0, x1 = 0, y1 = 0;
    static float coef (float fc, float sr) noexcept
    {
        const float t = std::tan (kPi * std::clamp (fc, 10.0f, 0.45f * sr) / sr);
        return (t - 1.0f) / (t + 1.0f);
    }
    void reset() { x1 = y1 = 0; }
    float process (float x) noexcept
    {
        const float y = a * x + x1 - a * y1;
        x1 = x; y1 = y;
        return y;
    }
};

// tanh with first-order antiderivative anti-aliasing (cuts drive aliasing without oversampling).
struct AdaaTanh
{
    float x1 = 0, F1 = 0;
    static float logCosh (float x) noexcept
    {
        const float a = std::fabs (x);
        return a + std::log1p (std::exp (-2.0f * a)) - 0.69314718f;
    }
    void reset() { x1 = 0; F1 = 0; }
    float process (float x) noexcept
    {
        const float F = logCosh (x);
        const float d = x - x1;
        const float y = std::fabs (d) < 1.0e-4f ? std::tanh (0.5f * (x + x1)) : (F - F1) / d;
        x1 = x; F1 = F;
        return y;
    }
};

// Two-grain delay-line pitch shifter.
struct PitchShifter
{
    DelayLine line;
    float phase = 0.0f, win = 2048.0f;
    void prepare (float sr) { win = 0.05f * sr; line.prepare ((int) win + 16); phase = 0; }
    void clear() { line.clear(); phase = 0; }
    float process (float x, float ratio) noexcept
    {
        line.write (x);
        phase = frac (phase + (1.0f - ratio) / win);
        float out = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            const float q = frac (phase + 0.5f * (float) k);
            const float s = std::sin (kPi * q);
            out += s * s * line.read (3.0f + q * win);
        }
        return out;
    }
};

// Soft ceiling for feedback loops: transparent below 0.7, never above 1.0.
inline float loopLimit (float x) noexcept
{
    const float a = std::fabs (x);
    if (a <= 0.7f) return x;
    return std::copysign (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f), x);
}

inline float lfoShape (int shape, float ph) noexcept
{
    ph = frac (ph);
    switch (shape)
    {
        case 1:  return 1.0f - 4.0f * std::fabs (ph - 0.5f);                 // triangle
        case 3:  return std::tanh (8.0f * std::sin (kTwoPi * ph));          // soft square
        default: return std::sin (kTwoPi * ph);                              // sine (random handled by caller)
    }
}

} // namespace charm
