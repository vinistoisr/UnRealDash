# Chunk 11: the Stage 0 UMG primitives (PLAN 4.5)

All nine pass/fail criteria pass on this machine. The document now drives what is on screen: six
primitives, colours from the theme, missing-data states rendered structurally, and a screenshot gate
that has been watched failing on the mutation it names.

The spec needed a revision 4 before any of it could be built. Nine of revision 3's statements were
contradicted by the schema, the checked-in fixtures or the engine, and two mechanisms the criteria
depend on were never specified. Those are recorded as C1 to C9 in
`docs/build/chunk-11-primitives.md` and summarised below.

## Files added or changed

**DashboardSpec library** (PLAN 3.4, in support of 4.5)

- `Public/dashboard_spec/Document.h`, `Private/DocumentView.cpp`: `Document::Root()`, the dashboard
  object itself, so the reference viewport, bindings and pages are reachable. `Theme()` already
  exposed one member of it.
- `Public/dashboard_spec/PackageReader.h`, `Private/PackageReader.cpp`:
  `LoadedPackage::ResolveAssetName`, and the `PathResolver` kept past load so it can serve.

**UnRealDashCore** (PLAN 4.5)

- `Public/UnRealDashCore/DashTheme.h`, `Private/Package/DashTheme.cpp`: day and night token sets,
  `SetMode`, colour-union resolution, and `EDashSignalState`.
- `Public/UnRealDashCore/ComponentRegistry.h`, `Private/Package/ComponentRegistry.cpp`: the theme
  and the forced state added to `FComponentContext`; a per-type adopter so a parent decides how it
  takes a child.
- `Private/Package/Primitives/Layout.{h,cpp}`: rect, the nine anchors, clipping, aspect-policy
  inheritance, and the missing-data lookups.
- `Private/Package/Primitives/Primitives.h`, `PrimitiveSupport.cpp`: the shared panel, flat fill,
  status band and missing-data rendering.
- `Private/Package/Primitives/{Container,Readout,Image,Shape,Indicator,PageSwitch}Primitive.cpp`:
  one file per primitive.
- `Private/Package/Primitives/Stage0Builders.cpp`: `RegisterStage0Builders`, replacing
  `RegisterPlaceholderBuilders`.
- `Private/Package/DashPackageLoader.cpp`, `Public/UnRealDashCore/DashPackageLoader.h`:
  `ReferenceViewport()`, `Bindings()`, `Pages()`, and `Asset()` now taking a document reference.
- `Private/Package/DashPackageTestCommandlet.cpp`: theme loading, the new build signature, the
  API-level theme error checks, and `tree_code` expectations.
- `UnRealDashCore.Build.cs`: ImageWrapper, RenderCore and RHI for the image primitive.

**UnRealDash game module** (PLAN 4.5)

- `Private/Package/DashPackageScreen.{h,cpp}`: document layout at 1:1 with the reference viewport,
  `-udash-state=` and `-udash-shot=`, and the padded error screen kept for the rejection path.

**Fixtures and tooling** (PLAN 4.5)

- `tools/gen-chunk11-fixture.py`: the gate fixture and the ellipse refusal fixture, with the pixel
  arithmetic written into the module docstring.
- `tools/gen-package-fixtures.py`: skips cases marked `external`.
- `tests/fixtures/documents/valid/all-primitives-stage0.json`, `tests/fixtures/packages/cases.json`.
- `tests/fixtures/references/*.png` and `capture-environment.json`: the three checked-in frames.
- `scripts/run-capture-gate.ps1`: criteria 2, 3 and 4 as one runnable gate.
- `scripts/mutate-chunk11-fixture.py`: the criterion 3 mutation, which refuses to run if the fixture
  no longer declares the token it is supposed to change.
- `scripts/compare-capture.py`: the environment check made real (see below).
- `docs/build/chunk-11-primitives.md`: revision 4.

## Criteria

**1. All six primitives render, packed and unpacked.** The package commandlet builds a widget tree
for every accepted fixture in both forms.

```
Package gate cases=45 accepted=9 rejected=36 both_forms=28 archive_only=17 failures=0
```

**2. The capture matches the reference.**

```
criterion 2 (valid matches reference): PASS worst=0 moved=0.000000 (0 of 921600 pixels)  limits worst<=48 moved<=0.001
```

Worst zero and nothing moved: two runs of the same document produce bit-identical frames, which is
what the frozen capture conditions are for.

**3. The mutation breaks the gate, by a wide margin.**

