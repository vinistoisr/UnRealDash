# Build chunk 20: the debug overlay, asset import rows, and launch-to-first-usable-value

Status: revision 2, written against the tree, reviewed and corrected. Covers the last quarter of PLAN.md task 4.8. Read
PLAN.md (4.8, and 6.5 and 6.7 for who consumes this), the chunk 17, 18 and 19 specs, and
`docs/ARCHITECTURE.md` first.

Chunks 17 to 19 built the log. This builds the two record types that were left, the overlay that
reads none of them, and the one measurement whose clock domain PLAN spends a paragraph on. It also
closes 4.8 by mapping every clause of its gate paragraph to the chunk that satisfied it.

## Facts this chunk depends on

- **`DecodePng` is the one runtime asset import path** (`ImagePrimitive.cpp:21-48`). It calls
  `IImageWrapperModule`, `SetCompressed`, `GetRaw`, `CreateTransient` and `UpdateResource`. Every
  timestamp PLAN's asset import row asks for is a point in that function, except the last.
- **`UpdateResource` is asynchronous.** It enqueues work on the render thread, so the call
  returning is not the texture being usable. PLAN asks for "the first-usable timestamp taken from
  the RHI's own completion signal", which on this path means a render command enqueued after
  `UpdateResource` and timestamped when it runs. That is a real completion signal for this
  resource, and it is later than the upload rather than earlier, which is the direction that
  matters.
- **The event log already has a wide ring and typed writers** (chunk 18), a sampler thread that
  reads platform counters (chunk 17), and a present join that owns the one present timestamp
  (chunk 19). Nothing here needs a new thread or a new ring.
- **The sampled stream's Android columns are still Windows-only.** Handle count is
  `GetProcessHandleCount` under `PLATFORM_WINDOWS` and temperature is null with a recorded reason.
- **The documented flag surface is frozen** (chunk 16). Anything this chunk needs on the command
  line is a hidden gate switch, declared to the resolver like the rest.
- **Nothing in the tree draws an overlay.** The two matches for "overlay" are unrelated comments.

## Deliverables

### 1. Asset import rows

One row per runtime-imported asset, exactly 4.8's columns: asset identifier, byte size, declared
pixel dimensions, decode start and decode end timestamps, upload start timestamp, and the
first-usable timestamp from the RHI's completion signal.

The identifier is the package-relative asset name, which is what the manifest and the document
both use, so 6.7 can join a row to the asset it is about without a second mapping.

**Import cost is a per-asset fact and belongs to the asset**, not to whichever frame happened to
be in flight. The row is written when the completion signal fires, which is after the first-usable
timestamp exists, so no row is ever written with a field it had to guess.

Decoding happens during package load, before the event log exists in the current order of
`BeginPlay`. **Deliverable 1a is to establish whether that is true and, if it is, to move the log
open before the screen is built** so the rows are not silently lost. An asset import record type
that is always empty because of call order would be worse than not having it.

### 2. The Android sampled-stream sources

- **Handle count**: the entry count of `/proc/self/fd`, read on the sampler thread.
- **Temperature**: the NDK thermal API where available, otherwise the first readable
  `/sys/class/thermal/thermal_zone*/temp` whose `type` names a CPU or GPU zone, with the chosen
  zone written into the header. `dumpsys thermalservice` is what 6.5 names for the harness; the
  app cannot spawn a process per sample, and 6.5 already provides for a platform with no
  in-process source.

Both are compiled for Android and **cannot be verified on this workstation.** The report says so
rather than implying a device run happened.

### 3. Launch-to-first-usable-value

**First usable** means the first present callback for a frame in which at least one bound signal
rendered a `valid` value. Chunk 18's present row already carries the rendered quality per bound
signal, so this is the first present row containing a `valid` entry, and the runtime emits a
`first_usable` record naming that frame and its present timestamp.

**On Windows the start endpoint is the launch request, measured launcher-side.**
`capture-metrics.ps1` reads the performance counter immediately before starting the process and
passes it in through a hidden switch; the app reads the same system-wide counter at its
first-usable-frame event and writes both values and the frequency into the log. Nothing is
converted, because both endpoints sit on one counter. The endpoint is labelled launch request
rather than process creation, since it is earlier by the cost of starting the process.

