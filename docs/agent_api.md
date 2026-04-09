# AgentVST Agent API Reference

This document is the authoritative reference for writing an AgentVST plugin. It covers everything an AI agent needs to produce a working, real-time-safe VST3/AU effect plugin.

---

## 1. Overview

An agent produces exactly two artifacts:

| Artifact | Purpose |
|---|---|
| `plugin.json` | Declares metadata, parameters, UI groups, and optional DSP routing |
| `*DSP.cpp` | Implements the per-sample audio processing logic |

The framework handles everything else: JUCE APVTS construction, UI generation, host transport, state serialization, parameter automation, and the audio thread watchdog.

**What the framework provides:**

- Parses `plugin.json` at compile time and builds the JUCE `AudioProcessorValueTreeState` from it.
- Generates a default parameter UI (sliders, toggles, combo boxes) automatically from the schema.
- Caches `std::atomic<float>*` pointers for every parameter so audio-thread reads are lock-free.
- Calls `prepare()` before audio processing begins so the agent can pre-allocate all memory.
- Calls `processSample()` for every channel of every sample in each audio buffer.
- Monitors execution time with a cooperative watchdog and bypasses the agent (passing audio through unchanged) if the budget is exceeded.
- Serializes plugin state to/from the DAW's project file automatically.

**What the agent must not do:**

- Dynamically allocate memory inside `processSample()`.
- Use blocking primitives (mutexes, file I/O) inside `processSample()`.
- Implement JUCE boilerplate, CMakeLists.txt content (beyond calling `agentvst_add_plugin()`), or UI code.

---

## 2. `plugin.json` Format

`plugin.json` is a UTF-8 encoded JSON file with four top-level keys. All keys are optional; defaults apply when absent.

```json
{
  "plugin":      { ... },
  "parameters":  [ ... ],
  "groups":      [ ... ],
  "dsp_routing": [ ... ]
}
```

Unknown top-level keys are silently ignored by the parser.

### 2.1 Plugin Metadata Block (`plugin`)

```json
"plugin": {
  "name":     "AgentCompressor",
  "version":  "1.0.0",
  "vendor":   "AgentVST",
  "category": "Dynamics"
}
```

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | string | `"AgentPlugin"` | Plugin display name. Used as the VST3 product name and in the UI header. Also seeds the deterministic plugin ID (see section 9). Must not change between releases. |
| `version` | string | `"1.0.0"` | Semantic version string. Informational only. |
| `vendor` | string | `"AgentVST"` | Company or developer name. Also seeds the deterministic plugin ID. Must not change between releases. |
| `category` | string | `"Fx"` | DAW category hint. Common values: `"Fx"`, `"Dynamics"`, `"EQ"`, `"Reverb"`, `"Delay"`. |

### 2.2 Parameters Array (`parameters`)

An array of parameter objects. Each object requires `id` and `name`; remaining fields depend on `type`.

**Common fields (all types):**

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `id` | string | Yes | — | Machine identifier used in DSP code with `ctx.getParameter("id")`. Must be unique across all parameters. Valid identifier characters only. |
| `name` | string | Yes | — | Human-readable label shown in the UI and automation lane. |
| `type` | string | No | `"float"` | Parameter type. One of: `"float"`, `"int"`, `"boolean"`, `"enum"`. |
| `unit` | string | No | `""` | Display unit appended to the value string: `"dB"`, `"Hz"`, `"ms"`, `"%"`, `":1"`, etc. |
| `group` | string | No | `""` | Group ID this parameter belongs to. References an entry in the `groups` array. |

#### `type: "float"` — Continuous floating-point parameter

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `min` | number | Yes | — | Minimum value. Must be strictly less than `max`. |
| `max` | number | Yes | — | Maximum value. |
| `default` | number | No | `min` | Initial value. Must satisfy `min <= default <= max`. |
| `step` | number | No | `0.0` | Quantization step. `0.0` means continuous. Passed directly to JUCE `NormalisableRange`. |
| `skew` | number | No | `1.0` | JUCE `NormalisableRange` skew factor. Values less than 1.0 concentrate resolution at the low end (useful for frequency and time parameters). Must be greater than 0. |

