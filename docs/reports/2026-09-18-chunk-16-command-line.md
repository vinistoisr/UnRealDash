# Chunk 16: the command-line surface (PLAN 4.7)

**PLAN 4.7 is complete**, with two deviations recorded below. The player now has one documented
surface of fourteen flags, every one of them also a `player.json` key of the same name, resolved
command line over file field by field. Four chunks of ad hoc gate switches are gone from it.

Three findings came out of running the surface rather than reading it, and one of them is a bug
that has been in the tree since chunk 11.

## The gate, in one run

`scripts/capture-metrics.ps1 -Smoke`, which runs `scripts/run-flag-gate.ps1`:

```
every documented flag, set from the command line:
  -udash=C:\Users\Vincent\UnRealDash\scripts\..\tests\fixtures\packages\dials-stage0.udash source=command line
  -scenario=acceleration source=command line
  -replay-file=C:/none.bin source=command line
  -connector=sim source=command line
  -connector-host=127.0.0.2 source=command line
  -connector-port=35001 source=command line
  -screenshot=C:\Users\Vincent\UnRealDash\scripts\..\.tmp\flag-gate\flags.png source=command line
  -quit-after=3 source=command line
  -profile=desktop source=command line
  -ack-warnings-at=1.5 source=command line
  -reveal-delay-ms=250 source=command line
  -rhi=vulkan source=command line
  -replay-loop=true source=command line
  -freeze-scenario-time=true source=command line

an unknown flag in this project namespace:
  rejected, exit 2, and the supported list was printed

an unknown player.json key:
  rejected, exit 2

the command line wins field by field:
  udash=C:\Users\Vincent\UnRealDash\scripts\..\tests\fixtures\packages\dials-stage0.udash source=command line
  and the other three still came from player.json: idle, 10.0.0.1, 4242

a missing player.json still runs:
  ran with the command line and the defaults

an invalid field does not:
  rejected, exit 2

the same run reproduces from the command line and from player.json:
  PASS worst=0 moved=0.000000 (0 of 230400 pixels)  limits worst<=48 moved<=0.001

Flag gate: 0 failures
```

The per-field `source=` is the load-bearing part, not decoration. A flag that parsed but did not
take effect reads as `source=default`, and the gate asserts the source of every flag it sets. That
is what turns "did my flag do anything" into something checked.

## PLAN 4.7's criteria

| # | criterion | result |
| --- | --- | --- |
| 1 | every existing gate still passes on the renamed flags | capture, dial, live, connector and the package commandlet, all 0 failures |
| 2 | each documented flag has a test, printing what it set and where it came from | 14 of 14, above |
| 3 | an unknown flag in this project's namespace exits non-zero with the supported list | exit 2, list printed |
| 4 | an unknown `player.json` key does the same | exit 2, key named |
| 5 | the same run reproduces from the command line and from the file | `worst=0 moved=0.000000` |
| 6 | the command line wins field by field | one field overridden, three kept from the file |
| 7 | a missing `player.json` is runnable, an invalid field is not | both |
| 8 | both CMake suites green with no drop | dashboard-spec 30 / 1,002,201 unchanged; signal-core 155 / 112,946, up from 148 / 112,876 |
| 9 | `doctor.ps1 -Profile workstation` exits 0, Pester passes with `-CI` | doctor 0, Pester 76 passed 0 failed |
| 10 | UBT builds Win64 and Android ARM64 | both Succeeded |

## Files, by PLAN task

