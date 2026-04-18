# AgentVST: Project Development Plan

**Document Version:** 1.0
**Date:** 2026-03-28
**Status:** Initial Draft
**Companion Document:** [SRS.md](SRS.md)
**Research Basis:** [AI-Assisted JUCE Plugin Development.md](AI-Assisted%20JUCE%20Plugin%20Development.md)

---

## 1. Project Overview

AgentVST is a C++ framework built on JUCE 8 that enables AI agents to produce production-ready audio plugins (VST3, AU, AAX) through declarative configuration and sandboxed DSP authoring. The framework treats the AI as an **untrusted configuration engine**—not a senior C++ engineer—and systematically neutralizes every documented LLM failure mode in real-time systems programming.

### 1.1 Key Research-Driven Design Decisions

The following architectural decisions are mandated by the research findings and are non-negotiable constraints on all implementation phases:

| Decision | Research Justification |
|----------|----------------------|
| Agent never writes raw JUCE C++ | LLMs exhibit fact-conflicting hallucinations (deprecated APIs like `ScopedPointer`, `createAndAddParameter`) and input-conflicting hallucinations (ignoring real-time constraints despite explicit instructions). |
| JSON schema for parameters, not C++ `ParameterLayout` | APVTS construction requires verbose `std::unique_ptr<RangedAudioParameter>` chains with `NormalisableRange`, skew lambdas, and string conversion functions—all high-hallucination-risk C++ template code. |
| `processSample(float)` sandbox, not `processBlock` access | Agents default to `std::vector::push_back`, `std::map::insert`, `printf`, and `new` inside audio callbacks. The framework must physically prevent heap allocation on the real-time thread. |
| All memory pre-allocated in `prepareToPlay` | The audio thread budget at 96 kHz / 64 samples is **0.66 ms**. Any OS heap call has unbounded latency due to non-deterministic preemptive scheduling. |
| `std::memory_order_relaxed` for parameter atomics | LLMs default to `seq_cst` (pipeline stall) or attempt custom spinlocks. Relaxed ordering is optimal for continuous audio parameters where microsecond staleness is acceptable. |
| JUCE 8 WebView for UI (HTML/CSS/JS) | LLMs are vastly more proficient in web languages. C++ bounds math (`removeFromTop`, `FlexItem`) causes overlapping components and DPI bugs. JUCE 8's `WebSliderParameterAttachment` bridges C++ APVTS to JS state natively. |
| Deterministic ID generation in CMake | If an agent regenerates a VST3 CID or AU Bundle ID, all existing DAW projects using the plugin break permanently. IDs must be derived once and cached. |
| PACE Cloud 2 Cloud for AAX signing in CI/CD | AAX requires cryptographic signing via physical iLok or PACE Cloud credentials. No LLM can navigate the Avid developer approval process or physical hardware. |

---

## 2. Technology Stack

| Component | Technology | Rationale |
|-----------|-----------|-----------|
| **Core Framework** | C++17 | JUCE 8 minimum requirement; `std::optional`, `std::variant`, structured bindings |
| **Audio Framework** | JUCE 8.x | WebView UI support, latest VST3/AU/AAX wrappers, `WebSliderParameterAttachment` |
| **Schema Parsing** | nlohmann/json (header-only) | Industry-standard C++ JSON; no runtime dependencies; compile-time validation possible |
| **Build System** | CMake 3.22+ | JUCE 8 native CMake support via `juce_add_plugin`; cross-platform |
| **UI Runtime** | HTML5 / CSS3 / Vanilla JS (or React) | JUCE 8 `WebBrowserComponent` with native bridge; hot-reload via Vite/Webpack dev server |
| **Testing** | Catch2 + custom audio test harness | Unit testing for DSP modules; integration testing for parameter binding |
| **CI/CD** | GitHub Actions | Multi-platform matrix builds (Windows MSVC, macOS Clang); PACE Cloud signing |
| **Package Manager** | CPM.cmake or git submodules | JUCE + nlohmann/json dependency management |

---

## 3. Development Phases

