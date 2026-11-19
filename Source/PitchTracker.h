// Monophonic pitch tracker (YIN on a ~12 kHz decimated copy of the input). JUCE-free.
#pragma once
#include "DspBlocks.h"

namespace charm
{
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        decim = std::max (1, (int) std::lround (sampleRate / 12000.0));
        dsr = sr / (float) decim;
        aa1.set (0.42f * dsr, 0.54f, sr);   // 4th-order Butterworth-ish anti-alias
        aa2.set (0.42f * dsr, 1.31f, sr);
        win = (int) (0.036f * dsr);
        maxLag = (int) (dsr / kMinHz);
        minLag = std::max (2, (int) (dsr / kMaxHz));
        hop = std::max (16, (int) (0.006f * dsr));
        int n = 1;
        while (n < win + maxLag + 4) n <<= 1;
        ring.assign ((size_t) n, 0.0f);
        ringMask = n - 1;
        lin.assign ((size_t) (win + maxLag + 2), 0.0f);
        diff.assign ((size_t) (maxLag + 2), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        aa1.reset(); aa2.reset();
        ringPos = 0; decimCount = 0; hopCount = 0; filled = 0;
        freq = 0.0f; confidence = 0.0f; level = 0.0f;
    }

    void push (float x) noexcept
    {
        const float y = aa2.lp (aa1.lp (x));
        if (++decimCount < decim) return;
        decimCount = 0;
        ring[(size_t) ringPos] = y;
        ringPos = (ringPos + 1) & ringMask;
        if (filled < win + maxLag) ++filled;
        if (++hopCount >= hop)
        {
            hopCount = 0;
            if (filled >= win + maxLag) analyse();
        }
    }

    float getFrequency() const noexcept  { return freq; }        // Hz of the last periodic frame (held through gaps)
    float getConfidence() const noexcept { return confidence; }  // 0..1 (1 - YIN aperiodicity), 0 when silent
    float getLevel() const noexcept      { return level; }       // RMS of the analysis window
    float getMidi() const noexcept       { return freq > 0.0f ? 69.0f + 12.0f * std::log2 (freq / 440.0f) : -1.0f; }

    static constexpr float kMinHz = 60.0f, kMaxHz = 1500.0f;

private:
    void analyse() noexcept
    {
        const int N = win + maxLag;
        double energy = 0.0;
        for (int j = 0; j < N; ++j)
        {
            const float v = ring[(size_t) ((ringPos - N + j) & ringMask)];
            lin[(size_t) j] = v;
        }
        for (int j = 0; j < win; ++j) energy += (double) lin[(size_t) j] * lin[(size_t) j];
        level = (float) std::sqrt (energy / (double) win);
        if (level < 2.0e-4f) { confidence = 0.0f; return; }   // below about -74 dBFS: treat as silence

        // YIN difference function with cumulative-mean normalisation
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double d = 0.0;
            const float* a = lin.data();
            const float* b = lin.data() + tau;
            for (int j = 0; j < win; ++j)
            {
                const float e = a[j] - b[j];
                d += (double) e * e;
            }
            running += d;
            diff[(size_t) tau] = running > 0.0 ? (float) (d * (double) tau / running) : 1.0f;
        }

        int best = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxLag && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 0)
        {
            best = minLag;
            for (int tau = minLag; tau < maxLag; ++tau)
                if (diff[(size_t) tau] < diff[(size_t) best]) best = tau;
        }

        const float dmin = diff[(size_t) best];
        confidence = std::clamp (1.0f - dmin, 0.0f, 1.0f);

        // parabolic interpolation around the minimum
        float t = (float) best;
        if (best > 1 && best < maxLag)
        {
            const float s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const float den = s0 - 2.0f * s1 + s2;
            if (std::fabs (den) > 1.0e-9f) t += std::clamp (0.5f * (s0 - s2) / den, -0.5f, 0.5f);
        }
        if (confidence > 0.5f)
            freq = dsr / t;
    }

    float sr = 44100.0f, dsr = 11025.0f;
    int decim = 4, win = 400, maxLag = 180, minLag = 8, hop = 64;
    Svf aa1, aa2;
    std::vector<float> ring, lin, diff;
    int ringMask = 0, ringPos = 0, decimCount = 0, hopCount = 0, filled = 0;
    float freq = 0.0f, confidence = 0.0f, level = 0.0f;
};
} // namespace charm
