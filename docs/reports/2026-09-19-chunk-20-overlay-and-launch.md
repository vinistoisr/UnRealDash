# Chunk 20: the overlay, asset import rows and launch timing (PLAN 4.8, part 4 of 4)

**PLAN 4.8 is complete**, with three clauses recorded open and named below. This chunk added the
two record types that were left, the launch measurement whose clock domain PLAN spends a paragraph
on, the on-screen overlay, and the clause map that checks nothing fell between the four chunks.

**Built by Codex** from the frozen spec, reviewed and proved here. One compile error and one gate
assumption needed fixing; both are described below. Codex ran nothing and said so.

## The new records, as a run wrote them

```json
{"type":"asset_import","asset":"assets/badge.png","bytes":93,"width":24,"height":24,"t_decode_start":208313999006800,"t_decode_end":208313999060000,"t_upload_start":208313999091299,"t_first_usable":208313999268900}
{"type":"asset_import","asset":"assets/face.png","bytes":144,"width":60,"height":60,"t_decode_start":208313999261600,"t_decode_end":208313999307800,"t_upload_start":208313999332299,"t_first_usable":208313999429600}
{"type":"first_usable","frame":0,"t_present":208208041496500}
{"type":"launch","launch_counter":2090019540888,"first_usable_counter":2090034827452,"counter_frequency":10000000,"process_start_ticks":null,"process_ticks_per_second":null,"first_usable_boot_ns":null,"am_start_output":null,"am_launch_state":null}
```

```
launch   1.528656s from launch request; under 30s: True
windows_launch_overhead_counter: 626690 ticks, 0.0627 s
```

Two distinct assets give two rows with distinct identifiers, correct dimensions and byte sizes,
and in every row decode start < decode end < upload start < first usable.

## Criteria

| # | criterion | result |
| --- | --- | --- |
| 1 | one import row per asset, two distinct assets give two rows | proven with a fixture extended to carry a second image |
| 2 | first-usable later than upload start, every row | both rows; a mutation that moves it earlier is rejected |
| 3 | `first_usable` names a frame whose present row carries a `valid` entry | verified by the classifier; naming the wrong frame is rejected |
| 4 | the launch record carries both counters and the frequency | 1.528656 s from launch request |
| 5 | the header carries the measured Windows uncertainty | 626,690 counter ticks, measured by `capture-metrics.ps1 -Launch` |
| 6 | the overlay draws, and with it off the capture gate passes unchanged | 26,291 pixels changed, region (15,17)-(288,184) against a placement of (12,12); capture gate green |
| 7 | no overlay number reaches the log | no writer call exists in the overlay; the only traffic is `SampledMemory` reading into it |
| 8 | both suites green with no drop | dashboard-spec 30 / 1,002,203; signal-core 179 / 117,162, unchanged |
| 9 | nine gates, doctor, Pester, UBT Win64 and Android | all clean |
| 10 | the 4.8 clause map is complete | below |
| 11 | a 30 s kill leaves 29 s of rows in the chunk 18 record types too | 30.71 s across every type, one trailing partial line |
| 12 | rule-expiry rows name a declared rule and a valid kind, one status row each | classifier; watched rejecting a synthetic row |
| 13 | every record type validates against its schema, timestamps monotonic and positive | mutation-checked in both directions |

## PLAN 4.8's gate paragraph, clause by clause

| # | clause | closed by | proof |
| --- | --- | --- | --- |
| 1 | per-frame row count within 1 percent of the frame count | 17 | event log gate, 0.01 percent against `GFrameCounter` |
| 2 | receive rows cover every published sample | 18 | lifecycle gate, receive rows exactly equal to `SamplesApplied` |
| 3 | sampled stream one row per second within 1 percent | 17 | event log gate, 1.0000 s from the rows' own `t` |
| 4 | one lifecycle state per sample, four counts sum | 18 | lifecycle gate, 3000 of 3000 |
| 5 | delayed present: counts still sum, no sample matches two | 19 | expiry gate 7b, `-udash-hold-frame-ms` |
| 6 | a signal bound to nothing ends `acquired-not-displayed` | 18 | lifecycle gate, 1499 samples |
| 7 | submission suppressed ends `acquired-not-displayed` | 19 | expiry gate 7a, 798 samples, presented 0 |
| 8 | killed between acquire and submit, every sample still gets a state | 19 | expiry gate, `-udash-kill-at`, 602 of 602 |
| 9 | a sample held across a pause ends `presented` | 19 | expiry gate 7b, 253 presented under a 30 ms hold |
| 10 | present and acquisition sample ids resolve to receive rows | 18 | classifier; an orphan id is rejected |
| 11 | armed-expiry names a signal with a receive row and a later deadline | 19 | classifier, every run |
| 12 | rule-expiry names a declared rule and kind `hold_last` or `debounce` | 20 | classifier; **no rows in a player run**, see below |
| 13 | every armed-expiry and rule-expiry row has exactly one status row | 19 + 20 | five doctests and every gate run |
| 14 | a clean exit produces `cancelled` with reason `run_end` | 19 | expiry gate, 2 rows, 0 unresolved |
| 15 | a kill before a deadline reports `unresolved` | 19 | expiry gate, 2 unresolved, no `run_end` rows |
| 16 | a re-armed expiry is cancelled and yields no observation | 19 | expiry gate, 298 of 300 |
| 17 | a signal restored between firing and acquisition is `fired-not-displayed` | 19 | expiry gate, 1 counted |
| 18 | a frame that acquires stale, then the signal recovers, then it presents, yields one observation | **open** | see below |
| 19 | two disconnect episodes yield two `fired` rows and two observations | **open** | see below |
| 20 | one sample across several frames yields exactly one observation | 18 | 1497 observations for 1497 samples, all spanning frames |
| 21 | every interpolated present row names `derived_from` | 18 | the Stage 0 path never interpolates; always null, recorded in the header |
| 22 | every runtime-imported asset has one import row | 20 | two assets, two rows |
| 23 | each record type validates against its declared column schema | 20 | every type, both directions |
| 24 | every timestamp present, monotonic and positive | 20 | mutation-checked; a regressed sampled `t` is caught |
| 25 | launch counters, Android fields, uncertainty per platform | 20 | Windows proven; **Android written, unverified** |
| 26 | a 30 s kill leaves 29 s of rows in every record type | 17 + 20 | 30.71 s across all types |

