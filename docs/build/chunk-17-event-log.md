# Build chunk 17: the event log file, per-frame rows and the sampled stream

Status: revision 2, written against the tree, corrected after review. Covers the first quarter of PLAN.md task 4.8. Read
PLAN.md (4.8, and 6.5 and 6.6 for who consumes this) and `docs/ARCHITECTURE.md` first.

## Why 4.8 is four chunks

PLAN 4.8 asks for five groups of record types, an exhaustive four-state partition over every
published sample, two latency definitions with their clock domains, and an on-screen overlay. Its
gate paragraph alone specifies twenty-two separate runs. Built as one chunk it would be a diff
nobody could review and a gate that goes red for reasons nothing in the diff names.

The split follows what each part needs from the tree, not what is convenient:

| chunk | part of 4.8 | why it can stand alone |
| --- | --- | --- |
| **17, this one** | the log file, its schema, the per-frame rows, the 1 Hz sampled stream | needs no new identity in signal-core. A frame already has an index and a time. |
| 18 | sample identity, receive/acquisition/submit/present rows, the four-state lifecycle partition, **launch-to-first-usable-value** | needs a sample id that does not exist yet, and changes the `IAcquisitionEventSink` signature. First-usable means the first frame in which a bound signal rendered `valid`, which is a present row. |
| 19 | armed-expiry and rule-expiry rows, expiry-to-present latency | needs 18's present rows for its end endpoint |
| 20 | the on-screen overlay, asset import rows, **the Android sources for the sampled stream** | reads what 17 to 19 produce; the overlay's rolling percentiles feed nothing downstream |

Each chunk's gate is a subset of 4.8's gate paragraph, and the chunk 20 report will state which of
4.8's gate clauses each chunk closed, so nothing falls between them.

## Facts this chunk depends on

- **The acquire and submit seam already exists and is dormant.** `IAcquisitionEventSink` declares
  `OnAcquire(frame, at, present)` and `OnSubmit(frame, at)` (`Acquisition.h:7-15`),
  `AcquisitionPipeline` calls both, and `AcquisitionEvents.h` already has a single-producer
  single-consumer ring and a sink built on it. `FDashAcquisition` binds a
  `NullAcquisitionEventSink` (`DashAcquisition.h:59`), so the events fire into nothing today.
  **This chunk does not touch that seam.** Chunk 18 does, because the acquisition row PLAN
  specifies carries sample ids and `OnAcquire` carries `SignalId`s.
- **signal-core already does file I/O.** `FileTransport` opens a `std::FILE` in binary mode and
  reads in bounded chunks (chunk 16). Exceptions and RTTI are off, so errors are values.
- **There is no sample id.** `Sample` carries `source`, `seq`, `generation` and timestamps
  (`Sample.h`) and nothing that identifies one published sample across threads. Chunk 18's problem.
- **`FDashAcquisition` owns the monotonic clock** through `FMonotonicClock` and hands
  `signal_core::Time` nanoseconds across the boundary. The event log uses the same clock, which is
  what lets 6.5 subtract one of its timestamps from another.
- **The player writes to `FPaths::ProjectSavedDir()`**, which chunk 16's resolver already logs as
  an absolute path, and which is where the device run finds its storage.

## Deliverables

### 1. `EventLog`, in signal-core

Engine-independent, so it is covered by doctest and behaves the same on the device. It owns the
file, the append path and the flush cadence, and nothing else. It owns no record type's meaning,
including the schema and header records: it cannot know which record types a run can write. The
owner writes those two first and flushes them before any other record. See revision 2 D.

**One thread writes.** `EventLog` has no lock, and a record is a write and then a newline, so two
threads calling it would interleave into a line no reader can parse. Every other thread formats
its row with `RowBuilder` into caller-owned storage and pushes it into an `EventRowRing`, the same
single-producer single-consumer shape `AcquisitionEvents.h` already uses; the sampler thread
drains the rings once a second and is the only thing that touches the file. The frame path
therefore does no locking, no allocation and no syscall, which a mutex on it would not have given.
See revision 2 C.

**Format: JSON Lines, one record per line, with a schema header as the first lines.** The
alternatives were considered and rejected:

- **A binary format** would be a third the size and unreadable after a hard kill without a tool
  that also has to survive the kill. 4.8's gate requires that a run killed at 30 seconds leaves a
  readable log, and "readable" should not mean "readable by a program we also have to trust".
- **One CSV per record type** gives the cleanest column schema and five files to flush, five files
  to truncate consistently at a kill, and no single artifact to hand to a reader. The five groups
  have to be correlated by frame index anyway, so splitting them across files buys nothing.

JSON Lines costs size and this chunk measures it rather than waving at it: the report carries bytes
per second at 60 fps, and the 8-hour soak figure extrapolated from it, so 6.6b knows what it is
committing to before it runs.

