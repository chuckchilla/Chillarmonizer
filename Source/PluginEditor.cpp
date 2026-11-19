#include "PluginEditor.h"

namespace
{
    const juce::Colour bgTop   (0xff1a1030);   // deep violet
    const juce::Colour bgBot   (0xff0b0716);
    const juce::Colour panel   (0xff24183f);
    const juce::Colour accent  (0xffb38cff);   // lavender
    const juce::Colour accent2 (0xffffc65c);   // gold
    const juce::Colour textCol (0xfff1eaff);
    const juce::Colour voiceCols[charm::kVoices] = { juce::Colour (0xff6fe3c1), juce::Colour (0xffffc65c),
                                                     juce::Colour (0xffff7aa8), juce::Colour (0xff7ab8ff) };

    juce::Font uiFont (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    const char* id (int param) { return pid::str (param); }
}

//==============================================================================
CharmLookAndFeel::CharmLookAndFeel()
{
    setColour (juce::Label::textColourId, textCol.withAlpha (0.85f));
    setColour (juce::Slider::textBoxTextColourId, textCol.withAlpha (0.75f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::textColourOffId, textCol);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, bgBot.withAlpha (0.7f));
    setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.35f));
    setColour (juce::ComboBox::textColourId, textCol);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, textCol);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void CharmLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float alpha = s.isEnabled() ? 1.0f : 0.3f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.10f * alpha));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, startAngle, angle, true);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (alpha), c.x - r, c.y,
                                             accent2.withAlpha (alpha), c.x + r, c.y, false));
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r * 0.70f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3e2c66).withAlpha (alpha), c.x, c.y - kr,
                                             juce::Colour (0xff160f29).withAlpha (alpha), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (accent.withAlpha (0.25f * alpha));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.2f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-1.6f, -kr + 3.0f, 3.2f, kr * 0.5f, 1.5f);
    g.setColour (textCol.withAlpha (alpha));
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c));
}

void CharmLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? accent : bgBot.withAlpha (0.7f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (accent.withAlpha (on ? 0.0f : 0.4f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    if (highlighted || down)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.25f : 0.10f));
        g.fillRoundedRectangle (r, 5.0f);
    }
}

//==============================================================================
KnobComp::KnobComp (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& caption)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 16);
    addAndMakeVisible (slider);
    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (uiFont (12.0f, true));
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    if (auto* prm = apvts.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void KnobComp::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

ChoiceComp::ChoiceComp (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& caption)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
        box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    label.setText (caption, juce::dontSendNotification);
    label.setFont (uiFont (11.0f, true));
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, box);
}

void ChoiceComp::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (15));
    box.setBounds (r);
}

ToggleComp::ToggleComp (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& text)
    : juce::TextButton (text)
{
    setClickingTogglesState (true);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, *this);
}

