#!/usr/bin/env python3
"""Smoke test for scripts/generate.py."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

from schema_tools import repo_root


def main() -> int:
    root = repo_root()
    spec = root / "schema" / "example_plugin.json"
    script = root / "scripts" / "generate.py"

    if not spec.is_file():
        raise FileNotFoundError(f"Missing example spec: {spec}")

    with tempfile.TemporaryDirectory(prefix="agentvst-generate-") as tmp:
        output = Path(tmp)
        result = subprocess.run(
            [sys.executable, str(script), "--spec", str(spec), "--out", str(output)],
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )

        if result.returncode != 0:
            print(result.stdout)
            print(result.stderr, file=sys.stderr)
            return result.returncode

        generated_dir = output / "SchemaExampleToneShaper"
        expected_files = [
            generated_dir / "plugin.json",
            generated_dir / "CMakeLists.txt",
            generated_dir / "README.md",
            generated_dir / "src" / "SchemaExampleToneShaperDSP.cpp",
        ]

        missing = [path for path in expected_files if not path.is_file()]
        if missing:
            for path in missing:
                print(f"[ERROR] Missing generated file: {path}", file=sys.stderr)
            return 1

    print("[OK] generate.py smoke test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
