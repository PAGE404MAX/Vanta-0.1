# VANTA FX — native development prototype

Independent Windows stereo mixer-insert VST3, unique identity `com.vanta.fx` / `Vnta` / `Vfxa`. Does not replace VANTA synth, Sidechain, or Laggify. No Valhalla code or algorithms are included.

Approved visual reference: `../design/vanta-fx-concept.html`.

## Implemented foundation

- Nine native section tabs with per-engine parameter IDs and retained values.
- Host automation and XML project-state serialization.
- Shared dry/wet, smoothed output trim and soft bypass.
- Actual output waveform via atomics (not the HTML's simulated signal).
- Preallocated, interpolated stereo delay network, damping, pitch-grain feedback for shimmer, modulation, character delay saturation, diffuse reverb paths.
- Standalone C++ engine smoke tests: finite output, nonzero tails and reset at 44.1/48/96 kHz.

## Not release-ready

This is the first native prototype, not a completed nine-effect product. The native layout is a fixed-size initial translation of the approved design. Spectrum/space views, preset browser, resize support and final visual polish remain. Frequency Echo currently uses amplitude modulation (not a true analytic frequency shifter). Plate/Room/Vintage share a preliminary feedback network and need distinct tuned algorithms. Some exposed advanced controls are reserved and not yet connected to DSP (including dedicated early/late shaping, predelay, multi-tap count, multiband EQ and era behavior). They must be completed or removed before release. Engine switching currently shares delay storage; crossfades and independent reset policy remain. No tempo transport integration yet (sync uses a placeholder 120 BPM).

Windows VST3/editor compilation, host loading, automation/state tests and listening/CPU tests are not yet verified. This product is deliberately **not** added to the Suite installer until those gates pass. Existing suite installers/uninstallers are unchanged.

## Build

```sh
cmake -S "VANTA FX" -B build/fx
cmake --build build/fx --config Release
ctest --test-dir build/fx -C Release --output-on-failure
```

Windows workflow: `.github/workflows/vanta-fx-windows.yml` (must be pushed before dispatch).
