# Build chunk 19: expiry rows, expiry-to-present latency, and the four forced lifecycle runs

Status: revision 2, written against the tree, blocked once and corrected. Covers the third quarter of PLAN.md task 4.8. Read
PLAN.md (4.8, and 2.5 for the rule and freshness expiries it records), the chunk 17 and 18 specs,
and `docs/ARCHITECTURE.md` first.

Chunk 18 left four of 4.8's lifecycle branches unreachable because they need the acquisition side
and the presentation side held apart on purpose. The expiry cases need the same thing. That shared
need is why these are one chunk.

## Facts this chunk depends on

- **An expiry has no identity per arming.** `Expiry` is `{time, kind, id}` (`ExpirySchedule.h`),
  and `id` is the signal id for a freshness expiry. `Arm` cancels any existing entry with the same
  `(kind, id)` and inserts (`ExpirySchedule.cpp:12-13`), so re-arming is already a cancel followed
  by an arm, and nothing can tell the two armings apart. PLAN requires exactly one status row per
  armed expiry, carrying "the same expiry identifier", so identity has to exist first.
- **Every arm, cancel and fire goes through `ExpirySchedule`.** `SignalRegistry::Apply` arms on a
  valid sample and cancels otherwise (`SignalRegistry.cpp:74,78`), `Expire` cancels when it marks
  a sample stale (`SignalRegistry.cpp:94`), `Reset` clears the whole schedule
  (`SignalRegistry.cpp:31`), and `AcquisitionPipeline::Pump` drains due entries with `PopDue`
  (`Acquisition.cpp:196`). **The schedule is therefore the one seam that sees all three events**,
  which is where the sink goes.
- **`Expire` marks stale and cancels, and `PopDue` also fires.** Both run in one `Pump`. A
  deadline that passes is observed by whichever runs first, so the rows have to be emitted from
  the schedule rather than from either caller, or the same expiry gets two status rows or none.
- **The schedule carries no sample id, generation or resulting quality.** PLAN's armed-expiry row
  wants all three, so `Expiry` grows to carry them rather than the sink guessing.
- **Rule expiries are not armed yet.** `ExpiryKind` declares `hold_last` and `debounce` and
  `RuleEngine` has a `debounce` duration (`RuleEngine.h:44`), but nothing in the tree arms an
  entry of either kind. Deliverable 5 establishes that rather than inventing rows for something
  that never happens.
- **The player has no way to hold the two sides apart.** Every forced run in 4.8's gate needs one.

## Deliverables

### 1. Expiry identity and one seam for all three events

`Expiry` grows: a `serial` unique per arming, the `sample` id and `generation` that armed it, and
the `quality` it transitions to when it fires. `ExpirySchedule` assigns the serial from its own
counter, starting at 1 so zero keeps meaning unassigned, exactly as the sample id does.

`IExpiryEventSink`, owned by the schedule:

- `OnArmed(const Expiry&)`
- `OnFired(const Expiry&, Time at)`
- `OnCancelled(const Expiry&, CancelReason)` where the reason carries the sample id that re-armed
  it, the generation that ended it, or `run_end`.

The sink is optional and defaults to nothing, so every existing construction keeps working and a
run with no event log costs nothing.

**Exactly one status row per armed expiry is a property of the schedule, not of the caller.** Arm
cancels before it inserts, so the cancelled row is emitted there; `PopDue` fires; `Cancel` and
`Clear` cancel. There is no path that removes an entry without going through one of them, which is
what makes the one-row rule structural rather than a convention four callers have to remember.

### 2. Armed-expiry and rule-expiry rows

Freshness expiries, per 4.8: an expiry identifier, signal id, the sample id and connection
generation that armed it, the armed deadline timestamp, and the quality it transitions to. Then
exactly one status row with the same identifier: `fired` with its timestamp, or `cancelled` with
the sample id that re-armed it or the generation that ended it.

**Orderly shutdown writes a `cancelled` row with reason `run_end` for every still-armed expiry**,
and flushes it before exit. After a hard kill, an armed expiry with no status row is `unresolved`,
excluded from observations and reported as a count.

Rule expiries are a separate record type with rule id, entry kind, deadline and status, and they
name no sample and are never latency endpoints. See deliverable 5.

### 3. Expiry-to-present latency

Computed by the classifier, not by the runtime, from exactly two record types:

- **Start**: the `fired` status row of a freshness expiry. A `cancelled` expiry produces no
  observation, so a deadline moved by a later arrival cannot yield a fictitious interval.
