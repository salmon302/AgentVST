# AgentVST: Software Requirements Specification

**Document Version:** 1.0
**Date:** 2026-03-28
**Status:** Initial Draft

---

## 1. Executive Summary

AgentVST is a framework designed to enable AI agents to reliably and efficiently develop professional audio plugins using JUCE. By abstracting the complexity of plugin development into declarative structures and providing modular building blocks, AgentVST lowers the barrier to entry and allows AI systems to generate production-ready VST3, AU, and AAX plugins without deep expertise in real-time audio programming or JUCE internals.

---

## 2. Problem Statement

### 2.1 Current Challenges in JUCE Development

- **High Learning Curve:** JUCE enforces a complex architectural separation between the `AudioProcessor` (DSP engine) and `AudioProcessorEditor` (GUI), requiring developers to manage non-trivial state synchronization.
- **Boilerplate Complexity:** Plugin developers must manually configure `AudioProcessorValueTreeState` (APVTS), bind UI controls, and implement serialization logic.
- **Real-time Constraints:** Incorrect buffer handling or memory allocation in the real-time audio thread can cause audible glitches (xruns), requiring deep knowledge of real-time audio programming principles.
- **AI Agent Limitations:** While AI agents excel at generating declarative structures and manipulating isolated functional blocks, they struggle with tightly coupled codebases requiring complex thread synchronization—exactly what raw JUCE becomes.
- **GUI Verbosity:** Writing JUCE GUI code requires extensive subclassing and manual layout management, making it error-prone and difficult for AI agents to generate correctly.

### 2.2 Target User: AI Agents

AgentVST is specifically optimized for **agentic development**—allowing AI systems to generate clean, modular, and maintainable plugin code through a structured, declarative interface.

---

## 3. Vision & Design Philosophy

### 3.1 "Agent-First" Architecture

The framework isolates three core domains of plugin development:

1. **State:** Parameter definitions and data flow
2. **DSP:** Audio processing logic and signal routing
3. **UI:** User interface and parameter visualization

The agent defines the "what" (parameters and routing) using simple data structures; the framework handles the "how" (C++ memory management, host communication, thread safety).

### 3.2 Core Principles

- **Declarative over Imperative:** Agents write configuration and high-level descriptions, not boilerplate C++.
- **Abstraction of Complexity:** Thread safety, real-time constraints, and host communication are handled transparently.
- **Modular DSP Design:** Pre-built, optimized signal processing components (filters, oscillators, envelopes) reduce the need for custom low-level code.
- **Automatic Code Generation:** The framework generates JUCE boilerplate from declarative schemas.

---

## 4. Functional Requirements

### 4.1 Layer 1: Declarative State Schema

**Requirement 1.1:** The framework shall accept a declarative configuration file (JSON or YAML) that defines the plugin's parameters.

- **Parameter Definition:** Each parameter must specify:
  - Unique identifier (string)
  - Human-readable name (string)
  - Parameter type (float, int, boolean)
  - Range (min, max)
  - Default value
  - Unit type (e.g., "Hz", "dB", "ms", "percentage")
  - Optional: step size, skew factor, labels for enum-like parameters

**Requirement 1.2:** The framework shall automatically generate a fully-configured `AudioProcessorValueTreeState` (APVTS) from the parameter schema.

- APVTS must be initialized with all parameters as specified in the configuration.
- Parameter IDs must be consistently derived from the configuration and validated for uniqueness.

**Requirement 1.3:** The framework shall provide runtime access to parameter values for DSP modules.

- DSP code must be able to query current parameter values in a thread-safe manner.
- Parameter changes from the host (or GUI) must be reflected immediately in the DSP engine.

**Requirement 1.4:** The framework shall support parameter grouping or categorization.

- Parameters should optionally be organized into logical groups (e.g., "Filter Controls", "Envelope Settings").
- Groups must be reflected in both the auto-generated UI and the state schema.

---

### 4.2 Layer 2: DSP Sandbox

**Requirement 2.1:** The framework shall provide pre-built, optimized DSP modules for common signal processing tasks.

