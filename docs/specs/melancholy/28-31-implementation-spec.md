# Melancholy & Mathematical Dualism — Implementation Spec (28–31)

This document converts concepts 28–31 from `VSTS_SRS.md` into implementation-ready specifications: DSP architecture, algorithms, exact parameter lists (with ranges/units), latency/PDC notes, test plan, and a short implementation roadmap.

Common implementation constraints and tools
- Platform: JUCE/C++ preferred (existing repo). Keep real-time-safe code in audio thread. Use lock-free single-producer single-consumer queues for GUI<->audio control where needed.
- Pitch detection: YIN or autocorrelation for monophonic; FFT-peak / harmonic-product for partial extraction.
- Pitch shifting / micro-tuning: Phase-vocoder (high-res) or granular for low-latency micro-shifts.
- Granular buffer: circular FIFO with adjustable grain size (5–200 ms).
- Testing: unit-testable helper functions + short audio test vectors (sine, saw, chords) and offline verification scripts.

1) `Dualism` — Undertone Anchor (Period vs Frequency)

Overview
- Detect fundamental period T and synthesize undertones at integer multiples of T (2T..5T) producing f/2..f/5. Blend with overtone synthesis.

DSP architecture
- Input -> Pre-filter (low cut 20 Hz) -> Peak-pitched monophonic detector (YIN) -> Fundamental freq estimate f
- Undertone synth: bank of band-limited sine/partial oscillators at f/n (n=2..5) with independent envelope per partial.
- Optionally derive undertone via time-domain resampling: read input buffer at slower rate to generate undertone texture.
- Mixer: `Overtone/Undertone Bias` controls crossfade; `Gravity` controls undertone amplitude and decay time.

Parameters (suggested ranges)
- `Overtone/Undertone Bias`: -1.0 (pure overtone) to +1.0 (pure undertone), default 0.0
- `Gravity`: 0.0–1.0 (linear), default 0.6 — also scales partial decay (50ms–3s)
- `Undertone Lowpass`: 100Hz–2000Hz (controls warmth)
- `Detection Sensitivity`: 0–1 (affects YIN threshold)

Latency / PDC
- Pitch detection buffer: 32–256 ms depending on accuracy/lag tradeoff. Provide a `Low-Latency` mode using smaller buffer (e.g., 32–64 ms) at the cost of pitch noise.
- No mandatory host PDC for prototype; document PDC recommendation for final plugin if look-ahead required.

Verification
- Test with single-note sine (A4) and verify spectral peaks at f/2..f/5.
- Confirm bias control crossfades cleanly without clicks.

Implementation notes
- Files: `examples/Dualism/*` or `plugins/Dualism/*` with audio processor class `DualismProcessor` and editor `DualismEditor`.
- Use existing repo oscillator helpers or implement band-limited sine via minBLEP or wavetables.

2) `Thread of Memory` — Mediant Splitter

Overview
- Detect chord changes, compute common tone(s), freeze common tone into a grain buffer, pitch-shift remaining voices by ratios (`6:5` or `5:4`), output combined result.

DSP architecture
- Input -> Polyphonic pitch/chord tracker (audio-to-MIDI style or FFT chord estimation) -> On chord-change: compute intersection of pitch sets -> Isolate common-tone via band-pass around that frequency -> Capture into granular buffer (configurable length)
- Pitch-shift remainder: implement spectral pitch-shift (phase vocoder) or high-quality time-domain pitch shifting for musical quality.

Parameters
- `Common-Tone Freeze` (ms): 0–3000 ms, default 500 ms
- `Mediant Mode`: enum {6:5, 5:4}
- `Wet/Ghost Mix`: 0–1
- `Transition Crossfade`: 0–2000 ms

Latency / PDC
- Chord-detection uses short analysis windows (50–200 ms). Document expected detection lag.

Verification
- Feed progressions that share a common tone (Cmaj -> Am) and verify the frozen tone persists while others shift.

Implementation notes
- Reuse pitch-tracking modules used elsewhere in the repo. Provide a CPU-saver bypass that falls back to simpler FFT chord estimation if polyphonic detector is unavailable.

3) `Lacrimosa` — 6:5 Modulator

Overview
- Detect the 3rd partial relative to a root, micro-pitch it toward exact 6:5 just ratio and add amplitude modulation at beat frequency between current and target pitch to create a sobbing tremolo.

DSP architecture
- Input -> Root/tonic detection (audio-to-MIDI or user-set root) -> Partial extractor (narrow band-pass around expected 3rd partial) -> Micro-pitch shift via phase vocoder or granular shift -> AM oscillator at beat frequency = |f_temper - f_6:5|

Parameters
- `Intensity` 0–1 (how strongly partial is moved toward 6:5)
- `Sob Depth` 0–1 (AM depth)
- `Tracking Speed` 10 ms – 2000 ms (smoothing of partial follow)

Latency / PDC
- Partial extraction needs small FFTs (e.g., 2048 @ 44100 ≈ 46ms) — expect ~30–100ms detection latency depending on FFT size.

Verification
- Test with root C and sung/stable 3rd; measure micro-pitch shift accuracy and AM frequency computed from beat frequency.

Implementation notes
- Provide user override for `Unity Root` if automatic root detection fails.

4) `Reluctant Resolution` — Temporal Suspension

Overview
- On chord changes, capture the highest-frequency transient of previous chord, delay it (lag), then glide its pitch downward by a configurable interval over a configurable time.

DSP architecture
- Input -> Transient detector (high-band emphasis + envelope follower) -> On chord-change or transient matched: isolate partial via band-pass -> Place isolated partial into dedicated delay/granular buffer -> Retrigger buffer output after `Lag Amount` with a pitch glide (portamento) applied via small-window granular or phase-vocoder.

Parameters
- `Catch Threshold` (transient sensitivity): 0–1
- `Lag Amount` (ms): 0–1000 ms
- `Resolution Glide`: interval (-12..+12 semitones) and `Glide Time` (10 ms – 5000 ms)

Latency / PDC
- The effect requires buffer-based delay; host PDC recommended if look-ahead is implemented. Otherwise, document expected additional output latency equal to `Lag Amount`.

Verification
- Test with rapid chord changes; verify the captured transient is delayed and glides down smoothly into the new chord.

Implementation roadmap (short term)
1. Create spec and SNDEV log (this file + SNDEV impl file).
2. Scaffold `Dualism` plugin directory with `DualismProcessor` and `DualismEditor`.
3. Implement a testable `PitchDetector` utility (YIN) and an `UndertoneBank` oscillator class.
4. Unit test `PitchDetector` and `UndertoneBank` using synthetic tones.
5. Integrate into JUCE example and run build in Debug.

Notes and risks
- Polyphonic detection (Thread of Memory) is more complex and may require fallback modes.
- Performance: high-resolution FFTs and phase vocoders are CPU-heavy; provide quality vs. CPU presets.

If you want, I can now scaffold `Dualism` plugin files and implement the `PitchDetector` + `UndertoneBank` prototype. Which step should I execute next?