- **End**: the first present row after that firing whose entry for that signal names **this expiry
  identifier** with rendered quality `stale`. Matching by signal alone would let a later episode's
  frame close an earlier expiry.

A `fired` expiry that no present row names is `fired-not-displayed`, counted separately and never
an observation, **decided after the run over the whole trace with no cutoff at the next valid
sample**, because a frame that acquired the stale snapshot before recovery still presents it
afterwards and that delayed presentation is exactly the latency being measured.

This requires the present row's `expiry` column, which chunk 18 writes as null. The binding table
learns which expiry a stale reading is displaying, from the snapshot.

### 4. The forcing machinery

Four gate-only switches, in the hidden namespace chunk 16 established, so the documented surface
does not grow:

| switch | what it does |
| --- | --- |
| `-udash-suppress-submit` | the game thread acquires and never records a submit |
| `-udash-delay-present-ms=<n>` | holds each frame's rendered set for `n` ms before the present row can close |
| `-udash-pause-at=<t>` and `-udash-pause-ms=<n>` | stops the game thread between acquire and submit while acquisition keeps publishing |
| `-udash-kill-at=<t>` | exits hard, with no orderly shutdown, at a scenario timestamp |

Every one is rejected by the resolver if misspelled, like the rest of the hidden set. They are
declared in the log header so a reader of a forced run knows it was forced.

### 5. Rule expiries, or a recorded reason there are none

Nothing in the tree arms a `hold_last` or `debounce` entry today. **This chunk establishes
whether the Stage 0 rule path ever does**, and either emits the rows or records, in the report and
the header, that no rule expiry is ever armed in Stage 0 and the record type is declared but
empty. Declaring a record type that can never appear, without saying so, would leave a reader
waiting for rows that are not coming. This is the same discipline chunk 18 applied to the
interpolator.

## Constraints

- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module.
- `ExpirySchedule` keeps its caller-owned storage and adds no allocation.
- The sink is optional; no existing construction changes behaviour without one.
- Errors are values.
- No em dashes; no PowerShell 7 only syntax in scripts.

## Non-goals

- The overlay, asset import rows, Android sources, launch-to-first-usable-value. Chunk 20.
- Whole-run statistics. 6.5.

## Pass/fail criteria

1. **Every armed-expiry row has exactly one status row** in an orderly run, and the identifiers
   match. Checked by the classifier over a real run, not by construction.
2. **A clean exit with a deadline still in the future produces a `cancelled` row with reason
   `run_end`** for it.
3. **A run that re-arms an expiry before it fires produces a `cancelled` row naming the sample
   that re-armed it, and no expiry-to-present observation.** This is the ordinary case at any
   sample rate above the deadline, so it needs no forcing.
4. **A run killed after an armed row is flushed but before its deadline reports that expiry as
   `unresolved`**, excluded from observations and counted.
5. **Expiry-to-present latency yields one observation per displayed firing**, attributed by expiry
   identifier, and `fired-not-displayed` is counted separately.
6. **Every armed-expiry row names a signal with at least one receive row and a deadline later than
   that row's receive timestamp.** PLAN's own clause.
7. **The four forced runs each produce the lifecycle branch they exist for**, and in every one the
   four counts still sum to the published count and no sample matches two states:
   - submission suppressed: every sample of those frames ends `acquired-not-displayed`, no submit
   - delayed present: the counts still sum and nothing is presented twice
   - killed between acquire and submit: every published sample still receives a state
   - paused across a submit while newer samples are published: the held sample ends `presented`
8. **Rule expiry rows are emitted or their absence is recorded** with the reason.
9. Both CMake suites green with no drop: dashboard-spec 30 cases and 1,002,203 assertions,
   signal-core 174 and 117,123.
10. All seven existing gates still pass, `doctor.ps1 -Profile workstation` exits 0, Pester passes
    with `-CI`, and UBT builds Win64 and Android ARM64.

## Report format

Files added or changed with their PLAN task; the proof output verbatim; the expiry rows as a run
wrote them; the latency observations with the `fired-not-displayed` and `unresolved` counts; which
of 4.8's gate clauses this chunk closes and which are left to chunk 20; what was not run and why;
any deviation from PLAN.md with the reason.

---

## Revision 2: two blockers and two majors, all accepted

### A. "Exactly one status row" was not structural, and one of its failure modes is real

