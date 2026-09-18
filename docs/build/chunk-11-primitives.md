# Build chunk 11: the Stage 0 UMG primitives

Status: revision 4. Revision 3 was frozen against a spec that six schema facts contradict; the resolutions are the last section of this file and they, not revision 3, are what gets built. Revision 1 drew six blockers and the verdict that it was a narrative design document rather than a build order. Revision 2 resolved all nine, then drew one more that mattered: the comparator thresholds were never tied to the size of the thing being compared, so the mutation check could have passed the gate it exists to break. Revision 3 pins the fixture geometry and writes the arithmetic down. Earlier: a spec review returned six blockers and the verdict that revision 1 was "a narrative design document, not a build order". That was correct: it described intent where it needed to state decisions, and the screenshot gate in particular was unbuildable. Frozen for a Codex build session. Covers PLAN.md task 4.5. Read PLAN.md (task 4.5, task 4.6 so you know what you are not building, task 3.x for the document model, the Sequencing block) and docs/ARCHITECTURE.md before writing anything. PLAN.md is the authority on what; this file adds the exact mechanisms, proof commands and implementation constraints. If the two disagree, PLAN.md wins and the disagreement goes in the report.

## Goal

This is the task that makes the project look like a product rather than plumbing. Everything underneath it exists and is proven: the document loads, validates and reaches the engine through `FDashPackage`, and `FComponentRegistry` is waiting for builders keyed by component type.

## Facts this chunk depends on

Every line here was checked against the tree, not recalled. Re-check them with the same commands before building; a fact with no command behind it is an assumption.

- The schema defines **nine** component types in `packages/dashboard-spec/schema/dashboard.schema.json`: `readout`, `image`, `shape`, `analog_dial`, `bar_gauge`, `indicator`, `history_graph`, `container`, `page_switch`.

  **This chunk owns six of them**: `readout`, `image`, `shape`, `indicator`, `container`, `page_switch`. PLAN 4.6 owns `analog_dial`, `bar_gauge` and `history_graph`. Do not build those three, and do not register placeholder builders for them that look finished.
- The same schema defines `anchor` (nine positions), `scaling` (`uniform`, `stretch`, `none`) and `aspect_policy` (`preserve`, `stretch`, `inherit`) per component.
- Missing data is declared per component under `missing_data`, with four independent states: `stale`, `unavailable`, `invalid` and **`age_unknown`**. Each takes a `presentation` of `dash`, `hidden`, `last_value_dimmed` or `icon`. PLAN 4.5's gate turns on `age_unknown` being visibly distinct from both valid and stale, and the schema already has the field for it.
- `FComponentRegistry` is delivered and matches what chunk 10 pinned:

  ```cpp
  struct FComponentContext { const FDashComponent& Component; const FDashPackage& Package;
                             EDashProfile Profile; UWidget* Parent; };
  using FComponentBuilder = TFunction<UWidget*(const FComponentContext&, FDashLoadError&)>;
  ```

  `FWidgetTreeBuilder::Build` walks the tree and `WidgetsById()` returns the id to widget map. `RegisterPlaceholderBuilders` is what this chunk replaces.
- `FDashPackage::Asset(name, out, error)` returns package asset bytes, and `FDashComponent` exposes type, id, parent id, JSON pointer and typed properties, with **no `dashboard_spec::` type in either**. The layering check in `scripts/doctor.ps1` enforces that the game module names neither library.
- The existing document fixtures in `tests/fixtures/documents/valid/` (27 files) cover `container`, `readout`, `indicator`, `page_switch`, `analog_dial` and `history_graph`. **None covers `image` or `shape`.** The richest is `page-switch.json` with three types. This chunk authors the fixture its own gate needs.
- Runtime PNG import into a `UTexture2D` is already proven on the device by PLAN 4.0, through `IImageWrapperModule`, `UTexture2D::CreateTransient` and `UpdateResource`.
- **`FScreenshotRequest::RequestScreenshot` takes `bShowUI` as its second argument, and it must be `true` here.** With `false` the capture excludes all UI and writes a black image. That was found on the device on 2026-09-17: everything this project draws is UI, so a gate comparing captures taken with `false` would compare two black images and pass for every fixture, forever. See `docs/reports/2026-09-17-device-package-gate.md`.

