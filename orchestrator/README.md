# AgentVST Orchestrator (Stub)

This service is a minimal API for running:

1. schema validation + scaffold generation
2. optional CMake build
3. optional CTest run
4. optional deterministic audio acceptance fixtures

It is intentionally conservative and keeps paths inside the repository root.

Recommended Python: 3.11-3.13.

## Install

```bash
pip install -r orchestrator/requirements.txt
```

## Run

```bash
python orchestrator/app.py
```

## Health Check

```bash
curl http://127.0.0.1:8000/health
```

## Generate Request

```bash
curl -X POST http://127.0.0.1:8000/generate \
  -H "Content-Type: application/json" \
  -d "{\"schemaPath\":\"schema/example_plugin.json\",\"outputDir\":\"generated\",\"force\":true}"
```

Example with optional checks enabled:

```bash
curl -X POST http://127.0.0.1:8000/generate \
  -H "Content-Type: application/json" \
  -d "{\"schemaPath\":\"schema/example_plugin.json\",\"outputDir\":\"generated\",\"force\":true,\"runBuild\":true,\"runTests\":true,\"runAudioAcceptance\":true,\"buildConfig\":\"Debug\"}"
```

The response includes per-step stdout/stderr and exit codes.
