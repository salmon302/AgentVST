Title: impl-2026-04-17-update-vst3-deploy-path
Date: 2026-04-17T14:45:00Z
Author: Seth Nenninger
Agent: Gemini 3 Flash (Preview)
Contribution Type: Implementation
Ticket/Context: ad-hoc
Summary: Update VST3 deployment directory to I:\Documents\Ableton\VST3\dev

Task Reference:
User request to "Ensure we use I:\Documents\Ableton\VST3\dev" for development.

Specification Summary:
Change all hardcoded and default VST3 deployment paths from C:\Users\salmo\Documents\VST3\AgentVST-Dev to I:\Documents\Ableton\VST3\dev across scripts and documentation.

Implementation Notes:
- Updated [_configure_with_ableton_vst3_deploy.cmd](_configure_with_ableton_vst3_deploy.cmd)
- Updated [DEPLOYMENT.md](DEPLOYMENT.md)
- Updated [QUICK_START.md](QUICK_START.md)
- Updated [scripts/verify_deployment.ps1](scripts/verify_deployment.ps1)

Verification Steps:
- Verified that the new path is correctly reflected in the configuration scripts.
- Documentation now points to the correct SSD-based VST3 folder for Ableton Live.
- Deployment verification script now defaults to the correct path.