### Phase 0: Project Scaffolding & Build Infrastructure
**Duration:** 2 weeks
**Priority:** Critical — all other phases depend on this

#### Objectives
- Establish the CMake project structure with JUCE 8 as a dependency
- Create the minimal plugin wrapper that compiles to VST3 + AU + Standalone
- Implement deterministic plugin ID generation and caching
- Set up CI/CD pipeline for automated cross-platform builds

#### Tasks

| # | Task | SRS Req | Deliverable |
|---|------|---------|-------------|
| 0.1 | Initialize CMake project with `juce_add_plugin` | 4.1 | `CMakeLists.txt` with VST3, AU, Standalone targets |
| 0.2 | Create `PluginProcessor.h/cpp` skeleton (empty `processBlock`) | — | Compiles and loads in DAW as silent pass-through |
| 0.3 | Create `PluginEditor.h/cpp` skeleton with `WebBrowserComponent` | — | Opens empty WebView window in DAW |
| 0.4 | Implement deterministic ID generator | 4.2 | `agentvst_ids.cmake` module; generates stable VST3 CID from plugin name + vendor hash |
| 0.5 | Configure GitHub Actions CI matrix | — | `.github/workflows/build.yml`; Windows x64 + macOS arm64/x64 |
| 0.6 | Add AAX target (unsigned, Developer build only) | 4.1 | AAX compiles; document PACE signing integration point for Phase 6 |
| 0.7 | Define project directory structure | — | See Section 4 below |

#### Acceptance Criteria
- `cmake --build` produces VST3 + AU + Standalone on macOS, VST3 + Standalone on Windows
- Plugin loads in REAPER/Ableton without crash
- CI pipeline green on both platforms

---

### Phase 1: Declarative State Schema Engine (Layer 1)
**Duration:** 3 weeks
**Priority:** Critical — DSP and UI layers depend on this

#### Objectives
- Build the JSON schema parser and validator
- Auto-generate APVTS `ParameterLayout` from schema at initialization
- Cache `std::atomic<float>*` pointers for real-time-safe parameter access
- Implement state serialization (save/load) via APVTS ValueTree + secondary non-parameter tree

#### Tasks

| # | Task | SRS Req | Deliverable |
|---|------|---------|-------------|
| 1.1 | Define JSON schema specification (formal JSONSchema or documented format) | 1.1 | `docs/schema_spec.md`; covers all parameter types, groups, metadata |
| 1.2 | Implement `SchemaParser` class | 1.1 | Parses + validates JSON; reports actionable errors for malformed input |
| 1.3 | Implement `ParameterLayoutBuilder` | 1.2 | Converts parsed schema → `juce::AudioProcessorValueTreeState::ParameterLayout` |
| 1.4 | Handle `NormalisableRange` with skew, step, unit conversion | 1.1 | Logarithmic frequency scaling, dB conversion, percentage mapping |
| 1.5 | Implement `ParameterCache` | 1.3 | Stores `std::atomic<float>*` per parameter; initialized once at construction; padded to avoid false sharing on 64-byte cache lines |
| 1.6 | Implement parameter grouping | 1.4 | `juce::AudioParameterGroup` hierarchy from schema `"groups"` field |
| 1.7 | Implement `StateSerializer` | 4.3 | `getStateInformation` / `setStateInformation` using APVTS XML + secondary ValueTree for non-parameter state |
| 1.8 | Implement non-parameter state store | 4.3 | Message-thread-only key-value store for strings/binary; sandboxed from real-time thread |
| 1.9 | Write unit tests for schema parsing | 5.2.2 | Invalid schemas produce clear error messages; edge cases (empty params, duplicate IDs, out-of-range defaults) |
| 1.10 | Write integration test: schema → APVTS → save → reload → verify values | 4.3 | Round-trip serialization test |

#### Technical Design Notes

**Parameter Access Pattern (real-time safe):**
```cpp
// Framework initialization (constructor, message thread)
for (auto& param : schema.parameters) {
    auto* rawPtr = apvts.getRawParameterValue(param.id);
    parameterCache.store(param.id, rawPtr); // std::atomic<float>*
}

// Framework processBlock (audio thread) — agent never sees this
float cutoff = parameterCache.load("cutoff_freq"); // memory_order_relaxed
```

