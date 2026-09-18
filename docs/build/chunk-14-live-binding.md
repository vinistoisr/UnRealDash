# Build chunk 14: the live binding path

Status: revision 1, written against the tree. Completes the part of PLAN.md task 4.9 that chunk 13
could not reach, and closes a gap chunks 11 and 12 left open. Read PLAN.md (4.9, 4.5, 4.6), the
chunk 13 report, and `docs/ARCHITECTURE.md` first.

## The gap this closes

`FDashAcquisition` publishes a snapshot. `UDashPackageScreen` builds a widget tree. **Nothing
connects them**, and the primitives cannot be connected as they stand: `BuildAnalogDial` writes the
deflection into a render transform once at construction and returns a widget with no way to be told
a new number. Every gauge in the tree is the same shape. That is why they are currently driven by
`-udash-fraction`, a gate switch, rather than by a signal.

So PLAN 4.9's criteria 4, 5 and 6 are unreachable: "the player shows live values", "killing the mock
marks every mapped signal stale" and "restarting reconnects without a player restart" all require a
widget that reacts.

## Facts this chunk depends on

- **`Signal` carries both identities.** `Sample.h:10-16`: `SignalId id` (a `std::uint32_t`) and
  `char name[128]`. A document's binding names a signal by string, the registry works in numbers,
  and the registry's own signal table is what joins them. No new mapping is invented.
- **The snapshot is already the right shape.** `SnapshotExchange::Acquire()` yields
  `std::span<SignalSample>`, each carrying `signal` (the numeric id), the `Sample` with its value
  and `Quality` and `AgeEvidence`, and `received`.
- **Bindings are already exposed.** `FDashPackage::Bindings()` returns the document's map, keyed by
  component id, each entry carrying `property`, `signal` and an optional `format` (chunk 11, C4).
- **The package can carry a signals document** (`PackageReader.cpp:362`, which admits
  `signals.json`), but nothing reaches it: `Document` exposes `ComponentCount`, `ComponentAt`,
  `Theme` and `Root` only. An accessor is the first deliverable.
- **The simulator is already engine-side and needs no socket.** `FSmokeSimulator::Tick` yields one
  `FSignalSample` per call from a `signal_core` scenario, so this chunk can prove a live path
  without a relay, a definition pack, or the connector selection layer chunk 13 left out.
- **Chunk 11 already decided that an unbound component presents no missing-data state**
  (`PrimitiveSupport.cpp`, `BindingFor(...).Exists()`). The same rule decides which components get
  an updater: exactly those the document binds.

## Deliverables

### 1. Reaching the signals document

`Document::Signals()` beside `Document::Theme()`, returning the bundle's `signals` member, and
`FDashPackage::Signals()` over it. Both follow the existing shape exactly; neither is new
machinery.

### 2. A builder returns a widget and a way to update it

```cpp
struct FDashSignalValue {
    double Value = 0.0;
    EDashSignalState State = EDashSignalState::Unavailable;
    bool bHasValue = false;
};
using FComponentUpdater = TFunction<void(const FDashSignalValue&)>;
struct FBuiltComponent { UWidget* Widget = nullptr; FComponentUpdater Updater; };
using FComponentBuilder = TFunction<FBuiltComponent(const FComponentContext&, FDashLoadError&)>;
```

Every builder changes, because every builder has to. A component with no binding returns an unset
updater, which is not a special case to remember: `BindingFor` already answers it and chunk 11
already uses that answer for missing-data rendering.

What each updater does, and nothing more:

| primitive | on a new value |
| --- | --- |
| `readout` | sets the value text through the binding's format, and the missing-data presentation |
| `analog_dial` | sets the needle's render transform angle |
| `bar_gauge` | resizes the fill, or rebuilds the arc's point list |
| `indicator` | lights or dims the lamp |
| `image` | missing-data presentation only; its asset does not change |
| `container`, `shape`, `page_switch` | no updater; they bind no signal |

