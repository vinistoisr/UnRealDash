# Build chunk 05: telemetry parser and offline converter

Status: frozen spec for a Codex build session. Covers PLAN.md tasks 2.9, 2.10, 2.11, 2.12 and 3.7. Read PLAN.md (those tasks, the WP2 preamble, the Approach preamble, the pin table, task 4.9 for what the connector will later need, the Test strategy section, Key decisions 6, 7, 11 and 13, risks R-E, R-H and R-I, assumptions A5, A6, A7 and A12), docs/ARCHITECTURE.md, docs/PILOT-INTEGRATION.md and docs/PLUGIN-EXPERIENCE.md before writing anything. PLAN.md is the authority on what; this file adds the exact file layout, the interface, the test names, the conversion rules and the proof commands. If the two disagree, PLAN.md wins and the disagreement goes in the report.

Chunks 01, 02 and 03 have all landed in this tree: the scaffold, `scripts/`, the vendored `third_party/` libraries, `runtime/UnRealDash/Source/SignalCore/` with `packages/signal-core/`, and `packages/dashboard-spec/` with `tools/` and the fixture corpus. Do not recreate any of them.

## Goal

A receive-only parser for the existing 16-byte frame format that produces the same frames from any chopping of the same bytes, publishes held values and sentinels honestly, never treats a relay heartbeat as a measurement, survives ten million malformed inputs without a crash or an allocation, and agrees field by field with the owner's reference decoder over every enumerated frame identifier. Plus the offline converter that turns the owner's schema XML into a committed definition pack, so the player reads a pack and never reads XML.

## Dependency direction, and where JSON lives