**The stated uncertainty goes in the header**: on Windows, the launcher-side overhead the endpoint
includes, measured as the spread between the counter read and `Start-Process` returning.

The Android endpoint is `/proc/self/stat` field 22 on the boot-time clock, read in-process, with
`am start -W` recorded as auxiliary evidence only. Written and not verifiable here.

### 4. The on-screen overlay

fps, rolling frame time p95 and p99, missed frames, resident memory, texture memory, and
per-signal freshness state. A UMG widget over the document, off by default, toggled by a hidden
gate switch.

**Its percentiles are its own and feed nothing.** 4.8 says so and 6.5 depends on it: a rolling p95
cannot be re-aggregated into a run p95, which is the entire reason the raw rows exist. The overlay
therefore computes from its own ring buffer and writes nothing to the log, and the report states
that no code path carries an overlay number into a record.

The overlay must not perturb what the capture gates measure, so it is off unless asked for, and
the gate proves a run without it is byte-identical to one before this chunk.

### 5. The 4.8 clause map

A table in the report mapping every clause of 4.8's gate paragraph to the chunk that closed it,
the gate that proves it, or a recorded reason it is not closed. 4.8 has about thirty clauses
spread over four chunks and nothing so far has checked that none fell between them.

## Constraints

- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module.
- The overlay allocates nothing per frame and is skipped entirely when off.
- Asset import timestamps come from the same monotonic clock as every other row.
- Errors are values. A missing thermal zone is a null column and a header note, not a failure.
- No em dashes; no PowerShell 7 only syntax in scripts.

## Non-goals

- Whole-run statistics and the metrics table. That is 6.5 reading these rows.
- Device verification of anything. 4.11 and 6.x.
- The two-disconnect-episode latency run, which chunk 19 recorded as belonging with the connector
  gate.

## Pass/fail criteria

1. **Every runtime-imported asset has exactly one import row**, carrying decode start and end,
   upload start and first-usable timestamps, all positive and in that order. A fixture with two
   distinct images produces two rows with distinct identifiers.
2. **The first-usable timestamp is later than the upload start**, for every row. If the
   completion signal were being read before the work completed, this is what would catch it.
3. **A `first_usable` record exists** and names a frame whose present row contains at least one
   entry with rendered quality `valid`, verified by the classifier against the present rows rather
   than trusted.
4. **The launch record carries both counter values and the frequency**, and the derived
   launch-to-first-usable-value is positive and under 30 seconds on this workstation. The figure
   itself is reported, not gated: it is a cold-start time and varies with the machine.
5. **The header carries the stated uncertainty** for the Windows launch endpoint, measured rather
   than asserted.
6. **The overlay draws** and shows all six quantities, proven by a capture in which the overlay
   region is not the background colour. **With the overlay off the existing capture gate passes
   unchanged**, and no overlay widget is constructed or added to the tree, so "off" cannot mean
   "collapsed but present". See revision 2 D; byte identity is not what that gate measures.
7. **No overlay number reaches the log.** Checked by the absence of any call path from the
   overlay's ring buffer into a writer, stated in the report with the grep that shows it.
8. Both CMake suites green with no drop: dashboard-spec 30 cases and 1,002,203 assertions,
   signal-core 179 and 117,162.
9. All eight existing gates still pass, `doctor.ps1 -Profile workstation` exits 0, Pester passes
   with `-CI`, and UBT builds Win64 and Android ARM64.
10. **The 4.8 clause map is complete**: every clause of the gate paragraph is either closed and
    named, or listed as open with a reason.

## Report format

Files added or changed with their PLAN task; the proof output verbatim; the asset import rows as a
run wrote them; the launch figure with its uncertainty; the 4.8 clause map; what was not run and
why; any deviation from PLAN.md with the reason.

---

## Revision 2: six clauses with no home, and one criterion that cannot be proven

### A. Two facts checked before the review, both as the spec assumed