- **IIR Biquad Filter:** Configurable low-pass, high-pass, band-pass, and notch filters.
- **Oscillator:** Waveform generation (sine, square, sawtooth, triangle) with frequency control.
- **Envelope Detector:** ADSR-style envelope generators with linear and exponential curves.
- **Additional Modules:** Delay, reverb, distortion, and other standard audio effects (prioritized by use case).

**Requirement 2.2:** Each DSP module shall be independently testable and shall not depend on JUCE or real-time constraints.

- Modules shall be pure C++ classes with minimal external dependencies.
- Modules shall be suitable for unit testing in isolation.

**Requirement 2.3:** The framework shall expose a safe `processSample(float input)` interface for custom DSP logic.

- The agent writes a single function that processes one audio sample at a time.
- The function receives the current sample value and returns the processed output.
- The function must be reentrant and stateless (except for module-owned buffers).

**Requirement 2.4:** The framework shall handle block-based processing transparently.

- The host provides audio data in blocks (buffers).
- The framework iterates through each sample in the buffer and invokes the agent's `processSample` function.
- The processed samples are accumulated into the output buffer.
- Memory safety and buffer bounds are enforced by the framework.

**Requirement 2.5:** The framework shall prevent real-time violations by the agent code.

- Dynamic memory allocation in `processSample` shall be detected and logged as a warning or error.
- The framework shall enforce timeout limits on `processSample` execution (configurable, defaulting to 10% of the host buffer duration).
- If an agent's `processSample` function exceeds the timeout or triggers an exception, the framework shall handle it gracefully (skip the sample, log the error, continue processing).

**Requirement 2.6:** The framework shall support module composition and signal routing.

- Agents shall be able to chain DSP modules together (e.g., input → filter → oscillator → output).
- The framework shall provide a routing configuration format (declarative) to define signal paths.
- Routing must be validated at compile-time or initialization to detect cycles and disconnected nodes.

---

### 4.3 Layer 3: Auto-Generated & Prompt-Driven UI

**Requirement 3.1:** The framework shall automatically generate a functional, generic GUI from the parameter schema.

- A default UI shall be created for all parameters defined in the schema.
- The default UI shall include:
  - **Sliders** for float and integer parameters with appropriate ranges.
  - **Toggle buttons** for boolean parameters.
  - **Dropdown menus** for enum-like parameters with predefined labels.
  - **Text displays** for parameter names, values, and units.
  - **Organized layout** with parameters grouped logically.

**Requirement 3.2:** The framework shall provide a declarative markup language for custom UI layouts.

- The markup shall resemble CSS Flexbox or a grid-based layout system.
- It shall allow agents to:
  - Define custom control layouts (positioning, sizing, alignment).
  - Assign visual properties (colors, fonts, spacing).
  - Link controls to specific parameters.
  - Embed custom visual elements (graphics, waveform displays, meters).

**Requirement 3.3:** The framework shall automatically bind UI controls to the APVTS.

- `juce::SliderAttachment`, `juce::ButtonAttachment`, and similar JUCE bindings shall be created automatically.
- Changes in the UI shall immediately reflect in the APVTS and vice versa.
- The agent shall not manually implement editor/processor communication.

**Requirement 3.4:** The framework shall support real-time parameter visualization.

- The UI shall display current parameter values and respond to changes initiated by the DSP or host.
- Parameter meters (e.g., input/output level displays) shall update in real-time.

**Requirement 3.5:** The framework shall handle DPI scaling and responsive layout.

- The UI shall adapt to different screen resolutions and DPI settings.
- Layout shall remain functional and readable on both high-DPI (e.g., 4K) and standard displays.

---

### 4.4 Plugin Output & Host Compatibility

**Requirement 4.1:** The framework shall generate production-ready plugin binaries for multiple formats.

- **Supported Formats:** VST3, Audio Units (AU), and AAX.
- Each format must be built from the same codebase without modification to the agent's code.

**Requirement 4.2:** The framework shall handle plugin metadata and branding.

- The agent shall specify plugin name, version, vendor, and category via the configuration schema.
- This metadata shall be embedded in the compiled plugin binary.

**Requirement 4.3:** The framework shall implement plugin state serialization and restoration.