```
criterion 3 (mutation): FAIL worst=233 moved=0.059637 (54961 of 921600 pixels)  limits worst<=48 moved<=0.001
criterion 3: moved=0.059637 (54961 pixels, needs over 0.01)
criterion 3 (restored): PASS worst=0 moved=0.000000 (0 of 921600 pixels)
```

54,961 pixels against a 9,216 threshold, six times over. The predicted figure was 57,600 (the
320 by 180 fill) minus the glyphs drawn on top of it, so the 2,639 pixel difference is the coverage
of "88.4" at font size 50. The arithmetic and the measurement agree.

**4. The three states are mutually distinguishable.**

```
criterion 4 pair valid/age_unknown: moved=0.017372 (16010 pixels, needs over 0.005)
criterion 4 pair valid/stale:       moved=0.020460 (18856 pixels, needs over 0.005)
criterion 4 pair age_unknown/stale: moved=0.021165 (19506 pixels, needs over 0.005)
```

The status band is 320 by 48, which is 15,360 pixels; the remainder is the glyph difference between
`88.4`, `88.4 ?` and `--`. Every pair clears the threshold by more than three times.

**5. Existing fixtures are not regressed.** `containers-three-deep.json` and `page-switch.json` are
in the commandlet's corpus, which reports zero failures.

**6. Three error cases, each readable, none a crash.** All three produce an error naming the JSON
pointer, and the report has to say which layer produces each, because two of them never reach a
builder:

| case | produced by | evidence |
| --- | --- | --- |
| unknown component type | schema validation, at load | `tests/fixtures/documents/invalid/unknown-component-type`; and `FComponentRegistry::Build` checked directly in the commandlet |
| missing theme token | the semantic pass, at load | `SemanticPass.cpp:308`; and `FDashTheme::Resolve` checked directly in the commandlet |
| `shape` with `kind: "ellipse"` | this chunk's shape builder | `shape-ellipse-refused`, gated by `tree_code` in `cases.json` and captured on screen |

The on-screen error for the third reads:

```
File: C:\Users\Vincent\UnRealDash\tests\fixtures\packages\shape-ellipse-refused.udash
Code: E_SCHEMA
Number: 7
Pointer: /dashboard/components/curve/properties/kind
Message: Shape kind ellipse is not drawn: a curve that is not a corner radius is artwork and
belongs in an image asset
```

That the load path stops the other two is the correct outcome rather than a gap. It does mean
criterion 6 is mostly a re-test of chunk 10, which is why the table above exists.

**7. Both CMake suites green, no drop in counts.**

```
dashboard-spec: cases=28 failed_cases=0 assertions=1002108 failed_assertions=0
signal-core:    cases=136 failed_cases=0 assertions=111782 failed_assertions=0
```

The chunk 10 report recorded `assertions=1002083` for dashboard-spec, so this is up 25 and down
none. No prior figure for signal-core is recorded anywhere in `docs/reports/`; this chunk touched
nothing in it, and 136 / 111,782 is the number to compare against next time.

**8. Doctor and Pester.** `doctor.ps1 -Profile workstation` exits 0 with the layering check at
0 token violations. Pester: 76 passed, 0 failed.

The Pester suite caught a real defect in this chunk's own work: `run-capture-gate.ps1` used the
PowerShell 7 ternary, which the repository forbids because its scripts must parse under 5.1.

**9. UBT builds Win64 and Android ARM64.** Both `Result: Succeeded`. Android is a build check here,
not a device run.

## What the spec got wrong, and what the review caught

Revision 3 was frozen without checking its deliverables against the schema. Nine corrections, in
`docs/build/chunk-11-primitives.md`:

- **C1** `missing_data` is required by the schema on every component, so the rule that declaring it
  on a non-signal primitive is an error would have rejected two fixtures criterion 5 requires.
- **C2** The same for the matrix's n/a cells: `page-switch.json` declares `dash` on an `indicator`.
- **C3** The schema admits `ellipse`; this chunk refuses it. That became criterion 6's third case.
- **C4** A readout has no format or precision; the binding carries the format.
- **C5** An image has no `tint` field, so images are never tinted.
- **C6** An indicator has no on and off artwork, only a colour.
- **C7** Two of criterion 6's three cases never reach a builder.
- **C8** No capture trigger existed for the package screen, so criteria 2, 3 and 4 could not be run
  at all; and an `icon` presentation has no icon asset field in the schema.
- **C9** The review findings, below.

