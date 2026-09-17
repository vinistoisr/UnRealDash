# Build chunk 08: connector interface and acquisition threading

Status: revision 5. Two build attempts stopped on bad prerequisites in this spec, both correctly. Revision 3 claimed three allocation assertions where 2.13 has two, and prohibited two assertions living inside scenarios it also told the builder not to recount. Revision 4 then published a per-scenario table built by grepping source lines, which undercounted three of the five scenarios because doctest counts executed assertions. The table is now taken from the doctest reporter. Revision 1 drew 22 review findings, revision 2 drew a further five including one blocker. Both rounds are resolved here. Frozen for a Codex build session. Covers PLAN.md task 4.3. Read PLAN.md (task 4.3, task 2.13, task 2.5, task 2.2, task 4.2's Win64 half, task 4.8's event-log section, the Sequencing block) and docs/ARCHITECTURE.md and docs/CONNECTORS.md before writing anything. PLAN.md is the authority on what; this file adds the exact mechanisms, proof commands and implementation constraints. If the two disagree, PLAN.md wins and the disagreement goes in the report.

## Goal

Put a real acquisition thread behind the registry, and make the game thread a reader that never blocks on it. Everything in 2.13 already proves the primitives work under contention. This chunk builds the object that uses them in the order the plan requires: rules evaluate on the acquisition side at sample rate, before any dropping for display, so a warning that exists for a single sample at 200 Hz cannot be lost by a renderer that only looks every 16 ms.

The second half of the chunk is the in-engine rerun of 2.13. The stress scenarios are not rewritten against Unreal; they are lifted into one header that both runners drive, the same way `SignalCore/SmokeSubset.h` is already driven by both the CMake test binary and the smoke commandlet.

No device is involved. Everything here runs on Windows and on the Linux CI runner.

## Facts this chunk depends on

Read out of the tree, not assumed. If one is wrong, stop and report it rather than inventing a replacement.

- `SnapshotExchange` is a three-buffer single-writer single-reader exchange with `Publish()` and `Acquire()`; `Acquire()` returns a `Snapshot` carrying `version`, `generation` and `new_publication`. Do not change this class.
- **`SnapshotExchange::WriterBlockedCount()` is literally `return 0;` in the header.** Asserting it is zero cannot fail and is not evidence of anything. Do not put it in a test. It is a structural guarantee, not a measurement.
- `SampleQueue` is a single-producer single-consumer bounded queue with a drop-newest policy and a `Drops()` counter, over caller-provided storage. Do not change it and do not add a second queue type.
- `SignalRegistry` owns `Apply`, `Expire`, `SetGeneration`, `Publish`, `RejectedOrdering()` and `RejectedGenerations()`. Its header states the acquisition thread owns every method except the reader's `Acquire`. `SignalSample::received` is registry bookkeeping, reset on a generation change.
- `RuleEngine::Evaluate(span<const SignalSample>, generation)` returns a `RuleResult` and `Current()` returns the last one. **It has no transition flag, no public accessor for its own `rule_id`, and no way to clear a latch other than `Acknowledge(rule_id)`.** Deliverable 0 fixes the last two.
- `ExpirySchedule` is a caller-storage ordered set with `Arm`, `Cancel`, `Earliest()` and `PopDue(now, expiry)`.
- `Clock` is an injected `{void* context, Time (*now)(void*)}` pair. Nothing in signal-core reads a wall clock, and nothing in this chunk may either.
- `packages/signal-core/tests/test_threading_stress.cpp` holds six `TEST_CASE`s. Five are 2.13 scenarios; `review 7 heap counter observes scalar array aligned and nothrow allocations` is a self-test of the test harness's own heap counter and is **not** a 2.13 scenario.
- The assertion counts below were produced by **running doctest**, not by grepping the source. That distinction matters and an earlier revision of this spec got it wrong: doctest reports assertions *executed*, which counts `REQUIRE` as well as `CHECK` and counts an assertion inside a loop once per iteration. Source-line counting undercounted three of the five scenarios. The gate compares runtime counts, so the spec quotes runtime counts.

  ```
  packages/signal-core/build/default/signal-core-tests.exe "--source-file=*test_threading_stress.cpp" --reporters=xml
  ```

  | scenario | assertions today | contains |
  | --- | --- | --- |
  | sustained publication with a continuously swapping reader | 5 | 2 allocation (lines 79, 80), 1 `WriterBlockedCount` (line 81) |
  | stalled reader holds a snapshot across 1000 publications | 5 | 1 `WriterBlockedCount` (line 131) |
  | repeated acquires with the writer idle | 2 | |
  | generation change mid stream rejects in-flight older samples | 4 | |
  | bounded queue above capacity drops and counts | 4 | |
  | *(self-test, not a 2.13 scenario, not moved)* | 4 | 3 allocation (lines 97 to 99) |

  Whole file today: 6 cases, 24 assertions. The five 2.13 scenarios account for 20.

  So 2.13 holds **two** allocation assertions, not three; the three at lines 97 to 99 belong to the self-test. And the two `WriterBlockedCount` assertions sit **inside** 2.13 scenarios, so removing them necessarily changes those scenarios' counts.
