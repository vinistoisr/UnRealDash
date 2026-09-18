# Build chunk 12: analog dial, bar gauge and history graph

Status: revision 1, written against the tree rather than from memory. Covers PLAN.md task 4.6. Read
PLAN.md (task 4.6, task 4.5 for what already exists, task 4.9 so you know what you are not building)
and `docs/ARCHITECTURE.md` before writing anything. PLAN.md is the authority on what; this file adds
the mechanisms, proof commands and implementation constraints. If the two disagree, PLAN.md wins and
the disagreement goes in the report.

Chunk 11 shipped with nine corrections because revision 3 of its spec was frozen without checking
its deliverables against the schema. Every fact below carries the command that produced it.

## Facts this chunk depends on

- **The three types this chunk owns declare no artwork fields.** Checked against
  `packages/dashboard-spec/schema/dashboard.schema.json`:

  | type | required properties |
  | --- | --- |
  | `analog_dial` | `minimum`, `maximum`, `colour` |
  | `bar_gauge` | `minimum`, `maximum`, `circular`, `colour` |
  | `history_graph` | `history_samples`, `colour` |

  `additionalProperties` is false on each. No component type except `image` names an asset anywhere
  in the schema. A chunk 11 note claiming otherwise is corrected in `chunk-11-primitives.md`.

- **The semantic pass already rejects a non-uniform dial at load.** `SemanticPass.cpp:273`:

  ```cpp
  if ((type == "analog_dial" || (type == "bar_gauge" && properties["circular"].GetBool())) && policy == "stretch")
      return fail(ErrorCode::E_ASPECT_POLICY_FORBIDDEN, pointer + "/aspect_policy", key);
  ```

  `policy` is resolved by walking up through **container ancestors only**
  (`SemanticPass.cpp:267-272`), and a chain that inherits all the way to the root stays `inherit`, so
  the check does not fire on it. `ResolveAspectPolicy` in `Layout.cpp` now matches that walk exactly;
  it did not before, and that divergence is recorded in `chunk-11-primitives.md`.

- **`history_samples` is capped at 4,096** (`Limits.h:16`), enforced by the semantic pass at
  `SemanticPass.cpp:275`.

- **`history_graph` declares no minimum or maximum**, and signals declare a `unit` but no range. A
  history graph therefore has no declared y-range and must scale to its own buffer.

- **Chunk 11's machinery is in place**: `RegisterStage0Builders`, the per-type adopter,
  `FDashTheme`, `ApplyLayout`, `ApplyMissingData`, the 1:1 reference-viewport layout,
  `-udash-state=`, `-udash-shot=`, `scripts/compare-capture.py` and `scripts/run-capture-gate.ps1`.

- **`tests/fixtures/documents/valid/nine-primitives.json` already contains all three types**, so the
  validator corpus covers them. No package fixture uses any of the three, which is why this chunk
  authors one.

## The design decision this chunk inherits

**Primitives consume artwork. They do not generate ornament.** Chunk 11 established it and the
schema enforces it by omission: there is no field through which a dial could name a face.

So each of these three primitives draws **only the part that moves**:

- `analog_dial` draws a needle, and nothing else. No bezel, no tick band, no numerals, no face. Those
  are `image` components the document positions behind it. This is exactly the owner's own RealDash
  cluster for the Scirocco, where the face is authored as SVG, rendered to PNG at 2x, and the only
  thing that moves at runtime is a rotating needle sprite.
- `bar_gauge` draws the filled portion. The track behind it is artwork.
- `history_graph` draws the trace. Axes, grid and labels are artwork.

A primitive that drew its own face could not be restyled by a document, which defeats the point of
having a document. That is the defect this rule exists to prevent, and it is the reason the gate
fixture composes its dial as an `image` face with an `analog_dial` needle on top.

## Deliverables

### 1. One line-painting widget, used by two primitives

Slate has no arc and no polyline widget that a document can position. Add exactly one:
`SDashLines`, a `SLeafWidget` whose `OnPaint` issues a single `FSlateDrawElement::MakeLines` for a
point list and a colour, and its `UWidget` wrapper `UDashLines`, in
`UnRealDashCore/Private/Package/Primitives/`.

Both the circular bar gauge's arc and the history graph's trace reduce to a point list, so one
widget serves both. Two separate custom painters for the same operation would be two places for the
same bug.

Antialiasing on the line is off. The screenshot gate measures pixel extents, and an antialiased
endpoint spreads the measured extent by a fraction of a pixel in a way that differs between drivers.

### 2. `analog_dial`

A needle rotated about the centre of its rect, drawn in the component's `colour`, resolved through
the theme like every other colour.