| path | task |
| --- | --- |
| `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPlayerConfig.{h,cpp}` | 4.7, the resolver |
| `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPlayerConfigTestCommandlet.{h,cpp}` | 4.7, 50 assertions with no engine launch |
| `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPackageScreen.{h,cpp}` | 4.7, `StartConnector` dispatch, `RejectRun`, `-udash-sweep` |
| `runtime/UnRealDash/Source/SignalCore/{Public/SignalCore,Private}/FileTransport.{h,cpp}` | 4.7, `-connector=replay` |
| `packages/signal-core/tests/test_file_transport.cpp` | 4.7, 7 cases over that transport |
| `runtime/UnRealDash/Source/UnRealDashCore/.../DashScenario.{h,cpp}` | 4.7, `GenerateNamedScenario` for `-scenario=<name>` |
| `runtime/UnRealDash/Source/UnRealDashCore/.../DashAcquisition.{h,cpp}` | 4.7, replay transport selection and `-freeze-scenario-time` |
| `scripts/run-flag-gate.ps1`, `scripts/capture-metrics.ps1` | 4.7, the `-Smoke` gate |
| `scripts/run-{capture,dial,live,connector}-gate.ps1` | 4.7, moved to the documented flag names |
| `scripts/README.md`, `docs/build/chunk-16-command-line.md` | 4.7 |

## The switches, before and after

| was | now |
| --- | --- |
| `-udash-shot=` | `-screenshot=` |
| `-udash-quit-after=` | `-quit-after=` |
| `-udash-profile=` | `-profile=` for the player; the batch switch of the same name stays gate-only |
| `-udash-scenario=<seconds>` | split: `-scenario=<name>` is a real `signal_core` scenario, `-udash-sweep=<seconds>` is the gate-only document-derived sweep |
| `-udash-state=`, `-udash-fraction=`, `-udash-shot-at=`, `-udash-batch=` | unchanged, gate-only |

The gate-only switches are declared to the resolver rather than merely tolerated, so a typo in one
is rejected instead of ignored. `-udash-quit-after=` and `-udash-shot=` are dropped outright, so a
stale script still passing one fails rather than quietly doing nothing.

## The three flags that consume nothing yet

Accepted, validated, stored, and each logs at `Display` that it is not consumed and names the task
that will. Rejecting them would stop the task they were specified for from passing the flag it
needs; ignoring them silently is exactly the failure the unknown-flag check exists to prevent.

| flag | blocked on |
| --- | --- |
| `-ack-warnings-at=<t>` | the package path builds no rules from the document, so nothing is latched to acknowledge |
| `-reveal-delay-ms=<n>` | the startup reveal is PLAN 5.6 |
| `-rhi=<vulkan\|gles>` | Android RHI selection is a device concern, PLAN 4.11 |

They are silent when nobody passes them, so an ordinary run does not carry three warnings.

## Deviations from PLAN.md

**1. `-replay=<file>` is `-replay-file=<file>`.** The engine owns `-replay=`:
`Engine/Source/Runtime/Engine/Private/GameInstance.cpp:650` reads it and hands the value to
`PlayReplay`, which never loads the map. The first run of the smoke gate hung for ten minutes on
`-replay=C:/none.bin`, with the log stopping at engine init and nothing from this project in it.
The `player.json` key is renamed to match, so both surfaces keep one spelling.

**2. The unknown-flag check does not cover the flat namespace on the command line.** Recorded in
revision 2 of the spec and unchanged here. `player.json` keys are policed completely, which is the
source a Shipping Android build actually uses. On the command line only `-udash*` is policed,
because a token outside it cannot be told from one of Unreal's several hundred. A flat-namespace
typo shows instead as `source=default` for the flag you meant to set, which the gate asserts.

**3. The resolver is in the `UnRealDash` game module, not `UnRealDashCore`.** The spec said core.
It sits beside `SmokeConfig.h`, which chunk 07 put in the game module for the same reason: this is
the player's command line, not core logic, and its test commandlet lives in the same module.

## What running it found that reading it did not

**`-rhi=` overlaps the engine.** `RHI/Private/Windows/WindowsDynamicRHI.cpp:439` reads `RHI=`, so on
Windows the engine acts on the flag while this project only validates and stores it. Kept, because
PLAN 4.7 gives the flag the same meaning the engine does and renaming would leave the player unable
to hand the engine the thing the flag is for. The consequence is that `-rhi=` must not appear in a
run whose capture is compared. No gate passes it except the smoke gate, which does not compare that
run's capture.

