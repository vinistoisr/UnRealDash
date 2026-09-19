# Build chunk 16: the command-line surface

Status: revision 3, written against the tree and corrected twice against runs of it. Covers PLAN.md task 4.7. Read PLAN.md (4.7, 4.0's
`player.json`, 4.9 for the connectors it selects) and `docs/ARCHITECTURE.md` first.

Four chunks have each added their own gate switch. This replaces them with the one documented
surface, and makes every flag settable from a file, because a Shipping Android build has no command
line at all.

## Facts this chunk depends on

- **A config resolver already exists and does most of what 4.7 asks.** `SmokeConfig.h` declares
  `ResolveSmokeConfig(SavedDirectory, CommandLine, ReadFile, Log)`, and chunk 07 built it with the
  precedence 4.7 needs: the command line wins over `player.json` field by field, a missing file
  leaves a runnable configuration while an invalid field does not, and it logs the final value and
  source of every field. It is covered by a commandlet driven with fakes. **This chunk generalises
  it rather than writing a second resolver**; two config paths that disagree would be worse than
  the switch sprawl it replaces.
- **`player.json` is read from `FPaths::ProjectSavedDir()`** and the resolver logs the absolute
  path it read, whether it existed, and every field's source. The device run reads that line to
  find the storage location, so it is not optional.
- **The scenarios are real and named.** `signal_core::Scenario` has `idle`, `acceleration`,
  `high_temperature`, `missing_signal`, `disconnect`, `reconnect` and `stale_heartbeat`
  (`Scenarios.h:5-13`), and `GenerateScenario` writes a recording carrying `speed` (id 1, m/s) and
  `temperature` (id 2, K) (`Scenarios.cpp:17-18`). That is exactly the shape
  `FDashAcquisition`'s recording constructor takes, so `-scenario=<name>` is real work rather than
  a new waveform generator.
- **`scripts/capture-metrics.ps1` has no smoke mode.** It is still the foundation skeleton: a
  doctor gate and a command preview. PLAN 4.7's gate puts the per-flag tests there, so this chunk
  adds the mode.
- **`FDashAcquisition` has two construction paths** and the telemetry one already publishes a
  signal name to id map. The recording path parses the same information and does not expose it yet.

## The switches this replaces

| today, gate-only | documented |
| --- | --- |
| `-udash=` | `-udash=` unchanged |
| `-udash-scenario=<seconds>` | split in two, see revision 3 C: `-scenario=<name>` is a real signal-core scenario, and the old document-derived sweep stays gate-only as `-udash-sweep=<seconds>` |
| `-udash-shot=` | `-screenshot=` |
| `-udash-quit-after=` | `-quit-after=` |
| `-udash-profile=` | `-profile=` |
| `-connector=`, `-connector-host=`, `-connector-port=` | unchanged |
| `-udash-state=`, `-udash-fraction=`, `-udash-shot-at=`, `-udash-batch=`, `-udash-profile=`, `-udash-sweep=` | **stay gate-only** |

The four that stay are gate machinery with no meaning to a user: forcing a missing-data state,
pinning a gauge at a deflection, delaying a capture, and loading every fixture in one run. They are
not in the documented surface and the unknown-flag check has to know about them anyway, so they are
declared as a separate hidden set rather than quietly tolerated.

## Deliverables

### 1. `FDashPlayerConfig`, one resolver for the documented surface

In `UnRealDashCore`, modelled on `ResolveSmokeConfig` and sharing its shape: a struct of typed
fields, a resolve function taking the saved directory, the command line, a file reader and a log,
and a result carrying errors and whether the configuration is runnable.

The fields are PLAN 4.7's list exactly:

```
udash, scenario, replay, replay-loop, connector, connector-host, connector-port,
screenshot, quit-after, profile, freeze-scenario-time, ack-warnings-at, reveal-delay-ms, rhi
```

`connector-host` defaults to `127.0.0.1` and `connector-port` to `35000`.

Every field is settable from the command line and from `player.json` under the same name without
the leading dash, and the command line wins field by field. That last part is not a detail: a
Shipping Android build receives no command line, so the file alone has to be sufficient, and a
developer overriding one field from a shell must not lose the rest.

### 2. An unknown flag is an error, not a shrug

