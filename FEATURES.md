# Chillarmonizer feature survey

Chillarmonizer combines the functions of the classic studio harmonizers, harmony and pitch pedals, and vocal harmony plugins. This table lists where each feature comes from and where it lives in the plugin.

## 1. What each unit contributes

| Source | Feature | In Chillarmonizer |
|---|---|---|
| Eventide H910 | Pitch ratio with delay and feedback; glitchy splices | **Vintage 910** engine, **Crystal Echoes** (pitch + feedback) |
| Eventide H949 | Micro-pitch detune, randomised delay | Voice **Detune** and **Delay**, **Humanize** |
| Eventide H3000 | Diatonic shift (key and scale), reverse shift, Crystal Echoes, MIDI | **Diatonic** mode, Echo **Reverse**, **Crystal Echoes**, MIDI map |
| Eventide H9 / PitchFactor / H90 | Quadravox (4 diatonic voices), HarModulator, Crystals, Octaver, Synthonizer, PitchFlex, polyphonic harmony | **4 voices**, **HarModulator**, **Crystals** preset, **Octaves**, **Tracking Synth**, **Bend**, **Poly** engine |
| Eventide MicroPitch / Soundtoys MicroShift | Stereo detune doubling | **MicroPitch Doubler** preset (detune + delay + pan) |
| DigiTech Whammy V | Expression-pedal bends up to ±2 octaves, fixed harmonies, detune, classic/chords tracking | **Bend** (Heel/Toe, CC 11), **Fixed Interval** mode, **Poly** vs **Low Latency** engines |
| DigiTech Whammy DT / Drop | Drop and raise tuning, momentary switching | **Drop / Raise** (±24 st), Bend **Switch: Momentary** |
| DigiTech Whammy Ricochet | Momentary or latching shift with rise and fall times | **Bend** button, **Rise** / **Fall**, **Switch: Momentary / Latch**, CC 80 |
| DigiTech HarmonyMan | Intelligent 2-voice harmony in a key | **Diatonic** mode, Key, Scale |
| Boss PS-6 / PS-5 | Harmony, pitch shift, detune, S-Bend | All of the above; S-Bend = **Bend** with Rise / Fall |
| Boss OC-5, EHX POG2 / Micro POG, TC Sub'n'Up | Polyphonic octaves −2/−1/+1/+2 on sliders, attack swell, filter, detune | **Octaves** section (Sub 2, Sub 1, Up 1, Up 2, Attack, Filter, Resonance, Detune) |
| EHX HOG2 | Harmonic voices, freeze, swell | **Freeze** (CC 64 / 81), Octave **Attack**, **HOG Swell Pad** preset |
| EHX Pitch Fork | ±3 octave polyphonic shift, latch/momentary | **Bend**, **Drop / Raise**, Switch modes |
| TC Quintessence / Brainwaves, Mooer Harmony X2 | Dual intelligent harmony, presets | **Diatonic** mode, preset menu, A/B slots |
| Meris Hedra | 3 voices with rhythmic delays, pitch correction, glide, micro-tuning | Voice **Delay Beat** (tempo-synced), **Pitch Correction**, **Glide**, **Detune** in cents |
| Red Panda Particle | Granular pitch delay, reverse grains | **Crystal Echoes** with Reverse |
| Line 6 HX Twin Harmony | 2 voices in a key, each with its own delay and pan | Per-voice **Delay** and **Pan** |
| TC-Helicon VoiceLive / Harmony Singer | Harmony chosen from guitar chords (NaturalPlay), humanize, doubling | **Sidechain Chord** mode, **Humanize**, **Choir** |
| DigiTech Vocalist Live | Key detection from the input, vibrato | **Key: Auto**, **Vibrato** |
| Antares Harmony Engine | 4 voices; fixed, scale, chord and MIDI modes; throat modelling; choir; humanize | All five **Modes**, per-voice **Formant**, **Choir** (up to 4 per voice), **Humanize** |
| Waves Harmony, iZotope Nectar | Up to 8 voices from MIDI or key/scale, formant, per-voice pan | **MIDI Notes** mode, per-voice **Pan** / **Formant** |
| Soundtoys Little AlterBoy | Pitch and formant, robot mode, MIDI-played pitch | **Formant** engine, **Correct To: Robot / MIDI Note** |
| Antares Auto-Tune | Pitch correction with retune speed | **Pitch Correction** (Amount, Speed, Chromatic / Scale) |
| Polyverse Manipulator, A/B on pedals | Morphing between sounds | **A / B** slots and **Morph** (CC 4) |
| Strymon / Eventide shimmer | Pitch-shifted reverb feedback | **Shimmer Reverb** |

## 2. Engines

| Engine | How it works | Good for |
|---|---|---|
| **Formant (Vocal)** | Pitch-synchronous overlap-add (PSOLA) driven by the pitch tracker. Keeps formants natural and can shift them. | Vocals, solo lines. One note at a time. |
| **Poly (Chords)** | Four overlapping 70 ms grains. | Chords, pads, octave tricks. |
| **Low Latency** | Two grains sized to whole periods of the detected pitch (8–30 ms). | Live guitar, Whammy moves. |
| **Vintage 910** | Two 35 ms grains with near-hard splices. | The glitchy character of the first harmonizers. |

The pitch tracker is YIN on a 12 kHz copy of the input (60 Hz–1.5 kHz). Key detection uses Krumhansl-Schmuckler profiles on the notes you play. Sidechain chord detection uses a Goertzel chroma with triad, seventh, diminished and suspended templates.

## 3. Not included
- True polyphonic *intelligent* harmony, where each note of a strummed chord gets its own diatonic interval. That needs polyphonic pitch detection, a research-grade problem. Poly mode shifts chords by fixed intervals, and the chord modes harmonise a single melody line.
- Spectral (phase-vocoder) shifting. The grain engines cover the same ground at lower CPU and latency.
