# Build chunk 21: the control dashboard example document

Status: revision 2, written against the tree, blocked once and corrected. Covers PLAN.md task 4.10. Read PLAN.md (4.10, and
4.11 which edits this document without a rebuild), `docs/PROJECT-PLAN.md` section 8, and the
chunk 11 and 12 specs for what each primitive can actually do.

This is the first document in the project written to be looked at rather than measured. Every
fixture so far is flat colour blocks chosen so a gate can find them by hue. This one has to be a
plausible control dashboard and still pass everything.

## Facts this chunk depends on

- **Nine component types exist** and all nine are built: `container`, `readout`, `image`, `shape`,
  `indicator`, `page_switch`, `analog_dial`, `bar_gauge`, `history_graph`. Chunks 11 and 12 built
  them and the dial gate measures the last three.
- **PROJECT-PLAN section 8 defines the standard dashboard** as "60 Hz at 1280x720, 12 active
  instruments plus indicators" (`docs/PROJECT-PLAN.md:143`). 4.10's gate restates it as at least
  12 instruments plus 4 indicators.
- **`examples/` exists and holds only a README** saying control and showcase dashboard data belong
  there and compiled packages do not. So `examples/control-dashboard/` is the source form:
  `dashboard.json`, `signals.json`, `manifest.json` and an `assets/` directory.