- Deflection is `(value - minimum) / (maximum - minimum)`, clamped to 0..1, mapped onto a sweep of
  **270 degrees starting at 225 degrees clockwise from twelve o'clock**, which is the usual
  automotive layout and matches the Scirocco cluster. The sweep is not a document field; the schema
  has none, and inventing one would change the schema this chunk must not change.
- The needle is a rectangle whose length is 42 percent of the smaller rect dimension and whose width
  is 3 percent, rotated by `FWidgetTransform` about a pivot at the rect centre. A rotation, not a
  redraw: `UImage` over a colour brush with a render transform costs no custom painting.
- `aspect_policy` `stretch` is refused with a readable error naming the pointer, even though the
  semantic pass already rejects it at load, because a direct caller can build a component the
  validator never saw. The error code is `E_ASPECT_POLICY_FORBIDDEN`, 18, matching the pass.
- The needle's square working area is the largest square centred in the rect, so a dial in a
  non-square rect keeps a circular sweep instead of tracing an ellipse.

### 3. `bar_gauge`

- `circular: false`: a filled rectangle from the low edge to the deflection, along the rect's longer
  axis. `UImage` over a colour brush, sized by the canvas slot.
- `circular: true`: an arc through `UDashLines`, on the same 270 degree sweep as the dial, at a
  radius of 42 percent of the smaller rect dimension and a thickness of 6 percent. The point list is
  one point per degree of swept arc, which at 270 degrees is at most 271 points.
- `circular: true` under a `stretch` aspect policy is refused exactly as the dial is, and for the
  same reason.

### 4. `history_graph`

- A polyline through `UDashLines` of the component's `history_samples` most recent values, oldest at
  the left edge, newest at the right.
- **The y-range is the buffer's own minimum and maximum**, padded by 5 percent at each end, because
  neither the component nor the signal declares a range. A buffer whose values are all equal draws a
  horizontal line at the vertical centre rather than dividing by zero.
- A buffer with fewer than two samples draws nothing and presents the component's missing-data state,
  because a single point is not a trace.

### 5. The value source, and what it is not

None of the three can render without a value, and the connectors that supply real values are PLAN
4.9. Chunk 11 added `-udash-state=` for exactly this shape of problem and this chunk adds its
counterpart:

**`-udash-fraction=<0..1>`**, a gate switch beside `-udash-state=` and `-udash-batch=`, outside the
runtime command-line surface PLAN 4.7 owns. It supplies one synthetic constant to every bound signal,
at that fraction of the consuming gauge's declared `minimum` to `maximum` range. A `history_graph`,
which has no declared range, receives a linear ramp from 0 to the fraction across its
`history_samples` points, so its polyline has a slope to measure rather than a degenerate flat line.

**It is not data and must never be presented as data.** It exists because 4.6's gate has to render a
gauge at a known deflection before a connector exists. It proves that the three primitives draw the
right geometry, at the right extent, in the right colour, and that a dial is never scaled
non-uniformly. It does not prove anything about a changing series, freshness, or a value arriving
from a connector; those are 4.9's gate. The report says so explicitly.

Default is 0, which rests every gauge at its minimum. A value outside 0..1, or one that does not
parse, exits non-zero rather than silently clamping, for the same reason a mistyped `-udash-state`
does: a mistyped gate run must not look green.

## The gate, and how the dial is measured

PLAN 4.6's gate is: *rendering the same document at 1280x720 and at 1280x660 produces a dial whose
measured width and height ratio stays within 1 percent of 1.0 in both captures.*

That needs a measuring tool, not a comparator. `scripts/compare-capture.py` answers "did this frame
change"; this gate asks "how big is this thing". Add `scripts/measure-extent.py`:

- Takes a capture, a target colour as `#rrggbb`, and a per-channel tolerance.
- Prints the bounding box of matching pixels, its width, height, `ratio = width / height`, and the
  matching pixel count.
- **Exits non-zero when it finds no pixels, or fewer than a `--minimum-pixels` floor.** A measurement
  of nothing must never read as a pass, which is the failure mode that has bitten this project twice.
- Ships with `--self-test` proving it measures a known rectangle exactly, reports a stretched one as
  a ratio away from 1.0, and refuses an empty frame.

Two quantities are measured, at both resolutions, and both must hold:

1. **The needle's own bounding box**, with the dial forced to `-udash-fraction=0.5`, which puts the
   needle at twelve o'clock, and to a fraction that puts it on a diagonal. A needle on the diagonal
   has equal width and height when the scale is uniform and unequal width and height when it is not,
   so this measures the dial primitive's own geometry rather than the artwork behind it.
2. **The face image's bounding box**, which is what a viewer sees as the circular gauge and what
   `aspect_policy: preserve` protects.