- Plugin state (parameter values, internal module state) shall be saved when the user saves a project in the DAW.
- Plugin state shall be restored when the user loads a saved project.
- Serialization shall support both **APVTS binary format** and **custom agent-defined state** (for module-specific settings).

**Requirement 4.4:** The framework shall expose host timing and context information.

- The DSP code shall have access to:
  - Current sample rate and buffer size.
  - Current transport state (playing, stopped, rewinding).
  - Tempo and time signature (if available from the host).

---

## 5. Non-Functional Requirements

### 5.1 Performance

**Requirement 5.1.1:** The framework shall introduce minimal CPU overhead.

- Framework infrastructure (parameter lookups, routing, buffer management) shall consume <5% of the real-time budget.
- DSP modules shall be optimized for low CPU usage and minimal latency.

**Requirement 5.1.2:** Latency shall be minimized.

- The plugin shall introduce <10 ms of latency (typical for modern plugins).
- Latency shall be reported to the host via the `getLatencySamples()` method.

### 5.2 Reliability

**Requirement 5.2.1:** The framework shall be robust against agent-generated errors.

- Crashes in the agent's DSP code shall not crash the entire host application.
- Infinite loops or excessive computation shall be detected and handled gracefully.

**Requirement 5.2.2:** The framework shall provide clear error messages for common mistakes.

- Configuration errors (e.g., missing parameter ranges, invalid parameter IDs) shall be reported at initialization time with actionable error messages.
- Runtime errors (e.g., buffer overflows, invalid routing) shall be logged with context and stack traces.

### 5.3 Maintainability & Code Quality

**Requirement 5.3.1:** The framework codebase shall be modular and well-documented.

- Core framework classes shall be organized by domain (state, DSP, UI, host communication).
- Public APIs shall have comprehensive Doxygen documentation.
- Example plugins shall demonstrate common usage patterns.

**Requirement 5.3.2:** The agent shall not need to understand JUCE internals.

- All JUCE-specific code shall be encapsulated within the framework.
- Agents shall interact only with AgentVST's public API.

### 5.4 Compatibility

**Requirement 5.4.1:** The framework shall support modern DAWs and plugin hosts.

- Tested compatibility with: Ableton Live, Logic Pro, Studio One, Reaper, Cubase, Pro Tools.
- Support for the latest VST3 specification and AU standards.

**Requirement 5.4.2:** The framework shall support both 32-bit and 64-bit architectures (as applicable to each platform).

- Windows: 64-bit VST3, AAX.
- macOS: 64-bit AU, VST3, AAX.

---

## 6. System Architecture

