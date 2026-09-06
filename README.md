# AudioScratcher

Records live audio input continuously into a rolling buffer, and lets a MIDI
DJ controller's jog wheel scratch (scrub position/speed) through what was
just recorded, in real time.

## How it works

- **ScratchAudioEngine** (`Source/ScratchAudioEngine.*`) runs on the audio
  thread. It always writes incoming input audio into a 120-second ring
  buffer while recording is armed. Each block it reads the current jog-wheel
  motion and touch state from `MidiJogController`:
  - While the jog wheel is **touched**, output position is scrubbed directly
    by MIDI motion (classic turntable scratch, including reverse).
  - While **released**, playback free-runs forward at normal speed from
    wherever the scratch left off (like a CDJ platter kept spinning).
  - Output = scratched recording mixed with live input passthrough, so you
    always hear yourself plus the scratch effect.
- **MidiJogController** (`Source/MidiJogController.*`) opens a MIDI input
  device and has a "learn" mode: click *Learn Jog Wheel*, move the jog
  wheel, and it captures whichever CC number is moving. Click *Learn Touch
  Button*, press the platter, and it captures the touch-sensor note. This
  works with pretty much any DJ controller since jog wheel CC numbers vary
  by brand/model. It auto-detects relative-encoder (centred-on-64) vs.
  absolute CC value encoding.
- **MainComponent** (`Source/MainComponent.*`) is the GUI: audio device
  picker, MIDI device picker + learn buttons, record toggle, live
  monitor/scratch-output gain sliders, scratch sensitivity slider, and WAV
  export of the current buffer contents.

## Build

Requires CMake 3.22+ and a C++17 compiler. JUCE is fetched automatically via
CMake `FetchContent` (needs internet access on first configure).

```sh
cmake -B build -S .
cmake --build build --config Release
```

The built app is under `build/AudioScratcher_artefacts/Release/` (or
`Debug/`). On Windows this produces `AudioScratcher.exe`.

## Using it

1. Launch the app, pick your audio input/output device in the settings
   panel at the bottom, and select your MIDI controller from the MIDI
   device dropdown.
2. Click **Learn Jog Wheel**, then spin the jog wheel on your controller.
3. Click **Learn Touch Button**, then touch/press the jog wheel's platter
   (most controllers send a note on/off for this).
4. Click **Start Recording**.
5. Touch the jog wheel and scratch — you're scratching the live audio
   that's being recorded, played back with a short delay equal to however
   far back you scrub.
6. **Export Recording As WAV...** saves the current buffer to disk.

If your controller's jog wheel doesn't have a touch sensor (some cheaper
ones don't), skip step 3 — the app will just always play forward at normal
speed instead of scratching, since without touch it can't distinguish
"grab the record" from "let it spin." Controllers with only absolute
(non-relative) jog CCs are also supported automatically.
