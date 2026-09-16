# Build chunk 02: signal-core

Status: frozen spec for a Codex build session. Covers PLAN.md tasks 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.13 and the source-layout half of 4.2. Read PLAN.md (those tasks, the WP2 preamble, the Approach preamble, the pin table, task 4.2, the Test strategy section, Key decisions 6, 7 and 8) and docs/ARCHITECTURE.md before writing anything. PLAN.md is the authority on what; this file adds the exact file layout, the exact flags, the test names and the proof commands. If the two disagree, PLAN.md wins and the disagreement goes in the report. Chunk 01 has already created the repository scaffold, the READMEs, `.gitattributes`, `.clang-format`, `.clang-tidy` and `scripts/`; do not recreate them.

## Goal

One set of plain C++20 sources that hold every signal semantic Stage 0 depends on, living at `runtime/UnRealDash/Source/SignalCore/` as an Unreal module that contains no Unreal headers, and compiled a second time by CMake and Ninja from `packages/signal-core/` with exceptions and RTTI off on a machine with no engine installed. The doctest suite is the pass/fail criterion for every task in this chunk: PLAN.md's gates are restated below as named test cases, and a task is not done until its named tests exist and pass.

## Dependency direction and the JSON decision

signal-core reads rule trees and recordings, but it must not depend on dashboard-spec's vendored JSON libraries, because dependencies point downward only (docs/ARCHITECTURE.md, layering). The split is fixed here so the two Codex sessions do not both implement JSON parsing:

- signal-core takes rule trees and signal declarations through plain C++ structs built by the caller. It never parses rule JSON. The JSON-to-struct conversion for rules is chunk 03's `RuleTreeBuilder` in `packages/dashboard-spec`, which depends on signal-core.
- The recording format in 2.7 is the one exception, and it is self-contained: a small JSON Lines reader and writer for exactly the shape the writer emits lives inside signal-core, under `Private/`, with no third-party JSON library and no public surface. It accepts flat objects and the header's `signals` array and nothing else.
- No RapidJSON, no valijson, no nlohmann, no miniz anywhere in this chunk.

## Deliverables

### 1. Source layout (task 4.2)

Runtime tree, the single copy of the sources:

```
runtime/UnRealDash/Source/SignalCore/SignalCore.Build.cs
runtime/UnRealDash/Source/SignalCore/SignalCoreModule.cpp
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Errors.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Clock.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Sample.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Units.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/SignalRegistry.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/SnapshotExchange.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/ExpirySchedule.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/RuleEngine.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Interpolator.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Recording.h
runtime/UnRealDash/Source/SignalCore/Public/SignalCore/Scenarios.h
runtime/UnRealDash/Source/SignalCore/Private/Units.cpp
runtime/UnRealDash/Source/SignalCore/Private/SignalRegistry.cpp
runtime/UnRealDash/Source/SignalCore/Private/SnapshotExchange.cpp
runtime/UnRealDash/Source/SignalCore/Private/ExpirySchedule.cpp
runtime/UnRealDash/Source/SignalCore/Private/RuleEngine.cpp
runtime/UnRealDash/Source/SignalCore/Private/Interpolator.cpp
runtime/UnRealDash/Source/SignalCore/Private/Recording.cpp
runtime/UnRealDash/Source/SignalCore/Private/JsonLinesText.h
runtime/UnRealDash/Source/SignalCore/Private/JsonLinesText.cpp
runtime/UnRealDash/Source/SignalCore/Private/Scenarios.cpp
runtime/UnRealDash/Source/SignalCore/Private/Errors.cpp
```

`Errors.h`, `Clock.h` and `ExpirySchedule.h` are additions this spec makes to the header list, because docs/ARCHITECTURE.md requires errors as values and an injected monotonic clock, and PLAN.md 2.5 requires an ordered set of armed expiries shared by freshness deadlines and rule timers. Report them as additions.

`SignalCoreModule.cpp` is the only file allowed to reference the engine. It contains the `IMPLEMENT_MODULE` boilerplate, nothing else, and opens with the comment `// Only file in SignalCore that may include an Unreal header. The CMake build in packages/signal-core excludes it.` Keep it under 20 lines.