//==============================================================================
void HarmonyDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (bgBot.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (accent.withAlpha (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const auto st = proc.getStatus();
    auto inner = r.reduced (10.0f, 6.0f);
    auto header = inner.removeFromTop (15.0f);
    auto meter = inner.removeFromBottom (5.0f);
    inner.removeFromBottom (3.0f);
    auto keys = inner.reduced (0.0f, 2.0f);

    // header: input note, key, chord
    juce::String in = "IN  --";
    if (st.inputMidi > 0.0f)
    {
        const int n = juce::roundToInt (st.inputMidi);
        const int cents = juce::roundToInt ((st.inputMidi - (float) n) * 100.0f);
        in = "IN  " + ChillarmonizerProcessor::noteName (n) + (cents >= 0 ? " +" : " ") + juce::String (cents) + "c";
    }
    const auto& sc = charm::scales()[(size_t) juce::jlimit (0, (int) charm::scales().size() - 1, st.scale)];
    static const char* pcNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    juce::String keyText = juce::String ("KEY  ") + pcNames[charm::mod12 (st.keyRoot)] + " " + sc.name;
    juce::String chordText = st.chordRoot >= 0 ? juce::String ("SC CHORD  ") + pcNames[st.chordRoot] + charm::chordTypeName (st.chordType)
                                               : juce::String();
    g.setFont (uiFont (11.0f, true));
    g.setColour (st.voiced ? accent2 : textCol.withAlpha (0.5f));
    g.drawText (in, header, juce::Justification::centredLeft);
    g.setColour (textCol.withAlpha (0.8f));
    g.drawText (keyText, header.withTrimmedLeft (100.0f), juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText (chordText, header, juce::Justification::centredRight);

    // keyboard C2..C6
    const int lo = 36, hi = 84;
    int whiteCount = 0;
    for (int n = lo; n <= hi; ++n)
    {
        const int pc = charm::mod12 (n);
        if (! (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)) ++whiteCount;
    }
    const float ww = keys.getWidth() / (float) whiteCount;
    auto isBlack = [] (int n) { const int pc = charm::mod12 (n); return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };
    std::array<juce::Rectangle<float>, 128> keyRect {};
    int wi = 0;
    for (int n = lo; n <= hi; ++n)
        if (! isBlack (n))
        {
            keyRect[(size_t) n] = { keys.getX() + (float) wi * ww, keys.getY(), ww - 1.0f, keys.getHeight() };
            ++wi;
        }
    for (int n = lo; n <= hi; ++n)
        if (isBlack (n))
        {
            const auto& left = keyRect[(size_t) n - 1];
            keyRect[(size_t) n] = { left.getRight() - ww * 0.3f, keys.getY(), ww * 0.6f, keys.getHeight() * 0.6f };
        }

    auto noteColour = [&st] (int n, bool& lit) -> juce::Colour
    {
        for (int v = charm::kVoices - 1; v >= 0; --v)
            if (st.voiceOn[(size_t) v] && st.voiceNote[(size_t) v] > 0.0f && juce::roundToInt (st.voiceNote[(size_t) v]) == n)
            { lit = true; return voiceCols[v]; }
        if (st.voiced && st.inputMidi > 0.0f && juce::roundToInt (st.inputMidi) == n) { lit = true; return juce::Colours::white; }
        lit = false;
        return {};
    };

    for (int pass = 0; pass < 2; ++pass)
        for (int n = lo; n <= hi; ++n)
        {
            if (isBlack (n) != (pass == 1)) continue;
            const auto kr = keyRect[(size_t) n];
            bool lit = false;
            const auto c = noteColour (n, lit);
            g.setColour (lit ? c : (pass == 0 ? textCol.withAlpha (0.16f) : bgBot));
            g.fillRoundedRectangle (kr, 2.0f);
            for (int i = 0; i < st.numHeld; ++i)
                if (st.held[(size_t) i] == n)
                {
                    g.setColour (accent);
                    g.drawRoundedRectangle (kr.reduced (0.5f), 2.0f, 1.5f);
                }
        }

    // level meter: input (grey) and harmony (lavender)
    const float lvl = juce::jmin (1.0f, std::sqrt (st.level)), wet = juce::jmin (1.0f, std::sqrt (st.wetLevel));
    g.setColour (textCol.withAlpha (0.1f));
    g.fillRoundedRectangle (meter, 2.0f);
    g.setColour (textCol.withAlpha (0.35f));
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * lvl), 2.0f);
    g.setColour (accent.withAlpha (0.8f));
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * wet).withTrimmedTop (2.5f), 1.5f);
}

//==============================================================================
juce::Component* ChillarmonizerEditor::own (std::unique_ptr<juce::Component> c)
{
    auto* ptr = c.get();
    content.addChildComponent (*ptr);
    owned.push_back (std::move (c));
    return ptr;
}

KnobComp* ChillarmonizerEditor::knob (const char* paramId, const char* caption)
{
    return static_cast<KnobComp*> (own (std::make_unique<KnobComp> (proc.apvts, paramId, caption)));
}

