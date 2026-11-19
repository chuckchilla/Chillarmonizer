#pragma once
#include <JuceHeader.h>
#include "HarmEngine.h"

#define CHARM_VOICE_PARAMS(X, n) \
    X (v##n##On, "v" #n "On") X (v##n##Interval, "v" #n "Interval") X (v##n##Degree, "v" #n "Degree") \
    X (v##n##Level, "v" #n "Level") X (v##n##Pan, "v" #n "Pan") X (v##n##Detune, "v" #n "Detune") \
    X (v##n##Delay, "v" #n "Delay") X (v##n##Beat, "v" #n "Beat") X (v##n##Formant, "v" #n "Formant")

// Every parameter, in one place. The string is the stable host-facing ID.
#define CHARM_PARAMS(X) \
    X (mode, "mode") X (key, "key") X (scale, "scale") X (engine, "engine") X (glide, "glide") X (tracking, "tracking") \
    CHARM_VOICE_PARAMS (X, 1) CHARM_VOICE_PARAMS (X, 2) CHARM_VOICE_PARAMS (X, 3) CHARM_VOICE_PARAMS (X, 4) \
    X (formant, "formant") X (humanize, "humanize") X (vibDepth, "vibDepth") X (vibRate, "vibRate") \
    X (choir, "choir") X (choirSpread, "choirSpread") \
    X (bend, "bend") X (bendHeel, "bendHeel") X (bendToe, "bendToe") X (bendTarget, "bendTarget") \
    X (bendTrig, "bendTrig") X (bendMode, "bendMode") X (bendRise, "bendRise") X (bendFall, "bendFall") \
    X (transpose, "transpose") X (freeze, "freeze") X (freezeLevel, "freezeLevel") \
    X (lfoDepth, "lfoDepth") X (lfoRate, "lfoRate") X (lfoShape, "lfoShape") X (lfoSync, "lfoSync") X (lfoDivision, "lfoDivision") \
    X (correct, "correct") X (correctSpeed, "correctSpeed") X (correctTarget, "correctTarget") \
    X (octSub2, "octSub2") X (octSub1, "octSub1") X (octUp1, "octUp1") X (octUp2, "octUp2") \
    X (octAttack, "octAttack") X (octFilter, "octFilter") X (octReso, "octReso") X (octDetune, "octDetune") \
    X (synthLevel, "synthLevel") X (synthOctave, "synthOctave") X (synthWave, "synthWave") \
    X (echoMix, "echoMix") X (echoTime, "echoTime") X (echoSync, "echoSync") X (echoDivision, "echoDivision") \
    X (echoFeedback, "echoFeedback") X (echoPitch, "echoPitch") X (echoReverse, "echoReverse") X (echoTone, "echoTone") \
    X (reverbMix, "reverbMix") X (reverbDecay, "reverbDecay") X (shimmer, "shimmer") X (shimmerPitch, "shimmerPitch") \
    X (stereoMode, "stereoMode") X (width, "width") X (input, "input") X (dry, "dry") X (wet, "wet") \
    X (output, "output") X (active, "active") X (morph, "morph")

namespace pid
{
    enum Id
    {
       #define X(name, str) name,
        CHARM_PARAMS (X)
       #undef X
        count
    };

    inline const char* str (int id)
    {
        static const char* names[] = {
           #define X(name, s) s,
            CHARM_PARAMS (X)
           #undef X
        };
        return names[id];
    }

    // The nine per-voice parameters, in CHARM_VOICE_PARAMS order.
    enum VoiceField { vOn, vInterval, vDegree, vLevel, vPan, vDetune, vDelay, vBeat, vFormant, vFieldCount };
    inline int voice (int v, int field) { return v1On + v * vFieldCount + field; }
    inline const char* voiceStr (int v, int field) { return str (voice (v, field)); }
}

class ChillarmonizerProcessor : public juce::AudioProcessor,
                                private juce::AsyncUpdater,
                                private juce::Timer
{
public:
    ChillarmonizerProcessor();
    ~ChillarmonizerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static const juce::StringArray& divisionNames();
    static float divisionBeats (int index);
    static const juce::StringArray& beatNames();
    static float beatBeats (int index);   // 0 = off
    static juce::String noteName (int midi);
    double getHostBpm() const { return hostBpm.load(); }

    // User presets (message thread)
    static juce::File getUserPresetFolder();
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);
    void stepPreset (int delta);

    // A/B slots and morph (message thread)
    void selectSlot (int slot);
    void copyActiveSlotToOther();
    int getActiveSlot() const { return activeSlot; }

    void setParam (int id, float realValue);   // message thread, with gesture
    void pressBend (bool down);                 // honours Momentary / Latch

    // What the engine hears, for the display (message thread reads)
    struct Status
    {
        float inputMidi = -1.0f;
        bool voiced = false;
        int keyRoot = 0, scale = 0, chordRoot = -1, chordType = 0;
        std::array<float, charm::kVoices> voiceNote {};
        std::array<bool, charm::kVoices> voiceOn {};
        std::array<int, 8> held {};
        int numHeld = 0;
        float level = 0.0f, wetLevel = 0.0f;
    };
    Status getStatus() const;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    float raw (int id) const { return params[(size_t) id]->load(); }
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void captureSlot (int slot);
    void applySlot (int slot);
    static bool isMorphExcluded (int id);

    charm::HarmEngine engine;
    std::array<std::atomic<float>*, pid::count> params {};
    std::atomic<double> hostBpm { 120.0 };
    int currentProgram = 0;
    double sampleRate = 44100.0;

    // MIDI -> message thread
    std::atomic<int> pendingProgram { -1 }, pendingFreeze { -1 }, pendingBend { -1 }, pendingStep { 0 };
    std::atomic<float> pendingExpression { -1.0f }, pendingMorph { -1.0f }, pendingLfo { -1.0f };
    bool sustainDown = false;

    // status snapshot (written on the audio thread, read by the UI)
    std::atomic<float> stInputMidi { -1.0f }, stLevel { 0.0f }, stWet { 0.0f };
    std::atomic<int> stVoiced { 0 }, stKey { 0 }, stScale { 0 }, stChordRoot { -1 }, stChordType { 0 }, stNumHeld { 0 };
    std::array<std::atomic<float>, charm::kVoices> stVoiceNote {};
    std::array<std::atomic<int>, charm::kVoices> stVoiceOn {};
    std::array<std::atomic<int>, 8> stHeld {};

    // A/B
    std::array<std::array<float, pid::count>, 2> slots {};
    std::array<bool, 2> slotValid { false, false };
    int activeSlot = 0;
    float lastMorph = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChillarmonizerProcessor)
};