`SignalCore.Build.cs` targets UE 5.8.2 per the pin table: `CppStandard = CppStandardVersion.Cpp20`, `bUseUnity = false`, `bEnableExceptions = false`, `PublicDependencyModuleNames` containing only `Core`, and `PublicIncludePaths` adding `Public`. No engine is installed on this machine, so this file cannot be compiled or verified in this session. Write it to the pin and record in the report that it is unverified.

CMake package, which compiles the files above by relative path:

```
packages/signal-core/CMakeLists.txt
packages/signal-core/CMakePresets.json
packages/signal-core/README.md
packages/signal-core/tools/scenario-dump.cpp
packages/signal-core/tests/main.cpp
packages/signal-core/tests/support/FakeClock.h
packages/signal-core/tests/support/TempPath.h
packages/signal-core/tests/test_sample.cpp
packages/signal-core/tests/test_units.cpp
packages/signal-core/tests/test_registry.cpp
packages/signal-core/tests/test_snapshot_exchange.cpp
packages/signal-core/tests/test_rule_engine.cpp
packages/signal-core/tests/test_rule_state.cpp
packages/signal-core/tests/test_interpolator.cpp
packages/signal-core/tests/test_recording.cpp
packages/signal-core/tests/test_scenarios.cpp
packages/signal-core/tests/test_threading_stress.cpp
```

doctest is already vendored at the repository root, `third_party/doctest/doctest.h` (v2.4.12, unmodified, with `LICENSE.txt` and `REVISION.md` beside it). Include it from there. Do not copy it, do not modify it, and do not vendor a second copy anywhere. No network access is available in this session.

`packages/signal-core/tools/scenario-dump.cpp` builds a second executable so the 2.8 determinism gate runs from the shell as PLAN.md writes it. Usage: `signal-core-scenario <scenario-name> <seed> <duration-seconds> <output-path>`. It is an addition to PLAN.md's file list; report it.

### 2. CMake build

`CMakeLists.txt` requirements, all mandatory:

- `cmake_minimum_required(VERSION 3.25)`, project languages `CXX` only, `set(CMAKE_CXX_STANDARD 20)`, `set(CMAKE_CXX_STANDARD_REQUIRED ON)`, `set(CMAKE_CXX_EXTENSIONS OFF)`.
- One `STATIC` library target `signal_core` built from an explicit list of the `Private/*.cpp` files above, referenced by relative path `${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/<file>.cpp`. No `file(GLOB)` for the build list.
- A file-list drift check: glob the `Private/` directory into a second variable, remove nothing, and `message(FATAL_ERROR ...)` naming any file present on disk and absent from the explicit list, or present in the list and absent on disk. `SignalCoreModule.cpp` sits in the module root and not in `Private/`, so it is never in either set. This is what keeps the two build systems' file lists in step, which Key decision 6 names as the cost of this arrangement.
- `target_include_directories(signal_core PUBLIC ../../runtime/UnRealDash/Source/SignalCore/Public)` and `PRIVATE ../../runtime/UnRealDash/Source/SignalCore/Private`.
- Exceptions and RTTI off. On GCC and Clang: `-fno-exceptions -fno-rtti`. On MSVC: `/EHs-c- /GR-`, and the default `/EHsc` and `/GR` must be **removed** from `CMAKE_CXX_FLAGS` with `string(REPLACE ...)` before appending, otherwise the compiler reports D9025 and keeps the default.
- Warnings as errors. GCC and Clang: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. MSVC: `/W4 /WX /permissive-`.
- `target_compile_definitions` on the test target: `DOCTEST_CONFIG_NO_EXCEPTIONS` and `DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS`. The second is required: with exceptions off, doctest removes the `REQUIRE` family unless that macro is also defined. Tests use `CHECK` by default and `REQUIRE` only where a failure would make the rest of the case meaningless.
- Test target `signal-core-tests` from `tests/*.cpp`, linking `signal_core` and `Threads::Threads` via `find_package(Threads REQUIRED)`.
- Executable `signal-core-scenario` from `tools/scenario-dump.cpp`.
- `enable_testing()` plus one `add_test` per doctest source file, using `--source-file=<name>` so a failing file is named in the ctest output rather than one opaque test.