## The design decision this chunk exists to encode

**Primitives consume artwork. They do not generate ornament.**

The reference for quality is the owner's own RealDash cluster for the Scirocco, in `C:\Users\Vincent\scirocco-dash`. Its gauge faces are authored as SVG and rendered to transparent PNG at 2x by `dash/generate_assets.py`: chrome bezel, a dense band of fine white ticks with heavier majors, red ticks for the redline drawn inside that band rather than as a filled arc, and large numerals sitting inside the band. At runtime only two things move: a needle sprite rotates, and text values change.

That is the shape this chunk must support, and the package format already assumes it. Packages carry PNG assets with a manifest, per-profile texture budgets and image dimension limits, and `image` is a first-class component type.

So: the `image` primitive is the workhorse, not a decoration. A dial face, a bezel, an indicator icon and a background are all images positioned by the document. Do not draw bezels, tick bands or numerals in code. A primitive that generates its own ornament cannot be restyled by a document, which defeats the point of having a document.

One trap worth copying from the owner's notes: RealDash multiplies a per-gauge colour over image gauges, so every image gauge must be set to pure white or the artwork renders tinted. Theme tokens here can make exactly that mistake. A token that tints an image must be opt-in per component, never a default.

## Deliverables

### 1. Six builders, registered and nothing else

In `UnRealDashCore`, `Private/Package/Primitives/`, one file per primitive, registered through a single `RegisterStage0Builders(FComponentRegistry&, UWidgetTree&)` that replaces `RegisterPlaceholderBuilders`. Adding a primitive later must mean adding one file and one registration line, and touching neither the registry nor `FWidgetTreeBuilder`.

- **`container`**: a panel that positions children by `anchor`, honours `scaling`, and clips. Containers nest; `containers-three-deep.json` already exists as a fixture and must render.
- **`readout`**: text and numeric, with format, units and precision from the document. This is the primitive the missing-data gate is measured on.
- **`image`**: resolves an asset name through `FDashPackage::Asset`, decodes with `IImageWrapperModule`, and honours `aspect_policy`. `preserve` must never scale non-uniformly. A missing asset is a readable error, not a blank space.
- **`shape`**: the one primitive allowed to draw rather than sample artwork, and the boundary is operational rather than a matter of taste. It may draw **only** axis-aligned rectangles, rounded rectangles and straight lines, with a single uniform fill and a single uniform stroke. Anything with a curve that is not a corner radius, any gradient, any text, and any repeated element such as a tick band must be an image asset. If a visual element cannot be expressed under that rule, it is artwork, and drawing it in code is the defect this chunk exists to prevent.
- **`indicator`**: a lamp with on and off artwork, driven by a rule result. Off must be visibly off rather than absent, so a dark cluster does not look broken.
- **`page_switch`**: swaps the visible page. `page-switch.json` already exists and must render both pages.

### 2. Theme tokens, day and night

Colours, and only colours, come from the document's theme section. Two token sets, day and night.

- Selection is one engine-side call, `FDashTheme::SetMode(EDashThemeMode::Day | Night)`, defaulting to `Day`. There is no document field for it and no automatic switching; 5.x owns that.
- A component referencing a token that does not exist is an **error** naming the token and the component's JSON pointer, surfaced the same way a malformed component is. It is not a silent fallback to white, because a silently defaulted colour looks like a design choice and docs/ARCHITECTURE.md forbids silent fallbacks.
- A token is applied to an `image` only when that component sets `tint` explicitly. Default is no tint. This is the RealDash trap above: a per-gauge colour multiplied over artwork by default turns every face the theme colour.