### 6.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Host DAW / Application                │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│              AgentVST Plugin Wrapper                     │
│  (Implements VST3, AU, AAX protocol)                    │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│             AgentVST Framework Core                      │
├─────────────────────────────────────────────────────────┤
│  Layer 1: State Management (APVTS, Parameters)         │
│  Layer 2: DSP Engine (Module routing, processSample)   │
│  Layer 3: UI Controller (Parameter binding, Layout)    │
│  Thread Safety & Real-time Constraint Handler          │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│              Agent-Generated Code                        │
│  - Parameter Schema (JSON/YAML)                         │
│  - DSP Logic (processSample function)                   │
│  - UI Layout (declarative markup, optional)             │
└─────────────────────────────────────────────────────────┘
```

### 6.2 Core Components

#### 6.2.1 State Management Module
- **AgentVSTState:** Wraps JUCE's APVTS and provides thread-safe parameter access.
- **ParameterSchema:** Parses and validates the agent's configuration file.
- **StateSerializer:** Handles save/load of plugin state.

#### 6.2.2 DSP Engine Module
- **DSPNode:** Base class for all signal processing components.
- **DSPRouter:** Manages connections between DSP nodes and handles signal flow.
- **ProcessSampleWrapper:** Wraps the agent's `processSample` function and handles real-time constraints.
- **BuiltInModules:** Pre-implemented filters, oscillators, envelopes, etc.

#### 6.2.3 UI Module
- **UIGenerator:** Creates a default GUI from the parameter schema.
- **UILayout:** Parses the agent's custom UI markup (if provided).
- **UIBinder:** Creates and manages JUCE attachments between UI controls and parameters.

#### 6.2.4 Host Communication Module
- **PluginProcessor:** Implements the host protocol (VST3, AU, AAX).
- **PluginEditor:** Implements the GUI editor class.
- **HostContext:** Provides the agent with host timing, tempo, and transport information.

---

## 7. Data Specifications

### 7.1 Parameter Schema Format (JSON Example)

```json
{
  "plugin": {
    "name": "SimpleFilter",
    "version": "1.0.0",
    "vendor": "AgentVST",
    "category": "Filter"
  },
  "parameters": [
    {
      "id": "cutoff_freq",
      "name": "Cutoff Frequency",
      "type": "float",
      "min": 20.0,
      "max": 20000.0,
      "default": 1000.0,
      "unit": "Hz",
      "step": 1.0,
      "skew": 0.3
    },
    {
      "id": "resonance",
      "name": "Resonance",
      "type": "float",
      "min": 0.0,
      "max": 5.0,
      "default": 0.707,
      "unit": "linear",
      "step": 0.01
    },
    {
      "id": "filter_type",
      "name": "Filter Type",
      "type": "enum",
      "options": ["Low-Pass", "High-Pass", "Band-Pass"],
      "default": 0
    }
  ],
  "dsp_routing": [
    {
      "source": "input",
      "destination": "biquad_filter",
      "parameter_link": { "cutoff_freq": "biquad_filter.frequency" }
    },
    {
      "source": "biquad_filter",
      "destination": "output"
    }
  ]
}
```

### 7.2 DSP Module Specification

Each pre-built DSP module shall expose:

- **Input/Output Ports:** Defines audio signal flow.
- **Parameters:** Configuration parameters (e.g., filter cutoff, oscillator frequency).
- **State:** Internal buffers or history (e.g., biquad filter state variables).
- **Process Function:** Core DSP algorithm.

Example (Biquad Filter):
```cpp
class BiquadFilter {
public:
  void prepare(double sampleRate);
  void setFrequency(float freq);
  void setResonance(float q);
  float processSample(float input);
};
```

### 7.3 UI Markup Format (Example)

```yaml
layout:
  type: vertical
  padding: 10
  children:
    - type: label
      text: "Filter Settings"
      style:
        font_size: 18
        font_weight: bold
    - type: horizontal
      padding: 5
      children:
        - type: slider
          parameter: "cutoff_freq"
          label: "Cutoff"
        - type: slider
          parameter: "resonance"
          label: "Resonance"
    - type: dropdown
      parameter: "filter_type"
      label: "Type"