`CMakePresets.json`, `"version": 6`, two configure presets:

- `default`: generator `Ninja`, `binaryDir` `${sourceDir}/build/default`, `CMAKE_BUILD_TYPE` `RelWithDebInfo`. A matching build preset and test preset, both named `default`, the test preset with `"output": {"outputOnFailure": true}`.
- `tsan`: generator `Ninja`, `binaryDir` `${sourceDir}/build/tsan`, `CMAKE_BUILD_TYPE` `Debug`, `CMAKE_CXX_FLAGS` adding `-fsanitize=thread -fno-omit-frame-pointer -O1`, the same flags in `CMAKE_EXE_LINKER_FLAGS`, and `"condition": {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux"}` so it is not offered on Windows. Matching build and test presets named `tsan`.

### 3. Numbers used, and where they come from

| Number | Value | Source |
| --- | --- | --- |
| Rule expression depth limit | 32 | PLAN.md 2.4, chosen default |
| Rule expression node limit, per rule | 512 | PLAN.md 2.4, chosen default |
| Unit round-trip tolerance | `abs(a-b) <= max(1e-5, 1e-6 * max(abs(a), abs(b)))` | PLAN.md 2.3, fixed |
| Freshness deadline used in tests | 500 ms, sample at 5 ms, expiry at 505 ms | PLAN.md 2.2 and 2.5, the worked case |
| Threshold oscillation rate in the debounce test | 20 Hz | PLAN.md 2.5 |
| Held-snapshot publications | 1000 | PLAN.md 2.13 |
| Idle-writer consecutive acquires | 1000 | PLAN.md 2.13 |
| Recording round-trip scenario length | 60 seconds | PLAN.md 2.7 |
| Replay gap length | 3 seconds | PLAN.md 2.7 |
| Bounded queue capacity | 256 entries | chosen default for this spec, not in PLAN.md |
| Sustained-publication iterations | 200,000 | chosen default for this spec, not in PLAN.md |
| Generation-change scenario in-flight samples | 64 | chosen default for this spec, not in PLAN.md |
| `hold_last` maximum hold used in tests | 2000 ms | chosen test value; PLAN.md 2.5 requires the field, not a value |
| Interpolator maximum extrapolation window in tests | 100 ms | chosen test value; PLAN.md 2.6 requires the field, not a value |

Every number this spec chose is a test parameter, not a library default. The library requires the caller to supply it.

### 4. Interface published to chunk 03

Chunk 03 compiles against these declarations. Do not rename them. They appear verbatim in `docs/build/chunk-03-dashboard-spec.md` so the two sessions agree without talking.

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
  double literal_value{};             // literal only
  Unit literal_unit{};                // literal only, never inferred
  std::uint32_t signal{};             // signal_reference only, a SignalId value
  std::uint16_t first_child{};        // index into the node span
  std::uint8_t child_count{};
  const char* source_pointer{};       // caller-owned JSON pointer, may be null
};