### 3. Missing-data rendering, with the matrix pinned

Only primitives that bind a signal have missing-data states. That is `readout`, `image` and `indicator`. **`container`, `shape` and `page_switch` bind no signal and are exempt**; if the document declares `missing_data` on one of those, it is an error naming the pointer, not something to implement.

Required combinations, and nothing else is required in this chunk:

| primitive | dash | hidden | last_value_dimmed | icon |
| --- | --- | --- | --- | --- |
| `readout` | required | required | required | required |
| `image` | n/a, no text to dash | required | required | required |
| `indicator` | n/a | required | required | required |

A presentation marked n/a in a document is an error naming the pointer.

**`age_unknown` must be structurally distinct, not merely dimmer.** A signal fed by a `held` definition-pack field carries a real measurement whose age is unknown, so it is neither valid nor stale. For the gate's `readout` the three states are rendered as:

- **valid**: the value text alone.
- **age_unknown**: the value text plus a distinct badge glyph adjacent to it, in the `age_unknown` token colour.
- **stale**: the `dash` presentation, so no value text at all.

Those three differ in glyph coverage, not opacity, which is what makes criterion 3 measurable rather than a judgement call. A pair that differed only by a few percent of alpha would pass a tolerance test while looking identical.

### 4. The fixture, the capture conditions and the comparator

No existing fixture covers `image` or `shape`, so author `tests/fixtures/documents/valid/all-primitives-stage0.json` exercising all six, plus the package assets it needs. It must pass the existing validator and semantic pass unchanged, and be added to whatever list the C++ and Python validators already walk.

**Capture conditions are part of the gate.** A screenshot comparison is only meaningful if the frame is deterministic, and by default it is not: temporal antialiasing, auto exposure, bloom and animated values all move between runs on the same machine.

Capture with exactly this, and record it in the test rather than in a person's memory:

```
-windowed -ResX=1280 -ResY=720 -nosplash -unattended
-ExecCmds="r.PostProcessAAQuality 0, r.DefaultFeature.AntiAliasing 0, r.DefaultFeature.Bloom 0, r.DefaultFeature.AutoExposure 0, r.DefaultFeature.MotionBlur 0, r.ScreenPercentage 100, r.Tonemapper.Sharpen 0"
```

Values must be frozen, not sampled: the fixture binds constants, not a running scenario, so two captures of the same document are identical by construction. If any primitive needs a live value to render, that is a defect in this chunk, not a reason to loosen the comparator.

**The comparator is specified, not left to judgement.** Implement it once, in `scripts/compare-capture.py`, and use it for every criterion below.

- Both images must have identical dimensions. A size mismatch is an immediate fail, never a resize.
- Compare 8-bit RGB. **Alpha is ignored**, because the captures are opaque and an alpha channel that differs invisibly would fail the gate for no reason.
- Per pixel, `d(p) = max over channels of |reference - candidate|`.
- Report two numbers every run, pass or fail: `worst = max d(p)` and `moved = fraction of pixels with d(p) > 8`.
- **Pass when `worst <= 48` and `moved <= 0.001`.**

The two-part rule is deliberate. A single max-delta rule fails on one antialiased glyph edge; a single percentage rule lets a whole component change colour as long as it is small. Requiring both means text edges may differ slightly while any component actually changing appearance is caught. The threshold of 8 is below what a human sees on a dark panel and above driver-level rounding; 0.1 percent of a 1280x720 frame is about 920 pixels, roughly one glyph's worth of edge.

**The thresholds only mean anything if the fixture is big enough to move them.** A review of revision 2 caught this: if the readout under test is small, recolouring it moves fewer pixels than the pass threshold, so the mutation check would pass the gate it is supposed to break. The fixture is therefore constrained, and the arithmetic is written down rather than assumed.

A 1280x720 frame is 921,600 pixels, so the thresholds are:

| threshold | fraction | pixels |
| --- | ---: | ---: |
| pass, `moved` at most | 0.001 | 922 |
| three states differ, `moved` over | 0.005 | 4,608 |
| mutation fails, `moved` over | 0.01 | 9,216 |

The fixture must satisfy them by construction:

- The readout under test is **320 by 180** with an opaque filled background, which is 57,600 pixels. Recolouring its fill token moves all of them, `moved` about 0.0625, which is six times the mutation threshold rather than scraping past it.
- Each of the three states additionally paints a **status band across the readout's full width, 320 by 48**, in that state's token colour: 15,360 pixels, over three times the distinguishability threshold. The band is what makes criterion 4 hold regardless of how much area a glyph happens to cover, because glyph coverage is a font metric and not something this spec can pin.

The band is not a trick to satisfy the gate. A dashboard that shows signal state as a visible band rather than as slightly dimmer text is the more usable design, and it is what makes the state legible at a glance in a moving car.

Record these numbers in the test alongside the thresholds. If a later fixture changes size, the arithmetic has to be redone, and the test should say so.

**References are machine-specific and this gate does not run in CI.** GPU, driver and font rasterisation all move these numbers, and the hosted runners have no GPU. Check the references in under `tests/fixtures/references/`, and beside them a `capture-environment.json` recording GPU name, driver version, engine version and resolution. A mismatch between that file and the running machine is reported as **not run**, never as a pass and never as a failure. Say so in the report.

## Constraints

- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module. `scripts/doctor.ps1` enforces it and the Pester test fails the build if it is broken.
- No hand-authored widget Blueprints. docs/ARCHITECTURE.md forbids it: the document is the only source of layout.
- Nothing switches on a component type string outside the registry.
- Errors are values. A malformed component is a readable on-screen error naming the JSON pointer, never a crash and never a silent skip.
- `RequestScreenshot`'s `bShowUI` is `true`. Anything else makes the gate meaningless.
- No em dashes in any file, comment or printed string.

## Non-goals

- `analog_dial`, `bar_gauge` and `history_graph`. Those are PLAN 4.6.
- The command-line surface, which is 4.7. Use what 4.4 already wired.
- The debug overlay, which is 4.8.
- Any device work. The gate here is desktop captures.
- Authoring beautiful artwork. This chunk proves the primitives consume artwork correctly; a fixture PNG may be plain.

## Pass/fail criteria

Each is a command that exits non-zero on failure. Criteria 2, 3 and 5 all use `scripts/compare-capture.py` and its two numbers.

1. `all-primitives-stage0.json` renders every one of the six primitives, in both packed and unpacked package form.
2. A capture of that fixture, taken under the conditions above, passes against the checked-in reference: `worst <= 48` and `moved <= 0.001`. Print both numbers.
3. **The mutation check, named exactly so it is evaluable.** Change the `readout` fill token in the fixture from its theme value to `#FF00FF`, recapture, and compare against the same reference with the same comparator. It must fail, and `moved` must exceed **0.01**, that is more than ten times the pass threshold. A mutation that only just crosses the line proves the comparator is twitchy rather than that the gate works. Restore the fixture and confirm it passes again.
4. Three captures of the same `readout` under valid, `age_unknown` and stale conditions are mutually distinguishable: for each of the three pairs, `moved` exceeds **0.005**. Report all three pair numbers, not just a pass. This is the same comparator as criterion 2, so a builder cannot satisfy one metric here and another there.
5. `containers-three-deep.json` and `page-switch.json` still render, so existing fixtures are not regressed.
6. An unknown component type, a missing theme token, and `missing_data` declared on a primitive that binds no signal each produce a readable error naming the offending JSON pointer, and none of them crashes.
7. Both CMake suites stay green with no drop in case or assertion count, taken from the doctest reporter rather than counted by hand.
8. `scripts/doctor.ps1 -Profile workstation` exits 0 including the layering check, and the full Pester suite passes with `-CI`.
9. UBT builds Win64 and Android ARM64. Android is a build check here, not a device run.

