# Chunk 13: the Stage 0 connectors, signal-core half (PLAN 4.9)

**PLAN 4.9 is not complete.** Six of its ten criteria pass; three cannot be reached yet and one of
those is the reason. This report says which, and why, rather than claiming the task.

What is done is the whole of the engine-independent half: the retry schedule, the connection
supervisor, the receive-only TCP transport, and the adapter that finally wires the
binary-telemetry-v1 parser to the acquisition pipeline. All of it compiles in the CMake build with
exceptions and RTTI off, so it is covered by the hosted CI suite rather than only by a run here.

## What blocks the rest

PLAN 4.9's criteria 4, 5 and 6 are "the player shows live values", "killing the mock marks every
mapped signal stale", and "restarting reconnects without a player restart". Each needs a capture of
the player reacting to data arriving over the socket.

**Nothing connects acquisition to the widgets.** `FDashAcquisition` publishes a snapshot and
`UDashPackageScreen` builds a widget tree, and there is no path between them. Worse, the primitives
chunks 11 and 12 built bake their value in at construction: `BuildAnalogDial` writes the deflection
into a render transform once and returns a widget with no way to be told a new number. That is why
the gauges are currently driven by `-udash-fraction`, a gate switch, rather than by a signal.

Closing that means a real addition: every builder has to produce an updater alongside its widget,
the screen has to hold component id to updater, and a tick has to walk the document's bindings
against the latest snapshot. It is a chunk of its own, and improvising it at the end of this one is
how the generation bug below would have shipped. It is specified rather than guessed at, and it is
what the next session starts on.

## Criteria

| # | criterion | status |
| --- | --- | --- |
| 1 | the backoff schedule is exact | **pass** |
| 2 | an absent relay is not a spin loop | **pass** |
| 3 | zero bytes sent, two ways | **pass** |
| 4 | live values through the real socket | blocked, see above |
| 5 | killing the relay marks signals stale | blocked, transport half proven |
| 6 | restarting reconnects with no player restart | blocked, supervisor half proven |
| 7 | the wall-clock backoff run | **pass** |
| 8 | both suites green, no drop | **pass** |
| 9 | doctor and Pester | **pass** |
| 10 | UBT builds, chunks 11 and 12 not regressed | **pass** |

**1 and 2.** The schedule is 500, 1000, 2000, 4000, 5000, 5000 ms, reset to 500 by a successful
connect, asserted not-due one millisecond early and due exactly on time at every step. Over a
simulated sixty seconds polled every 10 ms, 6001 polls produce exactly 15 connect attempts at
0, 500, 1500, 3500, 7500, 12500 and every 5000 ms after, and the other 5986 polls do nothing.

**3.** Measured at a real server socket rather than asserted about the code: after a full connect
and read session, `LoopbackListener::Received()` returns 0. And `scripts/doctor.ps1` reads
`TcpTransport.cpp` for a send call. Both halves were watched failing: inserting `::send` into the
transport makes doctor print `TcpTransport.cpp:202: receive-only transport calls send` and exit 1.

**7.** Sixty real seconds against a dead port:

```
attempts: 15, cpu seconds: 0.015625
attempt 0 at 1.8e-06 s, expected 0
attempt 1 at 0.515805 s, expected 0.5
attempt 2 at 1.51663 s,  expected 1.5
attempt 3 at 3.51975 s,  expected 3.5
attempt 4 at 7.53511 s,  expected 7.5
attempt 5 at 12.5267 s,  expected 12.5
...
attempt 14 at 57.5639 s, expected 57.5
```

Every attempt within 64 ms of the schedule, against a 100 ms band. **0.0156 seconds of process CPU
over sixty seconds is 0.026 percent of a core**, against the 1 percent the criterion allows. The
test is skipped by default and run with `--no-skip`: a minute-long test in the default suite is a
suite people stop running.

**8.** signal-core 148 cases and 112,876 assertions, up from chunk 12's 136 and 111,782, none lost.
dashboard-spec unchanged at 28 cases and 1,002,149. **9.** doctor exits 0 with both the layering
check and the new receive-only check; Pester 76 passed, 0 failed. **10.** Win64 and Android ARM64
both `Result: Succeeded`; `Capture gate: 0 failures` and `Dial gate: 0 failures`.