### The three that are open, and why

- **Clause 18**, the delayed-presentation observation. The mechanism exists and is exercised:
  firings are displayed and measured at 2.89 ms, and `-udash-hold-frame-ms` widens the
  acquire-to-present gap. What is not isolated is a run where the signal demonstrably *recovers*
  between the acquire and the present, because the sweep source stops rather than resumes. It
  needs a source that goes quiet and then comes back, which is the relay the connector gate
  drives.
- **Clause 19**, two disconnect episodes. Same reason: it needs a fixture that reconnects twice.
  Chunk 19's spec listed it as criterion 5c and chunk 20's non-goals said it belonged with the
  connector gate; **this map resolves that contradiction as open and assigned to the connector
  gate**, which already kills and restores a relay once.
- **Clause 25, the Android half.** The in-process `/proc/self/stat` read, the `am start -W`
  evidence path and the Android uncertainty text are all written and compile for Android ARM64.
  None is verified, because verifying it needs the device. The header says so in its own words
  rather than leaving a reader to assume.

**Clause 12 is satisfied by a check that would bite rather than by rows existing.** The Stage 0
player loads no threshold rules, so it arms no `hold_last` or `debounce` entry and a player log
contains no rule-expiry rows. The header states that. The classifier's check was watched rejecting
a synthetic rule-expiry row, so the clause is closed by a live check over an empty set rather than
by a vacuous pass nobody tested.

## What Codex built, and what I had to fix

Codex worked from the frozen spec for about 25 minutes and 185,000 tokens, touched 15 files, ran
nothing, and was accurate about that. Two things needed fixing here:

1. **A compile error.** `DashDebugOverlay.cpp` declared a local named `Slot`, which hides
   `UUserWidget::Slot`; warnings are errors in this project.
2. **A gate assumption, not Codex's fault.** Chunk 18's lifecycle gate mutates a log by removing
   every present row. Codex's new validator correctly rejects that log, because the `first_usable`
   and `launch` records name a presentation that no longer exists. The mutation now removes all
   three, which keeps it about what it is about: whether the partition stays exhaustive when its
   happy path is gone, rather than whether the log contradicts itself.

Two pieces of Codex's design are worth keeping on the record:

- **The overlay pre-builds a widget switcher per digit** and flips an index per frame, so updating
  six numbers allocates nothing. It costs about 420 widgets built once.
- **The completion signal is an `ENQUEUE_RENDER_COMMAND` issued after `UpdateResource`**, so it
  runs on the render thread behind the upload it is timing. That is a real completion signal for
  that resource rather than a guess, and criterion 2 checks the ordering it implies.

I extended the chunk 18 fixture with a second image, because a fixture with one asset cannot tell
a per-asset identifier from a hardcoded one, nor show that the shared-texture cache does not
collapse two distinct assets into a single row.

## Suites and gates

```
signal-core     [doctest] test cases:    179 |    179 passed | 0 failed | 1 skipped
                [doctest] assertions: 117162 | 117162 passed | 0 failed |
dashboard-spec  [doctest] test cases:     30 |     30 passed | 0 failed | 0 skipped
                [doctest] assertions: 1002203 | 1002203 passed | 0 failed |
Pester          Tests Passed: 76, Failed: 0, Skipped: 0
doctor, UBT Win64, UBT Android   all clean
nine gates                       0 failures each
```

Five mutated logs were used to prove the new validator checks bite: a `first_usable` naming the
wrong frame, a completion signal moved before its upload, a middle sampled timestamp moved
backwards, a column the schema does not declare, and a rule-expiry row with no status row. All
five are rejected; the unmutated log passes.

## Not run, and why

- **Anything on the device.** Clause 25's Android half and the Android sampled-stream sources are
  written and compile, and nothing is deployed. 4.11 and 6.x.
- **Clauses 18 and 19**, named above, assigned to the connector gate.
- **The 8-hour soak.** 6.6b, at roughly 405 KB/s at 580 fps or about 34 KB/s at 60 fps.
