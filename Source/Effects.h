// Freeze, octave generator + tracking synth, crystal echoes, shimmer reverb. JUCE-free.
#pragma once
#include "Shifters.h"

namespace charm
{
//==============================================================================
// Freeze: captures the last 350 ms and plays it back as a smooth, endless grain cloud.
class Freezer
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        len = (int) (0.35f * sr);
        hold.assign ((size_t) len, 0.0f);
        reset();
    }
    void reset() { std::fill (hold.begin(), hold.end(), 0.0f); gain = 0.0f; captured = false; wasOn = false; }

    float process (const History& h, bool on) noexcept
    {
        if (on && ! wasOn)
        {
            for (int i = 0; i < len; ++i) hold[(size_t) i] = h.at (h.t - len + i);
            // nothing worth holding if the capture is silent
            float rms = 0.0f;
            for (float v : hold) rms += v * v;
            rms = std::sqrt (rms / (float) len);
            captured = rms > 1.0e-5f;
            for (auto& g : grains) { g.pos = 0.0f; g.length = 0.0f; }
            for (int k = 0; k < 4; ++k) grains[(size_t) k].pos = (float) k * 0.25f;   // staggered
            for (int k = 0; k < 4; ++k) newGrain (grains[(size_t) k], (float) k * 0.25f);
        }
        wasOn = on;
        const float target = (on && captured) ? 1.0f : 0.0f;
        gain += (target - gain) * (target > gain ? 0.004f : 0.0006f);
        if (gain < 1.0e-4f) return 0.0f;

        float out = 0.0f;
        for (auto& g : grains)
        {
            const float q = g.pos;
            const float s = std::sin (kPi * q);
            const float idx = g.start + q * g.length;
            const int i0 = (int) idx;
            const float f = idx - (float) i0;
            const float a = hold[(size_t) std::clamp (i0, 0, len - 1)], b = hold[(size_t) std::clamp (i0 + 1, 0, len - 1)];
            out += s * s * (a + f * (b - a));
            g.pos += 1.0f / std::max (1.0f, g.length);
            if (g.pos >= 1.0f) newGrain (g, 0.0f);
        }
        return 0.5f * out * gain;
    }

    float getGain() const noexcept { return gain; }

private:
    struct Grain { float start = 0, length = 1, pos = 0; };
    void newGrain (Grain& g, float startPos) noexcept
    {
        g.length = (0.12f + 0.08f * rng.next01()) * sr;
        g.start = rng.next01() * std::max (0.0f, (float) len - g.length - 2.0f);
        g.pos = startPos;
    }

    float sr = 48000.0f;
    int len = 1;
    std::vector<float> hold;
    std::array<Grain, 4> grains {};
    Rng rng { 4242u };
    float gain = 0.0f;
    bool captured = false, wasOn = false;
};

//==============================================================================
// Polyphonic octaves (sub 2, sub 1, up 1, up 2) with attack swell, resonant filter and detune.
// Plus a monophonic synth voice that follows the detected pitch.
struct OctaveParams
{
    float level[4] { 0, 0, 0, 0 };   // -24, -12, +12, +24
    float attack = 0.0f, filterHz = 20000.0f, reso = 0.0f, detune = 0.0f;
    float synthLevel = 0.0f;
    int synthOctave = 0, synthWave = 0;   // octave -2..+1, wave 0 saw / 1 square / 2 sine
};