ChoiceComp* ChillarmonizerEditor::choice (const char* paramId, const char* caption)
{
    return static_cast<ChoiceComp*> (own (std::make_unique<ChoiceComp> (proc.apvts, paramId, caption)));
}

ToggleComp* ChillarmonizerEditor::toggle (const char* paramId, const char* text)
{
    return static_cast<ToggleComp*> (own (std::make_unique<ToggleComp> (proc.apvts, paramId, text)));
}

juce::Label* ChillarmonizerEditor::infoLabel (const juce::String& text, float size)
{
    auto l = std::make_unique<juce::Label>();
    l->setText (text, juce::dontSendNotification);
    l->setFont (uiFont (size));
    l->setJustificationType (juce::Justification::topLeft);
    return static_cast<juce::Label*> (own (std::move (l)));
}

ChillarmonizerEditor::ChillarmonizerEditor (ChillarmonizerProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintContent (g); };
    addAndMakeVisible (content);

    // ---------- Page 1: harmony + the four voices ----------
    scaleChoice = choice (id (pid::scale), "SCALE");
    globalFormant = knob (id (pid::formant), "FORMANT");
    Page harmony { "HARMONY", {} };
    harmony.rows.push_back ({
        { "HARMONY", { knob (id (pid::glide), "GLIDE"), knob (id (pid::tracking), "TRACKING") },
                     { choice (id (pid::mode), "MODE"), choice (id (pid::key), "KEY"), scaleChoice }, {} },
        { "SHIFTER", { globalFormant }, { choice (id (pid::engine), "ENGINE") }, {} },
        { "CHOIR & HUMANIZE", { knob (id (pid::choir), "CHOIR"), knob (id (pid::choirSpread), "SPREAD"),
                                knob (id (pid::humanize), "HUMANIZE"), knob (id (pid::vibDepth), "VIBRATO"),
                                knob (id (pid::vibRate), "VIB RATE") }, {}, {} } });

    std::array<Section, charm::kVoices> voiceSections;
    for (int v = 0; v < charm::kVoices; ++v)
    {
        intervalKnobs[(size_t) v] = knob (pid::voiceStr (v, pid::vInterval), "INTERVAL");
        degreeKnobs[(size_t) v]   = knob (pid::voiceStr (v, pid::vDegree), "DEGREE");
        delayKnobs[(size_t) v]    = knob (pid::voiceStr (v, pid::vDelay), "DELAY");
        formantKnobs[(size_t) v]  = knob (pid::voiceStr (v, pid::vFormant), "FORMANT");
        voiceSections[(size_t) v] = { "VOICE " + juce::String (v + 1),
                                      { intervalKnobs[(size_t) v], degreeKnobs[(size_t) v],
                                        knob (pid::voiceStr (v, pid::vLevel), "LEVEL"), knob (pid::voiceStr (v, pid::vPan), "PAN"),
                                        knob (pid::voiceStr (v, pid::vDetune), "DETUNE"), delayKnobs[(size_t) v],
                                        formantKnobs[(size_t) v] },
                                      { toggle (pid::voiceStr (v, pid::vOn), "ON"), choice (pid::voiceStr (v, pid::vBeat), "DELAY BEAT") },
                                      {} };
    }
    harmony.rows.push_back ({ voiceSections[0], voiceSections[1] });
    harmony.rows.push_back ({ voiceSections[2], voiceSections[3] });
    pages.push_back (std::move (harmony));

    // ---------- Page 2: pitch performance ----------
    content.addChildComponent (bendButton);
    bendButton.onStateChange = [this]
    {
        const bool down = bendButton.isDown();
        if (down != bendDown) { bendDown = down; proc.pressBend (down); }
    };
    lfoRateKnob = knob (id (pid::lfoRate), "RATE");
    lfoDivChoice = choice (id (pid::lfoDivision), "SYNC RATE");

    Page pitch { "PITCH FX", {} };
    pitch.rows.push_back ({
        { "WHAMMY / BEND  (expression = CC 11)", { knob (id (pid::bend), "BEND"), knob (id (pid::bendHeel), "HEEL"),
                                                   knob (id (pid::bendToe), "TOE"), knob (id (pid::bendRise), "RISE"),
                                                   knob (id (pid::bendFall), "FALL") },
                                                 { choice (id (pid::bendTarget), "SHIFTS"), choice (id (pid::bendMode), "SWITCH"), &bendButton }, {} },
        { "DROP / RAISE TUNING", { knob (id (pid::transpose), "TRANSPOSE") }, {}, {} } });
    pitch.rows.push_back ({
        { "HARMODULATOR", { knob (id (pid::lfoDepth), "DEPTH"), lfoRateKnob },
                          { choice (id (pid::lfoShape), "SHAPE"), toggle (id (pid::lfoSync), "TEMPO SYNC"), lfoDivChoice }, {} },
        { "FREEZE", { knob (id (pid::freezeLevel), "LEVEL") }, { toggle (id (pid::freeze), "FREEZE") }, {} },
        { "PITCH CORRECTION", { knob (id (pid::correct), "AMOUNT"), knob (id (pid::correctSpeed), "SPEED") },
                              { choice (id (pid::correctTarget), "CORRECT TO") }, {} } });
    pages.push_back (std::move (pitch));

    // ---------- Page 3: octaves, synth, echoes, reverb ----------
    echoTimeKnob = knob (id (pid::echoTime), "TIME");
    echoDivChoice = choice (id (pid::echoDivision), "SYNC TIME");
    Page octs { "OCTAVES & FX", {} };
    octs.rows.push_back ({
        { "OCTAVES  (polyphonic)", { knob (id (pid::octSub2), "SUB 2"), knob (id (pid::octSub1), "SUB 1"),
                                     knob (id (pid::octUp1), "UP 1"), knob (id (pid::octUp2), "UP 2"),
                                     knob (id (pid::octAttack), "ATTACK"), knob (id (pid::octFilter), "FILTER"),
                                     knob (id (pid::octReso), "RESONANCE"), knob (id (pid::octDetune), "DETUNE") }, {}, {} },
        { "TRACKING SYNTH", { knob (id (pid::synthLevel), "LEVEL") },
                            { choice (id (pid::synthOctave), "OCTAVE"), choice (id (pid::synthWave), "WAVE") }, {} } });
    octs.rows.push_back ({
        { "CRYSTAL ECHOES", { knob (id (pid::echoMix), "MIX"), echoTimeKnob, knob (id (pid::echoFeedback), "FEEDBACK"),
                              knob (id (pid::echoPitch), "PITCH"), knob (id (pid::echoTone), "TONE") },
                            { toggle (id (pid::echoSync), "SYNC"), echoDivChoice, toggle (id (pid::echoReverse), "REVERSE") }, {} },
        { "SHIMMER REVERB", { knob (id (pid::reverbMix), "MIX"), knob (id (pid::reverbDecay), "DECAY"),
                              knob (id (pid::shimmer), "SHIMMER") },
                            { choice (id (pid::shimmerPitch), "SHIMMER PITCH") }, {} } });
    pages.push_back (std::move (octs));

    // ---------- Page 4: output, morph, MIDI ----------
    auto* morphText = infoLabel ("Set up a sound, press A. Set up another, press B.\n"
                                 "Morph glides every knob between them, like a pedal's\n"
                                 "heel and toe settings (map it to CC 4).", 12.5f);
    auto* midiText = infoLabel ("Notes: harmony notes in MIDI Chord / MIDI Notes modes,\n"
                                "  and the target for Correct To: MIDI Note\n"
                                "Pitch wheel: bends the harmony +/-2 st\n"
                                "Program Change: preset    CC 82 / 83: next / previous preset\n"
                                "CC 11: Whammy expression    CC 80: Bend switch\n"
                                "CC 64 (sustain): hold Freeze    CC 81: Freeze on/off\n"
                                "CC 1: HarModulator depth    CC 4: Morph", 12.5f);
    auto* sideText = infoLabel ("Sidechain Chord mode: route a guitar or keys track to the\n"
                                "plugin's sidechain input. Its chords pick the harmony notes.\n\n"
                                "Engines: Formant keeps a voice natural (one note at a time).\n"
                                "Poly handles chords. Low Latency is for live playing.\n"
                                "Vintage 910 has the glitchy splices of the first harmonizers.", 12.5f);

    Page out { "OUTPUT & MIDI", {} };
    out.rows.push_back ({
        { "OUTPUT", { knob (id (pid::input), "INPUT"), knob (id (pid::dry), "DRY"), knob (id (pid::wet), "HARMONY"),
                      knob (id (pid::output), "OUTPUT"), knob (id (pid::width), "WIDTH") },
                    { choice (id (pid::stereoMode), "STEREO"), toggle (id (pid::active), "ON") }, {} },
        { "MORPH", { knob (id (pid::morph), "MORPH A > B") }, {}, { morphText } } });
    out.rows.push_back ({
        { "MIDI CONTROL", {}, {}, { midiText } },
        { "TIPS", {}, {}, { sideText } } });
    pages.push_back (std::move (out));

    // ---------- header ----------
    for (size_t i = 0; i < pages.size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (pages[i].name);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->onClick = [this, i] { showPage ((int) i); };
        content.addAndMakeVisible (*b);
        tabButtons.push_back (std::move (b));
    }

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        const int sel = presetBox.getSelectedId();
        if (sel <= 0) return;
        if (sel < 1000) { proc.setCurrentProgram (sel - 1); shownProgram = sel - 1; }
        else if (juce::isPositiveAndBelow (sel - 1000, userPresets.size()))
        {
            proc.loadUserPreset (userPresets[sel - 1000]);
            shownProgram = proc.getCurrentProgram();
        }
    };
    refreshPresetList();
    prevButton.onClick = [this] { proc.stepPreset (-1); };
    nextButton.onClick = [this] { proc.stepPreset (1); };
    saveButton.onClick = [this] { savePreset(); };
    slotA.onClick = [this] { proc.selectSlot (0); };
    slotB.onClick = [this] { proc.selectSlot (1); };
    copyAB.onClick = [this] { proc.copyActiveSlotToOther(); };
    for (auto* c : std::initializer_list<juce::Component*> { &prevButton, &presetBox, &nextButton, &saveButton,
                                                             &slotA, &slotB, &copyAB, &display })
        content.addAndMakeVisible (*c);

    content.setBounds (0, 0, baseW, baseH);
    prevButton.setBounds (330, 18, 26, 28);
    presetBox.setBounds (358, 18, 204, 28);
    nextButton.setBounds (564, 18, 26, 28);
    saveButton.setBounds (596, 18, 56, 28);
    slotA.setBounds (658, 18, 34, 28);
    slotB.setBounds (694, 18, 34, 28);
    copyAB.setBounds (730, 18, 44, 28);
    for (size_t i = 0; i < tabButtons.size(); ++i)
        tabButtons[i]->setBounds (330 + (int) i * 112, 56, 108, 26);
    display.setBounds (baseW - 12 - 378, 10, 378, 82);
    for (auto& page : pages) layoutPage (page);
    showPage (0);

    setResizable (true, true);
    setResizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);
    timerCallback();
    startTimerHz (30);
}

