#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<int, float>> values;   // anything not listed = default
    };

    // Values are in real units (semitones, ms, 0..1, dB, choice index).
    const std::vector<Preset>& presets()
    {
        using namespace pid;
        static const std::vector<Preset> list = {
            { "Init", {} },
            { "Diatonic 3rd Up", { { mode, 1 }, { v1On, 1 }, { v1Degree, 2 }, { v1Pan, 0.3f } } },
            { "Thirds & Fifths", { { mode, 1 }, { v1On, 1 }, { v1Degree, 2 }, { v1Pan, -0.5f }, { v2On, 1 }, { v2Degree, 4 },
                                   { v2Pan, 0.5f }, { v2Level, 0.65f } } },
            { "Power Fifth", { { mode, 0 }, { v1On, 1 }, { v1Interval, 7 }, { v1Pan, -0.3f }, { v2On, 1 }, { v2Interval, 12 },
                               { v2Level, 0.45f }, { v2Pan, 0.3f } } },
            { "Twelve-String", { { mode, 0 }, { engine, 1 }, { v1On, 1 }, { v1Interval, 12 }, { v1Level, 0.55f }, { v1Detune, 6 },
                                 { v1Pan, -0.4f }, { v2On, 1 }, { v2Interval, 0 }, { v2Detune, -8 }, { v2Delay, 14 },
                                 { v2Level, 0.6f }, { v2Pan, 0.4f } } },
            { "MicroPitch Doubler", { { mode, 0 }, { engine, 1 }, { v1On, 1 }, { v1Interval, 0 }, { v1Detune, 9 }, { v1Pan, -1 },
                                      { v1Delay, 11 }, { v1Level, 0.7f }, { v2On, 1 }, { v2Interval, 0 }, { v2Detune, -9 },
                                      { v2Pan, 1 }, { v2Delay, 19 }, { v2Level, 0.7f } } },
            { "Whammy Octave (Exp)", { { mode, 0 }, { v1On, 0 }, { bendHeel, 0 }, { bendToe, 12 }, { bendTarget, 1 }, { engine, 2 } } },
            { "Whammy Dive Bomb", { { mode, 0 }, { v1On, 0 }, { bendHeel, 0 }, { bendToe, -24 }, { bendTarget, 1 }, { bendMode, 0 },
                                    { bendRise, 700 }, { bendFall, 180 }, { engine, 2 } } },
            { "Ricochet Fifth", { { mode, 0 }, { v1On, 0 }, { bendHeel, 0 }, { bendToe, 7 }, { bendTarget, 1 }, { bendMode, 0 },
                                  { bendRise, 60 }, { bendFall, 60 }, { engine, 2 } } },
            { "Drop D Tune", { { mode, 0 }, { v1On, 0 }, { transpose, -2 }, { engine, 1 } } },
            { "Baritone Drop Octave", { { mode, 0 }, { v1On, 0 }, { transpose, -12 }, { engine, 1 } } },
            { "POG Organ", { { v1On, 0 }, { octSub1, 0.55f }, { octUp1, 0.6f }, { octUp2, 0.35f }, { octFilter, 6000 },
                             { octDetune, 0.3f }, { dry, -2 } } },
            { "Sub Octave Bass", { { v1On, 0 }, { octSub1, 0.9f }, { octSub2, 0.4f }, { octFilter, 1200 }, { dry, -6 } } },
            { "HOG Swell Pad", { { v1On, 0 }, { octSub1, 0.5f }, { octUp1, 0.6f }, { octUp2, 0.4f }, { octAttack, 0.6f },
                                 { reverbMix, 0.35f }, { reverbDecay, 0.75f }, { shimmer, 0.3f } } },
            { "Choir of Thirds", { { mode, 1 }, { engine, 0 }, { v1On, 1 }, { v1Degree, 2 }, { v2On, 1 }, { v2Degree, 4 },
                                   { v2Pan, 0.5f }, { v3On, 1 }, { v3Degree, -3 }, { v3Pan, -0.6f }, { choir, 3 },
                                   { choirSpread, 0.6f }, { humanize, 0.4f }, { reverbMix, 0.2f } } },
            { "Vocal Duet", { { mode, 1 }, { engine, 0 }, { v1On, 1 }, { v1Degree, 2 }, { v1Pan, 0.3f }, { humanize, 0.3f },
                              { vibDepth, 15 } } },
            { "Formant Monster", { { mode, 0 }, { engine, 0 }, { v1On, 1 }, { v1Interval, -7 }, { v1Formant, -5 }, { v1Level, 1 },
                                   { v1Pan, 0 }, { dry, -60 } } },
            { "Helium", { { mode, 0 }, { engine, 0 }, { v1On, 1 }, { v1Interval, 7 }, { v1Formant, 6 }, { v1Level, 1 },
                          { v1Pan, 0 }, { dry, -60 } } },
            { "Hard Tune", { { engine, 0 }, { v1On, 0 }, { correct, 1 }, { correctSpeed, 0 }, { correctTarget, 1 } } },
            { "Natural Pitch Correct", { { engine, 0 }, { v1On, 0 }, { correct, 0.8f }, { correctSpeed, 60 }, { correctTarget, 1 } } },
            { "Robot Voice", { { engine, 0 }, { v1On, 0 }, { correct, 1 }, { correctSpeed, 0 }, { correctTarget, 3 } } },
            { "MIDI Choir (Play Keys)", { { mode, 3 }, { engine, 0 }, { v1On, 1 }, { v2On, 1 }, { v3On, 1 }, { v4On, 1 },
                                          { humanize, 0.3f }, { choir, 2 }, { dry, -3 }, { reverbMix, 0.2f } } },
            { "MIDI Chord Follow", { { mode, 2 }, { engine, 0 }, { v1On, 1 }, { v1Degree, 1 }, { v2On, 1 }, { v2Degree, -1 },
                                     { v3On, 1 }, { v3Degree, 2 }, { humanize, 0.25f } } },
            { "Guitar Steers Vocals", { { mode, 4 }, { engine, 0 }, { v1On, 1 }, { v1Degree, 1 }, { v2On, 1 }, { v2Degree, 2 },
                                        { humanize, 0.3f } } },
            { "Crystals", { { v1On, 0 }, { echoMix, 0.5f }, { echoReverse, 1 }, { echoPitch, 12 }, { echoFeedback, 0.6f },
                            { echoTime, 300 }, { reverbMix, 0.35f }, { reverbDecay, 0.75f } } },
            { "Crystal Echoes Up", { { v1On, 0 }, { echoMix, 0.4f }, { echoPitch, 7 }, { echoFeedback, 0.55f }, { echoTime, 380 } } },
            { "Shimmer Cathedral", { { v1On, 0 }, { reverbMix, 0.5f }, { reverbDecay, 0.85f }, { shimmer, 0.6f }, { shimmerPitch, 3 } } },
            { "HarModulator Wobble", { { mode, 0 }, { v1On, 1 }, { v1Interval, 0 }, { v1Pan, -1 }, { v2On, 1 }, { v2Interval, 0 },
                                       { v2Pan, 1 }, { lfoDepth, 40 }, { lfoRate, 3 } } },
            { "Rhythmic Harmonies", { { mode, 1 }, { v1On, 1 }, { v1Degree, 2 }, { v1Beat, 3 }, { v1Pan, -0.6f }, { v2On, 1 },
                                      { v2Degree, 4 }, { v2Beat, 6 }, { v2Pan, 0.6f }, { v3On, 1 }, { v3Degree, 7 },
                                      { v3Beat, 4 }, { v3Level, 0.5f } } },
            { "Vintage 910 Glitch", { { mode, 0 }, { engine, 3 }, { v1On, 1 }, { v1Interval, 7 }, { echoMix, 0.3f },
                                      { echoPitch, 7 }, { echoFeedback, 0.5f }, { echoTime, 150 } } },
            { "Synth Tracker", { { v1On, 0 }, { synthLevel, 0.6f }, { synthOctave, 1 }, { synthWave, 1 }, { octFilter, 2500 },
                                 { octReso, 0.4f } } },
            { "Freeze Drone (hit Freeze)", { { mode, 1 }, { v1On, 1 }, { v1Degree, 4 }, { freezeLevel, 0.9f }, { octSub1, 0.5f },
                                             { reverbMix, 0.4f }, { reverbDecay, 0.7f } } },
        };
        return list;
    }

    juce::String pctText (float v, int)  { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String hzText (float v, int)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String dbText (float v, int)   { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String levelDb (float v, int)  { return v <= -59.9f ? juce::String ("Off") : dbText (v, 0); }
    juce::String msText (float v, int)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String centsText (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct"; }
    juce::String panText (float v, int)
    {
        const int p = juce::roundToInt (v * 100.0f);
        return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p));
    }
    juce::String semiText (float v, int)
    {
        const int s = juce::roundToInt (v);
        static const char* names[] = { "Unison", "m2", "M2", "m3", "M3", "4th", "Tritone", "5th", "m6", "M6", "m7", "M7", "Octave" };
        const int a = std::abs (s);
        const juce::String name = a <= 12 ? juce::String (names[a]) : (a == 24 ? juce::String ("2 Oct") : juce::String ("Oct+") + names[a - 12]);
        return s == 0 ? name : (s > 0 ? "+" : "-") + juce::String (a) + " (" + name + ")";
    }
    juce::String degreeText (float v, int)
    {
        const int d = juce::roundToInt (v);
        static const char* names[] = { "Unison", "2nd", "3rd", "4th", "5th", "6th", "7th", "Octave",
                                       "9th", "10th", "11th", "12th", "13th", "14th", "2 Oct" };
        const int a = juce::jmin (14, std::abs (d));
        return d == 0 ? juce::String ("Unison") : juce::String (names[a]) + (d > 0 ? " up" : " down");
    }

    const float shimmerSemis[] = { -12.0f, 5.0f, 7.0f, 12.0f, 19.0f, 24.0f };
}