- The CMake presets in `packages/signal-core/CMakePresets.json` are named **`default`** and **`tsan`**, both Ninja, and the presets file lives in `packages/signal-core`, so preset commands run from that directory. There is no `windows-msvc` preset; an earlier revision of this spec told the builder to use one. `tsan` is the Linux CI preset.
- `test_heap::Read()` in `tests/support/HeapCounter.h` replaces global `operator new`. Unreal replaces global `operator new` in `ModuleBoilerplate.h`, so that counter cannot link into an engine module.
- `UnRealDashCore.Build.cs` lists `SignalCore` in `PublicDependencyModuleNames`, so the game module inherits signal-core's include path and link and **can** call signal-core directly. The linker does not enforce the layering rule, and the game module already breaks it.

## Deliverable 0: three small additions to existing classes

These are prerequisites the review found missing. Make them first; they are small and everything else depends on them.

1. `RuleEngine::RuleId()` returning `definition_.rule_id`. `Reconnect` cannot clear latches without it, because `std::span<RuleEngine>` exposes no ids.
2. `RuleEngine::ClearLatch()`, which does what `Acknowledge` does without needing the caller to know the id. `Acknowledge(rule_id)` keeps its current behaviour and is implemented in terms of it.

Each gets a doctest case. Neither changes existing behaviour.

Criterion 3's evaluation identity is served by `AcquisitionPipeline::RuleEvaluations()`, which counts passes. Do **not** add a counter to `RuleEngine` for it; a per-rule counter there would make the identity depend on the rule count, which is the ambiguity this revision removed.

## Deliverables

### 1. The connector interface (`SignalCore`, new `Public/SignalCore/Connector.h` + `Private/Connector.cpp`)

Four separate objects with separate lifetimes, per docs/CONNECTORS.md. Do not collapse them, and do not let one layer's header name another layer's concrete type.

Every method is listed here. Do not invent additional ones; if you believe one is missing, stop and report it.

```cpp
enum class Pending : std::uint8_t { produced, need_more_data, done };

struct ITransport {
    virtual ~ITransport() = default;
    // Non-blocking. Fills as much of `into` as is available right now and sets `written`.
    // Returns need_more_data with written == 0 when nothing is available. Never waits.
    virtual Status Read(std::span<std::uint8_t> into, std::size_t &written) = 0;
    virtual Status Connect() = 0;
    virtual Status Disconnect() = 0;
    virtual bool Connected() const = 0;
};

struct Message { std::span<const std::uint8_t> bytes; };

struct ISession {
    virtual ~ISession() = default;
    // Takes ownership of `input` for the duration of the call, appends to internal framing
    // state, and reports how many bytes it consumed. Call Next() until it returns need_more_data.
    virtual Status Offer(std::span<const std::uint8_t> input, std::size_t &consumed) = 0;
    virtual Pending Next(Message &out) = 0;
    virtual void Reset() = 0;   // called on every generation change
};

struct DecodedField { SignalId signal; Sample sample; };

struct IDecoder {
    virtual ~IDecoder() = default;
    // One message may decode to zero, one or many fields. Call Next() until need_more_data.
    virtual Status Offer(const Message &message) = 0;
    virtual Pending Next(DecodedField &out) = 0;
    virtual void Reset() = 0;
};

struct IMapping {
    virtual ~IMapping() = default;
    // Normalizes units and selects the source. Returns done to drop the field deliberately,
    // which is a mapping decision and is not a display drop and is not an error.
    virtual Pending Map(const DecodedField &in, DecodedField &out) = 0;
};
```