**The schema is declared in the file, not just in this document.** The first record is
`{"type":"schema","records":{"frame":["frame","t_start",...],...}}`, one entry per record type the
run can write, listing its columns in order. A reader validates every later row against it and a
row of an undeclared type is an error. That is what makes "each record type validates against its
declared column schema" checkable by a tool rather than by eye.

The second record is `{"type":"header",...}`: build id, platform, RHI name, resolution, clock
domain, and one `uncertainty` entry per measurement whose endpoint is not exact. 6.5 puts these in
its table header and must not have to guess them.

**Flush at least once per second**, and on every schema and header record. PLAN's reason is the
one that matters: a crash or a kill has to leave the evidence up to that second rather than an
empty file. Flushing every row would put a syscall on the frame path; flushing never would make
the crash-survivability clause meaningless.

**What a hard kill actually costs.** Between flushes, whole records sit in the stdio buffer, so
the usual loss is up to one second of complete records and the file ends cleanly on a newline. A
truncated final line is also possible, when the kill lands inside a write. Both are tolerated: the
validator drops at most one trailing incomplete line and reports that it did, and two would mean
something worse than a kill happened. Criterion 3 is about the second of rows that survives, not
about the shape of the last one.

### 2. Per-frame rows

Columns exactly as 4.8 lists them: frame index, frame start, frame time, present timestamp,
missed-frame flag, resident memory, texture memory.

Two of these need a definition rather than a lookup, and both go in the schema header so a reader
is never guessing:

- **Present timestamp.** A true present timestamp comes from the RHI's own completion signal and
  is not uniformly available. What this chunk records is the end of the render thread's frame,
  through `FCoreDelegates::OnEndFrameRT`, labelled `present_source: "end_of_render_thread_frame"`
  in the header. It is earlier than a scanout by the compositor's queue depth. Calling it a
  present timestamp without saying which one it is would put an unstated bias into every
  receive-to-present figure 6.5 reports.
- **Missed frame.** Defined here and written into the header as `missed_frame_rule`: a frame is
  missed when its frame time exceeds 1.5 times the presentation interval implied by the target
  frame rate. The rule is stated in the file because "missed frames: 4" means nothing without it.

### 3. The 1 Hz sampled stream

One row per second for the life of the process, driven by its own timer rather than by the frame
loop, because 6.6's whole point is that a run that stalls or drops to 5 fps still produces a trace.

| column | Windows source | Android source |
| --- | --- | --- |
| `t`, the timestamp | the same monotonic clock as every other record | same |
| resident memory | `FPlatformMemory::GetStats().UsedPhysical` | same |
| texture memory | `RHIGetTextureMemoryStats` | same |
| handle count | `GetProcessHandleCount` | `/proc/self/fd` entry count |
| temperature | **`not measured`** | `dumpsys thermalservice`, chunk 20 |

**Temperature is `not measured` on Windows, with the reason in the header.** 6.5 names `nvidia-smi`
as the Windows source, which is an out-of-process tool the app cannot run every second without
spawning a process per sample. 6.5 already provides for this: "Where a platform exposes no source
for a cell, the harness writes `not measured` into that cell and records why in the header." The
harness, not the app, is the right place for `nvidia-smi`, and 6.5 is where that lands. The column
exists from the start so the schema does not change under 6.6.

### 4. `scripts/validate-event-log.py`

Reads a log and exits non-zero on: a row of a type the schema does not declare, a row whose columns
do not match its declared schema, a missing, non-positive or non-monotonic timestamp, more than one
trailing partial line, or a missing schema or header record. It prints per-type row counts, the
elapsed span, and bytes per second, which is what the gate and the report both read.

It is a tool rather than a gate step so that 6.5, 6.6 and 6.6b can all run it over their own logs.

## Constraints

- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module.
- `EventLog` takes no dependency on Unreal. The engine passes values in.
- Errors are values. A log that cannot be opened is a logged error and a run that still runs: a
  dashboard that refuses to start because it could not write telemetry about itself is worse than
  one that starts without it. The header of a run with no log says so in the player log.
- The frame path does no allocation and no syscall per row. Rows are formatted into a fixed buffer
  and appended; the flush is on the second.
- No em dashes; no PowerShell 7 only syntax in scripts.

## Non-goals

- Sample identity, receive rows, acquisition rows carrying sample ids, present rows, the lifecycle
  partition, launch-to-first-usable-value. All chunk 18.
- Expiry rows and expiry-to-present latency. Chunk 19.
- The on-screen overlay and asset import rows. Chunk 20.
- Whole-run statistics. That is 6.5 reading these rows, and it is deliberately not this chunk:
  computing them here would mean two places that know what p95 means.

## Pass/fail criteria

1. **A 60-second run writes a log whose per-frame row count matches the frame count within 1
   percent.** The frame count is read from the engine, not from the row count, or the check is
   circular.
2. **The sampled stream has one row per second within 1 percent of the elapsed seconds**, measured
   from its own `t` column rather than from the row count, so a stream that wrote the right number
   of rows at the wrong cadence fails.
