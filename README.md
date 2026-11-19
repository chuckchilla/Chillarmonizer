# Chillarmonizer

An "every harmonizer in one box" plugin by CHILLplugins. It has up to four harmony voices, each with a choir of up to four copies, and five harmony modes: fixed interval, diatonic in a key, MIDI chord, MIDI notes, and chords read from a sidechain. It also has four shifting engines (formant-preserving, polyphonic, low-latency and vintage), Whammy-style bends, drop tuning, polyphonic octaves, a tracking synth, freeze, pitch correction, crystal echoes and shimmer reverb. VST3 on Windows and Mac, plus AU on Mac. Built with JUCE.

See [FEATURES.md](FEATURES.md) for the survey of studio units, pedals and plugins this is built from.

## Controls (four pages)
**HARMONY**
| Section | Controls |
|---|---|
| Harmony | **Mode** (Fixed Interval / Diatonic / MIDI Chord / MIDI Notes / Sidechain Chord), **Key** (Auto or C–B), **Scale** (14 scales and modes), **Glide**, **Tracking** sensitivity |
| Shifter | **Engine** (Formant / Poly / Low Latency / Vintage 910), global **Formant** |
| Choir & Humanize | **Choir** (1–4 copies per voice), **Spread**, **Humanize** (timing and pitch drift), **Vibrato** depth and rate |
| Voice 1–4 | **On**, **Interval** (±24 st, Fixed mode), **Degree** (scale steps or chord tones), **Level**, **Pan**, **Detune** (±50 cents), **Delay** (ms or a tempo **Beat**), **Formant** |

**PITCH FX**: **Whammy / Bend** (expression amount, Heel and Toe intervals, Rise and Fall times, shifts the voices or everything, Momentary or Latch **BEND** switch), **Drop / Raise** tuning, **HarModulator** (pitch LFO with tempo sync), **Freeze**, **Pitch Correction** (Amount, Speed, to Chromatic / Scale / MIDI Note / Robot).

**OCTAVES & FX**: polyphonic **Octaves** (Sub 2, Sub 1, Up 1, Up 2, Attack swell, Filter, Resonance, Detune), **Tracking Synth** (Level, Octave, Saw / Square / Sine), **Crystal Echoes** (Mix, Time or Sync, Feedback, Pitch per repeat, Tone, Reverse), **Shimmer Reverb** (Mix, Decay, Shimmer, Shimmer Pitch).

**OUTPUT & MIDI**: Input, Dry, Harmony, Output, Width, Stereo (Stereo / Split dry-left wet-right / Mono), On, **Morph** between the A and B slots, and the MIDI map.

**Header**: preset menu (32 factory presets plus your own) with previous and next buttons, **SAVE**, **A / B / A>B** compare slots, and a live display of the detected note, key, sidechain chord and a keyboard lighting the input note and every voice.

**MIDI**: notes drive MIDI Chord / MIDI Notes modes and Correct To: MIDI Note. The pitch wheel bends the harmony ±2 st. Program Change = preset, CC 82 / 83 = next / previous preset, CC 11 = Whammy expression, CC 80 = Bend switch, CC 64 = hold Freeze, CC 81 = Freeze on/off, CC 1 = HarModulator depth, CC 4 = Morph.

**Sidechain Chord mode**: route a guitar or keys track into the plugin's sidechain input. Its chords choose the harmony notes for the main input (for example, a vocal).

Your presets are saved as `.chillarm` files in `Documents/Chillarmonizer Presets`.

## Getting the plugin (no compiler needed)
1. Every push runs the **Build Chillarmonizer** workflow (Actions tab), or click *Run workflow*.
2. When it finishes (~10 min), open **Releases → "Chillarmonizer (latest build)"** and download the zip for your OS.
3. Windows: copy `Chillarmonizer.vst3` into `C:\Program Files\Common Files\VST3\`. Mac: copy the VST3 to `~/Library/Audio/Plug-Ins/VST3/` and the AU (`.component`) to `~/Library/Audio/Plug-Ins/Components/`.
4. Rescan plug-ins in your DAW.

## Building locally
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
On Windows you need Visual Studio 2022 with "Desktop development with C++" (it includes the Windows SDK).

## Tests
The engine (`Source/HarmEngine.h`, `PitchTracker.h`, `Shifters.h`, `Harmony.h`, `Effects.h`, `DspBlocks.h`) is JUCE-free. `tools/selftest.cpp` checks it offline:
- pitch tracking at three sample rates
- diatonic and chord-tone theory
- key and chord detection
- output pitch of every engine at +7, −12 and +12 semitones
- the diatonic, MIDI Notes, MIDI Chord and Sidechain Chord modes
- formant shift keeping the pitch
- octaves, synth, drop tuning, whammy bends and pitch correction
- freeze hold and release
- pitched echo repeats
- silence in gives silence out
- an 80-combination stability sweep
- mono I/O with odd block sizes

CI runs it before building the plugin.
```
cmake -B build -DCHILLARMONIZER_TESTS_ONLY=ON && cmake --build build --config Release
build/Release/chillarmonizer_selftest
```
