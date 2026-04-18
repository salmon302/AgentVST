Title: impl-2026-04-18-launch-json
Date: 2026-04-18T14:40:00Z
Author: Seth Nenninger (Gemini 3 Flash (Preview) Agent)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Create launch.json for building and pushing VSTs

Task Reference:
User request: "create a launch.json for building & pushing vsts"

Specification Summary:
Provide a VS Code launch configuration that allows developers to run build and deployment scripts (PowerShell) directly from the Run and Debug view.

Implementation Notes:
- Created `.vscode/launch.json` using `type: node-terminal` to ensure compatibility regardless of the PowerShell extension's debug adapter state.
- Created `.vscode/tasks.json` to provide alternative access via the Tasks menu.
- Added configurations for `Build & Push VST3 (Release)`
- Added configurations for `Build & Push VST3 (Debug)` with `-Force` to bypass warnings
- Added configuration for `Run Pluginval (Validation)`
- Added configuration for `Verify Deployment`

Evidence links:
- File: [.vscode/launch.json](.vscode/launch.json)
- File: [.vscode/tasks.json](.vscode/tasks.json)
- Script: [scripts/build_and_deploy_vst3.ps1](scripts/build_and_deploy_vst3.ps1)
