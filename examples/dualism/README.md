Dualism Undertone Anchor prototype

This folder contains a minimal, implementation-focused scaffold for the `Dualism` Undertone Anchor plugin.

Files:
- `src/DualismProcessor.h/.cpp` — core processing class (non-JUCE minimal API)
- `src/PitchDetector.h/.cpp` — simple YIN/autocorrelation placeholder for fundamental detection
- `src/UndertoneBank.h/.cpp` — bank of band-limited sine partials to synthesize undertones

Notes:
- These files are a lightweight prototype and do not yet integrate with the project's CMake or JUCE wrapper. Use them as a starting point for integration into the plugin framework.
