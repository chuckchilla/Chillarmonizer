// Music theory helpers: scales, diatonic / chord-tone harmony, key detection, chord detection. JUCE-free.
#pragma once
#include "DspBlocks.h"

namespace charm
{
struct Scale { const char* name; int count; int steps[12]; };

inline const std::array<Scale, 14>& scales()
{
    static const std::array<Scale, 14> s { {
        { "Major",            7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural Minor",    7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor",   7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",    7, { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorian",           7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",         7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",           7, { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",       7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",          7, { 0, 1, 3, 5, 6, 8, 10 } },
        { "Major Pentatonic", 5, { 0, 2, 4, 7, 9 } },
        { "Minor Pentatonic", 5, { 0, 3, 5, 7, 10 } },
        { "Blues",            6, { 0, 3, 5, 6, 7, 10 } },
        { "Whole Tone",       6, { 0, 2, 4, 6, 8, 10 } },
        { "Chromatic",       12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    } };
    return s;
}

inline int floorDiv (int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
inline int mod12 (int n) { return ((n % 12) + 12) % 12; }

// Note `steps` scale degrees away from `note` in the given key. Notes outside the scale snap down first.
inline int diatonicTarget (int note, int root, const Scale& s, int steps)
{
    const int rel = note - root;
    const int oct = floorDiv (rel, 12);
    const int pc = rel - oct * 12;
    int idx = 0;
    for (int i = 0; i < s.count; ++i)
        if (s.steps[i] <= pc) idx = i;
    const int ti = idx + steps;
    const int o2 = floorDiv (ti, s.count);
    const int ii = ti - o2 * s.count;
    return root + (oct + o2) * 12 + s.steps[ii];
}

// Nearest in-scale note to a (fractional) MIDI pitch.
inline int nearestScaleNote (float midi, int root, const Scale& s)
{
    const int base = (int) std::lround (midi);
    int best = base;
    float bestDist = 1.0e9f;
    for (int d = -6; d <= 6; ++d)
    {
        const int n = base + d;
        const int pc = mod12 (n - root);
        bool in = false;
        for (int i = 0; i < s.count; ++i) in = in || s.steps[i] == pc;
        if (in && std::fabs ((float) n - midi) < bestDist) { bestDist = std::fabs ((float) n - midi); best = n; }
    }
    return best;
}

// Chord-tone harmony: steps = 0 snaps to the nearest chord tone, +k = k-th chord tone above, -k = below.
// mask: bit i set = pitch class i is in the chord. Returns -1 if the chord is empty.
inline int chordTarget (int note, int mask, int steps)
{
    if ((mask & 0xFFF) == 0) return -1;
    auto inChord = [mask] (int n) { return ((mask >> mod12 (n)) & 1) != 0; };
    if (steps == 0)
    {
        for (int d = 0; d <= 6; ++d)
        {
            if (inChord (note + d)) return note + d;
            if (inChord (note - d)) return note - d;
        }
        return note;
    }
    const int dir = steps > 0 ? 1 : -1;
    int count = 0, n = note;
    for (int guard = 0; guard < 96; ++guard)
    {
        n += dir;
        if (inChord (n) && ++count == std::abs (steps)) return n;
    }
    return note;
}

//==============================================================================
// Krumhansl-Schmuckler key finding from a decaying pitch-class histogram.
class KeyDetector
{
public:
    void reset() { hist.fill (0.0f); root = 0; minor = false; score = -1.0f; hasKey = false; }

    // Call per analysis hop with the detected note and a weight (confidence * seconds).
    void add (int midiNote, float weight, float decay)
    {
        for (auto& h : hist) h *= decay;
        hist[(size_t) mod12 (midiNote)] += weight;
        if (++counter >= 16) { counter = 0; evaluate(); }
    }

    int getRoot() const { return root; }
    bool isMinor() const { return minor; }
    bool known() const { return hasKey; }

    void evaluate()
    {
        static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float mnr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        float total = 0.0f;
        for (float h : hist) total += h;
        if (total < 0.5f) return;
        float bestScore = -2.0f;
        int bestRoot = 0;
        bool bestMinor = false;
        for (int m = 0; m < 2; ++m)
            for (int r = 0; r < 12; ++r)
            {
                const float s = correlate (m == 0 ? maj : mnr, r);
                if (s > bestScore) { bestScore = s; bestRoot = r; bestMinor = m == 1; }
            }
        const float current = hasKey ? correlate (minor ? mnr : maj, root) : -2.0f;
        if (! hasKey || bestScore > current + 0.03f)
        {
            root = bestRoot; minor = bestMinor; hasKey = true;
        }
        score = bestScore;
    }

private:
    float correlate (const float* profile, int r) const
    {
        float mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += hist[(size_t) mod12 (i + r)]; my += profile[i]; }
        mx /= 12.0f; my /= 12.0f;
        float sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float x = hist[(size_t) mod12 (i + r)] - mx, y = profile[i] - my;
            sxy += x * y; sxx += x * x; syy += y * y;
        }
        return sxy / std::sqrt (sxx * syy + 1.0e-12f);
    }

    std::array<float, 12> hist {};
    int root = 0, counter = 0;
    bool minor = false, hasKey = false;
    float score = -1.0f;
};

//==============================================================================
// Chord detection on a sidechain (e.g. a guitar steering vocal harmonies): Goertzel chroma + templates.
enum ChordType { ChMaj, ChMin, Ch7, ChMaj7, ChMin7, ChDim, ChSus4, ChSus2, ChNumTypes };

inline const char* chordTypeName (int t)
{
    static const char* n[] = { "", "m", "7", "maj7", "m7", "dim", "sus4", "sus2" };
    return n[std::clamp (t, 0, (int) ChNumTypes - 1)];
}

inline int chordMask (int root, int type)
{
    static const int iv[ChNumTypes][4] = { { 0, 4, 7, -1 }, { 0, 3, 7, -1 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 },
                                           { 0, 3, 7, 10 }, { 0, 3, 6, -1 }, { 0, 5, 7, -1 }, { 0, 2, 7, -1 } };
    int m = 0;
    for (int i = 0; i < 4; ++i)
        if (iv[type][i] >= 0) m |= 1 << mod12 (root + iv[type][i]);
    return m;
}

class ChordDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        decim = std::max (1, (int) std::lround (sampleRate / 6000.0));
        dsr = sr / (float) decim;
        aa1.set (0.4f * dsr, 0.54f, sr);
        aa2.set (0.4f * dsr, 1.31f, sr);
        buf.assign (kN, 0.0f);
        for (int i = 0; i < kN; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) (kN - 1));
        for (int k = 0; k < kNotes; ++k)
        {
            const float f = 440.0f * std::pow (2.0f, (float) (kLowNote + k - 69) / 12.0f);
            coef[(size_t) k] = 2.0f * std::cos (kTwoPi * f / dsr);
        }
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        chroma.fill (0.0f);
        aa1.reset(); aa2.reset();
        pos = 0; decimCount = 0; hopCount = 0;
        mask = 0; root = -1; type = 0;
    }

    void push (float x) noexcept
    {
        const float y = aa2.lp (aa1.lp (x));
        if (++decimCount < decim) return;
        decimCount = 0;
        buf[(size_t) pos] = y;
        pos = (pos + 1) % kN;
        if (++hopCount >= kN / 4) { hopCount = 0; analyse(); }
    }

    int getMask() const { return mask; }
    int getRoot() const { return root; }
    int getType() const { return type; }

