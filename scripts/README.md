# AgentVST Build & Deployment Scripts

## ⚠️  IMPORTANT: Always Use Release Builds for DAW Testing

Debug builds are large (~19MB) and unoptimized. **Always use Release mode** when deploying to DAWs like Ableton Live.

```bash
# ✅ CORRECT - Release build
cmake --build build --config Release --parallel

# ❌ WRONG - Debug build (don't use for DAW)
cmake --build build --config Debug
```

See `DEPLOYMENT.md` for complete guide.

## Python Tooling

These scripts provide incubation-layer helpers for schema validation and scaffold generation.

### Install

```bash
pip install -r scripts/requirements.txt
```

### Validate Specs

```bash
python scripts/validate_schema.py
python scripts/validate_schema.py schema/example_plugin.json
python scripts/validate_schema.py examples/
```

### Generate a Plugin Scaffold

```bash
python scripts/generate.py --spec schema/example_plugin.json --out generated
```

### Smoke Test

```bash
python scripts/smoke_test_generate.py
```

## Build & Deployment Scripts

### Build and Deploy Plugins (PowerShell)

Default: Release build (RECOMMENDED)

```powershell
.\build_and_deploy_vst3.ps1
```

Explicitly specify Release:

```powershell
.\build_and_deploy_vst3.ps1 -Config Release
```

Use Debug (not recommended, requires confirmation):

```powershell
.\build_and_deploy_vst3.ps1 -Config Debug -Force
```

### Verify Deployment Status

Check if deployed plugins are fresh Release builds or stale Debug builds:

```powershell
.\verify_deployment.ps1
```

Output shows:
- ✅ Fresh Release builds (6MB, recent date)
- ⚠️ Debug builds (19MB, should convert to Release)
- ❌ Stale binaries (older than 24 hours, rebuild needed)

If any issues found, follow the recommendations in output.

### Run pluginval Validation

Validate all deployed VST3s using the pluginval plugin validator:

```powershell
# Uses default pluginval path: C:\Users\salmo\Documents\pluginval.exe
.\run_pluginval.ps1

# Custom pluginval path:
.\run_pluginval.ps1 -PluginValPath "C:\path\to\pluginval.exe"
```

## Troubleshooting Unchanged Audio

**⚠️ MOST COMMON CAUSE: Stale or Debug deployment**

1. **Verify deployment** is using fresh Release builds:
   ```powershell
   .\verify_deployment.ps1
   ```

2. **If stale/debug detected**, rebuild fresh:
   ```bash
   cmake --build build --config Release --parallel
   ```

3. **Restart your DAW** (Ableton caches plugins)

4. **Then check plugin logs** for `Potential no-op DSP detected`:
   - Verify your DSP modifies input under active settings
   - Check that parameter names in your DSP match `plugin.json` schema
   - Ensure bypass/mix parameters default to active state (not bypassed)

See `DEPLOYMENT.md` for complete troubleshooting guide.
