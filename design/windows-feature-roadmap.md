# VANTA — Windows feature roadmap

This roadmap is based on the requested synth feature list, not a statement that every feature already exists. The HTML concept was approved and remains a no-audio design preview; Beta 0.7 begins implementing it in the Windows plugin and standalone. The numbers below are design goals, not release promises or verified comparisons with other products.

## Targets

- Windows VST3 plugin and Windows standalone application. The existing CMake project and Windows workflow already target these two formats.
- Preserve the existing plugin and parameter IDs; plan and test migration of saved projects and presets as the engine grows.
- Keep the OSC workflow approachable, with deeper editing on the other pages. The interface should remain usable at narrow sizes and with a keyboard.

## Synthesis and sources

- Beta 0.7 implements up to eight oscillator layers per preset, with individual waveform, level, pan, semitone and fine tuning. Independent filter/envelope paths and modulation are still planned.
- Beta 0.7 implements virtual-analog sine, saw, square and triangle. Sub and noise, unison voices and stereo spread remain planned.
- Wavetable synthesis with a visual editor, import, 2D/3D views, up to 256 frames as a design target, and formula/text/audio-to-wave exploration.
- User-provided PCM samples, multisamples and SFZ; granular and spectral/resynthesis engines where technically feasible. Import formats and asset handling need separate engineering and compatibility testing; no commercial sample libraries are included or implied.
- Warp options including FM, AM, phase, bend, sync, asymmetry, remap and PWM; explore spectral/formant, vocoder-like, harmonic shifting and squeeze modes.
- Per-layer amp, pitch and filter envelopes and routing; additional multimode and analog-style filters, including high-pass, band-pass, peak, comb, formant and ladder-style designs. Explore dual filters with series/parallel and per-layer routing.

## Modulation and performance

- Design targets: up to four envelopes, ten LFOs and eight macros. Include drawable/step LFOs, tempo sync, stereo offset and alternative shapes where practical.
- A modulation matrix, intuitive assignment, key tracking and useful visual feedback. Per-layer targets and ranges require a real parameter model and DSP implementation.
- MIDI learn, configurable pitch bend, mod wheel and expression. Maintain velocity, sustain and host MIDI support.

## Effects and routing

- Expand the rack beyond the current reverb, delay and distortion: chorus, flanger, phaser, EQ, multiband/OTT-style compression, filter, dimension, additional delay/reverb/distortion flavors and amp-style processing.
- Reorderable and multiple instances, per-layer/FX-bus routing and multiband routing are goals, not capabilities of the current engine.
- Beta 0.7 has a dedicated mixer for layer gain, pan and enabling/disabling additional oscillators. Advanced layer/FX-bus routing remains planned.

## Sequencing and preset workflow

- A 32-step arpeggiator and a 64-step clip/automation sequencer, with tempo sync and appropriate per-step controls. The HTML grids are visual sketches only and cannot play or automate anything.
- Searchable preset navigation, categories and tags, plus user preset management. The HTML currently searches only the five displayed factory entries; its changes are in-memory previews.
- Keep native `.vanta` presets, and document any import conversion honestly. The existing Vital importer is partial; Serum and ZENOLOGY presets are not currently supported. Future import work needs separate format research, licensing review and tests, not a promise of full compatibility.

## Current baseline and approval boundary

Beta 0.7's native engine has up to eight sine/saw/square/triangle oscillators, per-layer level/tuning/pan, 16 voices, velocity, sustain and pitch bend, a shared low-pass filter and ADSR amplitude envelope, sine pitch vibrato, reverb/delay/distortion, five factory presets and native preset save/load. Additional source types, per-layer filter/envelope paths, general modulation routing, playable sequencers, extra effects and macros are not implemented. The HTML's OSC tabs, sequence grids and expanded effects rack remain session-only visual demonstrations; reloading clears them, and the sequencer and effects sketches are not saved per preset.

Further implementation should be staged and tested in the native engine, editor, Windows VST3/standalone builds and an actual DAW/audio/MIDI setup before claiming support.
