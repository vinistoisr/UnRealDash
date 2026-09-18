# Chunk 14: the live binding path (PLAN 4.9)

The dashboard now reacts to signals. A bound dial sweeps, a circular gauge fills, a history graph
accumulates a real series, and when the source stops every bound signal goes stale because a
deadline passed in the real registry rather than because something decided to say so.

This closes PLAN 4.9's criteria 4 and 5 through the scenario source. Criterion 6, reconnecting with
no player restart, still needs the connector selection layer, which is the last piece of 4.9.

## What the gate measured

```
criterion 2, the bound needle moves:
  early box 176,277,422,368, late box 414,353,619,527
criterion 3, the unbound face does not move:
  identical box 120,60,719,659 in both
criterion 2b, the circular gauge sweeps:
  2232 pixels early, 9558 late
criterion 4, the source stops and the signal goes stale:
  the stale band is present, 81600 pixels
  and the needle is gone, which is what the dash presentation means

Live gate: 0 failures
```

Criterion 3 is the one that makes the rest mean anything. If everything in the frame moved between
two captures, the gate would pass on a tree that rebuilt itself rather than on a binding that
worked. The face image is bound to nothing and measures identically at both points.

Criterion 5, that an updater never adds a child, cannot be measured by a capture at all: a
screenshot records pixels, not a widget tree. Logged instead:

```
DashWidgets tick=1 count=19
DashWidgets tick=600 count=19 first=19
```

## Files added or changed

- `UnRealDashCore/Public/UnRealDashCore/DashBindingTable.h`, `Private/Package/DashBindingTable.cpp`:
  resolves the document's bindings once against the package's signals, and applies one snapshot to
  every bound widget. PLAN 4.9.
- `UnRealDashCore/Public/UnRealDashCore/DashScenario.h`, `Private/Package/DashScenario.cpp`: the
  value source, which generates recording bytes. PLAN 4.9.
- `UnRealDash/Private/Package/DashPackageScreen.{h,cpp}`: `NativeTick`, `-udash-scenario=`,
  `-udash-shot-at=`, and the widget count log. PLAN 4.9.
- `UnRealDashCore/Private/Package/Primitives/DashLines.cpp`: antialiased polylines. PLAN 4.6 fix.
- `UnRealDashCore/Private/Package/Primitives/BarGaugePrimitive.cpp`, `Primitives.h`: four arc points
  per degree. PLAN 4.6 fix.
- `tools/gen-chunk14-fixture.py`, `tests/fixtures/...`: the live fixture. PLAN 4.9.
- `scripts/run-live-gate.ps1`: criteria 2 to 4. PLAN 4.9.
- `docs/build/chunk-14-live-binding.md`: the spec, revision 2.

## What the build found

### The value source has to know what the document's numbers mean

The first working run moved the needle by two pixels, which reads as a binding that does not work.
It was doing exactly what it was told: the scenario swept 0 to 1 while the dial declared 0 to 100,
so the needle travelled two tenths of one percent of its face.

A sweep needs a range, and the only defensible range is the one the document declares.
`FDashBindingTable` now collects, per signal, the minimum and maximum of every gauge bound to it,
unioned where several disagree, and the scenario sweeps that. A signal that only feeds a readout
declares no range and keeps 0 to 1, which is what a fraction means when nothing says otherwise.

### The value source generates bytes, not readings

It builds a recording covering the document's own signals and hands it to `FDashAcquisition`.
Everything downstream is the real thing: the real registry, the real freshness deadlines, the real
expiry schedule, the real snapshot exchange.

That is why criterion 4 works without any staleness code in this chunk. The scenario is one second
long, the capture is four seconds later, and the registry expires each signal on its own deadline
exactly as it would for a relay that stopped. A stale dial here is stale for the same reason a stale
dial will be stale in the car.

### The circular gauge was visibly serrated, and the cause was a chunk 12 decision

The arc came out with black notches along its outer edge. Chunk 12 disabled antialiasing on
`SDashLines` so the screenshot gate would measure a stable extent, and Slate's non-antialiased path
draws a thick polyline as one quad per segment with no join, so every turn leaves an uncovered notch
whose width grows with thickness and turn angle.

Raising the point density from one to four per degree made the notches thinner without closing them,
which is what identified the cause: unjoined quads, not a coarse polygon. Antialiasing is now on and
the arc is a clean band.

The original reason for turning it off did not survive checking. The gates that touch these two
primitives use pixel-count floors with wide margins, an arc floor of 3,000 against about 9,500
measured, rather than an exact bounding box, so a fraction of a pixel at an edge changes nothing.
Both gates pass unchanged.

### Moving the state rendering out of the build path silently switched it off

Already recorded with the previous commit, and worth repeating because of how it was caught. With
the missing-data rendering moved into the updater, widgets were built in their valid appearance and
nothing applied the state until a signal arrived, so all three `-udash-state` captures came out
byte-identical. The chunk 11 gate failed on the first run after the change.

## The review

DeepSeek reviewed the spec. Two of its findings were right and are fixed:

- **The badge grew a child per call too**, not only the status band. The spec named only the band.
  A third case it did not reach: the `dash` presentation replaced the value text in place, so the
  original was gone and a return to `Valid` could not restore it.
- **Criterion 5 could not be measured the way it was written.** A capture cannot count widgets. It
  is now a logged count, which is the numbers above.

One finding it raised as MAJOR and correctly marked unverified was that a `UUserWidget` might not
tick by default, given the `DisableNativeTick` metadata on the class. Settled by reading
`UserWidget.cpp`:

```cpp
UWidgetBlueprintGeneratedClass* WidgetBPClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass());
bCanTick |= !WidgetBPClass || WidgetBPClass->ClassRequiresNativeTick();
```

`TickFrequency` defaults to `Auto`, a native C++ class is never a `UWidgetBlueprintGeneratedClass`,
so the cast is null and it ticks. The metadata governs Blueprint-generated subclasses. No
`bCanEverTick` call was needed and none was added. Raising it as unverified rather than asserting
it was the right way to handle a claim about code it could not read.

Its blocker about `Document::Signals()` already existing was a report on a tree that had moved: the
accessor was built and tested earlier in the same session. That is the cost of reviewing a spec while
building from it.

## What was not run, and why

- **No device run.** The gate is desktop captures.
- **No relay.** The scenario source replaces a connector, and the connector selection layer is the
  remaining piece of 4.9.
- **The needle's exact angle is not pinned to an exact scenario timestamp.** The two captures land
  in decisively different places, which is what criterion 2 asks, but the capture clock and the
  scenario clock are not locked together. `-freeze-scenario-time` is PLAN 4.11 and that is where an
  exact-value assertion belongs.
- **The live gate does not run in CI**, for the same reason the other two capture gates do not.

## Known limitations

- **`-udash-scenario=` is a gate switch**, beside `-udash-state=` and `-udash-fraction=`, outside
  the runtime flag surface PLAN 4.7 owns. It generates bytes and says so.
- **The scenario writes every signal as dimensionless.** The units a document declares belong to the
  connector that will replace it, and claiming one here would assert a physical quantity nothing
  measured.
- **An indicator lights on a non-zero reading.** A rule result driving it is PLAN 5.x.
- **The binding table resolves a signal's numeric id as its index in the document's signals array.**
  The scenario recording is written from the same array, so the two agree by construction. A real
  connector will bring its own ids from a definition pack and that join moves with it.