//==============================================================================
ChillarmonizerProcessor::ChillarmonizerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < pid::count; ++i)
        params[(size_t) i] = apvts.getRawParameterValue (pid::str (i));
    startTimerHz (30);
}

ChillarmonizerProcessor::~ChillarmonizerProcessor()
{
    stopTimer();
    cancelPendingUpdate();
}

const juce::StringArray& ChillarmonizerProcessor::divisionNames()
{
    static const juce::StringArray names { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                           "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1 Bar", "2 Bars", "4 Bars" };
    return names;
}

float ChillarmonizerProcessor::divisionBeats (int index)
{
    static const float beats[] = { 0.0625f, 0.25f / 3.0f, 0.125f, 0.5f / 3.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f,
                                   2.0f / 3.0f, 1.0f, 1.5f, 4.0f / 3.0f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

const juce::StringArray& ChillarmonizerProcessor::beatNames()
{
    static const juce::StringArray names { "Off (ms)", "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1 Bar" };
    return names;
}

float ChillarmonizerProcessor::beatBeats (int index)
{
    static const float beats[] = { 0.0f, 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

juce::String ChillarmonizerProcessor::noteName (int midi)
{
    static const char* n[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return juce::String (n[charm::mod12 (midi)]) + juce::String (charm::floorDiv (midi, 12) - 1);
}

juce::AudioProcessorValueTreeState::ParameterLayout ChillarmonizerProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto addFloat = [&p] (int id, const String& name, NormalisableRange<float> range, float def,
                          std::function<String (float, int)> text)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::str (id), 1 }, name, range, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text))));
    };
    auto addChoice = [&p] (int id, const String& name, const StringArray& choices, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::str (id), 1 }, name, choices, def));
    };
    auto addBool = [&p] (int id, const String& name, bool def)
    {
        // Two-step choice rather than AudioParameterBool: the VST3 wrapper did not restore
        // bool values set to in-between positions (pluginval state-restoration test).
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::str (id), 1 }, name,
                                                             StringArray { "Off", "On" }, def ? 1 : 0));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };
    const NormalisableRange<float> unit (0.0f, 1.0f);
    const NormalisableRange<float> semis (-24.0f, 24.0f, 1.0f);

    StringArray scaleNames;
    for (const auto& s : charm::scales()) scaleNames.add (s.name);

    // Harmony
    addChoice (pid::mode, "Harmony Mode", { "Fixed Interval", "Diatonic (Key)", "MIDI Chord", "MIDI Notes", "Sidechain Chord" }, 1);
    addChoice (pid::key, "Key", { "Auto", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }, 1);
    addChoice (pid::scale, "Scale", scaleNames, 0);
    addChoice (pid::engine, "Engine", { "Formant (Vocal)", "Poly (Chords)", "Low Latency", "Vintage 910" }, 1);
    addFloat (pid::glide, "Glide", skewed (0.0f, 1000.0f, 120.0f), 20.0f, msText);
    addFloat (pid::tracking, "Tracking", unit, 0.5f, pctText);

    // Voices
    const float defInterval[] = { 4, 7, -5, 12 }, defDegree[] = { 2, 4, -3, 7 }, defPan[] = { -0.4f, 0.4f, -0.7f, 0.7f };
    for (int v = 0; v < charm::kVoices; ++v)
    {
        const String n = "Voice " + String (v + 1) + " ";
        addBool (pid::voice (v, pid::vOn), n + "On", v == 0);
        addFloat (pid::voice (v, pid::vInterval), n + "Interval", semis, defInterval[v], semiText);
        addFloat (pid::voice (v, pid::vDegree), n + "Degree", NormalisableRange<float> (-14.0f, 14.0f, 1.0f), defDegree[v], degreeText);
        addFloat (pid::voice (v, pid::vLevel), n + "Level", unit, 0.8f, pctText);
        addFloat (pid::voice (v, pid::vPan), n + "Pan", NormalisableRange<float> (-1.0f, 1.0f), defPan[v], panText);
        addFloat (pid::voice (v, pid::vDetune), n + "Detune", NormalisableRange<float> (-50.0f, 50.0f), 0.0f, centsText);
        addFloat (pid::voice (v, pid::vDelay), n + "Delay", skewed (0.0f, 1000.0f, 120.0f), 0.0f, msText);
        addChoice (pid::voice (v, pid::vBeat), n + "Beat", beatNames(), 0);
        addFloat (pid::voice (v, pid::vFormant), n + "Formant", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
                  [] (float x, int) { return (x > 0 ? "+" : "") + String (x, 1) + " st"; });
    }

    // Character
    addFloat (pid::formant, "Formant", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
              [] (float x, int) { return (x > 0 ? "+" : "") + String (x, 1) + " st"; });
    addFloat (pid::humanize, "Humanize", unit, 0.15f, pctText);
    addFloat (pid::vibDepth, "Vibrato Depth", NormalisableRange<float> (0.0f, 100.0f), 0.0f, centsText);
    addFloat (pid::vibRate, "Vibrato Rate", skewed (0.5f, 10.0f, 4.0f), 5.0f, [] (float x, int) { return String (x, 2) + " Hz"; });
    addFloat (pid::choir, "Choir", NormalisableRange<float> (1.0f, 4.0f, 1.0f), 1.0f,
              [] (float x, int) { return roundToInt (x) == 1 ? String ("Off") : String (roundToInt (x)) + " each"; });
    addFloat (pid::choirSpread, "Choir Spread", unit, 0.5f, pctText);

    // Pitch performance
    addFloat (pid::bend, "Bend (Expression)", unit, 0.0f, pctText);
    addFloat (pid::bendHeel, "Heel", semis, 0.0f, semiText);
    addFloat (pid::bendToe, "Toe", semis, 12.0f, semiText);
    addChoice (pid::bendTarget, "Bend Target", { "Voices", "Everything" }, 1);
    addBool (pid::bendTrig, "Bend Trigger", false);
    addChoice (pid::bendMode, "Bend Switch", { "Momentary", "Latch" }, 0);
    addFloat (pid::bendRise, "Rise Time", skewed (0.0f, 3000.0f, 300.0f), 250.0f, msText);
    addFloat (pid::bendFall, "Fall Time", skewed (0.0f, 3000.0f, 300.0f), 250.0f, msText);
    addFloat (pid::transpose, "Drop / Raise", semis, 0.0f, semiText);
    addBool (pid::freeze, "Freeze", false);
    addFloat (pid::freezeLevel, "Freeze Level", unit, 0.7f, pctText);
    addFloat (pid::lfoDepth, "HarMod Depth", skewed (0.0f, 1200.0f, 100.0f), 0.0f, centsText);
    addFloat (pid::lfoRate, "HarMod Rate", skewed (0.05f, 10.0f, 1.0f), 1.0f, [] (float x, int) { return String (x, 2) + " Hz"; });
    addChoice (pid::lfoShape, "HarMod Shape", { "Sine", "Triangle", "Random", "Square" }, 0);
    addBool (pid::lfoSync, "HarMod Sync", false);
    addChoice (pid::lfoDivision, "HarMod Division", divisionNames(), 10);

    // Correction
    addFloat (pid::correct, "Correction", unit, 0.0f, pctText);
    addFloat (pid::correctSpeed, "Retune Speed", skewed (0.0f, 400.0f, 40.0f), 20.0f, msText);
    addChoice (pid::correctTarget, "Correct To", { "Chromatic", "Scale", "MIDI Note", "Robot" }, 1);

    // Octaves + synth
    addFloat (pid::octSub2, "Sub 2 Oct", unit, 0.0f, pctText);
    addFloat (pid::octSub1, "Sub 1 Oct", unit, 0.0f, pctText);
    addFloat (pid::octUp1, "Up 1 Oct", unit, 0.0f, pctText);
    addFloat (pid::octUp2, "Up 2 Oct", unit, 0.0f, pctText);
    addFloat (pid::octAttack, "Octave Attack", unit, 0.0f, [] (float x, int)
              { return x <= 0.001f ? String ("Off") : String (x * 1.5f, 2) + " s"; });
    addFloat (pid::octFilter, "Octave Filter", skewed (200.0f, 20000.0f, 2000.0f), 20000.0f, hzText);
    addFloat (pid::octReso, "Octave Resonance", unit, 0.0f, pctText);
    addFloat (pid::octDetune, "Octave Detune", unit, 0.0f, pctText);
    addFloat (pid::synthLevel, "Synth Level", unit, 0.0f, pctText);
    addChoice (pid::synthOctave, "Synth Octave", { "-2 Oct", "-1 Oct", "Unison", "+1 Oct" }, 1);
    addChoice (pid::synthWave, "Synth Wave", { "Saw", "Square", "Sine" }, 0);

    // Crystal echoes
    addFloat (pid::echoMix, "Echo Mix", unit, 0.0f, pctText);
    addFloat (pid::echoTime, "Echo Time", skewed (20.0f, 2500.0f, 400.0f), 400.0f, msText);
    addBool (pid::echoSync, "Echo Sync", false);
    addChoice (pid::echoDivision, "Echo Division", divisionNames(), 10);
    addFloat (pid::echoFeedback, "Echo Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.45f, pctText);
    addFloat (pid::echoPitch, "Echo Pitch", semis, 12.0f, semiText);
    addBool (pid::echoReverse, "Echo Reverse", false);
    addFloat (pid::echoTone, "Echo Tone", skewed (1000.0f, 18000.0f, 5000.0f), 7000.0f, hzText);

    // Reverb
    addFloat (pid::reverbMix, "Reverb Mix", unit, 0.0f, pctText);
    addFloat (pid::reverbDecay, "Reverb Decay", unit, 0.5f, pctText);
    addFloat (pid::shimmer, "Shimmer", unit, 0.0f, pctText);
    addChoice (pid::shimmerPitch, "Shimmer Pitch", { "-12", "+5", "+7", "+12", "+19", "+24" }, 3);

    // Output
    addChoice (pid::stereoMode, "Stereo Mode", { "Stereo", "Split (Dry L / Wet R)", "Mono" }, 0);
    addFloat (pid::width, "Width", NormalisableRange<float> (0.0f, 2.0f), 1.0f, pctText);
    addFloat (pid::input, "Input", NormalisableRange<float> (-24.0f, 12.0f), 0.0f, dbText);
    addFloat (pid::dry, "Dry", NormalisableRange<float> (-60.0f, 6.0f), 0.0f, levelDb);
    addFloat (pid::wet, "Harmony", NormalisableRange<float> (-60.0f, 6.0f), 0.0f, levelDb);
    addFloat (pid::output, "Output", NormalisableRange<float> (-24.0f, 12.0f), 0.0f, dbText);
    addBool (pid::active, "On", true);
    addFloat (pid::morph, "Morph", unit, 0.0f, [] (float v, int)
              { return "A" + String (roundToInt ((1.0f - v) * 100.0f)) + " B" + String (roundToInt (v * 100.0f)); });

    return { p.begin(), p.end() };
}

