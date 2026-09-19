# Chunk 19: expiry rows, expiry-to-present latency, and forcing (PLAN 4.8, part 3 of 4)

**The third quarter of PLAN 4.8 is complete.** Every arming of an expiry now has an identity and
exactly one status row, the present row names the firing it is displaying, expiry-to-present
latency is computed by identifier rather than by signal, and the player can be made to hold its
acquisition and presentation sides apart on purpose so PLAN's forced runs can happen at all.

It also found and fixed a real defect that chunk 18 shipped.

## The gate, in one run

```
criteria 1, 5 and 6: a source that stops
  presented                     149
  acquired-not-displayed        149
  sum                           300 of 300 published
  armed                         300
  fired                           2
  cancelled, re-armed           298
  unresolved                      0
  displayed firings               1
  fired-not-displayed             1
  expiry-to-present        median 2.89 ms, p95 2.89 ms

criterion 2: a clean exit with deadlines still armed
  cancelled, re-armed           802
  cancelled, run_end              2
  unresolved                      0

criterion 4: a hard kill mid-flight
  sum                           602 of 602 published
  unresolved                      2

criterion 7a: submission suppressed
  presented                       0
  acquired-not-displayed        798
  sum                           806 of 806 published
  acquired-not-displayed: no submit row: 798

criterion 7b: frames held between acquire and apply
  presented                     253
  sum                           810 of 810 published

Expiry gate: 0 failures
```

`fired 2, displayed firings 1, fired-not-displayed 1` is the shape PLAN describes: one bound
signal's expiry reaches the screen and is measured, and the unbound signal's expiry fires with
nothing to draw it, so it is counted separately and never becomes an observation.

## Criteria

| # | criterion | result |
| --- | --- | --- |
| 1 | every armed expiry has exactly one status row | checked over every gate run, and by five doctests |
| 2 | a clean exit cancels what is still armed, with reason `run_end` | 2 rows, 0 unresolved |
| 3 | a re-armed expiry is cancelled naming the sample that moved it, with no observation | 298 of 300 in the first case |
| 4 | a hard kill leaves armed expiries `unresolved`, excluded and counted | 2 unresolved, no `run_end` rows |
| 5 | one observation per displayed firing, attributed by identifier | 1 observation at 2.89 ms |
| 6 | every armed row names a signal with a receive row and a later deadline | classifier, every run |
| 7 | the four forced branches each produce their lifecycle state, and the counts still sum | below |
| 8 | rule expiry rows emitted or their absence recorded | recorded; see below |
| 9 | both suites green with no drop | dashboard-spec 30 / 1,002,203; signal-core 179 / 117,162, up from 174 / 117,123 |
| 10 | eight gates, doctor, Pester, UBT Win64 and Android | all clean |

## The defect chunk 18 shipped

**Present rows were being matched to the wrong frames, and nothing noticed.**

Chunk 18 kept two frame numberings without realising it. `FDashAcquisition` numbered frames with
its own counter starting at 0, while the per-frame row and the render-thread present stamp used
`GFrameCounter`, which starts wherever the engine happens to be after level load. The join looked
up a rendered set by `GFrameCounter` in a map keyed by the acquisition's counter. Once the run was
long enough for the two ranges to overlap, every lookup succeeded and paired a frame with a
rendered set from roughly 250 frames earlier.

It passed chunk 18's gate because the partition only asks whether each sample reached *a* frame,
not *which*. It surfaced here because `-udash-hold-frame-ms` slows the run enough that the two
ranges never overlap, and every present row vanished.

Two changes, because the first alone was not enough:

- **One numbering.** `AcquireFrameSnapshot` now takes the caller's frame index, and the screen
  passes `GFrameCounter`. Acquire, submit, per-frame and present rows all key on it.
- **Pairing in order, not by index.** `GFrameCounterRenderThread` has already advanced by the time
  `OnEndFrameRT` fires, so a stamp carries the number of the frame the render thread is moving on
  to rather than the one it just finished. The join now pairs the oldest waiting frame with each
  arriving stamp, guarded by the timestamp so a stamp cannot pair with a frame it predates. The
  header says what a present timestamp therefore means:
  `"first end_of_render_thread_frame after the frame's game tick, paired in order"`.

The rendered set also moved onto the pending frame entry rather than living in a second map keyed
by the same number. With two structures, the join could find the frame and miss the rendered set,
which is precisely what happened.

## Decisions worth their own line

**`SignalRegistry::Expire` now calls `Fire`, not `Cancel`.** Marking a sample stale because its
deadline passed **is** the firing. Recording it as a cancellation would have lost the start
endpoint of every expiry-to-present observation, and the spec's first draft did not notice that
the two call sites meant different things by the same function.

**Exactly one status row is a property of two functions.** `Fire` and `PopDue` are the only paths
that fire, each removes the entry, and whichever runs second finds nothing. `Cancel` emits only
when it actually removed something, which is what makes the cancel `Expire` issues after `PopDue`
has taken the entry write nothing at all. The first draft of the spec claimed this followed from
putting the sink in the schedule; it did not, and the review said so.

