// Offline checks for the Chillarmonizer engine (no JUCE, no DAW).
#include "HarmEngine.h"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace charm;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static std::vector<float> saw (float f, int n, float sr, float amp = 0.4f)
{
    std::vector<float> x ((size_t) n, 0.0f);
    for (int k = 1; k <= 12 && (float) k * f < sr / 2.5f; ++k)
        for (int i = 0; i < n; ++i)
            x[(size_t) i] += amp * std::sin (kTwoPi * (float) k * f * (float) i / sr) / (float) k;
    return x;
}

static std::vector<float> sine (float f, int n, float sr, float amp = 0.5f)
{
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = amp * std::sin (kTwoPi * f * (float) i / sr);
    return x;
}

static float pitchOf (const std::vector<float>& x, size_t a, size_t b, float sr)
{
    PitchTracker t;
    t.prepare (sr);
    for (size_t i = a; i < std::min (b, x.size()); ++i) t.push (x[i]);
    return t.getConfidence() > 0.5f ? t.getFrequency() : 0.0f;
}

static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; b = std::min (b, v.size());
    for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return b > a ? std::sqrt (s / (double) (b - a)) : 0.0;
}

static double brightness (const std::vector<float>& v, size_t a, size_t b)
{
    double e = 0, d = 0;
    for (size_t i = a + 1; i < b; ++i) { e += (double) v[i] * v[i]; d += (double) (v[i] - v[i - 1]) * (v[i] - v[i - 1]); }
    return e > 0 ? d / e : 0.0;
}

static void run (HarmEngine& e, std::vector<float>& l, std::vector<float>& r, const EngineParams& p, int block = 256,
                 const std::vector<float>* sc = nullptr, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, l.size());
    for (size_t pos = from; pos < to; pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, to - pos);
        float* ptrs[2] = { l.data() + pos, r.data() + pos };
        const float* scp[2] = { sc ? sc->data() + pos : nullptr, sc ? sc->data() + pos : nullptr };
        e.process (ptrs, 2, sc ? scp : nullptr, sc ? 2 : 0, n, p);
    }
}