`Reset()` on the session and the decoder is what makes "discard anything in flight" mean something concrete: it throws away partial framing and partial decode state at a generation change. It does not reach into the registry.

`ConnectionHealth` is a plain struct: `std::uint64_t generation`, `bool connected`, `Time last_byte_at`, `std::uint64_t reconnects`, `std::uint64_t bytes`, `char last_error[128]`.

**Connection generation.** The pipeline owns the counter, not the transport. On every successful (re)connect the pipeline increments it and calls `SignalRegistry::SetGeneration`.

### 2. `AcquisitionPipeline` (`SignalCore`, new `Public/SignalCore/Acquisition.h` + `Private/Acquisition.cpp`)

**It contains no thread.** It is a state machine the host drives. That is what lets the CMake stress harness drive it from `std::thread` and the engine drive it from `FRunnableThread` without two implementations of the contract existing.

**Construction** takes, by reference or span, all of: `Clock`, `SignalRegistry&`, `ExpirySchedule&`, `std::span<RuleEngine> rules`, `std::span<RuleResult> previous_results`, `std::span<std::uint8_t> read_buffer`, `SampleQueue& display`, `ITransport&`, `ISession&`, `IDecoder&`, `IMapping&`, `IAcquisitionEventSink&`.

`previous_results` must be the same length as `rules`; the constructor rejects a mismatch. It holds each rule's `current`/`latched` from the previous evaluation, which is how a transition is detected. It is caller-provided because the pipeline allocates nothing.

`read_buffer` is the scratch space for `ITransport::Read`. Caller-provided, for the same reason.

**The display queue is sized by the host**, because `SampleQueue` already takes caller storage and the pipeline cannot see the bound. The engine host sizes it at four times the expected samples-per-frame, minimum 64. The tests size it deliberately, including at capacity 1.

**Public surface:**

```cpp
Status Start();
Status Stop();                       // idempotent
Status Reconnect();
PumpResult Pump(Time deadline);      // deadline is an ABSOLUTE instant in the injected clock
const ConnectionHealth& Health() const;
Status Acknowledge(std::uint32_t rule_id);
std::uint64_t PublishedCount() const;
std::uint64_t DisplayDrops() const;
std::uint64_t MappingDrops() const;
std::uint64_t SamplesApplied() const;
std::uint64_t RuleEvaluations() const;
std::uint64_t RuleTransitions() const;
```

```cpp
struct PumpResult {           // every field is a DELTA for this call, not a running total
    std::uint32_t bytes_read;
    std::uint32_t samples_applied;
    std::uint32_t rule_evaluations;
    std::uint32_t transitions;
    std::uint32_t display_pushed;
    std::uint32_t display_dropped;
    std::uint32_t mapping_drops;
    std::uint32_t expiries_fired;
    Status status;
};
```

The accessors are cumulative totals. `PumpResult` is per call. Never conflate them.

**`deadline` is an absolute instant** in the injected clock's domain, not a duration. Two distinct clock uses, and conflating them is a defect:

- **Timestamping.** `Pump` reads the clock once at the top into `now` and uses that single value for every timestamp it records and for `PopDue`, so one `Pump` is one logical instant.
- **The deadline test.** `Pump` calls `Clock::Now()` again at the top of each loop iteration purely to compare against `deadline`. This is the only place a second read is allowed, and it is why `Pump` cannot overrun by more than one iteration.

`Pump` loops over steps 1 and 2 until the transport reports zero bytes or the live clock passes `deadline`, then runs steps 3 and 4 exactly once.

**The ordering inside `Pump` is the contract and is not negotiable:**

