# Chunk 17: the event log file, per-frame rows and the sampled stream (PLAN 4.8, part 1 of 4)

**The first quarter of PLAN 4.8 is complete.** The player writes a JSON Lines event log that
declares its own schema, carries a per-frame row and a 1 Hz sampled row, flushes once a second,
and survives a kill with the last second of evidence in it. A validator checks any log against the
schema that log declared.

PLAN 4.8 is four chunks. The split and the reason for it are in
`docs/build/chunk-17-event-log.md`; the short version is that 4.8's gate paragraph alone specifies
twenty-two runs, and built as one chunk it would be a diff nobody could review.

## The gate, in one run

```
criteria 1, 2, 4 and 5: a 60 second run
  engine frames 32720, frame rows 32718, sampled rows 61, dropped 0
  criterion 1: frame rows are 0.01% off the engine's frame count, limit 1.00%
  cadence  sampled 1.0000s between rows, target 1.0000s
  bytes    5624162
  elapsed  60.01s
  rate     93714 bytes/s, 2.70 GB per 8 hours
    frame         32718 rows    545.17/s
    header            1 rows      0.02/s
    sampled          61 rows      1.02/s
    schema            1 rows      0.02/s

criterion 4 again, the other direction: a row of an undeclared type must be rejected
  rejected
  and a renamed column is rejected too

criterion 3: a kill at 30 seconds leaves a readable log
  elapsed  30.56s
    frame         15953 rows    521.96/s
    sampled          31 rows      1.01/s
    one trailing partial line, dropped; a kill inside a write looks like this

criterion 7: a log that cannot be opened does not stop the run
  ran and rendered, and said: DashEventLog unavailable: cannot open event log
  C:/Users/.../Saved/events/run.jsonl

Event log gate: 0 failures
```

The frame count is `GFrameCounter`'s own delta, logged by the player at `EndPlay`, not the log's
count of what it managed to write. Criterion 1 measured against the row count would be the log
checked against itself.

## The schema and header, as a run wrote them

```json
{"type":"schema","records":{"frame":["frame","t_start","frame_ns","t_present","missed","resident_bytes","texture_bytes"],"sampled":["t","resident_bytes","texture_bytes","handles","temperature_c"]}}
{"type":"header","build_id":"++UE5+Release-5.8-CL-56702186","platform":"Windows","rhi":"D3D12","width":1280,"height":720,"clock":"monotonic_nanoseconds","target_fps":60,"present_source":"end_of_render_thread_frame","missed_frame_rule":"frame_ns > 1.5 * (1e9 / target_fps)","frame_memory_source":"most_recent_sampled_row, not a per-frame syscall","temperature_source":"not measured: no in-process source on this platform, see PLAN 6.5"}
{"type":"sampled","t":160722048742400,"resident_bytes":944205824,"texture_bytes":69570560,"handles":3358,"temperature_c":null}
{"type":"frame","frame":1,"t_start":160722095503800,"frame_ns":21492400,"t_present":160722115017899,"missed":false,"resident_bytes":944205824,"texture_bytes":69570560}
```

The schema is in the file and not only in a document, so the validator checks a log against what
that run said it would write rather than against a copy of the truth kept somewhere else.

## Criteria

| # | criterion | result |
| --- | --- | --- |
| 1 | per-frame row count within 1 percent of the frame count | 0.01 percent, against `GFrameCounter` |
| 2 | sampled stream one row per second within 1 percent | 1.0000 s between rows, from the rows' own `t` |
| 3 | a kill at 30 s leaves at least 29 s of readable rows | 30.56 s, one trailing partial line |
| 4 | every row validates against the declared schema, and an undeclared type is rejected | both directions, with two mutated logs |
| 5 | every timestamp present, monotonic and positive | validator, over every log in the gate |
| 6 | the header carries build, platform, RHI, resolution, clock, `present_source`, `missed_frame_rule`, the temperature reason | above |
| 7 | a log that cannot be opened still runs and says why | rendered a capture, logged the reason, exit 0 |
| 8 | `EventLog` covered by doctest without an engine | 16 cases, 4,142 assertions |
| 9 | both CMake suites green with no drop | dashboard-spec 30 / 1,002,201 unchanged; signal-core 171 / 117,088, up from 155 / 112,946 |
| 10 | five existing gates, doctor, Pester, UBT Win64 and Android | see below |

## Files, by PLAN task

| path | task |
| --- | --- |
| `runtime/UnRealDash/Source/SignalCore/{Public/SignalCore,Private}/EventLog.{h,cpp}` | 4.8, `RowBuilder`, `EventRowRing`, `EventLog` |
| `packages/signal-core/tests/test_event_log.cpp` | 4.8, 16 cases |
| `runtime/UnRealDash/Source/UnRealDashCore/Public/UnRealDashCore/DashEventLog.h` | 4.8, the engine-side seam, no `signal_core` name in it |
| `runtime/UnRealDash/Source/UnRealDashCore/Private/Diagnostics/DashEventLog.cpp` | 4.8, platform counters, the sampler thread, the present join |
| `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPackageScreen.{h,cpp}` | 4.8, opens on `BeginPlay`, ticks, closes and reports on `EndPlay` |
| `scripts/validate-event-log.py` | 4.8, the schema, timestamp and cadence checks |
| `scripts/run-event-log-gate.ps1` | 4.8, the seven runtime criteria |
| `docs/build/chunk-17-event-log.md` | 4.8 |

## Decisions worth their own line

**JSON Lines, not a binary format.** 4.8 requires that a run killed at 30 seconds leaves a
*readable* log, and readable should not mean readable by a program that also has to survive the
kill. The cost is measured rather than waved at: see the size section below.