static EngineParams base (int engine)
{
    EngineParams p;
    p.engine = engine;
    p.mode = ModeFixed;
    for (auto& v : p.voice) v.on = false;
    p.voice[0] = { true, 7.0f, 0, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    p.humanize = 0.0f;
    p.dryDb = -60.0f;
    p.wetDb = 0.0f;
    return p;
}

// Render `input` through the engine and return the detected output pitch from 0.5 s to the end.
static float renderPitch (const std::vector<float>& input, const EngineParams& p, float sr,
                          const std::vector<float>* sc = nullptr, int heldNote = -1)
{
    auto e = std::make_unique<HarmEngine>();
    e->prepare (sr);
    if (heldNote >= 0) e->noteOn (heldNote);
    auto l = input, r = input;
    run (*e, l, r, p, 256, sc);
    return pitchOf (l, (size_t) (0.5f * sr), l.size(), sr);
}

static bool near (float got, float want, float tolPct) { return got > 0.0f && std::fabs (got - want) <= want * tolPct * 0.01f; }
static const char* engName (int e) { static const char* n[] = { "Formant", "Poly", "LowLatency", "Vintage" }; return n[e]; }

int main()
{
    const float sr = 48000.0f;
    const int len = (int) (1.2f * sr);

    std::printf ("Pitch tracker\n");
    for (float rate : { 44100.0f, 48000.0f, 96000.0f })
        for (float f : { 82.41f, 110.0f, 220.0f, 440.0f, 880.0f })
        {
            const auto x = sine (f, (int) (0.4f * rate), rate);
            const float got = pitchOf (x, 0, x.size(), rate);
            CHECK (near (got, f, 1.0f), "sine %.1f Hz at %.0f: got %.2f", f, rate, got);
        }
    {
        const auto x = saw (196.0f, (int) (0.4f * sr), sr);
        const float got = pitchOf (x, 0, x.size(), sr);
        CHECK (near (got, 196.0f, 1.0f), "sawtooth 196 Hz: got %.2f (octave error?)", got);
    }

    std::printf ("Theory\n");
    const auto& maj = scales()[0];
    CHECK (diatonicTarget (64, 0, maj, 2) == 67, "E + 3rd in C = G");
    CHECK (diatonicTarget (62, 0, maj, 2) == 65, "D + 3rd in C = F");
    CHECK (diatonicTarget (65, 0, maj, 2) == 69, "F + 3rd in C = A");
    CHECK (diatonicTarget (71, 0, maj, 2) == 74, "B + 3rd in C = D");
    CHECK (diatonicTarget (60, 0, maj, 7) == 72, "C + octave");
    CHECK (diatonicTarget (60, 0, maj, -3) == 55, "C - 4th = G below, got %d", diatonicTarget (60, 0, maj, -3));
    CHECK (diatonicTarget (69, 9, scales()[1], 2) == 72, "A + 3rd in A minor = C");
    CHECK (diatonicTarget (64, 0, scales()[9], 1) == 67, "E + 1 step in C major pentatonic = G");
    const int cmaj = chordMask (0, ChMaj);
    CHECK (chordTarget (60, cmaj, 1) == 64 && chordTarget (64, cmaj, 1) == 67 && chordTarget (67, cmaj, 1) == 72, "chord tones up");
    CHECK (chordTarget (60, cmaj, -1) == 55, "chord tone below C = G");
    CHECK (chordTarget (61, cmaj, 0) == 60 && chordTarget (62, cmaj, 0) == 64, "chord snap");
    CHECK (chordTarget (60, 0, 1) == -1, "empty chord");
    CHECK (nearestScaleNote (61.4f, 0, maj) == 62, "nearest scale note");

    std::printf ("Key detection\n");
    {
        KeyDetector k; k.reset();
        const int notes[] = { 60, 64, 67, 62, 65, 69, 71, 60, 67, 64, 72, 60 };
        for (int rep = 0; rep < 20; ++rep) for (int n : notes) k.add (n, 0.5f, 1.0f);
        k.evaluate();
        CHECK (k.getRoot() == 0 && ! k.isMinor(), "C major phrase -> C major (got %d %s)", k.getRoot(), k.isMinor() ? "minor" : "major");
        KeyDetector m; m.reset();
        const int mn[] = { 57, 60, 64, 57, 62, 65, 64, 60, 57, 59, 64, 57, 69 };
        for (int rep = 0; rep < 20; ++rep) for (int n : mn) m.add (n, 0.5f, 1.0f);
        m.evaluate();
        CHECK (m.getRoot() == 9 && m.isMinor(), "A minor phrase -> A minor (got %d %s)", m.getRoot(), m.isMinor() ? "minor" : "major");
    }

    std::printf ("Chord detection\n");
    auto chordSignal = [sr] (std::initializer_list<float> freqs)
    {
        std::vector<float> s ((size_t) sr, 0.0f);
        for (float f : freqs) { auto x = saw (f, (int) sr, sr, 0.15f); for (size_t i = 0; i < s.size(); ++i) s[i] += x[i]; }
        return s;
    };
    {
        struct Case { std::vector<float> f; int root, type; const char* name; };
        const Case cases[] = { { { 261.63f, 329.63f, 392.0f }, 0, ChMaj, "C" }, { { 220.0f, 261.63f, 329.63f }, 9, ChMin, "Am" },
                               { { 146.83f, 185.0f, 220.0f, 293.66f }, 2, ChMaj, "D (open voicing)" } };
        for (const auto& c : cases)
        {
            std::vector<float> s ((size_t) sr, 0.0f);
            for (float f : c.f) { auto x = saw (f, (int) sr, sr, 0.15f); for (size_t i = 0; i < s.size(); ++i) s[i] += x[i]; }
            ChordDetector d; d.prepare (sr);
            for (float v : s) d.push (v);
            CHECK (d.getRoot() == c.root && d.getType() == c.type, "%s chord: got root %d type %d", c.name, d.getRoot(), d.getType());
        }
    }

    std::printf ("Fixed intervals, every engine\n");
    const auto in220 = saw (220.0f, len, sr);
    for (int eng = 0; eng < 4; ++eng)
    {
        auto p = base (eng);
        float got = renderPitch (in220, p, sr);
        CHECK (near (got, 329.63f, 1.5f), "%s +7 st: got %.2f, want 329.63", engName (eng), got);
        p.voice[0].interval = -12.0f;
        got = renderPitch (in220, p, sr);
        CHECK (near (got, 110.0f, 1.5f), "%s -12 st: got %.2f, want 110", engName (eng), got);
        p.voice[0].interval = 12.0f;
        got = renderPitch (in220, p, sr);
        CHECK (near (got, 440.0f, 1.5f), "%s +12 st: got %.2f, want 440", engName (eng), got);
    }

    std::printf ("Diatonic, MIDI and sidechain modes\n");
    const auto inE4 = saw (329.63f, len, sr);
    for (int eng : { (int) EngFormant, (int) EngPoly })
    {
        auto p = base (eng);
        p.mode = ModeDiatonic; p.keyRoot = 0; p.scale = 0; p.voice[0].degree = 2;
        const float got = renderPitch (inE4, p, sr);
        CHECK (near (got, 392.0f, 1.5f), "%s diatonic E4 + 3rd in C: got %.2f, want 392 (G4)", engName (eng), got);
    }
    {
        auto p = base (EngFormant);
        p.mode = ModeDiatonic; p.keyRoot = 9; p.scale = 1; p.voice[0].degree = 2;
        const float got = renderPitch (inE4, p, sr);
        CHECK (near (got, 392.0f, 1.5f), "diatonic E4 + 3rd in A minor: got %.2f, want 392 (G4)", got);
    }
    {
        auto p = base (EngFormant);
        p.mode = ModeMidiNotes;
        const auto inC4 = saw (261.63f, len, sr);
        const float got = renderPitch (inC4, p, sr, nullptr, 67);
        CHECK (near (got, 392.0f, 1.5f), "MIDI Notes: C4 in, G4 held: got %.2f, want 392", got);
        p.mode = ModeMidiChord; p.voice[0].degree = 1;
        auto e = std::make_unique<HarmEngine>(); e->prepare (sr);
        e->noteOn (57); e->noteOn (60); e->noteOn (64);   // A minor
        auto l = inC4, r = inC4;
        run (*e, l, r, p);
        const float g2 = pitchOf (l, (size_t) (0.5f * sr), l.size(), sr);
        CHECK (near (g2, 329.63f, 1.5f), "MIDI Chord Am: C4 + next chord tone = E4: got %.2f", g2);
        auto silentVoice = base (EngFormant);
        silentVoice.mode = ModeMidiNotes;
        auto e2 = std::make_unique<HarmEngine>(); e2->prepare (sr);
        auto l2 = inC4, r2 = inC4;
        run (*e2, l2, r2, silentVoice);
        CHECK (rms (l2, (size_t) (0.6f * sr), l2.size()) < 1.0e-3, "MIDI Notes with no keys held is silent");
    }
    {
        const auto scC = chordSignal ({ 261.63f, 329.63f, 392.0f });
        std::vector<float> sc ((size_t) len, 0.0f);
        for (size_t i = 0; i < sc.size(); ++i) sc[i] = scC[i % scC.size()];
        auto p = base (EngFormant);
        p.mode = ModeSidechainChord; p.voice[0].degree = 1;
        const float got = renderPitch (inE4, p, sr, &sc);
        CHECK (near (got, 392.0f, 1.5f), "Sidechain C chord: E4 + next chord tone = G4: got %.2f", got);
    }

    std::printf ("Formant\n");
    {
        auto p = base (EngFormant);
        p.voice[0].interval = 0.0f;
        auto e0 = std::make_unique<HarmEngine>(); e0->prepare (sr);
        auto l0 = in220, r0 = in220; run (*e0, l0, r0, p);
        p.voice[0].formant = 6.0f;
        auto e1 = std::make_unique<HarmEngine>(); e1->prepare (sr);
        auto l1 = in220, r1 = in220; run (*e1, l1, r1, p);
        const float f1 = pitchOf (l1, (size_t) (0.5f * sr), l1.size(), sr);
        CHECK (near (f1, 220.0f, 1.5f), "formant +6 keeps the pitch: got %.2f", f1);
        const double b0 = brightness (l0, (size_t) (0.5f * sr), l0.size()), b1 = brightness (l1, (size_t) (0.5f * sr), l1.size());
        CHECK (b1 > b0 * 1.3, "formant +6 is brighter (%.4f vs %.4f)", b1, b0);
    }

    std::printf ("Octaves, synth, transpose, bend, correction\n");
    {
        auto p = base (EngPoly); p.voice[0].on = false; p.oct.level[1] = 1.0f;
        float got = renderPitch (in220, p, sr);
        CHECK (near (got, 110.0f, 1.5f), "sub octave: got %.2f", got);
        p.oct.level[1] = 0.0f; p.oct.level[2] = 1.0f;
        got = renderPitch (in220, p, sr);
        CHECK (near (got, 440.0f, 1.5f), "octave up: got %.2f", got);
        p.oct.level[2] = 0.0f; p.oct.synthLevel = 1.0f; p.oct.synthOctave = -1; p.oct.synthWave = 2;
        got = renderPitch (in220, p, sr);
        CHECK (near (got, 110.0f, 1.5f), "tracking synth one octave down: got %.2f", got);
    }
    {
        auto p = base (EngPoly); p.voice[0].on = false; p.dryDb = 0.0f; p.transpose = -2.0f;
        float got = renderPitch (in220, p, sr);
        CHECK (near (got, 196.0f, 1.5f), "drop 2 st: got %.2f, want 196", got);
        p.transpose = 0.0f; p.bend = 1.0f; p.bendHeel = 0.0f; p.bendToe = 12.0f; p.bendAll = true;
        got = renderPitch (in220, p, sr);
        CHECK (near (got, 440.0f, 1.5f), "whammy toe-down +12: got %.2f", got);
        p.bend = 0.0f; p.bendTrig = true; p.bendRiseMs = 100.0f; p.bendToe = 7.0f;
        got = renderPitch (in220, p, sr);
        CHECK (near (got, 329.63f, 1.5f), "momentary bend to +7 after rise: got %.2f", got);
    }
    {
        auto p = base (EngFormant); p.voice[0].on = false; p.dryDb = 0.0f;
        p.correct = 1.0f; p.correctSpeedMs = 0.0f; p.correctTarget = CorrChromatic;
        const auto off = saw (225.0f, len, sr);   // 39 cents sharp of A3
        const float got = renderPitch (off, p, sr);
        CHECK (near (got, 220.0f, 0.8f), "hard chromatic correction 225 -> 220: got %.2f", got);
    }

    std::printf ("Freeze\n");
    {
        auto p = base (EngPoly); p.voice[0].on = false; p.dryDb = 0.0f; p.freezeLevel = 1.0f;
        std::vector<float> l ((size_t) (2.5f * sr), 0.0f);
        const auto x = saw (220.0f, (int) sr, sr);
        std::copy (x.begin(), x.end(), l.begin());
        auto r = l;
        auto e = std::make_unique<HarmEngine>(); e->prepare (sr);
        run (*e, l, r, p, 256, nullptr, 0, (size_t) (0.9f * sr));
        p.freeze = true;
        run (*e, l, r, p, 256, nullptr, (size_t) (0.9f * sr), (size_t) (2.0f * sr));
        p.freeze = false;
        run (*e, l, r, p, 256, nullptr, (size_t) (2.0f * sr), l.size());
        CHECK (rms (l, (size_t) (1.4f * sr), (size_t) (1.95f * sr)) > 0.02, "freeze sustains after the input stops");
        CHECK (rms (l, (size_t) (2.4f * sr), l.size()) < 1.0e-3, "freeze releases");
    }

    std::printf ("Crystal echoes\n");
    {
        auto p = base (EngPoly); p.voice[0].on = false; p.dryDb = 0.0f;
        p.echoMix = 1.0f; p.echoTimeMs = 400.0f; p.echoFeedback = 0.0f; p.echoPitch = 12.0f;
        std::vector<float> l ((size_t) (1.0f * sr), 0.0f);
        const auto x = saw (220.0f, (int) (0.3f * sr), sr);
        std::copy (x.begin(), x.end(), l.begin());
        auto r = l;
        auto e = std::make_unique<HarmEngine>(); e->prepare (sr);
        run (*e, l, r, p);
        const float got = pitchOf (l, (size_t) (0.42f * sr), (size_t) (0.55f * sr), sr);
        CHECK (near (got, 440.0f, 2.0f), "echo repeat is an octave up: got %.2f", got);
    }

    std::printf ("Silence and stability\n");
    {
        auto p = base (EngFormant);
        for (auto& v : p.voice) v.on = true;
        p.oct.level[0] = p.oct.level[1] = p.oct.level[2] = p.oct.level[3] = 1.0f; p.oct.synthLevel = 1.0f;
        p.echoMix = 0.5f; p.reverbMix = 0.5f; p.shimmer = 1.0f; p.dryDb = 0.0f;
        auto e = std::make_unique<HarmEngine>(); e->prepare (sr);
        std::vector<float> l ((size_t) sr, 0.0f), r ((size_t) sr, 0.0f);
        run (*e, l, r, p);
        float mx = 0; for (float v : l) mx = std::max (mx, std::fabs (v));
        CHECK (mx < 1.0e-5f, "silence in -> silence out (peak %g)", mx);
    }
    {
        Rng rng (12345u);
        int combos = 0;
        for (int eng = 0; eng < 4; ++eng)
            for (int mode = 0; mode < 5; ++mode)
                for (int choir : { 1, 4 })
                    for (int fx = 0; fx < 2; ++fx)
                    {
                        EngineParams p;
                        p.engine = eng; p.mode = mode; p.choir = choir; p.choirSpread = 1.0f;
                        p.keyRoot = mode == 1 ? -1 : 2;
                        for (int v = 0; v < kVoices; ++v)
                            p.voice[(size_t) v] = { true, (float) (int) (rng.next() * 24.0f), (int) (rng.next() * 9.0f), 1.0f,
                                                    rng.next(), rng.next() * 50.0f, 300.0f * rng.next01(), rng.next() * 12.0f };
                        p.humanize = 1.0f; p.vibDepth = 100.0f; p.lfoDepth = 600.0f; p.lfoRate = 6.0f; p.lfoShape = fx * 2;
                        p.formant = rng.next() * 12.0f; p.glideMs = 0.0f;
                        p.correct = 0.7f; p.correctTarget = fx ? CorrRobot : CorrScale;
                        p.transpose = fx ? -12.0f : 5.0f; p.bend = 0.5f; p.bendToe = 24.0f; p.bendAll = fx == 1;
                        for (float& o : p.oct.level) o = 1.0f;
                        p.oct.synthLevel = 1.0f; p.oct.reso = 1.0f; p.oct.attack = 0.5f; p.oct.detune = 1.0f;
                        p.echoMix = 1.0f; p.echoFeedback = 0.95f; p.echoPitch = fx ? -12.0f : 12.0f; p.echoReverse = fx == 1;
                        p.echoTimeMs = 60.0f;
                        p.reverbMix = 1.0f; p.reverbDecay = 1.0f; p.shimmer = 1.0f;
                        p.width = 2.0f; p.stereoMode = fx ? StSplit : StStereo; p.inputDb = 6.0f;

                        auto e = std::make_unique<HarmEngine>(); e->prepare (sr);
                        e->noteOn (60); e->noteOn (64); e->noteOn (67);
                        std::vector<float> l ((size_t) sr), r ((size_t) sr);
                        float f = 110.0f;
                        for (size_t i = 0; i < l.size(); ++i)
                        {
                            if (i % 6000 == 0) f = 90.0f + 600.0f * rng.next01();
                            l[i] = 0.6f * std::sin (kTwoPi * f * (float) i / sr) + 0.2f * rng.next();
                            r[i] = l[i];
                        }
                        p.freeze = false;
                        run (*e, l, r, p, 128, &l, 0, l.size() / 2);
                        p.freeze = true;
                        run (*e, l, r, p, 128, &l, l.size() / 2, l.size());
                        bool finite = true; float mx = 0;
                        for (size_t i = 0; i < l.size(); ++i)
                        {
                            finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]);
                            mx = std::max (mx, std::max (std::fabs (l[i]), std::fabs (r[i])));
                        }
                        CHECK (finite && mx < 40.0f, "stability %s mode %d choir %d fx %d: finite=%d peak=%.2f",
                               engName (eng), mode, choir, fx, (int) finite, mx);
                        ++combos;
                    }
        std::printf ("  %d combinations\n", combos);
    }
    {
        // mono I/O with odd block sizes
        auto p = base (EngLowLatency);
        p.dryDb = 0.0f; p.echoMix = 0.3f;
        auto e = std::make_unique<HarmEngine>(); e->prepare (44100.0);
        auto x = saw (147.0f, 44100, 44100.0f);
        Rng rng (9u);
        size_t pos = 0;
        while (pos < x.size())
        {
            const int n = (int) std::min<size_t> (x.size() - pos, 1 + (size_t) (rng.next01() * 97.0f));
            float* ptr[1] = { x.data() + pos };
            e->process (ptr, 1, nullptr, 0, n, p);
            pos += (size_t) n;
        }
        bool finite = true;
        for (float v : x) finite = finite && std::isfinite (v);
        CHECK (finite && rms (x, 22050, x.size()) > 0.01, "mono I/O, odd block sizes");
    }

    if (failures == 0) std::printf ("ALL CHECKS PASSED\n");
    else std::printf ("%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