private:
    void analyse() noexcept
    {
        std::array<float, 12> c {};
        double energy = 0.0;
        for (int i = 0; i < kN; ++i) energy += (double) buf[(size_t) i] * buf[(size_t) i];
        if (energy / kN < 1.0e-7) return;   // silence: hold the last chord

        for (int k = 0; k < kNotes; ++k)
        {
            float s1 = 0, s2 = 0;
            const float cf = coef[(size_t) k];
            for (int i = 0; i < kN; ++i)
            {
                const float s0 = buf[(size_t) ((pos + i) % kN)] * window[(size_t) i] + cf * s1 - s2;
                s2 = s1; s1 = s0;
            }
            const float power = s1 * s1 + s2 * s2 - cf * s1 * s2;
            c[(size_t) mod12 (kLowNote + k)] += std::sqrt (std::max (0.0f, power));
        }
        float mx = 0.0f;
        for (float v : c) mx = std::max (mx, v);
        if (mx <= 0.0f) return;
        for (size_t i = 0; i < 12; ++i)
            chroma[i] = 0.5f * chroma[i] + 0.5f * (c[i] / mx);

        float bestScore = -1.0e9f;
        int bestRoot = 0, bestType = 0;
        for (int t = 0; t < ChNumTypes; ++t)
            for (int r = 0; r < 12; ++r)
            {
                const int m = chordMask (r, t);
                float in = 0, out = 0;
                int nIn = 0;
                for (int i = 0; i < 12; ++i)
                {
                    if ((m >> i) & 1) { in += chroma[(size_t) i]; ++nIn; }
                    else out += chroma[(size_t) i];
                }
                float score = in / (float) nIn - out / (float) (12 - nIn);
                if (t == ChMaj || t == ChMin) score += 0.04f;            // prefer plain triads when close
                score += 0.05f * chroma[(size_t) r];                     // root should be strong
                if (score > bestScore) { bestScore = score; bestRoot = r; bestType = t; }
            }
        if (bestScore > 0.15f)
        {
            root = bestRoot; type = bestType; mask = chordMask (bestRoot, bestType);
        }
    }

    static constexpr int kN = 1024, kLowNote = 40, kNotes = 48;
    float sr = 44100.0f, dsr = 6000.0f;
    int decim = 8;
    Svf aa1, aa2;
    std::vector<float> buf;
    std::array<float, kN> window {};
    std::array<float, kNotes> coef {};
    std::array<float, 12> chroma {};
    int pos = 0, decimCount = 0, hopCount = 0;
    int mask = 0, root = -1, type = 0;
};
} // namespace charm