- **`z_order` decides stacking, not declaration order.** The parser sorts every object's members
  by name (chunk 12's finding), so anything overlapping needs an explicit `z_order`.
- **Ranges come from the gauges.** `FDashBindingTable::SignalRanges` unions the declared minimum
  and maximum of every gauge bound to a signal, and the sweep source uses that. A gauge with no
  declared range sweeps 0 to 1 and looks broken.
- **The semantic pass rejects a binding to an undeclared signal** with `E_UNRESOLVED_SIGNAL`, so
  every bound signal must appear in `signals.json`.

## Deliverables

### 1. `examples/control-dashboard/`

A document at a 1280x720 reference viewport with **at least 12 active instruments and at least 4
indicators**, in the source form the package builder consumes.

"Active" means bound to a signal and changing when that signal changes. A decorative shape is not
an instrument. The report counts them by type and says which are which, so the gate is not
counting components and calling them instruments.

The instrument mix covers every primitive that can show a value, because an example document that
exercises two of the nine would not be an example of anything:

| primitive | at least |
| --- | --- |
| `analog_dial` | 2 |
| `bar_gauge` | 3 |
| `history_graph` | 1 |
| `readout` | 4 |
| `indicator` | 4 |

The remainder makes up the twelve. `container`, `shape` and `image` are used for layout and
chrome and are not counted as instruments.

### 2. Signals that mean something

Named for what a control dashboard actually shows, with real units and freshness deadlines, not
`signal_0`. Every bound signal is declared, every gauge declares a minimum and maximum that suits
its signal, and the ranges are what the sweep source will drive.

### 3. A generator, not a hand-written JSON blob

`tools/gen-control-dashboard.py`, for the same reason every fixture has one: a 1280x720 layout
with 20 or so components has arithmetic in it, and arithmetic belongs in code rather than in
copied coordinates. It writes the source form into `examples/control-dashboard/` and builds the
`.udash` the gate loads.

**The document is the product here, not the generator.** 4.11 edits `dashboard.json` by hand and
relaunches, so the generated file has to be readable and editable by a person.

### 4. The gate

`scripts/run-example-gate.ps1`:

- the document validates through the existing validator corpus path
- the packaged player loads it and reports `accepted=true`
- the component census meets the minimums above, counted from the document
- a capture at 1280x720 is not blank and not a single flat colour, which is the cheapest honest
  check that something was actually laid out
- a live run with the sweep source moves at least one pixel in each instrument's own region

## Constraints

- The document is authored, not measured. No colour in it is chosen to be findable by a gate,
  except where an instrument's own region has to be located, and the report says where that
  applies.
- It must load under both profiles. The mobile profile has a smaller texture budget, so any image
  asset has to fit it.
- No em dashes; no PowerShell 7 only syntax in scripts.

## Non-goals

- The showcase dashboard, which is 5.x.
- Edit-without-rebuild, which is 4.11 and needs this document to exist first.
- Theming beyond what chunk 11's tokens already support.

## Pass/fail criteria

1. **At least 12 active instruments and at least 4 indicators**, counted from the document by
   type, with the census printed.
2. **Every primitive in the table above appears at least the stated number of times.**
3. **The document validates** and the packaged player reports `accepted=true` under both the
   desktop and mobile profiles.
4. **A 1280x720 capture is neither blank nor a single flat colour**, measured rather than eyeballed.
5. **Every bound signal is declared**, and every gauge declares a minimum and maximum, checked
   from the document rather than by loading it.
6. **A live run moves each instrument**: with the sweep source running, two captures taken at
   different scenario points differ inside every instrument's declared rect.
7. All nine existing gates still pass, `doctor.ps1 -Profile workstation` exits 0, Pester passes
   with `-CI`, and both CMake suites are green with no drop.

## Report format

Files added with their PLAN task; the component census; the proof output verbatim; a note on what
the document looks like and what it does not yet do; what was not run and why.

---

## Revision 2: four blockers, all accepted

### A. The required properties were not stated, and one of them cannot exist

The spec named `minimum` and `maximum` and nothing else, so a generator following it would emit
documents the schema rejects. The schema requires, beyond position and binding:

| type | also required |
| --- | --- |
| `analog_dial` | `colour`, `minimum`, `maximum` (`dashboard.schema.json:383-387`) |
| `bar_gauge` | `circular`, `colour`, `minimum`, `maximum` (`:435-440`) |
| `history_graph` | `history_samples`, `colour` (`:524-527`) |
| `readout` | `text` (`:262-264`) |
| `indicator` | `colour` (`:479-481`) |

**`history_graph` has no `minimum` or `maximum`**, and `additionalProperties` is false, so adding
them is a validation failure rather than a harmless extra. Criterion 5's "every gauge declares a
minimum and maximum" is therefore rewritten to name the two types that have them, `analog_dial`
and `bar_gauge`, and to assert that `history_graph` does **not**.

### B. An indicator cannot be made to move by a sweep, so criterion 6 was unachievable

`IndicatorPrimitive.cpp:45-48` lights an indicator when the reading is valid and non-zero. An
indicator declares no range, so it takes the 0 to 1 default, and the sweep crosses zero only
instantaneously. Four indicators on four signals have four different crossing times, so no pair
of captures can flip all of them. Criterion 6 would have failed on a correct document.

**Criterion 6 now covers the value-showing instruments only**: `analog_dial`, `bar_gauge`,
`history_graph` and `readout`. **Indicators get criterion 6b instead**, which uses machinery that
already exists: a capture with `-udash-state=unavailable` differs inside every indicator's rect
from the same capture taken valid. That tests the thing an indicator actually does, which is
change with quality rather than with magnitude.

### C. The corpus path needs a file this chunk never said it would write

"Validates through the existing validator corpus path" means a bundle under
`tests/fixtures/documents/valid/`, because that is the only tree both corpus runners scan
(`test_corpus.cpp:6-8`, `tools/dashboard_spec/report.py:28`). Deliverable 3 wrote only
`examples/control-dashboard/` and a `.udash`. **The generator also writes the corpus bundle**, the
way every fixture generator already does.

The required fields, stated here so the generator is not guessing: the manifest needs
`schema_version: 1`, `package_id`, `revision` of at least 1, `runtime_compatibility` and
`assets`, each asset carrying `width`, `height`, `format` and `bytes`; every signal needs `id`,
`type`, `unit`, `freshness_deadline_ms` and `discrete`.

### D. The count was ambiguous and the loose reading would have passed a document PLAN rejects

PLAN says "12 active instruments plus indicators" and gates "at least 12 instruments plus 4
indicators". Revision 1 said "at least 12 active instruments and at least 4 indicators", which a
document of 12 components including the 4 indicators also satisfies.

**The strict reading is the one that holds: at least 12 active instruments NOT counting
indicators, plus at least 4 indicators**, so at least 16 bound components. The census prints both
numbers separately so the distinction cannot blur again.
