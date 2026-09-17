# Dashboard specification

The document model, schemas, validation and package reader belong here (PLAN.md 3.x).
This layer may depend on SignalCore and vendored JSON libraries. Unreal types,
rendering and upward dependencies do not belong here.

GitHub LFS storage and bandwidth allowance: to be read from the GitHub billing page by the owner; not yet recorded.

Chunk 03 implements PLAN.md 3.1, 3.2, 3.3, the CMake half of 3.4,
the local script half of 3.5, and 3.6. The five schemas use draft-7.
All limits are chosen defaults. MB and KB mean powers of 1024 here.
`include/dashboard_spec/Limits.h` and `tools/dashboard_spec/errors.py`
are the respective single definitions of the shared limits.

Each document may be validated on its own. A corpus fixture needing several
documents is an object keyed by `dashboard`, `signals`, `manifest`, `showcase`
or `definition-pack`. This is a test and CLI document-set envelope, not a sixth
schema or a package format. References resolve only against that set. The
envelope itself has the same parser bounds as a single input file. Packages
load the five named root documents and apply each document's parse bounds
before assembling the set. Other JSON entries are rejected with E_PKG_UNSUPPORTED_DOCUMENT; no accepted document is discarded.

The schemas make the field shapes left open by the frozen spec explicit:

- Signals are a `signals` array. Source mappings have `connector` and `field`.
  The supported types are `number` and `boolean`; unit spellings match
  SignalCore's `ParseUnit` table.
- A rectangle carries `x`, `y`, `width` and `height`. Type-specific properties
  are closed objects. Page definitions map page names to component ID lists.
  A page switch carries `pages` and `initial_page`.
- A parent is a single string ID. The schema makes multiple parents inexpressible.
  Omitting parent declares a root.
- Expressions carry `op`. Unary operations have `a`; binary operations and
  `clamp` have an ordered `args` array of exactly two or three children.
  Leaves have only the fields specified for `literal` or `signal`. This keeps
  a depth-32 unary tree within the JSON limit and lets the valid nesting fixture
  exercise exactly 64 container levels with a mixture of operators.
- Showcase values are material scalars in [0, 1], transition milliseconds in
  [0, 10000], page indices in [0, 15], and literal colour tokens. These are
  chosen Stage 0 ranges. No value is clamped and no geometry is expressible.
- Manifest version and definition-pack API are integer 1, revision is a positive
  integer, and runtime compatibility is a string. Definition-pack frame fields
  use the `signals` key from PLUGIN-EXPERIENCE.md. Asset keys cannot be empty or
  contain control characters. Path safety remains a separate semantic check.

The validation sequence is bounded parse, schema, then semantics. JSON nodes
count every value including containers, but not object keys. Container depth
starts at one; string limits count decoded UTF-8 bytes. The Python preflight
walk bounds recursion before the standard decoder builds a document; its
`object_pairs_hook` also rejects duplicates. C++ performs one RapidJSON SAX
pre-pass for duplicates and limits, then builds a DOM. RapidJSON has no duplicate
key option. Both return the first parse violation in input order. C++ sorts DOM
members after parsing so schema errors are independent of object member order.
Schema diagnostics choose the lexically first leaf pointer; missing and extra
properties point at the property, including one that is absent.

The semantic order is manifest reference syntax, definition-pack byte bounds,
component count, parent resolution, hierarchy, inherited aspect policy and
component limits, image and page references, theme references, page membership,
bindings, then rules. Map keys are visited lexically; arrays retain their order.
A rootless cycle reports `E_COMPONENT_CYCLE`; a disconnected cycle alongside a
root reports `E_ORPHAN_SUBTREE`. A self-parent always reports a cycle. An empty
component map reports `E_NO_ROOT`. With a single-parent graph, no other orphan
shape remains after parent and root checks. Inheritance follows container
ancestors; an unresolved root `inherit` behaves as preserve for the stretch
prohibition. Missing binding owners and rule targets report
`E_UNRESOLVED_COMPONENT`; parent references alone use `E_UNRESOLVED_PARENT`.