```json
{
  "id":      "cutoff_freq",
  "name":    "Cutoff",
  "type":    "float",
  "min":     20.0,
  "max":     20000.0,
  "default": 1000.0,
  "unit":    "Hz",
  "skew":    0.4,
  "group":   "filter"
}
```

#### `type: "int"` — Integer-stepped parameter

Identical fields to `"float"`. The parser does not enforce integer values for `min`, `max`, `default`, or `step`, but integer-safe values are expected. The UI displays zero decimal places, and JUCE's `NormalisableRange` applies the step.

```json
{
  "id":      "num_voices",
  "name":    "Voices",
  "type":    "int",
  "min":     1,
  "max":     8,
  "default": 4
}
```

#### `type: "boolean"` — Toggle parameter

The parser maps `true` → `1.0` and `false` → `0.0`. Use `ctx.getParameter("id") >= 0.5f` to test the state in DSP code. The JUCE parameter is created as `AudioParameterBool`.

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `default` | boolean | No | `false` | Initial state. Accepts JSON `true`/`false`. |

```json
{
  "id":      "bypass",
  "name":    "Bypass",
  "type":    "boolean",
  "default": false
}
```

#### `type: "enum"` — Discrete choice parameter

The JUCE parameter is created as `AudioParameterChoice`. The value returned by `ctx.getParameter()` is the zero-based index of the selected option.

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `options` | array of strings | Yes | — | Non-empty list of choice labels displayed in the UI combo box. |
| `default` | number | No | `0` | Zero-based index of the initial selection. Should be in `[0, options.length - 1]`. |

```json
{
  "id":      "filter_type",
  "name":    "Filter Type",
  "type":    "enum",
  "options": ["LowPass", "HighPass", "BandPass"],
  "default": 0,
  "group":   "filter"
}
```

### 2.3 Groups Array (`groups`)

Groups provide logical sections for the UI. Parameters listed in a group are rendered together under a section header.

```json
"groups": [
  { "id": "dynamics", "name": "Dynamics",  "parameters": ["threshold_db", "ratio", "knee_db"] },
  { "id": "envelope", "name": "Envelope",  "parameters": ["attack_ms", "release_ms"] },
  { "id": "output",   "name": "Output",    "parameters": ["makeup_db", "bypass"] }
]
```

| Field | Type | Required | Description |
|---|---|---|---|
| `id` | string | Yes | Unique group identifier. Referenced by the `group` field on parameter objects. |
| `name` | string | No (defaults to `id`) | Display label shown as the section header. |
| `parameters` | array of strings | No | Ordered list of parameter IDs to display in this group. Every ID must exist in the `parameters` array; the parser throws `ValidationError` for unknown references. |

Parameters not assigned to any group are rendered in an ungrouped section below named groups.

### 2.4 DSP Routing Array (`dsp_routing`)

`dsp_routing` is optional. When present, the framework builds a DSP graph using pre-built modules from the module library rather than calling the agent's `processSample()` directly. See [dsp_modules.md](dsp_modules.md) for the full module reference.

Each entry in the array describes one directed edge in the signal graph:

```json
"dsp_routing": [
  {
    "source":      "input",
    "destination": "main_filter",
    "parameter_link": {
      "cutoff_freq": "frequency",
      "resonance":   "q"
    }
  },
  {
    "source":      "main_filter",
    "destination": "output"
  }
]
```

| Field | Type | Required | Description |
|---|---|---|---|
| `source` | string | Yes | ID of the source node. Use the special string `"input"` for the audio input endpoint. `"input"` may only be used as a source, never a destination. |
| `destination` | string | Yes | ID of the destination node. Use the special string `"output"` for the audio output endpoint. `"output"` may only be used as a destination, never a source. |
| `parameter_link` | object | No | Maps schema parameter IDs (keys) to the destination node's parameter names (values). At each block the framework reads the current value of the schema parameter and calls `setParameter(name, value)` on the destination node. |

**Node type inference:** The framework infers which pre-built module to instantiate from the node ID string by substring matching (case-insensitive):