class OctaveBox
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        win = 0.06f * sr;
        for (int i = 0; i < 4; ++i) { shift[(size_t) i].reset (0.13f * (float) i); shiftDet[(size_t) i].reset (0.37f + 0.11f * (float) i); }
        fast.set (300.0f, sr); slow.set (20.0f, sr);
        synthEnv.set (25.0f, sr);
        reset();
    }

    void reset()
    {
        filter.reset(); synthFilter.reset();
        fast.reset(); slow.reset(); synthEnv.reset();
        swell = 1.0f; armed = true; synthPhase = 0.0f; synthGate = 0.0f; synthFreq = 0.0f;
    }

    // Call once per block.
    void setParams (const OctaveParams& p)
    {
        par = p;
        const float q = 0.707f + 7.0f * p.reso * p.reso;
        filter.set (p.filterHz, q, sr);
        synthFilter.set (std::min (p.filterHz, 9000.0f), 0.8f + 2.0f * p.reso, sr);
        swellCoef = p.attack <= 0.001f ? 1.0f : 1.0f - std::exp (-1.0f / (p.attack * 1.5f * sr * 0.25f));
    }

    bool active() const noexcept
    {
        return par.level[0] + par.level[1] + par.level[2] + par.level[3] > 1.0e-4f || par.synthLevel > 1.0e-4f;
    }

    // input: the live mono input (for envelopes). trackHz / voiced: from the pitch tracker.
    float process (const History& h, float input, float trackHz, bool voiced) noexcept
    {
        // onset detection for the attack swell
        const float a = std::fabs (input);
        const float f = fast.process (a), s = slow.process (a);
        if (armed && f > 2.2f * s + 2.0e-3f) { swell = par.attack > 0.001f ? 0.0f : 1.0f; armed = false; }
        if (! armed && f < 1.3f * s) armed = true;
        swell += (1.0f - swell) * swellCoef;

        static const float ratios[4] = { 0.25f, 0.5f, 2.0f, 4.0f };
        float oct = 0.0f;
        const float det = std::pow (2.0f, par.detune * 0.2f / 12.0f);   // up to 20 cents
        for (int i = 0; i < 4; ++i)
        {
            if (par.level[i] <= 1.0e-4f) continue;
            float v = shift[(size_t) i].process (h, ratios[i], win, 4, 1.0f, 0.0f);
            if (par.detune > 0.001f)
                v = 0.7f * v + 0.7f * par.detune * shiftDet[(size_t) i].process (h, ratios[i] * det, win * 1.13f, 4, 1.0f, 0.0f);
            oct += par.level[i] * v;
        }
        oct = filter.lp (oct) * swell;

        // tracking synth
        float syn = 0.0f;
        if (par.synthLevel > 1.0e-4f)
        {
            const float env = synthEnv.process (a);
            synthGate += ((voiced ? 1.0f : 0.0f) - synthGate) * 0.002f;
            if (voiced && trackHz > 0.0f)
            {
                const float target = trackHz * std::pow (2.0f, (float) par.synthOctave);
                synthFreq = synthFreq <= 0.0f ? target : synthFreq + (target - synthFreq) * 0.01f;
            }
            if (synthFreq > 0.0f)
            {
                const float dt = std::min (0.45f, synthFreq / sr);
                synthPhase = frac (synthPhase + dt);
                float w;
                if (par.synthWave == 2) w = std::sin (kTwoPi * synthPhase);
                else if (par.synthWave == 1) w = (synthPhase < 0.5f ? 1.0f : -1.0f) + blep (synthPhase, dt) - blep (frac (synthPhase + 0.5f), dt);
                else w = 2.0f * synthPhase - 1.0f - blep (synthPhase, dt);
                syn = synthFilter.lp (w) * std::min (1.0f, env * 3.0f) * synthGate * par.synthLevel * 0.6f;
            }
        }
        return oct + syn;
    }

private:
    static float blep (float t, float dt) noexcept
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float sr = 48000.0f, win = 2880.0f;
    OctaveParams par;
    std::array<GrainShifter, 4> shift, shiftDet;
    Svf filter, synthFilter;
    OnePole fast, slow, synthEnv;
    float swell = 1.0f, swellCoef = 1.0f;
    bool armed = true;
    float synthPhase = 0.0f, synthGate = 0.0f, synthFreq = 0.0f;
};

//==============================================================================
// Crystal echoes: stereo cross-fed delay where every repeat is pitch-shifted again (and optionally reversed).
class CrystalEcho
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& l : line) l.prepare ((int) (2.6f * sr) + 16);
        reset();
    }
    void reset()
    {
        for (auto& l : line) l.clear();
        for (auto& t : tone) t.reset();
        for (auto& hp : lowCut) hp.reset();
        phase[0] = 0.0f; phase[1] = 0.37f;
    }

    void setParams (float timeMs, float feedback, float pitchSemis, bool rev, float toneHz)
    {
        T = std::clamp (timeMs * 0.001f * sr, 64.0f, 2.5f * sr);
        fb = std::clamp (feedback, 0.0f, 0.98f);
        ratio = std::pow (2.0f, pitchSemis / 12.0f);
        reverse = rev;
        for (auto& t : tone) t.set (toneHz, sr);
        for (auto& hp : lowCut) hp.set (80.0f, sr);
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        float y[2];
        const float W = std::min (T, 0.12f * sr);
        for (int c = 0; c < 2; ++c)
        {
            const float Tc = c == 0 ? T : std::min (T * 1.05f, 2.55f * sr);
            const float speed = reverse ? (1.0f + ratio) : (1.0f - ratio);
            const bool shifting = reverse || std::fabs (ratio - 1.0f) > 1.0e-4f;
            if (! shifting) { y[c] = line[(size_t) c].read (Tc); continue; }
            phase[c] = frac (phase[c] + speed / W);
            float v = 0.0f;
            for (int k = 0; k < 2; ++k)
            {
                const float q = frac (phase[c] + 0.5f * (float) k);
                const float s = std::sin (kPi * q);
                v += s * s * line[(size_t) c].read (Tc - W + 3.0f + q * W);
            }
            y[c] = v;
        }
        const float in[2] = { inL, inR };
        for (int c = 0; c < 2; ++c)
        {
            float f = tone[(size_t) c].process (y[1 - c]);
            f -= lowCut[(size_t) c].process (f);
            line[(size_t) c].write (loopLimit (in[c] + fb * f));
        }
        outL = y[0];
        outR = y[1];
    }