Revision 1 said that putting the sink in the schedule makes the one-status-row rule automatic
because every mutation goes through `Arm`, `Cancel`, `PopDue` or `Clear`. That is true of the data
structure and does not follow for the rows, because revision 1 never said what each of those
emits. Two concrete failures:

- **`PopDue` calls `Cancel` on its own entry** (`ExpirySchedule.cpp:28`). If `Cancel` emits, a
  firing produces `OnFired` and then `OnCancelled` for the same expiry.
- **`SignalRegistry::Expire` cancels a deadline it has just decided has passed**
  (`SignalRegistry.cpp:94`). If `Expire` runs before `PopDue` in a `Pump`, the entry is gone and
  the expiry gets a cancel and no fire, which is backwards: `Expire` marking a sample stale **is**
  the firing.

**The emission rules are now part of the deliverable rather than left to the implementer:**

- `Cancel` emits `OnCancelled` **only when it actually removed an entry**, and never otherwise.
- `PopDue` removes without notifying and emits `OnFired`. Its internal removal is not a cancel.
- `Arm`'s implicit cancel emits `OnCancelled` naming the sample that re-armed it.
- `Clear` emits `OnCancelled` for every remaining entry, with the generation or `run_end`.
- **`SignalRegistry::Expire` calls a new `ExpirySchedule::Fire(kind, id, now)`**, not `Cancel`,
  because it is observing a deadline pass rather than calling one off. `Fire` emits `OnFired` and
  removes, and does nothing if the entry is already gone.

`Fire` and `PopDue` are then the only two paths that fire, each removes the entry, and whichever
runs second finds nothing and emits nothing. That is what makes it exactly one, and it is a
property of two named functions rather than of a sentence about the data structure.

### B. The present row cannot know which expiry it is displaying

Revision 1 said the binding table learns the expiry "from the snapshot". It cannot: no expiry
identifier exists anywhere on `signal_core::Sample`, `SignalSample`, `Snapshot`, `FSignalSample`
or `FFrameSnapshot`. The binding table can see only the signal and that its rendered quality is
stale, which is exactly the "match by signal alone" that 4.8 forbids because it lets a later
episode's frame close an earlier expiry.

**The serial has to travel on the sample.** `signal_core::Sample` gains `expiry`, stamped when
`Expire` marks the sample stale, with the serial of the expiry that fired. It then rides the same
road the sample id already does: `SignalSample`, the snapshot, `FSignalSample`, the binding
table's rendered entry, and the present row's `expiry` column. Zero means the sample is not
displaying a firing, which is every valid sample.

### C. `hold_last` and `debounce` ARE armed, and revision 1 said they were not

`RuleEngine::Evaluate` arms a `hold_last` entry (`RuleEngine.cpp:222-224`) and a `debounce` entry
(`RuleEngine.cpp:289-290`), and the signal-core tests arm both directly. Revision 1's "nothing in
the tree arms an entry of either kind" is false, and it would have misled the implementer into
thinking a path the doctests exercise could not be reached.

The true statement is narrower and is what deliverable 5 now rests on: **signal-core arms both
kinds, and the Stage 0 player never loads a rule that could.** Every `FDashAcquisition`
construction in `DashPackageScreen.cpp` passes no threshold rules. So the rule-expiry record type
is real, is exercised by the doctests, and is empty in a player run. Deliverable 5 emits the rows
from the schedule like every other kind, and the report states the player-path emptiness as a
measured fact rather than as a reason not to build them.

### D. Three of PLAN's named latency runs were collapsed into one sentence

Criterion 5 said "one observation per displayed firing" where PLAN names three separate runs.
Restored as their own criteria:

- **5a.** A signal restored between a firing and the next snapshot acquisition counts that expiry
  as `fired-not-displayed` and produces no observation.
- **5b.** A frame that acquires the stale snapshot, after which the signal recovers and that frame
  presents anyway, produces exactly one observation for that expiry from its delayed
  presentation. This is what `-udash-delay-present-ms` is for, and revision 1 never connected the
  switch to the case it exists for.
- **5c.** Two disconnect episodes produce two `fired` rows per bound signal and two observations
  when both are displayed, each attributed by expiry identifier. The connector gate already kills
  and restores a relay once; this needs it twice.

Criterion 8 is also tightened: the rule-expiry rows are emitted and checked against the schedule's
own arming in the doctests, rather than the chunk being allowed to record their absence and stop.

### E. The suite counts are a floor, not a target

Criterion 9's numbers are the measured values at the end of chunk 18. They are there to catch a
drop, and this chunk is expected to raise them.