ChillarmonizerEditor::~ChillarmonizerEditor()
{
    setLookAndFeel (nullptr);
}

void ChillarmonizerEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    userPresets = ChillarmonizerProcessor::getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.chillarm");
    userPresets.sort();
    if (! userPresets.isEmpty())
    {
        presetBox.addSectionHeading ("Your presets");
        for (int i = 0; i < userPresets.size(); ++i)
            presetBox.addItem (userPresets[i].getFileNameWithoutExtension(), 1000 + i);
    }
    shownProgram = -1;
}

void ChillarmonizerEditor::savePreset()
{
    const auto folder = ChillarmonizerProcessor::getUserPresetFolder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save preset", folder.getChildFile ("My Preset.chillarm"), "*.chillarm");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file == juce::File()) return;
                              if (proc.saveUserPreset (file.withFileExtension ("chillarm")))
                              {
                                  refreshPresetList();
                                  const int idx = userPresets.indexOf (file.withFileExtension ("chillarm"));
                                  if (idx >= 0) presetBox.setSelectedId (1000 + idx, juce::dontSendNotification);
                                  shownProgram = proc.getCurrentProgram();
                              }
                          });
}

void ChillarmonizerEditor::showPage (int index)
{
    currentPage = juce::jlimit (0, (int) pages.size() - 1, index);
    for (size_t p = 0; p < pages.size(); ++p)
        for (auto& row : pages[p].rows)
            for (auto& s : row)
                for (auto* list : { &s.knobs, &s.side, &s.wide })
                    for (auto* c : *list)
                        c->setVisible ((int) p == currentPage);
    for (size_t i = 0; i < tabButtons.size(); ++i)
        tabButtons[i]->setToggleState ((int) i == currentPage, juce::dontSendNotification);
    content.repaint();
}

