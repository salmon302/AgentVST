Title: impl-devils-grid-irrational-delay
Date: 2026-04-18T10:00:00Z
Author: GitHub Copilot (Gemini 3.1 Pro (Preview) Agent)
Contribution Type: Implementation
Ticket/Context: Suite 1: The "Devil's Grid" Irrational Delay
Summary: Implement core features for Devil's Grid Irrational Delay

Task Reference:
[VSTS_SRS.md](../../VSTS_SRS.md#L19) - The "Devil's Grid" Irrational Delay

Specification Summary:
- Multiply host-synced delay time by irrational constants: sqrt(2), phi, pi.
- Use Hermite/Lagrange fractional delay interpolation (currently using linear).

Implementation Notes:
- Updated `schema/devils_grid_irrational_delay.json` to broaden irrational options to `x_sqrt2`, `x_phi`, `x_pi`.
- Modified `examples/devils_grid/Devils_Grid_Irrational_Delay/src/Devils_Grid_Irrational_DelayDSP.cpp`:
  - Added constants for Phi and Pi.
  - Mapped enum indices to corresponding constants.
  - Implemented 4-point Hermite interpolation replacing linear interpolation.
