# AgentVST Plugin Deployment Guide

## Quick Start (Correct Way)

```bash
# Configure for Release build deployment to Ableton VST3 folder
cmake -S . -B build -DAGENTVST_VST3_DEPLOY_DIR="C:\Users\salmo\Documents\VST3\AgentVST-Dev"

# Build and deploy Release version (REQUIRED for DAW use)
cmake --build build --config Release --parallel
```

## The Problem: Stale Deployments

**AgentVST plugins stopped producing audio** because deployed binaries were outdated:

| Issue | Debug Build | Release Build |
|-------|-------------|----------------|
| **Size** | 19MB (symbols included) | 6MB (optimized) |
| **Date** | Apr 4-5 (stale) | Apr 9+ (current) |
| **Audio Output** | ❌ No | ✅ Yes |
| **DAW Performance** | Slow | Optimized |

### Why This Happens

1. **Debug vs Release Confusion**: Debug builds include full debug symbols and are unoptimized. Release builds strip symbols and optimize for performance.
2. **DAW Caching**: DAWs like Ableton cache plugin metadata and may refuse to reload if a file is locked.
3. **CMake Configuration Drift**: If CMake is configured with Debug mode, builds default to Debug even when using `--config Release`.

## Best Practices

### ✅ DO

- **Always use Release builds for deployment** to DAWs
  ```bash
  cmake --build build --config Release
  ```

- **Restart your DAW** after deploying new plugins
  - Ableton may cache plugin metadata even if you rescan folders

- **Verify deployment timestamps** before testing
  ```bash
  ls -lh "C:\Users\salmo\Documents\VST3\AgentVST-Dev\*/Contents/x86_64-win/*.vst3"
  ```

- **Use `_configure_with_ableton_vst3_deploy.cmd`** to automate setup
  ```cmd
  _configure_with_ableton_vst3_deploy.cmd
  cmake --build build --config Release --parallel
  ```

### ❌ DON'T

- Don't use Debug builds for production DAW testing (too large, too slow)
- Don't assume CMake remembered your last configuration
- Don't skip restarting the DAW after deploying new plugins
- Don't mix Debug and Release builds in the same deployment

## Deployment Workflow

### One-Time Setup

```cmd
:: Configure CMake for Ableton VST3 deployment
_configure_with_ableton_vst3_deploy.cmd
```

### Build and Deploy

```cmd
:: Build all plugins in Release mode
cmake --build build --config Release --parallel

:: Verify deployment
dir "C:\Users\salmo\Documents\VST3\AgentVST-Dev"

:: Restart Ableton Live manually, then rescan VST3 folder
```

### Test in DAW

1. **Load a plugin** (e.g., SimpleGain)
2. **Verify it processes audio** by changing a parameter:
   - SimpleGain: Set Gain to +6dB, should hear volume increase
   - AgentCompressor: Set threshold/ratio, should hear dynamic control
3. **Check meters** in the plugin UI to see input/output levels

## Build Modes Explained

| Mode | Use Case | Size | Speed | Debug Info |
|------|----------|------|-------|-----------|
| **Debug** | Development & testing locally | Large (19MB) | Slow | Full symbols |
| **Release** | DAW deployment (REQUIRED) | Small (6MB) | Fast | No symbols |

## Troubleshooting

### Plugins Show "No Output" in DAW

**Check 1: Deployment Timestamp**
```bash
# Is the binary recent?
ls -lh "C:\Users\salmo\Documents\VST3\AgentVST-Dev\SimpleGain.vst3\Contents\x86_64-win\SimpleGain.vst3"
# Should be today's date, not several days old
```

**Check 2: Build Configuration**
```bash
# Did you use Release?
cmake --build build --config Release

# NOT Debug:
cmake --build build --config Debug  # ❌ WRONG
```

**Check 3: DAW Cache**
- Close Ableton completely
- Wait 5 seconds
- Reopen Ableton
- Rescan VST3 folder in settings
- Reload the plugin

**Check 4: Plugin Validation**
```powershell
# Run pluginval validation
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_pluginval.ps1
```

### "File Locked" Errors During Deploy

This happens when a DAW has the plugin loaded:

```cmd
# Solution: Close all DAWs, then rebuild
cmake --build build --config Release --parallel
```

### Plugin Loads But No Audio Changes

Check the plugin logger output for `Potential no-op DSP detected`. This indicates:

- The DSP is not modifying audio
- Parameter values may not be reading correctly
- Verify your `plugin.json` schema matches your DSP parameter names

## File Structure

```
C:\Users\salmo\Documents\VST3\AgentVST-Dev\
├── SimpleGain.vst3/
│   └── Contents/
│       └── x86_64-win/
│           └── SimpleGain.vst3               ← Binary (should be 6MB)
├── AgentCompressor.vst3/
│   └── Contents/
│       └── x86_64-win/
│           └── AgentCompressor.vst3
├── AgentEQ.vst3/
│   └── Contents/
│       └── x86_64-win/
│           └── AgentEQ.vst3
└── Escher_Pitch_Shifter.vst3/
    └── Contents/
        └── x86_64-win/
            └── Escher_Pitch_Shifter.vst3
```

## CLI Shortcuts

### Deploy All Plugins (Release)
```bash
cmake --build build --config Release --parallel
```

### Deploy Single Plugin
```bash
cmake --build build --config Release --target SimpleGain
```

### Verify Deployment
```bash
# PowerShell
Get-ChildItem "C:\Users\salmo\Documents\VST3\AgentVST-Dev\*\Contents\x86_64-win" |
  Select-Object @{N="Plugin";E={Split-Path (Split-Path (Split-Path $_.FullName)) -Leaf}},
                @{N="Size (MB)";E={[math]::Round($_.Parent.Parent.FullName | Get-ChildItem -Recurse | Measure-Object -Property Length -Sum | Select -ExpandProperty Sum / 1MB, 1)}},
                @{N="Date";E={(Get-ChildItem $_.FullName).LastWriteTime}}
```

### Reset to Clean Deployment
```bash
# Remove stale plugins
rm -r "C:\Users\salmo\Documents\VST3\AgentVST-Dev\*"

# Rebuild and deploy fresh
cmake --build build --config Release --parallel
```

## Related Documentation

- **Agent DSP API**: See `docs/agent_api.md` for writing DSP code
- **Schema Format**: See `docs/schema_spec.md` for plugin.json format
- **Testing**: See `scripts/README.md` for pluginval validation

---

**Last Updated**: 2026-04-09
**Issue Resolved**: Stale VST3 deployment causing no audio output