Colour references are visited only in documented component colour fields and
missing-data presentation tokens. Component IDs remain opaque, including the ID
`token`. Both visitors explicitly bound depth, and C++ uses length-aware lookup
for every authored name. Colour literals use an absolute end assertion that
rejects terminal newlines in both regex engines. Component property dispatch
ends in a false type schema, so extending the enum without a branch fails.
The public semantic entry goes through Validator; its private pass uses guarded
member and type reads that return E_SCHEMA with the field pointer.


Expressions count root depth as one and include leaves in node totals. Traversal
is preorder. Unit quantities have four exponent coordinates: temperature,
pressure, speed and angular rate. Addition, comparisons, extrema and clamp
require equal dimensions; multiply and divide add or subtract exponents;
logical operations require dimensionless inputs. Compatible temperature units
therefore pass without requiring equal unit spellings. Conversion to SI stays
in SignalCore. Unknown operation and component enums report E_SCHEMA.
Unused unknown-name and multiple-parent codes were removed by review request.
Remaining numeric values are unchanged, with removed values left unused.
New codes are appended: E_PKG_UNSUPPORTED_DOCUMENT (42),
E_UNRESOLVED_COMPONENT (43), and E_PKG_ASSET_NOT_IN_PACKAGE (44).

`RuleTreeBuilder` owns a vector through caller-owned builder storage. It returns
a `RuleDefinition` span rooted at index zero, with immediate children contiguous
and pointer text owned alongside the nodes. Keep the builder alive and do not
rebuild it while consuming a returned rule. This preserves caller ownership
without exposing standard-library containers in an engine-facing API. Validate
the document first; conversion also checks structural and representation bounds.
The consumed SignalCore declarations match the frozen interface. The actual
headers additionally provide `kLiteralUnitOmitted` and other APIs. Omitted units
are rejected by this schema, so the sentinel is not needed here.

Package validation uses miniz and one resolver for both forms. A bounded EOCD/ZIP64 preflight checks declared entry counts and
central directory size before miniz initialization. The central directory bound
is min(expanded_size, entry_count * (46 + 3 * string_length)); ambiguous footer
signatures, split archives and inconsistent offsets are rejected. Entry count and
expanded-size gates run before contents are read; document and asset size gates
run before allocation. Directory traversal rejects symbolic links, hard links
and paths leaving the canonical root. ZIP entries must prove a regular file or directory type; DOS reparse points
and unknown file types are rejected. JSON is streamed through the miniz iterator
with the document cap, CRC completion and declared-versus-actual length checks. Normalized
duplicates and case collisions are distinct errors. Case comparison uses the
Windows invariant Unicode mapping, or a privately owned C.UTF-8 locale on POSIX;
it never changes the process locale. Asset references can reach only inspected
entries; a missing file reports E_PKG_ASSET_NOT_IN_PACKAGE. Manifest keys
and references use the same resolver normalization. Manifest normalized and
case-folded collisions are rejected separately. Shared references are deduplicated for the decoded texture budget.
Only PNG headers at bit depth 8 and colour types 6, 2 and 0 are supported, mapping
to rgba8, rgb8 and gray8. Reads inspect 33 header bytes and do not decode pixels.
Width, height, format and actual file size must all match the manifest before
header-confirmed dimensions contribute to the budget. No decode or upload occurs.

The 42 generated archive cases include 26 cases also exercised as directories.
Hostile entry paths, link metadata, normalized and case-folded entry collisions,
bombs, malformed ZIPs, oversized footer declarations and under-declared JSON
sizes are archive-only. The 100,000-entry declaration fixture occupies 98 bytes.
No elevation is requested. Package-only failures stay out of document parity.
All generated binaries and size-driven documents are ignored by local
`.gitignore` files. The small corpus files and their expected failures are text.