**One writer thread, not a mutex.** The frame path and the 1 Hz sampler are two producers, and a
record is a write and then a newline, so two threads calling the log directly would interleave
into a line no reader can parse. Instead every other thread formats its row into caller-owned
storage and pushes it into an `EventRowRing`, the same single-producer single-consumer shape
`AcquisitionEvents.h` already uses, and the sampler thread is the only thing that touches the
file. The frame path does no locking, no allocation and no syscall.

**The per-frame memory columns come from the most recent sample.** `FPlatformMemory::GetStats` and
`RHIGetTextureMemoryStats` are both system calls and 4.8 asks for the columns rather than for a
fresh reading per frame. The header says `frame_memory_source: most_recent_sampled_row, not a
per-frame syscall`, so nobody reads them as per-frame measurements.

**`present_source` is named, not assumed.** A true present timestamp comes from the RHI's own
completion signal and is not uniformly available. What this records is the end of the render
thread's frame, attributed by `GFrameCounterRenderThread` so the two sides join by frame index
rather than by arrival order. It is earlier than a scanout by the compositor's queue depth, and
the header says which one it is. Chunk 18's present rows carry the same value from the same
capture, so the two cannot disagree.

**Temperature is `not measured` on Windows.** 6.5 names `nvidia-smi`, which is a process per
sample. 6.5 itself provides for a platform with no source, and the column exists from the start so
the schema does not change under 6.6.

## Size, which is the cost of the format

| | |
| --- | --- |
| measured, uncapped windowed run at 545 fps | 93,714 bytes/s, **2.70 GB per 8 hours** |
| a frame row | 171 bytes |
| extrapolated at 60 fps | about 10.3 KB/s, **296 MB per 8 hours** |

The 8-hour soak in 6.6b should run frame-rate limited. Uncapped it is not measuring anything the
soak is about and it writes ten times the log.

## Four things the review found before any of this ran

DeepSeek reviewed the frozen spec. All four findings were accepted and are recorded as revision 2
in `docs/build/chunk-17-event-log.md`:

1. **Two present timestamps that could disagree.** The per-frame row and chunk 18's present row
   both need one and revision 1 defined only the first. One source now, inherited by chunk 18.
2. **The sampled stream had no timestamp column**, which made the monotonicity criterion vacuous
   for it. It has `t` now, and the cadence check reads that column rather than the row count.
3. **Two writers on one unsynchronised `FILE *`**, described above. This was the real one.
4. **Two claims that were not true of the code**: that `EventLog` owns the schema header, and that
   a partial final line is the usual hard-kill loss. Both corrected. The usual loss is the
   unflushed second, with the file ending cleanly on a newline; the torn line is the rarer case,
   and it did happen in the criterion 3 run above.

The review also caught that revision 1 left `launch-to-first-usable-value` and the Android
temperature source unassigned to any of the four chunks. Both are assigned now.

## What running it found that the review and the spec did not

**The first run dropped 7,974 of 11,060 frame rows.** The design had the sampler thread drain the
ring once a second, and an unthrottled windowed run of a trivial document renders at about 920
fps, which is 920 rows a second into a 256-slot ring. The drain is now decoupled from the sample:
the thread wakes twenty times a second to drain and writes its own row on the second, and the ring
is 1024 slots. Zero drops since, at 545 to 600 fps.

This is exactly what the drop counter exists for. A log missing three rows in four, silently,
would have passed a schema check and every count computed from it would have been wrong.

**The header wrote `0 by 0` for the resolution.** `BeginPlay` runs before the game viewport has a
size. It reads `GSystemResolution` now, with the viewport preferred when it has one.

**The gate lied twice before it told the truth**, and both were faults in the gate rather than in
the code:

- The column-rename mutation rewrote the schema record too, leaving the file self-consistent, so
  the validator correctly accepted it. A mutation has to make a row disagree with what its own log
  declared.
- Criterion 7 puts a directory where the log file goes. The launcher's own stale-log cleanup
  deleted it first, so the run opened its log normally and the case passed as a no-op. The setup
  is now asserted before the case runs, and the case keeps the path.
- The killed run left a **child** process holding the log files, because the player spawns one and
  `Stop-Process` on the parent id leaves it running. Kills are by name now, and waited on.
- Criterion 3's thirty seconds were counted from `Start-Process` rather than from the log
  appearing, and the player spends about a second and a half booting. The log spanned 28.63 s and
  failed a 29 s clause for no reason worth chasing.

## Suites and gates

```
signal-core     [doctest] test cases:    171 |    171 passed | 0 failed | 1 skipped
                [doctest] assertions: 117088 | 117088 passed | 0 failed |
dashboard-spec  [doctest] test cases:     30 |     30 passed | 0 failed | 0 skipped
                [doctest] assertions: 1002201 | 1002201 passed | 0 failed |
Pester          Tests Passed: 76, Failed: 0, Skipped: 0
doctor.ps1 -Profile workstation   exit 0
```

signal-core gains 16 cases and 4,142 assertions: the row builder's escaping, its non-finite
handling, its sticky overflow and its balance check; the log's flush cadence under a fake clock, a
backwards clock, a dropped row, a reopen and a double close; the ring's carry, its counted drops,
and two producer threads draining through one writer into a file where every line is whole.

The ring test was watched failing: writing one byte too few into a slot turned it red on
`line.back() == '}'` and green again on restore.

## Not run, and why

- **Anything on the device.** The Android sources for handle count and temperature are chunk 20.
- **The 8-hour soak.** That is 6.6b. This chunk reports the size rate it will cost.
- **`-Smoke` was not extended.** The flag gate covers the command-line surface; this gate covers
  the log. Folding them would make one gate that fails for two unrelated reasons.
