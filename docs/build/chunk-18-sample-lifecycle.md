# Build chunk 18: sample identity, the four correlated row types, and the lifecycle partition

Status: revision 2, written against the tree, rejected once and corrected. Covers the second quarter of PLAN.md task 4.8. Read
PLAN.md (4.8, and 4.3 for the snapshot discipline it rests on), `docs/build/chunk-17-event-log.md`
and `docs/ARCHITECTURE.md` first.

Chunk 17 built the file. This builds the thing the file exists to carry: a record of what happened
to every published sample, from the wire to the screen or to whichever of three fates it met
instead.

## The split, adjusted from chunk 17's table

Chunk 17 announced four chunks. Building this one against the tree moved one boundary, and the
reason is worth stating rather than quietly redrawing:

| chunk | scope |
| --- | --- |
| 17, done | the log file, the schema, per-frame rows, the 1 Hz sampled stream |
| **18, this one** | sample identity, receive/acquisition/submit/present rows, the post-run classifier, and the two lifecycle runs that need no forcing |
| 19 | the four lifecycle runs that need the two sides forced out of step, plus armed-expiry and rule-expiry rows and expiry-to-present latency |
| 20 | the on-screen overlay, asset import rows, the Android sampled-stream sources, launch-to-first-usable-value |

**Why the lifecycle runs move.** PLAN's gate asks for a run with a deliberately delayed present, a
run with submission suppressed, a run killed between an acquire and its submit, and a run that
pauses across a submit while acquisition keeps publishing. Every one of them needs the same new
thing: a way to hold the game thread and the acquisition side apart on purpose. That machinery is
one piece of work, it is the same machinery chunk 19's expiry cases need to make a signal go stale
on cue, and it is not needed at all to write the rows or to classify a normal run. Building it
here would put two unrelated risks in one diff.

What stays here is the part that must be right before any forced run means anything: that the rows
exist, that they correlate, and that the partition is exhaustive on an ordinary run.

## Facts this chunk depends on

- **`registry_.Publish()` publishes a snapshot, not a sample.** `Acquisition.cpp:206` publishes
  once per `Pump`, and `published_` counts publications. The per-sample count is `applied_`,
  incremented at `Acquisition.cpp:173` after each successful `registry_.Apply`. **A published
  sample, for 4.8's purposes, is a sample that `Apply` accepted**, and the partition's four counts
  sum to that. Reading "published" as "publications" would make the denominator the frame count
  and the whole partition meaningless.
- **`Sample` has no identity.** It carries `source`, `seq`, `generation` and timestamps
  (`Sample.h`). `seq` is the source's sequence and is not unique across signals or reconnects.
- **The event sink carries signal ids, not sample ids.** `OnAcquire(frame, at, span<const
  SignalId>)` (`Acquisition.h:9`) is the wrong shape for PLAN's acquisition row, which lists
  "the sample ids present in that snapshot". The signature changes here.
- **`FFrameSnapshot` has aligned `Samples` and `SignalIds`, and a compacted `Present`**
  (`DashAcquisition.h:12-24`). A sample id column has to be aligned with `Samples`, not with
  `Present`, or a consumer cannot tell which sample a row is about. The header already warns about
  exactly this trap for `Present`.
- **`FDashBindingTable::Apply` is `const` and reports nothing.** It walks its entries and pushes
  values into updaters. The present row needs it to say what it rendered, so it gains an out
  parameter. It is the only thing that knows which signals are bound to a visible component, which
  is the distinction between `presented` and `acquired-not-displayed`.
- **The interpolator exists.** `Interpolator.h` is in signal-core. Whether the live path uses it
  is checked in deliverable 4 rather than assumed.

## Deliverables

### 1. A sample id, assigned once, carried everywhere

`Sample` gains `std::uint64_t id`. It is assigned by `AcquisitionPipeline` immediately before
`registry_.Apply` succeeds, from a counter that starts at 1 and never repeats within a run. Zero
means unassigned, so a sample that reached a row without going through the pipeline is visible
rather than plausible.

Assigned in the pipeline rather than by the decoder or the mapping, because those two produce
fields that may be dropped, and an id burned on a dropped field would leave gaps that look like
lost rows. Assigned before `Apply` rather than after, because the registry has to store it.

It travels: registry, snapshot buffers, `SampleQueue`, `FFrameSnapshot`. Everywhere `Sample` goes
already, which is the point of putting it on `Sample` rather than in a side table keyed by
something.

### 2. Receive rows

One per published sample, written when `Apply` accepts it. Columns exactly as 4.8 declares them:
sample id, signal id, receive timestamp, sequence, connection generation, quality, `age_evidence`.

Written from the acquisition thread, so it pushes into its own `EventRowRing` and chunk 17's
single writer drains it. A second ring, not a shared one: `EventRowRing` is single-producer by
construction and the frame ring's producer is the game thread.

