# AgentVST Troubleshooting Guide

## Issue: Plugins Load But No Audio Output / Parameters Don't Affect Sound

### Root Cause: Stale or Debug Deployment

This is the #1 reason AgentVST plugins don't produce audio. The deployed `.vst3` files are outdated or built in Debug mode.

### Quick Fix

```powershell
# 1. Verify deployment status
cd scripts
.\verify_deployment.ps1

# 2. If it shows stale/debug, rebuild and deploy:
# (from project root)
cmake --build build --config Release --parallel

# 3. Close and reopen your DAW
# (Ableton must be fully restarted to reload plugins)

# 4. Rescan VST3 plugins in DAW settings

# 5. Reload and test the plugin
```

### Why This Happens

| Symptom | Cause | Fix |
|---------|-------|-----|
| Binary is 19MB | Debug build deployed | Use `--config Release` |
| Binary is old (>24h) | Last build was days ago | Rebuild: `cmake --build build --config Release` |
| DAW still uses old version | Plugin cache not cleared | Restart DAW completely |
| File copy fails during build | DAW has plugin loaded | Close DAW, retry build |

### Prevention

**Always use Release builds for DAW:**

```bash
# ✅ CORRECT
cmake --build build --config Release --parallel

# ❌ WRONG - Debug builds are NOT for DAW testing
cmake --build build --config Debug
```

**Use the convenience script:**

```cmd
_build_release_and_deploy.cmd
```

This automates everything and ensures Release mode.

---

## Issue: Plugins Load, Audio Passes Through, But Parameters Have No Effect

### Root Cause: DSP Not Modifying Audio

The plugin loaded successfully, but the audio processing isn't working.

### Check Plugin Log Output

Look for this message in the plugin UI (if visible) or in DAW console:

```
[WARNING] AgentVST: Potential no-op DSP detected
(relative diff energy=1e-10, consecutive blocks=64)
```

This means the DSP is returning input unchanged.

### Debugging Steps

1. **Verify SimpleGain works** (simplest test plugin)
   - Load SimpleGain
   - Leave Bypass OFF
   - Set Gain to +6dB
   - Play audio - volume should increase ✅
   - If no change: your deployment is stale (see section above)

2. **Check your DSP logic**
   ```cpp
   // GainDSP.cpp
   float processSample(int channel, float input, const AgentVST::DSPContext& ctx) override {
       float bypass = ctx.getParameter("bypass");
       if (bypass >= 0.5f)
           return input;  // ← When bypass=ON, unmodified

       float gainDb = ctx.getParameter("gain_db");
       float gainLin = std::pow(10.0f, gainDb / 20.0f);
       return input * gainLin;  // ← When bypass=OFF, apply gain
   }
   ```
   Make sure your DSP actually modifies the signal!

3. **Verify parameter names match schema**
   ```json
   // plugin.json
   "parameters": [
       {
           "id": "gain_db",  // ← Name must match DSP
           "name": "Gain",
           ...
       }
   ]
   ```

   In DSP:
   ```cpp
   float gainDb = ctx.getParameter("gain_db");  // ← Must match plugin.json
   ```

4. **Check parameter defaults**
   - If "bypass" defaults to `true`, plugin will always be bypassed
   - If "wet" parameter defaults to 0, effect will be silent
   - Verify defaults in `plugin.json` enable the effect

### Example: Correct SimpleGain Schema

```json
{
  "parameters": [
    {
      "id": "gain_db",
      "name": "Gain",
      "type": "float",
      "min": -60.0,
      "max": 12.0,
      "default": 0.0,
      "unit": "dB"
    },
    {
      "id": "bypass",
      "name": "Bypass",
      "type": "boolean",
      "default": false      ← OFF by default (NOT bypassed)
    }
  ]
}
```

---

## Issue: Build Fails / Plugin Won't Compile

### Check Visual Studio & CMake Installation

```bash
# Test Visual Studio Build Tools
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
# Should not error

# Test CMake
cmake --version
# Should show 3.22+
```

### Clean Rebuild

```bash
# Remove build directory
rm -r build

# Reconfigure
cmake -S . -B build -DAGENTVST_VST3_DEPLOY_DIR="C:\Users\salmo\Documents\VST3\AgentVST-Dev"

# Rebuild
cmake --build build --config Release --parallel
```

---

## Issue: DAW Won't Recognize VST3 Plugins

### Verify Deployment Location

```powershell
# Check if plugins exist
dir "C:\Users\salmo\Documents\VST3\AgentVST-Dev"

# Should show:
# - SimpleGain.vst3/
# - AgentCompressor.vst3/
# - AgentEQ.vst3/
# - Escher_Pitch_Shifter.vst3/
```

### Verify VST3 Binary Structure

```powershell
# Each plugin should have this structure:
dir "C:\Users\salmo\Documents\VST3\AgentVST-Dev\SimpleGain.vst3\Contents\x86_64-win\"

# Should show:
# SimpleGain.vst3 (the binary file)
```

### Force DAW to Rescan

**Ableton Live:**
1. Go to Settings → Files and Folders
2. Click "VST3 Extensions" folder location
3. Verify it shows your deployment directory
4. Click "Rescan" button
5. Wait for scan to complete
6. Restart Ableton completely

---

## Issue: "File Locked" Error During Build

```
[ERROR] Failed to remove existing deployed bundle '...'.
Close any DAW/host that may be locking the plugin and try again.
```

### Solution

1. **Close all DAWs** (Ableton, etc.)
2. **Wait 5 seconds** (for OS to release file locks)
3. **Retry build:**
   ```bash
   cmake --build build --config Release --parallel
   ```

---

## Checking Plugin Processing Status

### Use pluginval to Validate

```powershell
cd scripts
.\run_pluginval.ps1
```

This runs automatic plugin validation and reports:
- Audio I/O functionality ✅/❌
- Parameter automation ✅/❌
- Crash resistance ✅/❌

### Read Plugin Logs (if available in UI)

Some AgentVST builds include a web-based UI that displays:
- Input/output meters (shows audio is being processed)
- Parameter values (shows DSP is reading parameters)
- Watchdog violations (shows if processing is too slow)
- No-op detection (shows if DSP output equals input)

---

## Common Messages Explained

| Message | Meaning | Action |
|---------|---------|--------|
| `Potential no-op DSP detected` | Output doesn't differ from input | Fix DSP logic or parameter defaults |
| `Watchdog violation` | DSP processing took too long | Optimize code or reduce block size |
| `Parameter not found: xyz` | Parameter ID missing from schema | Add to `plugin.json` |
| `Schema parse error` | `plugin.json` is malformed JSON | Validate JSON syntax |

---

## Still Not Working?

1. Run `scripts/verify_deployment.ps1` to check binary freshness
2. Check `DEPLOYMENT.md` for detailed troubleshooting
3. See `docs/agent_api.md` to verify DSP implementation
4. See `docs/schema_spec.md` to verify plugin.json format
5. Run `scripts/run_pluginval.ps1` for automated validation
