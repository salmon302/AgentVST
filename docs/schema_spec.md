# AgentVST Schema Specification

Version: 1.0 (implementation-aligned)

This document defines the JSON schema format consumed by AgentVST at plugin initialization.
It reflects the behavior currently implemented in `SchemaParser`.

## 1. File Format

- Encoding: UTF-8 JSON text
- Required top-level type: object
- Top-level keys (all optional):
  - `plugin`
  - `parameters`
  - `groups`
  - `dsp_routing`

Unknown keys are ignored by the parser.

## 2. Top-Level Structure

```json
{
  "plugin": {
    "name": "MyPlugin",
    "version": "1.0.0",
    "vendor": "AgentVST",
    "category": "Fx"
  },
  "parameters": [],
  "groups": [],
  "dsp_routing": []
}
```

## 3. Plugin Metadata (`plugin`)

Type: object (optional)

Supported fields:
- `name` (string, default: `"AgentPlugin"`)
- `version` (string, default: `"1.0.0"`)
- `vendor` (string, default: `"AgentVST"`)
- `category` (string, default: `"Fx"`)

If `plugin` is missing, all metadata defaults are used.

## 4. Parameters (`parameters`)

Type: array of parameter objects (optional)

Each parameter must include:
- `id` (string, required)
- `name` (string, required)

Optional fields common to parameter types:
- `type` (string, default: `"float"`)
- `unit` (string, default: `""`)
- `group` (string, default: `""`)

Supported `type` values and required fields are below.

### 4.1 Float Parameter (`type: "float"`)

Required fields:
- `min` (number)
- `max` (number)

Optional fields:
- `default` (number, default: `min`)
- `step` (number, default: `0.0`)
- `skew` (number, default: `1.0`)

Validation rules:
- `min < max`
- `default` must be inside `[min, max]`
- `skew > 0`

### 4.2 Integer Parameter (`type: "int"`)

Required fields:
- `min` (number)
- `max` (number)

Optional fields:
- `default` (number, default: `min`)
- `step` (number, default: `0.0`)
- `skew` (number, default: `1.0`)

Validation rules are the same as float parameters.

Note: the parser currently does not enforce integer-only values for `min`, `max`, `default`, or `step`. This is accepted but integer-safe values are recommended.

### 4.3 Boolean Parameter (`type: "boolean"`)

Optional fields:
- `default` (boolean, default: `false`)

Derived internal values:
- `min = 0.0`
- `max = 1.0`
- `step = 1.0`
- `default = 1.0` when `true`, else `0.0`

### 4.4 Enum Parameter (`type: "enum"`)

Required fields:
- `options` (non-empty array of strings)

Optional fields:
- `default` (number, default: `0`)

Derived internal values:
- `min = 0.0`
- `max = options.size - 1`
- `step = 1.0`

Note: the parser currently does not reject out-of-range enum defaults. Use a default in `[0, options.size - 1]` for predictable behavior.

### 4.5 Parameter ID Constraints

- Parameter IDs must be unique across `parameters`.
- Duplicate IDs raise a validation error.

## 5. Groups (`groups`)

Type: array of group objects (optional)

Each group supports:
- `id` (string, required)
- `name` (string, default: same as `id`)
- `parameters` (array of parameter IDs, optional)

Validation rules:
- Every ID listed in `groups[].parameters` must exist in `parameters`.
- Unknown references raise a validation error.

Notes:
- A parameter can appear in a group list and/or include a `group` field; current UI grouping uses `groups[].parameters` for grouped sections.

## 6. DSP Routing (`dsp_routing`)

Type: array of route objects (optional)

Each route requires:
- `source` (string)
- `destination` (string)

Optional field:
- `parameter_link` (object)
  - key: schema parameter ID
  - value: node parameter name

Example:

```json
{
  "source": "input",
  "destination": "main_filter",
  "parameter_link": {
    "cutoff": "frequency",
    "resonance": "q"
  }
}
```

Parser-level validation checks only presence of `source` and `destination`.
Graph validity (cycles, disconnected paths, endpoint reachability) is enforced later by `DSPRouter` when routing is initialized.

## 7. Error Reporting

Error categories:
- Parse errors: malformed JSON or invalid field shape
- Validation errors: duplicate IDs or bad references

Error messages include source context (file path or `<string>`) when available.

## 8. Complete Example

```json
{
  "plugin": {
    "name": "SimpleGain",
    "version": "1.0.0",
    "vendor": "AgentVST",
    "category": "Fx"
  },
  "parameters": [
    {
      "id": "gain_db",
      "name": "Gain",
      "type": "float",
      "min": -60.0,
      "max": 12.0,
      "default": 0.0,
      "unit": "dB",
      "step": 0.1,
      "skew": 1.0
    },
    {
      "id": "bypass",
      "name": "Bypass",
      "type": "boolean",
      "default": false
    },
    {
      "id": "mode",
      "name": "Mode",
      "type": "enum",
      "options": ["Clean", "Color"],
      "default": 0
    }
  ],
  "groups": [
    {
      "id": "main",
      "name": "Main",
      "parameters": ["gain_db", "bypass", "mode"]
    }
  ],
  "dsp_routing": [
    {
      "source": "input",
      "destination": "gain",
      "parameter_link": {
        "gain_db": "gain_db"
      }
    },
    {
      "source": "gain",
      "destination": "output"
    }
  ]
}
```