**The row is written for every accepted sample, at up to 200 Hz.** That is the rate 4.3 exists to
cope with, and it is the rate that makes the partition worth computing, since most of those
samples never reach a frame.

### 3. Acquisition, submit and present rows

- **Acquisition row**, one per frame, at the snapshot acquire: frame index, acquisition timestamp,
  and the sample ids in that snapshot. `IAcquisitionEventSink::OnAcquire` changes from
  `span<const SignalId>` to `span<const std::uint64_t>` of sample ids.
- **Submit row**, one per frame: frame index, submit timestamp. The seam already exists.
- **Present row**, one per presented frame: frame index, present timestamp, and one entry per
  signal bound to a visible component, carrying the sample id rendered for that signal, the
  rendered quality, a null expiry identifier (chunk 19 fills it), and a null interpolation
  attribution unless deliverable 4 says otherwise.

The present timestamp is **the same value chunk 17 captures**, from the same `OnEndFrameRT` hook
keyed by `GFrameCounterRenderThread`. Not a second capture. Chunk 17 revision 2 A exists because
two present timestamps for one frame would make 6.5's subtraction meaningless.

A present row is written only for a frame that actually presented, which is why it is emitted on
the same join chunk 17 already does rather than at tick time.

### 4. Interpolation attribution, or a recorded reason there is none

4.8 requires the present row to name the sample a displayed value was derived from "when the value
came from the 2.6 interpolator rather than directly from a sample". `Interpolator.h` exists in
signal-core. **This chunk establishes whether the live binding path actually runs it**, and either
fills the field or records, in the report and in the header, that the Stage 0 path renders samples
directly and the column is always null. Writing the column as null without checking would be a
guess wearing a schema.

### 5. The post-run classifier

Extends `scripts/validate-event-log.py` with a `--lifecycle` mode implementing 4.8's partition,
**in the declared precedence order**, over the receive, acquisition, submit and present rows:

1. `presented`
2. `acquired-not-displayed`, with the three sub-reasons 4.8 names told apart: bound to no visible
   component, no submit row, or submitted and the process ended before it presented
3. `superseded-unacquired`
4. `latest-unacquired`

It prints the four counts per signal and the arithmetic check that they sum to the published
count. Precedence is what makes it one state per sample, and the classifier tests in order and
stops, rather than evaluating four conditions and hoping they are disjoint.

It is a post-run pass over the log, never live state. 4.8 is explicit about why: the game thread
holds a snapshot from acquire to submit, and a newer sample arriving inside that interval would
look like supersession of a sample the renderer is already drawing.

## Constraints

- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module.
- The acquisition thread does no file I/O and no allocation per sample. It formats into a fixed
  buffer and pushes into a ring, exactly as the frame path does.
- Changing `Sample` touches the registry, the exchange, the queue and their tests. Both CMake
  suites stay green with no drop in counts; a test that had to change says why in its diff.
- Errors are values.
- No em dashes; no PowerShell 7 only syntax in scripts.

## Non-goals

- The four forced lifecycle runs, expiry rows and expiry-to-present latency. Chunk 19.
- The overlay, asset import rows, Android sources, launch-to-first-usable-value. Chunk 20.
- Whole-run statistics. That is 6.5.

## Pass/fail criteria

1. **Every acquisition row's sample ids and every present row's sample ids resolve to receive
   rows.** This is 4.8's own clause and it is the one that catches an id assigned in two places.
2. **Every published sample ends a 60-second run in exactly one lifecycle state**, assigned in the
   declared precedence order, and the four counts sum to the published count, per signal and in
   total. The published count comes from the receive row count, and the pipeline's own
   `SamplesApplied()` is logged beside it so the two can be compared rather than assumed equal.
3. **A signal present in the registry but bound to no visible component ends every one of its
   samples `acquired-not-displayed`, never `presented`.** A fixture declaring one unbound signal.
   This is the only one of 4.8's lifecycle runs that needs no forcing, so it belongs here.
4. **A sample displayed across several consecutive frames yields exactly one receive-to-present
   observation**, at its earliest presentation, unchanged by the later frames.
5. **The classifier is watched failing.** A log mutated to drop one acquisition row must move at
   least one sample out of `presented`, and the sum check must still hold.
6. **Rows validate against the declared schema** and every timestamp column is present, monotonic
   and positive, for the four new record types as well as chunk 17's.
7. **The interpolation column is either filled or documented as always null with the reason**, in
   the report and in the log header.
8. Both CMake suites green with no drop: dashboard-spec 30 cases and 1,002,201 assertions,
   signal-core 171 and 117,088.
9. All six existing gates still pass, `doctor.ps1 -Profile workstation` exits 0, Pester passes
   with `-CI`, and UBT builds Win64 and Android ARM64.