An unrecognised `-flag=` on the command line, or an unrecognised key in `player.json`, prints the
supported list and exits non-zero.

This is the deliverable with teeth. A mistyped flag that is silently ignored produces a run that
looks like the one you asked for and is not, which is the failure every gate in this project has
been written to avoid. The same applies to the file: a `player.json` key with a typo is a setting
that is not in force.

Unknown-flag detection has to know the engine's own flags too, which a player is launched with in
quantity (`-windowed`, `-ResX`, `-nosplash`, `-unattended`, `-ExecCmds` and more). So the check is
scoped: only flags in a reserved prefix set are validated, and anything else is left to the engine.
The reserved set is the documented list plus the hidden gate switches. A flag beginning `-udash`
that is not one of them is rejected, which is what catches a typo in this project's own surface
without claiming authority over Unreal's.

### 3. The flags that do something, doing it

- `-udash=<path>` as today.
- `-connector=<sim|replay|tcp>`. `sim` runs a named scenario, `tcp` is chunk 15's client, and
  `replay` is a new file transport reading recorded bytes in bounded chunks.
- `-scenario=<name>` selects a `signal_core` scenario by name. An unparseable name is an error
  naming the supported list, never a silent fall back to a default, which chunk 07 already decided.
- `-replay-file=<file>`, `-replay-loop`. Looping restarts at the end rather than disconnecting.
  Not `-replay=`, which the engine owns; see revision 3 A.
- `-connector-host=`, `-connector-port=`.
- `-screenshot=<path>`, `-quit-after=<seconds>`, `-profile=<mobile|desktop>`.
- `-freeze-scenario-time` holds the acquisition clock at the instant of the first tick, so a
  capture is taken at a known point of a scenario rather than wherever the frame happened to land.
  PLAN 4.11 needs it and the chunk 14 report recorded its absence as the reason a needle's angle
  could not be pinned to a scenario timestamp.

### 4. The flags that cannot do anything yet, saying so

Three of PLAN 4.7's flags name features that do not exist:

- `-ack-warnings-at=<t>` issues the acknowledge call for every latched rule. The package path
  builds no rules from the document, so there is nothing latched to acknowledge.
- `-reveal-delay-ms=<n>` delays the startup reveal's assets. The reveal is PLAN 5.6.
- `-rhi=<vulkan|gles>` selects the Android RHI, which is a device concern.

They are **accepted, validated and stored**, and the resolver logs at `Display` that the flag is
accepted but not yet consumed, naming the task that will consume it. Rejecting them would stop 5.6
passing the flag it was specified for; ignoring them silently would be the exact failure
deliverable 2 exists to prevent. Saying so in the log is the honest third option, and the report
lists all three.

### 5. A smoke mode in `capture-metrics.ps1`

`-Smoke` runs the flag matrix: every documented flag exercised once, the unknown-flag and
unknown-key rejections, and the reproduction check.

## Constraints

- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module.
- Errors are values. A bad flag is a message and an exit code, never a crash and never a default.
- The resolver is testable without an engine launch, through the commandlet pattern chunk 07 built,
  because a config resolver covered only by launching a player is a config resolver nobody tests.
- No em dashes; no PowerShell 7 only syntax in scripts.

## Non-goals

- The debug overlay (4.8), the example document (4.10) and device work (4.11).
- Changing what any existing gate measures. The gates move to the new flag names and must produce
  the same numbers, which is criterion 1.

## Pass/fail criteria

1. **Every existing gate still passes**, on the renamed flags: the package commandlet, and the
   capture, dial, live and connector gates. Four chunks of switches are being renamed under them.
2. **Each documented flag has a test** in `capture-metrics.ps1 -Smoke`, and the run prints one line
   per flag saying what it set and where it came from.
3. **An unknown flag in this project's own namespace exits non-zero and prints the supported
   list.** A mistyped `-udash*` switch. A mistyped flat-namespace flag is NOT caught, which is a
   deviation from PLAN 4.7 recorded in revision 2 C1; it shows instead as `source=default` in the
   per-field log, which criterion 2 asserts.
4. **An unknown `player.json` key does the same.**
5. **The same run reproduces from the command line and from `player.json`**, on a deterministic
   configuration with no connector, and the two captures pass `scripts/compare-capture.py`. Not
   with a connector: a socket or a scenario over real time cannot produce the same frame twice, so
   that would measure the connector's timing rather than whether the two configuration paths
   agree. See revision 2 C2.
