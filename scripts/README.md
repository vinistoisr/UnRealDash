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
part of the runtime command-line surface PLAN 4.7 owns. They are still declared to the resolver,
alongside `-udash-state=`, `-udash-fraction=`, `-udash-shot-at=` and `-udash-sweep=`, so a typo in
one is rejected rather than ignored.

## The documented command-line surface

PLAN 4.7's surface, resolved by `DashPlayerConfig.cpp`. Every flag is also a `player.json` key of
the same name without the leading dash, read from the player's own `Saved` directory, and the
command line wins field by field. A Shipping Android build receives no command line at all, so the
file alone has to be sufficient.

```
-udash=<path>              -connector=<sim|replay|tcp>   -scenario=<name>
-replay-file=<file>        -replay-loop                  -connector-host=  -connector-port=
-screenshot=<path>         -quit-after=<seconds>         -profile=<mobile|desktop>
-freeze-scenario-time      -ack-warnings-at=<t>          -reveal-delay-ms=<n>   -rhi=<vulkan|gles>
```

The last three are accepted and validated but consume nothing yet; the resolver logs which task
will consume each. `-replay-file=` is not `-replay=` because the engine owns that one and hangs the
player with it; see `docs/build/chunk-16-command-line.md` revision 3.

```powershell
pwsh -NoProfile -File scripts/capture-metrics.ps1 -Smoke
```

Note that the schema files do not currently reach the device; see
`docs/reports/2026-09-17-device-package-gate.md`. Until that is fixed the five files in
`packages/dashboard-spec/schema/` have to be pushed by hand, and any results collected that way
are evidence about the loader rather than about packaging.

## Chunk 20 launch and event-log diagnostics

`capture-metrics.ps1 -Launch -Target windows -Player <exe> -Package <package>` launches a
60-second sweep by default. `-Seconds` changes the duration; `-PlayerArguments` replaces the
sweep arguments for another connector. It passes the launch-request QPC reading through
`-udash-launch-counter` and atomically publishes the counter read after `Start-Process` returns
in a unique JSON evidence file. The app records that difference in the header as
`uncertainty.windows_launch_overhead_counter`; divide by the launch row's `counter_frequency`
for seconds. The extra hidden `-udash-launch-evidence` switch identifies that file. A missing
file or counter remains unmeasured, not zero elapsed time.

The hidden `-udash-overlay` switch constructs the debug widget only when requested. Its frame
window is 240 samples, its percentiles use nearest rank, and it refreshes its display four times
per second. Memory comes from the sampler's latest values. Its ring never feeds an event writer.

`validate-event-log.py <log> --survived-kill` requires at least 29 seconds separately in frame,
sampled, receive, acquire, submit and present. `--all-types` requires every known and declared
record type to occur. `--orderly` rejects unresolved expiries. `--launch-required` rejects
missing launch endpoints or measured uncertainty, and reports the launch duration without
gating it against a machine-dependent cold-start target. `--expect-assets=a.png,b.png` checks
exactly one import for each listed package-relative name. These options do not launch a run.

Rule expiries retain the existing expiry columns: kind 1 is hold_last, kind 2 is debounce, and
`signal` carries the numeric rule id for those kinds. The header's `declared_rules` maps each
numeric `id` to its document `name`. Supply `--document <dashboard.json>` when validating such
rows so the classifier can check those names against the document's rules. Every rule arming
must have exactly one status, even without `--lifecycle`. Stage 0 currently loads no threshold
rules, so its declaration list and rule-expiry rows are empty; a synthetic proof must supply both
the declaration mapping and document.

Android sources and launch fields are implemented but have not been device-verified. Thermal
status from the NDK is a severity rather than Celsius, so temperature uses the first readable
CPU/GPU sysfs zone and names it in the header. Process start comes from `/proc/self/stat` field
22 and `_SC_CLK_TCK`. A startup boot-clock offset maps the first presentation onto boot time
without a file or OS call in the frame path; this assumes no suspend before first usable and
is stated in the uncertainty text. The optional evidence JSON accepts `am_start_output`,
`am_launch_state` and `observed_spread_ns`. Those auxiliary values must come from the device
harness; this chunk does not run `am start -W`, and absent evidence stays null. The Windows
launch mode rejects `-Target device` rather than claiming to capture that evidence.