private:
    float sr = 48000.0f, T = 1000.0f, fb = 0.0f, ratio = 1.0f;
    bool reverse = false;
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> tone, lowCut;
    float phase[2] { 0.0f, 0.37f };
};

//==============================================================================
// 8-line FDN hall with a pitch-shifted feedback path (shimmer).
class ShimmerReverb
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& l : lines) l.prepare ((int) (0.15f * sr) + 8);
        static const float preMs[2][4] = { { 4.7f, 3.6f, 12.7f, 9.3f }, { 5.3f, 3.1f, 11.3f, 8.9f } };
        for (int c = 0; c < 2; ++c)
            for (int a = 0; a < 4; ++a) { pre[(size_t) c][(size_t) a].prepare ((int) (preMs[c][a] * 0.001f * sr)); pre[(size_t) c][(size_t) a].g = 0.7f; }
        shimmerHist.prepare ((int) (0.2f * sr));
        currentDecay = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& d : damp) d.reset();
        for (auto& side : pre) for (auto& a : side) a.clear();
        shimmerHist.clear();
        shimmerShift.reset();
        shimmerHp.reset();
        shimmerFb = 0.0f;
    }

    void setParams (float decay, float shimmer, float shimmerSemis)
    {
        decay = std::clamp (decay, 0.0f, 1.0f);
        shimmerAmt = std::clamp (shimmer, 0.0f, 1.0f);
        shimmerRatio = std::pow (2.0f, shimmerSemis / 12.0f);
        shimmerHp.set (300.0f, sr);
        if (std::fabs (decay - currentDecay) > 1.0e-4f)
        {
            currentDecay = decay;
            static const float baseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 61.1f, 67.9f, 73.1f };
            const float rt = 0.5f * std::pow (24.0f, decay);
            for (int i = 0; i < 8; ++i)
            {
                len[i] = std::max (8, (int) (baseMs[i] * 1.4f * 0.001f * sr));
                gain[i] = std::pow (10.0f, -3.0f * (float) len[i] / (rt * sr));
                damp[(size_t) i].set (6500.0f, sr);
            }
        }
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        float l = inL + shimmerFb, r = inR + shimmerFb;
        for (auto& a : pre[0]) l = a.process (l);
        for (auto& a : pre[1]) r = a.process (r);

        float o[8];
        for (int i = 0; i < 8; ++i) o[i] = damp[(size_t) i].process (lines[(size_t) i].readInt (len[i]));
        float m[8];
        std::copy (o, o + 8, m);
        for (int h = 1; h < 8; h *= 2)
            for (int i = 0; i < 8; i += 2 * h)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = m[j], b = m[j + h];
                    m[j] = a + b; m[j + h] = a - b;
                }
        for (int i = 0; i < 8; ++i)
            lines[(size_t) i].write (loopLimit (((i & 1) ? r : l) * 0.35f + gain[i] * m[i] * 0.35355339f));

        outL = 0.5f * (o[0] - o[2] + o[4] - o[6]);
        outR = 0.5f * (o[1] - o[3] + o[5] - o[7]);

        if (shimmerAmt > 1.0e-4f)
        {
            shimmerHist.write (0.5f * (outL + outR));
            const float s = shimmerShift.process (shimmerHist, shimmerRatio, 0.08f * sr, 2, 1.0f, 0.0f);
            shimmerFb = loopLimit (shimmerHp.hp (s) * shimmerAmt * 0.75f);
        }
        else shimmerFb = 0.0f;
    }

private:
    struct HighPass   // one-pole high-pass
    {
        OnePole lp;
        void set (float fc, float s) { lp.set (fc, s); }
        void reset() { lp.reset(); }
        float hp (float x) noexcept { return x - lp.process (x); }
    };

    float sr = 48000.0f;
    std::array<DelayLine, 8> lines;
    std::array<OnePole, 8> damp;
    int len[8] {};
    float gain[8] {};
    std::array<std::array<Allpass, 4>, 2> pre;
    History shimmerHist;
    GrainShifter shimmerShift;
    HighPass shimmerHp;
    float shimmerFb = 0.0f, shimmerAmt = 0.0f, shimmerRatio = 2.0f, currentDecay = -1.0f;
};
} // namespace charm