```
---

## 8. Incubation-Layer Pivot

AgentVST will support an "incubation-layer" mode: rather than being a full, standalone AI development engine, AgentVST becomes the reliable scaffolding that makes existing AI developer tools (editor assistants like GitHub Copilot and remote systems like Claude Code) much more effective at producing robust audio plugins.

### 8.1 Objectives

- Provide deterministic schemas and code templates that encourage assistants to output parseable, machine-validated artifacts.
- Offer build/test orchestration to compile, run unit and audio acceptance tests, and produce actionable diagnostics.
- Provide curated prompt templates and in-repo instruction blocks for editor-integrated assistants (Copilot) and remote LLMs (Claude Code).
- Offer an optional orchestrator API to safely run remote generations server-side with sandboxing and an approval workflow.

### 8.2 Core Capabilities

- Schema Registry: authoritative JSON Schema(s) for plugin definitions and UI markup.
- Prompt Templates & Snippets: short, deterministic templates to guide in-editor completions.
- Template-Based Code Generator: render JUCE skeletons from validated schema inputs.
- Orchestrator API (optional): run generate → build → test pipelines and return machine-readable results.
- Testbeds & Validators: audio fixtures, performance/xrun checks, static analysis, and runtime sanitizers.

### 8.3 Integration Patterns

- Editor-first (Copilot): ship `docs/prompts/` and in-repo instruction blocks that guide Copilot to emit schema-only outputs or minimal, well-scoped patches.
- Remote LLMs (Claude Code and others): use the orchestrator API to submit prompts, require JSON-only responses, then run generation and validation server-side with sandboxing.
- CI Hooks: run builds and tests on AI-generated branches and fail merges when validations or tests fail.

### 8.4 MVP (Prioritized)

1. `schema/` — JSON Schema + example plugin spec.
2. `docs/prompts/` — Copilot + remote-LLM prompt templates and example instruction-blocks.
3. `scripts/generate.py` — template-only generator that maps schema → JUCE skeletons.
4. `orchestrator/` (stub) — minimal FastAPI CLI that runs generation and invokes the build/test hooks.
5. Simple audio acceptance tests and a CI workflow template.

For full guidelines, sample prompts, and quick-start workflows see [docs/INCUBATION_LAYER.md](docs/INCUBATION_LAYER.md).


---

## 8. Constraints & Assumptions

### 8.1 Constraints

- **Real-time Thread Safety:** All framework code running in the real-time audio thread must be lock-free or use wait-free algorithms.
- **Supported Platforms:** Windows (VST3, AAX), macOS (AU, VST3, AAX). Linux support is out of scope for this version.
- **Minimum Configuration:** Agents must provide a valid parameter schema; plugins with no parameters are allowed but uncommon.
- **Module Dependency:** All pre-built DSP modules must be self-contained and not depend on external libraries (except JUCE).

### 8.2 Assumptions

- **Agent Competence:** The agent-provided DSP code is functionally correct (though the framework will protect against real-time violations).
- **JUCE Version:** The framework targets JUCE 7.0 or later.
- **Host Compliance:** The host DAW correctly implements VST3, AU, or AAX specifications.
- **Configuration Validity:** The agent provides a well-formed configuration file; the framework performs validation but assumes no intentional corruption.

---

## 9. Success Criteria

### 9.1 Functional Success

- ✓ An AI agent can define a plugin using only the parameter schema and a `processSample` function.
- ✓ The framework generates a working VST3, AU, or AAX plugin without manual intervention.
- ✓ The plugin passes basic DAW compatibility tests (load, play audio, save/restore state).
- ✓ Parameter changes in the GUI are reflected in real-time in the DSP output.

### 9.2 Performance Success

- ✓ The framework introduces <5% CPU overhead on a typical real-time audio processing workload.
- ✓ Plugin latency is reported accurately and is <10 ms.
- ✓ The plugin handles buffer sizes from 64 to 4096 samples without glitches.

### 9.3 Robustness Success

- ✓ Crashes or hangs in agent-generated DSP code do not crash the host.
- ✓ Configuration errors are reported with actionable error messages.
- ✓ The plugin recovers gracefully from edge cases (sample rate changes, transport state changes, parameter saturation).

### 9.4 Developer Experience Success

- ✓ An AI agent can generate a complete, working plugin from a verbal description in <5 minutes of agent iterations.
- ✓ The framework documentation and example plugins are clear and comprehensive.
- ✓ Errors encountered by agents are clearly communicated with suggestions for resolution.

---

## 10. Out of Scope

The following are explicitly **not** addressed in this initial version:

- **Linux support:** Targeting Windows and macOS only.
- **Advanced plugin features:** Sidechain inputs, MIDI note processing, sample-accurate automation (will be added in future versions).
- **Machine learning integration:** Using neural networks for DSP; the framework supports agent-generated classical DSP only.
- **Preset management:** Beyond APVTS save/load (custom preset systems are out of scope).
- **Visual GUI generation:** The framework generates functional layouts; custom graphics/artwork are the agent's responsibility.

---

## 11. Future Enhancements (Post-1.0)

- **MIDI Support:** Enable plugins to receive and process MIDI notes.
- **Extended DSP Library:** Additional modules for granular synthesis, convolution, spectral processing.
- **Linux Support:** VST3 on Linux.
- **Sidechain Inputs:** Allow plugins to respond to external audio signals.
- **Sample-Accurate Automation:** Support for parameter changes synchronized to individual samples.
- **Visual Debugging Tools:** Real-time visualization of signal flow and parameter values.
- **Plugin Preset Browser:** Built-in UI for managing and switching between saved presets.

---

## 12. Approval & Sign-Off

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Product Owner | TBD | | |
| Technical Lead | TBD | | |
| QA Lead | TBD | | |

---

**End of SRS Document**
