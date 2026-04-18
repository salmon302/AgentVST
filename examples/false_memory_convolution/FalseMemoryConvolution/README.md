# FalseMemoryConvolution (Generated Scaffold)

This directory was generated from `I:\Documents\GitHub\AgentVST\schema\false_memory_convolution.json` at 2026-04-18 04:52:43Z.

## Files

- `plugin.json` - validated schema input copied into this directory
- `CMakeLists.txt` - `agentvst_add_plugin` entry for `FalseMemoryConvolution`
- `src/FalseMemoryConvolutionDSP.cpp` - starter `processSample` implementation

## Next Steps

1. Add this folder with `add_subdirectory(...)` in your root CMake project.
2. Refine DSP logic in the generated source file.
3. Validate schema changes with `python scripts/validate_schema.py <path-to-plugin.json>`.
4. During testing, check logs for `Potential no-op DSP detected` warnings.

## Troubleshooting

- If audio sounds unchanged, verify your DSP does not return input unchanged under active settings.
- If build succeeds but DAW still runs old behavior, ensure plugin deploy copy was not blocked by a host lock.
- Close the DAW (or unload plugin), rebuild, and confirm the deployed `.vst3` timestamp changed.