**The missing-data rendering moves into the updater**, because state is now something that changes
rather than something fixed at build time. `ApplyMissingData` currently paints a status band as a
new child; called every tick it would add one per frame. It becomes: build the band once, and let
the updater set its colour and visibility.

### 3. The tree builder collects updaters

`FWidgetTreeBuilder::UpdatersById()` beside `WidgetsById()`. Same map, same keys, populated in the
same pass.

### 4. `FDashBindingTable`

Resolves the document's bindings once, at load, into a flat list of `{SignalId, FComponentUpdater*}`.

- A binding naming a signal the signals document does not declare is an **error naming the
  component's JSON pointer**, not a silently dead gauge. The semantic pass already rejects an
  unresolved binding signal (`E_UNRESOLVED_SIGNAL`), so through `LoadPackage` this cannot fire; it
  is kept for the same reason chunk 11 kept its unreachable errors, and the report says which layer
  produced it.
- Resolution happens once. A per-tick string comparison against 128-byte signal names, for every
  binding, every frame, is the kind of thing that is invisible on a workstation and shows up as a
  dropped frame on the head unit.

### 5. The screen ticks

`UDashPackageScreen::NativeTick` acquires the latest snapshot, walks the binding table, and calls
each updater with the value and state for its signal.

- **The snapshot is acquired once per tick**, not once per binding. `SnapshotExchange::Acquire`
  gives one coherent set of values; calling it per binding could show two components values from
  different publications, which is exactly what the triple buffer exists to prevent.
- `Quality` and `AgeEvidence` map to `EDashSignalState`: `valid` with `measured` is `Valid`,
  `valid` with `unknown` is `AgeUnknown`, `stale` is `Stale`, `unavailable` is `Unavailable`,
  `invalid` is `Invalid`. That mapping is the whole of what chunk 11's four presentations mean, and
  it belongs in one function.

### 6. A value source that needs no socket

`-udash-scenario=<name>` runs `FSmokeSimulator` in the screen and feeds the registry, so this chunk
proves the update path without a relay, a definition pack, or the connector selection layer.

It replaces `-udash-fraction` for bound components. The fraction switch stays for the chunk 12 dial
gate, which measures geometry at a pinned deflection and must not become time dependent.

## Constraints

- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module.
- No hand-authored widget Blueprints; the document stays the only source of layout.
- Nothing switches on a component type string outside the registry.
- An updater allocates nothing. It runs every frame on the game thread, and a per-frame allocation
  in a dashboard is a stutter in a moving car.
- No em dashes; no PowerShell 7 only syntax in scripts.

## Non-goals

- The connector selection layer and `-connector=`. That is the rest of 4.9 and comes next.
- The debug overlay (4.8) and the example document (4.10).
- Device work.

## Pass/fail criteria

1. Every existing gate still passes unchanged: the package commandlet, the chunk 11 capture gate and
   the chunk 12 dial gate. The builder signature change touches all nine primitives, so this is the
   regression check that matters.
2. **A bound gauge moves.** Two captures of the same document at two frozen scenario timestamps,
   measured with `scripts/measure-extent.py`: the needle's bounding box differs by more than the
   measurement's own quantisation, and the two positions match the scenario's values at those
   timestamps to within the needle's angular resolution. Print both.
3. **An unbound component does not move.** In the same pair of captures, the face image measures
   identically. This is the control: if everything moved, the test would pass on a tree that rebuilt
   itself rather than on a binding that worked.
4. **A stopped source goes stale.** With the scenario finished, the bound readout shows its `stale`
   presentation within the signal's declared deadline, measured by the status band's colour.
5. **No per-frame growth.** A capture after 600 ticks has the same widget count as one after 1, so
   an updater is not adding a child per frame. Print both counts.
6. Both CMake suites stay green with no drop. Chunk 13 recorded signal-core 148 cases and 112,876
   assertions, dashboard-spec 28 and 1,002,149.
7. `doctor.ps1 -Profile workstation` exits 0 and Pester passes with `-CI`.
8. UBT builds Win64 and Android ARM64.

## Report format