## Proof

Codex runs these and pastes the output verbatim.

```
cd packages/dashboard-spec && cmake --preset default && cmake --build --preset default
cd packages/dashboard-spec && ctest --preset default --output-on-failure
cd packages/signal-core && ctest --preset default --output-on-failure
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI -Output Detailed"
git status --short
```

On this machine a Ninja configure needs a developer environment:
`cmd /c "call \"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat\" >nul 2>&1 && cmake --preset default"`. Without it you get `LNK1104 cannot open file kernel32.lib`, which is an environment problem and not a code one.

`[claude]` The UBT builds, the fixture renders, the reference captures, the mutation check in criterion 2 and the three-state comparison in criterion 3.

Codex's pasted proof is advisory. Claude re-runs everything.

## Report format

End with: files added or changed (one line each: path, what, which PLAN.md task), the proof output verbatim for what you ran, the per-pixel tolerance chosen and why, the measured difference between each pair of states in criterion 3, an explicit list of what you did not run and why, any deviation from PLAN.md or this spec with the reason, and anything you could not do.

---

## Revision 4: six conflicts between this spec and the schema, and how each is resolved

Revision 3 was frozen without checking every deliverable against
`packages/dashboard-spec/schema/dashboard.schema.json` and the checked-in fixtures. Six of its
statements are contradicted by the tree. Each is recorded here with the command that shows it and
the decision that replaces it. Nothing below changes the schema: the fixture this chunk authors
must pass the existing validator and semantic pass unchanged, so the schema is the fixed point and
this spec is what moves.

### C1. `missing_data` is required on every component, so its presence can never be an error

Revision 3, deliverable 3: "`container`, `shape` and `page_switch` bind no signal and are exempt;
if the document declares `missing_data` on one of those, it is an error naming the pointer."

The schema lists `missing_data` in the component `required` array, with all four states required
inside it. Every component in every valid fixture therefore declares it, including components that
bind no signal:

```
python -c "import json;d=json.load(open('tests/fixtures/documents/valid/missing-data-four-states.json'));print(d['components']['root']['type'], list(d['components']['root']['missing_data']))"
container ['stale', 'unavailable', 'invalid', 'age_unknown']
```

`page-switch.json` declares it on a `page_switch` in the same way. Implementing revision 3's rule
would reject both fixtures, which criterion 5 requires to keep rendering. The rule is unbuildable
and is withdrawn.

**Decision.** For `container`, `shape` and `page_switch` the `missing_data` object is parsed and
ignored. This is not a silent fallback: those primitives have no signal, so there is no state to
present, and the schema gives the document no way to omit the field.

### C2. The n/a cells in the presentation matrix are also unenforceable

Revision 3: "A presentation marked n/a in a document is an error naming the pointer." The matrix
marks `dash` n/a for `image` and `indicator`. But `page-switch.json` declares
`unavailable: {presentation: "dash"}` on its `indicator`, and criterion 5 requires that fixture to
render. The rule contradicts the same checked-in corpus.

**Decision.** No cell is an error. `dash` on a primitive with no value text renders the state band
in the state's token colour with the component's content hidden. That is a defined rendering, named
here, and the same three-way structural distinction criterion 4 measures still holds.

### C3. `shape` may be `ellipse` in the schema and may not be a curve in this spec

The schema's `kind` enum is `["line", "rectangle", "ellipse"]`. Revision 3 allows only axis-aligned
rectangles, rounded rectangles and straight lines, on the grounds that anything curved is artwork.
No valid fixture uses `ellipse`:

```
grep -l ellipse tests/fixtures/documents/valid/*.json   # no output
```

**Decision.** The builder implements `line` and `rectangle`. `ellipse` produces a readable error
naming the component's JSON pointer and saying that a curve is artwork and belongs in an `image`
asset. It does not crash and it does not draw an approximation.