| Substring in node ID | Module instantiated |
|---|---|
| `filter`, `biquad` | `BiquadFilter` |
| `osc`, `lfo` | `Oscillator` |
| `env`, `follow` | `EnvelopeFollower` |
| `delay`, `echo` | `Delay` |
| `gain`, `amp`, `volume`, `level` | `Gain` |

If no substring matches, the router is disabled and audio passes through unmodified.

The router validates the graph at construction time: unknown node IDs, cycles, nodes unreachable from `input`, and nodes that cannot reach `output` all raise `DSPRouter::RoutingError` and disable the router.

---

## 3. The DSP Class

Every plugin must implement one class that inherits from `AgentVST::IAgentDSP`. Include only `<AgentDSP.h>`.

```cpp
#include <AgentDSP.h>

class MyProcessor : public AgentVST::IAgentDSP {
public:
    void  prepare      (double sampleRate, int maxBlockSize) override;
    float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override;
    void  reset        () override;
};

AGENTVST_REGISTER_DSP(MyProcessor)
```

### `void prepare(double sampleRate, int maxBlockSize)`

Called by the framework before audio processing begins, and again whenever the host changes the sample rate or maximum block size.

**When to use it:**

- Pre-compute all coefficients that depend on sample rate (filter cutoffs, envelope time constants, LFO phase increments).
- Allocate and size all internal buffers, history arrays, and delay lines. After `prepare()` returns, `processSample()` must not allocate.
- Reset all internal state by calling your own `reset()` implementation, or zeroing state directly.

**Example:**

```cpp
void prepare(double sampleRate, int /*maxBlockSize*/) override {
    sampleRate_ = sampleRate;
    // Pre-compute a 20ms smoothing coefficient
    smoothCoeff_ = std::exp(-1.0 / (sampleRate_ * 0.020));
    reset();
}
```

`maxBlockSize` tells you the largest number of samples the host will ever deliver in a single `processBlock()` call. Use it to size any per-block working buffers.

### `float processSample(int channel, float input, const DSPContext& ctx)`

The per-sample processing function. Called once per channel per sample on the audio thread. Must return the processed output sample.

| Parameter | Description |
|---|---|
| `channel` | Zero-based channel index. `0` = left, `1` = right. May be up to the number of channels the host configured. |
| `input` | The incoming audio sample for this channel and sample position. |
| `ctx` | Current plugin state: sample rate, transport info, and parameter values. See Section 4. |

**Return value:** The processed output sample. Returning `input` unchanged produces a bypass.

### `void reset()`

Called when the host rewinds the transport or the plugin is reset. Clear all stateful DSP data: filter histories, envelope integrators, oscillator phase, delay buffer contents, and gain smoothing state.

```cpp
void reset() override {
    for (auto& s : filterState_) s = {};
    envelopeLevel_ = 0.0f;
    smoothedGain_  = 1.0f;
}
```

---

## 4. `DSPContext` Struct

`DSPContext` is passed by const reference to every `processSample()` call. All fields are read-only from the agent's perspective.

### Audio Configuration

| Field | Type | Description |
|---|---|---|
| `sampleRate` | `double` | Current host sample rate in Hz (e.g., 44100.0, 48000.0, 96000.0). |
| `numChannels` | `int` | Number of audio channels in the current buffer. Typically 2 for stereo. |
| `numSamplesInBlock` | `int` | Total number of samples in the current block. Useful for per-block algorithms. |
| `currentSample` | `int` | Zero-based index of the current sample within the block (0 to `numSamplesInBlock - 1`). |

### Host Transport

| Field | Type | Description |
|---|---|---|
| `isPlaying` | `bool` | `true` when the host transport is running. |
| `bpm` | `double` | Host tempo in beats per minute. Defaults to 120.0 when unavailable. |
| `ppqPosition` | `double` | Current playback position in quarter-note beats since transport start. |

### Parameter Access

```cpp
float getParameter(const std::string& paramId) const noexcept;
```

Returns the current normalized or physical value of a parameter declared in `plugin.json`.