**Every rejection since chunk 11 has been exiting 0.**
`FPlatformMisc::RequestExitWithStatus(false, N)` reaches `WindowsPlatformMisc.cpp:1517`, which
passes N to `PostQuitMessage`; that wParam never becomes the process exit code, because
`GuardedMain` returns its own `ErrorLevel`. Nine call sites in `DashPackageScreen.cpp` were using
it. No gate had caught it because none of them asserted an exit code on a rejection path, and the
flag gate is the first that does. They now go through `RejectRun`, which flushes `GLog` and forces
the exit so the code survives.

**A claim in revision 3 of the spec was wrong, and mutation is what caught it.** The first draft
said `FParse::Value` matches a bare substring, so `-profile=` would be read out of
`-udash-profile=`. Three hidden switches do end in a documented name and one of them is passed on
every device batch run, so it looked sound, and a boundary-checking helper was written and shipped
on it. `FCString::Strifind` "requires non-alphanumeric lead-in" (`CString.h:378`), so the match is
refused and the collision never existed. Removing the helper and rebuilding left all 50 resolver
assertions passing, which a real collision could not have done. The helper is removed rather than
kept as harmless insurance, and a test replaces it: the resolver commandlet passes all three hidden
switches and asserts the documented fields still read `source=default`, so the engine's lead-in
rule is pinned rather than assumed.

The `-udash-sweep=` rename stays, on the reason that survives. A named `signal_core` scenario
carries `speed` and `temperature`; the old switch synthesises a sweep over the document's own
signals and their declared ranges. The chunk 14 live fixture declares neither of those names, so
both have to exist, and two different things should not be one word apart.

## Two ways the gate could have lied, now closed

**A hang produced no verdict at all.** `Invoke-Player` waits with a 90 second bound instead of
`-Wait`, and a launch that does not return is a failure.

**A hung player held the log open.** The next run's delete failed silently and every later run read
the hung run's log, so one real failure was reported as twenty-five, nineteen of them in flags that
were working correctly. The delete is now checked, and a log that will not go away fails the gate
and kills the process holding it.

## Suites

```
dashboard-spec  [doctest] test cases:      30 |      30 passed | 0 failed | 0 skipped
                [doctest] assertions: 1002201 | 1002201 passed | 0 failed |

signal-core     [doctest] test cases:    155 |    155 passed | 0 failed | 1 skipped
                [doctest] assertions: 112946 | 112946 passed | 0 failed |

DashPlayerConfigTest assertions=50 failures=0
Package gate cases=51 accepted=14 rejected=37 both_forms=34 archive_only=17 failures=0
Pester          Tests Passed: 76, Failed: 0, Skipped: 0
doctor.ps1 -Profile workstation  exit 0
UBT Win64 UnRealDashEditor       Result: Succeeded
UBT Android UnRealDash           Result: Succeeded
```

signal-core gains 7 cases and 70 assertions, all for `FileTransport`: a path it cannot use, a
missing file, bounded reads on a buffer that is not a multiple of the frame size, the end of a
file reported as a disconnect rather than as silence, looping with a lap count, an empty file
disconnecting rather than spinning, and an empty destination that consumes nothing.

Every one of the 50 resolver assertions was watched failing before being trusted: changing the
default port to 35001 turned the suite red on exactly one line and green again on restore.

## Not run, and why

- **Anything needing the head unit.** `-rhi=` has no effect worth measuring on Windows beyond the
  engine's own RHI selection, and the Android half of `-profile=` is PLAN 4.11.
- **`-connector=replay` end to end through the player.** The transport is covered by seven doctest
  cases and the flag resolves and dispatches, but no gate feeds a recorded file through the player
  and measures the result. That needs a recorded `.bin` fixture, which nothing generates yet.
- **The Shipping "no HUD created" finding** from chunk 11 is still open and untouched here.