**This replaces the third error case in criterion 6**, which was "`missing_data` declared on a
primitive that binds no signal" and is withdrawn under C1. The property under test is unchanged: a
document the schema accepts but this runtime cannot honour produces a readable, pointer-naming
error rather than a crash or a silent skip.

### C4. `readout` has no format, units or precision; the binding does

Revision 3, deliverable 1: "`readout`: text and numeric, with format, units and precision from the
document." The schema's `readout` properties are `text` (required) and `colour` (optional), and
nothing else; `additionalProperties` is false. Format lives on the binding instead:

```
"bindings": { "root": { "property": "value", "signal": "temperature", "format": ".1f" } }
```

and the unit is a property of the signal definition (`"unit": "K"`).

**Decision.** The readout renders `text`, coloured by `colour` when present, which is either a
literal `#rrggbb` or `{"token": "..."}`. Format and precision are read from the binding's `format`
field for the component. Units come from the signal. No schema change, no invented field.

### C5. `image` has no `tint` property

Revision 3, deliverable 2: "A token is applied to an `image` only when that component sets `tint`
explicitly." The schema's `image` properties are `asset` and nothing else.

**Decision.** Images are never tinted in this chunk. The RealDash trap the deliverable was guarding
against is avoided by construction rather than by a flag. If a later chunk adds tinting it must add
the field as opt-in, and this paragraph is the reason.

### C6. `indicator` has no on and off artwork

Revision 3, deliverable 1: "`indicator`: a lamp with on and off artwork." The schema's `indicator`
properties are `colour` and nothing else. There are no asset fields to resolve.

**Decision.** The lamp is drawn from its `colour`. On is the colour at full value; off is the same
hue at 18 percent value, which keeps it visibly present against the panel background rather than
absent. The requirement that off must not look like a broken cluster is met; the artwork half is a
PLAN 4.6 concern.

**Correction, made while planning chunk 12.** This paragraph originally said the PLAN 4.6 dial
primitives bring their own asset fields. They do not: `analog_dial` declares only `minimum`,
`maximum` and `colour`, `bar_gauge` adds `circular`, and `history_graph` declares `history_samples`
and `colour`. No component type in the schema except `image` names an asset. Artwork reaches a dial
the way it reaches every other primitive, as `image` components the document positions behind it,
which is the composition the RealDash reference uses and the one this chunk's design rule already
implies.

**A second correction.** `ResolveAspectPolicy` took the policy from any ancestor that declared one.
The semantic pass takes it from `container` ancestors only and walks past every other type
(`SemanticPass.cpp:267-272`). The two have to agree, because the pass is what rejects a circular
gauge under an inherited `stretch`, and a runtime that resolved `inherit` differently could scale a
dial non-uniformly that the validator had accepted. The runtime now matches the pass.

### Two further corrections to the facts block

- Revision 3 says the existing fixtures cover neither `image` nor `shape`. That is wrong:
  `nine-primitives.json` contains an `image` with `assets/shared.png` and a `shape` with
  `kind: "line"`. Authoring `all-primitives-stage0.json` is still required, but for the reason in
  the threshold arithmetic rather than for coverage: no existing fixture pins the geometry that
  criteria 2, 3 and 4 measure.
- The corpus test walks the fixture directory rather than a checked-in list
  (`test_corpus.cpp`, `for (const char *side : {"valid", "invalid"})`), so a new fixture is picked
  up with no list to update. `CHECK(valid >= 20)` stays green.

### Two mechanisms revision 3 needed and did not specify

**M1. Layout must be 1:1 with `reference_viewport` or the pixel arithmetic is meaningless.**
`UDashPackageScreen::RebuildWidget` currently wraps the built root in a `USizeBox` of width 1160
inside a `UBorder` with 24 units of padding. Under that wrapper a component declared 320 by 180 does
not occupy 320 by 180 pixels of a 1280 by 720 capture, and the 57,600 pixel figure criterion 3
depends on is wrong. `reference_viewport` is also not exposed by `FDashPackage`.

