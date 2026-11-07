# VANTA Delay Lab

A no-build, browser-local audio delay tester. Serve this directory with any static HTTP server and open `index.html`. No audio is uploaded, no microphone permission is requested, and there are no external scripts/assets.

```sh
python3 -m http.server 8081 --bind 0.0.0.0 --directory web/delay
```

Drop a browser-decodable audio file (PCM WAV recommended, limit 100 MB), or use the built-in pluck sound. Press Play. Controls apply to real audio via Web Audio: delay, filtered feedback, linear dry/wet, mid/side wet width, modulation and output trim. Ping-pong sums the input to mono, injects it on the left, and crosses feedback left/right. Sync uses the user-entered BPM, not a DAW transport. Pause lets the tail decay; Stop and seek clear the delay memory. Bypass is dry-only and retains the output gain.

**This is not a VST3 host or a port of the current VANTA FX DSP.** Its audio graph is a separate Web Audio implementation for interactive delay auditioning. Testing it does not establish native VST3 correctness or sound parity.

## Automated tests

Run `npm ci && npm test` inside this directory. Uses jsdom for UI and web-audio-engine to render the actual page's graph from an impulse. Checks echo timing/feedback decay, finite output, controls and transport. It is not a real-browser hardware/audio-output test. The page itself requires no npm dependencies.