//==============================================================================
void ChillarmonizerProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    engine.prepare (sr);
}

bool ChillarmonizerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    if (! (in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo())))
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void ChillarmonizerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int numIn = mainIn.getNumChannels(), numOut = mainOut.getNumChannels();
    if (numIn == 1 && numOut == 2)
        mainOut.copyFrom (1, 0, mainOut, 0, 0, n);
    else
        for (int c = numIn; c < numOut; ++c)
            mainOut.clear (c, 0, n);

    const float* scPtrs[2] = { nullptr, nullptr };
    int numSc = 0;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            numSc = juce::jmin (2, sc.getNumChannels());
            for (int c = 0; c < numSc; ++c) scPtrs[c] = sc.getReadPointer (c);
        }

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0)
                    hostBpm.store (*bpm);
    const float bpm = (float) juce::jmax (20.0, hostBpm.load());

    // MIDI: notes and pitch wheel go straight to the engine; switches go via the message thread.
    bool midiAction = false;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
        else if (m.isPitchWheel()) engine.setPitchWheel ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
        else if (m.isProgramChange()) { pendingProgram = m.getProgramChangeNumber(); midiAction = true; }
        else if (m.isController())
        {
            const int num = m.getControllerNumber(), val = m.getControllerValue();
            const bool press = val >= 64;
            switch (num)
            {
                case 1:  pendingLfo = (float) val / 127.0f; midiAction = true; break;
                case 4:  pendingMorph = (float) val / 127.0f; midiAction = true; break;
                case 11: pendingExpression = (float) val / 127.0f; midiAction = true; break;
                case 64:
                    if (press != sustainDown) { sustainDown = press; pendingFreeze = press ? 1 : 0; midiAction = true; }
                    break;
                case 80: pendingBend = press ? 1 : 0; midiAction = true; break;
                case 81: if (press) { pendingFreeze = 2; midiAction = true; } break;
                case 82: if (press) { pendingStep = 1; midiAction = true; } break;
                case 83: if (press) { pendingStep = -1; midiAction = true; } break;
                default: break;
            }
        }
    }
    midi.clear();
    if (midiAction) triggerAsyncUpdate();

    charm::EngineParams p;
    p.mode     = (int) raw (pid::mode);
    p.keyRoot  = (int) raw (pid::key) - 1;
    p.scale    = (int) raw (pid::scale);
    p.engine   = (int) raw (pid::engine);
    p.glideMs  = raw (pid::glide);
    p.tracking = raw (pid::tracking);
    for (int v = 0; v < charm::kVoices; ++v)
    {
        auto& vp = p.voice[(size_t) v];
        vp.on       = raw (pid::voice (v, pid::vOn)) > 0.5f;
        vp.interval = raw (pid::voice (v, pid::vInterval));
        vp.degree   = juce::roundToInt (raw (pid::voice (v, pid::vDegree)));
        vp.level    = raw (pid::voice (v, pid::vLevel));
        vp.pan      = raw (pid::voice (v, pid::vPan));
        vp.detune   = raw (pid::voice (v, pid::vDetune));
        const float beats = beatBeats ((int) raw (pid::voice (v, pid::vBeat)));
        vp.delayMs  = beats > 0.0f ? juce::jmin (1000.0f, beats * 60000.0f / bpm) : raw (pid::voice (v, pid::vDelay));
        vp.formant  = raw (pid::voice (v, pid::vFormant));
    }
    p.formant     = raw (pid::formant);
    p.humanize    = raw (pid::humanize);
    p.vibDepth    = raw (pid::vibDepth);
    p.vibRate     = raw (pid::vibRate);
    p.choir       = juce::roundToInt (raw (pid::choir));
    p.choirSpread = raw (pid::choirSpread);
    p.bend        = raw (pid::bend);
    p.bendHeel    = raw (pid::bendHeel);
    p.bendToe     = raw (pid::bendToe);
    p.bendAll     = raw (pid::bendTarget) > 0.5f;
    p.bendTrig    = raw (pid::bendTrig) > 0.5f;
    p.bendRiseMs  = raw (pid::bendRise);
    p.bendFallMs  = raw (pid::bendFall);
    p.transpose   = raw (pid::transpose);
    p.freeze      = raw (pid::freeze) > 0.5f;
    p.freezeLevel = raw (pid::freezeLevel);
    p.lfoDepth    = raw (pid::lfoDepth);
    p.lfoRate     = raw (pid::lfoSync) > 0.5f ? bpm / 60.0f / divisionBeats ((int) raw (pid::lfoDivision)) : raw (pid::lfoRate);
    p.lfoShape    = (int) raw (pid::lfoShape);
    p.correct       = raw (pid::correct);
    p.correctSpeedMs = raw (pid::correctSpeed);
    p.correctTarget = (int) raw (pid::correctTarget);
    p.oct.level[0]  = raw (pid::octSub2);
    p.oct.level[1]  = raw (pid::octSub1);
    p.oct.level[2]  = raw (pid::octUp1);
    p.oct.level[3]  = raw (pid::octUp2);
    p.oct.attack    = raw (pid::octAttack);
    p.oct.filterHz  = raw (pid::octFilter);
    p.oct.reso      = raw (pid::octReso);
    p.oct.detune    = raw (pid::octDetune);
    p.oct.synthLevel  = raw (pid::synthLevel);
    p.oct.synthOctave = (int) raw (pid::synthOctave) - 2;
    p.oct.synthWave   = (int) raw (pid::synthWave);
    p.echoMix      = raw (pid::echoMix);
    p.echoTimeMs   = raw (pid::echoSync) > 0.5f ? juce::jmin (2500.0f, divisionBeats ((int) raw (pid::echoDivision)) * 60000.0f / bpm)
                                                : raw (pid::echoTime);
    p.echoFeedback = raw (pid::echoFeedback);
    p.echoPitch    = raw (pid::echoPitch);
    p.echoReverse  = raw (pid::echoReverse) > 0.5f;
    p.echoTone     = raw (pid::echoTone);
    p.reverbMix    = raw (pid::reverbMix);
    p.reverbDecay  = raw (pid::reverbDecay);
    p.shimmer      = raw (pid::shimmer);
    p.shimmerPitch = shimmerSemis[juce::jlimit (0, 5, (int) raw (pid::shimmerPitch))];
    p.stereoMode   = (int) raw (pid::stereoMode);
    p.width        = raw (pid::width);
    p.inputDb      = raw (pid::input);
    p.dryDb        = raw (pid::dry);
    p.wetDb        = raw (pid::wet);
    p.outputDb     = raw (pid::output);
    p.active       = raw (pid::active) > 0.5f;

    engine.process (mainOut.getArrayOfWritePointers(), numOut, numSc > 0 ? scPtrs : nullptr, numSc, n, p);

    // status for the display
    stInputMidi = engine.getInputMidi();
    stVoiced = engine.isVoiced() ? 1 : 0;
    stKey = engine.getKeyRoot();
    stScale = engine.getScaleShown();
    stChordRoot = engine.getChordRoot();
    stChordType = engine.getChordType();
    for (int v = 0; v < charm::kVoices; ++v)
    {
        stVoiceNote[(size_t) v] = engine.getVoiceNote (v);
        stVoiceOn[(size_t) v] = engine.isVoiceSounding (v) ? 1 : 0;
    }
    int heldNotes[8];
    const int nh = engine.getHeldNotes (heldNotes, 8);
    for (int i = 0; i < nh; ++i) stHeld[(size_t) i] = heldNotes[i];
    stNumHeld = nh;
    float d, w;
    engine.takePeaks (d, w);
    stLevel = juce::jmax (d, stLevel.load() * 0.9f);
    stWet = juce::jmax (w, stWet.load() * 0.9f);
}