3. **A run killed at 30 seconds leaves a readable log** containing at least 29 seconds of rows in
   both the per-frame type and the sampled stream, with at most one trailing partial line.
4. **Every row validates against the schema declared in the file**, and the validator rejects a log
   containing a row of an undeclared type. Both directions are checked, with a mutated log for the
   second.
5. **Every timestamp column is present, monotonic and positive.**
6. **The header carries** build id, platform, RHI, resolution, clock domain, `present_source`,
   `missed_frame_rule`, and the reason temperature is `not measured` on this platform.
7. **A run whose log path cannot be opened still runs**, renders, and logs why there is no log.
8. `EventLog` is covered by doctest without an engine: schema and header emission, append, the
   flush cadence under a fake clock, a bounded write into a fixed buffer, and a truncated file
   reopened.
9. Both CMake suites green with no drop: dashboard-spec 30 cases and 1,002,201 assertions,
   signal-core 155 and 112,946.
10. All five existing gates still pass, `doctor.ps1 -Profile workstation` exits 0, Pester passes
    with `-CI`, and UBT builds Win64 and Android ARM64.

Reported, not gated: bytes per second at 60 fps and the extrapolated 8-hour size; frame time with
the log on and off. PLAN says run-to-run percentile stability is not a gate and that holds here.

## Report format

Files added or changed with their PLAN task; the proof output verbatim; the declared schema as the
run wrote it; the size figures; which of 4.8's gate clauses this chunk closes and which are left to
chunks 18 to 20; what was not run and why; any deviation from PLAN.md with the reason.

---

## Revision 2: four things the review found, all accepted

### A. Two present timestamps that could disagree

PLAN puts a present timestamp in the per-frame row (`PLAN.md:340`) and another in the present row
(`PLAN.md:345`). Revision 1 defined the source for the first and said nothing about the second,
which chunk 18 writes. If chunk 18 had taken its timestamp from a different callback, the same
frame would carry two present timestamps and 6.5's receive-to-present subtraction would be
computed across two clocks that mean different things.

**One source, defined here, inherited by chunk 18.** The present timestamp is captured once per
frame on the render thread at `FCoreDelegates::OnEndFrameRT`, attributed to the frame index in
`GFrameCounterRenderThread`, and both the per-frame row and chunk 18's present row carry that same
value. The header's `present_source` names it for the reader. Chunk 18 does not get to pick.

### B. The sampled stream had no timestamp column

Criterion 5 requires every timestamp column to be present, monotonic and positive, and the sampled
stream's column list had no timestamp at all, so the criterion was vacuous for it. The row now
begins with `t`, on the same monotonic clock as every other record, and the 1 percent check on row
count is over that column rather than over the row count alone.

### C. Two writers on one unsynchronised `FILE *`

The larger miss. Revision 1 created a frame writer and a 1 Hz sampler and never said which thread
owned the file. A record is a write and then a newline, so two threads interleave into a line no
reader can parse, and `stats_` and the flush deadline would both be data races. The "one trailing
partial line" model would not have covered it: the corruption lands in the middle of the file.

**Resolved by design rather than by a lock.** The 1 Hz sampler thread is the sole writer. Every
other thread formats its row with `RowBuilder` and pushes it into an `EventRowRing`, the same
single-producer single-consumer shape as `AcquisitionEventRing`; the writer drains the rings once
a second, writes, and flushes. The frame path therefore does no locking, no allocation and no
syscall, which a mutex would not have given it, and the one-second flush ceiling is exact by
construction rather than by a check on every record.

A row that does not fit a slot, and a ring that is full, are counted drops. A log silently missing
rows would make every count computed from it wrong, and chunk 18's lifecycle partition sums counts
for a living.

### D. Two claims that were not true of the code

- Revision 1 said `EventLog` owns the schema header. It does not and should not: it cannot know
  which record types a run can write. **The owner writes the schema and header records first and
  flushes them before any other record**, and `EventLog` guarantees only the one-second ceiling.
  The class comment and this document now say the same thing.
- Revision 1 described the hard-kill loss as a partial final line. That is possible but it is not
  the common case: complete records sit in the stdio buffer between flushes, so a kill usually
  loses up to one second of whole records and the file ends cleanly on a newline. Both are
  tolerated, the validator still drops at most one trailing partial line, and criterion 3 is about
  the second of rows that survives rather than about the shape of the last one.

### E. What this chunk's criteria do NOT close, stated rather than implied

Three of PLAN 4.8's gate clauses are partly this chunk's and partly a later one's. Revision 1 read
as though they were closed here:

| PLAN clause | closed here | left to |
| --- | --- | --- |
| a kill at 30 s leaves 29 s of rows in every record type | per-frame and sampled | 18 for receive/acquisition/submit/present |
| each record type validates against its declared schema | the types this chunk emits | 18, 19 and 20 for theirs |
| every timestamp column present, monotonic, positive | the same | the same |

The chunk 20 report carries the full map of 4.8's gate clauses to the chunk that closed each, so
none of them can fall between the four.
