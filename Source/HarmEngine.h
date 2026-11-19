// The Chillarmonizer engine: pitch tracking -> up to 4 harmony voices (x4 choir copies),
// octaves + tracking synth, freeze, whammy bend, pitch correction, crystal echoes, shimmer reverb. JUCE-free.
#pragma once
#include "PitchTracker.h"
#include "Harmony.h"
#include "Effects.h"

namespace charm
{
constexpr int kVoices = 4, kMaxChoir = 4;

enum HarmonyMode { ModeFixed, ModeDiatonic, ModeMidiChord, ModeMidiNotes, ModeSidechainChord };
enum ShiftEngine { EngFormant, EngPoly, EngLowLatency, EngVintage };
enum CorrectTarget { CorrChromatic, CorrScale, CorrMidi, CorrRobot };
enum StereoMode { StStereo, StSplit, StMono };

struct VoiceParams
{
    bool on = false;
    float interval = 0.0f;     // semitones (Fixed mode)
    int degree = 0;            // scale steps (Diatonic) or chord tones (chord modes)
    float level = 0.8f, pan = 0.0f, detune = 0.0f /* cents */, delayMs = 0.0f, formant = 0.0f /* semitones */;
};

struct EngineParams
{
    int mode = ModeFixed, keyRoot = 0 /* -1 = auto */, scale = 0, engine = EngPoly;
    float glideMs = 20.0f, tracking = 0.5f;
    std::array<VoiceParams, kVoices> voice {};

    float formant = 0.0f, humanize = 0.0f, vibDepth = 0.0f /* cents */, vibRate = 5.0f;
    int choir = 1;
    float choirSpread = 0.5f;

    float bend = 0.0f, bendHeel = 0.0f, bendToe = 12.0f;
    bool bendAll = false, bendTrig = false;
    float bendRiseMs = 300.0f, bendFallMs = 300.0f;
    float transpose = 0.0f;
    bool freeze = false;
    float freezeLevel = 0.7f;
    float lfoDepth = 0.0f /* cents */, lfoRate = 1.0f;
    int lfoShape = 0;

    float correct = 0.0f, correctSpeedMs = 20.0f;
    int correctTarget = CorrScale;

    OctaveParams oct;

    float echoMix = 0.0f, echoTimeMs = 400.0f, echoFeedback = 0.4f, echoPitch = 12.0f, echoTone = 8000.0f;
    bool echoReverse = false;
    float reverbMix = 0.0f, reverbDecay = 0.5f, shimmer = 0.0f, shimmerPitch = 12.0f;