6. **The command line wins field by field.** A `player.json` setting four fields plus a command line
   overriding one yields the overridden value and the other three from the file, shown in the log.
7. **A missing `player.json` leaves a runnable configuration**, and an invalid field does not.
8. Both CMake suites green with no drop: dashboard-spec 30 cases and 1,002,201 assertions,
   signal-core 148 and 112,876.
9. `doctor.ps1 -Profile workstation` exits 0 and Pester passes with `-CI`.
10. UBT builds Win64 and Android ARM64.

## Report format

Files added or changed with their PLAN task; the proof output verbatim; the flag matrix with each
flag's resolved value and source; the three accepted-but-unconsumed flags named with the task that
will consume each; what was not run and why; any deviation from PLAN.md with the reason.

---

## Revision 2: two things the spec promised that it cannot deliver as written

### C1. The unknown-flag check cannot cover the flat namespace, and the spec said it would

Deliverable 2 opened with "an unrecognised `-flag=` on the command line ... prints the supported
list and exits non-zero" and then narrowed it to a reserved prefix. Criterion 3 then demanded both
"a mistyped documented flag and a mistyped hidden one". Most of PLAN 4.7's names carry no prefix,
so `-scenaro=acceleration` is exactly the case the narrowed rule cannot police, and the criterion
could not pass. The spec promised a gate it had already ruled out two paragraphs earlier.

**This is a deviation from PLAN 4.7**, which says flatly that an unknown flag prints the supported
list and exits non-zero. Recorded rather than quietly narrowed:

- **`player.json` keys are fully policed.** The file's namespace belongs entirely to this project,
  so an unrecognised key is unambiguous and is rejected. This is the whole of PLAN's requirement
  for the source that a Shipping Android build actually uses, which is the one that matters most.
- **The command line is policed only within `-udash*`.** A token outside it cannot be told apart
  from one of Unreal's several hundred, and rejecting by edit distance was considered and dropped:
  Unreal has its own replay system, so `-replay` is not safely assumed to be ours.
- **Every documented flag logs its resolved value and source**, so a flat-namespace typo shows up
  as `source=default` for the flag you meant to set. The smoke mode asserts the source of each
  flag it sets, which is what turns "did my flag take effect" into something checked rather than
  hoped.

Criterion 3 is rewritten to claim only what is delivered.

### C2. "Identical" is not what the comparator means

Criterion 5 said two captures compared with `scripts/compare-capture.py` must be "identical". That
tool passes within a tolerance by design, `worst <= 48` and `moved <= 0.001`, and it ignores alpha.
"Identical, compared with a tolerance comparator" is two different claims in one sentence.

Worse, the criterion as written would have been run against a connector. A socket or a scenario
over real time cannot produce the same frame twice, so a reproduction check over one of those
measures the connector's timing rather than whether two configuration paths agree.

**Criterion 5 is rewritten**: the reproduction check runs a deterministic configuration, with no
connector, where chunk 11 already measured two captures of one document as bit identical at
`worst=0 moved=0.000000`. The comparator's own pass condition is then the check, and what it tests
is the thing the criterion is about, that the command line and the file produce the same
configuration.

### C3. The engine-flag collision was asserted and is now checked

The prefix-scoping argument rests on Unreal owning the flat namespace. The review could not verify
that from this repository and correctly marked it unverified. Checked since, against the engine
source: none of `scenario`, `replay`, `connector`, `screenshot`, `quit-after`, `profile` or `rhi`
appears as a top-level `FParse::Value(FCommandLine::Get(), ...)` or `FParse::Param` switch in
`Engine/Source/Runtime`. That is one grep pattern rather than a proof, since the engine reads flags
many ways, so `-profile` in particular is exercised in the smoke mode to confirm a normal launch.

---

## Revision 3: what running the surface found, and one thing this revision itself got wrong first

Revision 2 C3 asserted, from a grep, that the engine claims none of the documented names. Running
them proved otherwise for two of them.

### A. `-replay=` is an engine flag, and it hangs the player

