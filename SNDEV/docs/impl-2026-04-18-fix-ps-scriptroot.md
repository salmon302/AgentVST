Title: impl-2026-04-18-fix-ps-scriptroot
Date: 2026-04-18T14:55:00Z
Author: Seth Nenninger (Gemini 3 Flash (Preview) Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Fix $PSScriptRoot being empty in certain contexts

Task Reference:
User reported error: `Join-Path : Cannot bind argument to parameter 'Path' because it is an empty string` at `...$PSScriptRoot ".."`.

Specification Summary:
Ensure PowerShell scripts can resolve their workspace root even when `$PSScriptRoot` is null (e.g., when executed via certain interactive methods or older runtimes).

Implementation Notes:
- Updated `scripts/build_and_deploy_vst3.ps1`: Moved `$WorkspaceFolder` logic into a conditional check that uses `$PWD` if `$PSScriptRoot` is empty.
- Updated `scripts/run_pluginval.ps1`: Applied same safety logic for `$WorkspaceRoot`.
- These changes allow the scripts to run correctly whether launched via `node-terminal`, interactive shell, or standard script execution.

Evidence links:
- Script: [scripts/build_and_deploy_vst3.ps1](scripts/build_and_deploy_vst3.ps1)
- Script: [scripts/run_pluginval.ps1](scripts/run_pluginval.ps1)