- For `"float"` and `"int"` parameters: returns the current physical value in the declared range.
- For `"boolean"` parameters: returns `1.0f` when on, `0.0f` when off.
- For `"enum"` parameters: returns the zero-based index of the selected option as a float.
- Returns `0.0f` if `paramId` is not found. Does not allocate, does not throw.

Reads are performed with `std::memory_order_relaxed` from a pre-cached `std::atomic<float>*`. This is lock-free and has negligible overhead even when called per-sample.

```cpp
float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
    if (ctx.getParameter("bypass") >= 0.5f)
        return input;

    const float gainDb = ctx.getParameter("gain_db");
    const float gainLin = std::pow(10.0f, gainDb / 20.0f);
    return input * gainLin;
}
```

### Internal Field

`_paramCache` is a framework-internal pointer set before every `processSample()` call. Do not access or modify it.

---

## 5. `AGENTVST_REGISTER_DSP` Macro

Place this macro exactly once, outside any namespace, at the bottom of the `.cpp` file that defines your DSP class. It must appear in the same translation unit as the class definition.

```cpp
AGENTVST_REGISTER_DSP(ClassName)
```

**What it does:** Declares a file-scope static object whose constructor calls `AgentVST::registerDSP()` with a lambda that returns `std::make_unique<ClassName>()`. This static initializer runs before `main()` (before the plugin is loaded by the host). The framework calls `getRegisteredDSPFactory()` during `AgentVSTProcessor` construction to instantiate the agent's class.

**Constraints:**

- Only one `AGENTVST_REGISTER_DSP` macro is allowed per plugin. Linking two `.cpp` files that each contain this macro causes a `std::logic_error` at load time.
- The class must be default-constructible.

```cpp
class GainProcessor : public AgentVST::IAgentDSP {
public:
    float processSample(int, float input, const AgentVST::DSPContext& ctx) override {
        return input * std::pow(10.0f, ctx.getParameter("gain_db") / 20.0f);
    }
};

AGENTVST_REGISTER_DSP(GainProcessor)
```

---

## 6. Real-Time Constraints

`processSample()` runs on the audio thread. The audio thread has a hard deadline: the entire buffer must be processed before the host needs the next one. Any operation that may block or take unpredictable time is **prohibited**.

### Prohibited Patterns

| Pattern | Why Prohibited | Correct Alternative |
|---|---|---|
| `new`, `delete`, `malloc`, `free` | May block on the heap allocator lock | Pre-allocate in `prepare()` |
| `std::vector` local variable | Constructor allocates on the heap | Declare as a member, resize in `prepare()` |
| `std::vector::push_back()` | May trigger a reallocation | Use a fixed-size `std::array` or member buffer |
| `std::vector::emplace_back()` | Same as `push_back` | Use a fixed-size `std::array` or member buffer |
| `std::cout`, `printf`, `fprintf` | Kernel syscall, takes unbounded time | Remove all I/O from `processSample()` |
| `JUCE_DBG()` macro | Calls `std::cerr` on the audio thread | Remove debug output from `processSample()` |
| `std::mutex`, `std::unique_lock` | Will block if another thread holds the lock | Use `std::atomic<>` for shared state |
| `std::lock_guard`, `std::scoped_lock` | Same as above | Use `std::atomic<>` |
| File I/O, network I/O | Unbounded blocking | Keep all I/O off the audio thread |

The `agentvst validate-dsp` command scans DSP source files for these patterns using regex matching and reports violations with line numbers.

---

## 7. Complete Working Example: Compressor

This is the actual compressor example from `examples/compressor/`. It demonstrates all API patterns in a realistic dynamics processor.

### `plugin.json`

