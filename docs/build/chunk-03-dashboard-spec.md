# Build chunk 03: dashboard-spec

Status: frozen spec for a Codex build session. Covers PLAN.md tasks 3.1, 3.2, 3.3, the CMake half of 3.4, 3.5 as a local parity script, and 3.6. Read PLAN.md (those tasks, the Approach preamble, the pin table, the Test strategy section, Key decisions 7 and 8), docs/ARCHITECTURE.md, docs/PLUGIN-EXPERIENCE.md (the definition-pack format the 3.1 schema must cover a subset of) and docs/PROJECT-PLAN.md sections 4 and 5 before writing anything. PLAN.md is the authority on what; this file adds the exact file layout, the exact flags, the error-code vocabulary, the fixture list and the proof commands. If the two disagree, PLAN.md wins and the disagreement goes in the report. Chunk 01 has already created the repository scaffold, the READMEs, `.gitattributes`, `.clang-format`, `.clang-tidy` and `scripts/`; do not recreate them.

## Goal

One document model, two validators that agree fixture by fixture and pointer by pointer, a bounded package reader with one path resolver, and a fixture corpus that fails for a named reason rather than merely failing. Everything in this chunk runs on a machine with no engine installed. The corpus is the pass/fail criterion: a check with no fixture exercising it does not count as built.

## Dependency direction

`dashboard-spec` may include `SignalCore` headers and its own vendored JSON libraries, and nothing from the engine (docs/ARCHITECTURE.md, layering). signal-core does not parse rule JSON. **The rule-tree JSON to struct conversion belongs to this chunk.** signal-core takes rule trees through plain C++ structs built by the caller, and the JSON Lines codec for recordings lives inside signal-core and is private to it. Do not implement a recording reader or writer here, and do not vendor a JSON library into `packages/signal-core/`.

Chunk 02 has already landed: `runtime/UnRealDash/Source/SignalCore/` and `packages/signal-core/` exist in this tree. `RuleTreeBuilder` compiles against the real headers, included through `../../runtime/UnRealDash/Source/SignalCore/Public`, and its tests are always built. The declarations it uses are repeated verbatim in **Interface consumed from signal-core** below so you can check them against the headers; if the headers differ from that listing, the headers win and the difference goes in the report. Expression depth and node counting for the semantic pass reads the JSON directly and does not depend on signal-core.

## Deliverables

### 1. File layout

```
packages/dashboard-spec/schema/manifest.schema.json
packages/dashboard-spec/schema/dashboard.schema.json
packages/dashboard-spec/schema/signals.schema.json
packages/dashboard-spec/schema/showcase.schema.json
packages/dashboard-spec/schema/definition-pack.schema.json
packages/dashboard-spec/include/dashboard_spec/Errors.h
packages/dashboard-spec/include/dashboard_spec/Limits.h
packages/dashboard-spec/include/dashboard_spec/Document.h
packages/dashboard-spec/include/dashboard_spec/Validator.h
packages/dashboard-spec/include/dashboard_spec/SemanticPass.h
packages/dashboard-spec/include/dashboard_spec/PackageReader.h
packages/dashboard-spec/include/dashboard_spec/RuleTreeBuilder.h
packages/dashboard-spec/src/Errors.cpp
packages/dashboard-spec/src/BoundedParse.cpp
packages/dashboard-spec/src/SchemaValidation.cpp
packages/dashboard-spec/src/SemanticPass.cpp
packages/dashboard-spec/src/PackageReader.cpp
packages/dashboard-spec/src/PathResolver.cpp
packages/dashboard-spec/src/ImageHeader.cpp
packages/dashboard-spec/src/RuleTreeBuilder.cpp
packages/dashboard-spec/src/validate_main.cpp
packages/dashboard-spec/CMakeLists.txt
packages/dashboard-spec/CMakePresets.json
packages/dashboard-spec/tests/main.cpp
packages/dashboard-spec/tests/test_bounded_parse.cpp
packages/dashboard-spec/tests/test_schema.cpp
packages/dashboard-spec/tests/test_semantic.cpp
packages/dashboard-spec/tests/test_corpus.cpp
packages/dashboard-spec/tests/test_package_reader.cpp
packages/dashboard-spec/tests/test_rule_tree_builder.cpp
packages/dashboard-spec/tests/test_malformed_harness.cpp
tools/validate-dashboard.py
tools/dashboard_spec/__init__.py
tools/dashboard_spec/loader.py
tools/dashboard_spec/schema.py
tools/dashboard_spec/semantic.py
tools/dashboard_spec/pointers.py
tools/dashboard_spec/errors.py
tools/dashboard_spec/report.py
tools/gen-image-fixtures.py
tools/gen-document-fixtures.py
tools/gen-package-fixtures.py
tools/requirements.txt
tools/tests/test_loader.py
tools/tests/test_semantic.py
tools/tests/test_corpus.py
tools/tests/test_generators.py
tests/fixtures/documents/valid/*.json
tests/fixtures/documents/invalid/*.json + *.expected
tests/fixtures/documents/invalid/.gitignore
tests/fixtures/packages/<case-name>/...  (unpacked directories, plus cases.json)
tests/fixtures/malformed-json/*.json
scripts/validate-parity.ps1
```

