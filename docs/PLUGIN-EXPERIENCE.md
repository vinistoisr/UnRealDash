# Protocol plugin authoring experience

Status: proposed contract, not an implemented SDK. Community members must be able to add a protocol using short documentation and examples, without learning Unreal or asking a maintainer to ship it.

## Core requirement

An ordinary protocol plugin must not require Unreal Editor, an engine build, a fork of the player, a publisher account, or approval from a central registry. Users can install local plugin packages and share them directly. The bundled registry is a convenience, not an authority over what owners can run.

Adding an ECU's message definitions is different from adding an operating-system driver. Make the common case simple while documenting the boundary honestly.

## Three extension levels

| Level | Contributor writes | Installation |
| --- | --- | --- |
| Definition pack | JSON/YAML definitions or an imported DBC: frame IDs, fields, units, scaling, enums, polling and identification profiles | Import a package; no application rebuild |
| Portable protocol module | Small sandboxed module for custom framing, parsing or session state that definitions cannot express | Import a compatible module package; no application rebuild once the module host is shipped |
| Native transport integration | Platform code for a new USB driver, Bluetooth capability or OS API unavailable through the host | Platform build/update; document this as advanced integration |

Existing transports should cover USB serial, Bluetooth Classic, supported BLE profiles and network streams. Protocol code consumes host-provided bytes or CAN frames; it does not implement Android permissions, reconnection UX, or Unreal widgets.

Definition packs are the first implementation target. For portable custom modules, evaluate WebAssembly as the preferred candidate rather than choosing an untested runtime now. A spike must verify Android ARM64, Windows x64 and Linux x86-64 embedding, interpreter/AOT requirements, licensing, bounded memory/execution, and artifact portability. Do not claim arbitrary Python, JavaScript or native DLL plugins work in the player.

## Contributor journey

1. Copy a minimal example or run a scaffold command.
2. Declare the transport and protocol capabilities the plugin needs.
3. Describe fields or implement the small decode/session interface.
4. Supply a few synthetic input records and their expected signals.
5. Run the validator and replay harness locally without Unreal or vehicle hardware.
6. Import the package into Studio, inspect values and units, and connect a dashboard.
7. Export/share the package or submit it to an optional registry.

AI can create and edit the same ordinary files. Plugin creation must also work by hand using the documentation alone.

## Example: a declarative CAN decoder

Illustrative proposed syntax; the SDK does not yet implement this format. This synthetic frame is not an actual ECU protocol.

```yaml
id: community.example.engine
version: 0.1.0
api: 1
input: can.classic
frames:
  - id: 0x600
    id_format: standard
    length: 8
    signals:
      - name: engine.rpm
        byte_offset: 0
        type: uint16
        byte_order: little
        scale: 1
        unit: rpm
      - name: engine.coolant_temperature
        byte_offset: 2
        type: uint8
        offset: -40
        unit: degC
```

Example fixture: frame 0x600 with bytes `B8 0B 82 00 00 00 00 00` must decode to 3000 rpm and 90 °C. The host handles mapping to dashboard signal names, source identity and receive time. Protocol-specific validity indicators and sample age belong in the definition when available.

Provide bit-field and multiplexing support through a documented canonical representation and DBC import. Do not expose ambiguous start-bit conventions without explicit examples. Reject overlapping/invalid fields where inappropriate, unsupported expressions and impossible lengths with precise errors.

## Small custom-code API

Keep the public API independent of the eventual module language:

- `describe`: identity, versions, configuration schema and output signal descriptions.
- `start(config)`: initialize bounded session state.
- `on_bytes(chunk)` or `on_can_frame(frame)`: process host-delivered input; byte chunks need not align with messages.
- `on_timer(token)` and `on_disconnect(reason)`: explicit lifecycle callbacks.
- Host services: publish typed samples/quality, schedule timers, log diagnostics, and submit permitted protocol requests through the selected transport.

The host owns buffers, monotonic clocks, transport access, quotas and restart behavior. Modules do not receive ambient filesystem access, arbitrary sockets, credentials or rendering access. A malformed plugin should fail its connector instance without crashing the player.

Protocol-request transmission is an explicit capability because OBD polling requires it. A generic raw transmit capability cannot by itself prove an operation is read-only: built-in polling profiles can constrain known requests, while custom transmitting modules need clear owner-visible permissions and test evidence.

## Minimum documentation set

- A short “first plugin” guide with a complete working example.
- Definition reference, types/units, bit order and multiplexing examples.
- Transport/session guide, including OBD polling and BLE profiles.
- Custom module quickstart and precise host API reference after the runtime spike.
- Testing guide with replay, malformed-packet and disconnect fixtures.
- Packaging/install guide, version compatibility and troubleshooting.

Example templates should cover a CAN broadcast ECU, an ELM-compatible polling profile, and a custom serial stream. Build the contributor harness independently of Unreal so ordinary CI and developer laptops can validate plugins cheaply.

## Acceptance gates

- A contributor unfamiliar with the codebase can add the synthetic CAN example and verify it within roughly 30 minutes using only the quickstart. This is a usability test target, not an established result.
- Definition packs install offline without rebuilding the player.
- One decoder works unchanged through two compatible transports in the harness.
- Validation identifies the exact file/field responsible for a bad package.
- A failed plugin can be disabled/rolled back without losing the dashboard.
- Versioned API capabilities prevent a newer plugin silently running against an incompatible host.
- A community contributor can share and install a plugin without a central account or maintainer approval.
- The portable-code runtime is accepted only after the same conformance fixtures pass on all three target platforms and resource limits are demonstrated.
