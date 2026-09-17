# Repository scripts

Toolchain checks, local build and packaging entry points, shared script logic and
Pester tests belong here (PLAN.md 0.7 and 1.6). Runtime code and compiled outputs do not.

Run doctor.ps1 with workstation, android, linux or device as -Profile. Use
-PreInstall for capacity and installer prerequisites only. pins.json owns versions,
detection metadata and profile membership. Its engine.root is a provisional install
location; the owner must record the actual location after installation. The device
address remains a placeholder until task 0.8.

Doctor -SearchPath replaces command discovery PATH; -PinsFile substitutes the data
file. Missing or inaccessible tools fail with an explanation. Symbolic links found
on that PATH are resolved to their existing target before execution.

Build and packaging scripts call doctor before -WhatIf prints a RunUAT command.
-DoctorScript substitutes the doctor for tests. Without -WhatIf, the skeletons
stop without building, deploying or capturing. Later tasks implement those actions.
Configuration is Development. Deployment and capture currently preview the package
command for their target; they do not yet contain device or metrics operations.

Run `pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI"`.
The installed Pester 6.2 module supports the Pester 5 APIs used by these tests.

Package-loader fixture gate (PLAN 4.4), after building the editor:

```powershell
& '<Engine>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
    runtime/UnRealDash/UnRealDash.uproject -run=DashPackageTest `
    "-PackageFixtures=$((Resolve-Path tests/fixtures/packages).Path)" -unattended -nullrhi
```

The gate derives expected acceptance, rejection, profile and archive-only counts
from `cases.json`. It calls the public engine loader for every archive and every
meaningful directory, compares verdict/code/pointer, and constructs each accepted
placeholder tree. It also checks rejection text fields and unknown registrations.
It does not prove on-screen readability. Launch each archive with `-udash=<path>`
for that gate, plus `well-formed/` for the directory rendering gate. The package
HUD is now the default; the smoke spike remains selectable with
`/Game/Smoke/L_Smoke?game=/Script/UnRealDash.SmokeGameMode` as an explicit map URL. No new flag surface or
`player.json` behavior was added by this task.

## Device half of the PLAN 4.4 package gate

The desktop half is a commandlet and needs the editor, so it cannot run on Android. On device the
packaged player is driven instead, and `ADashPackageHUD` emits one machine-readable line per
package:

```
DashVerdict accepted=<true|false> code=<CodeName> pointer=<json pointer> path=<path>
```

Batch mode is the one to use. `-udash-batch=<dir>` loads every fixture in a single run and
`-udash-profile=<mobile|desktop>` selects the texture budget, so the whole gate is two launches
rather than one cold start per fixture, which on the Pixel costs about 85 seconds each:

```powershell
pwsh -NoProfile -File scripts/run-device-package-gate.ps1 -Batch
```

The player reports a verdict for every fixture it finds, including directory forms of the
archive-only cases. The runner decides what was expected, reading `cases.json` at runtime, and
ignores the extra verdicts rather than counting them as coverage. The game module carries no test
expectations.

Both switches are gate switches, like the smoke commandlet's `-stress`, and are deliberately not
part of the runtime command-line surface PLAN 4.7 owns.

Note that the schema files do not currently reach the device; see
`docs/reports/2026-09-17-device-package-gate.md`. Until that is fixed the five files in
`packages/dashboard-spec/schema/` have to be pushed by hand, and any results collected that way
are evidence about the loader rather than about packaging.
