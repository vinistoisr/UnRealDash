# Build chunk 01: foundation

Status: frozen spec for a Codex build session. Covers PLAN.md tasks 0.2, 0.7, 1.1, 1.2, 1.3 and 1.6. Read PLAN.md (those tasks, the Approach preamble, the pin table, and the Sequencing block) and docs/ARCHITECTURE.md before writing anything. PLAN.md is the authority on what; this file adds the exact proof commands and a few implementation constraints. If the two disagree, PLAN.md wins and the disagreement goes in the report.

## Goal

A repository skeleton that every later chunk builds inside, a doctor script that tells any contributor exactly what their machine is missing per target, and build-script skeletons that refuse to run when the doctor fails. Nothing in this chunk compiles code. Everything in it is testable today on a machine with no engine installed.

## Deliverables

1. **Repository layout** (task 1.1): `packages/signal-core/`, `packages/dashboard-spec/`, `runtime/UnRealDash/`, `connectors/`, `tools/`, `tests/fixtures/`, `examples/`, `docs/reports/`, `scripts/`, each with a `README.md` that states what belongs there and what does not, in plain factual language. No empty placeholder source files. Also add at the repo root: `.editorconfig`, `.clang-format` (LLVM base, 4-space indent, 120 columns, C++20), `.clang-tidy` (a conservative default set: bugprone-*, performance-*, readability-identifier-naming off), and a short `CONTRIBUTING.md` that points at PLAN.md, docs/ARCHITECTURE.md and the doctor script.
2. **Git LFS tracking** (task 1.2): `.gitattributes` tracking `*.uasset`, `*.umap`, `*.png`, `*.jpg`, `*.fbx`, `*.wav`, `*.ttf`, `*.exr`, `*.tga` through LFS, plus text normalization (`* text=auto eol=lf`, with `*.ps1` and `*.cmd` as `eol=crlf`). Generate one 16x16 PNG at `tests/fixtures/images/lfs-probe.png` with Python (stdlib `zlib` and `struct`, no Pillow) so the LFS gate has something to check. Record the account's current LFS storage and bandwidth allowance in `packages/dashboard-spec/README.md` as "to be read from the GitHub billing page by the owner; not yet recorded", since you cannot read it.
3. **Licences** (task 1.3): `LICENSE` with the MIT text, copyright year 2026, holder "Vincent Royer", and a first line marking it provisional; `LICENSES-ASSETS.md` stating CC-BY-4.0 for original example assets, provisional; a `README.md` section "Licensing" that links both files and states that engine and third-party assets remain under their own terms.
4. **Pin table as data**: `scripts/pins.json` holding every row of the PLAN.md pin table (component, expected version or minimum, how it is detected, which profiles include it). `doctor.ps1` and the Pester tests read this file. The pin table is not duplicated in code.
5. **Doctor** (task 0.7): `scripts/doctor.ps1` with `-Profile workstation|android|linux|device` and a `-PreInstall` switch, exactly as PLAN.md task 0.7 specifies. Row sets per profile come from `pins.json`. Output is a table containing only the requested profile's rows, with columns Component, Expected, Found, Status. Exit code 0 when every row in the profile passes, 1 otherwise. `-PreInstall` checks free capacity on C: against 150 GB and the prerequisites for installing (git, winget, PowerShell version) and nothing that the installs themselves provide. Test seams, both documented in the script header: `-SearchPath <string>` replaces the PATH the script searches for command-line tools, and `-PinsFile <path>` replaces `pins.json`. The device profile reads the head unit's ADB address from `pins.json` (`device.adb_address`, placeholder value `192.168.0.0:5555` until the owner records it in 0.8).
6. **Script skeletons** (task 1.6): `scripts/build.ps1 -Target win64|android|linux`, `package-windows.ps1`, `package-android.ps1`, `package-linux.ps1`, `deploy-deck.ps1`, `capture-metrics.ps1 -Target windows|device|soak`. Each calls the doctor first with the profile PLAN.md 1.6 assigns, prints which profile it chose, and exits non-zero without doing anything else when the doctor fails. Each supports `-WhatIf`, which prints the exact `RunUAT` command line it would execute (engine root from `pins.json` `engine.root`, project file `runtime/UnRealDash/UnRealDash.uproject`, platform, configuration) and exits 0. Test seam: `-DoctorScript <path>` replaces `scripts/doctor.ps1`, so tests can substitute a doctor that always passes or always fails. Shared logic (invoking the doctor, composing the UAT command, printing) lives in one dot-sourced `scripts/lib/common.ps1`; the six scripts are thin.
7. **Tests**: Pester 5 tests under `scripts/tests/` covering: each profile's row set equals the `pins.json` row set for that profile; `-SearchPath` without cmake fails the workstation profile and names cmake; `-PreInstall` passes on this machine; each skeleton script with a passing fake doctor prints a UAT line under `-WhatIf` and exits 0; each with a failing fake doctor exits non-zero and prints no UAT line; `capture-metrics.ps1 -Target windows` chooses `workstation` and `-Target device` chooses `device`. If Pester 5 is not installed, install it with `Install-Module Pester -Scope CurrentUser -Force -MinimumVersion 5.0` and say so in the report.
8. **Small tools** (task 0.2): confirm `git lfs`, `cmake`, `ninja` resolve. `ninja` is already installed. If `cmake` is not on PATH, install it with `winget install --id Kitware.CMake -e --scope user`; if that fails, `python -m pip install --user cmake` and report the deviation. Do not attempt anything that needs elevation.