Notes on the layout. Every third-party library is already vendored at the repository root under `third_party/`, each with its licence and a `REVISION.md`: `third_party/doctest/doctest.h` (v2.4.12), `third_party/rapidjson/include/rapidjson/` (commit 24b5e7a), `third_party/valijson/include/valijson/` (v1.0.5), `third_party/miniz/miniz.h` and `miniz.c` (3.0.2). Include them from there; do not copy, modify, or re-vendor them, and do not create a `third_party/` directory inside `packages/dashboard-spec/`. `tests/fixtures/malformed-json/` holds the seed corpus for the 3.4 harness and is an addition to PLAN.md's paths, which say only `tests/fixtures/`; it is kept out of `tests/fixtures/documents/` so it cannot disturb the corpus counts. `tests/fixtures/documents/invalid/.gitignore` lists the generated fixture names so the generated files are never committed.

Vendored revisions are recorded in each library's `REVISION.md` at the repository root; PLAN.md 3.4 requires pinned revisions and those files are the pins. No network access is available in this session. ZIP reading uses the vendored **miniz** (MIT) rather than a hand-written ZIP parser: a central-directory parser written from scratch is exactly the code a hostile archive is aimed at.

### 2. Limits, verbatim from PLAN.md 3.6

All chosen defaults, not measured ones. One shared set, applied identically to a `.udash` archive and to an unpacked directory.

| Limit | Default |
| --- | --- |
| Total expanded size | 64 MB |
| Entry count | 4096 |
| Bytes per asset | 16 MB |
| Bytes per JSON document | 4 MB |
| JSON nesting depth | 64 |
| JSON node count per document | 200,000 |
| JSON string length | 64 KB |
| Expression depth | 32 |
| Expression nodes per document | 20,000 |
| Expression nodes per rule | 512 |
| Components per document | 2000 |
| History samples per graph component | 4096 |
| Image pixel dimensions | 4096 by 4096 |
| Aggregate decoded texture budget, mobile profile | 192 MB |
| Aggregate decoded texture budget, desktop profile | 512 MB |

Expression depth 32 and 512 nodes per rule come from PLAN.md 2.4 and are also chosen defaults; both live in the table above. `Limits.h` and `tools/dashboard_spec/errors.py` hold these numbers once each; nothing else hardcodes them.

### 3. Document model (task 3.1)

Five draft-7 schemas, each with `"$schema": "http://json-schema.org/draft-07/schema#"`, `"additionalProperties": false` on every object, and no `default` keyword anywhere, because a default hides an authoring omission.

**dashboard.schema.json.** `reference_viewport` with integer `width` and `height`. `components` is an **ID-keyed map**, not an array of objects carrying an `id`, because draft-7 cannot express uniqueness of a property across array items and `uniqueItems` compares whole objects. Component IDs match `^[a-z0-9][a-z0-9_.-]{0,63}$`, are stable and are opaque to the runtime. Each component carries `type`, optional `parent`, `rect` against the reference viewport, `anchor` (one of `top_left`, `top`, `top_right`, `left`, `center`, `right`, `bottom_left`, `bottom`, `bottom_right`), `scaling` in `{uniform, stretch, none}`, `clipping` boolean, `aspect_policy` in `{preserve, stretch, inherit}`, a required `missing_data` object, and a type-specific `properties` object.

The nine Stage 0 primitive type names, and no others: `readout` (text and numeric), `image`, `shape` (line and shape), `analog_dial`, `bar_gauge` (arc and bar), `indicator`, `history_graph`, `container`, `page_switch`.