**Cache-line padding to prevent false sharing:**
```cpp
struct alignas(64) PaddedAtomicFloat {
    std::atomic<float>* ptr;
    char padding[64 - sizeof(std::atomic<float>*)];
};
```

**Non-parameter state isolation:**
```cpp
// Secondary ValueTree — message thread ONLY
juce::ValueTree nonParamState {"NonParameterState"};
// Aggregated into main state tree for serialization
juce::ValueTree rootState {"AgentVSTState"};
rootState.addChild(apvts.state, -1, nullptr);
rootState.addChild(nonParamState, -1, nullptr);
```

#### Acceptance Criteria
- Example JSON schema for a 3-band EQ produces a working APVTS with correct ranges, skew, and defaults
- Parameters survive DAW project save/reload cycle
- Schema validation catches 100% of documented error cases with human-readable messages
- Zero allocations on audio thread during parameter reads (verified with `-fsanitize=thread` or JUCE's `juce::ScopedNoDenormals` + allocation detector)

---

### Phase 2: DSP Sandbox Engine (Layer 2)
**Duration:** 4 weeks
**Priority:** Critical — core audio processing

#### Objectives
- Build the `processSample` wrapper that safely iterates host buffers
- Implement cooperative watchdog timing
- Build initial set of pre-built DSP modules
- Implement declarative signal routing from JSON schema

#### Tasks

| # | Task | SRS Req | Deliverable |
|---|------|---------|-------------|
| 2.1 | Implement `ProcessBlockHandler` | 2.4 | Receives host buffer; iterates channels × samples; calls agent's `processSample` |
| 2.2 | Implement cooperative watchdog | 2.5 | Checks `std::chrono::high_resolution_clock` every N samples; bypasses on timeout |
| 2.3 | Implement `DSPContext` struct | 2.3 | Passed to `processSample`; contains current sample rate, parameter values, host transport info |
| 2.4 | Implement `DSPNode` base class | 2.1, 2.2 | Virtual `prepare(double sampleRate, int maxBlockSize)`, `processSample(float)`, `reset()` |
| 2.5 | Implement `BiquadFilter` node | 2.1 | LP, HP, BP, notch, peak, low-shelf, high-shelf; Direct Form II Transposed; coefficient smoothing |
| 2.6 | Implement `Oscillator` node | 2.1 | Sine, saw, square, triangle; anti-aliased (PolyBLEP); frequency/phase modulation inputs |
| 2.7 | Implement `EnvelopeFollower` node | 2.1 | Attack/release with linear and exponential curves; RMS and peak modes |
| 2.8 | Implement `Gain` node | 2.1 | Linear and dB modes; parameter-smoothed to avoid zipper noise |
| 2.9 | Implement `Delay` node | 2.1 | Pre-allocated circular buffer (max size set in `prepareToPlay`); fractional delay via linear interpolation |
| 2.10 | Implement `DSPRouter` | 2.6 | Parses `"dsp_routing"` from JSON schema; validates graph (no cycles, no disconnected nodes); topological sort for execution order |
| 2.11 | Implement parameter linking | 2.6, 1.3 | Maps schema parameter IDs to DSP node parameters via `"parameter_link"` in routing config |
| 2.12 | Implement parameter smoothing | — | `juce::SmoothedValue` wrapper; prevents zipper noise on rapid parameter changes; configurable ramp time |
| 2.13 | Implement `prepareToPlay` memory pre-allocation | 2.5 | All DSP node buffers allocated here; verified zero allocations in `processBlock` |
| 2.14 | Write DSP module unit tests | 2.2 | Impulse response tests for filters; frequency sweep tests; known-signal verification |
| 2.15 | Write integration test: schema → routing → audio processing | — | End-to-end test: JSON config + processSample function → verified audio output |

#### Technical Design Notes

**Cooperative Watchdog Implementation:**
```cpp
void ProcessBlockHandler::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi) {
    const int numSamples = buffer.getNumSamples();
    const int checkInterval = 32; // Check clock every 32 samples
    const double budgetMs = (numSamples / sampleRate) * 1000.0 * 0.10; // 10% of buffer
    auto startTime = std::chrono::high_resolution_clock::now();
    bool timedOut = false;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            if (!timedOut) {
                channelData[i] = dspRouter.processSample(ch, channelData[i], context);
            }
            // else: pass-through (input unchanged)

            if ((i & (checkInterval - 1)) == 0) {
                auto elapsed = std::chrono::high_resolution_clock::now() - startTime;
                double elapsedMs = std::chrono::duration<double, std::milli>(elapsed).count();
                if (elapsedMs > budgetMs) {
                    timedOut = true;
                    asyncLogger.logViolation("DSP timeout", elapsedMs, budgetMs);
                }
            }
        }
    }
}
```

**DSP Node Interface:**
```cpp
class DSPNode {
public:
    virtual ~DSPNode() = default;
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual float processSample(int channel, float input) = 0;
    virtual void reset() = 0;
    virtual void setParameter(const std::string& name, float value) = 0;
};
```

**Memory Pre-allocation Contract:**
```cpp
// In prepareToPlay — allocate everything
void AgentVSTProcessor::prepareToPlay(double sampleRate, int maxBlockSize) {
    dspRouter.prepare(sampleRate, maxBlockSize); // cascades to all nodes
    // After this point: ZERO heap operations permitted
}
```

#### Acceptance Criteria
- A JSON-defined filter plugin processes audio correctly at 44.1 kHz, 48 kHz, and 96 kHz
- Watchdog triggers and bypasses DSP when an intentionally slow `processSample` is injected
- All DSP nodes pass unit tests against known reference outputs (tolerance: < -120 dBFS error)
- Thread sanitizer reports zero data races
- Allocation detector reports zero heap operations in `processBlock`

---

### Phase 3: WebView UI Engine (Layer 3)
**Duration:** 3 weeks
**Priority:** High — required for usable plugin

#### Objectives
- Implement JUCE 8 `WebBrowserComponent` integration with native JS bridge
- Auto-generate a default HTML/CSS UI from the parameter schema
- Implement `WebSliderParameterAttachment` bindings for all parameter types
- Support custom UI markup (HTML/CSS/JS) served via `withResourceProvider`

#### Tasks

| # | Task | SRS Req | Deliverable |
|---|------|---------|-------------|
| 3.1 | Configure `WebBrowserComponent` in `PluginEditor` | 3.1 | WebView renders in DAW editor window; handles resize/DPI |
| 3.2 | Implement `withResourceProvider` asset serving | 3.2 | Serves HTML/CSS/JS from embedded resources or filesystem |
| 3.3 | Implement native JS bridge (`withNativeFunction`) | 3.3 | C++ exposes parameter query/set functions to JS; returns Promises |
| 3.4 | Implement `WebSliderParameterAttachment` bindings | 3.3 | `Juce.getSliderState("param_id")` works from JS; bidirectional sync |
| 3.5 | Implement `WebToggleButtonParameterAttachment` | 3.3 | Boolean parameters bound to JS toggle state |
| 3.6 | Implement `WebComboBoxParameterAttachment` | 3.3 | Enum parameters bound to JS dropdown state |
| 3.7 | Build default UI generator | 3.1 | Reads parameter schema; emits HTML with sliders/toggles/dropdowns grouped by category |
| 3.8 | Implement real-time meter bridge | 3.4 | Lock-free ring buffer from DSP → UI; JS polls via `requestAnimationFrame`; input/output level meters |
| 3.9 | Implement DPI-aware responsive layout | 3.5 | CSS viewport units + media queries; tested on 1080p and 4K |
| 3.10 | Support hot-reload dev server mode | 3.2 | When `AGENTVST_DEV_MODE=1`, load UI from `localhost:5173` (Vite) instead of embedded resources |
| 3.11 | Write integration tests | — | Parameter change in JS → verify APVTS update; APVTS automation → verify JS state update |

#### Technical Design Notes

**WebView Architecture:**
```
┌─────────────────────────────────────┐
│  JUCE PluginEditor (C++)            │
│  ┌───────────────────────────────┐  │
│  │  WebBrowserComponent          │  │
│  │  ┌─────────────────────────┐  │  │
│  │  │  HTML/CSS/JS Frontend   │  │  │
│  │  │                         │  │  │
│  │  │  Juce.getSliderState()  │←─┤──┤── WebSliderParameterAttachment
│  │  │  Juce.getNativeFunction │←─┤──┤── withNativeFunction bridge
│  │  │  emitEventIfVisible()   │──┤──┤── C++ → JS events (meters)
│  │  └─────────────────────────┘  │  │
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

**Default UI Generation (pseudo-code):**
```
for each group in schema.groups:
    emit <fieldset> with group name
    for each param in group.parameters:
        if param.type == "float" or "int":
            emit <input type="range"> bound to Juce.getSliderState(param.id)
        if param.type == "boolean":
            emit <input type="checkbox"> bound to Juce.getToggleState(param.id)
        if param.type == "enum":
            emit <select> bound to Juce.getComboBoxState(param.id)
```

**Lock-Free Meter Bridge (DSP → UI):**
```cpp
// Single-producer (audio thread), single-consumer (UI timer)
// Using juce::AbstractFifo or custom SPSC ring buffer
struct MeterData {
    float peakL, peakR, rmsL, rmsR;
};
SPSCQueue<MeterData, 64> meterQueue; // pre-allocated, lock-free
```

#### Acceptance Criteria
- Default UI renders all parameters from schema with correct labels, ranges, and units
- Slider movement in UI → parameter changes in DSP within 1 audio block
- DAW automation → UI slider position updates within 20 ms (ValueTree timer polling)
- Hot-reload mode: editing CSS in Vite → live update in plugin window without recompilation
- UI scales correctly from 100% to 200% DPI

---

### Phase 4: Agent Interface & CLI Toolchain
**Duration:** 2 weeks
**Priority:** High — the agent-facing API

#### Objectives
- Define the stable public API surface that agents interact with
- Build a CLI tool (`agentvst`) for project creation, validation, and building
- Create the `processSample` registration mechanism
- Document the complete agent workflow

#### Tasks

| # | Task | SRS Req | Deliverable |
|---|------|---------|-------------|
| 4.1 | Define `AgentDSP.h` — the single header an agent includes | 2.3 | Contains `processSample` signature, `DSPContext` struct, built-in module accessors |
| 4.2 | Implement `processSample` registration macro | 2.3 | `AGENTVST_REGISTER_DSP(MyProcessor)` — agent writes a class with `processSample`; framework wraps it |
| 4.3 | Build `agentvst init` CLI command | — | Generates project skeleton: `plugin.json`, `MyDSP.cpp`, `CMakeLists.txt` |
| 4.4 | Build `agentvst validate` CLI command | 5.2.2 | Validates `plugin.json` schema; checks DSP code for prohibited patterns (regex: `new `, `malloc`, `std::vector`, `std::cout`) |
| 4.5 | Build `agentvst build` CLI command | 4.1 | Wraps CMake configure + build; injects stable IDs; outputs VST3/AU/Standalone |
| 4.6 | Implement static analysis pass for real-time violations | 2.5 | Scan agent `.cpp` for heap allocation, mutex, I/O, string operations; emit warnings |
| 4.7 | Write comprehensive agent API documentation | 5.3.1 | `docs/agent_api.md` — every function, every struct, every constraint |
| 4.8 | Create example plugins | 5.3.1 | Gain plugin, 3-band EQ, simple compressor — complete JSON + DSP + optional custom UI |

#### Agent Workflow (End-to-End)

```
1. Agent generates:
   ├── plugin.json          ← Parameter schema + routing + metadata
   ├── src/MyDSP.cpp        ← processSample implementation
   └── ui/index.html (opt)  ← Custom UI markup

2. Agent runs:  agentvst validate ./plugin.json
   → Schema OK ✓
   → DSP static analysis OK ✓ (no prohibited patterns)

3. Agent runs:  agentvst build --format vst3,au,standalone
   → Framework generates:
     ├── PluginProcessor.cpp  (APVTS from schema, processBlock wrapper)
     ├── PluginEditor.cpp     (WebView + auto-generated UI or custom HTML)
     └── build/               (compiled binaries)

4. Output:
   ├── build/MyPlugin.vst3
   ├── build/MyPlugin.component  (AU, macOS only)
   └── build/MyPlugin.app        (Standalone)
```

#### Acceptance Criteria
- `agentvst init "SimpleGain"` produces a valid project that compiles out of the box
- `agentvst validate` catches intentional errors (missing parameter range, `new` in DSP code)
- `agentvst build` produces loadable VST3 from a 10-line JSON + 5-line DSP function
- All three example plugins compile and pass audio processing tests

---

### Phase 5: Testing, Validation & DAW Compatibility
**Duration:** 3 weeks
**Priority:** High — production readiness

#### Objectives
- Comprehensive testing across DAWs and platforms
- Performance benchmarking
- Thread safety verification
- Edge case handling

#### Tasks

| # | Task | SRS Req | Deliverable |
|---|------|---------|-------------|
| 5.1 | DAW compatibility testing matrix | 5.4.1 | Test in REAPER, Ableton Live, Logic Pro, Studio One, Cubase |
| 5.2 | State serialization round-trip testing | 4.3 | Save project → close DAW → reopen → all parameters restored exactly |
| 5.3 | Buffer size stress testing | 9.2 | Test at buffer sizes 32, 64, 128, 256, 512, 1024, 2048, 4096 |
| 5.4 | Sample rate compatibility | — | Test at 44.1, 48, 88.2, 96, 176.4, 192 kHz |
| 5.5 | Thread sanitizer pass | 5.2.1 | Build with `-fsanitize=thread`; full DAW session with automation; zero reports |
| 5.6 | Address sanitizer pass | 5.2.1 | Build with `-fsanitize=address`; zero memory errors |
| 5.7 | CPU profiling benchmarks | 5.1.1 | Framework overhead < 5% of buffer budget; measured with `std::chrono` in release build |
| 5.8 | Watchdog stress test | 2.5 | Inject intentionally expensive DSP; verify graceful bypass without crash |
| 5.9 | Rapid parameter automation test | — | Automate all parameters simultaneously at audio rate; no clicks, pops, or crashes |
| 5.10 | Offline bounce test (faster-than-realtime) | — | Verify correct output when DAW bounces at maximum speed |
| 5.11 | Multiple instance test | — | Load 50+ instances of plugin simultaneously; no resource leaks or ID conflicts |

#### Acceptance Criteria
- Plugin loads and functions correctly in all 5 target DAWs
- Zero thread sanitizer violations
- Zero address sanitizer violations
- Framework overhead < 5% CPU at 48 kHz / 512 samples
- State survives 1000 save/load cycles without data corruption

---

### Phase 6: CI/CD, Signing & Distribution Pipeline
**Duration:** 2 weeks
**Priority:** Medium — required for AAX distribution

#### Objectives
- Automate multi-platform builds in CI
- Implement AAX signing via PACE Cloud 2 Cloud
- Implement macOS notarization
- Implement Windows code signing
- Package plugins for distribution

#### Tasks

| # | Task | SRS Req | Deliverable |
|---|------|---------|-------------|
| 6.1 | GitHub Actions: Windows x64 build + VST3 artifact | 4.1 | Automated Windows build on push/tag |
| 6.2 | GitHub Actions: macOS universal build + VST3/AU artifacts | 4.1 | arm64 + x86_64 universal binary |
| 6.3 | Integrate PACE Cloud 2 Cloud AAX signing | 4.1 | `wraptool` executed in CI with cloud credentials; signed AAX artifact |
| 6.4 | macOS notarization via `notarytool` | — | Stapled notarization ticket on .component and .vst3 bundles |
| 6.5 | Windows Authenticode signing | — | Signed .vst3 DLL via signtool in CI |
| 6.6 | Installer packaging | — | macOS: `.pkg` installer; Windows: NSIS or Inno Setup |
| 6.7 | VST3 licensing flag configuration | — | CMake correctly embeds GPL or proprietary license flag based on config |
| 6.8 | Release automation | — | Git tag → CI builds all formats → signs → packages → GitHub Release |

#### Acceptance Criteria
- Tagged release produces signed VST3 + AU + AAX on both platforms
- AAX loads in commercial Pro Tools (not just Developer build)
- macOS plugins pass Gatekeeper without warning
- Full pipeline completes in < 30 minutes

---

## 4. Project Directory Structure

```
AgentVST/
├── CMakeLists.txt                    # Root CMake; fetches JUCE, defines framework library
├── cmake/
│   ├── agentvst_ids.cmake            # Deterministic ID generation
│   └── agentvst_plugin.cmake         # Helper function for agent plugins
├── framework/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── AgentVST.h                # Master include
│   │   ├── AgentDSP.h                # Agent-facing DSP API (the ONLY header agents need)
│   │   ├── SchemaParser.h
│   │   ├── ParameterLayoutBuilder.h
│   │   ├── ParameterCache.h
│   │   ├── StateSerializer.h
│   │   ├── DSPNode.h
│   │   ├── DSPRouter.h
│   │   ├── ProcessBlockHandler.h
│   │   └── UIGenerator.h
│   ├── src/
│   │   ├── SchemaParser.cpp
│   │   ├── ParameterLayoutBuilder.cpp
│   │   ├── ParameterCache.cpp
│   │   ├── StateSerializer.cpp
│   │   ├── DSPRouter.cpp
│   │   ├── ProcessBlockHandler.cpp
│   │   ├── UIGenerator.cpp
│   │   └── PluginWrapper/
│   │       ├── AgentVSTProcessor.cpp  # Concrete AudioProcessor implementation
│   │       └── AgentVSTEditor.cpp     # WebBrowserComponent editor
│   └── modules/                       # Pre-built DSP nodes
│       ├── BiquadFilter.h / .cpp
│       ├── Oscillator.h / .cpp
│       ├── EnvelopeFollower.h / .cpp
│       ├── Gain.h / .cpp
│       └── Delay.h / .cpp
├── ui/
│   ├── default/                       # Auto-generated default UI template
│   │   ├── index.html
│   │   ├── style.css
│   │   └── main.js
│   └── templates/                     # Starter custom UI templates
│       └── minimal/
├── cli/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp                   # agentvst CLI entry point
│       ├── InitCommand.cpp
│       ├── ValidateCommand.cpp
│       └── BuildCommand.cpp
├── tests/
│   ├── unit/
│   │   ├── SchemaParserTest.cpp
│   │   ├── ParameterCacheTest.cpp
│   │   ├── BiquadFilterTest.cpp
│   │   ├── OscillatorTest.cpp
│   │   └── DSPRouterTest.cpp
│   └── integration/
│       ├── FullPipelineTest.cpp       # JSON → build → load → process → verify
│       └── StateRoundTripTest.cpp
├── examples/
│   ├── simple_gain/
│   │   ├── plugin.json
│   │   └── src/GainDSP.cpp
│   ├── three_band_eq/
│   │   ├── plugin.json
│   │   ├── src/EQDSP.cpp
│   │   └── ui/index.html             # Custom EQ visualizer UI
│   └── compressor/
│       ├── plugin.json
│       └── src/CompressorDSP.cpp
├── docs/
│   ├── schema_spec.md                 # Formal JSON schema specification
│   ├── agent_api.md                   # Agent-facing API reference
│   ├── dsp_modules.md                 # Pre-built module documentation
│   └── architecture.md               # Internal architecture guide
├── .github/
│   └── workflows/
│       ├── build.yml                  # CI: build + test
│       └── release.yml                # CD: sign + package + publish
├── SRS.md
├── DEVELOPMENT_PLAN.md
└── AI-Assisted JUCE Plugin Development.md
```

---

## 5. Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|-----------|--------|------------|
| R1 | JUCE 8 WebView has platform-specific bugs (especially Windows WebView2) | Medium | High | Test early on Windows; maintain C++ fallback UI generator for critical failures |
| R2 | Agent generates `processSample` code with hidden allocations (e.g., lambda captures, `std::function`) | High | Critical | Static analysis in `agentvst validate`; runtime allocation detector in debug builds |
| R3 | PACE Cloud 2 Cloud API changes or becomes unavailable | Low | High | Abstract signing step behind interface; support both cloud and local iLok signing |
| R4 | Lock-free meter bridge causes subtle data races under high load | Medium | Medium | Use `juce::AbstractFifo` (battle-tested); thread sanitizer in CI |
| R5 | Agent produces DSP that is mathematically correct but causes denormals (performance collapse) | Medium | Medium | Insert `juce::ScopedNoDenormals` at top of `processBlock`; flush-to-zero in `prepareToPlay` |
| R6 | WebView resource provider fails to serve assets on certain DAW hosts | Medium | Medium | Embed resources as binary data via CMake `juce_add_binary_data`; no filesystem dependency |
| R7 | Parameter ID collisions when agent reuses common names across plugins | Low | High | Namespace IDs with plugin name hash; validate uniqueness in schema parser |
| R8 | JUCE license changes affect framework distribution model | Low | High | Track JUCE licensing terms; framework designed to work with both GPL and commercial JUCE licenses |

---

## 6. Timeline Summary

```
Week  1-2   ████████  Phase 0: Project Scaffolding & Build Infrastructure
Week  3-5   ████████████  Phase 1: Declarative State Schema Engine
Week  5-8   ████████████████  Phase 2: DSP Sandbox Engine
Week  8-10  ████████████  Phase 3: WebView UI Engine
Week 10-11  ████████  Phase 4: Agent Interface & CLI
Week 12-14  ████████████  Phase 5: Testing & DAW Compatibility
Week 14-15  ████████  Phase 6: CI/CD, Signing & Distribution
```

**Total estimated duration: 15 weeks (~4 months)**

Notes:
- Phases 2 and 3 overlap by 1 week (UI work can begin once parameter cache is stable)
- Phase 5 testing begins in parallel with Phase 4 CLI work
- Phase 6 CI/CD setup starts during Phase 5 testing

---

## 7. Definition of Done (v1.0 Release)

The framework is considered complete for v1.0 when ALL of the following are satisfied:

- [ ] An AI agent can generate a working compressor plugin from a text description using only `plugin.json` + `processSample` function
- [ ] The plugin compiles to VST3, AU, and Standalone from a single codebase
- [ ] The plugin loads and functions correctly in REAPER, Ableton Live, Logic Pro, Cubase, and Studio One
- [ ] State serialization survives save/load across all tested DAWs
- [ ] Thread sanitizer reports zero violations under full automation stress test
- [ ] Framework overhead is < 5% CPU at 48 kHz / 512 samples
- [ ] `agentvst validate` catches all documented prohibited patterns in agent DSP code
- [ ] Three example plugins ship with the framework (gain, EQ, compressor)
- [ ] Agent API documentation is complete with code examples
- [ ] CI pipeline produces signed binaries on tagged releases (AAX signing deferred if PACE approval pending)

---

## 8. Post-v1.0 Roadmap

| Version | Features | Estimated Effort |
|---------|----------|-----------------|
| **1.1** | MIDI note input support; `processNote` callback; synth plugin archetype | 4 weeks |
| **1.2** | Sidechain input support; external audio routing in schema | 2 weeks |
| **1.3** | Extended DSP library: convolution reverb, FFT-based spectral processing, waveshaper | 4 weeks |
| **1.4** | Preset management system with browser UI | 3 weeks |
| **1.5** | Sample-accurate automation; per-sample parameter interpolation | 2 weeks |
| **2.0** | Linux VST3 support; CLAP format support; neural network DSP inference module | 8 weeks |

---

**End of Development Plan**


## DSP Plugins Needing Design Work
- NegativeSpaceDelay
- IntermodulationWallGenerator
- DynamizationHaasComb