struct RuleDefinition {
  std::uint32_t rule_id{};
  std::span<const ExpressionNode> nodes{};   // root at index 0
  double hysteresis_band{};
  Unit hysteresis_unit{};
  std::chrono::nanoseconds debounce{};
  MissingInputPolicy missing_input_policy{};
  std::chrono::nanoseconds maximum_hold{};   // required when policy is hold_last
};

}  // namespace signal_core
```

The node span is caller-owned storage, so no standard-library container crosses the boundary. `source_pointer` is how a load-time error reports the offending node path with the same JSON pointer the Python validator reports; PLAN.md 2.4 requires the node path and does not say how it arrives, so this spec fixes it here. `ParseUnit(std::string_view) -> Result<Unit>` and `QuantityOf(Unit) -> Quantity` are public in `Units.h` and are what chunk 03 uses for unit-compatibility checks at load.

Naming convention for the whole chunk: types and functions in PascalCase, struct data members and enumerators in snake_case matching the JSON vocabulary, so a document field name and its C++ enumerator read the same.

### 5. Errors as values

`Errors.h` declares `enum class ErrorCode : std::uint16_t` with `none = 0` first and a stable value per case, and `struct Status { ErrorCode code; char message[256]; bool Ok() const; }` plus `template <class T> class Result`. Messages name the offending input: signal name, unit string, node pointer, file line and byte offset. Nothing in this chunk throws, allocates unboundedly, or writes to a global. `std::filesystem` is used only through its `std::error_code` overloads.

### 6. Semantics and their tests, task by task

Test case names below are literal. Use them.

**2.1 Sample and quality types.** `Sample` carries value, unit, source identity, monotonic sequence, monotonic receive timestamp, optional source timestamp, `Quality` and `AgeEvidence`. Receive time is always receive time: a `held` field's receive timestamp updates on every frame exactly as a `live` field's does.
Tests in `test_sample.cpp`: `2.1 default sample is unavailable`; `2.1 zero is never a missing-data stand-in` (a sample whose value is 0.0 and quality `valid` is distinguishable from an `unavailable` sample, and no API returns 0.0 for a missing value); `2.1 quality and age evidence are independent` (all four combinations of `{valid, stale}` with `{measured, unknown}` constructed, each representable and distinguishable). Plus, in the same translation unit and not inside a test case, `static_assert(!std::is_constructible_v<Sample, Value, Unit>)`, which proves at compile time that a sample cannot be built without a receive timestamp.

**2.2 Registry, freshness, generations, triple buffer.** Single writer. Readers take a whole-registry snapshot. Per-signal deadline against an injected monotonic clock. Samples carry a connection generation; a sample older than the current generation is rejected and counted.
The exchange is a bounded triple buffer with an atomic publication version that increments by one per publication. The reader swaps with the ready slot **only when the ready slot's version is newer than the version of the buffer the reader already holds**, otherwise it keeps its buffer and reports no new publication. The writer never recycles the buffer the reader holds. An unconsumed publication is overwritten by design.
Tests in `test_registry.cpp`: `2.2 deadline expiry marks stale with no new sample` (fake clock advanced past 505 ms); `2.2 wall clock jump does not change freshness` (the same case with the wall clock moved forwards and backwards, outcome unchanged); `2.2 older generation sample is rejected and counted`; `2.2 snapshot never mixes publications`; `2.2 held snapshot survives 1000 publications` (one field read, snapshot held, 1000 publications, every field unchanged afterwards, writer blocked count zero).
Tests in `test_snapshot_exchange.cpp`: `2.2 publication version increments by one`; `2.2 reader keeps its buffer when no newer version is ready`; `2.2 reader never receives a buffer it deposited`; `2.2 unconsumed publication is overwritten`.

**2.3 Unit normalization.** SI internally: kelvin, pascal, metres per second, radians per second. Declared source units convert at the connector edge; display units apply at render time only.
Tests in `test_units.cpp`: `2.3 round trip within tolerance` is table-driven over degC, degF and K; kPa, bar and psi; km/h, mph and m/s; rpm and rad/s. The table includes zero and near-zero rows in every affine family, because the relative term degenerates there, and the tolerance is the absolute floor plus relative term above. `2.3 unknown unit string is rejected` asserts `ParseUnit("furlongs")` returns `ErrorCode::unknown_unit` with the offending string in the message rather than passing the value through. `2.3 quantity classification` asserts each unit maps to the right `Quantity`.

**2.4 Rule engine over an expression tree.** Whitelisted operations only, the `Operation` enum above and nothing else. Depth limit 32, 512 nodes per rule, both load-time checks. No string parsing anywhere. Every literal carries a unit and a dimensionless literal says so explicitly. Comparison and additive arithmetic require unit-compatible operands; multiplicative arithmetic composes units. Compatibility is checked once at load against the registry's declared units, never per evaluation. Literals convert to SI at load through the 2.3 table. Any non-finite result, including division by zero and overflow, yields `Quality::invalid` rather than a number. Missing input is `{stale, unavailable, invalid}` and routes to 2.5's policy; `AgeEvidence::unknown` is not a missing input, the rule evaluates the value and the result inherits an `age_unknown` flag.
Tests in `test_rule_engine.cpp`: `2.4 unknown operation is a load error naming the node pointer`; `2.4 depth 33 is rejected with the node pointer`; `2.4 node count 513 is rejected with the node pointer`; `2.4 pressure literal against temperature signal is a load error naming both units`; `2.4 degC threshold evaluates at the correct kelvin point`; `2.4 division by zero yields invalid`; `2.4 overflowing product yields invalid`; `2.4 stale input routes to missing input policy`; `2.4 unavailable input routes to missing input policy`; `2.4 invalid input routes to missing input policy`; `2.4 age unknown input evaluates and sets age_unknown`; `2.4 reference to a missing signal returns the declared missing input result`; `2.4 multiplicative arithmetic composes units`; `2.4 literal without an explicit unit is a load error`.

**2.5 Hysteresis, debounce, missing-input policy, armed expiries, latching.** Fixed evaluation order, documented in the header: quality check, missing-input policy, evaluate, hysteresis, debounce. `hold_last` carries a mandatory maximum hold duration; past it the result is `unavailable` whatever the policy says. Hysteresis state, debounce state and any `hold_last` timer reset on a connection generation change.
Rules are driven by time as well as arrivals. `ExpirySchedule` holds an ordered set of armed expiry times, one entry per armed freshness deadline, `hold_last` expiry and debounce window, and exposes the earliest entry so the caller arms a single timer against the same injected clock. There is no fixed tick; the withdrawn 10 Hz tick must not reappear.
`current` and `latched` are separate. `current` follows the inputs down as well as up. `latched` sets when `current` first asserts and clears only through `Acknowledge(rule_id)` or a generation change, never because the condition went away.
Tests in `test_rule_state.cpp`: `2.5 oscillation at 20 Hz changes output at most twice`; `2.5 treat_unavailable reports unavailable not false`; `2.5 hold_last flips to unavailable at its maximum hold and not before` (driven by the armed expiry with no sample arriving); `2.5 invalid input triggers the policy not arithmetic`; `2.5 generation change clears hysteresis and debounce state`; `2.5 current clears while latched stays set`; `2.5 acknowledge clears latched`; `2.5 acknowledge while still asserting relatches on the next evaluation`; `2.5 expiry schedule arms the earliest entry`; `2.5 expiry schedule re-arms on a new sample before the deadline`; `2.5 expiry cancelled by a generation change yields no firing`. Boundary cases: `2.5 value exactly on the threshold`, `2.5 value exactly on each hysteresis band edge`, `2.5 transition exactly at the debounce boundary`. Every time-driven case uses deadlines offset from frame and scheduler boundaries, the 500 ms deadline on a sample received at 5 ms expiring at 505 ms being the worked case, so no test can pass because a periodic tick happened to land on the deadline.

**2.6 Display-only bounded interpolation.** Per-signal maximum extrapolation window, disabled by construction for discrete signals, no path into rule evaluation.
Tests in `test_interpolator.cpp`: `2.6 rule evaluates the raw sample while the display value differs`; `2.6 interpolation stops at the window bound`; `2.6 interpolation does not cross into the stale period`; `2.6 discrete signal configured for interpolation is a configuration error`.

**2.7 Record and replay, format version 1.** JSON Lines, LF only, files opened in binary mode so no CRLF translation happens on Windows. One header line then one sample line each. Header: `{"format":"unrealdash-recording","version":1,"signals":[...]}` where each signals entry carries id, name, unit, deadline in nanoseconds and the discrete flag. Sample line keys in this fixed order, with `t_source_ns` omitted when absent: `t_recv_ns`, `signal`, `value`, `unit`, `source`, `quality`, `age_evidence`, `seq`, `t_source_ns`, `generation`. Doubles are written with `std::to_chars` shortest round-trip form, which is unique and therefore identical across conforming implementations; integers are written plain. The full 2.1 `Sample` round-trips and nothing affecting freshness, ordering, age evidence or reset semantics is dropped.
The private reader accepts only that shape, with a depth limit of 3, and rejects anything else with `ErrorCode::recording_malformed_line` naming the line number and byte offset. Strings pass UTF-8 bytes through and support the two-character escapes; a `\u` escape is rejected. This restriction is this spec's choice and is recorded in `packages/signal-core/README.md`.
Replay: rebase recorded monotonic timestamps onto the injected clock so the first sample plays at t=0; preserve inter-sample gaps at their recorded durations; at end of file hold the last sample and let each signal go stale at its own deadline, emitting no disconnect unless one was recorded; an explicit loop flag restarts the file and increments the connection generation so rule, debounce and `hold_last` state reset as on a real reconnect. Seek is out of scope.
Tests in `test_recording.cpp`: `2.7 sixty second round trip is byte identical` (write, read, write again, compare bytes, `age_evidence` included, a held field recorded as unknown replaying as unknown); `2.7 three second gap produces stale at the deadline and valid on the next sample` (transition times checked against the injected clock); `2.7 end of file holds then goes stale at the deadline`; `2.7 loop flag resets hysteresis debounce and hold_last at the wrap`; `2.7 malformed line is rejected with line and byte offset`; `2.7 unknown format version is rejected`.

**2.8 Deterministic scenario generator.** Seven scenarios: `idle`, `acceleration`, `high_temperature`, `missing_signal`, `disconnect`, `reconnect`, `stale_heartbeat`. Each seeded and reproducible.
Determinism constraints this spec fixes, because PLAN.md requires reproducibility and does not say how: the only random source is `std::mt19937_64`, converted to a double with `(x >> 11) * 0x1.0p-53` rather than `std::uniform_real_distribution`, whose output is not portable. Scenario value generation uses only addition, subtraction, multiplication, division and table-driven piecewise-linear ramps; no `sin`, `cos`, `exp` or `pow`, because libm results differ by a low-order bit between platforms and would break a cross-platform comparison. Output is therefore identical on Windows and Linux as well as run to run.
Tests in `test_scenarios.cpp`: `2.8 each scenario is identical across two runs with the same seed` (all seven, compared byte for byte in memory); `2.8 different seeds produce different output`; `2.8 disconnect scenario increments the generation`; `2.8 stale_heartbeat scenario leaves transport connected while signals expire`. The shell-level gate uses `signal-core-scenario` and `fc /b`, in Proof below.

**2.13 Acquisition threading stress.** Deterministic seed and a fixed iteration count, never a wall-clock duration, so a slow hosted runner reproduces the same run. Real writer thread, real reader thread, against the 2.2 registry. Five scenarios, all in `test_threading_stress.cpp`:
`2.13 sustained publication with a continuously swapping reader` asserts no growth in resident allocation over 200,000 publications, measured by a counting allocator wrapper in the test rather than by an operating-system query.
`2.13 stalled reader holds a snapshot across 1000 publications` asserts the snapshot is bit-identical before and after and the writer's blocked count is zero.
`2.13 repeated acquires with the writer idle` performs 1000 consecutive acquires with no publication in between and asserts the observed publication version never decreases, that no acquire after the first reports a new publication, and that no value and no latched warning state alternates across the run.
`2.13 generation change mid stream rejects in-flight older samples` asserts none of the 64 in-flight older-generation samples is applied and each is counted.
`2.13 bounded queue above capacity drops and counts` asserts depth never exceeds 256 and the drop count equals publications minus consumptions.
Every case prints nothing itself; the assertion count comes from the doctest summary.

## Constraints

- Do not run `git commit`, `git push`, `git add`, or change git configuration. Claude commits.
- Do not modify PLAN.md, PLAN-REVIEW-LOG.md, or anything under `docs/`. The only README this chunk may write is `packages/signal-core/README.md`. Chunk 01 already created that file with a statement of what belongs in the directory; extend it with the build instructions, the MSVC environment steps, the recording-format restrictions and the file-list discipline, and do not delete what chunk 01 wrote.
- Nothing in this chunk may require elevation. If a step needs it, stop and report it.
- Plain factual language in every file and every string a user can see. No em dashes anywhere. No marketing words. Do not name any commercial dashboard product.
- Follow docs/ARCHITECTURE.md: one public header per concept, nothing in `Private/` included from outside the module, no global mutable state, no singletons, injected clock, errors as values, plain structs and spans across boundaries.
- Environment pins: `pwsh` 7.6, Python 3.14 locally and 3.12 in CI, CMake 4.4.3 installed per-user, Ninja 1.13, MSVC 14.44 from Build Tools 2022 at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`. No Unreal Engine is installed on this machine, so nothing in this chunk may depend on it at build or test time.
- **This chunk owns `runtime/UnRealDash/Source/SignalCore/` and `packages/signal-core/` and touches nothing else.** Chunk 03 owns `packages/dashboard-spec/`, `tools/`, `tests/fixtures/documents/`, `tests/fixtures/packages/` and `scripts/validate-parity.ps1`. Chunk 03 runs after this chunk has been merged, so a write outside this chunk's directories is a merge conflict. Do not create `packages/dashboard-spec/`, do not add to `tools/`, do not edit anything under `scripts/`, and do not create `.github/` workflows; those are other chunks.
- No third-party code beyond the doctest header already vendored at `third_party/doctest/`. No package manager, no `FetchContent`, no network access at build or test time (this session runs in a sandbox with no network).

