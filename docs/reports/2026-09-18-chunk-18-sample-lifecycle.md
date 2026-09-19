# Chunk 18: sample identity and the lifecycle partition (PLAN 4.8, part 2 of 4)

**The second quarter of PLAN 4.8 is complete.** Every published sample now has an identity, four
correlated record types follow it from the wire to the screen, and a post-run classifier assigns
each one exactly one of 4.8's four lifecycle states, with the counts summing to the published
count.

## The gate, in one run

```
a 30 second run with one declared signal bound to nothing
  receives 3000, acquires 18804, submits 18804, presents 18544, dropped 0
  criterion 2a: 3000 receive rows for 3000 applied samples, exactly

criteria 1, 2 and 4: correlation and the partition
  lifecycle
    presented                    1497
    acquired-not-displayed       1501
    superseded-unacquired           2
    latest-unacquired               0
    sum                          3000 of 3000 published
      acquired-not-displayed: bound to no visible component: 1499
      acquired-not-displayed: submitted, the run ended before it presented: 2
    receive-to-present observations 1497, 1497 of them displayed across more than one frame
  criterion 3: 1499 samples of the unbound signal, none of them presented
  criterion 4: 1497 observations, 1497 samples spanning several frames

criterion 5: the classifier must be able to fail
  an acquisition row naming an unknown sample is rejected
  with every present row removed: presented 0, and the partition still sums, 3000 of 3000

Lifecycle gate: 0 failures
```

`3000 receive rows for 3000 applied samples, exactly` is the line that matters most. The
denominator is the pipeline's own count of what `Apply` accepted, not the log's account of what it
managed to write, so the partition cannot pass by quietly lowering the number it sums to.

## What a published sample is, and why it matters

`registry_.Publish()` publishes a snapshot once per `Pump` and `published_` counts those
(`Acquisition.cpp:206`). The per-sample count is `applied_`, incremented after each successful
`registry_.Apply` (`Acquisition.cpp:173`). **A published sample, for 4.8's partition, is one
`Apply` accepted.** Reading it the other way would have made the denominator the frame count and
the whole partition meaningless. DeepSeek verified this reading against the tree before any of it
was built.

## Criteria

| # | criterion | result |
| --- | --- | --- |
| 1 | every acquisition and present sample id resolves to a receive row | classifier, and a mutated log proves it bites |
| 2 | one state per sample, four counts summing to the published count, per signal and in total | 3000 of 3000, and receive rows exactly equal to samples applied |
| 3 | a signal bound to nothing ends every sample `acquired-not-displayed` | 1499 samples, none presented |
| 4 | one observation per presented sample, at its earliest presentation | 1497 observations for 1497 samples, every one on screen for many frames |
| 5 | the classifier is watched failing | an orphan sample id is rejected; with all present rows removed, presented drops to 0 and the sum still holds |
| 6 | rows validate against the declared schema, timestamps present, monotonic, positive | validator, with `t_acquire` and `t_submit` added to the checked set |
| 7 | interpolation attribution filled or documented as null with the reason | documented, and verified: see below |
| 8 | both CMake suites green with no drop | dashboard-spec 30 / 1,002,203; signal-core 174 / 117,123, up from 171 / 117,088 |
| 9 | seven gates, doctor, Pester, UBT Win64 and Android | all clean |

## Files, by PLAN task

| path | task |
| --- | --- |
| `runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Sample.h` | 4.8, the sample id |
| `runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Acquisition.h` | 4.8, `OnReceive`, `OnAcquire` on sample ids |
| `runtime/UnRealDash/Source/SignalCore/Private/Acquisition.cpp` | 4.8, id assignment and the receive call |
| `runtime/UnRealDash/Source/SignalCore/Public/SignalCore/AcquisitionEvents.h` | 4.8, the ring sink follows |
| `packages/signal-core/tests/test_acquisition.cpp` | 4.8, three identity cases |
| `runtime/UnRealDash/Source/UnRealDashCore/.../SignalCoreAdapter.{h,cpp}` | 4.8, the id across the boundary |
| `runtime/UnRealDash/Source/UnRealDashCore/.../DashAcquisition.{h,cpp}` and its private header | 4.8, `SampleIds`, the forwarding sink, `SetEventLog` |
| `runtime/UnRealDash/Source/UnRealDashCore/.../DashEventLog.{h,cpp}` | 4.8, two more rings and four typed writers |
| `runtime/UnRealDash/Source/UnRealDashCore/.../DashBindingTable.{h,cpp}` | 4.8, reporting what was rendered |
| `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPackageScreen.{h,cpp}` | 4.8, per-frame wiring and the counts line |
| `scripts/validate-event-log.py` | 4.8, the classifier |
| `scripts/run-lifecycle-gate.ps1`, `tools/gen-chunk18-fixture.py` | 4.8, the gate and its fixture |

## Decisions worth their own line

**The id is assigned before `Apply` and consumed only on success.** Before, because the registry
stores the sample and everything downstream reads its copy. Only on success, because a rejected
sample that burned an id would leave a hole in the sequence, and a hole is indistinguishable from
a row the ring dropped. A test pins this by feeding an old-generation sample and checking the next
accepted one gets the next id.

**`seq` could not serve as the identity.** It is the source's sequence and restarts at 1 on a new
connection, which a test pins by reconnecting and asserting the new sample's `seq` is 1 while its
id is larger than the previous one's.