1. `ITransport::Read` into `read_buffer`. If `written > 0`, set `last_byte_at` to `now`. **On a zero-byte read `last_byte_at` is left unchanged**, because no byte arrived.
2. `ISession::Offer` the bytes, then `ISession::Next` until `need_more_data`. For each message, `IDecoder::Offer` then `IDecoder::Next` until `need_more_data`. For each decoded field, `IMapping::Map`; a `done` from `Map` drops the field deliberately, increments `MappingDrops()`, and is **not** a display drop. Then, for each surviving field, in this exact order and before the next field is touched:

   a. `SignalRegistry::Apply`.
   b. `SignalRegistry::Expire()`.
   c. `RuleEngine::Evaluate` on **every** rule against `registry.Samples()`. Compare each against `previous_results[i]`; if `current` or `latched` differs, that is a transition: count it, hand it to `IAcquisitionEventSink::OnRuleTransition`, and store the new result. Increment `RuleEvaluations()` **once per pass, not once per rule**, so the identity in criterion 3 holds for any rule count.
   d. Only now, push this sample into the display `SampleQueue`. The drop-newest policy applies **here and nowhere else**. Every drop increments `DisplayDrops()`.

   Sub-steps c and d in this order, per sample, are the whole point of the task. Evaluating rules for a sample that is about to be dropped for display is what stops a 200 Hz warning from being lost by a 60 Hz renderer. Doing this per sample rather than in two separate passes is deliberate: a deferred push would need a staging buffer the pipeline is not allowed to allocate, and the per-sample form makes the guarantee local and auditable.
3. Drain the acknowledge slot, then drain `ExpirySchedule::PopDue(now)`; for each due expiry, `SignalRegistry::Expire()` and re-evaluate the rules exactly as in 2c. This step runs even when step 1 read zero bytes, which is what lets a warning be raised during silence and a `hold_last` timer expire with no traffic at all. Expiry-driven evaluations increment `RuleEvaluations()` and so are excluded from criterion 3's identity; the test uses a scenario with no armed expiry firing, and says so.
4. `SignalRegistry::Publish()`.

**The pipeline never blocks on the reader.** No path from `Pump` waits on the game thread, no mutex is held across a transport read, and `Pump` allocates nothing.

**Cross-thread acknowledge.** `Acknowledge` arrives from the game thread. It must not touch the rules directly, because the rules are acquisition-owned. Implement it as a single `std::atomic<std::uint64_t>` bitmask, set with `fetch_or` by the caller and cleared with `exchange(0)` by `Pump` at the top of step 4. **Rule ids 0 to 63 are supported; `Acknowledge` returns an error for an id of 64 or above rather than silently ignoring it.** Record that ceiling in the header. This needs no storage, which is why it is chosen over a ring. Do not add a mutex.

**`Reconnect`** does, in this order: `Disconnect`, `ISession::Reset`, `IDecoder::Reset`, `Connect`, increment the generation, `SignalRegistry::SetGeneration`, and `ClearLatch()` on every rule. It does **not** reach into the registry to discard samples. Anything already decoded before the bump that still reaches `Apply` carries the old generation and is rejected by the registry, which counts it in `RejectedGenerations()`. That is the mechanism the gate measures, and pre-discarding would make the gate unsatisfiable.

### 3. `FDashAcquisition` (`UnRealDashCore`, `Private/Acquisition/DashAcquisition.{h,cpp}` + `Public/UnRealDashCore/DashAcquisition.h`)

The engine host. Owns an `FRunnableThread` running `AcquisitionPipeline::Pump` in a loop, owns all the storage the pipeline borrows, and exposes to the game thread:

- `bool AcquireFrameSnapshot(FFrameSnapshot& Out)` - calls `SnapshotExchange::Acquire()`, **copies** the samples through `UnRealDashCore::ToEngineSample` into `Out`'s own storage, builds the `present` list, records the acquisition event, and returns. A copy rather than a span into the exchange buffers, so the 2.2 ownership rule holds even if the caller keeps `Out` past its next acquire.
- `void RecordSubmit(uint64 FrameIndex)`
- `void RequestAcknowledge(uint32 RuleId)` - `fetch_or` into the pipeline's bitmask.
- `FConnectionHealth GetHealth() const`

**"Present" means `SignalSample::received == true` in the acquired snapshot.** That is the registry's own bookkeeping flag, it is independent of quality, and it resets on a generation change, which is exactly the predicate 4.8's partition needs. The `present` id list is built by the game thread into a fixed buffer inside `FFrameSnapshot`, sized to the signal count.

`FFrameSnapshot` holds `TArray<UnRealDashCore::FSignalSample>`, a frame index, an acquisition timestamp and a generation. **It exposes no `signal_core::` type**, so the game module can consume a snapshot without including a signal-core header. That is the seam the layering rule needs.