```json
{
  "plugin": {
    "name":     "AgentCompressor",
    "version":  "1.0.0",
    "vendor":   "AgentVST",
    "category": "Dynamics"
  },
  "parameters": [
    {
      "id":      "threshold_db",
      "name":    "Threshold",
      "type":    "float",
      "min":     -60.0,
      "max":     0.0,
      "default": -18.0,
      "unit":    "dB",
      "group":   "dynamics"
    },
    {
      "id":      "ratio",
      "name":    "Ratio",
      "type":    "float",
      "min":     1.0,
      "max":     20.0,
      "default": 4.0,
      "unit":    ":1",
      "skew":    0.5,
      "group":   "dynamics"
    },
    {
      "id":      "attack_ms",
      "name":    "Attack",
      "type":    "float",
      "min":     0.1,
      "max":     200.0,
      "default": 10.0,
      "unit":    "ms",
      "skew":    0.4,
      "group":   "envelope"
    },
    {
      "id":      "release_ms",
      "name":    "Release",
      "type":    "float",
      "min":     10.0,
      "max":     2000.0,
      "default": 100.0,
      "unit":    "ms",
      "skew":    0.4,
      "group":   "envelope"
    },
    {
      "id":      "makeup_db",
      "name":    "Makeup Gain",
      "type":    "float",
      "min":     0.0,
      "max":     24.0,
      "default": 0.0,
      "unit":    "dB",
      "group":   "output"
    },
    {
      "id":      "knee_db",
      "name":    "Knee",
      "type":    "float",
      "min":     0.0,
      "max":     12.0,
      "default": 2.0,
      "unit":    "dB",
      "group":   "dynamics"
    },
    {
      "id":      "bypass",
      "name":    "Bypass",
      "type":    "boolean",
      "default": false,
      "group":   "output"
    }
  ],
  "groups": [
    { "id": "dynamics", "name": "Dynamics", "parameters": ["threshold_db", "ratio", "knee_db"] },
    { "id": "envelope", "name": "Envelope", "parameters": ["attack_ms", "release_ms"] },
    { "id": "output",   "name": "Output",   "parameters": ["makeup_db", "bypass"] }
  ]
}
```

### `CompressorDSP.cpp`

```cpp
#include <AgentDSP.h>
#include <cmath>
#include <algorithm>

class CompressorProcessor : public AgentVST::IAgentDSP {
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        updateCoefficients(10.0f, 100.0f);
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f)
            return input;

        const float threshDb  = ctx.getParameter("threshold_db");
        const float ratio     = std::max(1.0f, ctx.getParameter("ratio"));
        const float attackMs  = ctx.getParameter("attack_ms");
        const float releaseMs = ctx.getParameter("release_ms");
        const float makeupDb  = ctx.getParameter("makeup_db");
        const float kneeDb    = ctx.getParameter("knee_db");

        updateCoefficients(attackMs, releaseMs);

        // 1. RMS level detection (per-channel leaky integrator)
        const int ch = std::min(channel, kMaxCh - 1);
        const float xSq = input * input;
        rmsState_[ch] = rmsCoeff_ * rmsState_[ch] + (1.0f - rmsCoeff_) * xSq;
        const float rmsLin = std::sqrt(std::max(0.0f, rmsState_[ch]));

        // 2. Level in dB
        const float levelDb = (rmsLin > 1e-9f)
                                  ? 20.0f * std::log10(rmsLin)
                                  : -120.0f;

        // 3. Gain computer with soft knee
        float gainReductionDb = 0.0f;
        const float halfKnee  = kneeDb * 0.5f;
        const float overDb    = levelDb - threshDb;

        if (kneeDb > 0.0f && overDb > -halfKnee && overDb < halfKnee) {
            const float t = (overDb + halfKnee) / kneeDb;
            gainReductionDb = (1.0f / ratio - 1.0f) * overDb * t * t * 0.5f;
        } else if (overDb >= halfKnee) {
            gainReductionDb = (1.0f / ratio - 1.0f) * overDb;
        }

        // 4. Attack/release ballistics
        auto& grState = grState_[ch];
        if (gainReductionDb < grState)
            grState = attackCoeff_  * grState + (1.0f - attackCoeff_)  * gainReductionDb;
        else
            grState = releaseCoeff_ * grState + (1.0f - releaseCoeff_) * gainReductionDb;

        // 5. Apply gain reduction + makeup
        const float totalDb = grState + makeupDb;
        const float gainLin = std::pow(10.0f, totalDb / 20.0f);
        return input * gainLin;
    }

    void reset() override {
        for (auto& s : rmsState_) s = 0.0f;
        for (auto& s : grState_)  s = 0.0f;
    }

private:
    static constexpr int kMaxCh = 8;

    double sampleRate_   = 44100.0;
    float  attackCoeff_  = 0.0f;
    float  releaseCoeff_ = 0.0f;
    float  rmsCoeff_     = 0.0f;

    float rmsState_[kMaxCh] = {};
    float grState_ [kMaxCh] = {};

    void updateCoefficients(float attackMs, float releaseMs) noexcept {
        const float sr = static_cast<float>(sampleRate_);
        attackCoeff_  = std::exp(-1.0f / (sr * attackMs  * 0.001f));
        releaseCoeff_ = std::exp(-1.0f / (sr * releaseMs * 0.001f));
        rmsCoeff_     = std::exp(-1.0f / (sr * 0.050f));  // 50ms RMS window
    }
};

AGENTVST_REGISTER_DSP(CompressorProcessor)
```