    int stereoMode = StStereo;
    float width = 1.0f, inputDb = 0.0f, dryDb = 0.0f, wetDb = 0.0f, outputDb = 0.0f;
    bool active = true;
};

class HarmEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        hist.prepare ((int) (0.7f * sr));
        tracker.prepare (sampleRate);
        ana.prepare (sr);
        chordDet.prepare (sampleRate);
        freezer.prepare (sr);
        octBox.prepare (sr);
        echo.prepare (sr);
        reverb.prepare (sr);
        for (auto& v : voices)
            for (auto& d : v.dl) d.prepare ((int) (1.05f * sr) + 16);
        uint32_t seed = 101u;
        for (auto& v : voices)
            for (int c = 0; c < kMaxChoir; ++c)
            {
                v.drift[(size_t) c] = SmoothRandom (seed++ * 7919u);
                v.timing[(size_t) c] = SmoothRandom (seed++ * 104729u);
                v.gs[(size_t) c].reset (0.23f * (float) c);
            }
        reset();
    }

    void reset()
    {
        hist.clear();
        tracker.reset();
        keyDet.reset();
        chordDet.reset();
        freezer.reset();
        octBox.reset();
        echo.reset();
        reverb.reset();
        for (auto& v : voices)
        {
            for (auto& p : v.ps) p.reset();
            for (auto& d : v.dl) d.clear();
            v.gain = 0.0f; v.semisInit = false; v.delay = 0.0f;
        }
        dryPs.reset(); dryGs.reset();
        dryShiftMix = 0.0f; corrSemis = 0.0f; bendRamp = 0.0f;
        curMidi = -1.0f; curNote = 60; voiced = false; everVoiced = false;
        activeGain = 1.0f; peakDry = peakWet = 0.0f;
        llWin = 0.015f * sr;
        lastEngine = -1;
    }

    // MIDI (audio thread)
    void noteOn (int note)
    {
        noteOff (note);
        if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note;
    }
    void noteOff (int note)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == note)
            {
                for (int j = i; j + 1 < numHeld; ++j) held[(size_t) j] = held[(size_t) j + 1];
                --numHeld;
                return;
            }
    }
    void allNotesOff() { numHeld = 0; }
    void setPitchWheel (float semis) { pitchWheel = semis; }

    // io: numIo channels (1 or 2) processed in place. sc: optional sidechain (for chord detection).
    void process (float* const* io, int numIo, const float* const* sc, int numSc, int n, const EngineParams& p)
    {
        octBox.setParams (p.oct);
        echo.setParams (p.echoTimeMs, p.echoFeedback, p.echoPitch, p.echoReverse, p.echoTone);
        reverb.setParams (p.reverbDecay, p.shimmer, p.shimmerPitch);
        const float inGain = dbToGain (p.inputDb), outGain = dbToGain (p.outputDb);
        const float dryGain = p.dryDb <= -59.9f ? 0.0f : dbToGain (p.dryDb);
        const float wetGain = p.wetDb <= -59.9f ? 0.0f : dbToGain (p.wetDb);

        for (int start = 0; start < n; start += kChunk)
        {
            const int len = std::min (kChunk, n - start);
            updateControl (p, len);

            for (int s = start; s < start + len; ++s)
            {
                const float inL = io[0][s] * inGain;
                const float inR = (numIo > 1 ? io[1][s] : io[0][s]) * inGain;
                const float mono = 0.5f * (inL + inR);

                if (freezer.getGain() < 0.5f) tracker.push (mono);
                const float cloud = freezer.process (hist, p.freeze);
                const float fg = freezer.getGain();
                const float harmIn = mono * (1.0f - fg) + cloud;
                hist.write (harmIn);
                if (numSc > 0 && sc != nullptr)
                    chordDet.push (numSc > 1 ? 0.5f * (sc[0][s] + sc[1][s]) : sc[0][s]);
                ana.update (hist, period, voiced);

                // harmony voices
                float wetL = 0.0f, wetR = 0.0f;
                for (int v = 0; v < kVoices; ++v)
                {
                    auto& vc = voices[(size_t) v];
                    vc.gain += (vc.gainTarget - vc.gain) * 0.0025f;
                    if (vc.gain < 1.0e-5f && vc.gainTarget <= 0.0f) { vc.dl[0].write (0.0f); vc.dl[1].write (0.0f); continue; }
                    float sumL = 0.0f, sumR = 0.0f;
                    for (int c = 0; c < choir; ++c)
                    {
                        const float x = usePsola
                            ? vc.ps[(size_t) c].process (hist, ana, vc.ratio[(size_t) c], vc.formantRatio, vc.offset[(size_t) c])
                            : vc.gs[(size_t) c].process (hist, vc.ratio[(size_t) c], grainWin, grainCount, grainSharp, vc.offset[(size_t) c]);
                        sumL += x * vc.panL[(size_t) c];
                        sumR += x * vc.panR[(size_t) c];
                    }
                    vc.dl[0].write (sumL);
                    vc.dl[1].write (sumR);
                    if (vc.delay >= 3.0f) { sumL = vc.dl[0].read (vc.delay); sumR = vc.dl[1].read (vc.delay); }
                    wetL += sumL * vc.gain;
                    wetR += sumR * vc.gain;
                }

                // dry path (optionally transposed / bent / pitch-corrected)
                float dryL = inL, dryR = inR;
                if (dryShiftMix > 1.0e-4f)
                {
                    const float shifted = usePsola ? dryPs.process (hist, ana, dryRatio, 1.0f, 0.0f)
                                                   : dryGs.process (hist, dryRatio, grainWin, grainCount, grainSharp, 0.0f);
                    dryL = inL + (shifted - inL) * dryShiftMix;
                    dryR = inR + (shifted - inR) * dryShiftMix;
                }

                // octaves, synth, the frozen sound itself
                float extra = cloud * p.freezeLevel;
                if (octBox.active()) extra += octBox.process (hist, harmIn, trackHz, voiced);
                wetL = (wetL + extra) * wetGain;
                wetR = (wetR + extra) * wetGain;
                dryL *= dryGain;
                dryR *= dryGain;

                // width on the harmony
                const float mid = 0.5f * (wetL + wetR), side = 0.5f * (wetL - wetR) * p.width;
                wetL = mid + side;
                wetR = mid - side;

                float outL, outR;
                if (p.stereoMode == StSplit)
                {
                    outL = 0.5f * (dryL + dryR);
                    outR = 0.5f * (wetL + wetR);
                }
                else if (p.stereoMode == StMono)
                {
                    outL = outR = 0.5f * (dryL + dryR + wetL + wetR);
                }
                else
                {
                    outL = dryL + wetL;
                    outR = dryR + wetR;
                }

                if (p.echoMix > 1.0e-4f || echoTail > 0)
                {
                    float eL, eR;
                    echo.process (outL, outR, eL, eR);
                    outL += eL * p.echoMix;
                    outR += eR * p.echoMix;
                    echoTail = p.echoMix > 1.0e-4f ? (int) (12.0f * sr) : echoTail - 1;
                }
                if (p.reverbMix > 1.0e-4f || reverbTail > 0)
                {
                    float rL, rR;
                    reverb.process (outL, outR, rL, rR);
                    outL += rL * p.reverbMix;
                    outR += rR * p.reverbMix;
                    reverbTail = p.reverbMix > 1.0e-4f ? (int) (20.0f * sr) : reverbTail - 1;
                }

                activeGain += ((p.active ? 1.0f : 0.0f) - activeGain) * 0.002f;
                outL = inL + (outL - inL) * activeGain;
                outR = inR + (outR - inR) * activeGain;

                peakDry = std::max (peakDry, std::fabs (mono));
                peakWet = std::max (peakWet, std::fabs (0.5f * (wetL + wetR)));

                io[0][s] = outL * outGain;
                if (numIo > 1) io[1][s] = outR * outGain;
            }
        }
    }

    // ---- status for the UI ----
    float getInputMidi() const noexcept   { return curMidi; }
    bool isVoiced() const noexcept        { return voiced; }
    int getKeyRoot() const noexcept       { return keyRootShown; }
    int getScaleShown() const noexcept    { return scaleShown; }
    int getChordRoot() const noexcept     { return chordDet.getRoot(); }
    int getChordType() const noexcept     { return chordDet.getType(); }
    float getVoiceNote (int v) const noexcept { return voices[(size_t) v].shownNote; }
    bool isVoiceSounding (int v) const noexcept { return voices[(size_t) v].gainTarget > 0.0f; }
    int getHeldNotes (int* out, int max) const noexcept
    {
        const int k = std::min (max, numHeld);
        for (int i = 0; i < k; ++i) out[i] = held[(size_t) i];
        return k;
    }
    void takePeaks (float& d, float& w) noexcept { d = peakDry; w = peakWet; peakDry = peakWet = 0.0f; }

