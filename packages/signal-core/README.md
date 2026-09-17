# Signal-core build package

The engine-independent CMake and Ninja build and tests belong here (PLAN.md 2.x).
They compile the C++20 sources in runtime/UnRealDash/Source/SignalCore.
Duplicate sources, Unreal headers, runtime rendering, exceptions and RTTI do not belong here.


## Build and test

The root `third_party/doctest/doctest.h` is the only test dependency. No downloads
or package manager are used. The library and its consumers compile as C++20 with
exceptions and RTTI disabled. MSVC also needs `_HAS_EXCEPTIONS=0` so its standard
library does not instantiate exception handlers. Warnings are errors.

From the repository root in PowerShell 7:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -products * -latest -property installationPath
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" && cd packages\signal-core && cmake --preset default && cmake --build --preset default && ctest --preset default --output-on-failure"
packages\signal-core\build\default\signal-core-tests.exe --reporters=console
```

In a shell that already has a compiler environment, enter this directory and run
`cmake --preset default`, `cmake --build --preset default`, and
`ctest --preset default`. Linux additionally offers the `tsan` configure, build,
and test presets. The tsan preset is disabled on Windows. ThreadSanitizer and the
Unreal Engine 5.8.2 module build are not verified by a Windows CMake run.

The initial sandbox session could not execute the per-user WinGet CMake 4.4.3
and Ninja 1.13 binaries: access was denied. Its inherited PATH also contained a
stray double quote. The installed Visual Studio CMake 3.31.6-msvc6 and Ninja 1.12.1
were used with MSVC 14.44.35207 instead, with this process-local preparation:

```powershell
$vsTools = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake'
$env:PATH = "C:\Program Files (x86)\Microsoft Visual Studio\Installer;$vsTools\CMake\bin;$vsTools\Ninja;" + $env:PATH.Replace('"','')
```

This does not install tools or change persistent environment settings.

## Source and storage contracts

`runtime/UnRealDash/Source/SignalCore` is the single source copy. CMake lists every
`Private/*.cpp` explicitly and compares that list against the directory, rejecting
both missing and unlisted files. Add a source to that explicit list when adding
it to the module. `SignalCoreModule.cpp` is excluded and is the only engine include.
CTest filters use `--source-file=*test_name.cpp` because doctest records absolute
source paths on MSVC; the bare basename matches zero cases.

The acquisition thread owns the registry, rules, replay cursor and expiry schedule.
There is one snapshot reader. Construct exactly three disjoint, equally sized,
value-initialized snapshot buffers. Give their spans to `SnapshotExchange`, then
construct and initialize `SignalRegistry`. Declarations and backing spans must
outlive their users. There is no dynamic allocation in the library. Signal names
are NUL-terminated UTF-8 byte strings with at most 127 bytes, plus a terminator.
Rule compilation uses fixed storage for the specified 512-node and 32-depth limits.
Other capacities come from caller-provided spans. Rule IDs must be unique within
a shared expiry schedule. Do not copy an active registry, rule, replay or schedule.

Apply each sample, expire due freshness deadlines, evaluate rules, then
publish their latched flags with the registry. The exchange atomically transfers
the ready slot and its version. After publication the writer copies the published
samples into the returned writer slot before the next batch. This uses exactly
three registry buffers. The reader's snapshot remains unchanged until its next
successful acquire; repeated acquires without publication keep the same buffer.
The 64-bit packed publication word uses two index bits and 62 version bits. The
supported lifetime is fewer than 2^62 publications per exchange instance.

`SampleQueue` is a single-producer, single-consumer bounded queue with a caller-
supplied capacity. Full queues drop the incoming sample and increment their count.
The test executable replaces global scalar, array, aligned and nothrow `new`/`delete`
to count allocations and live bytes from all test and library translation units.
After worker startup and one warm-up publication/acquire, the sustained case checks
that both counters remain unchanged across 200,000 publications. Both workers stay
alive until the counters are captured, excluding thread teardown. These globals
exist only in the test executable. The library keeps no global mutable state.

Use the injected monotonic clock for every deadline. Arm one external timer to
`ExpirySchedule::Earliest()`. At that time call `registry.Expire()` and evaluate
all rules, then publish. Re-arm to the new earliest entry. There is no polling tick.
A generation change cancels expiries and resets registry state; each rule resets
when next evaluated with that generation. The clock and durations must fit signed
nanoseconds. The clock must not regress.

The registry normalizes incoming values to SI. Rule evaluation consumes only SI
registry samples, never `DisplayValue`. Public structs and spans carry data;
`Result::Get()` returns null on error, never a zero substitute.

## Rules and explicit choices

The operation whitelist is a table of arity, dimension behavior and evaluator.
Units are checked once at load. Dimension exponents compose four independent
quantity axes: temperature, pressure, speed and angular rate. All children are
quality-checked, including both boolean branches. Evaluation does not short-circuit
missing inputs or hide a non-finite intermediate result.

The fixed `ExpressionNode` interface cannot distinguish omitted `literal_unit{}`
from explicitly authored `dimensionless`: both have enum value zero. A caller
building nodes must set `kLiteralUnitOmitted` from `RuleEngine.h` for an omitted unit. Load
rejects that value. No rule JSON parser is included.

Each `RuleEngine` owns one compiled rule. Nonzero hysteresis requires an ordered
comparison at the root; other roots with a nonzero band are rejected. The band is
an SI delta, not an affine absolute temperature. For `greater`, assertion requires
`left > right + band`, and clearing occurs at `left <= right - band`; inclusive
operators preserve their equality semantics. Less-than comparisons mirror this.
Debounce applies to both directions and completes at the exact boundary.
`hold_last` starts when missing input is first evaluated, expires at its maximum
hold boundary, and never renews from repeated missing input. Recovery cancels it.
No previous result means unavailable during the hold. Schedule capacity failures
produce an invalid rule result. Current and latched are separate; acknowledgement
clears latched immediately and an asserted current relatches on the next evaluation.

## Recording v1 and replay

Output is JSON Lines with LF only. File adapters must open binary mode; the scenario
CLI uses `wb`. The library accepts an injected `TextSink`, and reads a caller-owned
text span into bounded caller-owned signal and sample spans. It does not own a
filesystem or allocate a recording-sized container.

The private reader accepts exactly the writer's key order and compact shape,
with a header, its signals array, and flat sample objects. That grammar has a
maximum depth of three without recursive parsing. It rejects extra keys, duplicate
keys, additional nesting, CRLF, missing final LF, malformed numbers, and unsupported
escapes. UTF-8 bytes pass through. Two-character JSON escapes are supported;
`\u` escapes are rejected. The writer rejects unsupported control bytes. Finite
doubles use shortest round-trip `to_chars`; missing or invalid NaN values use JSON
`null` and read back as quiet NaN. Infinity is rejected. NaN payload bits are not
part of the recording contract. Errors identify line number and zero-based absolute
byte offset. Capacity errors also name the line and offset.

Replay rebases receive timestamps to its injected start time, leaves optional
source timestamps intact, preserves gaps, and retains quality, age evidence,
sequence, source and generation. At the next sample time and all armed expiry
times, call `Replay::Step()` and evaluate rules after each true result before
stepping again. This preserves warning/latch transitions for same-timestamp samples.
A false result means no sample is due and freshness has been expired. `Poll()` drains
`Step()` for callers that do not need per-sample rule evaluation. EOF retains values until their individual deadlines. Looping requires an
explicit positive pause after the last sample; the first sample at the next wrap
advances the connection generation by one beyond the preceding generation. The
positive pause makes a one-sample loop finite. Seeking is not implemented.

The prescribed recording shape has no transport-event record. Consequently no
transport-disconnect event is invented or inferred from EOF, silence, or quality.
Recorded unavailable samples remain unavailable samples. Scenario transport events
are exposed through `ScenarioObserver`; a separate transport-event recording shape
would require a future format decision. The disconnect scenario advances generation
in that observer even when no further sample is emitted. Reconnect emits samples
with the advanced generation.

## Scenarios and chosen parameters

```text
signal-core-scenario <scenario-name> <seed> <duration-seconds> <output-path>
```

Names are `idle`, `acceleration`, `high_temperature`, `missing_signal`, `disconnect`,
`reconnect`, and `stale_heartbeat`. The only random source is `mt19937_64`, mapped
with `(x >> 11) * 0x1.0p-53`. Values use arithmetic and a piecewise-linear triangular
ramp. Floating-point contraction is disabled on GCC and Clang to avoid fused
multiply-add differences; MSVC uses its default precise floating-point mode.
Cross-platform byte identity still needs the later Linux run.

`SignalCore.Build.cs` requests `FPSemantics = FPSemanticsMode.Precise` at module
scope. This is the explicit non-fast floating-point policy selected for UBT; it
is not a verified guarantee that engine Clang disables contraction identically
to CMake's `-ffp-contract=off`. The UE 5.8.2 ModuleRules API and generated compiler
flags cannot be checked locally because Unreal is absent. The engine build's
floating-point policy remains unverified until PLAN.md 4.2's in-engine commandlet
compares scenario output against the CMake build.

The library requires duration, interval and deadline from the caller. The CLI
chooses a 50 ms interval and 500 ms deadline. Signal IDs are 1 (speed) and 2
(temperature), source ID is 1, initial generation is 0, and sequence starts at 1.
Temperature is marked age-unknown to represent a held channel. Scenario baselines
are 1 m/s and 293.15 K; uniform noise amplitudes are 0.1 m/s and 1 K. Acceleration
adds a 30 m/s triangular ramp; high temperature adds a 100 K ramp. Interruptions
start at one quarter of the requested duration, with reconnect at one half.
Generation advances on disconnect and again on reconnect. Heartbeats never emit
measurement samples or advance their timestamps.

Additional test parameters are explicit in the suite: two signal slots, one warning
slot, 16 expiry slots in the shared fixture, rule ID 7, 100 degC threshold, 2 K
hysteresis, debounce windows of 75 and 100 ms, and a 25 ms half-period for 20 Hz
oscillation over 100 observations. The queue overflow test publishes 1,000 items
into 256 slots before consumption. Registry snapshots additionally use 10 and 100
publication checks. Replay starts at 5 ms, uses recorded origins 5 or 10,005 ms,
50 ms or 1,000 ms loop pauses, and tests recorded generation 10 wrapping to 11.
Display fixtures use 100 ms sample spacing and 10 K steps, with a 1,000 ms window
only to prove clamping at a 500 ms stale boundary. Other rule fixtures use values
300, 400, and threshold/band boundaries offset by 0.01 K; invalid arithmetic uses
1e308 products and zero division. Unit round-trip rows are 0, +/-1e-9, -40, 100,
and 1,000,000. Byte formatting uses 64-byte double and 32-byte integer scratch
buffers, 128-byte unit strings, and 32-byte quality/age strings. These bounds do
not silently truncate input. Unit conversion factors are physical constants.

The debounce oscillation fixture uses zero hysteresis so it exercises debounce
itself; separate fixtures exercise the 2 K band. Arithmetic whitelist fixtures
use operands 1, 2, 3, 6 and 8 with their exact arithmetic or boolean results.
Other adversarial fixtures use -1e300, diagnostic value 999, zero/maximum integer
values, a -7 ns optional source timestamp, and wall-clock shifts of +/-1,000,000 s.
Boundary timestamps and sequences in tests are derived from these stated origins,
intervals, counts and durations, with a 1 ns before-boundary probe. Additional
bounds are the representable integer ranges, enum ranges, the two ring index bits,
and four dimensional exponent axes, rather than hidden scheduling defaults.

Registry ordering uses `SignalSample::received`, which is independent of quality,
travels with snapshot-buffer copies, and resets on a generation change. Regressing
sequence or receive time increments `RejectedOrdering()`, including after an
unavailable sample. Every finite value normalizes to SI regardless of quality;
missing NaN values remain NaN. The received marker is registry bookkeeping, not a
recording-v1 field. `DisplayValue::status` reports checked time-subtraction errors
with invalid quality and a NaN value. `Export.h` preserves UBT's `SIGNALCORE_API`
and defines it as empty only when the build system has not supplied it.


## Binary telemetry (tasks 2.9, 2.10, 2.11)

`DefinitionPack.h` owns plain frame and field definitions and validation. The
caller keeps names and spans alive and immutable. The decoder validates its pack
in the constructor; GetStatus returns the validation status and an invalid decoder
publishes no samples. JSON stays in
`dashboard-spec`. Supported integer widths are at most four bytes, matching the
landed schema. Definitions specify effective signedness and declared units.

`BinaryTelemetryV1Framer` starts in RESYNC. It requires a complete four-byte tag
at candidate + 16 to confirm a candidate, uses a 64-byte fixed carry array with
at most 20 bytes occupied, and scans complete caller-owned spans without copying.
Callbacks consume payload pointers synchronously. EndOfStream counts all retained
bytes as discarded and counts an enumerated candidate whose lookahead is incomplete.
Reset applies the same discard accounting, retains cumulative counters and clears
the stream offset and retained bytes. False synchronization is possible
when payload bytes themselves form a candidate and its confirmation tag.

The decoder uses the existing injected `Clock`, emits affine values in declared
units, rejects raw sentinels before conversion, and refreshes held receive timestamps.
`Sample.source` carries `FieldDefinition.signal` because the specified SampleSink
has no separate binding argument. The JSON builder assigns these keys from zero in
ascending frame order and field declaration order. A connector maps these keys to
its signal registry; a decoder-wide sequence increases for every field emission.
The generation setter resets sequence and health timestamps, retaining decoder
counters. ReconnectBinaryTelemetryV1 resets the framer and increments the decoder
generation together; generation overflow returns an error without changing either.

The specification describes health but omits access to decoder timestamps and raw
read notifications. `OnBytesReceived()` and `Health()` supply those two operations.
Call OnBytesReceived only for nonempty reads, including garbage. Unseen decoder
health timestamps start at Time::min so health is false before the first event.
The public TelemetryHealth predicates use checked subtraction and inclusive deadlines.

Status-role records publish their own fields according to declared acquisition and
refresh transport and status health. They never refresh acquisition health or any
telemetry-role signal. The converted pack marks status fields held, so their receive
timestamps refresh and their age evidence remains unknown while telemetry signals
expire during heartbeat-only traffic. TelemetryHealth is exported across module boundaries.

Sentinels explicitly publish NaN with unavailable quality. A non-finite affine
result publishes NaN with invalid quality and increments non_finite_results.
Both count as published samples when a sink receives them.

A tag and unknown identifier are classified from eight bytes and increment the
framer's unknown_identifier_dropped once at that position. RESYNC advances one byte
and includes it in bytes_discarded, preserving overlapping candidates. All skipped
bytes, including retained bytes discarded at termination or reset, count as discarded. The decoder
retains its separate counter for callers that pass unknown FrameEvents directly.
The locks counter counts each RESYNC-to-SYNCED confirmation. The fuzz false_locks
metric is max(locks - 1 - reconnects, 0) per input stream, summed across inputs.
The harness uses fresh framers and no reconnects; streams_with_lock supplies the
initial-lock baseline in the aggregate. The original generated-boundary metric is
retained as false_boundary_frames, which does not claim that recovery counts prove
incorrect frame boundaries. Reset retains cumulative framer counters across reconnects.

The fuzz executable accepts --inputs, --seconds, --seed and --corpus. A zero seconds
budget disables the time cap for the 10,000,000-input offline proof. Hex corpus
comments `# boundaries:` identify true generated frame starts; arbitrary tag bytes
are not treated as ground truth. New/delete instrumentation is thread-local and
limited to the executable, with counting enabled only during framer/decoder work.
No allocation instrumentation or mutable global state is in the library. Mutation
inputs are at most 192 bytes (64 garbage bytes plus eight records); random feed
chunks are 1 through 31 bytes. The seed is 1234 in the reported proofs.