void ChillarmonizerEditor::timerCallback()
{
    auto get = [this] (int param) { return proc.apvts.getRawParameterValue (pid::str (param))->load(); };
    const int mode = (int) get (pid::mode);
    const bool formantEngine = (int) get (pid::engine) == charm::EngFormant;
    for (int v = 0; v < charm::kVoices; ++v)
    {
        intervalKnobs[(size_t) v]->setEnabled (mode == charm::ModeFixed);
        degreeKnobs[(size_t) v]->setEnabled (mode == charm::ModeDiatonic || mode == charm::ModeMidiChord
                                             || mode == charm::ModeSidechainChord);
        delayKnobs[(size_t) v]->setEnabled ((int) get (pid::voice (v, pid::vBeat)) == 0);
        formantKnobs[(size_t) v]->setEnabled (formantEngine);
    }
    globalFormant->setEnabled (formantEngine);
    scaleChoice->setEnabled ((int) get (pid::key) != 0);
    const bool lfoSync = get (pid::lfoSync) > 0.5f, echoSync = get (pid::echoSync) > 0.5f;
    lfoRateKnob->setEnabled (! lfoSync);
    lfoDivChoice->setEnabled (lfoSync);
    echoTimeKnob->setEnabled (! echoSync);
    echoDivChoice->setEnabled (echoSync);
    bendButton.setToggleState (get (pid::bendTrig) > 0.5f, juce::dontSendNotification);

    slotA.setToggleState (proc.getActiveSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState (proc.getActiveSlot() == 1, juce::dontSendNotification);

    if (proc.getCurrentProgram() != shownProgram)
    {
        shownProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
    }
    display.repaint();
}

void ChillarmonizerEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgBot);
}

