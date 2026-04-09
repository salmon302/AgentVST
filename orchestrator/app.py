#!/usr/bin/env python3
"""Minimal orchestration API for schema -> generate -> optional build/test."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

FASTAPI_IMPORT_ERROR: Exception | None = None

try:
    from fastapi import FastAPI, HTTPException
    from pydantic import BaseModel, Field
except Exception as import_error:  # noqa: BLE001
    FASTAPI_IMPORT_ERROR = import_error

    class HTTPException(RuntimeError):
        def __init__(self, status_code: int, detail: str):
            super().__init__(detail)
            self.status_code = status_code
            self.detail = detail

    class BaseModel:
        def __init__(self, **kwargs):
            for key, value in kwargs.items():
                setattr(self, key, value)

    def Field(default=None, **_kwargs):
        return default

    class FastAPI:
        def __init__(self, *args, **kwargs):
            self.args = args
            self.kwargs = kwargs

        def get(self, *_args, **_kwargs):
            def decorator(func):
                return func

            return decorator

        def post(self, *_args, **_kwargs):
            def decorator(func):
                return func

            return decorator


REPO_ROOT = Path(__file__).resolve().parents[1]
GENERATE_SCRIPT = REPO_ROOT / "scripts" / "generate.py"
AUDIO_ACCEPTANCE_SCRIPT = REPO_ROOT / "tests" / "audio-fixtures" / "run_audio_acceptance.py"

app = FastAPI(title="AgentVST Orchestrator", version="0.1.0")


class GenerateRequest(BaseModel):
    schemaPath: str = Field(..., description="Path to plugin spec JSON, relative to repo root or absolute.")
    outputDir: str = Field("generated", description="Destination directory for generated scaffold.")
    force: bool = Field(True, description="Overwrite existing generated output folder.")
    runBuild: bool = Field(False, description="If true, run CMake configure/build after generation.")
    runTests: bool = Field(False, description="If true and runBuild is true, run ctest after build.")
    runAudioAcceptance: bool = Field(False, description="If true, run deterministic audio fixture acceptance tests.")
    buildConfig: str = Field("Release", description="CMake build configuration.")


class StepResult(BaseModel):
    name: str
    command: list[str]
    exitCode: int
    stdout: str
    stderr: str


class GenerateResponse(BaseModel):
    ok: bool
    generatedPath: str | None = None
    steps: list[StepResult]


def sanitize_identifier(text: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_]", "_", text)
    cleaned = re.sub(r"_+", "_", cleaned).strip("_")
    if not cleaned:
        cleaned = "AgentPlugin"
    if cleaned[0].isdigit():
        cleaned = f"_{cleaned}"
    return cleaned


def ensure_in_repo(path: Path) -> Path:
    resolved = path.resolve()
    try:
        resolved.relative_to(REPO_ROOT)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=f"Path must stay inside repository: {resolved}") from exc
    return resolved


def resolve_repo_path(raw_path: str, must_exist: bool = False) -> Path:
    candidate = Path(raw_path)
    if not candidate.is_absolute():
        candidate = REPO_ROOT / candidate

    candidate = ensure_in_repo(candidate)

    if must_exist and not candidate.exists():
        raise HTTPException(status_code=404, detail=f"Path not found: {candidate}")

    return candidate


def run_step(name: str, command: list[str], cwd: Path, timeout_seconds: int = 1200) -> StepResult:
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            capture_output=True,
            text=True,
            check=False,
            timeout=timeout_seconds,
        )
    except subprocess.TimeoutExpired as exc:
        return StepResult(
            name=name,
            command=command,
            exitCode=-1,
            stdout=exc.stdout or "",
            stderr=(exc.stderr or "") + f"\nCommand timed out after {timeout_seconds} seconds.",
        )

    return StepResult(
        name=name,
        command=command,
        exitCode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
    )


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.post("/generate", response_model=GenerateResponse)
def generate(request: GenerateRequest) -> GenerateResponse:
    schema_path = resolve_repo_path(request.schemaPath, must_exist=True)
    if schema_path.suffix.lower() != ".json":
        raise HTTPException(status_code=400, detail="schemaPath must point to a .json file.")

    output_dir = resolve_repo_path(request.outputDir, must_exist=False)
    output_dir.mkdir(parents=True, exist_ok=True)

    if not GENERATE_SCRIPT.is_file():
        raise HTTPException(status_code=500, detail=f"Generator script missing: {GENERATE_SCRIPT}")

    steps: list[StepResult] = []

    generate_command = [
        sys.executable,
        str(GENERATE_SCRIPT),
        "--spec",
        str(schema_path),
        "--out",
        str(output_dir),
    ]
    if request.force:
        generate_command.append("--force")

    step = run_step("generate", generate_command, cwd=REPO_ROOT)
    steps.append(step)
    if step.exitCode != 0:
        return GenerateResponse(ok=False, steps=steps)

    try:
        spec_data = json.loads(schema_path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail=f"Failed to read generated schema metadata: {exc}") from exc

    plugin_name = spec_data.get("plugin", {}).get("name", schema_path.stem)
    generated_path = output_dir / sanitize_identifier(plugin_name)

    if request.runBuild:
        configure_step = run_step(
            "cmake-configure",
            [
                "cmake",
                "-S",
                ".",
                "-B",
                "build",
                f"-DCMAKE_BUILD_TYPE={request.buildConfig}",
                "-DAGENTVST_BUILD_EXAMPLES=ON",
                "-DAGENTVST_BUILD_TESTS=ON",
            ],
            cwd=REPO_ROOT,
        )
        steps.append(configure_step)
        if configure_step.exitCode != 0:
            return GenerateResponse(ok=False, generatedPath=str(generated_path), steps=steps)

        build_step = run_step(
            "cmake-build",
            [
                "cmake",
                "--build",
                "build",
                "--config",
                request.buildConfig,
                "--parallel",
            ],
            cwd=REPO_ROOT,
        )
        steps.append(build_step)
        if build_step.exitCode != 0:
            return GenerateResponse(ok=False, generatedPath=str(generated_path), steps=steps)

        if request.runTests:
            test_step = run_step(
                "ctest",
                [
                    "ctest",
                    "--test-dir",
                    "build",
                    "--build-config",
                    request.buildConfig,
                    "--output-on-failure",
                    "--parallel",
                    "4",
                ],
                cwd=REPO_ROOT,
            )
            steps.append(test_step)
            if test_step.exitCode != 0:
                return GenerateResponse(ok=False, generatedPath=str(generated_path), steps=steps)

    if request.runAudioAcceptance:
        if not AUDIO_ACCEPTANCE_SCRIPT.is_file():
            raise HTTPException(status_code=500, detail=f"Audio acceptance script missing: {AUDIO_ACCEPTANCE_SCRIPT}")

        audio_step = run_step(
            "audio-acceptance",
            [
                sys.executable,
                str(AUDIO_ACCEPTANCE_SCRIPT),
            ],
            cwd=REPO_ROOT,
        )
        steps.append(audio_step)
        if audio_step.exitCode != 0:
            return GenerateResponse(ok=False, generatedPath=str(generated_path), steps=steps)

    return GenerateResponse(ok=True, generatedPath=str(generated_path), steps=steps)


if __name__ == "__main__":
    if FASTAPI_IMPORT_ERROR is not None:
        raise SystemExit(
            "FastAPI dependencies are unavailable in this Python runtime. "
            "Use Python 3.11-3.13 with orchestrator/requirements.txt.\n"
            f"Import error: {FASTAPI_IMPORT_ERROR}"
        )

    import uvicorn

    uvicorn.run(app, host="127.0.0.1", port=8000, reload=False)
