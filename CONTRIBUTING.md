# Contributing

Read [PLAN.md](PLAN.md) for task scope and gates, and
[the architecture rules](docs/ARCHITECTURE.md) for dependency and module boundaries.
Run `pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation` from the repository root.
Use the android, linux, or device profile for work on that target. Before installing
the toolchain, run `pwsh -NoProfile -File scripts/doctor.ps1 -PreInstall`.

Run `pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI"` for foundation tests.
Scripts require PowerShell 7. Keep changes within a PLAN.md task and record gate evidence.
Do not commit generated builds or captures. Binary assets use Git LFS.