void ChillarmonizerEditor::paintContent (juce::Graphics& g)
{
    auto b = content.getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (bgTop, 0, 0, bgBot, 0, b.getHeight(), false));
    g.fillAll();

    // stacked waves: one voice becoming several
    for (int i = 0; i < 4; ++i)
    {
        juce::Path wave;
        const float y0 = 30.0f + (float) i * 9.0f;
        for (int x = 0; x <= 300; x += 4)
        {
            const float y = y0 + 6.0f * std::sin ((float) x * 0.045f * (1.0f + 0.25f * (float) i));
            if (x == 0) wave.startNewSubPath ((float) x + 14.0f, y); else wave.lineTo ((float) x + 14.0f, y);
        }
        g.setColour (voiceCols[i].withAlpha (0.10f));
        g.strokePath (wave, juce::PathStrokeType (1.5f));
    }

    g.setColour (textCol);
    g.setFont (uiFont (30.0f, true));
    g.drawText ("CHILLARMONIZER", 22, 14, 310, 40, juce::Justification::centredLeft);
    g.setColour (accent2);
    g.setFont (uiFont (11.5f));
    g.drawText ("EVERY HARMONIZER, ONE BOX  //  CHILLPLUGINS", 24, 56, 300, 16, juce::Justification::centredLeft);

    for (const auto& row : pages[(size_t) currentPage].rows)
        for (const auto& s : row)
        {
            auto r = s.bounds.toFloat();
            g.setColour (panel.withAlpha (0.75f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (accent.withAlpha (0.12f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            juce::Colour titleCol = accent;
            if (s.title.startsWith ("VOICE "))
                titleCol = voiceCols[juce::jlimit (0, charm::kVoices - 1, s.title.getTrailingIntValue() - 1)];
            g.setColour (titleCol);
            g.setFont (uiFont (12.0f, true));
            g.drawText (s.title, s.bounds.reduced (12, 8).removeFromTop (16), juce::Justification::centredLeft);
        }
}

void ChillarmonizerEditor::layoutSection (Section& s)
{
    auto r = s.bounds.reduced (10);
    r.removeFromTop (22);

    if (! s.side.empty())
    {
        auto side = r.removeFromRight (128);
        r.removeFromRight (6);
        int y = side.getY();
        for (auto* comp : s.side)
        {
            const int h = dynamic_cast<ChoiceComp*> (comp) != nullptr ? 40 : 26;
            comp->setBounds (side.getX(), y, side.getWidth(), h);
            y += h + 5;
        }
    }

    if (! s.wide.empty())
    {
        auto area = s.knobs.empty() ? r : r.removeFromRight (r.getWidth() * 2 / 3);
        const int h = area.getHeight() / (int) s.wide.size();
        for (auto* comp : s.wide)
            comp->setBounds (area.removeFromTop (h).reduced (4));
    }

    const int n = (int) s.knobs.size();
    if (n == 0) return;
    const int rows = n >= 5 ? 2 : 1;
    const int cols = (n + rows - 1) / rows;
    const int cw = r.getWidth() / cols, chh = r.getHeight() / rows;
    for (int i = 0; i < n; ++i)
    {
        const juce::Rectangle<int> cell (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * chh, cw, chh);
        s.knobs[(size_t) i]->setBounds (cell.withSizeKeepingCentre (cw, juce::jmin (chh, cw + 30)).reduced (2));
    }
}

void ChillarmonizerEditor::layoutPage (Page& page)
{
    auto area = juce::Rectangle<int> (0, 0, baseW, baseH).reduced (12);
    area.removeFromTop (92);

    auto weight = [] (const Section& s)
    {
        const int n = (int) s.knobs.size();
        const int rows = n >= 5 ? 2 : 1;
        const int cols = n > 0 ? (n + rows - 1) / rows : 0;
        return cols * 90 + (s.side.empty() ? 0 : 134) + (s.wide.empty() ? 0 : 330);
    };

    const int gap = 8;
    const int numRows = (int) page.rows.size();
    const int rowH = (area.getHeight() - gap * (numRows - 1)) / juce::jmax (1, numRows);
    for (auto& row : page.rows)
    {
        auto rowArea = area.removeFromTop (rowH);
        area.removeFromTop (gap);
        int total = 0;
        for (auto& s : row) total += weight (s);
        const int avail = rowArea.getWidth() - gap * ((int) row.size() - 1);
        for (size_t i = 0; i < row.size(); ++i)
        {
            const int w = (i + 1 == row.size()) ? rowArea.getWidth() : avail * weight (row[i]) / juce::jmax (1, total);
            row[i].bounds = rowArea.removeFromLeft (w);
            rowArea.removeFromLeft (gap);
            layoutSection (row[i]);
        }
    }
}

void ChillarmonizerEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (scale));
}
