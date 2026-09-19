# Chunk 21: the control dashboard example document (PLAN 4.10)

**PLAN 4.10 is complete.** `examples/control-dashboard/` is a 1280x720 document with 12 active
instruments and 4 indicators, it loads under both profiles, and every one of the 16 moves under
measurement.

**Built by Codex** from the frozen spec; every proof run here. It found a bug in shipped code,
which is the point of writing a document meant to be looked at rather than measured.

## The census

```
bound analog_dial=2   minimum=2
bound bar_gauge=3     minimum=3
bound history_graph=1 minimum=1
bound readout=6       minimum=4
bound indicator=4     minimum=4
value instruments=12; indicators=4
```

51 components in total, 16 of them bound. The other 35 are a container, 13 shapes, 19 unbound
readouts used as labels and units, and 2 images. Chrome is not counted as an instrument, which is
why the census reports bound components by type rather than a component total.

Ten signals, named for what a control dashboard shows: `vehicle_speed`, `engine_speed`,
`coolant_temperature`, `oil_pressure`, `fuel_level`, `engine_load`, `ignition_on`, `abs_ready`,
`traction_ready`, `cruise_enabled`.

## The gate

```
criterion 3: existing validator corpus path
valid=33 invalid=65 mismatches=0
desktop-early (desktop): DashVerdict accepted=true ... state=valid
mobile (mobile):         DashVerdict accepted=true ... state=valid
capture: 1280x720, non-flat RGB

criterion 6: every value instrument changes inside its own rect
  speed_dial               changed_pixels=1349  worst=161
  engine_dial              changed_pixels=1474  worst=214
  coolant_temperature_bar  changed_pixels=5760  worst=129
  oil_pressure_bar         changed_pixels=8064  worst=187
  fuel_level_bar           changed_pixels=5760  worst=127
  engine_history           changed_pixels=4131  worst=214
  value_1                  changed_pixels=1019  worst=213
  value_2                  changed_pixels=993   worst=213
  value_3                  changed_pixels=1436  worst=213
  value_4                  changed_pixels=990   worst=213
  value_5                  changed_pixels=536   worst=213
  value_6                  changed_pixels=512   worst=213

criterion 6b: every indicator changes with unavailable quality
  ignition_on_lamp / abs_ready_lamp / traction_ready_lamp / cruise_enabled_lamp
  changed_pixels=960 worst=71 each

Example gate: 0 failures
```

## The bug this document found

**A bound readout could never show a live value, in any document, since chunk 11.**

`FMissingDataPresenter::Apply` restores the text captured at build time
(`PrimitiveSupport.cpp:142-145`), and the readout's updater set the live value and *then* called
`Apply`. Every frame wrote the reading and immediately overwrote it with the document's static
`text` property. The first gate run showed exactly that: 216 glyph pixels in the readout rect,
byte-identical between two captures taken seconds apart, while the dials and bars on the same
signals moved.

It survived ten chunks because nothing had ever measured a bound readout in motion. The chunk 14
live gate watches a needle and an arc; every fixture before this one is flat colour blocks chosen
to be findable by hue, and none of them bound a readout to a moving signal.

**The fix:** `Apply` takes the live reading, so a state presentation is applied *on top of* the
current value rather than instead of it. The ordering matters and the other two orderings are
both wrong:

- value then `Apply`, the old code: the state wins and the value never shows.
- `Apply` then value: the value wins and a `dash` presentation is undone, which defeats the point
  of `dash`, whose whole job is to replace the reading with `--`.

Passing the reading into `Apply` lets `dash` still replace it, `last_value_dimmed` dim it, and
`icon` keep it and add a badge, which is what each of those presentations means.

## Files

| path | task |
| --- | --- |
| `tools/gen-control-dashboard.py` | 4.10, writes all three forms |
| `examples/control-dashboard/{dashboard,signals,manifest}.json`, `assets/dial-face.png` | 4.10, the source form 4.11 will edit by hand |
| `tests/fixtures/packages/control-dashboard.udash` | 4.10, what the player loads |
| `tests/fixtures/documents/valid/control-dashboard.json` | 4.10, the corpus bundle, the only tree the corpus runners scan |
| `scripts/run-example-gate.ps1`, `scripts/check-example.py` | 4.10, criteria 1 to 6b |
| `runtime/UnRealDash/Source/UnRealDashCore/.../Primitives/{Primitives.h,PrimitiveSupport.cpp,ReadoutPrimitive.cpp}` | 4.10, the readout fix above |

The generator writes three artefacts because they serve three consumers: the source form for a
human and for 4.11, the `.udash` for the player, and the corpus bundle for the validator. The
review caught that revision 1 specified only the first two, which would have left the document
outside the only tree the corpus runners read.

## Four blockers the review caught before any of it was built

DeepSeek returned BLOCKED on revision 1. All four accepted:

1. **Required properties were unstated**, so a generator following the spec would have emitted
   invalid documents. `analog_dial` needs `colour`, `bar_gauge` needs `circular` and `colour`,
   `history_graph` needs `history_samples` and `colour`, `readout` needs `text`, `indicator`
   needs `colour`.
2. **`history_graph` cannot have `minimum` or `maximum`.** `additionalProperties` is false, so my
   "every gauge declares a minimum and maximum" criterion would have mandated a document the
   schema rejects.
3. **Indicators cannot be made to move by a value sweep.** They light on non-zero and take the
   0 to 1 default range, so they are off only at an instantaneous zero crossing and no pair of
   captures can flip four of them. Criterion 6 would have failed on a correct document. Split into
   criterion 6b, which forces `-udash-state=unavailable` and tests what an indicator actually
   does: change with quality rather than with magnitude.
4. **The 12-plus-4 count was ambiguous**, and the loose reading would have passed a document of 12
   components including the indicators. Resolved to the strict reading: 12 active instruments not
   counting indicators, plus at least 4 indicators.

## Verification

```
ten gates                        0 failures each
signal-core     179 cases / 117,162 assertions
dashboard-spec   30 cases / 1,002,203 assertions
corpus          valid=33 invalid=65 mismatches=0
Pester          76 passed, 0 failed
doctor, UBT Win64, UBT Android   all clean
```

## Not done, and why

- **The document is functional, not designed.** It is a plausible instrument layout with real
  signals and units, and nobody has made it look good. 5.x owns the showcase dashboard and the
  creative direction; this is the control dashboard 4.11 edits and 6.5 measures.
- **The six readouts are named `value_1` to `value_6`** as component ids, though they bind
  properly named signals. Harmless, and worth renaming when 4.11 starts editing this file by hand.
- **Edit-without-rebuild is untested.** That is 4.11, whose Windows half can now run and whose
  Android half waits for the device visit.
