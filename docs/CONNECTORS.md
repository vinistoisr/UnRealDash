# Connector architecture and coverage plan

## Direction

Generic OBD-II, initially through common Bluetooth adapters, is the first broadly useful connectivity target. Aftermarket ECU integration is a major product track, with MegaSquirt, AEM and MoTeC explicitly prioritized for research and profiles. The existing Scirocco/Feather/DUDU7 setup is a [custom test rig](PILOT-INTEGRATION.md), not the standard architecture or a setup requirement for users. Long-term worldwide coverage should grow through documented, independently maintained plugins.

CAN, USB, Bluetooth, OBD-II, and an ECU's broadcast format occupy different layers. A Bluetooth OBD adapter commonly provides a diagnostic command interface, not a raw CAN stream. A USB cable does not establish whether the device exposes serial bytes, vendor endpoints, or an OBD interface. Keep these differences visible in the implementation while offering a guided setup to users.

## Four reusable layers

| Layer | Examples | Responsibility |
| --- | --- | --- |
| Transport | Android USB host/serial, Bluetooth Classic RFCOMM, BLE GATT, TCP | Discovery, permission, connection, bytes/messages, reconnect |
| Adapter/session protocol | ELM-compatible AT commands, vendor USB-to-CAN framing, network gateway framing | Initialization, framing, request scheduling, responses and device errors |
| ECU/data decoder | Standard OBD PIDs, DBC mapping, MegaSquirt CAN broadcasts, documented proprietary serial protocol | Decode fields into typed values using the correct firmware/protocol variant |
| Signal mapping | VSS-aligned names and vendor/custom namespaces | Unit normalization, source selection, timestamps, quality, dashboard bindings |

Examples:

```text
Android USB → adapter framing → documented ECU CAN decoder → signals → dashboard
Bluetooth Classic → ELM-compatible session → supported OBD PIDs → signals → dashboard
BLE vendor GATT profile → ELM-compatible session → supported OBD PIDs → signals → dashboard
SocketCAN → documented ECU CAN decoder → signals → dashboard
```

This allows a decoder to work across several adapters and an adapter to serve several ECUs. BLE service/characteristic UUIDs and packet behavior are device-specific; do not assume every Bluetooth product implements the same transport.

## Plugin contract

Ease of independent authoring is a product requirement, not just an internal interface. See [Plugin authoring experience](PLUGIN-EXPERIENCE.md) for the proposed definition format, small custom-code API, documentation set and contributor usability tests. Common protocol additions must install without Unreal tooling or a player rebuild. Evaluate a portable sandboxed module host for protocols that exceed declarative definitions; native OS transport extensions remain a separate advanced path.

Each manifest declares plugin ID/version, layer, platforms, supported devices and firmware variants, configuration schema, required permissions, input/output types, capabilities, documentation source, and compatibility evidence. Device matching may use USB identifiers or Bluetooth services as hints; it must not select an ECU calibration solely from transport identity.

Lifecycle: discover, connect, identify, configure, enumerate signals, start, stop, reconnect, report health. Separate control-plane operations from streaming data. Acquisition runs away from Unreal's render/game thread with bounded queues and explicit backpressure/drop policy. Tag data with receive times and source identity before notifying consumers.

Configuration-only decoder packs should cover fixed CAN layouts, scaling, enumeration maps, and declarative polling profiles. Native code is reserved for new transports, complex session protocols, or genuinely new behavior. Native Android additions ship in an application build; an arbitrary shared library downloaded by prompt is not the default extension mechanism.

Publish the interfaces and conformance fixtures early. Avoid making every vendor plugin duplicate a USB driver, Bluetooth stack, or dashboard widget.

## Initial coverage tracks

| Track | Initial deliverable | What remains unknown |
| --- | --- | --- |
| Simulator/replay | Deterministic signal and raw-frame fixtures, disconnect/error scenarios | No vehicle hardware dependency |
| Custom Android test rig (parallel) | Consume the existing TCP relay for rendering/reliability tests; direct USB optional | Confirm installed revisions and freshness semantics; does not validate generic OBD-II compatibility |
| Bluetooth OBD-II | Classic transport plus one verified BLE profile; ELM-compatible session; capability discovery and adaptive polling | Reference adapters and vehicle-supported PIDs |
| MegaSquirt | Versioned 11-bit CAN broadcast decoder with published-message test vectors | Exact ECU/firmware and broadcast configuration for physical verification |
| AEM | Research Infinity and Series 2/EMS-4 separately; build profiles for documented CAN output | Exact model/firmware, enabled stream, message layout, adapter and hardware validation |
| MoTeC | Research M1 package-dependent CAN output and older ECU families separately | ECU model, package/firmware, selected transmit template, mapping rights and physical validation |
| Further aftermarket ECUs | Prioritized protocol profiles and contributor templates | Vendor documentation, redistribution terms, hardware access and demand |

MegaSquirt's manual index distinguishes 11-bit CAN broadcasts, its proprietary 29-bit CAN protocol, and serial communication. Treat these as separate protocol capabilities, not one interchangeable connection. Start with documented broadcast decoding; serial support and older firmware need explicit profiles. [Manufacturer manuals](https://www.msextra.com/manuals/), [broadcast specification](https://www.msextra.com/doc/pdf/Megasquirt_CAN_Broadcast.pdf)

An Android USB-serial library is a candidate for supported serial-class bridges, not a universal USB-to-CAN driver. Inspect the actual adapter before selecting it. [usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android)

AEM's engine-management support portal provides model-specific documentation; MoTeC's GPR documentation describes ECU CAN output using MoTeC templates. These are research entry points, not implemented support. Never equate a tuning connection over USB/Ethernet with the dashboard's telemetry transport. [AEM support](https://www.aemelectronics.com/support/engine_management/), [MoTeC GPR](https://www.motec.com.au/products/GPR)

Candidate further vendor families include Haltech, Link, ECUMaster, MaxxECU, and FuelTech, plus community ECU projects. This is a research backlog only: no compatibility, protocol access, or licensing conclusion has been established for these families.

## OBD behavior and user experience

Probe documented adapter capabilities, discover supported data items, handle timeouts and partial replies, and schedule high-priority signals more frequently than slow values. Report achieved rates per signal. Display smoothing cannot compensate for unavailable or very slow telemetry; show this clearly in Studio's preview and diagnostics.

Separate standard PIDs from manufacturer-specific identifiers and adapter extensions. Never assume all vehicles expose oil pressure, gear, or individual wheel data through standard OBD. Telemetry requests may transmit onto the vehicle network, but actuator commands, calibration writes, fault clearing, and ECU programming are outside initial scope.

Connection setup should explain: device connected; protocol identified; signals available; mapping unresolved; signal stale. Avoid a single green indicator that hides missing data. AI can recommend a profile from observed identifiers and supplied documentation; it cannot certify an unknown mapping from plausible-looking numbers.

## Evidence required for a supported plugin

Track status per platform/device/firmware combination: planned, implemented, fixture-tested, hardware-verified, community-reported, or deprecated. Record the sample provenance and limits of each result.

Conformance tests cover fragmented/coalesced packets, invalid lengths, timeouts, reconnect, USB detach, permission denial, byte order, signed values, scaling, multiplexing, stale data, and conflicting signal sources. Android testing also covers sleep/resume, app lifecycle, and simultaneous power/data where applicable. Use synthetic or permitted captures in the repository.

Acceptance for the owner's first path requires comparison against known readings or trusted logs, measured update rates, loss/recovery behavior, and sustained rendering under real acquisition load. Decoding a published example is necessary evidence, but does not establish hardware support.