Reported, not gated: the receive row rate and the new bytes per second, since receive rows at up
to 200 Hz change chunk 17's size figures and 6.6b needs the new number.

## Report format

Files added or changed with their PLAN task; the proof output verbatim; the declared schema as the
run wrote it; the four lifecycle counts with the sum check; which of 4.8's gate clauses this chunk
closes and which are left to 19 and 20; what was not run and why; any deviation from PLAN.md with
the reason.

---

## Revision 2: six things the review found, all accepted

The first revision was rejected, correctly. Four of the six are places where the spec named an
outcome and skipped the seam that would produce it, which is the failure mode a frozen spec exists
to prevent.

### A. The partition denominator was circular

Revision 1 made the receive row count the published count, and only *logged* the pipeline's
`SamplesApplied()` beside it. If the receive ring dropped rows, the denominator would quietly
shrink to match and the sum check would pass over a log missing samples. PLAN requires that
receive rows cover every published sample (`PLAN.md:361`).

**New criterion: `receive row count == AcquisitionPipeline::SamplesApplied()`, exactly.** The
pipeline's own counter is logged at `EndPlay` the way chunk 17 logs `GFrameCounter`, and the gate
compares them. Not within a percent: a dropped receive row is a lost sample, not a rounding error.

### B. The id has to be added on the engine side too, and revision 1 named only signal-core

`FFrameSnapshot` holds `FSignalSample`, not `signal_core::Sample` (`SignalCoreAdapter.h:8-16`,
`DashAcquisition.cpp:239`). Putting an id only on `signal_core::Sample` would leave the present
row unable to name what it rendered. The full list, stated rather than implied:

`signal_core::Sample`, `SignalCoreAdapter`'s `FSignalSample` and `ToEngineSample`, and an id
column on `FFrameSnapshot` aligned with `Samples` rather than with the compacted `Present`.

### C. Changing `OnAcquire` changes three more things

`AcquisitionEvent::present` is `std::array<SignalId, Signals>` (`AcquisitionEvents.h:12`) and
`RingAcquisitionEventSink::OnAcquire` overrides the old signature (`AcquisitionEvents.h:49`). The
caller passes `Out.Present`, a compacted signal-id array (`DashAcquisition.cpp:247`). All three
change with the interface, and `FFrameSnapshot` gains the sample-id array the caller will pass.

### D. There was no seam to emit a receive row through, and none to emit a present row

Revision 1 said the acquisition thread "pushes into its own `EventRowRing`" and that the present
row is emitted "on the same join chunk 17 already does", and specified neither path.

- **Receive.** `IAcquisitionEventSink` gains `OnReceive(SignalId, const Sample&)`, called from
  `AcquisitionPipeline` where `applied_` is incremented. `FDashAcquisition` gains
  `SetEventLog(FDashEventLog*)` and its sink forwards to it. Both types are in UnRealDashCore, so
  nothing crosses the layering rule.
- **`FDashEventLog` gains a second ring and typed writers.** `WriteReceive`, `WriteAcquire`,
  `WriteSubmit` and `WriteRendered`, each building its own row, one ring for the acquisition
  thread and one for the game thread. Typed rather than a raw byte push, so `signal_core` stays
  out of the public header and row formatting stays in one place.
- **Present.** `FDashBindingTable::Apply` gains an out parameter listing what it rendered per
  bound signal. The screen hands that to `WriteRendered(frame, rendered)`, which stashes it beside
  the pending frame; the present row is written inside the join chunk 17 already runs, when that
  frame's present timestamp arrives from the render thread. One join, one present timestamp.

### E. A bound signal that has never received a sample

Revision 1 said "one entry per signal bound to a visible component" and separately that every
present-row sample id must resolve to a receive row. Those contradict for a signal that is bound
but has not received anything yet, which is every signal for the first frames of every run.

**The entry is present and its sample id is null.** A bound signal with no sample renders the
missing-data presentation, which is a real thing on screen and belongs in the row. Criterion 1
applies to non-null ids. Omitting the entry instead would make "one entry per bound signal" false
and would hide the first seconds of every run from the reader.

### F. The validator does not know the new timestamp columns

`validate-event-log.py` hard-codes the set it checks. `t_acquire` and `t_submit` join it, or
criterion 6 passes without checking anything.

### G. What this chunk cannot prove, stated plainly

The review is right that four of 4.8's lifecycle branches are unreachable without chunk 19's
forcing machinery: no submit row, submitted and never presented, a kill between acquire and
submit, and a sample held across a pause while newer ones are published. **PLAN 4.8 is not
complete at the end of this chunk**, and the chunk 20 report carries the clause-by-clause map. The
`acquired-not-displayed` sub-reason this chunk does reach is the bound-to-nothing one.
