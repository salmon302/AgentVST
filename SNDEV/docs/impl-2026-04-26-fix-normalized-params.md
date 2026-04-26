Title: impl-2026-04-26-fix-normalized-params
Date: 2026-04-26T18:30:00Z
Author: Seth Nenninger (GitHub Copilot Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc — all VSTs have glitchy/distorted audio
Summary: Fixed APVTS parameter denormalization so DSP code receives real-world values instead of normalized 0.0–1.0 floats.
Task Reference:
  Conception log: SNDEV/docs/conception-2026-04-26-fix-normalized-params.md
Specification Summary:
  All VST plugins (AgentEQ, SimpleGain, AgentCompressor, etc.) produced glitchy/distorted audio because the ParameterCache was returning raw normalized APVTS values (0.0–1.0) instead of denormalized real-world values.
Implementation Notes:
  Files changed:
  1. framework/include/ParameterCache.h
     - Added minValue, maxValue, skew, isBoolean fields to Entry struct
     - Added denormalize() static method
     - Updated registerParameter() signature to accept range info
     - Updated getValue() and copyValuesTo() to denormalize on read
  2. framework/src/ParameterCache.cpp
     - Implemented denormalize() with linear and skewed mappings
     - Updated registerParameter() to store range metadata
     - Updated getValue() to denormalize before returning
     - Updated copyValuesTo() to denormalize each value
  3. framework/src/PluginWrapper/AgentVSTProcessor.cpp
     - Updated initParameterCache() to pass schema min/max/skew/type to registerParameter()
  4. tests/unit/ParameterCacheTest.cpp
     - Rewrote all tests to use normalized values (0.0–1.0) with proper range registration
     - Added new test cases for boolean parameters and skewed denormalization
  No changes needed to any agent DSP code — they already call ctx.getParameter() which now returns correct values.
Evidence links:
  Build: cmake --build build --config Debug --target AgentEQ_VST3 SimpleGain_VST3 AgentCompressor_VST3 — PASS
  Tests: .\build\tests\Debug\agentvst_tests.exe — 5114 assertions in 61 test cases — PASS
  ParameterCache tests: .\build\tests\Debug\agentvst_tests.exe "[cache]" — 40 assertions in 10 test cases — PASS
Verification & Quality Gates:
  Build: PASS (AgentEQ, SimpleGain, AgentCompressor all compile successfully)
  Tests: PASS (61 test cases, 5114 assertions, 0 failures)
  Lint/Typecheck: N/A (C++ project, no linter configured)