ChillarmonizerProcessor::Status ChillarmonizerProcessor::getStatus() const
{
    Status s;
    s.inputMidi = stInputMidi.load();
    s.voiced = stVoiced.load() != 0;
    s.keyRoot = stKey.load();
    s.scale = stScale.load();
    s.chordRoot = stChordRoot.load();
    s.chordType = stChordType.load();
    for (int v = 0; v < charm::kVoices; ++v)
    {
        s.voiceNote[(size_t) v] = stVoiceNote[(size_t) v].load();
        s.voiceOn[(size_t) v] = stVoiceOn[(size_t) v].load() != 0;
    }
    s.numHeld = juce::jlimit (0, 8, stNumHeld.load());
    for (int i = 0; i < s.numHeld; ++i) s.held[(size_t) i] = stHeld[(size_t) i].load();
    s.level = stLevel.load();
    s.wetLevel = stWet.load();
    return s;
}

//==============================================================================
void ChillarmonizerProcessor::setParam (int id, float realValue)
{
    if (auto* prm = apvts.getParameter (pid::str (id)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
        prm->endChangeGesture();
    }
}

void ChillarmonizerProcessor::pressBend (bool down)
{
    if (raw (pid::bendMode) > 0.5f)   // latch: each press toggles
    {
        if (down) setParam (pid::bendTrig, raw (pid::bendTrig) > 0.5f ? 0.0f : 1.0f);
    }
    else
        setParam (pid::bendTrig, down ? 1.0f : 0.0f);
}

void ChillarmonizerProcessor::handleAsyncUpdate()
{
    if (const int prog = pendingProgram.exchange (-1); prog >= 0)
        setCurrentProgram (prog % getNumPrograms());
    if (const int step = pendingStep.exchange (0); step != 0)
        stepPreset (step);
    if (const float m = pendingMorph.exchange (-1.0f); m >= 0.0f)
        setParam (pid::morph, m);
    if (const float e = pendingExpression.exchange (-1.0f); e >= 0.0f)
        setParam (pid::bend, e);
    if (const float l = pendingLfo.exchange (-1.0f); l >= 0.0f)
        setParam (pid::lfoDepth, l * 100.0f);
    if (const int b = pendingBend.exchange (-1); b >= 0)
        pressBend (b == 1);
    if (const int fz = pendingFreeze.exchange (-1); fz >= 0)
        setParam (pid::freeze, fz == 2 ? (raw (pid::freeze) > 0.5f ? 0.0f : 1.0f) : (float) fz);
}

void ChillarmonizerProcessor::stepPreset (int delta)
{
    const int count = getNumPrograms();
    setCurrentProgram (((currentProgram + delta) % count + count) % count);
}

//==============================================================================
bool ChillarmonizerProcessor::isMorphExcluded (int id)
{
    return id == pid::morph || id == pid::bend || id == pid::bendTrig || id == pid::freeze;
}

void ChillarmonizerProcessor::captureSlot (int slot)
{
    for (int i = 0; i < pid::count; ++i)
        if (auto* prm = apvts.getParameter (pid::str (i)))
            slots[(size_t) slot][(size_t) i] = prm->getValue();
    slotValid[(size_t) slot] = true;
}

void ChillarmonizerProcessor::applySlot (int slot)
{
    for (int i = 0; i < pid::count; ++i)
        if (! isMorphExcluded (i))
            if (auto* prm = apvts.getParameter (pid::str (i)))
                if (std::abs (prm->getValue() - slots[(size_t) slot][(size_t) i]) > 1.0e-6f)
                    prm->setValueNotifyingHost (slots[(size_t) slot][(size_t) i]);
}

void ChillarmonizerProcessor::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot) return;
    captureSlot (activeSlot);
    if (slotValid[(size_t) slot]) applySlot (slot);
    else captureSlot (slot);
    activeSlot = slot;
    lastMorph = raw (pid::morph);
}