---

## 8. Pre-Built DSP Modules

When a plugin does not need custom algorithm logic, it can use the DSP routing system to connect pre-built modules entirely through `plugin.json`. Available modules:

- **BiquadFilter** — Low-pass, high-pass, band-pass, notch, peak EQ, low shelf, high shelf
- **Gain** — Linear or dB gain with juce::SmoothedValue anti-zipper ramping
- **Delay** — Circular-buffer delay with feedback, wet/dry mix, and linear interpolation
- **EnvelopeFollower** — Peak or RMS envelope detector with separate attack and release
- **Oscillator** — Sine, sawtooth, square, and triangle with PolyBLEP anti-aliasing

Full documentation including all parameter names, direct API, and routing examples: [dsp_modules.md](dsp_modules.md).

---

## 9. CLI Tool

The `agentvst` command-line tool assists agents with scaffolding, validation, and building.

### `agentvst init`

Scaffolds a new plugin project directory with starter files.

```
agentvst init <PluginName> [--vendor <VendorName>] [--version <x.y.z>]
```

Creates:

```
<PluginName>/
├── plugin.json           (starter schema with gain_db and bypass parameters)
├── src/<PluginName>DSP.cpp  (starter DSP class with smoothed gain implementation)
└── CMakeLists.txt        (calls agentvst_add_plugin() with correct arguments)
```

**Example:**

```
agentvst init MyReverb --vendor "MyStudio" --version "1.0.0"
```

### `agentvst validate`

Parses and validates a `plugin.json` file. Prints a summary of all parameters and groups on success, or an actionable error message on failure. Exits with code 0 on success, 1 on any error.

```
agentvst validate <schema.json>
```

Catches:
- Malformed JSON (parse error with context)
- Duplicate parameter IDs
- Group entries referencing unknown parameter IDs
- Parameters with `min >= max` or `default` outside `[min, max]`

### `agentvst validate-dsp`

Scans DSP source files for prohibited real-time patterns using regex matching. Reports violations with file path, line number, the matched line, and a remediation suggestion. Exits 0 if clean, 1 if any violations are found.

```
agentvst validate-dsp <file.cpp> [file2.cpp ...]
```

Patterns checked: `new`, `malloc`, `calloc`, `realloc`, `std::vector` local variable, `.push_back()`, `.emplace_back()`, `std::cout`, `printf`, `fprintf`, `std::mutex`, `std::unique_lock`, `std::lock_guard`, `std::scoped_lock`, `DBG()`.

Comment lines (starting with `//` or `/*` after whitespace) are skipped.

### `agentvst build`

Validates the schema, generates a temporary `CMakeLists.txt`, then invokes CMake to configure and build the plugin.

```
agentvst build <schema.json> --sources <dsp.cpp> [--config Debug|Release]
```

- Default configuration is `Release`.
- Build artifacts are placed in `build_<PluginName>/build/`.
- Plugin IDs (VST3 CID, AU bundle) are generated deterministically from the vendor and plugin name — they will be stable across rebuilds as long as the `plugin` metadata does not change.

**Example:**

```
agentvst build plugin.json --sources src/CompressorDSP.cpp --config Release
```