The clock is monotonic and engine-sourced: `FPlatformTime::Cycles64` converted to nanoseconds. Not `UWorld::GetRealTimeSeconds`, which the smoke spike used and which is a game-thread quantity the acquisition thread must not read.

Shutdown: the engine `Stop()` sets an atomic stop flag, the run loop finishes its current `Pump`, calls `AcquisitionPipeline::Stop()` once, and exits; the destructor joins. No detached threads, no thread outliving borrowed storage.

### 4. The event sink (`SignalCore`, in `Acquisition.h`; engine sinks in `UnRealDashCore`)

PLAN 4.8 owns the event log on disk. 4.3 owns the record types and the seam, and must not wait for 4.8.

```cpp
struct IAcquisitionEventSink {
    virtual ~IAcquisitionEventSink() = default;
    virtual void OnAcquire(std::uint64_t frame, Time at, std::span<const SignalId> present) = 0;
    virtual void OnSubmit(std::uint64_t frame, Time at) = 0;
    virtual void OnRuleTransition(std::uint32_t rule, Time at, bool current, bool latched) = 0;
};
```

`OnRuleTransition` is called by the pipeline on the acquisition thread, and its `at` is the `now` of the `Pump` that produced the transition. **`OnAcquire` and `OnSubmit` are called by the game thread**, from `FDashAcquisition`, and their `at` is the engine monotonic clock read at the moment of that call, not a value carried from the acquisition side. The two sides therefore share a clock domain but not a sample point, which is what lets 4.8 measure the interval between them. A sink implementation must therefore be safe for two threads; the ring sink below uses one SPSC ring per caller rather than a lock.

Ship two implementations: a null sink, and a fixed-capacity in-memory ring sink used by the tests. Do not write a file in this chunk.

### 5. The shared stress suite, and the in-engine rerun

Move the five 2.13 scenarios out of `test_threading_stress.cpp` into `runtime/UnRealDash/Source/SignalCore/Public/SignalCore/ThreadingStressSuite.h`, following `SmokeSubset.h` exactly: no Unreal headers, no engine macros, and no identifier the engine defines as a macro (`check`, `ensure`, `verify`, `TEXT`).

Parameterized on:

- `ReportCheck(bool passed, const char* name)`.
- `ThreadLauncher` - `Launch(const char* name, Callable)` returning a handle with `Join()`. CMake supplies `std::thread`; the commandlet supplies `FRunnableThread`. Launching happens before any measured window, so an allocation inside `Launch` does not disturb the allocation checks.
- `AllocationProbe` - `bool Supported() const` and `Counts Read() const`.

**Case and assertion accounting, so criterion 6 is mechanically checkable.** The suite exposes `RunScenario(index, ...)` for indices 0 to 4 and reports per-scenario counts. `test_threading_stress.cpp` keeps **one `TEST_CASE` per scenario**, each calling exactly one `RunScenario`, so doctest's case count is unchanged at six including the heap-counter self-test, and each scenario's `CHECK` count is unchanged. Both runners print one line per scenario: `scenario=<n> checks=<n> failures=<n> skipped=<n>`. The gate compares those lines, not a single total, which removes the need to subtract the self-test.

**The allocation probe.** Two assertions compare heap allocation counts across a measured window and cannot run in-engine. Both are in the `sustained publication` scenario, at lines 79 and 80. The other three allocation comparisons in the file belong to the heap-counter self-test, which is not moved and not rerun in-engine. When `Supported()` is false the suite **does not call `ReportCheck` for those assertions and increments a skipped counter instead**, naming each skipped check. It must not report them as passing. A check that cannot fail is a defect class an earlier review of this project already caught, and a fake pass would hide the allocation regression the assertion exists to find. The owner confirmed this approach on 2026-09-17.

**Remove the two `WriterBlockedCount` assertions** while moving the scenarios, at lines 81 and 131. They cannot fail, for the reason in the Facts section. Removing them lowers `sustained publication` from 5 to 4 and `stalled reader` from 5 to 4. That is the intended change, not a regression, and it takes the five scenarios from 20 assertions to 18 and the whole file from 24 to 22 before any new assertion is added.

