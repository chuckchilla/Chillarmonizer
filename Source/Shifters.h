// Pitch shifters that read a shared input history. JUCE-free.
//   GrainShifter : delay-line grains (2 or 4). Works on chords; used for Poly, Low Latency and Vintage.
//   PsolaVoice   : pitch-synchronous overlap-add on a monophonic source. Keeps formants, can shift them.
#pragma once
#include "DspBlocks.h"

namespace charm
{
// Mono input history addressed by absolute sample index.
struct History
{
    std::vector<float> buf;
    int64_t mask = 0, t = 0;   // t = samples written so far

    void prepare (int minSize)
    {
        int64_t n = 1;
        while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        t = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    int64_t size() const noexcept { return mask + 1; }
    void write (float x) noexcept { buf[(size_t) (t & mask)] = x; ++t; }
    float at (int64_t i) const noexcept { return buf[(size_t) (i & mask)]; }

    // Hermite read at an absolute fractional position. Clamped so it never reads unwritten samples.
    float read (double pos) const noexcept
    {
        pos = std::min (pos, (double) (t - 3));
        pos = std::max (pos, (double) (t - size() + 4));
        const double fl = std::floor (pos);
        const int64_t i = (int64_t) fl;
        const float f = (float) (pos - fl);
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

// Grain window. sharp = 1 is Hann; larger values approach hard splices (the H910 glitch)
// while keeping two half-offset grains summing to exactly 1.
inline float grainWindow (float q, float sharp) noexcept
{
    const float s = std::sin (kPi * q);
    const float x = s * s;
    if (sharp <= 1.0f) return x;
    const float a = std::pow (x, sharp), b = std::pow (1.0f - x, sharp);
    return a / (a + b + 1.0e-20f);
}

struct GrainShifter
{
    float phase = 0.0f;
    void reset (float startPhase = 0.0f) { phase = startPhase; }

    // ratio: pitch ratio. win: grain length in samples. grains: 2 or 4. extraDelay: added read delay (samples).
    float process (const History& h, float ratio, float win, int grains, float sharp, float extraDelay) noexcept
    {
        phase = frac (phase + (1.0f - ratio) / win);
        const double now = (double) (h.t - 1);
        float out = 0.0f;
        if (grains <= 2)
        {
            for (int k = 0; k < 2; ++k)
            {
                const float q = frac (phase + 0.5f * (float) k);
                out += grainWindow (q, sharp) * h.read (now - (3.0 + extraDelay + q * win));
            }
            return out;
        }
        for (int k = 0; k < 4; ++k)
        {
            const float q = frac (phase + 0.25f * (float) k);
            const float s = std::sin (kPi * q);
            out += s * s * h.read (now - (3.0 + extraDelay + q * win));
        }
        return 0.5f * out;
    }
};

// Analysis marks shared by every PSOLA voice: one per input period, snapped to waveform peaks.
struct PsolaAnalysis
{
    double mark = 0.0;
    float period = 400.0f;
    bool voiced = false;
    int latency = 1200;      // samples between input and the newest usable mark
    float sr = 48000.0f;
    Rng rng { 777u };

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        latency = (int) (0.025f * sr);
        mark = 0.0;
    }

    // Call once per sample after writing the history.
    void update (const History& h, float periodSamples, bool isVoiced) noexcept
    {
        voiced = isVoiced;
        period = isVoiced ? std::clamp (periodSamples, 0.0007f * sr, 0.0167f * sr) : 0.006f * sr;
        const double limit = (double) (h.t - latency);
        if (mark < limit - 4.0 * 0.0167 * sr || mark > limit)
            mark = limit - period;
        while (mark + period <= limit)
        {
            double next = mark + period;
            if (voiced)
            {
                const int64_t lo = (int64_t) (next - 0.25 * period), hi = std::min ((int64_t) (next + 0.25 * period), h.t - 4);
                int64_t best = (int64_t) next;
                float bestV = -1.0e9f;
                for (int64_t i = lo; i <= hi; ++i)
                {
                    const float v = h.at (i);
                    if (v > bestV) { bestV = v; best = i; }
                }
                if ((double) best > mark + 0.5 * period) next = (double) best;
            }
            else
            {
                next += rng.next() * 0.3 * period;   // jitter so unvoiced sounds don't buzz at 1/period
            }
            mark = next;
        }
    }
};

struct PsolaVoice
{
    struct Grain
    {
        double centre = 0.0;
        float half = 0.0f, step = 1.0f, pos = 0.0f, gain = 0.0f;
        bool active = false;
    };
    std::array<Grain, 24> grains {};
    float countdown = 0.0f;

    void reset() { for (auto& g : grains) g.active = false; countdown = 0.0f; }

    // ratio: pitch ratio. formant: formant ratio (1 = preserved). offset: extra delay in samples (humanize / choir).
    float process (const History& h, const PsolaAnalysis& a, float ratio, float formant, float offset) noexcept
    {
        countdown -= 1.0f;
        if (countdown <= 0.0f)
        {
            const float P = a.period;
            const float synth = std::max (8.0f, P / std::max (0.05f, ratio));
            // Two-period grains (one period each side of the mark). When lowering the pitch the grains
            // stop overlapping; that gap is what makes the new, longer period.
            const float half = std::min (P / formant, ((float) a.latency - 4.0f) / formant);
            for (auto& g : grains)
                if (! g.active)
                {
                    g.active = true;
                    g.centre = a.mark - (double) offset;
                    g.half = std::max (4.0f, half);
                    g.step = formant;
                    g.pos = -g.half;
                    g.gain = std::min (1.0f, synth / g.half);
                    break;
                }
            countdown += synth;
        }

        float out = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float w = 0.5f + 0.5f * std::cos (kPi * g.pos / g.half);
            out += g.gain * w * h.read (g.centre + (double) (g.pos * g.step));
            g.pos += 1.0f;
            if (g.pos >= g.half) g.active = false;
        }
        return out;
    }
};
} // namespace charm