**Decision.** Add `FDashPackage::ReferenceViewport()`. For an accepted package the screen lays the
document out in a `UScaleBox` over a `USizeBox` sized to the reference viewport, so a 1280 by 720
document captured at 1280 by 720 maps one document unit to one pixel. The existing padded wrapper
is kept for the error path, where no document geometry exists to honour.

**M2. Criterion 4 needs a deterministic way to force each missing-data state.**
The fixture binds constants, so nothing in the document can put a signal into `age_unknown` or
`stale`, and the capture conditions forbid a running scenario.

**Decision.** A gate-only launch switch, `-udash-state=valid|age_unknown|stale`, forces every
signal-bound primitive into that state for the run. It sits beside `-udash-batch=`, which is
already a gate switch and deliberately outside the runtime command-line surface PLAN 4.7 owns.
Values stay frozen; only the state presentation changes, which is exactly what criterion 4
measures.

### C7. Two of criterion 6's three error cases never reach a builder

Verified rather than assumed:

- An unknown component type is rejected by schema validation. The component `type` is an enum of
  the nine names and the final `else` branch is `{"properties": {"type": false}}`, so a tenth name
  fails before the package loads.
- A missing theme token is rejected by the semantic pass, for component colours and for
  `missing_data.*.token` alike (`SemanticPass.cpp:308`, `E_UNRESOLVED_THEME_TOKEN`, and the loop at
  line 327 that walks every `missing_data` state's token).

So a loaded package can carry neither. `FComponentRegistry::Build`'s unknown-type error and
`FDashTheme::Resolve`'s unresolved-token error are kept as defence in depth, because both are
callable outside a validated document, but neither is reachable through `LoadPackage`.

**Decision.** Criterion 6 still runs all three cases end to end and still requires a readable
pointer-naming error and no crash from each. The report must say which layer produced each one.
Only the `ellipse` case of C3 exercises this chunk's own error handling; the other two verify that
chunk 10's load path is doing its job. That is the correct outcome rather than a gap: the load path
being thorough is why so little malformed input can reach a builder.

### C8. The gate had no capture trigger, and the icon presentation has no icon

Two more gaps, both found while building rather than while writing revision 4.

**The capture trigger.** Criteria 2, 3 and 4 all require a screenshot of the package screen, and
nothing in `UDashPackageScreen` or `ADashPackageHUD` requested one. The only
`FScreenshotRequest::RequestScreenshot` in the tree was in the 4.0 smoke spike, which is a
different HUD. The non-goals section said to use what 4.4 already wired, and 4.4 wired no capture.
Without one the three criteria could not be run at all.

**Decision.** `ADashPackageHUD` gains `-udash-shot=<path>`, a gate switch beside `-udash-batch=`
and `-udash-state=`, which captures after the widget tree has been laid out and drawn and then
exits. `bShowUI` is `true`, for the reason already in the facts block: everything this project
draws is UI, and a capture taken with `false` writes a black image, which would make the
comparator pass every fixture forever.

**The icon presentation.** A `missing_data` state declares a `presentation` of `icon` but the
schema gives it only a `presentation` and a `token`. There is no icon asset field, so the image
primitive can consume artwork through `asset` while an `icon` presentation cannot. It is rendered
as a text badge beside the value, in the state's token colour.

**Decision.** The badge is what the document actually offers, and it is recorded here rather than
presented as a design choice. Criterion 4 does not rest on it: the status band carries the
measurable difference, because glyph coverage is a font metric this repository cannot pin.

### C9. What the spec review of revision 4 changed

DeepSeek reviewed revision 4 against the tree before the build and again against the built code.
Five findings were real and are fixed; one was an engine-semantics claim it could not read in this
repository, and that one was settled by measurement rather than by argument.

**Criterion 4 could not have failed for the primitive it names.** This is the finding that mattered.
`-udash-state` forced every signal-capable primitive into the state at once, so in the gate fixture
the image and the lamp between them moved 66,816 pixels of a 921,600 pixel frame. The three states
would have been mutually distinguishable even if the `readout` the criterion names had rendered
identically in all three.

**Decision.** A component the document binds to no signal has no signal state to present, and
`ApplyMissingData` now returns before rendering one. That is the honest semantics independently of
the gate: missing data is a property of a signal, and a component bound to nothing has no signal
that could be stale. It also isolates criterion 4 to `speed`, the fixture's only bound component,
so the three pairs now rest on the readout's status band and glyphs alone.

**The `age_unknown` badge was drawn in the value colour, not the state colour.** The `icon`
presentation appended a glyph to the existing text block, which carries the readout's own colour, so
the state's token colour appeared only in the band. The badge is now its own text block coloured
with the state token.

**The capture environment check was a no-op.** `compare-capture.py` skipped any key the caller left
empty, and `run-capture-gate.ps1` passed neither `--gpu` nor `--driver`, so a reference captured on
different hardware would have been compared as though it came from the same machine. A key the
caller does not supply is now a mismatch rather than agreement, and the gate script reads the
adapter and passes both.

**Criterion 3 names the wrong component.** Criterion 3 above says to change "the `readout` fill
token". The `readout` has no fill token; its only colour is its text colour, and changing that would
move glyph pixels alone and could fall under the 9,216 pixel threshold. The mutation target is
`gauge_fill`, the 320 by 180 `shape` behind the readout, at pointer
`/dashboard/components/gauge_fill/properties/colour/token`. `scripts/mutate-chunk11-fixture.py`
mutates that one and refuses to run if the fixture no longer declares it, so the criterion is
evaluable as implemented; this paragraph corrects the wording.

**The claim that was not accepted.** The review read the status band's canvas slot as being laid out
below the panel and clipped away, and the readout's text area as growing by 48 units rather than
reserving 48. Both readings depend on `UCanvasPanelSlot` offset semantics, which differ by axis
according to whether that axis is anchored to a point or stretched: on a stretched axis the offsets
are insets, and on a point axis they are position and size. The band is anchored to a point
vertically and stretched horizontally, so `FMargin(0, 0, 0, 48)` is full width, bottom aligned, 48
tall.

A reviewer that cannot run the engine should not be taken on trust here, and neither should a
recollection of the engine's behaviour, so this was settled by reading
`Engine/Source/Runtime/Slate/Private/Widgets/Layout/SConstraintCanvas.cpp` lines 246 to 281:

```
const bool bIsVerticalStretch = Anchors.Minimum.Y != Anchors.Maximum.Y;
const FVector2D SlotSize = FVector2D(Offset.Right, Offset.Bottom);
const FVector2D Size = AutoSize ? CurWidget->GetDesiredSize() : SlotSize;
FVector2D AlignmentOffset = Size * Alignment;
...
if (bIsVerticalStretch) {
    LocalPosition.Y = AnchorPixels.Top + Offset.Top;
    LocalSize.Y = AnchorPixels.Bottom - LocalPosition.Y - Offset.Bottom;
} else {
    LocalPosition.Y = AnchorPixels.Top + Offset.Top - AlignmentOffset.Y;
    LocalSize.Y = Size.Y;
}
```

On a stretched axis `Offset.Bottom` is subtracted, so it is an inset: the readout's content area is
180 minus 48, which is 132, not 180 plus 48. On a point-anchored axis `Offset.Bottom` is the size
and `Alignment` moves the widget back by it: for the band, `AnchorPixels.Top` is 180,
`AlignmentOffset.Y` is 48, so `LocalPosition.Y` is 132 and `LocalSize.Y` is 48. The band occupies
132 to 180 inside a panel 180 tall, which is inside the clip rectangle, and measures 320 by 48.
The capture in the report is the second, independent check of the same thing.