The dial colour, the face colour and the background are chosen so no other component in the fixture
shares either colour within the tolerance. A measurement that could match two components is a
measurement of neither.

**Why the ratio is expected to hold at 1280x660.** The root component's `scaling` is `uniform`, which
maps to `EStretch::ScaleToFit`, so the whole document scales by `min(1280/1280, 660/720)`, which is
0.91666 on both axes. A 300 unit square renders 275 by 275 in both. Under `stretch`
(`EStretch::Fill`) it would render 300 by 275, a ratio of 1.0909, which is 9 percent off and fails
the 1 percent band by a wide margin. The gate therefore also captures a **deliberately stretched
control**: the same document with the root's `scaling` set to `stretch`, which must FAIL the ratio
check. A gate nobody has watched fail is not a gate, and this project has already shipped two
assertions that could not fail.

## Constraints

- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module. `scripts/doctor.ps1`
  enforces it and the Pester suite fails the build if it is broken.
- No hand-authored widget Blueprints.
- Nothing switches on a component type string outside the registry. The three new builders are
  registered in `Stage0Builders.cpp` beside the six; adding them must not touch `FComponentRegistry`
  or `FWidgetTreeBuilder`.
- Errors are values. A malformed component is a readable on-screen error naming the JSON pointer,
  never a crash and never a silent skip.
- `RequestScreenshot`'s `bShowUI` stays `true`.
- No em dashes in any file, comment or printed string.
- No PowerShell 7 only syntax in scripts. The Pester suite parses every script under 5.1 rules and
  caught exactly this in chunk 11.

## Non-goals

- Connectors, scenarios and live values. PLAN 4.9.
- The command-line surface. PLAN 4.7. `-udash-fraction=` is a gate switch, like `-udash-state=`.
- The debug overlay. PLAN 4.8.
- Device work. The gate here is desktop captures.
- Beautiful artwork. The fixture's face may be plain.

## Pass/fail criteria

Each is a command that exits non-zero on failure.

1. A fixture exercising all three new primitives renders, in both packed and unpacked package form,
   through the existing package commandlet, with zero failures across the whole corpus.
2. The dial's needle bounding box has `ratio` within 1 percent of 1.0 at 1280x720 and at 1280x660,
   measured on the diagonal deflection. Print both ratios and both pixel counts.
3. The face image's bounding box has `ratio` within 1 percent of 1.0 at both resolutions. Print both.
4. **The stretched control fails.** The same document with the root scaling set to `stretch` produces
   a face ratio outside the 1 percent band at 1280x660, and the measured ratio is printed. If it
   passes, criteria 2 and 3 prove nothing and the run is a failure.
5. A `bar_gauge` with `circular: true` and a `history_graph` each render a non-empty trace in their
   own colour, with `measure-extent.py` reporting a pixel count above a floor recorded in the test.
6. An `analog_dial` under a directly declared `stretch` aspect policy produces a readable error
   naming its JSON pointer, with code `E_ASPECT_POLICY_FORBIDDEN`, and does not crash. Note in the
   report which layer produced it, as chunk 11's criterion 6 required.
7. The chunk 11 capture gate still passes unchanged, so the six existing primitives are not
   regressed.
8. Both CMake suites stay green with no drop in case or assertion count, taken from the doctest
   reporter rather than counted by hand. The chunk 11 figures are dashboard-spec 28 cases and
   1,002,108 assertions, signal-core 136 cases and 111,782 assertions.
9. `scripts/doctor.ps1 -Profile workstation` exits 0 including the layering check, and the full
   Pester suite passes with `-CI`.
10. UBT builds Win64 and Android ARM64. Android is a build check here, not a device run.

## Proof

```
cd packages/dashboard-spec && cmake --build --preset default && ctest --preset default --output-on-failure
cd packages/signal-core && ctest --preset default --output-on-failure
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI -Output Detailed"
python scripts/measure-extent.py --self-test
pwsh -NoProfile -File scripts/run-capture-gate.ps1
pwsh -NoProfile -File scripts/run-dial-gate.ps1
git status --short
```

A Ninja configure on this machine needs a developer environment:
`cmd /c "call \"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat\" >nul 2>&1 && cmake --preset default"`.
Without it you get `LNK1104 cannot open file kernel32.lib`, which is an environment problem and not
a code one.

## Report format

Files added or changed, one line each with path, what and which PLAN.md task; the proof output
verbatim; the measured ratios and pixel counts for every capture; an explicit list of what was not
run and why; any deviation from PLAN.md or this spec with the reason; and anything that could not be
done.

---

## Revision 2: five things the build found

### C1. A document cannot say what is in front of what