This makes PLAN 4.3's gate wording "the same assertion count and zero failures" unsatisfiable as literally written. The gate this chunk implements is: **per scenario, in-engine `checks + skipped` equals the CMake run's `checks`; in-engine `failures` is zero; and `skipped` totals exactly the two named allocation checks, both in `sustained publication`.** Record this in the report as a proposed PLAN amendment. Do not edit PLAN.md.

**Determinism: no wall-clock sleeps anywhere in the suite.** The producer already drives an injected `FakeClock`. The consumer's 60 Hz cadence and the 2 second stall are expressed as scripted advances of that same fake clock, with the consumer acquiring only at scripted points. A `std::this_thread::sleep_for` anywhere in the suite is a build failure, and a `yield` used only to spin on an atomic is not.

The commandlet from 4.2 gains a `-stress` switch that instantiates the same suite with its own reporter, `FRunnableThread` and the null probe. Without `-stress` it behaves exactly as today, so 4.2's gate is unaffected.

### 6. Enforce the layering rule

`UnRealDashCore` is the only module allowed to call signal-core. The game module already breaks it in three places, all from the 4.0 smoke spike:

```
runtime/UnRealDash/Source/UnRealDash/Private/Smoke/SmokeHUD.cpp:47,48
runtime/UnRealDash/Source/UnRealDash/Private/Smoke/SmokeHUD.h:26
```

1. Add a check to `scripts/doctor.ps1`, with a Pester test, that greps `runtime/UnRealDash/Source/` outside `Source/SignalCore/` and `Source/UnRealDashCore/` for `#include "SignalCore/`, `signal_core::` and `using namespace signal_core`, and fails naming the offending file and line.

   **This grep is a heuristic and the spec says so rather than pretending otherwise.** It catches the named tokens. It does not catch a `signal_core` type reached through `auto` or through a transitive include from a `UnRealDashCore` public header. That residual gap is closed by design instead: `UnRealDashCore`'s public headers expose no `signal_core` type, so there is nothing for the game module to reach transitively. `SignalCoreAdapter.h` currently includes `SignalCore/Sample.h` and therefore violates this; fixing it is part of this deliverable.

2. Make the three violations pass. `FSmokeSimulator`'s constructor takes a `signal_core::Clock`, forcing every caller to name the namespace. Introduce a concrete engine-side type:

```cpp
namespace UnRealDashCore {
struct FMonotonicClock { void* Context; int64 (*NowNanoseconds)(void*); };
UNREALDASHCORE_API FMonotonicClock MakePlatformClock();   // FPlatformTime::Cycles64 based
}
```

`FSmokeSimulator` takes `FMonotonicClock` and converts to `signal_core::Clock` inside its `.cpp`. `FSmokeHUD` holds `UnRealDashCore::FSignalSample` only. `SignalCoreAdapter.h` stops including `SignalCore/Sample.h`: `FSignalSample`'s `Unit`, `Quality` and `AgeEvidence` fields become plain `uint8` with the conversion done in the `.cpp`, or mirrored engine-side enums, your choice, stated in the report.

Do not fix this by loosening the check to allow the smoke files, and do not make `SignalCore` a private dependency, which would break the module's own compilation.

## Constraints

- Nothing in `SignalCore/` gains an Unreal header, Unreal type or engine macro. The CMake build enforces it and must stay green.
- No new global state, no singleton, no static mutable.
- `AcquisitionPipeline` allocates nothing after construction. All storage is caller-provided spans.
- No mutex anywhere between the acquisition thread and the game thread.
- No wall-clock read and no sleep in signal-core or in the stress suite.
- Errors are values. Nothing throws, and nothing calls `check()`, `verify()` or `ensure()` on a path a bad connector can reach.
- C++20, exceptions and RTTI off for the signal-core half.
- No em dashes in any file, comment or printed string.

## Non-goals

- No real transport. Ship an in-memory `ITransport` plus a replay-backed one driven by `signal_core::Replay`. The TCP transport is 4.7's; do not write sockets here.
- **No runtime command-line surface.** `-connector=`, `-connector-host=`, `-ack-warnings-at=` and the rest are PLAN 4.7. The commandlet's `-stress` switch in deliverable 5 is explicitly permitted and is not part of that surface.
- No event log on disk. That is 4.8.
- No UMG, no widget, no rendering. The game-thread side stops at `AcquireFrameSnapshot`.
- No device work of any kind.

## Pass/fail criteria

Each is a command that exits non-zero on failure.

