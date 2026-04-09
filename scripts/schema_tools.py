#!/usr/bin/env python3
"""Shared helpers for AgentVST schema validation tools."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, List, TYPE_CHECKING

if TYPE_CHECKING:
    from jsonschema import Draft202012Validator


class SchemaValidationError(Exception):
    """Raised when a schema specification fails validation."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def canonical_schema_path(root: Path | None = None) -> Path:
    base = root if root is not None else repo_root()
    return base / "schema" / "plugin.schema.json"


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def make_validator(schema_path: Path) -> Any:
    from jsonschema import Draft202012Validator
    schema_data = load_json(schema_path)
    return Draft202012Validator(schema_data)


def format_error_path(error_path: List[Any]) -> str:
    if not error_path:
        return "/"
    return "/" + "/".join(str(part) for part in error_path)


def validate_spec_data(
    validator: Any,
    spec_data: Any,
    source_label: str,
) -> list[str]:
    errors: list[str] = []
    for error in sorted(validator.iter_errors(spec_data), key=lambda item: list(item.absolute_path)):
        pointer = format_error_path(list(error.absolute_path))
        errors.append(f"{source_label}{pointer}: {error.message}")
    return errors
