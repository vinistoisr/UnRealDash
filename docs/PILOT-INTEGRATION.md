# Custom vehicle test rig

This installation is a convenient development and field-test fixture. It is not the standard user setup and must not dictate the public onboarding, generic OBD-II implementation, or aftermarket ECU interfaces. Generic OBD-II is the first broadly useful connectivity target; direct USB replacement in this custom installation is optional.

## Identified setup

Read-only inspection of the owner's existing Scirocco project established the following documented setup. No connection to the car, firmware deployment, or live compatibility test was performed.

| Component | Documented configuration |
| --- | --- |
| Vehicle | 2009 VW Scirocco 2.0 TSI, CAWB / EA888 Gen 1 |
| ECU | Bosch MED17.5; vehicle-specific measuring-block mapping |
| Acquisition board | Adafruit Feather M4 CAN Express running CircuitPython |
| Vehicle protocol | VW TP 2.0 carrying KWP2000 diagnostic measuring-block requests |
| Board output | USB CDC data interface carrying fixed binary telemetry frames |
| Head unit | DUDU7, UIS7870, Android 13, unrooted |
| Display | Project documents a 1280×720 panel with a 1280×660 dashboard area when navigation chrome is present; verify actual application viewport |
| Existing software path | Android USB bridge → local Telnet endpoint 2323 → Python telemetry relay/logger → local raw TCP endpoint 35000 |
| Acquisition rate | Documentation reports approximately 14–18 Hz for the main loop; this is not the rate of every channel or a new measurement |

## First integration boundary

```text
ECU → CAN / TP 2.0 / KWP2000 → Feather
                                      ↓ USB CDC
                              Existing bridge and relay
                                      ↓ local TCP :35000
                              UnRealDash compatibility decoder
                                      ↓ normalized signals
                              Unreal dashboard player
```

Connect the first player to the relay's output, preserving the acquisition firmware, logging, and existing reconnect behavior. The relay already separates the display client from USB ownership. Verify client coexistence and backpressure in a bench test before trying two display applications concurrently.

The follow-on direct-USB transport should use Android USB host permissions and select the data interface explicitly. The board's boot configuration normally disables its console CDC interface, but can expose it in a recovery mode. Never assume that the first serial interface is always telemetry.

Direct USB ownership would conflict with the existing bridge's claim. Replacing that path requires a deliberate handover and a plan to preserve logging; it is not necessary for the first rendering pilot. Keep the user's unrooted-device constraint.

## Frame and mapping requirements

The inspected encoder emits fixed 16-byte records: a four-byte synchronization marker, a little-endian 32-bit record identifier, and eight payload bytes. Payload fields have per-signal scaling and signedness. These records carry application telemetry, not an unmodified capture of the vehicle CAN bus.

Implement a neutral `binary-telemetry-v1` connector profile with a versioned mapping derived from the user's schema after a separate reuse/license review. Test split records, coalesced records, resynchronization, negative temperatures, unsigned fields in otherwise signed records, and unit conversions. Do not treat arbitrary external XML conversion expressions as executable code.

The upstream bridge speaks Telnet and escapes 0xFF. The existing relay handles that before exposing its raw downstream stream. A plugin connecting to port 2323 would need Telnet framing; a client of port 35000 must not decode it twice.

Receive-only is the initial profile. The current ecosystem includes command records such as fault clearing; exclude command emission from the first UnRealDash connector. Preserve the ECU polling in the Feather rather than adding a competing diagnostic session from the dashboard.

## Freshness and correctness

The project records that interleaving ordinary OBD requests with the active TP 2.0 session disrupted acquisition on this ECU. Keep Bluetooth OBD development independent from this pilot unless coexistence is explicitly tested.

The relay can emit synthetic status records to keep clients connected when board data stops. A live TCP connection or heartbeat is therefore not evidence of fresh ECU measurements. Track transport health, acquisition health, and per-signal validity separately. Some values are slow snapshots or held values; receiving their repeated frames does not prove a new measurement. Where the existing feed cannot express age precisely, document that limitation rather than invent timestamps.

Preserve the source project's uncertainty labels for inferred/unverified channels. Boost pressure, rail pressure, temperatures, and other signals need explicit absolute/gauge and unit conventions. Do not convert diagnostic or unsupported sentinel values into valid physical measurements.

## Bench-first acceptance

The existing project contains a simulated board stream and a schema decoder. Use them as local reference tools; no private recordings, credentials, or third-party dashboard assets are copied into this repository.

1. Feed synthetic records to the new parser and compare values with the reference decoder.
2. Exercise disconnect, fragmented packets, idle heartbeat, held data and reconnect scenarios.
3. Render an original showcase dashboard against the stream on the workstation.
4. Package for the deck and verify Vulkan/OpenGL capabilities, frame time, memory, viewport and wake behavior.
5. Connect to the existing local relay and compare values against trusted reference readings.
6. Test direct USB as a separate transport milestone after the relay-based player works.

The inspected project contains older setup notes alongside newer runbooks. Prefer agreement between the current top-level architecture, boot configuration, parser and relay implementation; confirm the installed versions before relying on details in the car.