`missing_data` is required on every component and has no default. It carries one entry for each of `stale`, `unavailable`, `invalid` and `age_unknown`, each naming a presentation in `{dash, hidden, last_value_dimmed, icon}` plus a theme token. `age_unknown` is the distinct presentation for a signal fed by a `held` definition-pack field and must be authorable as visibly different from both valid and stale.

`theme` carries `day` and `night`, each a map of token name to colour. A colour literal is a lowercase string `#rrggbb` or `#rrggbbaa`; theme token values use the same literal form. Every colour in a component is either such a literal or `{"token": "<name>"}`; a token reference resolves in the semantic pass.

`bindings` is keyed by component ID, each entry naming a `property`, a `signal` and an optional `format`. `rules` is keyed by rule ID, each entry carrying `expression` (the signal-core expression tree as JSON), `hysteresis` with a value and a unit, `debounce_ms`, `missing_input_policy` in `{hold_last, treat_unavailable, force_warn}`, `maximum_hold_ms` required when the policy is `hold_last`, and `targets` naming component IDs.

Expression node JSON, whitelisted operations only, matching signal-core's `Operation` enum name for name: `literal`, `signal`, `add`, `subtract`, `multiply`, `divide`, `less`, `less_or_equal`, `greater`, `greater_or_equal`, `equal`, `not_equal`, `logical_and`, `logical_or`, `logical_not`, `minimum`, `maximum`, `clamp`, `absolute_value`. A `literal` node requires both `value` and `unit`; `unit` may be `"dimensionless"` but never omitted, because a dimensionless literal must say so explicitly rather than by omission. A `signal` node requires `signal`. No node accepts a string expression, anywhere, in any form.

**signals.schema.json.** Per signal: id, type, unit, `freshness_deadline_ms`, `discrete` boolean, optional source mapping and optional display unit.

**manifest.schema.json.** `schema_version`, `package_id`, `revision`, `runtime_compatibility`, and `assets` as a map from archive-relative path to an entry carrying **`width`, `height`, `format` and `bytes`**, all four required. `format` is one of `rgba8`, `rgb8`, `gray8`. Without those four, 3.6's decoded-texture budget has nothing to compute from before it decodes, which is the one moment at which refusing is still cheap.

**showcase.schema.json.** The bounded sidecar parameter set only, per Key decision 9. No mesh instances, cameras, transform hierarchies or state machines; a full 3D component schema is not Stage 0.

