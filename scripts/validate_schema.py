#!/usr/bin/env python3
"""Validate AgentVST plugin spec JSON files against the canonical JSON Schema."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from schema_tools import canonical_schema_path, load_json, make_validator, repo_root, validate_spec_data


def default_targets(root: Path) -> list[Path]:
    targets: list[Path] = []

    canonical_example = root / "schema" / "example_plugin.json"
    if canonical_example.is_file():
        targets.append(canonical_example)

    for plugin_json in sorted((root / "examples").glob("*/plugin.json")):
        if plugin_json.is_file():
            targets.append(plugin_json)

    return targets


def collect_targets(root: Path, raw_targets: list[str]) -> list[Path]:
    if not raw_targets:
        return default_targets(root)

    resolved: list[Path] = []
    for target in raw_targets:
        path = Path(target)
        if not path.is_absolute():
            path = (root / path).resolve()

        if path.is_dir():
            resolved.extend(sorted(path.rglob("*.json")))
        else:
            resolved.append(path)

    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "targets",
        nargs="*",
        help="JSON files or directories to validate. If omitted, validates schema/example_plugin.json and examples/*/plugin.json.",
    )
    parser.add_argument(
        "--schema",
        default=None,
        help="Path to JSON Schema file. Defaults to schema/plugin.schema.json.",
    )

    args = parser.parse_args()

    root = repo_root()
    schema_path = Path(args.schema).resolve() if args.schema else canonical_schema_path(root)

    if not schema_path.is_file():
        print(f"[ERROR] Schema file not found: {schema_path}", file=sys.stderr)
        return 1

    targets = collect_targets(root, args.targets)
    if not targets:
        print("[ERROR] No JSON targets found.", file=sys.stderr)
        return 1

    validator = make_validator(schema_path)
    print(f"Using schema: {schema_path}")

    failed = False
    validated_count = 0

    for target in targets:
        if not target.exists():
            print(f"[ERROR] Missing target: {target}", file=sys.stderr)
            failed = True
            continue

        try:
            payload = load_json(target)
        except Exception as exc:  # noqa: BLE001
            print(f"[ERROR] Failed to parse JSON: {target}: {exc}", file=sys.stderr)
            failed = True
            continue

        errors = validate_spec_data(validator, payload, str(target))
        if errors:
            failed = True
            print(f"[FAIL] {target}")
            for error in errors:
                print(f"  - {error}")
        else:
            validated_count += 1
            print(f"[OK] {target}")

    print(f"\nValidated specs: {validated_count}/{len(targets)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