## Constraints

- PowerShell scripts must run under `pwsh` 7 and must not use syntax that breaks Windows PowerShell 5.1 parsing (no `?:` ternary, no `??`), so a 5.1 user gets a clear error rather than a parse failure.
- Plain factual language in every file. No em dashes anywhere. No marketing words. Do not name any commercial dashboard product.
- Follow docs/ARCHITECTURE.md. READMEs say what belongs in a directory and what does not.
- Do not create source files for later chunks. Do not create `.github/` workflows; that is chunk 04.
- Do not run `git commit`, `git push`, or change git configuration. Claude commits.
- Do not modify PLAN.md, PLAN-REVIEW-LOG.md, or anything under docs/ except docs/reports/README.md.

## Non-goals

Anything compiled. CI workflows. The Unreal project. Signal-core. Schemas. The device address (placeholder only).

## Proof (run all of these and paste full output)

```
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -File scripts/doctor.ps1 -Profile android; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -File scripts/doctor.ps1 -Profile linux; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -File scripts/doctor.ps1 -Profile device; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -File scripts/doctor.ps1 -PreInstall; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI"
pwsh -NoProfile -File scripts/build.ps1 -Target win64 -WhatIf -DoctorScript scripts/tests/fakes/doctor-pass.ps1; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -File scripts/build.ps1 -Target win64 -WhatIf -DoctorScript scripts/tests/fakes/doctor-fail.ps1; echo "exit=$LASTEXITCODE"
git lfs track
python - <<EOF
import struct,zlib; print(open('tests/fixtures/images/lfs-probe.png','rb').read(8)==b'\x89PNG\r\n\x1a\n')
EOF
```

Expected today, on a machine with no engine and no Visual Studio IDE: the four profile runs exit 1 and list the missing rows (Visual Studio 2022 IDE, engine, and for android/linux/device their own rows) while showing git lfs, cmake and ninja as found; `-PreInstall` exits 0; Pester reports zero failures and a non-zero test count; the passing-fake `-WhatIf` prints a `RunUAT` line and exits 0; the failing-fake run exits 1 with no `RunUAT` line.

## Report format

End with: files added or changed (one line each: path, what, which PLAN.md task), the proof output verbatim, any deviation from PLAN.md or this spec with the reason, and anything you could not do.
