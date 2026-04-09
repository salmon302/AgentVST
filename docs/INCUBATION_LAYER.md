# AgentVST — Incubation-Layer Guidelines

## Overview

AgentVST can operate as an incubation layer for existing AI development tools. Rather than re-implementing model logic, AgentVST provides deterministic schemas, prompt templates, code templates, and validation harnesses that make editor assistants (e.g., GitHub Copilot) and remote LLMs (e.g., Claude Code) more reliable at producing production-ready plugins.

This document summarizes recommended workflows, sample prompts, and minimal quick-start steps to integrate an assistant-driven workflow into this repository.

## Quick Start (Editor-first: Copilot)

1. Edit or create a plugin spec under `schema/` (JSON conforming to the project's schema).
2. Open the spec file and use the provided Copilot instruction block from `docs/prompts/` (or the example below) to ask Copilot to complete or expand the spec.
3. Validate and generate from the local tooling:

```bash
python scripts/validate_schema.py schema/example_plugin.json
python scripts/generate.py --spec schema/example_plugin.json --out generated
python tests/audio-fixtures/run_audio_acceptance.py
```

4. Build and run tests using the project's normal CMake workflow.

Windows example (local build):

```powershell
cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Debug --target agentvst_tests
```

For local iteration without auto-deploy copy (avoids DAW file-lock failures), use:

```powershell
.\_configure_only_agentvst.cmd
```

5. Build VST3 example artifacts and run module validation preflight:

```powershell
.\_build_and_validate_vst3_examples.cmd
```

### No-Op DSP Debug Checklist

If a plugin appears to pass audio unchanged, run this checklist before deeper redesign:

1. Confirm parameters are active and not effectively bypassing DSP (for example, wet/mix at 0).
2. Check runtime logs for `Potential no-op DSP detected` warnings emitted by AgentVST.
3. Verify your `processSample()` path writes transformed samples for non-silent input.
4. Confirm deployed plugin copy succeeded; a DAW file lock can prevent `.vst3` replacement and leave stale behavior loaded.
5. Close host/unload plugin, rebuild, and re-open host to ensure the new binary is running.

## Quick Start (Remote LLMs: Orchestrator)

1. Post the schema and the prompt to the orchestrator API (stubbed under `orchestrator/`).
2. Orchestrator requires JSON-only responses; it will validate the response against the JSON Schema, then run the generate→build→test pipeline.
3. Orchestrator returns machine-readable diagnostics (build errors, test failures, static analysis warnings) and an artifact location for human review.

Example (curl to orchestrator stub):

```bash
python orchestrator/app.py

curl -X POST http://localhost:8000/generate \
  -H 'Content-Type: application/json' \
  -d '{"schemaPath":"schema/example_plugin.json","outputDir":"generated","force":true}'
```

## Sample Prompt Templates

Editor/Copilot instruction block (place near schema file to bias completions):

```
/* AGENTVST_PROMPT
You are an assistant that helps author AgentVST plugin specs. Output ONLY valid JSON that conforms to the schema in schema/plugin.schema.json. Do NOT add explanatory text or code fences. Ensure every parameter object contains: id, name, type, min, max, default, and unit.
*/
```

Remote LLM instruction (for orchestrator):

```
System: You are an agent that must return a single JSON object. The object must validate against the schema provided in the request. Return only the JSON. If the object cannot be built, return {"error":"<short message>"}.
User: See attached schema file and examples. Produce a plugin spec named "MyPlugin" that demonstrates parameter groups and one DSP routing entry.
```

## Repository Layout

- `schema/` — JSON Schema(s) and example plugin specs.
- `docs/prompts/` — curated prompt templates (editor and remote).
- `scripts/generate.py` — template-to-code generator (maps schema → JUCE skeleton).
- `scripts/validate_schema.py` — canonical schema validation for examples and generated specs.
- `scripts/smoke_test_generate.py` — smoke test for generation workflow.
- `orchestrator/` — optional FastAPI service that runs generate→build→test.
- `tests/audio-fixtures/` — deterministic acceptance fixtures and runner for core audio behavior checks.

## Safety and Validation

- Require JSON-only outputs from LLMs and validate them against schema before generation.
- Run static analysis (`clang-tidy`, `clang-format`) and build sanitizers (ASan/UBSan) in CI.
- Sandbox builds and tests in ephemeral environments or containers before merging.
- Keep human-in-the-loop approval gates for any code that touches real-time audio threads.
- Include an audibility sanity check in acceptance tests (input vs output relative-difference threshold) to catch accidental pass-through DSP.

## Next Steps (recommended immediate work)

1. Add host-level smoke validation (e.g., pluginval or scripted AudioPluginHost scan) in CI.
2. Extend schema validation in CI to include generated specs from PR artifacts.
3. Add sandbox controls for orchestrator execution (timeouts, limited workspace mounts).
4. Add structured diagnostics schema for orchestrator responses.

---

For more context see the formal SRS update: [SRS.md](../SRS.md).