signal-core includes nothing above it and vendors no JSON library (docs/ARCHITECTURE.md, layering; chunk 02's JSON decision). That constrains this chunk in one specific way and it is settled here rather than left to the session:

- **The definition-pack loader in signal-core takes the pack as plain structs.** `DefinitionPack.h` declares the structs and a `ValidatePack` function. No JSON text reaches signal-core.
- **JSON to struct conversion lives in dashboard-spec**, as `DefinitionPackBuilder`, beside chunk 03's `RuleTreeBuilder`, which already owns the same job for rule trees. dashboard-spec links signal-core and owns the vendored RapidJSON, so the dependency runs downward.
- **The parser tests in `packages/signal-core/tests/` build their packs from C++ literals, not from JSON.** The fixture pack file is loaded by a dashboard-spec test instead.

That last point is a choice between two arrangements PLAN.md permits, and this is the reason. A test-only helper in signal-core's test target reaching dashboard-spec's builder would make `packages/signal-core` unbuildable without `packages/dashboard-spec`, inverting the dependency direction for the one build whose job is to prove independence, and would break chunk 04's `signal-core.yml`, which configures and tests `packages/signal-core` on its own. Moving the parser tests wholesale into dashboard-spec is rejected because docs/ARCHITECTURE.md puts unit tests next to the code they test. So: parser behaviour is tested in signal-core against hand-built structs, and anything that reads JSON, meaning the fixture-pack load test and the differential-check executable, lives in `packages/dashboard-spec/`.

Drift between the JSON fixture pack and the C++ literals is controlled by one file, `packages/signal-core/tests/support/TestPackData.h`, holding the literal table. The dashboard-spec test builds the same pack from the JSON fixture and asserts the result equals that table field by field, including the header by relative path. A test-support header crossing from dashboard-spec down to signal-core is the allowed direction; the reverse would not be.

## Deliverables

### 1. File layout

New files in the runtime tree, the single copy of the sources:

```
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/BinaryTelemetryV1.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/DefinitionPack.h
runtime/UnRealDash/Source/SignalCore/Private/BinaryTelemetryV1.cpp
runtime/UnRealDash/Source/SignalCore/Private/DefinitionPack.cpp
```

Both `.cpp` files go into the explicit source list in `packages/signal-core/CMakeLists.txt`. Chunk 02's drift check fails the configure step if they do not; do not weaken or bypass it.

New files in `packages/signal-core/`:

```
packages/signal-core/tests/test_binary_telemetry_v1.cpp
packages/signal-core/tests/test_definition_pack.cpp
packages/signal-core/tests/test_heartbeat_and_held.cpp
packages/signal-core/tests/support/TestPackData.h
packages/signal-core/tools/telemetry-fuzz.cpp
```

Add one `add_test` per new test source using `--source-file=<name>`, as chunk 02 did, and an executable target `signal-core-telemetry-fuzz` from `tools/telemetry-fuzz.cpp` linking `signal_core`.

New files in `packages/dashboard-spec/`:

```
packages/dashboard-spec/include/dashboard_spec/DefinitionPackBuilder.h
packages/dashboard-spec/src/DefinitionPackBuilder.cpp
packages/dashboard-spec/tests/test_definition_pack_builder.cpp
packages/dashboard-spec/tools/telemetry-decode-jsonl.cpp
```

`DefinitionPackBuilder.cpp` joins the `dashboard_spec` library sources; `telemetry-decode-jsonl.cpp` becomes an executable target of that name linking `dashboard_spec`. `packages/dashboard-spec/tools/` is a new directory and an addition to chunk 03's layout; report it.

New fixtures, tools and reports:

```
tests/fixtures/packs/binary-telemetry-v1.test.json
tests/fixtures/telemetry-fuzz/*.hex
tools/convert-existing-schema.py
tools/differential-check.py
tools/tests/test_convert_existing_schema.py
tools/tests/test_differential_check.py
connectors/binary-telemetry-v1/definition-pack.json
connectors/binary-telemetry-v1/README.md
docs/reports/differential-check.md
```

`tools/tests/test_differential_check.py` and `connectors/binary-telemetry-v1/README.md` are additions to PLAN.md's file list; report them. The fuzz corpus files are **hex text**, not binary: whitespace-separated byte pairs with `#` comment lines, one stream per file, parsed by the harness. Reason: chunk 01's `.gitattributes` sends binaries through LFS, risk R-E makes every committed byte count, and a hex corpus is diffable.

### 2. Interface published by this chunk

These declarations are the seam 4.9's TCP connector and the differential executable both build against. Do not rename them.

```cpp
namespace signal_core {

enum class Acquisition : std::uint8_t { live, held };

struct FieldDefinition {
  const char* name{};                       // caller-owned, stable, the owner's snake_case name
  std::uint32_t signal{};                   // SignalId value this field publishes into
  std::uint8_t byte_offset{};               // origin is payload byte 0
  std::uint8_t width_bytes{};               // from the pack's type, always
  bool is_signed{};                         // effective signedness, override already applied
  bool little_endian{true};
  double scale{1.0};
  double offset{0.0};                       // physical = raw * scale + offset
  Unit unit{};
  Acquisition acquisition{};
  std::span<const std::int64_t> sentinels{};   // raw values, rejected before conversion
};

enum class FrameRole : std::uint8_t { telemetry, status };

struct FrameDefinition {
  std::uint32_t frame_id{};
  FrameRole role{};                         // from the pack's required per-frame "role"
  std::uint8_t payload_length{};            // payload bytes, not the wire frame length
  std::span<const FieldDefinition> fields{};
};

struct DefinitionPack {
  const char* id{};
  const char* version{};
  std::uint32_t api{};
  std::span<const FrameDefinition> frames{};             // ascending by frame_id, no duplicates
};

Status ValidatePack(const DefinitionPack&);

struct FrameEvent {
  std::uint32_t frame_id{};
  std::uint64_t stream_offset{};            // byte offset of the tag in this session's byte stream
  const std::byte* payload{};               // payload_length bytes, never null
  std::uint8_t payload_length{};
};

struct FramerCounters {
  std::uint64_t frames_emitted{};
  std::uint64_t resyncs{};
  std::uint64_t bytes_discarded{};
  std::uint64_t unconfirmed_candidates_discarded{};
};

enum class FramerState : std::uint8_t { synced, resync };

using FrameSink = void (*)(void* context, const FrameEvent&);

class BinaryTelemetryV1Framer {
 public:
  explicit BinaryTelemetryV1Framer(std::span<const std::uint32_t> enumerated_identifiers);
  void Feed(std::span<const std::byte> bytes, FrameSink sink, void* context);
  void EndOfStream();                       // applies the termination rule
  void Reset();                             // returns to the initial state, keeps no bytes
  FramerState State() const;
  const FramerCounters& Counters() const;
};

struct DecoderCounters {
  std::uint64_t samples_published{};
  std::uint64_t unknown_identifier_dropped{};
  std::uint64_t status_records{};
  std::uint64_t sentinel_rejections{};
};

struct TelemetryHealth {
  Time last_bytes{};                 // last time the framer was fed any bytes
  Time last_telemetry_record{};      // last decoded telemetry-role record
  Time last_status_record{};         // last decoded status-role record
  bool TransportConnected(Time now, Time transport_deadline) const;      // now - last_bytes <= deadline
  bool AcquisitionReceiving(Time now, Time acquisition_deadline) const;  // now - last_telemetry_record <= deadline
};

using SampleSink = void (*)(void* context, const Sample&);

class BinaryTelemetryV1Decoder {
 public:
  BinaryTelemetryV1Decoder(const DefinitionPack& pack, const MonotonicClock& clock);
  void SetConnectionGeneration(std::uint32_t generation);
  void OnFrame(const FrameEvent&, SampleSink sink, void* context);
  const DecoderCounters& Counters() const;
};

}  // namespace signal_core
```

Callbacks are a function pointer plus a context pointer, not `std::function`: docs/ARCHITECTURE.md wants plain function objects over lower-layer types across a boundary, and `std::function` allocates. `MonotonicClock` is chunk 02's injected clock from `Clock.h`; if the landed header's name differs, the header wins and the difference goes in the report.

Framer and decoder are separate objects on purpose: docs/ARCHITECTURE.md keeps transport, session, decoder and signal mapping apart, the fuzz harness needs frame boundaries rather than samples, and the differential executable needs both halves separately. A single class would force the harness to reconstruct offsets it cannot know.

**Allocation contract.** The framer holds a fixed `std::array<std::byte, 64>` carry buffer and allocates nothing, ever. It never needs to retain more than 20 bytes of undecided input: a candidate, its 16-byte frame and the 4 lookahead bytes. It scans the caller's span in place and copies only the carry-over. The decoder allocates nothing. 2.11's no-unbounded-allocation gate is therefore an assertion that the counting allocator recorded zero allocations, not that growth stayed bounded.

New `ErrorCode` enumerators are **appended** to chunk 02's `Errors.h`, never renumbered and never reordered: `pack_duplicate_frame_id`, `pack_frames_unsorted`, `pack_field_past_payload_length`, `pack_zero_field_width`, `pack_empty_frame_list` (the pack's `frames` span is empty; a frame whose `fields` span is empty is accepted), `pack_invalid_scale` (a field's `scale` is zero or not finite, or its `offset` is not finite). A pack may contain zero or more status-role frames; nothing checks a status identifier against an external set. That file and `packages/signal-core/CMakeLists.txt` are the only chunk 02 files this chunk may edit.

### 3. Numbers used, and where they come from

| Number | Value | Source |
| --- | --- | --- |
| Wire frame length | 16 bytes | PLAN.md 2.9, A6 |
| Tag bytes | `44 33 22 11` | PLAN.md 2.9, A6 |
| Payload length | 8 bytes, four little-endian u16 words | PLAN.md 2.9, A6 |
| Enumerated identifiers | 21, `0xC80` to `0xC94` inclusive | PLAN.md 2.9, A6 |
| Lookahead confirmation | the byte at candidate+16 starts another tag | PLAN.md 2.9 |
| Chunk-invariance random multi-splits | 10,000 seeded | PLAN.md 2.9, chosen default |
| Split-pair bound | every pair of split positions | PLAN.md 2.9, chosen default |
| Exhaustive partition bound | inputs of at most 16 bytes, 2^15 partitions | PLAN.md 2.9 |
| Fuzz budget in CI | 1,000,000 inputs or 30 seconds, whichever first | PLAN.md 2.11 |
| Fuzz budget offline | 10,000,000 inputs | PLAN.md 2.11 |
| Heartbeat-only window | 30 seconds | PLAN.md 2.10 |
| Unchanged held-frame window | 30 seconds | PLAN.md 2.10 |
| Freshness deadline in tests | 500 ms, sample at 5 ms, expiry at 505 ms | PLAN.md 2.2 and 2.5, the worked case |
| Framer carry buffer | 64 bytes, at most 20 ever used | chosen default for this spec |
| Heartbeat period in tests | 1000 ms | chosen test value, matches the relay's observed period |
| Status-role frame in the converted pack | 0xC82, `role: status`, every field `acquisition: held` | derived in **Heartbeats** below |
| Differential tolerance | `abs(a-b) <= max(1e-9, 1e-9 * max(abs(a), abs(b)))` | chosen default for this spec |
| Definition-pack `api` emitted by the converter | 1 | docs/PLUGIN-EXPERIENCE.md example (`api: 1`); fixed, no flag |
| Transport and acquisition deadlines in tests | 2000 ms each | chosen test values |

PLAN.md 2.12 says "within the declared scaling precision" and gives no number. The tolerance above is chosen because both sides perform one double multiply and one add on the same integer, so agreement should be exact; a tolerance this tight makes any real disagreement visible instead of absorbed.

### 4. Framer semantics, restated as named tests

Two states, whose output is a function of the byte stream alone. Test names below are literal; use them, in `test_binary_telemetry_v1.cpp`.

**SYNCED.** Emit as soon as 16 bytes are buffered, the leading 4 are the tag, and the following little-endian u32 identifier is in the enumerated set. No lookahead. On either mismatch emit nothing, increment `resyncs`, and enter RESYNC.

**RESYNC.** Scan forward for a candidate, meaning the tag followed by an enumerated identifier. Emit it only once the byte at candidate+16 has been observed to begin another tag, buffering across as many reads as that takes; then return to SYNCED at candidate+16. On a failed lookahead, advance one byte from the candidate and continue scanning.

**Initial state is RESYNC**, on first connect and after every `Reset()`. **Termination:** `EndOfStream()` discards an unconfirmed candidate and increments `unconfirmed_candidates_discarded`. A candidate is never emitted on the strength of a short read or a timeout.

Tests:

- `2.9 fresh parser starts in resync`
- `2.9 garbage prefix carrying tag and enumerated identifier is never emitted`
- `2.9 first emitted frame is the first lookahead-confirmed candidate`
- `2.9 reset returns the parser to the initial resync behaviour`
- `2.9 record split across two reads`
- `2.9 two whole records coalesced in one read`
- `2.9 corrupted tag followed by a valid record`
- `2.9 payload containing the tag bytes is not a frame start`
- `2.9 payload carrying tag plus an enumerated identifier is bounded and counted` (a false lock is possible and counted, not prevented; assert the counters, not absence)
- `2.9 identifier absent from the pack enumeration is dropped with a counter` (use `0xC95` and `0xC7F`, one past each end of the span, so the test proves enumeration and not a range test)
- `2.9 unconfirmed candidate at end of stream is discarded and counted`

**Chunk invariance** is its own systematic gate. Build a set of short adversarial streams, each made of a garbage prefix, a valid frame A, a damaged frame B and a valid frame C, with one stream whose payload carries the tag plus an enumerated identifier. For each stream, feed a fresh framer every single split position, every pair of split positions, and 10,000 seeded random multi-split partitions, asserting every partition yields an identical emitted frame sequence, identical stream offsets and identical counters. Full enumeration of all partitions applies only to inputs of at most 16 bytes, where the set is 2^15 or smaller. Tests:

- `2.9 every single split yields the same frames and counters`
- `2.9 every pair of splits yields the same frames and counters`
- `2.9 ten thousand seeded random multi-splits yield the same frames and counters`
- `2.9 exhaustive partitions of a sixteen byte input yield the same frames and counters`

### 5. Decoder semantics, restated as named tests

Definitions come from the loaded pack, never a hardcoded table. Field byte offsets originate at payload byte 0. Width always comes from `type`; where `type` and an explicit per-value signedness disagree, the signedness wins over the frame default and over the type's own sign. Conversion is affine, `physical = raw * scale + offset`. A raw value matching a sentinel is rejected before conversion and publishes `unavailable`, never a converted number. A `live` field publishes `age_evidence = measured`; a `held` field publishes **every** received frame with `age_evidence = unknown` and its receive timestamp updated exactly as a live field's is. An identifier absent from the pack is dropped with `unknown_identifier_dropped` incremented and no exception. Tests in `test_binary_telemetry_v1.cpp`:

- `2.9 offset origin is payload byte zero`
- `2.9 negative temperature via the frame signed default`
- `2.9 unsigned override inside an otherwise signed frame`
- `2.9 non-zero offset decodes to the expected physical value`
- `2.9 scaled field decodes to the expected physical value`
- `2.9 sentinel raw value publishes unavailable and not a converted number`
- `2.9 held field publishes age evidence unknown with a refreshed receive timestamp`
- `2.9 live field in the same frame publishes age evidence measured`
- `2.9 all twenty one enumerated identifiers decode from the fixture pack`

`test_definition_pack.cpp` covers `ValidatePack` against hand-built structs: `2.9 field past the payload length is rejected`, `2.9 duplicate frame id is rejected`, `2.9 unsorted frame list is rejected`, `2.9 zero field width is rejected`, `2.9 empty frame list is rejected`, `2.9 zero or non-finite scale is rejected`, `2.9 non-finite offset is rejected`, and `2.9 a frame with an empty field list is accepted` (the converter can legitimately emit one, see **The converter**).

### 6. Heartbeats and held traffic (task 2.10)

The relay emits a synthetic status record on client accept and once per second while the board is silent, because its downstream client treats a gap of roughly two seconds as a disconnect. That record is a well-formed 16-byte record with the board's own status identifier (0xC82: barometric pressure, fault codes, reconnects, sample rate) and it repeats the last real barometric word, so **content cannot distinguish a heartbeat from a real status record**. Classification is therefore by the frame's declared role, which is explicit and lives in the pack, as PLAN.md 2.10 asks.

How it is expressed: the definition-pack schema (chunk 03) carries a required per-frame `role`, `telemetry` or `status`, and `DefinitionPackBuilder` maps it to `FrameDefinition::role`. A `status` record updates transport health, increments `status_records`, and publishes its fields exactly as their declared `acquisition` says; the converter marks every field of a status-role frame `held`, so those samples publish with the receive timestamp updated normally and `age_evidence = unknown`, and the renderer shows them with the age-unknown marker. A status record never counts toward acquisition health. Acquisition health is "a telemetry-role record was decoded within the acquisition deadline". Both states are computed by the `TelemetryHealth` struct in `BinaryTelemetryV1.h` (see the interface block) from the decoder's record timestamps and the injected clock, with caller-supplied deadlines; the 4.9 connector reuses it rather than inventing its own. Test values, chosen: transport is disconnected after 2000 ms with no bytes, acquisition is not receiving after 2000 ms with no telemetry-role record. Heartbeat-only traffic therefore keeps the transport connected, keeps the status frame's held fields valid with age unknown, and lets every telemetry-role signal go stale at its own deadline. There is no hidden struct member outside the pack: everything the parser needs to classify a frame is in the JSON the owner can read and edit. The differential check compares the status frame's fields as ordinary decoded values, because they are published.

Tests in `test_heartbeat_and_held.cpp`:

- `2.10 thirty seconds of heartbeat only traffic leaves transport connected` (30 status records at 1000 ms against the fake clock; transport state `connected`, `status_records` 30, every published sample from the status frame carries `age_evidence = unknown`, acquisition health `not receiving`)
- `2.10 thirty seconds of heartbeat only traffic marks every telemetry-role signal stale` (every signal mapped from a telemetry-role frame transitions at its own deadline, checked against the injected clock; the status frame's held fields stay `valid` with `age_evidence = unknown`)
- `2.10 thirty seconds of unchanged held frames keeps the signal valid with age evidence unknown` (ordinary well-formed frames whose held fields repeat an identical payload; `valid` throughout, `age_evidence = unknown` on every published sample)
- `2.10 a live field in the same held frame stream stays valid with age evidence measured`
- `2.10 held signals go stale at their own deadlines once the held traffic stops`
- `2.10 acquisition health reports receiving during telemetry traffic and not during heartbeat only traffic`

PLAN.md calls these the two most important correctness tests in WP2. Do not collapse them into one, and do not reintroduce the withdrawn rule that a repeated held payload withholds the receive-timestamp update.

### 7. Malformed-input harness (task 2.11)

`packages/signal-core/tools/telemetry-fuzz.cpp` builds `signal-core-telemetry-fuzz`, with flags `--inputs <n>`, `--seconds <n>`, `--seed <n>` and `--corpus <dir>`, stopping at whichever budget comes first. It seeds from `tests/fixtures/telemetry-fuzz/*.hex`, mutates and generates byte streams, and feeds them through a fresh framer and decoder against the fixture pack. Because it generates the streams it knows the true frame boundaries, so it computes the **false-lock count**: frames emitted whose `stream_offset` is not a true boundary.

It prints, one per line: the input count reached, the elapsed seconds, the seed, every `FramerCounters` and `DecoderCounters` field, the false-lock count, and the allocation count from the counting allocator. It asserts no crash, no hang, and zero allocations. It does **not** assert the false-lock count is zero, per PLAN.md 2.11: a payload can legitimately carry a tag and an enumerated identifier, so the count is bounded, reported, and read into the Stage 0 report.

Corpus: at least eight `.hex` files covering a clean stream, a stream opening mid-frame, a stream whose payload carries the tag, a stream of unknown identifiers, a truncated final frame, a single-byte stream, an empty stream, and a stream of status-identifier records only.

### 8. The fixture pack (task 2.9)

`tests/fixtures/packs/binary-telemetry-v1.test.json` is hand written, validates against chunk 03's `definition-pack.schema.json`, and is **not** the converted schema. It enumerates all 21 identifiers `0xC80` to `0xC94`, each with payload length 8, and covers a signed frame, an unsigned-override field inside it, a scaled field, a field with a non-zero offset, a `held` field, a `live` field in the same frame, and a field with a sentinel list. Its field names are test names and need not match the owner's channel names.

`packages/signal-core/tests/support/TestPackData.h` holds the same pack as C++ literals and is what the signal-core tests build from. `packages/dashboard-spec/tests/test_definition_pack_builder.cpp` loads the JSON through `DefinitionPackBuilder` and asserts the built structs equal that table field by field, so the two representations cannot drift: `3.1 fixture pack validates against the definition pack schema`, `3.1 builder output equals the literal test pack`, `3.1 builder rejects a field with no acquisition`, `3.1 builder rejects a field running past the payload length`, `3.1 builder maps type to width and applies the per-value signedness override`.

### 9. `DefinitionPackBuilder` (dashboard-spec)

`packages/dashboard-spec/include/dashboard_spec/DefinitionPackBuilder.h` and `src/DefinitionPackBuilder.cpp`. It parses pack JSON through the same bounded pre-pass and schema validation chunk 03 already uses, then flattens it into caller-owned storage and hands back a `signal_core::DefinitionPack` whose spans point into that storage, exactly as `RuleTreeBuilder` does for rule trees. Frames come out ascending by `frame_id`, each carrying its `role` from the JSON; a pack with a frame missing `role` fails schema validation before the builder runs. Unit strings resolve through `signal_core::ParseUnit`; an unresolvable unit is an error naming the string and the JSON pointer, never a pass-through. Failures use chunk 03's error-code vocabulary and report a JSON pointer.

### 10. The converter (task 3.7)

`tools/convert-existing-schema.py`, standard library only, reads the owner's schema XML (a single `board/*.xml` file in the owner's separate `scirocco-dash` repository; Claude supplies the absolute path on the Codex command line) and writes a definition pack in chunk 03's schema. It never runs in the player: docs/ARCHITECTURE.md forbids parsing that XML at runtime, and `tools/` never imports from the runtime tree.

```
python tools/convert-existing-schema.py --xml <XML_PATH> \
  --reference-decoder <DECODER_PATH> \
  --out connectors/binary-telemetry-v1/definition-pack.json \
  --report connectors/binary-telemetry-v1/README.md \
  --pack-id vehicle.binary-telemetry-v1 --pack-version 0.1.0
```

Rules, all of them:

1. **Strip comments before parsing.** The file's comments contain `--`, which strict XML forbids. Strip `<!--.*?-->` with `re.S` first, as the reference decoder does.
2. **Iterate `frame` elements without asserting the root element name**, so no product name enters this repository.
3. **Endianness.** Read `endianness` and also the misspelled `endianess` that several frames carry, defaulting to `little`. Any present value other than `little` is a hard failure naming the frame. The reference decoder reads only the correct spelling and falls through to the same default, so the two agree.
4. **Payload length** is 8 for every frame: from `size` when present, otherwise the format's fixed 8-byte payload. The pack's `length` is the payload length; the wire frame is that plus the 8-byte tag and identifier header.
5. **Skip write-direction frames**, meaning any frame carrying `writeInterval`. They describe traffic toward the board, are never decoded, and a pack entry for one would let the player publish a command value as a measurement.
6. **Skip display-only frames and values.** Import the display-only frame set and the `targetId` name table from the reference decoder by absolute path with `importlib.util.spec_from_file_location`; neither is copied into this repository, and a missing attribute is a hard failure naming it. That module imports cleanly because its self-check is guarded. Individual `value` elements carrying `displayOnly="true"` are skipped too, matching the reference decoder, so both sides see the same field set.
7. **Field names.** A `value` with a `name` is snake_cased; one with only a `targetId` takes its stable name from the imported table, and an unknown `targetId` is a hard failure. Where both are present `name` wins, matching the reference decoder. A duplicate name anywhere in the file is a hard failure.
8. **Width and signedness.** `length` defaults to 2; width 1 maps to `uint8`/`int8` and width 2 to `uint16`/`int16`. Effective signedness is the frame's `signed` default, `false` when absent, overridden by a per-value `signed`. Emit `type` and the per-value `signed` consistently.
9. **Conversion.** `conversion` is always of the form `V*<float>`; parse it to a numeric `scale` with `offset` 0 at conversion time. An absent `conversion` is `scale` 1.0. Anything else is a hard failure naming the frame, the offset and the expression. The player never evaluates an expression string.
10. **Units.** Map the XML's unit strings through an explicit table in the converter: chunk 02's `Unit` vocabulary where an equivalent exists, `dimensionless` where the XML declares no unit. A declared unit with no Stage 0 equivalent emits `dimensionless` plus a `unit-unrepresented` report line naming the field and the XML string. No silent conversion, ever. Temperature fields keep their declared unit and scale; kelvin normalization is signal-core's job at the connector edge per 2.3.
11. **Role and acquisition.** Every frame gets `role`: `status` for the board's status frame, identifier 0xC82, which is the record the existing relay re-emits as a heartbeat, and `telemetry` for every other frame; every field of a status-role frame gets `acquisition: held` regardless of the table below, because a repeated status record is not evidence that its values were re-measured. Every other field gets `acquisition` from a table in the converter that the owner confirms, defaulting to `live`. docs/PILOT-INTEGRATION.md records that some channels are slow snapshots or held values but **names none**, so the table ships empty and the report carries a line saying so, per PLAN.md 3.7. The report prints every field with its acquisition value so the owner can mark the table in one pass.
12. **Sentinels.** Emit an empty sentinel list for every field and print a report line naming every `value` carrying an `enum` attribute, so the owner can decide which enum entries are sentinels. An enum label is not a sentinel declaration, and guessing one would turn a diagnostic code into a reading, which R-H forbids.
13. **A frame left with zero fields.** The all-display-only frame is the real case. Check the landed `definition-pack.schema.json`: emit the frame with an empty `fields` array if the schema permits, so the identifier stays enumerated and accepted rather than counted as unknown; if the schema requires at least one field, omit the frame and record it. Report which branch was taken.
14. **The per-frame `timeout` attribute has no representation in the Stage 0 pack schema.** Do not invent a property for it; `additionalProperties: false` would reject one. Record each frame's declared timeout in the report so per-signal freshness deadlines can be authored from it later.
15. Output is byte-deterministic for a given XML and script revision: sorted keys, two-space indentation, LF endings, a trailing newline.

`connectors/binary-telemetry-v1/README.md` records the conversion: source path and date, pack id and version, identifiers emitted and identifiers skipped with the reason for each, the role assignment (which identifier is `status` and why) and the acquisition table with its empty-by-default note, the unit-unrepresented list, the enum-carrying fields awaiting a sentinel decision, and the per-frame timeouts. No product name appears in the pack or in that file. The pack's field names are the owner's snake_case names, which the differential check depends on.

`tools/tests/test_convert_existing_schema.py` runs against small synthetic XML strings written by the test, never against the owner's file: a signed frame, a per-value unsigned override, a `V*0.1` conversion, a missing conversion, an unsupported conversion expression that must raise, a write-direction frame that must be skipped, a `displayOnly` value that must be skipped, both endianness spellings, a non-little endianness that must raise, an unknown `targetId` that must raise, a duplicate name that must raise, and two runs producing byte-identical output.

### 11. Differential check (task 2.12)

`tools/differential-check.py` compares the new parser against the owner's reference decoder over a synthetic record set and writes `docs/reports/differential-check.md`.

```
python tools/differential-check.py --xml <XML_PATH> \
  --reference-decoder <DECODER_PATH> \
  --pack connectors/binary-telemetry-v1/definition-pack.json \
  --decoder-exe packages/dashboard-spec/build/default/telemetry-decode-jsonl \
  --report docs/reports/differential-check.md
```

The reference decoder is imported by absolute path and nothing from the owner's repository is copied into this one, per PLAN.md 2.12 and A7.

**The synthetic set** covers every identifier present in the converted pack (19 of the 21 identifiers in 0xC80 to 0xC94: the write-direction frame and the display-only frame lie inside that span, are not emitted by the converter, and appear in the report's no-comparable-output section; the hand-written fixture pack separately covers all 21 for the parser tests) and, within each frame, every field's boundary values: minimum, maximum, zero, and one least-significant bit either side of any sign boundary, meaning `0x7FFF`/`0x8000` for a 16-bit field and `0x7F`/`0x80` for an 8-bit field. Each record is 16 bytes: tag, little-endian u32 identifier, 8-byte payload. An identifier or field absent from the set fails the gate rather than passing silently, so the builder derives the set from the pack and cross-checks it against the identifiers the XML defines.

**The C++ side** is `telemetry-decode-jsonl`, reading JSON Lines of `{"frame_id": <int>, "bytes": "<32 hex chars>"}`, running them through the framer and decoder with the converted pack's roles, and writing JSON Lines of `{"frame_id", "field", "raw", "physical", "unit", "quality", "age_evidence", "stream_offset"}`. `physical` is `raw * scale + offset` in the pack's declared unit, and that is what is compared, not an SI-normalized value: the reference decoder performs no SI normalization, so comparing a converted number against an unconverted one would test signal-core's unit table rather than the parser. The unit string sits beside each row so a unit disagreement is still visible.

**The comparison** is field by field by name, within the tolerance in the numbers table. The report carries: one row per identifier per field with the reference value, the parser value, the unit and PASS or MISMATCH; a written cause beside every mismatch; a section listing identifiers with no comparable output, meaning the write-direction frame, the display-only frame and any frame whose fields are all display-only, each with the reason that the reference decoder produces nothing for it by design and what the parser does with it; a section listing the A12 unconfirmed unit conventions, naming each affected field and what is unconfirmed, **listed rather than converted**; and the counters from both sides. Zero unexplained mismatches is the gate.

`tools/tests/test_differential_check.py` covers the boundary-value record-set builder and the JSONL codec against a small synthetic pack, and skips the reference-decoder comparison when `--reference-decoder` is absent, so `pytest` stays green on a machine without the owner's repository.

## Constraints

- Sandbox session, no network access. No package manager, no `FetchContent`, no new third-party code. The only vendored libraries are the ones already at `third_party/`.
- Do not run `git commit`, `git push`, `git add`, or change git configuration. Claude commits.
- Do not modify PLAN.md or PLAN-REVIEW-LOG.md, and do not modify anything under `docs/` except `docs/reports/differential-check.md`.
- **This chunk owns exactly the files listed in the layout above and touches nothing else.** The only pre-existing files it may edit are `packages/signal-core/CMakeLists.txt` (add the two sources, three tests and the fuzz executable), `runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Errors.h` (append enumerators only), `packages/dashboard-spec/CMakeLists.txt` (add the builder source, the test and the tool executable), and `packages/signal-core/README.md` and `packages/dashboard-spec/README.md` (extend, delete nothing earlier chunks wrote). One further edit is permitted only if necessary: if the landed `tools/validate-dashboard.py` cannot validate a single definition-pack file because it cannot infer the document kind, add a `--kind definition-pack` option to it and to `tools/dashboard_spec/`, and report the addition. Change no schema, no other validator behaviour, no chunk 03 fixture, and nothing under `scripts/` or `.github/`.
- Nothing may require elevation.
- Plain factual language in every file and every string a user can see. No em dashes anywhere. No marketing words. **Do not name any commercial dashboard product**, in code, comments, file names, report text or the emitted pack. The format is `binary-telemetry-v1` or "the existing 16-byte frame format"; the tag is "tag bytes `44 33 22 11`". Do not copy XML content beyond attribute names, and do not copy the owner's decoder or any part of it into this repository.
- Follow docs/ARCHITECTURE.md: one public header per concept, nothing in `Private/` included from outside the module, no global mutable state, no singletons, injected clock, errors as values, plain structs, spans and function-pointer callbacks across boundaries, and no Unreal header in any signal-core source.
- signal-core stays exception-free and RTTI-free. The CMake build treats warnings as errors; do not relax a flag to get a file to compile.
- Environment pins: `pwsh` 7.6, Python 3.14 locally and 3.12 in CI, CMake 4.4.3 per-user, Ninja 1.13, MSVC 14.44 from Build Tools 2022 at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`. No Unreal Engine is installed, so nothing here may depend on it at build or test time.

## Non-goals

Task 4.9's TCP connector, socket, backoff schedule and connection-generation wiring: this chunk delivers the parser interface that connector will use and nothing platform-facing. The Unreal module build and the in-engine commandlet. The CI workflow files, which are chunk 04. The signals document that binds fields to signal ids. The player's age-unknown presentation, which is 4.5. Sentinel and acquisition decisions, which are the owner's: the tables ship with their defaults and the report asks the question.

## Proof (run all of these and paste full output)

Enter the MSVC environment first, as chunk 02's proof block does. `<XML_PATH>` and `<DECODER_PATH>` are supplied on the command line by Claude; do not guess them and do not hardcode them into any file.

```
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" && cd packages\signal-core && cmake --preset default && cmake --build --preset default && ctest --preset default --output-on-failure"
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" && cd packages\dashboard-spec && cmake --preset default && cmake --build --preset default && ctest --preset default --output-on-failure"

packages\signal-core\build\default\signal-core-tests.exe --reporters=console | Select-String "assertions:"
packages\dashboard-spec\build\default\dashboard-spec-tests.exe --reporters=console | Select-String "assertions:"

packages\signal-core\build\default\signal-core-telemetry-fuzz.exe --inputs 1000000 --seconds 30 --seed 1234 --corpus tests\fixtures\telemetry-fuzz

python tools/convert-existing-schema.py --xml <XML_PATH> --reference-decoder <DECODER_PATH> --out connectors/binary-telemetry-v1/definition-pack.json --report connectors/binary-telemetry-v1/README.md --pack-id vehicle.binary-telemetry-v1 --pack-version 0.1.0
python tools/validate-dashboard.py connectors/binary-telemetry-v1/definition-pack.json; "exit=$LASTEXITCODE"
packages\dashboard-spec\build\default\dashboard-spec-validate.exe connectors\binary-telemetry-v1\definition-pack.json; "exit=$LASTEXITCODE"

python tools/differential-check.py --xml <XML_PATH> --reference-decoder <DECODER_PATH> --pack connectors/binary-telemetry-v1/definition-pack.json --decoder-exe packages/dashboard-spec/build/default/telemetry-decode-jsonl.exe --report docs/reports/differential-check.md; "exit=$LASTEXITCODE"

python -m pytest tools/tests -q
pwsh -NoProfile -File scripts/validate-parity.ps1; "exit=$LASTEXITCODE"
```

Expected: both configures report C++20 with exceptions and RTTI off and the file-list drift check silent; both builds produce zero warnings, since warnings are errors; both `ctest` runs pass and both assertion lines show a non-zero count with zero failures; the fuzz run prints its input count, elapsed seconds, every counter, the false-lock count and an allocation count of zero; the converter writes the pack and its report and prints the skipped identifiers, the unit-unrepresented list and the enum-carrying fields; both validators accept the pack; the differential check exits 0 with zero unexplained mismatches and writes its report; `pytest` reports zero failures and a non-zero test count; and `validate-parity.ps1` prints an empty diff and exits 0, which is the check that nothing here disturbed chunk 03's corpus.

Run the converter twice and confirm the pack is byte-identical both times.

## Report format

End with: files added or changed, one line each giving path, what it is, and the PLAN.md task it serves; the proof output verbatim; the doctest assertion and failure counts for both packages; the fuzz harness input count, false-lock count and allocation count; the number of identifiers and fields in the emitted pack, the identifiers skipped with the reason, and which branch you took for a frame with no fields; the differential report's mismatch count and the A12 unconfirmed list; every place this spec or PLAN.md was ambiguous and what you chose; every number you chose that this spec did not give you; any deviation from PLAN.md or this spec with the reason; and anything you could not do, including anything blocked by the absent Unreal install or by a path Claude did not supply.