**Three forcing switches, not four.** The spec listed `-udash-delay-present-ms` and
`-udash-pause-at`/`-udash-pause-ms` separately. They are one mechanism: a widened gap between what
a frame holds and when it reaches the screen. They are `-udash-hold-frame-ms=<n>`, which sleeps
the game thread between acquiring a snapshot and applying it. **Deviation from the spec, not from
PLAN**, which names the runs rather than the switches.

**Suppressing the submit suppresses the presentation record too.** A frame the game thread never
finished with is a frame that never reached the screen, which is what PLAN's clause is about.
Suppressing only the row left every sample `presented`, which is the opposite of the case.

## A classifier weakness the forced runs exposed

With submission suppressed there are no present rows at all, so no signal appears in any of them
and every signal looks unbound. The `acquired-not-displayed` sub-reasons were tested
bound-to-nothing first, so 798 samples were attributed to the wrong reason.

The order is now: **no submit row, then bound to no visible component, then submitted and never
presented.** A missing submit row is a fact the rows state outright; "bound to no visible
component" is inferred from a signal appearing in no present row, and an inference must not
outrank a fact.

## Rule expiries, checked rather than assumed

The spec's first draft said nothing in the tree arms a `hold_last` or `debounce` entry. **That was
false.** `RuleEngine::Evaluate` arms both (`RuleEngine.cpp:222-224` and `:289-290`), and the
signal-core tests exercise them.

The true statement is narrower: **signal-core arms both kinds, and the Stage 0 player loads no
rule that could.** Every `FDashAcquisition` construction passes no threshold rules, so a player
run has no rule expiry rows, while the record type is real and a doctest covers both kinds
carrying the same identity as a freshness expiry. The report states the player-path emptiness as a
measured fact; it is not a reason the rows were not built.

## Files, by PLAN task

| path | task |
| --- | --- |
| `runtime/UnRealDash/Source/SignalCore/{Public/SignalCore,Private}/ExpirySchedule.{h,cpp}` | 4.8, identity, the sink, `Fire` and the emission rules |
| `runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Sample.h` | 4.8, the expiry a sample is displaying |
| `runtime/UnRealDash/Source/SignalCore/{Public/SignalCore,Private}/SignalRegistry.{h,cpp}` | 4.8, arming identity, `MarkFired` |
| `runtime/UnRealDash/Source/SignalCore/Private/Acquisition.cpp` | 4.8, stamping from the popped entry |
| `packages/signal-core/tests/test_rule_state.cpp` | 4.8, five schedule cases |
| `runtime/UnRealDash/Source/UnRealDashCore/.../DashEventLog.{h,cpp}` | 4.8, expiry rows, the reworked join |
| `runtime/UnRealDash/Source/UnRealDashCore/.../DashAcquisition.{h,cpp}` and its private header | 4.8, the expiry sink, `EndRun`, caller frame numbering |
| `runtime/UnRealDash/Source/UnRealDashCore/.../DashBindingTable.cpp` | 4.8, the displayed expiry |
| `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPackageScreen.{h,cpp}` | 4.8, the forcing switches |
| `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPlayerConfig.cpp` | 4.8, the switches declared to the resolver |
| `scripts/validate-event-log.py` | 4.8, expiry analysis and latency |
| `scripts/run-expiry-gate.ps1` | 4.8, six cases |

## Four things the review found before any of this was built

DeepSeek returned BLOCKERS FOUND on revision 1. All four accepted, recorded as revision 2 in
`docs/build/chunk-19-expiry-and-forcing.md`:

1. **The one-status-row rule was not structural**, with a concrete trace showing a fire followed
   by a cancel for the same arming.
2. **The present row could not know its expiry identifier**, because no such field existed
   anywhere between the registry and the binding table. The spec had promised the column and
   skipped the change that would carry it.
3. **`hold_last` and `debounce` are armed**, contradicting a stated fact.
4. **Three of PLAN's named latency runs had been collapsed into one blanket sentence.**

## Suites

```
signal-core     [doctest] test cases:    179 |    179 passed | 0 failed | 1 skipped
                [doctest] assertions: 117162 | 117162 passed | 0 failed |
dashboard-spec  [doctest] test cases:     30 |     30 passed | 0 failed | 0 skipped
                [doctest] assertions: 1002203 | 1002203 passed | 0 failed |
Pester          Tests Passed: 76, Failed: 0, Skipped: 0
doctor, UBT Win64, UBT Android   all clean
eight gates                      0 failures each
```

The five schedule cases were watched failing: routing `PopDue`'s own removal back through `Cancel`
turns the one-status-row invariant red in three places at once.

## What is left of 4.8

Chunk 20: the on-screen overlay, asset import rows, the Android sampled-stream sources, and
launch-to-first-usable-value. Its report carries the clause-by-clause map of 4.8's gate paragraph
to the chunk that closed each.

Three of PLAN's latency runs are named in this chunk's spec as criteria 5a, 5b and 5c and are
**not** separately gated here. The mechanism each needs exists and is exercised: firings are
displayed and measured, `fired-not-displayed` is counted, and `-udash-hold-frame-ms` produces the
widened acquire-to-present gap. What is missing is a fixture that reconnects twice, which is the
two-disconnect-episode case, and that belongs with the connector gate rather than here. Recorded
rather than claimed.

## Not run, and why

- **Anything on the device.** The Android build is proven; nothing is deployed.
- **A two-disconnect-episode run.** Named above.
- **The 8-hour soak.** 6.6b.
