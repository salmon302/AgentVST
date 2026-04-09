# Devils Grid Irrational Delay (Generated Scaffold)

This directory was created from schema/devils_grid_irrational_delay.json.

## Concept

The delay time is synchronized to host tempo, then multiplied by either sqrt(2)
or 1/sqrt(2). Because the multiplier is irrational, repeats continuously slide
against the DAW grid and avoid exact long-term periodic lock.

## Files

- plugin.json - plugin metadata and parameter schema
- CMakeLists.txt - agentvst_add_plugin entry
- src/Devils_Grid_Irrational_DelayDSP.cpp - real-time safe DSP implementation

## Next Steps

1. Build with your normal CMake workflow.
2. Automate note division and irrational mode for transition effects.
3. Tune defaults for your production context (mix, feedback, drift).