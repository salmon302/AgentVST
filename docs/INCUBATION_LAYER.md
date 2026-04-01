# AgentVST — Incubation-Layer Guidelines

## Overview

AgentVST can operate as an incubation layer for existing AI development tools. Rather than re-implementing model logic, AgentVST provides deterministic schemas, prompt templates, code templates, and validation harnesses that make editor assistants (e.g., GitHub Copilot) and remote LLMs (e.g., Claude Code) more reliable at producing production-ready plugins.

This document summarizes recommended workflows, sample prompts, and minimal quick-start steps to integrate an assistant-driven workflow into this repository.

## Quick Start (Editor-first: Copilot)

1. Edit or create a plugin spec under `schema/` (JSON conforming to the project's schema).
2. Open the spec file and use the provided Copilot instruction block from `docs/prompts/` (or the example below) to ask Copilot to complete or expand the spec.
3. Run the local generator (planned: `scripts/generate.py`) to render the JUCE skeleton from the validated JSON.
4. Build and run tests using the project's normal CMake workflow.

Windows example (local build):

```powershell
cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Debug --target agentvst_tests
```

## Quick Start (Remote LLMs: Orchestrator)

1. Post the schema and the prompt to the orchestrator API (stubbed under `orchestrator/`).
2. Orchestrator requires JSON-only responses; it will validate the response against the JSON Schema, then run the generate→build→test pipeline.
3. Orchestrator returns machine-readable diagnostics (build errors, test failures, static analysis warnings) and an artifact location for human review.

Example (curl to orchestrator stub):

```bash
curl -X POST http://localhost:8000/generate \
  -H 'Content-Type: application/json' \
  -d '{"schemaPath":"schema/example_plugin.json","promptTemplate":"plugin_spec_v1"}'
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

## Repository Layout (recommended additions)

- `schema/` — JSON Schema(s) and example plugin specs.
- `docs/prompts/` — curated prompt templates (editor and remote).
- `scripts/generate.py` — template-to-code generator (maps schema → JUCE skeleton).
- `orchestrator/` — optional FastAPI service that runs generate→build→test.
- `tests/audio-fixtures/` — input audio and expected metrics for acceptance testing.

## Safety and Validation

- Require JSON-only outputs from LLMs and validate them against schema before generation.
- Run static analysis (`clang-tidy`, `clang-format`) and build sanitizers (ASan/UBSan) in CI.
- Sandbox builds and tests in ephemeral environments or containers before merging.
- Keep human-in-the-loop approval gates for any code that touches real-time audio threads.

## Next Steps (recommended immediate work)

1. Add `schema/plugin.schema.json` (canonical schema) and one example spec.
2. Add `docs/prompts/` with the two templates above (Copilot + orchestrator).
3. Implement `scripts/generate.py` (template-only) and a minimal `orchestrator/` stub.
4. Add a GitHub Actions template to run generate→build→test on PRs.

---

For more context see the formal SRS update: [SRS.md](../SRS.md).
