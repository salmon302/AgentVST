# AgentVST Quick Start: Build & Deploy

## One-Command Deploy (Recommended)

```cmd
_build_release_and_deploy.cmd
```

This builds all plugins in Release mode and deploys to `I:\Documents\Ableton\VST3\dev`.

## Step-by-Step Manual Process

### 1. Configure CMake for Ableton deployment

```cmd
_configure_with_ableton_vst3_deploy.cmd
```

This sets up the deployment directory and CMake configuration.

### 2. Build plugins (Release mode only!)

```bash
# Build all plugins
cmake --build build --config Release --parallel

# OR build a single plugin
cmake --build build --config Release --target SimpleGain
```

### 3. Verify deployment

```powershell
cd scripts
.\verify_deployment.ps1
```

Should show:
```
✅ Fresh Release:  4 plugins
⚠️  Debug Build:   0 plugins
❌ Stale Binary:   0 plugins
```

### 4. Test in DAW

1. **Close Ableton Live** completely
2. **Reopen Ableton Live**
3. **Go to Settings > Files and Folders > VST3 Extensions**
4. **Click "Rescan" button**
5. **Load a plugin** (e.g., SimpleGain on an audio track)
6. **Test**: Set "Gain" parameter to +6dB → you should hear volume increase

## Key Rules

✅ **DO**

- Always use **Release builds** for DAW testing
- **Restart the DAW** after deploying new plugins
- **Verify deployment** is fresh before testing

❌ **DON'T**

- Don't use Debug builds (too large, too slow)
- Don't skip restarting the DAW
- Don't deploy while DAW has the plugin loaded

## Troubleshooting

**Q: Plugin doesn't process audio**

A: Check deployment freshness:
```powershell
cd scripts
.\verify_deployment.ps1
```

If stale/debug is detected, run `_build_release_and_deploy.cmd` again.

**Q: "File is locked" error**

A: Close all DAWs, then rebuild:
```bash
cmake --build build --config Release --parallel
```

**Q: Plugin loads but parameter changes have no effect**

A: Check plugin logs for `Potential no-op DSP detected`. This means:
- Your DSP code isn't modifying audio under current settings
- Parameter names in `plugin.json` don't match your DSP code
- Bypass/mix parameters are set to passive state

See `DEPLOYMENT.md` troubleshooting section for details.

## Files

- **DEPLOYMENT.md** - Complete deployment guide & troubleshooting
- **scripts/verify_deployment.ps1** - Check deployment status
- **scripts/build_and_deploy_vst3.ps1** - Manual build script
- **_build_release_and_deploy.cmd** - Quick all-in-one deploy
- **_configure_with_ableton_vst3_deploy.cmd** - One-time setup

## For Development

See `docs/agent_api.md` for writing DSP code and `docs/schema_spec.md` for plugin configuration.