`Engine/Source/Runtime/Engine/Private/GameInstance.cpp:650` reads `-REPLAY=` and passes the value to
`PlayReplay`. The map is never loaded, so `BeginPlay` never runs. The first run of the smoke gate
hung for ten minutes on `-replay=C:/none.bin` and the log stopped at engine init with nothing from
this project in it at all.

**Deviation from PLAN 4.7**, which names the flag `-replay=<file>`: it is `-replay-file=<file>`, on
both the command line and in `player.json`, so the two surfaces keep the same spelling.
`-replay-loop` is unaffected, and was checked by launching it: `FParse::Param` requires both a dash
before and whitespace after, and the flag carries no `=` for the engine's reader to key on.

### B. `-rhi=` overlaps with the engine's own RHI selection on Windows

`RHI/Private/Windows/WindowsDynamicRHI.cpp:439` reads `RHI=`. So on Windows the engine acts on the
flag even though this project's resolver only validates and stores it. Recorded rather than
renamed: PLAN 4.7 specifies `-rhi=<vulkan|gles>` for Android RHI selection, which is the same
meaning the engine gives it, and renaming would leave the player unable to hand the engine the
thing the flag is for. The consequence is that `-rhi=` must not appear in a run whose capture is
compared, because it can change the renderer under the comparator. No gate passes it except the
smoke gate, which does not compare that run's capture.

### C. The substring collision this revision first claimed does not exist

Revision 3 was first written claiming that `FParse::Value` matches a bare substring, so
`-profile=` would be read out of `-udash-profile=`, `-quit-after=` out of `-udash-quit-after=` and
`-scenario=` out of `-udash-scenario=`. Three hidden gate switches do end in a documented name and
one of them, `-udash-profile=`, is passed on every device batch run, so the reasoning looked sound
and a boundary-checking helper was written and shipped on it.

It is wrong. `FParse::Value` finds its match through `FCString::Strifind`, whose contract is "find
string in string, case insensitive, **requires non-alphanumeric lead-in**" (`CString.h:378`). The
character before the inner dash of `-udash-profile=` is a letter, so the match is refused.

This was caught by mutation rather than by reading: removing the helper and rebuilding left all 50
resolver assertions passing, which a real collision could not have done. **The helper is removed**
rather than kept as harmless insurance, because a guard whose stated reason is false teaches the
next reader something untrue about the engine. What replaces it is a test:
`DashPlayerConfigTestCommandlet` passes all three hidden switches and asserts the documented fields
still read `source=default`, so the lead-in rule is pinned instead of assumed and an engine upgrade
that changed it would fail there rather than silently in a gate.

The `-udash-scenario=` to `-udash-sweep=` rename **stays**, on the reason that survives: chunk 16
gave `-scenario=<name>` to a real `signal_core` scenario carrying `speed` and `temperature`, while
the old switch synthesises a sweep over the document's own signals and their declared ranges. The
chunk 14 live fixture declares neither `speed` nor `temperature`, so both have to exist, and two
different things should not be one word apart. `-udash-quit-after=` and `-udash-shot=` are dropped
from the hidden set entirely, so a stale script still passing one is rejected rather than ignored.

### D. A rejected run was exiting 0

`FPlatformMisc::RequestExitWithStatus(false, N)` reaches `WindowsPlatformMisc.cpp:1517`, which
passes N to `PostQuitMessage`. That wParam never becomes the process exit code, because
`GuardedMain` returns its own `ErrorLevel`. Nine rejection paths in `DashPackageScreen.cpp` were
using it, so every one of them, going back to chunk 11, has been exiting 0 and looking like a clean
run to any script that tests `$LASTEXITCODE`. No gate had noticed because none of them asserted an
exit code on a rejection; the flag gate is the first that does.

They now go through `RejectRun`, which flushes `GLog` and forces the exit so the code survives.
Forcing skips a clean shutdown, which is acceptable on a path that has already decided the run is
invalid, and the log is flushed explicitly first.

### E. A hung launch is now a gate failure, and a stale log is too

`Invoke-Player` waits with a 90 second bound instead of `-Wait`. The ten minute hang in A produced
no output and no verdict.

Worse, the hung player kept the log file open, so the next run's delete failed silently and every
later run read the hung run's log. One real failure was reported as twenty-five, nineteen of them
in flags that were working. The delete is now checked, and a log that will not go away fails the
gate and kills the process holding it.