private:
    static constexpr int kChunk = 32;

    struct Voice
    {
        std::array<GrainShifter, kMaxChoir> gs {};
        std::array<PsolaVoice, kMaxChoir> ps {};
        std::array<SmoothRandom, kMaxChoir> drift {}, timing {};
        std::array<float, kMaxChoir> ratio { 1, 1, 1, 1 }, offset {}, panL {}, panR {};
        std::array<DelayLine, 2> dl {};
        float formantRatio = 1.0f, semis = 0.0f, gain = 0.0f, gainTarget = 0.0f, delay = 0.0f, shownNote = -1.0f;
        bool semisInit = false;
    };

    void updateControl (const EngineParams& p, int len)
    {
        const float dt = (float) len / sr;

        // ---- engine settings ----
        if (p.engine != lastEngine)
        {
            for (auto& v : voices) for (auto& ps : v.ps) ps.reset();
            dryPs.reset();
            lastEngine = p.engine;
        }
        usePsola = p.engine == EngFormant;
        grainCount = 2; grainSharp = 1.0f;
        if (p.engine == EngPoly)          { grainWin = 0.07f * sr; grainCount = 4; }
        else if (p.engine == EngVintage)  { grainWin = 0.035f * sr; grainSharp = 6.0f; }
        else
        {
            float w = 0.015f * sr;
            if (voiced && trackHz > 0.0f)
            {
                // Whole even number of periods: the two grains sit half a window apart, so they
                // must be a whole number of periods apart or they cancel.
                const float P = sr / trackHz;
                w = std::max (1.0f, std::ceil (0.011f * sr / (2.0f * P))) * 2.0f * P;
            }
            llWin += (w - llWin) * 0.15f;
            grainWin = llWin;
        }

        // ---- pitch tracking ----
        const bool frozen = freezer.getGain() > 0.5f;
        const float conf = tracker.getConfidence(), hz = tracker.getFrequency();
        const float threshold = 0.9f - 0.4f * std::clamp (p.tracking, 0.0f, 1.0f);
        voiced = ! frozen && conf >= threshold && hz > 0.0f;
        if (voiced)
        {
            trackHz = hz;
            period = sr / hz;
            curMidi = tracker.getMidi();
            if (! everVoiced || std::fabs (curMidi - (float) curNote) > 0.65f) curNote = (int) std::lround (curMidi);
            everVoiced = true;
            keyDet.add (curNote, conf * dt, std::exp (-dt / 25.0f));
        }

        // ---- key ----
        int root = p.keyRoot, scaleIdx = std::clamp (p.scale, 0, (int) scales().size() - 1);
        if (root < 0)
        {
            root = keyDet.known() ? keyDet.getRoot() : 0;
            scaleIdx = keyDet.isMinor() ? 1 : 0;
        }
        keyRootShown = root;
        scaleShown = scaleIdx;
        const Scale& sc = scales()[(size_t) scaleIdx];

        // ---- held MIDI notes ----
        int mask = 0;
        std::array<int, 16> sorted {};
        for (int i = 0; i < numHeld; ++i) { mask |= 1 << mod12 (held[(size_t) i]); sorted[(size_t) i] = held[(size_t) i]; }
        std::sort (sorted.begin(), sorted.begin() + numHeld);

        // ---- pitch correction ----
        float want = 0.0f;
        if (p.correct > 0.001f)
        {
            want = corrSemis;
            if (voiced)
            {
                float t = curMidi;
                switch (p.correctTarget)
                {
                    case CorrChromatic: t = (float) std::lround (curMidi); break;
                    case CorrScale:     t = (float) nearestScaleNote (curMidi, root, sc); break;
                    case CorrMidi:      t = numHeld > 0 ? (float) held[(size_t) numHeld - 1] : curMidi; break;
                    case CorrRobot:     t = (float) root + 12.0f * std::round ((curMidi - (float) root) / 12.0f); break;
                    default: break;
                }
                want = (t - curMidi) * p.correct;
            }
        }
        corrSemis += (want - corrSemis) * smoothCoef (p.correctSpeedMs, len);

        // ---- bend (expression + momentary/latched ramp + pitch wheel) ----
        const float rampTarget = p.bendTrig ? 1.0f : 0.0f;
        const float rampMs = rampTarget > bendRamp ? p.bendRiseMs : p.bendFallMs;
        const float step = (float) len / std::max (1.0f, rampMs * 0.001f * sr);
        bendRamp = rampTarget > bendRamp ? std::min (rampTarget, bendRamp + step) : std::max (rampTarget, bendRamp - step);
        const float bendPos = std::max (p.bend, bendRamp);
        const float bendSemis = p.bendHeel + (p.bendToe - p.bendHeel) * bendPos + pitchWheel;
        const float dryShift = p.transpose + (p.bendAll ? bendSemis : 0.0f) + corrSemis;
        const float voiceShift = p.transpose + bendSemis + corrSemis;

        // ---- dry shifting ----
        const bool needDry = std::fabs (p.transpose) > 0.005f || (p.bendAll && std::fabs (bendSemis) > 0.005f) || p.correct > 0.001f;
        const float mixStep = (float) len / (0.02f * sr);
        dryShiftMix = needDry ? std::min (1.0f, dryShiftMix + mixStep) : std::max (0.0f, dryShiftMix - mixStep);
        dryRatio = std::clamp (std::exp2 (dryShift / 12.0f), 0.2f, 4.5f);

        // ---- modulation ----
        lfoPhase = frac (lfoPhase + p.lfoRate * dt);
        const float lfoRnd = lfoRandom.next (p.lfoRate * dt);
        vibPhase = frac (vibPhase + p.vibRate * dt);

        // ---- voices ----
        choir = std::clamp (p.choir, 1, kMaxChoir);
        const float glide = smoothCoef (std::max (3.0f, p.glideMs), len);
        const float copyNorm = 1.0f / std::sqrt ((float) choir);
        for (int v = 0; v < kVoices; ++v)
        {
            const auto& vp = p.voice[(size_t) v];
            auto& vc = voices[(size_t) v];
            bool muted = false;
            float h = 0.0f;
            switch (p.mode)
            {
                case ModeDiatonic: h = (float) (diatonicTarget (curNote, root, sc, vp.degree) - curNote); break;
                case ModeMidiChord:
                case ModeSidechainChord:
                {
                    const int t = chordTarget (curNote, p.mode == ModeMidiChord ? mask : chordDet.getMask(), vp.degree);
                    muted = t < 0;
                    h = muted ? 0.0f : (float) (t - curNote);
                    break;
                }
                case ModeMidiNotes:
                    muted = v >= numHeld;
                    h = muted ? 0.0f : (float) sorted[(size_t) v] - (everVoiced ? curMidi : (float) curNote);
                    break;
                default: h = vp.interval; break;
            }
            vc.shownNote = muted ? -1.0f : (float) curNote + h;

            const float target = h + voiceShift + vp.detune * 0.01f;
            if (! vc.semisInit) { vc.semis = target; vc.semisInit = true; }
            vc.semis += (target - vc.semis) * glide;
            vc.gainTarget = (vp.on && ! muted) ? vp.level : 0.0f;
            vc.formantRatio = std::clamp (std::exp2 ((p.formant + vp.formant) / 12.0f), 0.5f, 2.0f);
            vc.delay += (vp.delayMs * 0.001f * sr - vc.delay) * 0.08f;

            float lfo = p.lfoShape == 2 ? lfoRnd * ((v & 1) ? -1.0f : 1.0f) : lfoShape (p.lfoShape, lfoPhase + 0.25f * (float) v);
            const float mod = lfo * p.lfoDepth * 0.01f
                            + std::sin (kTwoPi * vibPhase + 1.3f * (float) v) * p.vibDepth * 0.01f;
            const float half = 0.5f * (float) (choir - 1);
            for (int c = 0; c < choir; ++c)
            {
                const float cc = (float) c - half;
                const float drift = vc.drift[(size_t) c].next (2.5f * dt) * p.humanize * 0.25f;
                const float semis = vc.semis + mod + p.choirSpread * cc * 0.14f + drift;
                vc.ratio[(size_t) c] = std::clamp (std::exp2 (semis / 12.0f), 0.2f, 4.5f);

                const float tm = 0.5f + 0.5f * vc.timing[(size_t) c].next (0.8f * dt);
                vc.offset[(size_t) c] = (p.choirSpread * (float) c * 0.011f + p.humanize * tm * 0.03f) * sr;

                const float spread = choir > 1 ? p.choirSpread * 0.8f * cc / std::max (0.5f, half) : 0.0f;
                const float pan = std::clamp (vp.pan + spread, -1.0f, 1.0f);
                const float angle = (pan + 1.0f) * 0.25f * kPi;
                vc.panL[(size_t) c] = std::cos (angle) * copyNorm * 1.41421356f;   // unity per side at centre
                vc.panR[(size_t) c] = std::sin (angle) * copyNorm * 1.41421356f;
            }
        }
    }

    float smoothCoef (float ms, int len) const noexcept
    {
        if (ms <= 0.5f) return 1.0f;
        return 1.0f - std::exp (-(float) len / (ms * 0.001f * sr));
    }

    float sr = 48000.0f;
    History hist;
    PitchTracker tracker;
    PsolaAnalysis ana;
    KeyDetector keyDet;
    ChordDetector chordDet;
    Freezer freezer;
    OctaveBox octBox;
    CrystalEcho echo;
    ShimmerReverb reverb;
    std::array<Voice, kVoices> voices {};
    GrainShifter dryGs;
    PsolaVoice dryPs;
    SmoothRandom lfoRandom { 99u };

    std::array<int, 16> held {};
    int numHeld = 0;
    float pitchWheel = 0.0f;

    bool usePsola = false;
    int grainCount = 4, choir = 1, lastEngine = -1;
    float grainWin = 3000.0f, grainSharp = 1.0f, llWin = 700.0f;
    float trackHz = 0.0f, period = 400.0f, curMidi = -1.0f;
    int curNote = 60;
    bool voiced = false, everVoiced = false;
    int keyRootShown = 0, scaleShown = 0;
    float corrSemis = 0.0f, bendRamp = 0.0f, dryShiftMix = 0.0f, dryRatio = 1.0f;
    float lfoPhase = 0.0f, vibPhase = 0.0f;
    float activeGain = 1.0f, peakDry = 0.0f, peakWet = 0.0f;
    int echoTail = 0, reverbTail = 0;
};
} // namespace charm
