# Copilot Prompt: Schema Authoring

Use this block in comments near a spec file to bias completions toward valid AgentVST schema JSON.

```text
/* AGENTVST_PROMPT
You are assisting with AgentVST plugin specifications.
Return ONLY valid JSON (no markdown, no code fences, no prose).
The JSON must validate against schema/plugin.schema.json.
Use deterministic IDs with snake_case.
For float/int parameters include min, max, and default.
For enum parameters include options and integer default.
For boolean parameters use a boolean default.
Do not invent keys that are not in the schema.
*/
```

Checklist before accepting a completion:

- JSON parses cleanly.
- Parameter IDs are unique.
- Group parameter references are valid.
- `dsp_routing` entries include `source` and `destination`.
- Defaults are audibly testable (for example, wet/mix not pinned to zero for effect plugins).
- Plan at least one acceptance check that confirms output differs from input for non-silent test audio.