1. The CMake `signal-core` build compiles with exceptions and RTTI off on Windows and Linux, its doctest suite passes, and `test_threading_stress.cpp` still reports **six** cases.

   Per-scenario assertion counts change in exactly two intended ways and no others. Removing the two `WriterBlockedCount` assertions takes `sustained publication` from 5 to 4 and `stalled reader` from 5 to 4. Criterion 2's gate scenarios then **add** assertions on top of that. No assertion that exists today is lost for any other reason.

   Take every count from the doctest XML reporter using the command in the Facts section, before and after, and put both tables in the report. Do not count assertions by reading the source.
2. The three PLAN 4.3 gate scenarios exist as doctest cases and pass:
   - **200 Hz into 60 Hz.** `display.Depth()` never exceeds its bound at any observation, `DisplayDrops()` is greater than zero, every transition the acquisition side counted is present in the sink, and the allocation probe reports no growth. Do **not** assert `WriterBlockedCount()`.
   - **Consumer stalled 2 simulated seconds mid-stream.** The queue stays bounded, no rule transition is lost, and the producer's `Pump` iteration count over the stall matches the un-stalled control run within the scripted tolerance recorded in the test. The iteration count is the real evidence that the producer never blocked.
   - **Forced reconnect with older-generation samples still in flight.** None is applied, and `RejectedGenerations()` equals the number injected. The test injects them by having the fake decoder emit fields stamped with the pre-bump generation after `Reconnect` has run, so they reach `Apply` and are rejected there.
3. **The ordering test.** This replaces the "reorder the two statements" wording from revision 1, which the review correctly showed could not fail: steps 3 and 5 touch disjoint data, so swapping them changes nothing observable.

   The defect this must catch is evaluating rules on **what survived the display queue** instead of on every applied sample. Construct it so: display queue capacity 1; within a single `Pump`, feed a sample that crosses a rule threshold followed immediately by one that crosses back, so the first is guaranteed to be dropped for display. Assert both:
   - the sink contains the transition for a sample that never entered the display queue, and
   - `RuleEvaluations()` equals `SamplesApplied()`, which is false for any implementation that evaluates per published frame or per surviving queue entry.

   `RuleEvaluations()` counts evaluation **passes**, not per-rule calls, so this identity holds whatever the rule count. Load exactly one rule anyway, to keep the failure unambiguous, and arrange the scenario so no armed expiry fires during it, because an expiry-driven pass would also increment the counter.

   State in the report that you verified this test fails against an implementation that evaluates rules from the queue.
4. ThreadSanitizer on the Linux runner reports zero data races over the whole suite, including the new cases.
5. UBT builds `SignalCore`, `UnRealDashCore` and `UnRealDash` for Win64.
6. The commandlet runs with `-stress` and prints one line per scenario. Per scenario, `checks + skipped` equals the CMake run's `checks`; `failures` totals zero; `skipped` totals exactly **two**, both in `sustained publication`, and both named in the output.
7. The commandlet still runs without `-stress` and still satisfies 4.2's gate unchanged.
8. `scripts/doctor.ps1 -Profile workstation` exits 0 including the new layering check, and the full Pester suite passes with no reduction in count.

## Proof

Codex runs these and pastes the output verbatim. Codex has no engine and no device; anything marked `[claude]` it must not attempt and must list as not run.

```
cd packages/signal-core && cmake --preset default && cmake --build --preset default
cd packages/signal-core && ctest --preset default --output-on-failure
packages/signal-core/build/default/signal-core-tests.exe "--source-file=*test_threading_stress.cpp" --reporters=xml
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -Output Detailed"
git status --short
```

`[claude]` The UBT builds, the commandlet with and without `-stress`, the per-scenario count comparison in criterion 6, the ordering mutant in criterion 3, and the Linux/TSan job.

Codex's pasted proof is advisory. Claude re-runs everything.

## Report format

End with: files added or changed (one line each: path, what, which PLAN.md task), the proof output verbatim for what you ran, an explicit list of what you did not run and why, the choice you made for `SignalCoreAdapter.h`'s enum handling, the proposed PLAN 4.3 gate amendment from deliverable 5 with its reason, confirmation that criterion 3's test fails against a queue-driven evaluator, any other deviation from PLAN.md or this spec with the reason, and anything you could not do.
