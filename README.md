# Vanta Beta 0.4

VANTA is an experimental Windows x64 VST3 instrument for FL Studio and other
VST3 hosts. Its native JUCE interface is based on the included `UI prototype.html`.
This beta is a fresh foundation, **not** a finished recreation of other synths.

## What works

- 16-voice polyphony; sine and band-limited saw; MIDI velocity, sustain pedal,
  and pitch bend (fixed at +/- 2 semitones).
- Master level, low-pass cutoff/resonance, amplitude ADSR and vibrato.
- Reverb, delay and distortion; their controls affect the audio.
- Five small VANTA factory sounds, plus saving and opening `.vanta` preset files.
- Saving and restoring plugin parameter state in the host. The Beta 0.3 plugin
  ID and its four parameter IDs are retained, but migration has **not yet been
  tested in FL Studio**. Back up your projects before trying it.

The OSC page exposes one oscillator. The other oscillator slots, extra partials,
unison, legato, portamento, UI scale and advanced modulation routing are not
implemented and are labelled/disabled rather than pretending to work.

## Preset compatibility

- **VANTA `.vanta`:** Native parameter presets can be written and opened with
  the buttons in the plugin. These store the VANTA parameters, not audio files.
- **Vital `.vital`:** Experimental, partial **approximation** only. It imports
  available amplitude envelope attack/decay/sustain/release and filter-1 cutoff
  when that filter is enabled, with values clamped to VANTA's ranges. VANTA uses
  a saw oscillator; Vital wavetables, samples, additional oscillators, envelope
  curves, modulation, filter models and effects are not imported. The resulting
  sound will usually be different. The importer displays a warning each time.
- **Serum 1 `.fxp`, Serum 2 `.SerumPreset`, and ZENOLOGY `.svz` / `.svd` /
  `.bin`:** Not implemented. Selecting one reports that it is unsupported and
  leaves the current sound unchanged. Their preset formats and synthesis engines
  are different; future work needs format-specific research, legally obtained
  example presets and comparison against the original instruments.

Only import files you trust. VANTA does not modify imported files. Preset files
larger than 32 MB are rejected. No third-party preset banks, wavetables or samples
are included.

## Build and test on Windows

The GitHub Actions workflow **Build Windows VST3 and Installer** runs the engine
and preset tests, builds the VST3 and unsigned Inno Setup installer, and performs
a silent install/uninstall smoke test in the runner's temporary directory. It
compares the installed VST3 files with the build; it does **not** test FL Studio
or confirm audio/UI behavior in a DAW. Download the `Vanta-Beta-0.4-Windows`
artifact from a successful run on the branch containing this version.

For a local build, install Visual Studio C++ tools, CMake 3.22+ and Inno Setup 6:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target VantaTests --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target VantaBeta_VST3 --parallel
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" installer.iss
```

The installer is `installer-output\Vanta-Beta-0.4-Setup.exe` and installs the
complete VST3 bundle under `C:\Program Files\Common Files\VST3`. It is unsigned.
Back up FL Studio projects first; uninstall older VANTA betas before installing
this one to avoid two bundles with the same VST3 ID, then rescan plugins in FL
Studio. Start with low volume. Test the UI, MIDI, controls, preset round-trip,
and project save/reopen in your host. Do not distribute before reviewing JUCE's
licensing and any third-party licensing obligations.
