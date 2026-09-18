# Build chunk 13: the three Stage 0 connectors

Status: revision 1, written against the tree. Covers PLAN.md task 4.9. Read PLAN.md (task 4.9, task
2.x for the parser, task 4.3 for the acquisition pipeline, task 4.7 so you know which flags you are
not building) and `docs/ARCHITECTURE.md` first. PLAN.md is the authority on what; this file adds the
mechanisms, proof commands and implementation constraints.

Chunks 11 and 12 each shipped corrections because a spec was frozen without checking it against the
tree. Every fact below carries the file and line that produced it.

## Facts this chunk depends on

- **The seam already exists.** `SignalCore/Connector.h` declares `ITransport` (nonblocking `Read`,
  `Connect`, `Disconnect`, `Connected`), `ISession` (`Offer`/`Next`/`Reset`), `IDecoder` (the same
  shape over `DecodedField`) and `IMapping`. `AcquisitionPipeline` takes all four by reference
  (`Acquisition.h`), so a connector is a transport plus a session plus a decoder plus a mapping, and
  nothing in the pipeline changes.

- **The pipeline already owns the connection generation.** `Acquisition.cpp:47-48` increments
  `health_.generation` and calls `registry_.SetGeneration` on every successful connect, and
  `Acquisition.cpp:163-168` rejects a sample claiming an unconnected generation. This chunk must not
  add a second generation counter.

- **The parser exists and has never been wired to the pipeline.**
  `BinaryTelemetryV1Framer::Feed(bytes, FrameSink, context)` and
  `BinaryTelemetryV1Decoder::OnFrame(frame, SampleSink, context)` are push-based; `ISession` and
  `IDecoder` are pull-based. Nothing in the tree implements either interface over them:
  `BinaryTelemetryV1Framer` appears only in its own header, its own source and
  `packages/signal-core/tests/test_binary_telemetry_v1.cpp`. **The adapter is this chunk's work and
  is where its risk is.**

- **`ReconnectBinaryTelemetryV1(framer, decoder)` already exists** as the connection glue that
  discards carry and lock state and advances the sample generation together.

- **`MemoryConnector.h` holds `MemoryTransport` and `ReplayTransport`.** Both are in-process test
  aids over `signal_core::Recording`. Neither is the file replay connector PLAN 4.7 names with
  `-replay=<file>`, and `ReplayTransport` writes to a private staging registry rather than to the
  pipeline's.

- **No backoff exists anywhere in the tree.** `grep -rn backoff runtime/UnRealDash/Source` finds
  nothing. `ConnectionHealth` carries `generation`, `connected`, `last_byte_at`, `reconnects`,
  `bytes` and `last_error`, and no backoff state, which 4.9 requires it to report.

- **The simulator is engine-side.** `UnRealDashCore::FSmokeSimulator` wraps `signal_core` scenarios
  behind an Unreal interface. The scenarios themselves are in `SignalCore/Scenarios.h` and are
  engine-independent.

## Deliverables

### 1. `BackoffPolicy`, pure and instantly testable

In signal-core. It holds no socket, no thread and no real clock: given the time now and the time of
the last attempt, it answers whether another attempt is due and what the current interval is.

- First retry at **500 ms**, doubling, ceiling **5 s**: 500, 1000, 2000, 4000, 5000, 5000, ...
- A successful connect resets it to the first interval.
- The interval is the state, so a test advances a fake clock and reads the schedule with no waiting.

Separating the policy from the socket is what makes 4.9's hardest gate cheap. "The retry intervals
follow the backoff schedule to the 5 s ceiling" becomes a unit test over a fake clock that runs in
microseconds, instead of a 60 second wall-clock test nobody will run twice.

### 2. `ReconnectingTransport`, a decorator over any `ITransport`

It owns the policy and the retry, so the same behaviour applies to a socket, to a file and to a
fake in a test.

- When the inner transport is connected, `Read` delegates.
- When it is not, `Read` returns `need_more_data` with **zero bytes written and no connect attempt**
  unless the policy says an attempt is due. That is the whole of "an absent relay is not a spin
  loop": the expensive call is gated, not the loop.
- On a successful reconnect it resets the policy. It does **not** touch the generation, because
  `AcquisitionPipeline::Connect` already owns that.
- It exposes `Attempts()`, `CurrentIntervalMs()` and `NextAttemptAt()` for the health interface.

### 3. `TcpTransport`, receive-only

A non-blocking client socket for `-connector-host`/`-connector-port`, default `127.0.0.1:35000`.

- `Connect` sets the socket non-blocking **before** calling `connect`, so a dead host returns
  `EWOULDBLOCK`/`WSAEWOULDBLOCK` immediately and the acquisition thread is never parked.