**One ring per producer thread.** `EventRowRing` is single-producer by construction and receive
rows come from the acquisition thread while frame, acquire, submit and present rows come from the
game thread. Three rings now: frame rows, receive rows, and a wide one for acquisition and present
rows, which carry an array and need far more than a frame row's 384 bytes.

**The present row comes out of the same join as the per-frame row**, keyed by the same
`OnEndFrameRT` stamp. Chunk 17 revision 2 A exists because two present timestamps for one frame
would make 6.5's receive-to-present subtraction meaningless, and the only way to guarantee one is
to emit both rows from one place.

**A bound signal with no sample keeps its present-row entry, with a null sample id.** It renders
the missing-data presentation, which is a real thing on screen. Omitting the entry would make "one
entry per bound signal" false and hide the first seconds of every run.

**The submit row is recorded where the game thread finishes with its snapshot**, not at a real
submit callback, because there is no such callback for a UMG widget's contribution to a frame. The
row's meaning is "the game thread is done with what it acquired", which is what the lifecycle
partition needs it for, and this note is the record that it is not a GPU submit.

## Interpolation, checked rather than assumed

PLAN 4.8 requires the present row to name the sample an interpolated value was derived from.
`Interpolator.h` exists in signal-core, so the spec made establishing whether the live path runs it
a deliverable rather than writing null and moving on.

**It does not.** `Interpolator` appears in no file under `UnRealDashCore` or `UnRealDash`, and
`FDashBindingTable::Apply` reads the snapshot's value directly. The column is therefore always
null in Stage 0, and the log header says so in its own words:

```
"interpolation":"none: the Stage 0 binding path renders samples directly, so derived_from is always null"
```

## Six things the review found before any of this was built

DeepSeek rejected revision 1 of the spec. All six findings were accepted and are recorded as
revision 2 in `docs/build/chunk-18-sample-lifecycle.md`. The four that mattered:

1. **The partition denominator was circular.** Revision 1 made the receive row count the published
   count and merely logged the pipeline's counter beside it. Now they must be equal, exactly.
2. **The id had to be added on the engine side too.** `FFrameSnapshot` holds `FSignalSample`, not
   `signal_core::Sample`, so an id only in signal-core would have left the present row unable to
   name what it rendered.
3. **There was no seam to emit a receive row through, and none for a present row.** Revision 1
   described both outcomes and specified neither path. `OnReceive`, `SetEventLog`, the typed
   writers and the binding table's out parameter all exist because of this finding.
4. **A bound signal that has never received a sample** contradicted two clauses of revision 1 at
   once. Resolved above.

## What running it found

**Nothing that changed a design**, which is a first for this project and is worth saying plainly
rather than treating as the expected case. The two defects were both the Android type trap:

`uint64` and `std::uint64_t` are distinct types on Android, where one is `unsigned long` and the
other `unsigned long long`, so `TArrayView<const uint64>` would not build from a
`const std::uint64_t*` and vice versa. This is the same trap chunk 17 hit with `SIZE_T`. It is now
one `reinterpret_cast` at one boundary, guarded by a `static_assert` on the width, rather than
spread through the seam.

## Size, which receive rows change

| | |
| --- | --- |
| measured at 587 fps with 94 receive rows/s | 404,968 bytes/s, **11.66 GB per 8 hours** |
| chunk 17's figure, before these rows | 93,714 bytes/s, 2.70 GB per 8 hours |
| extrapolated at 60 fps with the same receive rate | about 34 KB/s, **0.97 GB per 8 hours** |

Four rows per frame instead of one is what did it. The 8-hour soak in 6.6b must run frame-rate
limited, which chunk 17 already said and this makes four times more true.

## Suites

```
signal-core     [doctest] test cases:    174 |    174 passed | 0 failed | 1 skipped
                [doctest] assertions: 117123 | 117123 passed | 0 failed |
dashboard-spec  [doctest] test cases:     30 |     30 passed | 0 failed | 0 skipped
                [doctest] assertions: 1002203 | 1002203 passed | 0 failed |
Pester          Tests Passed: 76, Failed: 0, Skipped: 0
doctor.ps1 -Profile workstation   exit 0
seven gates                       0 failures each
```

The three new signal-core cases were watched failing: making a rejected sample burn an id turns
the sequence test red on the one assertion that cares.

## What this chunk cannot prove, and who proves it

Four of 4.8's lifecycle branches are unreachable without machinery that holds the acquisition and
presentation sides apart on purpose, which is chunk 19's:

| branch | needs |
| --- | --- |
| `acquired-not-displayed`, no submit row | submission suppressed |
| `acquired-not-displayed`, submitted and never presented | a deliberately delayed present |
| the counts still summing after a kill mid-flight | a kill between an acquire and its submit |
| a held sample staying `presented` while newer ones are published | a pause across a submit |

The third sub-reason did occur naturally here, twice, for the last frames of the run, which is why
it appears in the output above. **PLAN 4.8 is not complete at the end of this chunk.** The chunk 20
report carries the clause-by-clause map of 4.8's gate paragraph to the chunk that closed each.

## Not run, and why

- **Anything on the device.** The Android build is proven; nothing is deployed.
- **The 8-hour soak.** That is 6.6b, and this chunk gives it the new size figure.
- **Expiry rows.** The present row's `expiry` column is written as null by construction and
  chunk 19 fills it.
