#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class CharmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CharmLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
};

// Knob with its caption above it.
class KnobComp : public juce::Component
{
public:
    KnobComp (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& caption);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Dropdown with its caption above it.
class ChoiceComp : public juce::Component
{
public:
    ChoiceComp (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& caption);
    void resized() override;
    juce::ComboBox box;
    juce::Label label;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// On/off switch styled as a lit button.
class ToggleComp : public juce::TextButton
{
public:
    ToggleComp (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& text);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Detected note, key, chord, and a keyboard showing the input note and every harmony voice.
class HarmonyDisplay : public juce::Component
{
public:
    explicit HarmonyDisplay (ChillarmonizerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    ChillarmonizerProcessor& proc;
};

class ChillarmonizerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChillarmonizerEditor (ChillarmonizerProcessor&);
    ~ChillarmonizerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int baseW = 1180, baseH = 720;

    struct Section
    {
        juce::String title;
        std::vector<juce::Component*> knobs, side, wide;
        juce::Rectangle<int> bounds {};
    };
    struct Page
    {
        juce::String name;
        std::vector<std::vector<Section>> rows;
    };

    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    void timerCallback() override;
    void showPage (int index);
    void layoutPage (Page&);
    void layoutSection (Section&);
    void paintContent (juce::Graphics&);
    void refreshPresetList();
    void savePreset();
    KnobComp* knob (const char* id, const char* caption);
    ChoiceComp* choice (const char* id, const char* caption);
    ToggleComp* toggle (const char* id, const char* text);
    juce::Component* own (std::unique_ptr<juce::Component>);
    juce::Label* infoLabel (const juce::String& text, float size);

    ChillarmonizerProcessor& proc;
    CharmLookAndFeel lnf;
    Content content;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Page> pages;
    std::vector<std::unique_ptr<juce::TextButton>> tabButtons;
    int currentPage = 0;

    // controls whose enabled state depends on other settings
    std::array<KnobComp*, charm::kVoices> intervalKnobs {}, degreeKnobs {}, delayKnobs {}, formantKnobs {};
    KnobComp* globalFormant = nullptr;
    ChoiceComp* scaleChoice = nullptr;
    KnobComp* lfoRateKnob = nullptr;
    ChoiceComp* lfoDivChoice = nullptr;
    KnobComp* echoTimeKnob = nullptr;
    ChoiceComp* echoDivChoice = nullptr;

    juce::TextButton bendButton { "BEND" };
    bool bendDown = false;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" },
                     slotA { "A" }, slotB { "B" }, copyAB { "A>B" };
    juce::ComboBox presetBox;
    HarmonyDisplay display;
    juce::Array<juce::File> userPresets;
    std::unique_ptr<juce::FileChooser> chooser;
    int shownProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChillarmonizerEditor)
};
