#!/usr/bin/env python3
"""Run deterministic audio acceptance fixtures for AgentVST examples."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def find_parameter_ids(plugin_spec: dict) -> set[str]:
    return {str(param.get("id", "")) for param in plugin_spec.get("parameters", [])}


def generate_sine(sample_rate: int, duration_seconds: float, frequency_hz: float, amplitude: float) -> list[float]:
    total_samples = max(1, int(round(sample_rate * duration_seconds)))
    two_pi_f = 2.0 * math.pi * frequency_hz
    return [
        amplitude * math.sin(two_pi_f * (index / sample_rate))
        for index in range(total_samples)
    ]


def process_simple_gain(samples: list[float], parameters: dict[str, float]) -> list[float]:
    bypass = float(parameters.get("bypass", 0.0)) >= 0.5
    if bypass:
        return list(samples)

    gain_db = float(parameters.get("gain_db", 0.0))
    gain_lin = math.pow(10.0, gain_db / 20.0)
    return [sample * gain_lin for sample in samples]


def process_compressor_v1(samples: list[float], parameters: dict[str, float], sample_rate: int) -> list[float]:
    """Deterministic reference model aligned with examples/compressor/CompressorDSP.cpp."""
    bypass = float(parameters.get("bypass", 0.0)) >= 0.5
    if bypass:
        return list(samples)

    threshold_db = float(parameters.get("threshold_db", -18.0))
    ratio = max(1.0, float(parameters.get("ratio", 4.0)))
    attack_ms = max(0.01, float(parameters.get("attack_ms", 10.0)))
    release_ms = max(0.01, float(parameters.get("release_ms", 100.0)))
    makeup_db = float(parameters.get("makeup_db", 0.0))
    knee_db = max(0.0, float(parameters.get("knee_db", 2.0)))

    sr = float(sample_rate)
    attack_coeff = math.exp(-1.0 / (sr * attack_ms * 0.001))
    release_coeff = math.exp(-1.0 / (sr * release_ms * 0.001))
    rms_coeff = math.exp(-1.0 / (sr * 0.050))

    rms_state = 0.0
    gr_state = 0.0
    output: list[float] = []

    for sample in samples:
        x_sq = sample * sample
        rms_state = rms_coeff * rms_state + (1.0 - rms_coeff) * x_sq
        rms_lin = math.sqrt(max(0.0, rms_state))

        level_db = (20.0 * math.log10(rms_lin)) if rms_lin > 1e-9 else -120.0
        over_db = level_db - threshold_db
        half_knee = knee_db * 0.5

        gain_reduction_db = 0.0
        if knee_db > 0.0 and -half_knee < over_db < half_knee:
            t = (over_db + half_knee) / knee_db
            gain_reduction_db = (1.0 / ratio - 1.0) * over_db * t * t * 0.5
        elif over_db >= half_knee:
            gain_reduction_db = (1.0 / ratio - 1.0) * over_db

        target_gr_db = gain_reduction_db
        if target_gr_db < gr_state:
            gr_state = attack_coeff * gr_state + (1.0 - attack_coeff) * target_gr_db
        else:
            gr_state = release_coeff * gr_state + (1.0 - release_coeff) * target_gr_db

        total_db = gr_state + makeup_db
        gain_lin = math.pow(10.0, total_db / 20.0)
        output.append(sample * gain_lin)

    return output


def rms(values: list[float]) -> float:
    if not values:
        return 0.0
    return math.sqrt(sum(value * value for value in values) / len(values))


def max_abs(values: list[float]) -> float:
    if not values:
        return 0.0
    return max(abs(value) for value in values)


def evaluate_fixture(root: Path, fixture_path: Path) -> tuple[bool, str]:
    fixture = load_json(fixture_path)

    name = str(fixture.get("name", fixture_path.stem))
    plugin_spec_rel = fixture.get("pluginSpec")
    model = fixture.get("model")
    sample_rate = int(fixture.get("sampleRate", 48000))
    duration = float(fixture.get("durationSeconds", 0.5))
    freq_hz = float(fixture.get("frequencyHz", 1000.0))
    amplitude = float(fixture.get("inputAmplitude", 0.2))
    parameters = dict(fixture.get("parameters", {}))
    assertions = dict(fixture.get("assertions", {}))

    if not plugin_spec_rel:
        return False, f"{name}: missing pluginSpec"
    if model not in {"simple_gain_v1", "compressor_v1"}:
        return False, f"{name}: unsupported model '{model}'"

    plugin_spec_path = (root / str(plugin_spec_rel)).resolve()
    if not plugin_spec_path.is_file():
        return False, f"{name}: plugin spec not found: {plugin_spec_path}"

    plugin_spec = load_json(plugin_spec_path)
    known_ids = find_parameter_ids(plugin_spec)
    unknown_params = sorted(param_id for param_id in parameters.keys() if param_id not in known_ids)
    if unknown_params:
        return False, f"{name}: unknown parameters in fixture: {', '.join(unknown_params)}"

    input_signal = generate_sine(sample_rate, duration, freq_hz, amplitude)
    if model == "simple_gain_v1":
        output_signal = process_simple_gain(input_signal, parameters)
    elif model == "compressor_v1":
        output_signal = process_compressor_v1(input_signal, parameters, sample_rate)
    else:
        return False, f"{name}: unsupported model '{model}'"

    input_rms = rms(input_signal)
    output_rms = rms(output_signal)
    ratio = (output_rms / input_rms) if input_rms > 0.0 else 0.0

    rms_assert = assertions.get("rmsRatio")
    if not isinstance(rms_assert, dict):
        return False, f"{name}: missing assertions.rmsRatio"

    expected_ratio = float(rms_assert.get("expected"))
    tolerance = float(rms_assert.get("tolerance", 0.0))
    ratio_diff = abs(ratio - expected_ratio)

    if ratio_diff > tolerance:
        return (
            False,
            f"{name}: rmsRatio mismatch (actual={ratio:.6f}, expected={expected_ratio:.6f}, "
            f"diff={ratio_diff:.6f}, tol={tolerance:.6f})",
        )

    max_abs_assert = assertions.get("maxAbs")
    if isinstance(max_abs_assert, dict) and "lte" in max_abs_assert:
        limit = float(max_abs_assert["lte"])
        out_peak = max_abs(output_signal)
        if out_peak > limit:
            return False, f"{name}: maxAbs exceeded (actual={out_peak:.6f}, limit={limit:.6f})"

    summary = (
        f"{name}: PASS "
        f"(rmsRatio={ratio:.6f}, inputRMS={input_rms:.6f}, outputRMS={output_rms:.6f}, "
        f"peak={max_abs(output_signal):.6f})"
    )
    return True, summary


def discover_fixtures(fixtures_dir: Path) -> list[Path]:
    return sorted(path for path in fixtures_dir.glob("*.json") if path.is_file())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixtures-dir",
        default=None,
        help="Optional fixtures directory path. Defaults to tests/audio-fixtures.",
    )
    args = parser.parse_args()

    root = repo_root()
    fixtures_dir = Path(args.fixtures_dir).resolve() if args.fixtures_dir else (root / "tests" / "audio-fixtures")

    if not fixtures_dir.is_dir():
        print(f"[ERROR] Fixtures directory not found: {fixtures_dir}", file=sys.stderr)
        return 1

    fixtures = discover_fixtures(fixtures_dir)
    if not fixtures:
        print(f"[ERROR] No fixture .json files found in: {fixtures_dir}", file=sys.stderr)
        return 1

    failures: list[str] = []
    passes = 0

    for fixture_path in fixtures:
        ok, message = evaluate_fixture(root, fixture_path)
        if ok:
            passes += 1
            print(f"[OK] {message}")
        else:
            failures.append(message)
            print(f"[FAIL] {message}")

    print(f"\nAudio acceptance fixtures: {passes}/{len(fixtures)} passed")

    if failures:
        print("\nFailures:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