## What the build found

### The retry cannot live on the transport, and the decorator shape was wrong

The spec proposed a `ReconnectingTransport` decorator whose `Read` would reconnect when the schedule
said to. Reading `Acquisition.cpp` first showed it could never run: `Pump`'s read loop opens with
`while (health_.connected && !failed)`, so a disconnected transport is never polled at all, and
nothing in the pipeline calls `Connect` again on its own.

The tempting repair, having the decorator report `Connected() == true` throughout the retry window,
is worse. `Connect` is the only place the connection generation advances, and the registry rejects
samples from an unconnected generation. A decorator reconnecting the socket underneath a
still-connected pipeline would defeat exactly the guard that catches a stale link.

So the retry moved to a `ConnectionSupervisor` beside the pipeline, holding no socket and taking the
time as a parameter. That is also what makes criteria 1, 2 and 7 cheap.

### The connection generation cannot be self-incremented, and would have broken every live gate

The spec said both adapters' `Reset` should call `ReconnectBinaryTelemetryV1`, which advances the
decoder's generation by one. The review flagged it and the source confirmed it:
`AcquisitionPipeline::Start` and `::Reconnect` each call `session_.Reset()` then `decoder_.Reset()`,
and `Connect` advances its own generation only on success.

So two Resets per connect advance the decoder twice. Worse, and this half the review did not reach:
**every failed connect attempt advances it again while the pipeline stands still.** With the TCP
transport reporting a connect in progress, failed attempts are the normal case. The decoder would
have ended up arbitrarily far ahead, and the pipeline would have rejected the first sample that
arrived with "sample claims an unconnected generation" every time.

The generation is now told to the connector, not guessed by it: the owner passes
`Health().generation` after each successful connect. A test drives five Resets and a Reconnect and
then checks samples still arrive.

### The ring capacity claim was wrong by a factor of four

The spec said a 64 frame ring was larger than one read could produce. The framer consumes exactly 16
bytes per frame and the engine's read buffer is 4096, so a full read is **256** frames. The ring is
now 320, and a test feeds exactly 256 frames in one read and checks the overflow counter is zero, so
the bound is measured rather than believed.

## The review

DeepSeek reviewed the spec against the tree. Its first run spent all 40 tool calls reading and
produced nothing; resumed and asked to answer from what it had, it returned two blockers and two
majors. Both blockers were real and are above. It also corrected two facts I had overstated: the
framer appears in two tools I had not listed, and `MemoryTransport` is over a raw byte span rather
than a `Recording`.

One of its findings was already fixed before it answered, because it had read the tree before the
code existed. That is the cost of reviewing a spec and a build at the same time, and it is cheaper
than the alternative.

## What was not run, and why

- **No device run.** 4.9's gate is a local relay on loopback.
- **The mock relay has not driven the player**, because nothing connects acquisition to the widgets.
  Its frame format is verified against the framer's own constants and its output decodes correctly
  through the real pipeline in `test_connector_pipeline.cpp`, which is the same bytes by a shorter
  path.
- **The replay and simulator connectors are not assembled.** Both have their transports already in
  the tree from earlier chunks; what they lack is the same selection and wiring layer criteria 4 to
  6 need.
- **ThreadSanitizer was not re-run.** Nothing here runs on the acquisition thread yet.

## Known limitations

- **`-connector=`, `-connector-host=` and `-connector-port=` are not wired.** They belong with the
  selection layer, and wiring a flag to nothing would be worse than leaving it out.
- **`ConnectionHealth.backoff_ms` and `attempts_since_connect` are filled by the owner**, not by the
  pipeline, which holds its health privately and knows nothing about a supervisor. `ReportBackoff`
  takes the health by reference for that reason.
- **The TCP transport resolves names in `Connect`.** A slow name server delays a retry rather than
  stalling acquisition, but it is still a blocking call on whichever thread calls `Connect`. With a
  numeric host, which is the default and what the gate uses, nothing resolves.
