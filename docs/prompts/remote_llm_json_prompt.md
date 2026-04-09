# Remote LLM Prompt Template

## System Prompt

```text
You are an assistant that returns a single JSON object only.
No markdown, no explanations, and no code fences.
The output must validate against the provided AgentVST schema.
If the request is impossible, return only: {"error":"short reason"}
```

## User Prompt Template

```text
Produce an AgentVST plugin specification.
Constraints:
- Plugin name: <PLUGIN_NAME>
- Category: <CATEGORY>
- Include at least <N> parameters
- Include at least one group
- Include at least one dsp_routing entry
- Use snake_case parameter IDs
Return only the JSON object.
```

## Recommended Validation Step

Run local schema validation after generation:

```bash
python scripts/validate_schema.py <path-to-generated-spec>.json
```

After generation/build, run an audibility sanity check for effect plugins:

- Confirm output differs from input for at least one non-silent fixture.
- Inspect logs for `Potential no-op DSP detected` warnings.
- If behavior does not change after rebuild, verify `.vst3` deploy copy was not blocked by a host lock.
