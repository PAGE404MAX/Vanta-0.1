# VANTA — Windows feature roadmap

This roadmap is based on the requested synth feature list, not a statement that every feature already exists. The HTML concept was approved and remains a no-audio design preview; Beta 0.7 began implementing it in the Windows plugin and standalone. The numbers below are design goals, not release promises or verified comparisons with other products.

## Targets

- Windows VST3 plugin and Windows standalone application. The existing CMake project and Windows workflow already target these two formats.
- Preserve the existing plugin and parameter IDs; plan and test migration of saved projects and presets as the engine grows.
- Keep the OSC workflow approachable, with deeper editing on the other pages. The interface should remain usable at narrow sizes and with a keyboard.

## Synthesis and sources

- Beta 0.7 implements up to eight oscillator layers per preset, with individual waveform, level, pan, semitone and fine tuning. Independent filter/envelope paths and an editable modulation matrix are still planned; Beta 0.8 can play a subset of imported LFO routes.
- Beta 0.7 implements virtual-analog sine, saw, square and triangle. Dedicated sub/noise generators, unison voices and stereo spread remain planned; Beta 0.8 can approximate a source sub with a basic shape and play supported embedded noise samples.
- Beta 0.8 plays up to 16 imported wavetable frames per oscillator; a visual editor, standalone wavetable import, 2D/3D views, up to 256 frames as a design target, and formula/text/audio-to-wave exploration remain planned.
- Beta 0.8 can play one embedded PCM sample from supported presets. Direct sample import, multisamples and SFZ, and granular and spectral/resynthesis engines remain planned where feasible; no commercial sample libraries are included or implied.
- Warp options including FM, AM, phase, bend, sync, asymmetry, remap and PWM; explore spectral/formant, vocoder-like, harmonic shifting and squeeze modes.
- Per-layer amp, pitch and filter envelopes and routing; additional multimode and analog-style filters, including high-pass, band-pass, peak, comb, formant and ladder-style designs. Explore dual filters with series/parallel and per-layer routing.

## Modulation and performance

- Design targets: up to four envelopes, ten LFOs and eight macros. Include drawable/step LFOs, tempo sync, stereo offset and alternative shapes where practical.
- Beta 0.13 adds Delay and Hold to the shared amplitude envelope; further envelopes and curve editing remain planned.
- A modulation matrix, intuitive assignment, key tracking and useful visual feedback. Per-layer targets and ranges require a real parameter model and DSP implementation.
- MIDI learn, configurable pitch bend, mod wheel and expression. Maintain velocity, sustain and host MIDI support.

## Effects and routing

- Beta 0.12 expands the native rack to chorus, three-band dynamics, delay, distortion, EQ, post-filter, flanger, phaser and reverb. Dimension, additional delay/reverb/distortion flavors, amp-style processing and flexible routing remain planned. Laggify is a separate 16-step time/volume VST3 mixer insert with its own installer; it is not inside the VANTA synth or Gross Beat-compatible.
- Reorderable and multiple instances, per-layer/FX-bus routing and multiband routing are goals, not capabilities of the current engine.
- The mixer controls layer gain, pan and enable state, including OSC A in Beta 0.8. Advanced layer/FX-bus routing remains planned.

## Sequencing and preset workflow

- A 32-step arpeggiator and a 64-step clip/automation sequencer, with tempo sync and appropriate per-step controls. The HTML grids are visual sketches only and cannot play or automate anything.
- Searchable preset navigation, categories and tags, plus user preset management. The HTML currently searches only the five displayed factory entries; its changes are in-memory previews.
- Keep native `.vanta` presets and document conversion honestly. Beta 0.8 adds partial Vital, Serum 1 and Serum 2 conversion, with warnings and significant limitations; ZENOLOGY remains unsupported. Broader compatibility needs real preset fixtures, licensing review and audible testing, not a promise of full compatibility.

## Current baseline and approval boundary

Beta 0.13's native engine has up to eight layers, basic shapes or supported imported wavetables and one PCM sample, 16 voices, velocity, sustain and pitch bend, a shared low-pass filter and DAHDSR amplitude envelope, sine pitch vibrato, limited imported LFO routes, and nine effects. Laggify is a separate mixer-insert plugin. Per-layer filter/envelope paths, an editable general modulation matrix, note sequencers and macros remain unimplemented. The HTML is a design reference, not the running instrument; use the native VANTA editor for oscillator and rack controls, and the separate Laggify editor for its insert controls.

Further implementation should be staged and tested in the native engine, editor, Windows VST3/standalone builds and an actual DAW/audio/MIDI setup before claiming support.
