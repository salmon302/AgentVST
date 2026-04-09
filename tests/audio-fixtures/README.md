# Audio Fixture Acceptance Tests

This folder contains lightweight, deterministic audio acceptance tests used by CI.

The runner synthesizes an input signal and evaluates expected behavior from fixture files.

## Run

```bash
python tests/audio-fixtures/run_audio_acceptance.py
```

## Fixture Format

Each fixture JSON supports:

- `name`: test name
- `pluginSpec`: path to plugin schema JSON
- `model`: processing model name (`simple_gain_v1` or `compressor_v1`)
- `sampleRate`: sample rate for synthesis
- `durationSeconds`: test signal duration
- `frequencyHz`: sine frequency
- `inputAmplitude`: sine amplitude
- `parameters`: runtime parameter values
- `assertions.rmsRatio`: expected output/input RMS ratio with tolerance
- `assertions.maxAbs`: optional max absolute output check

This keeps acceptance tests stable without requiring DAW/plugin host execution.

## Models

- `simple_gain_v1`: static gain + bypass behavior.
- `compressor_v1`: feed-forward RMS compressor with soft knee, attack/release
  ballistics, and makeup gain (aligned to `examples/compressor/src/CompressorDSP.cpp`).
