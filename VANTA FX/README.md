# VANTA FX — native development prototype

Independent Windows stereo mixer-insert VST3, unique identity `com.vanta.fx` / `Vnta` / `Vfxa`. Does not replace VANTA synth, Sidechain, or Laggify. No Valhalla code or algorithms are included.

Approved visual reference: `../design/vanta-fx-concept.html`.

## Implemented foundation

- Nine native section tabs with per-engine parameter IDs and retained values.
- Host automation and XML project-state serialization.
- Shared dry/wet, smoothed output trim and soft bypass.
- Original approved crimson/black FX design translated to a native editor: nine tabs, factory variations, rotary knobs and right-hand engine/output card. No browser file loader, player or audio-tester UI.
- Live stereo waveform, FFT spectrum and stereo-space display via atomics; display pause does not pause audio.
- Resizable editor with control-bounds checks at 70% size; every knob is attached to its stable host parameter.
- Native stereo Delay path with interpolated repeat timing, smoothed feedback/time/mix/trim, echo low/high filtering, spread, wet width, modulation, drive, diffusion and mode-dependent age/character; separate dry path.
- Preallocated preliminary diffuse networks for the other effect families.
- Standalone C++ engine smoke tests: finite output, nonzero tails and reset at 44.1/48/96 kHz.

## Not release-ready

This is the first native prototype, not a completed nine-effect product. The native layout now follows the approved design and supports resizing, factory variations, and three real audio visualization views. User-file preset browsing and final visual polish remain. Frequency Echo currently uses amplitude modulation (not a true analytic frequency shifter). Plate/Room/Vintage share a preliminary feedback network and need distinct tuned algorithms. Some exposed advanced controls are reserved and not yet connected to DSP (including dedicated early/late shaping, reverb predelay, multi-tap count, multiband EQ and vintage era behavior). They must be completed or removed before release. Engine switching currently shares delay storage; crossfades and independent reset policy remain. No tempo transport integration yet (sync uses a placeholder 120 BPM).

Windows VST3 compilation, native editor creation/snapshots, processor audio, project-state round trips and Suite install/uninstall/upgrade tests passed in [run 42319172173](https://github.com/PAGE404MAX/Vanta-0.1/actions/runs/42319172173). FX is included as an **early development effect** in Suite 0.15.0. FL Studio host loading, real listening/CPU tests and complete host automation remain manually unverified. The installer guards the FX bundle against unsafe paths and unverified replacements; the shared uninstaller removes its installed files without recursive deletion.

## Build

```sh
cmake -S "VANTA FX" -B build/fx
cmake --build build/fx --config Release
ctest --test-dir build/fx -C Release --output-on-failure
```

Windows workflow: `.github/workflows/vanta-fx-windows.yml` (must be pushed before dispatch).


Native Delay regression tests additionally check 300/600 ms repeat timing, decaying 50% feedback, zero feedback, no early wet leakage, and dry bypass. The browser app is a separate audition tool, not embedded in the VST, and is not used to prove VST audio parity.

Latest [combined Suite installer](https://github.com/PAGE404MAX/Vanta-0.1/actions/runs/42319172173/artifacts/13552059125) and [actual native UI test snapshots](https://github.com/PAGE404MAX/Vanta-0.1/actions/runs/42319172173/artifacts/13551534037). Open the Delay tab to audition the new native delay path. If an earlier development Suite is installed and repair refuses its unrecognized hashes, back up projects, close the DAW, and uninstall that Suite through its own wizard before installing this build. Do not bypass the file-verification guard.