Files added or changed with their PLAN task; the proof output verbatim; the measured needle
positions and the scenario values they correspond to; what was not run and why; any deviation from
PLAN.md or this spec with the reason.

---

## Revision 2: what the review and the engine source changed

### C1. Two facts were stale before the review read them

`Document::Signals()` and `FDashPackage::Signals()` were already built and tested by the time the
review ran, so its blocker about deliverable 1 creating a duplicate is a report on a tree that had
already moved. The accessor exists, `test_signals_view.cpp` covers it against a real package, and
deliverable 1 is done rather than pending.

That is the cost of reviewing a spec while building from it. Worth the trade, and worth naming so
nobody reads the finding later as an open defect.

### C2. The badge grew a child per call too, and the spec only named the band

Correct, and it was fixed in the same pass. Deliverable 2 said the status band had to be built once;
the `icon` presentation also constructed a `UTextBlock` and added it through `Host->AddChild(Badge)`
on every call. Both are now built once by `BuildMissingData` and only shown or hidden by
`FMissingDataPresenter::Apply`.

A third one the review did not reach: the `dash` presentation replaced the value text in place, so
the original was gone and a return to `Valid` could not restore it. The presenter keeps it.

### C3. A native UUserWidget does tick, verified rather than assumed

The review raised this as a MAJOR it could not verify, which was the right way to raise it. Settled
by reading `Engine/Source/Runtime/UMG/Private/UserWidget.cpp`:

```cpp
void UUserWidget::UpdateCanTick()
...
        if (TickFrequency == EWidgetTickFrequency::Auto)
        {
            // Note: WidgetBPClass can be NULL in a cooked build.
            UWidgetBlueprintGeneratedClass* WidgetBPClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass());
            bCanTick |= !WidgetBPClass || WidgetBPClass->ClassRequiresNativeTick();
```

`TickFrequency` defaults to `Auto` (`UserWidget.cpp:98`). `UDashPackageScreen` is a native C++ class
and not a `UWidgetBlueprintGeneratedClass`, so the cast is null, `!WidgetBPClass` is true, and it
ticks. The `DisableNativeTick` metadata on `UUserWidget` itself governs Blueprint-generated classes
through `ClassRequiresNativeTick`, not native subclasses.

No `bCanEverTick` call is needed and none is added. `UpdateCanTick` does require a valid GC widget
and a world, both of which hold once the screen is in the viewport.

### C4. Criterion 5 could not be measured the way it was written

"A capture after 600 ticks has the same widget count as one after 1" cannot be measured by a
capture: a screenshot records pixels, and nothing in the screen reports a widget count.

**Criterion 5 is now:** the screen logs `WidgetTree->GetAllWidgets().Num()` after the first tick and
again after 600, and the two are equal. Logged rather than captured, and printed in the report.

This matters more than it sounds. The whole point of the presenter restructure is that an updater
sets properties and never adds a child, and without this criterion the only thing standing behind
that is the code reading as though it does.

### C5. The snapshot could not say which signal a sample belonged to

`FDashAcquisition::AcquireFrameSnapshot` filled `Samples` in registry order and `Present` with a
**compacted** list of the ids received this publication:

```cpp
if (Snapshot.samples[Index].received) Out.Present[Present++] = Snapshot.samples[Index].signal;
```

So `Present[i]` has nothing to do with `Samples[i]`, and a binding table could not find its signal.
`FSignalSample` carries no id of its own either.

**Decision.** `FFrameSnapshot` gains `SignalIds`, one per entry of `Samples` and in the same order,
filled in the same loop. An aligned array rather than an index arithmetic rule someone has to
remember, and `Present` keeps its existing meaning for the acquisition event sink that already uses
it.

### C6. Acquiring once per tick is required, not tidy

Confirmed from `SnapshotExchange.cpp`: `Acquire()` advances to a newer publication whenever one is
ready. Two calls in one frame can therefore straddle a publication and hand two components values
from different ones, which is exactly what the triple buffer exists to prevent. The spec said this;
the review confirmed it is a requirement rather than a preference, and it is worth the emphasis.