**definition-pack.schema.json.** The subset of the docs/PLUGIN-EXPERIENCE.md format that binary-telemetry-v1 needs: `id`, `version`, `api`, `input`, and `frames` as an explicit enumeration, one entry per valid frame identifier, each with `id`, `id_format`, `length`, where `length` is the **payload** length, and a **required** `role` of `telemetry` or `status` with no default. A `status` frame is one the transport may repeat on its own (the existing relay re-emits the board's status frame as a heartbeat), so its arrival proves the transport and not the acquisition; the parser in a later chunk uses `role` to keep transport health and acquisition health separate, and the converter marks such frames `status` with every field `held`. Each field carries `name`, `byte_offset` measured from payload byte 0, `type` in `{uint8, uint16, uint32}` which carries only the field width, `byte_order`, a required boolean `signed` which alone determines signedness (the converter derives it from the frame default plus any per-value override, so the pack is explicit), `scale`, `offset`, `unit`, a **required** `acquisition` of `live` or `held` with no default, and an optional list of raw `sentinels`. Conversion is affine, `physical = raw * scale + offset`, with `offset` defaulting to 0 in the semantics but stated explicitly in the document. Bit fields and multiplexing are not in the Stage 0 schema, and `additionalProperties: false` is what rejects `bit_offset` and `multiplex` rather than ignoring them.

### 4. The shared semantic pass (task 3.1)

Runs after schema validation, implemented identically in Python (3.3) and C++ (3.4), reporting the same JSON pointer from both. Order of the whole pipeline: bounded parse and document limits, then schema validation, then the semantic pass.

1. Duplicate object keys rejected at parse time, not later.
2. Every signal binding, rule signal reference, theme token reference and page reference resolved against the documents actually present.
3. Every image reference resolved to a manifest entry, so a component pointing at a file with no declared metadata is a failure rather than an unbudgeted decode.
4. Component hierarchy is a tree: every component has at most one parent, exactly one root, no cycle including a component naming itself, every referenced parent exists, and every component reachable from the root, so an orphan subtree is named rather than silently unrendered.
5. Aspect policy resolved **through container inheritance**, so an inherited `stretch` on a circular gauge is caught and not only a directly declared one. A circular gauge is an `analog_dial`, or a `bar_gauge` whose properties declare it circular.
6. Every definition-pack field checked so that `byte_offset + width_bytes(type) <= frame.length`.
7. Expression node totals and depth checked against the 2.4 limits, per rule and per document.
8. Rule literal units checked for compatibility against the declared signal units: comparison and additive arithmetic require the same quantity, multiplicative arithmetic composes.

Two components referencing the **same asset** is explicitly valid and is not a duplicate of anything. What stays invalid is two archive entries normalizing to one path, which is a packaging fault and belongs to 3.6.

### 5. Error codes

One vocabulary, shared by both validators, stable values, defined once in `include/dashboard_spec/Errors.h` and once in `tools/dashboard_spec/errors.py`, with a test in each language asserting the two lists are identical by reading the other side's file.

Parse and limits: `E_JSON_SYNTAX`, `E_DUPLICATE_KEY`, `E_DOC_TOO_LARGE`, `E_JSON_TOO_DEEP`, `E_JSON_TOO_MANY_NODES`, `E_JSON_STRING_TOO_LONG`.
Schema: `E_SCHEMA`.
Semantic: `E_UNKNOWN_COMPONENT_TYPE`, `E_UNRESOLVED_SIGNAL`, `E_UNRESOLVED_THEME_TOKEN`, `E_UNRESOLVED_PAGE`, `E_UNRESOLVED_PARENT`, `E_MULTIPLE_PARENTS`, `E_COMPONENT_CYCLE`, `E_NO_ROOT`, `E_MULTIPLE_ROOTS`, `E_ORPHAN_SUBTREE`, `E_ASPECT_POLICY_FORBIDDEN`, `E_IMAGE_NOT_IN_MANIFEST`, `E_ASSET_REFERENCE_FORBIDDEN`, `E_FIELD_PAST_FRAME_LENGTH`, `E_UNKNOWN_RULE_OPERATION`, `E_RULE_UNIT_MISMATCH`, `E_EXPRESSION_TOO_DEEP`, `E_EXPRESSION_TOO_MANY_NODES`, `E_TOO_MANY_COMPONENTS`, `E_TOO_MANY_HISTORY_SAMPLES`.
Package, emitted by the C++ package reader only but present in both vocabularies so the identity test holds: `E_PKG_EXPANDED_SIZE`, `E_PKG_ENTRY_COUNT`, `E_PKG_ASSET_SIZE`, `E_PKG_PATH_ABSOLUTE`, `E_PKG_PATH_TRAVERSAL`, `E_PKG_PATH_DRIVE_LETTER`, `E_PKG_PATH_LINK`, `E_PKG_DUPLICATE_ENTRY`, `E_PKG_CASE_COLLISION`, `E_PKG_IMAGE_DIMENSIONS`, `E_PKG_IMAGE_HEADER_MISMATCH`, `E_PKG_IMAGE_FORMAT_UNSUPPORTED`, `E_PKG_TEXTURE_BUDGET`, `E_PKG_ZIP_MALFORMED`.

Every failure carries a code, a JSON pointer escaped per RFC 6901, and a message naming the offending input. Enforcement points are fixed so no code is dead: anything the schema can express (required properties such as `acquisition`, `unit` on a literal, the four manifest metadata fields, `role`; enumerations; `additionalProperties: false`) is rejected by the schema as `E_SCHEMA` at the offending pointer, and the semantic pass only carries checks the schema cannot express. Duplicate component IDs are duplicate JSON keys in the ID-keyed map and are rejected at parse time as `E_DUPLICATE_KEY`. `E_ASSET_REFERENCE_FORBIDDEN` is a document-level syntactic check performed by both validators on every asset reference string: absolute paths, any `..` segment, a drive letter, or a backslash are rejected before any package is opened; the package reader's resolver then handles what only a real file system can reveal (links, duplicate normalized entries, case collisions, escape of the package root).

### 6. C++ validator, CMake half (task 3.4)

- `cmake_minimum_required(VERSION 3.25)`, C++20, extensions off. Library target `dashboard_spec`, command-line target `dashboard-spec-validate` from `src/validate_main.cpp`, test target `dashboard-spec-tests`.
- Exceptions and RTTI off: `-fno-exceptions -fno-rtti` on GCC and Clang, `/EHs-c- /GR-` on MSVC with the default `/EHsc` and `/GR` removed from `CMAKE_CXX_FLAGS` by `string(REPLACE ...)` first, otherwise MSVC reports D9025 and keeps the default. Warnings as errors: `-Wall -Wextra -Wpedantic -Werror`, or `/W4 /WX /permissive-`.
- valijson is compiled with `VALIJSON_USE_EXCEPTIONS=0`. RapidJSON is compiled with `RAPIDJSON_HAS_STDSTRING=1` and its assert macro pointed at a reporting function rather than `assert`. miniz compiles as C with its own warning flags, not with `-Werror`.
- **The R-G contract.** If valijson will not compile with exceptions off, enable exceptions on this target only, record the exact compile error that triggered it in the report and in `packages/dashboard-spec/README.md`, and keep validation intact. Never drop schema validation, never fall back to a structural-only check, and never disable the parity script. RTTI stays off in both branches. Do not take this fallback speculatively: try the no-exceptions build first and report what happened.
- `CMakePresets.json`, `"version": 6`, a `default` Ninja configure preset at `build/default` with `RelWithDebInfo`, plus matching build and test presets named `default` with `outputOnFailure` true.
- Duplicate-key detection and the JSON limits run in **one RapidJSON SAX pre-pass** before any DOM is built: the handler tracks member names per object level and reports the first duplicate with its pointer, and enforces depth, node count and string length as it goes. RapidJSON has no option that rejects duplicate keys, so PLAN.md 3.4's wording is implemented this way; report it. The DOM is built after the pre-pass has bounded the input, and valijson validates the DOM.
- The malformed-document harness lives in `tests/test_malformed_harness.cpp`, seeded from `tests/fixtures/malformed-json/`, mutating and generating JSON, stopping at whichever comes first of 1 million inputs or 30 seconds, printing the count it reached. No crash, no unbounded allocation, no hang.

### 7. Package reader (task 3.6)

One reader, one limit set, one path resolver, two forms: a `.udash` archive and an unpacked directory. The unpacked path is not an unbounded bypass of the packed one.

The resolver rejects any reference or archive entry that is absolute, contains `..` after normalization, contains a drive letter, is a symbolic or hard link, resolves outside the package root, duplicates another entry after normalization, or collides with another entry under case-insensitive comparison. There is no second path that reaches an asset, so a document cannot name a file the archive gate never inspected.

Image handling: read each referenced image's actual file header, a bounded read of a few dozen bytes and not a decode, and compare width, height, format and byte size against that asset's manifest entry. Any mismatch is `E_PKG_IMAGE_HEADER_MISMATCH` naming both the declared and the read values. Stage 0 reads PNG only, mapping IHDR colour type 6 to `rgba8`, 2 to `rgb8` and 0 to `gray8` at bit depth 8, and rejecting anything else as `E_PKG_IMAGE_FORMAT_UNSUPPORTED`; PLAN.md does not restrict the format, so record this restriction. The aggregate decoded-texture budget is computed from the **declared** manifest dimensions and format, summed across every asset the loaded documents reference, before any decode or upload, and a package over the active profile's budget is rejected rather than partially loaded. The CLI takes `--profile mobile|desktop` and defaults to `desktop`.

Expression node and depth limits, JSON nesting, node and string limits, component counts and history-sample counts are enforced by the same code the document validator uses.

### 8. Python validator and tools (task 3.3)

`tools/validate-dashboard.py` is a thin CLI over `tools/dashboard_spec/`. It takes a directory or a file, plus `--report <path>`, `--profile mobile|desktop` and `--json`. It rejects duplicate object keys at parse time through a custom `object_pairs_hook`, since Python's default decoder keeps the last duplicate silently, and enforces the document limits before schema validation, since a deeply nested document must not reach the parser stack on its way to being rejected. Then draft-7 validation with `jsonschema`, then the shared semantic pass.

Over a directory it walks `valid/` and `invalid/`, asserts each fixture lands on its own side, asserts every invalid fixture fails at the JSON pointer in its `.expected` sibling, prints the valid and invalid counts, and exits non-zero if any fixture lands on the wrong side or fails at the wrong pointer.

`.expected` file format, one fixture per file, LF endings: line 1 the error code, line 2 the JSON pointer. A fixture with more than one expected failure lists the first one only, the one the validator is required to report.

`tools/requirements.txt`: `jsonschema>=4.23,<5` and `pytest>=8,<9`. Ranges rather than exact pins because the tools must run on Python 3.12 in CI and 3.14 locally; record the resolved versions in the report. Both are already installed for the current user; do not run pip install (no network).

Generators, all standard library only, no Pillow, deterministic for a given script revision:

- `tools/gen-image-fixtures.py` writes the PNGs with `zlib` and `struct`: the well-formed image, the over-wide image at 4097 pixels, the image whose header contradicts its manifest entry, and the set that exceeds the mobile texture budget. PLAN.md 1.5 names a Pillow script; this spec deviates so CI needs no third-party dependency and so the generator matches the stdlib PNG writer chunk 01 already used. Record the deviation.
- `tools/gen-document-fixtures.py` writes the size-driven invalid fixtures and their `.expected` siblings: nesting past 64, node count past 200,000, a string past 64 KB, a document past 4 MB, expression nodes past 20,000, and a component count past 2000. It also writes the at-limit valid counterparts, at exactly 64, exactly 2000 components and exactly 4096 history samples.
- `tools/gen-package-fixtures.py` builds the `.udash` archives from `tests/fixtures/packages/<case>/` using `zipfile`, driven by `tests/fixtures/packages/cases.json`, which declares each case's entries including the hostile ones that cannot exist as real files: the zip bomb, the `../../etc/passwd` entry, the absolute path, the drive-lettered path, the symlink entry, the duplicate normalized entry, the case-only collision, the 5000-entry archive and the 20 MB asset. Archives are generated, never committed.

Generated files are listed in `tests/fixtures/documents/invalid/.gitignore` and in a `.gitignore` inside `tests/fixtures/packages/`. The chunk 01 LFS rule stays intact: no new binary is committed, so nothing new enters LFS.

`tools/tests/` is pytest over the loader, the semantic pass, the corpus and the generators, including a test that two runs of each generator produce byte-identical output.

### 9. Fixture corpus (task 3.2)

At least 20 valid and at least 30 invalid, counted after the generators have run. Every limit named in 3.6 and every check named in the 3.1 semantic pass has at least one invalid fixture.

Invalid, each with an `.expected` sibling (the expected code for a schema-level omission is `E_SCHEMA`): duplicate component key (`E_DUPLICATE_KEY`); nesting past the depth limit; node count past the limit; over-long string; document over 4 MB; container cycle; component naming itself as parent; component with two declared parents; orphan subtree; two roots; image reference with no manifest entry; manifest entry missing `width` (`E_SCHEMA`); definition-pack field running past its frame's payload length; definition-pack field with no `acquisition`; definition-pack frame with no `role`; definition-pack frame with a `role` outside `{telemetry, status}`; directly declared `stretch` aspect policy on a circular gauge; `stretch` inherited from a container onto a circular gauge; asset reference containing `..`, absolute asset path, drive-lettered asset path and backslash asset path (all `E_ASSET_REFERENCE_FORBIDDEN`, checked by both validators); rule referencing a signal no loaded document declares; unknown rule operation; unit-incompatible comparison of a pressure literal against a temperature signal; literal with no `unit`; expression at depth 33; expression at 513 nodes in one rule; expression nodes past 20,000 in a document; unknown component type; unresolved theme token; unresolved page reference; component count past 2000; history graph past 4096 samples.

Package cases under `tests/fixtures/packages/`, C++ package reader only (the Python validator has no package reader, so these never enter the documents corpus or the parity comparison). Representable cases run twice each, once as an archive and once as an unpacked directory, producing the same code both ways; the hostile cases that cannot exist as real files on Windows without elevation (zip bomb, traversal entry, absolute entry, drive-lettered entry, symlink entry, duplicate normalized entry, case-only collision) run as archives only and the report says so. Cases: image header contradicting the declared manifest metadata; over-wide image at 4097 pixels; declared texture set over the mobile budget; zip bomb, `../../etc/passwd` entry, 20 MB asset, 5000-entry archive, symlink entry, absolute entry, drive-lettered entry, two entries normalizing to the same path, two entries differing only in case, document referencing `../../outside.png` from inside an otherwise well-formed archive, image header mismatch, image with no manifest entry, and a well-formed package that loads in both forms.

Valid: minimal single-component dashboard; all nine primitives; **two components referencing the same asset with one manifest entry**, which must pass and is what stops the invalid set being read too broadly; complete day and night theme tokens; containers nested three deep; a page switch with two pages; a rule with hysteresis and debounce; one fixture per missing-input policy; a rule using every whitelisted operation; an expression at exactly depth 32; a rule at exactly 512 nodes; a literal declared `dimensionless` explicitly; a degC threshold against a kelvin-normalized signal; a definition pack covering a signed frame, an unsigned override, a scaled field, a non-zero offset, a `held` field, a sentinel list and one `status`-role frame whose fields are all `held`; a definition-pack field ending exactly at the frame boundary; a circular gauge under a container with `preserve`; a signals document with per-signal deadlines and discrete flags; a manifest with several assets and complete metadata; a showcase document with its sidecar parameter set; missing-data rendering declared per component including `age_unknown`; and the at-limit generated valid fixtures.

`tests/fixtures/packs/binary-telemetry-v1.test.json` belongs to the chunk that builds PLAN.md 2.9. Do not create it. The definition-pack fixtures in this chunk's own corpus are what prove the schema.

### 10. Parity (task 3.5, local half)

Both validators emit the same report format, one line per fixture, tab separated, sorted by relative path with forward slashes, LF endings, always exactly four tab-separated fields (a PASS line therefore ends with two empty fields and their separating tabs; that is not trailing whitespace), no spaces at line end:

```
<relative-path>\t<PASS|FAIL>\t<error-code-or-empty>\t<json-pointer-or-empty>
```

`scripts/validate-parity.ps1` runs the Python validator and the CMake-built C++ validator over `tests/fixtures/documents` with `--report` into two temporary files, compares them line by line, prints any differing lines with both sides, exits 0 on an empty diff and 1 otherwise. It takes `-PythonExe`, `-ValidatorExe` and `-CorpusPath` with sensible defaults so CI can override them. The CI job that calls it is a later chunk; this chunk delivers the script and proves it locally.

### 11. Interface consumed from signal-core

Write `RuleTreeBuilder` against exactly these declarations from chunk 02. Do not rename, do not redeclare them locally, and do not copy them into this package.

```cpp
namespace signal_core {
enum class Quality : std::uint8_t { valid, stale, unavailable, invalid };
enum class AgeEvidence : std::uint8_t { measured, unknown };
enum class Quantity : std::uint8_t { dimensionless, temperature, pressure, speed, angular_rate };
enum class Unit : std::uint16_t {
  dimensionless, kelvin, degree_celsius, degree_fahrenheit,
  pascal, kilopascal, bar, psi,
  metres_per_second, kilometres_per_hour, miles_per_hour,
  radians_per_second, revolutions_per_minute
};
enum class Operation : std::uint8_t {
  literal, signal_reference,
  add, subtract, multiply, divide,
  less, less_or_equal, greater, greater_or_equal, equal, not_equal,
  logical_and, logical_or, logical_not,
  minimum, maximum, clamp, absolute_value
};
enum class MissingInputPolicy : std::uint8_t { hold_last, treat_unavailable, force_warn };
struct ExpressionNode {
  Operation operation{};
  double literal_value{};
  Unit literal_unit{};
  std::uint32_t signal{};
  std::uint16_t first_child{};
  std::uint8_t child_count{};
  const char* source_pointer{};
};
struct RuleDefinition {
  std::uint32_t rule_id{};
  std::span<const ExpressionNode> nodes{};
  double hysteresis_band{};
  Unit hysteresis_unit{};
  std::chrono::nanoseconds debounce{};
  MissingInputPolicy missing_input_policy{};
  std::chrono::nanoseconds maximum_hold{};
};
Result<Unit> ParseUnit(std::string_view);
Quantity QuantityOf(Unit);
}  // namespace signal_core
```

`RuleTreeBuilder` flattens the JSON expression into a `std::vector<ExpressionNode>` owned by the caller, root at index 0, and sets `source_pointer` on each node to the JSON pointer of the node it came from so a signal-core load error names the same pointer the Python validator does. The JSON operation name `signal` maps to `Operation::signal_reference`; every other name maps by its own spelling.

## Constraints

- Do not run `git commit`, `git push`, `git add`, or change git configuration. Claude commits.
- Do not modify PLAN.md, PLAN-REVIEW-LOG.md, or anything under `docs/`. The only READMEs this chunk may write are `packages/dashboard-spec/README.md` and `tools/README.md`. Chunk 01 created both, and it recorded the account's LFS storage and bandwidth allowance in `packages/dashboard-spec/README.md`; extend both files and delete nothing chunk 01 wrote, the LFS note included.
- Nothing in this chunk may require elevation. If a step needs it, stop and report it.
- Plain factual language in every file and every string a user can see. No em dashes anywhere. No marketing words. Do not name any commercial dashboard product.
- Follow docs/ARCHITECTURE.md: one public header per concept, errors as values, no global mutable state, no singletons, nothing from the engine, and Python tools that depend only on `packages/dashboard-spec/schema/` and `tests/fixtures/` and never import from the runtime tree.
- Environment pins: `pwsh` 7.6, Python 3.14 locally and 3.12 in CI, CMake 4.4.3 installed per-user, Ninja 1.13, MSVC 14.44 from Build Tools 2022 at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`. No Unreal Engine is installed on this machine, so nothing in this chunk may depend on it at build or test time.
- **This chunk owns `packages/dashboard-spec/`, `tools/`, `tests/fixtures/documents/`, `tests/fixtures/packages/`, `tests/fixtures/malformed-json/` and `scripts/validate-parity.ps1`, and touches nothing else.** Chunk 02 owns `runtime/UnRealDash/Source/SignalCore/` and `packages/signal-core/`. Chunk 02 is already merged; a write outside this chunk's directories is a merge conflict with later chunks. Do not create or edit anything under `runtime/` or `packages/signal-core/`, do not touch other files under `scripts/`, and do not create `.github/` workflows; those are other chunks.
- Every Python file runs on 3.12 and 3.14 without a version check.

## Non-goals

Tasks 3.7 and 3.8: the offline converter for the owner's XML schema, and arbitration of parity mismatches. Also out: the Unreal-module half of 3.4, which needs an engine; the CI workflow files for 3.5, which are a later chunk; any runtime widget construction; any recording reader or writer; any frame parser; any 3D component schema; and `tests/fixtures/packs/`.

## Proof (run all of these and paste full output)

From the repository root in `pwsh`, Python side first:

```
python -m pip show jsonschema pytest | Select-String -Pattern '^(Name|Version)'
python tools/gen-image-fixtures.py
python tools/gen-document-fixtures.py
python tools/gen-package-fixtures.py
python -m pytest tools/tests -q
python tools/validate-dashboard.py tests/fixtures/documents; "exit=$LASTEXITCODE"
```

Then the C++ side, inside the MSVC environment. Either use "Developer PowerShell for VS 2022" or keep the environment in one `cmd` invocation:

```
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -products * -latest -property installationPath
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" && cd packages\dashboard-spec && cmake --preset default && cmake --build --preset default && ctest --preset default --output-on-failure"
packages\dashboard-spec\build\default\dashboard-spec-tests.exe --reporters=console | Select-String "assertions:"
```

Then parity, and the four 3.6 rejection cases as separate runs so four distinct codes are visible:

```
pwsh -NoProfile -File scripts/validate-parity.ps1; "exit=$LASTEXITCODE"

packages\dashboard-spec\build\default\dashboard-spec-validate.exe --package tests\fixtures\packages\zip-bomb.udash --profile mobile
packages\dashboard-spec\build\default\dashboard-spec-validate.exe --package tests\fixtures\packages\path-traversal.udash --profile mobile
packages\dashboard-spec\build\default\dashboard-spec-validate.exe --package tests\fixtures\packages\oversize-asset.udash --profile mobile
packages\dashboard-spec\build\default\dashboard-spec-validate.exe --package tests\fixtures\packages\entry-count.udash --profile mobile
```

Expected: `pytest` reports zero failures and a non-zero test count; the Python validator exits 0 and prints valid and invalid counts that match the corpus, at least 20 and at least 30; the CMake configure reports C++20 with exceptions and RTTI off, or reports the R-G fallback with the compile error that forced it; the build produces zero warnings, since warnings are errors; `ctest` passes and the assertion line shows a non-zero count with zero failures; the malformed-document harness prints the input count it reached; `validate-parity.ps1` prints an empty diff and exits 0; and the four package runs print `E_PKG_EXPANDED_SIZE`, `E_PKG_PATH_TRAVERSAL`, `E_PKG_ASSET_SIZE` and `E_PKG_ENTRY_COUNT`, four distinct codes, each with a non-zero exit.

Also report any difference between the signal-core headers in the tree and the interface listing above.

## Report format

End with: files added or changed, one line each giving path, what it is, and the PLAN.md task it serves; the proof output verbatim; the valid and invalid fixture counts and the doctest assertion count; the vendored revisions you actually fetched for RapidJSON, valijson, miniz and doctest; whether the R-G fallback was taken and the exact compile error if it was; every place this spec or PLAN.md was ambiguous and what you chose; any deviation from PLAN.md or this spec with the reason; and anything you could not do, including anything blocked by the absent Unreal install.
