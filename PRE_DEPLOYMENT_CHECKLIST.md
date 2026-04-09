# Pre-Deployment Checklist

Use this checklist before testing plugins in a DAW to ensure everything is set up correctly.

## Configuration

- [ ] Visual Studio Build Tools or Visual Studio installed
- [ ] CMake 3.22+ installed
- [ ] Ableton Live (or other DAW) installed
- [ ] Internet connection (first build downloads JUCE, nlohmann/json, Catch2)

## Build Preparation

- [ ] Run `_configure_with_ableton_vst3_deploy.cmd` (one-time setup)
- [ ] Deployment directory configured: `C:\Users\salmo\Documents\VST3\AgentVST-Dev`
- [ ] All DAWs closed (important: file locking can prevent deployment)

## Build & Deploy

- [ ] Build Release configuration: `cmake --build build --config Release --parallel`
  - ❌ NOT Debug: `cmake --build build --config Debug`
  - ❌ NOT without `--config Release` flag
- [ ] Build completes without errors
- [ ] All 4 plugins built:
  - [ ] SimpleGain
  - [ ] AgentCompressor
  - [ ] AgentEQ
  - [ ] Escher_Pitch_Shifter

## Deployment Verification

Run verification script:

```powershell
cd scripts
.\verify_deployment.ps1
```

Check output shows:
- [ ] All plugins listed
- [ ] ✅ Fresh Release (0 or 4 plugins)
- [ ] ⚠️ Debug Build: 0 plugins
- [ ] ❌ Stale Binary: 0 plugins
- [ ] All file sizes are ~6MB (NOT 19MB)
- [ ] All timestamps are recent (today)

## DAW Setup

For **Ableton Live:**

- [ ] Ableton completely closed
- [ ] Reopen Ableton
- [ ] Go to Settings → Files and Folders
- [ ] Locate "VST3 Extensions" folder path
- [ ] Click "Rescan" button
- [ ] Wait for scan to complete
- [ ] Confirm plugins appear in browser

## First Test

- [ ] Load **SimpleGain** on an audio track
  - This is the simplest plugin (minimal DSP complexity)
- [ ] Load an audio file or enable audio input
- [ ] Play audio - should hear dry signal
- [ ] Set "Bypass" to OFF (if not already)
- [ ] Set "Gain" to +6dB
- [ ] Play audio - should hear volume increase ✅

**If audio doesn't change:**
1. Check deployment is fresh (run verify script)
2. Verify `bypass` default is `false` in `plugin.json`
3. Check plugin logs for "Potential no-op DSP detected"
4. See TROUBLESHOOTING.md

## Secondary Tests

After SimpleGain works:

- [ ] **AgentCompressor**: Set threshold -30dB, ratio 4:1 → should hear compression
- [ ] **AgentEQ**: Boost 3kHz band → should hear frequency emphasis
- [ ] **Escher_Pitch_Shifter**: Enable effect → should hear pitch shifting

## After Development Change

If you modify DSP code or plugin.json:

- [ ] Close DAW
- [ ] Rebuild: `cmake --build build --config Release --parallel`
- [ ] Verify: `cd scripts && .\verify_deployment.ps1`
- [ ] Reopen DAW
- [ ] Rescan VST3 plugins
- [ ] Reload plugin and test

## If Something Still Doesn't Work

- [ ] Check TROUBLESHOOTING.md
- [ ] Check DEPLOYMENT.md troubleshooting section
- [ ] Run `scripts/run_pluginval.ps1` for plugin validation
- [ ] Read JUCE output for CMake/build errors
- [ ] Check plugin UI for error logs (if available)

## Quick Reference

| Problem | Check |
|---------|-------|
| No audio output | Release build? Fresh deployment? DAW restarted? |
| Parameters don't work | DSP logic? Parameter names match schema? Defaults correct? |
| File locked error | All DAWs closed? |
| Plugin not discovered | RescanVST3? Correct deployment folder? |
| Build fails | VS Build Tools installed? CMake 3.22+? |

---

**Estimated Time**: 5-10 minutes for full setup + test

**Success Criteria**: SimpleGain volume changes are audible when Gain parameter changes