void ChillarmonizerProcessor::copyActiveSlotToOther()
{
    captureSlot (activeSlot);
    slots[(size_t) (1 - activeSlot)] = slots[(size_t) activeSlot];
    slotValid[(size_t) (1 - activeSlot)] = true;
    lastMorph = raw (pid::morph);
}

// Morph: when the Morph knob moves, glide every continuous parameter between slot A and slot B.
void ChillarmonizerProcessor::timerCallback()
{
    const float m = raw (pid::morph);
    if (lastMorph < 0.0f) { lastMorph = m; return; }
    if (std::abs (m - lastMorph) < 0.0005f) return;
    lastMorph = m;
    if (! (slotValid[0] && slotValid[1])) return;

    for (int i = 0; i < pid::count; ++i)
    {
        if (isMorphExcluded (i)) continue;
        auto* prm = apvts.getParameter (pid::str (i));
        if (dynamic_cast<juce::AudioParameterFloat*> (prm) != nullptr)   // continuous params only
        {
            const float v = slots[0][(size_t) i] + m * (slots[1][(size_t) i] - slots[0][(size_t) i]);
            if (std::abs (prm->getValue() - v) > 1.0e-5f)
                prm->setValueNotifyingHost (v);
        }
    }
}

//==============================================================================
int ChillarmonizerProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String ChillarmonizerProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presets().size()))
        return presets()[(size_t) index].name;
    return {};
}

void ChillarmonizerProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;
    currentProgram = index;

    for (int i = 0; i < pid::count; ++i)
        if (i != pid::morph)
            if (auto* prm = apvts.getParameter (pid::str (i)))
                prm->setValueNotifyingHost (prm->getDefaultValue());

    for (const auto& [id, value] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (pid::str (id)))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    lastMorph = raw (pid::morph);
}

juce::File ChillarmonizerProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Chillarmonizer Presets");
}

bool ChillarmonizerProcessor::saveUserPreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
    {
        file.getParentDirectory().createDirectory();
        return xml->writeTo (file);
    }
    return false;
}

bool ChillarmonizerProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    const auto tree = juce::ValueTree::fromXml (*xml);
    for (const auto& child : tree)
    {
        const auto id = child.getProperty ("id").toString();
        if (id == pid::str (pid::morph))
            continue;
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) child.getProperty ("value")));
    }
    lastMorph = raw (pid::morph);
    return true;
}

juce::AudioProcessorEditor* ChillarmonizerProcessor::createEditor()
{
    return new ChillarmonizerEditor (*this);
}

void ChillarmonizerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("activeSlot", activeSlot, nullptr);
    for (int s = 0; s < 2; ++s)
    {
        if (! slotValid[(size_t) s]) continue;
        juce::StringArray vals;
        for (float v : slots[(size_t) s]) vals.add (juce::String (v, 6));
        state.setProperty (s == 0 ? "slotA" : "slotB", vals.joinIntoString (","), nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ChillarmonizerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
            activeSlot = juce::jlimit (0, 1, (int) tree.getProperty ("activeSlot", 0));
            for (int s = 0; s < 2; ++s)
            {
                const auto str = tree.getProperty (s == 0 ? "slotA" : "slotB").toString();
                juce::StringArray vals;
                vals.addTokens (str, ",", {});
                slotValid[(size_t) s] = vals.size() == pid::count;
                if (slotValid[(size_t) s])
                    for (int i = 0; i < pid::count; ++i)
                        slots[(size_t) s][(size_t) i] = vals[i].getFloatValue();
            }
            apvts.replaceState (tree);
            lastMorph = raw (pid::morph);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChillarmonizerProcessor();
}