Two facts in revision 3 were also simply wrong: `nine-primitives.json` does cover `image` and
`shape`, and the corpus test walks the directory rather than a list.

### The review

DeepSeek reviewed revision 4 against the tree before the build and again against the built code.
Four findings were real.

**Criterion 4 could not have failed for the primitive it names.** This is the one that mattered.
`-udash-state` forced every signal-capable primitive at once, so the image and the lamp between them
moved 66,816 pixels of the frame. The three states would have been distinguishable even if the
readout had rendered identically in all three.

The fix is a semantics change, not a gate tweak: a component the document binds to no signal has no
signal state to present, and `ApplyMissingData` now returns before rendering one. Missing data is a
property of a signal, and a component bound to nothing has no signal that could be stale. The gate
fixture binds only `speed`, so criterion 4 now rests on the readout alone. The three captures in
`tests/fixtures/references/` show everything outside the readout identical across all three states.

**The `age_unknown` badge was drawn in the value colour.** The `icon` presentation appended a glyph
to the existing text block, so the state's token colour appeared only in the band. The badge is now
its own text block carrying the state token.

**The capture environment check was a no-op.** `compare-capture.py` skipped any key whose supplied
value was empty, and `run-capture-gate.ps1` supplied none, so a reference captured on different
hardware would have been compared as if it came from this machine. A key the caller does not supply
is now a mismatch, and the environment is passed as repeated `--env KEY=VALUE` taken from the
running machine rather than as two hardcoded flags. The first run after the fix reported
`NOT RUN: reference was captured on a different engine (not supplied), resolution (not supplied)`,
which is the check working.

**Criterion 3 named the wrong component.** It says "the `readout` fill token"; a readout has no fill
token, and changing its text colour would move only glyph pixels and could fall under the threshold.
The target is `gauge_fill`. `mutate-chunk11-fixture.py` mutates that one and refuses to run if the
fixture stops declaring it.

**One finding was not accepted.** The review read the status band as laid out below the panel and
clipped away, and the readout's content area as growing by 48 rather than reserving 48. Both depend
on `UCanvasPanelSlot` offset semantics, which differ by axis. Settled by reading
`Engine/Source/Runtime/Slate/Private/Widgets/Layout/SConstraintCanvas.cpp` lines 246 to 281: on a
stretched axis `Offset.Bottom` is subtracted, so it is an inset; on a point-anchored axis it is the
size, and `Alignment` moves the widget back by it. The band computes to position 132, size 48, in a
panel 180 tall. The measured capture agrees: the pairs moved 16,010 to 19,506 pixels around a
15,360 pixel band, which cannot happen if the band is clipped away.

## A defect this chunk found in chunk 10

`FDashPackage::Asset` took a name already normalized, and nothing outside the library could
normalize one. The placeholder builders never called it, so the two `normalized-reference` package
fixtures passed while their image references could never have resolved. With real builders they
failed immediately.

The fix belongs in the library, not in the caller: `LoadedPackage::ResolveAssetName` exposes the
`PathResolver` the reader already builds. Restating those rules engine-side would have been a second
copy of the package path gate, drifting from the first.

## What was not run, and why

- **No device run.** Chunk 11's gate is desktop captures by design, and the spec's non-goals say so.
  The Android half is a build check.
- **The capture gate does not run in CI and must not be made to.** GPU, driver and font
  rasterisation all move the comparator's numbers and the hosted runners have no GPU. The references
  are specific to `NVIDIA RTX A2000 12GB`, driver `32.0.15.9706`, engine `5.8.2`, at 1280x720.
  Another machine gets `NOT RUN`, never a pass and never a failure.
- **The ThreadSanitizer preset** was not re-run. Nothing in this chunk touches signal-core or any
  threaded path.
- **Format and precision are implemented but not measured.** The gate fixture binds `speed` with
  `format: ".1f"` and its text is already `88.4`, so the formatter runs but the frame would look the
  same without it. A fixture whose text needed rounding would measure it; that is worth adding when
  a readout is fed by a live value in 4.9.

## Known limitations

- **`scaling` is honoured only on the root component**, where it selects how the document maps to a
  viewport that differs from the reference viewport. At 1:1 no option is observable, which is the
  condition the gate captures under. A non-root component's `scaling` currently does nothing.
- **The `icon` presentation is a text badge** because the schema gives a missing-data state only a
  presentation and a token, with no icon asset field.
- **An indicator is always drawn off**, since no rule is evaluated in this chunk. Lighting a lamp
  with no rule behind it would report a condition nothing measured.
