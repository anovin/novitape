# NoviTape

A cassette performance effect for FL Studio. Put it on a mixer insert and play eight tape effects over anything: hold a key for a quick hit, or tap it to leave the effect on.

When nothing is engaged the audio passes through untouched, with zero latency. Time effects (Speed, Reverse, Brake, the transport holds) pull the tape away from "now", and when you let go it snaps back onto the beat.

## The eight effects

Each effect has four variations (left knob) and one amount control (right knob).

| Key | Effect | Variations | Right knob |
|---|---|---|---|
| 1 | **Speed** | Half, 3/4 slow, 1.5x fast, Double | Glide: how slowly the tape changes speed |
| 2 | **Warble** | Subtle, Wobbly, Seasick, Chewed | Depth of the wow and flutter |
| 3 | **Saturate** | Tape, Warm, Hot, Crush | Drive |
| 4 | **Brake** | Short, Medium, Long, Spin back | Length of the stop |
| 5 | **Reverse** | Flip, Half, Rewind, Swell | Slew: how gradually the tape turns around |
| 6 | **Lo-Fi** | Cassette, Dictaphone, 8-bit, AM radio | Amount |
| 7 | **Mute** | Cut, Dropout, Chop 1/8, Chop 1/16 | Depth |
| 8 | **Stutter** | 1/4, 1/8, 1/16, 1/32 (synced to the project tempo) | Gate length |

Speed, Reverse, Stutter and Chop follow FL Studio's tempo.

## Playing it

- **Hold** an effect key for a momentary hit. **Tap** it to latch it on (amber light), and tap again to release.
- The knobs edit the effect you touched last. The coral stripe on the cassette label shows which one.
- Double-click the right knob or a slider to reset it. Hold **Shift** while turning for fine control.
- **MIDI:** notes 60–67 (C5–G5 in FL Studio) hold effects 1–8. In the plugin's wrapper settings, set a MIDI input port and send notes from a controller or a pattern.
- **Automation:** every effect switch, variation and amount is an automatable parameter. Right-click any of them in FL's wrapper and choose *Create automation clip*.

## The transport keys

| Key | What it does |
|---|---|
| ■ | Hold for a tape stop. Release and it spins back up. |
| ◀◀ | Hold to rewind the tape you just played. |
| ▶ | On (teal) = effects engaged. Off = clean bypass. |
| ▶▶ | Hold to cue: plays the last beat at double speed. |
| ❚❚ | Hold to pause the tape. |
| ● | Record the plugin's output to `Music/NoviTape`. Press again to stop. Then drag the cassette straight into FL Studio's playlist, or double-click it to show the file in Finder. |

## Install (Mac)

1. Download `NoviTape-mac.zip` from the repo's **Releases** page ("latest") and unzip it.
2. Right-click `Install-NoviTape.command` and choose **Open**. You only need to right-click the first time, because the build isn't notarised by Apple.
3. In FL Studio, go to **Options → Manage plugins → Find installed plugins**.
4. Add **NoviTape** to a mixer insert slot. Use the VST3 version in FL Studio; the AU build is there for Logic and other Mac hosts.

## Building from source

The GitHub Actions workflow builds a universal (Apple Silicon + Intel) VST3 and AU on every push to `main`. To build locally you need CMake 3.22+ and Xcode command line tools:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target NoviTape_VST3 NoviTape_AU
```

The DSP engine has a standalone test: `cmake --build build --target EngineTest && ./build/EngineTest`.

The plugin is built with JUCE 8, which is free for personal use under its licence terms (AGPLv3, or JUCE's free personal licence). The fonts (Barlow Condensed, Reenie Beanie, Share Tech Mono) are under the SIL Open Font License; their licence files are in `Resources/fonts`.