Both CLIs support `--report`, `--profile mobile|desktop` and `--json`; C++ also
supports `--package`. Python has no package reader, so profile selection does
not change document-only checks. Reports are sorted UTF-8, LF, four-field TSV.
PASS rows end in two tabs. To keep arbitrary malformed-key pointers from
breaking the TSV framing, field characters U+0000 through U+0020 and backslash
are written as `\u00xx`; `--json` carries the original pointer. This resolves
the otherwise incompatible requirements of arbitrary RFC 6901 keys and exactly
four fields with no terminal spaces. Corpus expectations compare raw pointers.

CMake builds with C++20, exceptions off, RTTI off and project warnings as errors.
miniz is compiled as C with separate warning flags. All four dependencies are
used from the existing repository-root `third_party`; nothing was fetched or
copied. The frozen spec's root-level location supersedes PLAN.md 3.4's older
nested-vendor path, as explicitly required by this task.

The initial no-exceptions build found this valijson 1.0.5 adapter issue:

```text
rapidjson_adapter.hpp(48): fatal error C1021: invalid preprocessor command 'warning'
```

The adapter uses `#ifdef VALIJSON_USE_EXCEPTIONS`, so defining it to zero still
enters a branch that conflicts with the required reporting assertion macro.
`SchemaValidation.cpp` loads valijson's exception policy with value zero, hides
the macro for the adapter include, then restores zero. No vendored file changes
and no exceptions fallback are needed. Full valijson validation remains enabled.

Local proof used MSVC 14.44 with the specified `vcvars64.bat`. The inherited PATH
contained an unmatched quote and needed process-local repair. The per-user
CMake 4.4.3 and Ninja 1.13 files were access-denied to the sandbox account.
The installed Visual Studio CMake 3.31.6-msvc6 and Ninja 1.12.1 ran the same preset
successfully. This is a tool-version proof deviation, not a claim that the pinned
versions were tested. No installation, network access or persistent environment
change was performed. Proof output is reported in the session response; reports are not stored in this source tree.


## Definition pack builder and differential tool (tasks 2.9, 2.12, 3.7)

`DefinitionPackBuilder::Build(json, validator)` runs the bounded JSON pre-pass,
schema validation and existing semantic validation before creating caller-owned
storage. `Pack()` borrows that storage until the next Build or destruction. Failed
builds expose an empty pack. Frames are sorted by identifier; names, sentinel
arrays and field spans remain stable. Unknown units name their string and pointer.
Frame lengths and offsets must fit the public uint8 representation. Signal keys
are zero-based field ordinals after sorting frames, not authored signal bindings.

The landed schema uses `signals`, permits empty signal arrays, and represents
signed fields with unsigned width types plus `signed: true`. The converter and
fixture follow that schema without modifying it. A field type still supplies the
width and the explicit signed flag supplies effective signedness.

`tools/telemetry-decode-jsonl.cpp` is the new package tools directory. Build the
`telemetry-decode-jsonl` target and invoke it with `--pack <path>` and JSON Lines
on standard input. It feeds every record through the real framer and decoder,
appends a four-byte tag delimiter for single-record confirmation, emits fields
on stdout and all parser counters as JSON on stderr. Values remain in declared
units. An empty frame still contributes to the frame counter.

The offline converter and differential driver import the owner's reference module
by an explicit absolute path without writing Python caches beside it. The converter
uses the fixed source label "the owner's schema XML (path withheld)". Conversion
dates honor SOURCE_DATE_EPOCH in UTC, falling back to today. Unit aliases include kPA to kPa. The reference module supplies no unit or
health metadata, so these are checked against the converted declarations rather
than claimed as independent reference evidence. Its decode-call and field counts
are measured by the driver. The differential report lists unconfirmed temperature
and pressure conventions without converting them.

Local chunk 05 proof used the permitted Visual Studio 17 2022 x64 generator and
MSVC 14.44. The pinned per-user CMake 4.4.3 was inaccessible to the sandbox account,
so the installed Visual Studio CMake 3.31.6-msvc6 was used. No schema, validator,
vendored library, persistent environment setting or CI file was changed.

The CMake build consumes the signal_core target through add_subdirectory, including
its source-list drift check and compile options. Standalone signal-core presets
build tools and tests by default; the nested library build leaves those targets off.