- `Read` polls and returns what is available, zero bytes included.
- **The class has no send path at all.** No `send`, no `write`, no `sendto` anywhere in the file.
  "Zero bytes sent" is therefore a property of the source, not only of a test run, and the gate
  checks both: a source-level check in `scripts/doctor.ps1` beside the existing layering check, and
  a counting fake socket in the suite.
- Platform is Winsock on Windows and BSD sockets elsewhere, behind one small `#if` in the source
  file. No Unreal headers: this has to compile in the CMake build for hosted CI.

### 4. `BinaryTelemetryV1Session` and `BinaryTelemetryV1Decoding`

The adapters from push to pull.

- `Session::Offer(input, consumed)` feeds the framer and collects every `FrameEvent` into a bounded
  ring; `Next(out)` hands them out one at a time. A frame that arrives when the ring is full is
  **dropped and counted**, never silently overwritten, because a silently lost frame is
  indistinguishable from a signal that stopped.
- `Decoding::Offer(message)` runs the decoder over one frame and collects the emitted samples;
  `Next(out)` hands them out. Same bounded ring, same counted drop.
- `Reset()` on either calls `ReconnectBinaryTelemetryV1`, so carry, lock state and generation are
  discarded together rather than in two places.
- Ring capacities are compile-time constants stated here: **64 frames** and **256 samples**. Both are
  larger than one `Read` of the 4 KiB read buffer can produce, and the drop counters exist so the
  assumption is measured rather than assumed.

### 5. `DefinitionPackMapping`

`IMapping` over the WP3-generated pack, converting a decoded field to SI through the existing units
table. A field the pack does not describe is a counted mapping drop, which the pipeline already
reports as `mapping_drops`.

### 6. The three connectors, assembled

One factory each, returning the four parts wired together:

- **simulator**: the in-process scenario source. No socket, no backoff.
- **replay**: a file of recorded bytes, read in bounded chunks. `-replay-loop` restarts at the end
  rather than disconnecting.
- **tcp**: `ReconnectingTransport` over `TcpTransport`, with the binary-telemetry-v1 session and
  decoder.

### 7. Health

`ConnectionHealth` gains `backoff_ms` and `attempts_since_connect`. 4.9 requires the backoff state
to be reported and the struct has nowhere to put it. Both are written by `ReconnectingTransport` and
are zero for the connectors that do not retry.

### 8. The mock relay

`scripts/mock-relay.py`: listens on `127.0.0.1:35000`, accepts one client, replays a file of
recorded bytes at a stated rate, and exits on request. It is the gate's other half and it must be
startable and killable from the gate script, because "killing the mock marks every mapped signal
stale" is the test.

It never reads from the socket. If the connector ever sent anything, the relay would not notice,
which is why zero-bytes-sent is proven on the connector side instead.

## Constraints

- No Unreal headers in signal-core. The CMake build compiles the same sources with exceptions and
  RTTI off for hosted CI, and that is where most of this chunk's correctness lives.
- No `dashboard_spec::` or `signal_core::` name in the `UnRealDash` game module.
- Errors are values. A refused connection, a missing replay file and an unmapped field are all
  counted or reported, never thrown and never silent.
- The acquisition thread never blocks on `connect`, on DNS or on a read.
- No em dashes; no PowerShell 7 only syntax in scripts.

## Non-goals

- The full command-line surface, which is 4.7. This chunk wires `-connector=`, `-connector-host=`
  and `-connector-port=` because it cannot be demonstrated without them, and leaves the rest.
- The debug overlay (4.8) and the example document (4.10).
- Any device work. The gate is a local relay on loopback.
- Sending anything, ever. Stage 0 telemetry is receive-only.

## Pass/fail criteria

1. **The backoff schedule is exact.** A unit test over a fake clock asserts the intervals are 500,
   1000, 2000, 4000, 5000, 5000 ms and that a successful connect resets to 500. Print the schedule.
2. **An absent relay is not a spin loop.** With no relay, a fixed number of pumps over a simulated
   60 seconds performs exactly the number of connect attempts the schedule predicts, and no more.
   Print attempts and the predicted count. This is the unit form; the wall-clock form is criterion 7.
3. **Zero bytes sent, two ways.** A counting fake socket reports zero bytes written across a session
   of at least 100,000 pumps, and a source check finds no send call in the TCP transport.
4. **Live values through the real socket.** With `scripts/mock-relay.py` running, a capture of the
   player shows a gauge deflected away from its minimum, measured with `scripts/measure-extent.py`
   rather than judged by eye.
5. **Killing the relay marks every mapped signal stale within its deadline**, and the capture shows
   the stale presentation.
6. **Restarting the relay reconnects with no player restart**, and a later capture shows live values
   again. The reconnect count in health increments by exactly one.
7. **The wall-clock backoff run.** 60 seconds with no relay: the recorded attempt timestamps match
   the schedule within 100 ms, and the process CPU time over the window is under 1 percent of a
   core. Run once, output in the report.