- **Assets really are decoded before the log opens.** `DecodePng` has one call site
  (`ImagePrimitive.cpp:80`), inside the image builder, which runs from `Builder.Build` in
  `RebuildWidget`, which `AddToViewport` triggers at `DashPackageScreen.cpp:380`. The log opens at
  line 399. Every import row would have been lost. **The log open moves above `Screen->Open`.**
- **`FPlatformTime::Cycles64()` on Windows is literally `QueryPerformanceCounter`**
  (`WindowsPlatformTime.h:39-44`), and PowerShell's `Stopwatch::GetTimestamp` is the same counter.
  The "nothing is converted" claim holds.

There is exactly one runtime asset import path, so no second one is unaccounted for.

### B. Four clauses of 4.8's gate had fallen between chunks

The review read the gate paragraph clause by clause against all four chunk specs. These had no
home and are now **this chunk's criteria**, because it is the last one:

1. **The 30 second kill must leave 29 seconds of receive, acquisition, submit and present rows**,
   not only per-frame and sampled rows. Chunk 17 gated its half and said the rest was chunk 18's;
   chunk 18 never picked it up. New criterion 11.
2. **Every rule-expiry row names a rule declared in the loaded document and a kind of `hold_last`
   or `debounce`**, and has exactly one status row. Chunk 19 covered freshness expiries and left
   the rule half to doctests. The classifier now applies both checks to whatever rule-expiry rows
   a log contains, and the report states that the Stage 0 player arms none, so the clause is
   satisfied by a check that would bite rather than by absence. New criterion 12.
3. **Each record type validates against its declared column schema**, for the types chunks 19 and
   20 added as well. The validator already checks this generically; nothing gated it. New
   criterion 13, over a log containing every record type.
4. **Every timestamp column present, monotonic and positive**, for those same types. `t_fired` is
   null on a cancellation and is checked where it must exist rather than always. Same criterion.

### C. The Android launch clause was underspecified

PLAN wants, on Android, the in-process `/proc/self/stat` field 22 value **and** `am start -W`'s
output **with its launch state**, plus the platform's stated uncertainty, which is the tick
granularity of field 22 and the observed spread against `am start -W`.

Revision 1 said only that the Android endpoint is "written and not verifiable here". The record
now carries all four fields, the header carries the Android uncertainty text, and the report lists
the clause as **written but unverified**, because the spread against `am start -W` cannot be
observed without a device. That is a recorded open item, not a closed one.

### D. Criterion 6 asked for something the capture gate does not do

It required a capture with the overlay off to be "byte-identical" to one from before this chunk.
`run-capture-gate.ps1` does not compare bytes: it feeds PNGs to `compare-capture.py`, which
decodes to 8 bit RGB, drops alpha and passes within `worst <= 48` and `moved <= 0.001`. The gate's
own header says GPU, driver and font rasterisation all move those numbers. Byte identity is a
stricter and different claim, and one nothing in this project has ever established.

**Criterion 6 is rewritten**: with the overlay off, the existing capture gate passes unchanged,
which is exactly the guarantee that matters and is already measured every run. And the constraint
is made testable rather than aspirational: with the overlay off **no overlay widget is
constructed and none is added to the tree**, so "not shown" cannot quietly mean "collapsed but
present in the layout". The report states which of the two it is.

### E. A contradiction between two of my own specs

Chunk 19 listed the two-disconnect-episode latency run as its criterion 5c and then did not gate
it; chunk 20's non-goals said it belongs with the connector gate. Both cannot be right. **The
clause map resolves it as open**, assigned to the connector gate, with the reason: it needs a
fixture that reconnects twice, and the machinery for that is the relay the connector gate already
drives rather than anything in the event log.

## Added criteria

11. **A run killed at 30 seconds leaves at least 29 seconds of rows in the receive, acquisition,
    submit and present record types**, as well as the two chunk 17 already gates.
12. **Every rule-expiry row names a declared rule and a kind of `hold_last` or `debounce`, and has
    exactly one status row.** Checked by the classifier over any log; the report states that the
    Stage 0 player arms none and that the check was watched biting on a synthetic log.
13. **Every record type in a log containing all of them validates against its declared schema, and
    every timestamp column present, monotonic and positive**, `t_fired` excepted where the status
    is a cancellation.