## Non-goals

Tasks 2.9, 2.10, 2.11 and 2.12: the binary-telemetry-v1 parser, heartbeat and held-value traffic classification, the malformed-input fuzz harness and the differential check against the reference decoder. They are a later chunk and they depend on chunk 03's definition-pack schema. Do not write a frame parser, a resynchronization state machine, a definition-pack loader, or `tests/fixtures/packs/`. Also out: the Unreal module build, the in-engine smoke commandlet, the CI workflow files, and any rule-JSON parsing.

## Proof (run all of these and paste full output)

Enter the MSVC environment first. Either open "Developer PowerShell for VS 2022", or locate the toolchain and run the build inside one `cmd` invocation so the environment survives:

```
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -products * -latest -property installationPath
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" && cd packages\signal-core && cmake --preset default && cmake --build --preset default && ctest --preset default --output-on-failure"
```

Then, from the repository root in `pwsh`:

```
# assertion count must be non-zero and failures zero
packages\signal-core\build\default\signal-core-tests.exe --reporters=console | Select-String "assertions:"

# no Unreal header outside the module registration file
$files = Get-ChildItem -Recurse runtime\UnRealDash\Source\SignalCore -Include *.h,*.cpp |
  Where-Object { $_.Name -ne 'SignalCoreModule.cpp' }
$hits = $files | Select-String -Pattern '^\s*#\s*include\s*[<"](Core|CoreMinimal|CoreTypes|Engine|HAL|Modules|UObject|Containers|Templates|Misc|Math|Logging|GenericPlatform|Windows)'
$hits; if ($hits) { "FAIL: unreal include outside SignalCoreModule.cpp"; exit 1 } else { "PASS: no unreal includes" }
Select-String -Path runtime\UnRealDash\Source\SignalCore\SignalCoreModule.cpp -Pattern 'IMPLEMENT_MODULE'

# 2.8 determinism, one command per scenario
foreach ($s in 'idle','acceleration','high_temperature','missing_signal','disconnect','reconnect','stale_heartbeat') {
  packages\signal-core\build\default\signal-core-scenario.exe $s 1234 60 "$env:TEMP\$s.a.jsonl"
  packages\signal-core\build\default\signal-core-scenario.exe $s 1234 60 "$env:TEMP\$s.b.jsonl"
  cmd /c "fc /b %TEMP%\$s.a.jsonl %TEMP%\$s.b.jsonl"
  "$s exit=$LASTEXITCODE"
}
```

Expected: the CMake configure reports the compiler, C++20, and exceptions and RTTI off; the build produces zero warnings, since warnings are errors; `ctest` reports every test passing; the assertion line shows a non-zero count with zero failures; the include scan prints `PASS: no unreal includes`; `IMPLEMENT_MODULE` is found in `SignalCoreModule.cpp` and nowhere else; each of the seven `fc /b` runs reports no differences and exits 0. The 2.7 byte-identical round trip and the 2.13 assertions are inside the ctest run; name them in the report with their assertion counts.

The `tsan` preset cannot run here, because it is Linux only. Report it as not run rather than as passing. Its consumer is the ThreadSanitizer job in PLAN.md 1.4, which is a later chunk.

## Report format

End with: files added or changed, one line each giving path, what it is, and the PLAN.md task it serves; the proof output verbatim; the doctest assertion count and failure count; every place this spec or PLAN.md was ambiguous and what you chose; every number you chose that this spec did not give you; any deviation from PLAN.md or this spec with the reason; and anything you could not do, including anything blocked by the absent Unreal install.