8. Both CMake suites stay green with no drop in case or assertion count. Chunk 12 recorded
   dashboard-spec 28 cases and 1,002,149 assertions, signal-core 136 cases and 111,782 assertions.
9. `scripts/doctor.ps1 -Profile workstation` exits 0 including the layering check and the new
   no-send check, and the full Pester suite passes with `-CI`.
10. UBT builds Win64 and Android ARM64, and the chunk 11 and 12 gates still pass unchanged.

## Proof

```
cd packages/signal-core && cmake --build --preset default && ctest --preset default --output-on-failure
cd packages/dashboard-spec && ctest --preset default --output-on-failure
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI"
pwsh -NoProfile -File scripts/run-connector-gate.ps1
pwsh -NoProfile -File scripts/run-capture-gate.ps1
pwsh -NoProfile -File scripts/run-dial-gate.ps1
git status --short
```

## Report format

Files added or changed, one line each with path, what and which PLAN.md task; the proof output
verbatim; the measured backoff schedule from both the unit and the wall-clock run; an explicit list
of what was not run and why; any deviation from PLAN.md or this spec with the reason.

---

## Revision 2: the retry does not belong on the transport

Found by reading `Acquisition.cpp` before building, which is where the last two chunks' corrections
also came from.

### C1. A disconnected transport is never polled, so a decorator on it can never retry

`AcquisitionPipeline::Pump` opens with:

```cpp
while (health_.connected && !failed) {
```

and closes with `health_.connected = transport_.Connected();`. So once the transport reports
disconnected, `Pump` does no reads at all and never calls `Connect`. Nothing in the pipeline retries
on its own: `Reconnect()` exists and is public, and something outside has to call it.

Deliverable 2 proposed a `ReconnectingTransport` decorator whose `Read` would attempt a reconnect
when the policy said one was due. Its `Read` would never be called.

The alternative of having the decorator report `Connected() == true` at all times and hide the real
state was considered and rejected: `Connect` is the only place the connection generation advances
(`Acquisition.cpp:47-48`), and the registry rejects samples from an unconnected generation
(`Acquisition.cpp:163`). A decorator that never lets `Connect` run again would reconnect the socket
without advancing the generation, which is the exact hazard that guard exists for.

**Decision.** The retry moves to a `ConnectionSupervisor` that owns the `BackoffPolicy` and sits
beside the pipeline rather than inside it. It holds no socket: given the health and the time, it
answers whether `Reconnect()` is due and records the attempt. It is pure, so the backoff schedule
and the attempt count are unit tests over a fake clock, which is what criteria 1 and 2 need.

The engine-side acquisition runner calls it once per tick. The `ITransport` implementations stay
simple and have no retry logic in them at all.

### C2. A non-blocking connect cannot report success immediately, and the pipeline checks

```cpp
auto status = transport_.Connect();
if (!status.Ok())
    return Remember(status);
health_.connected = transport_.Connected();
if (!health_.connected)
    return Remember(Error(ErrorCode::io_error, "connect succeeded without connection"));
```

A non-blocking `connect()` to a host that is not listening returns in progress, so `Connected()` is
false when `Connect()` returns. Reporting `Ok` there gives "connect succeeded without connection",
and reporting `Connected() == true` would be a lie the generation guard cannot tolerate.

**Decision.** `TcpTransport::Connect()` returns `need_more_data` while the connect is still in
flight, which short-circuits above the check. The supervisor treats that as a failed attempt and
schedules the next one under the backoff. On loopback the connect completes immediately and the
first attempt succeeds; against an absent relay every attempt fails cheaply, which is the case
criterion 2 measures.

Each attempt is a fresh socket, because `Reconnect()` calls `Disconnect()` first. That is a few
microseconds on loopback and it keeps the state machine to two states rather than three.

### C3. `Offer` must always consume, so the rings stop feeding instead of dropping

```cpp
if (!consumed || consumed > written - offset) {
    Fail(Error(ErrorCode::invalid_configuration, "session Offer made invalid progress"));
```

A session that refuses to consume because its ring is full fails the pipeline outright. And the call
pattern is `Offer`, drain `Next` fully, `Offer` again with the remainder, so the ring only ever has
to hold what one `Offer` chooses to produce.

**Decision.** `Offer` feeds the framer only while the frame ring has room and returns with the bytes
it actually consumed, which is always at least one. The pipeline drains and calls it again with the
rest. Nothing is dropped, and the drop counters stay in place as an assertion that the bound holds
rather than as an expected path.

The same applies to the decoder: `Offer` takes one frame, so its ring only has to hold one frame's
fields.

### C4. The clock is injectable, so criteria 1 and 2 are cheap

`Clock` is a function pointer and a context (`Clock.h`), so a test supplies a `now` that reads a
variable it advances. The backoff schedule and the attempt count over a simulated 60 seconds are
microsecond unit tests, and criterion 7's wall-clock run is a confirmation rather than the proof.