The dial rendered under its own face image, and nothing in the document could change that. Swapping
the declaration order changed nothing, and an explicit z-order taken from the delivered component
index changed nothing either, because the delivered order is not the written order.

`BoundedParse.cpp:118-132` sorts the members of every object, deliberately:

```cpp
// Schema diagnostics must not depend on the author's object member order.
std::function<void(Value &)> sort_members = [&](Value &value) {
    if (value.IsObject()) {
        std::sort(value.MemberBegin(), value.MemberEnd(), ...);
```

That is a sound decision, and JSON object member order is not significant in the first place. But
`components` is an object, so the written order of components is gone by construction, and stacking
among overlapping siblings fell out as alphabetical by component id. `dial` sorts before `face`, so
the needle was painted first and the face covered it. Chunk 11 was unaffected only by luck:
`gauge_fill` sorts before `speed`.

**Decision.** An optional integer `z_order` is added to the component schema, defaulting to 0, with
ties broken by component id so every render stays deterministic. Stacking is layout, `ARCHITECTURE`
says the document is the only source of layout, and before this there was no way for a document to
compose a dial out of a face and a needle at all.

This is a schema change, which this spec said it would not make. The alternative was layout by
naming convention, which is the kind of shortcut that needs refactoring later. The change is
additive and optional, so every existing document stays valid.

### C2. Criterion 4's control was measured and did not work

Criterion 4 proposed setting the root component's `scaling` to `stretch`, on the reasoning that it
maps to `EStretch::Fill`, which the engine documents as "Scales the content non-uniformly filling
the entire space of the area" (`SScaleBox.h`, the `EStretch` enum).

Measured, it does not. The control captured at 1280x660 gave a face of 367 by 366, a ratio of
1.0027: still square, and scaled uniformly by about 0.61 on both axes rather than 1.0 by 0.9167.
So `scaling: stretch` is not a lever that distorts, and a control built on it PASSES, which would
have left criteria 2 and 3 proving nothing.

**Decision.** The control tests `aspect_policy` instead, which was measured to work, in two halves
that differ by exactly one field: the face in a 600 by 400 rect renders 600 by 400 under `stretch`
(a ratio of 1.5, fifty percent off, correctly failing) and 400 by 400 under `preserve` (correctly
passing). Both are in the gate.

**What `scaling: stretch` actually does is now an open question**, recorded in the report rather
than papered over. Nothing in Stage 0 depends on it.

### C3. The needle carries a 3 percent band, not 1 percent

PLAN 4.6 asks for 1 percent. A bounding box measured from a capture carries up to one pixel of
error at each edge, so width and height each carry up to two. On the 550 pixel face that is 0.73
percent, comfortably inside 1 percent. On the 175 pixel needle it is 2.31 percent, so a 1 percent
band there could fail a correct render.

**Decision.** The face carries PLAN's 1 percent band and is the criterion PLAN asks for. The needle
carries 3 percent, stated with its arithmetic in `tools/gen-chunk12-fixture.py`, and a non-uniform
scale still shows 9.09 percent, a margin of three times. Both are measured and both are reported.

### C4. Criterion 6 is produced by the semantic pass, not by a builder

A dial under a directly declared `stretch` aspect policy is rejected at load with
`E_ASPECT_POLICY_FORBIDDEN` and the pointer `/dashboard/components/dial/aspect_policy`. The corpus
already covered it as `direct-stretch` and `inherited-stretch`; this chunk adds the package form so
the on-screen error is gated too.

`RefuseStretchedCircle` in the dial and circular gauge builders therefore cannot fire through
`LoadPackage`. It is kept as defence in depth, for the same reason chunk 11 kept its unreachable
unknown-type and unresolved-token errors, and the report says which layer produces it.

### C5. The validator protects the dial but not the face

`SemanticPass.cpp:273` rejects `stretch` on an `analog_dial` and on a circular `bar_gauge`. It says
nothing about the `image` that forms a dial's face, and criterion 4a is a document that stretches
one to a ratio of 1.5 and loads cleanly.

A stretched face ruins a dial exactly as thoroughly as a stretched needle would. This is recorded as
a finding rather than fixed here: tightening it means deciding which images are gauge faces, which
the schema gives no way to express, and inventing one is out of scope for this chunk.

### Correction to the facts block

Revision 1 said `ResolveAspectPolicy` "now matches that walk exactly". The walk is the same,
container ancestors only, but the terminal case is not identical: the semantic pass leaves an
all-`inherit` chain as `inherit` and its `stretch` check does not fire, while `ResolveAspectPolicy`
returns `preserve`. The two agree on every rejection, which is what matters, but "exactly" was not
exact.
