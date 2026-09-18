# Chunk 15: the TCP connector, end to end (PLAN 4.9)

**PLAN 4.9 is complete.** The dashboard now runs off a real socket: a relay feeds
binary-telemetry-v1 frames over TCP, the gauges move, killing the relay marks every mapped signal
stale, and restarting it reconnects with no player restart and the connection generation advanced.

## The gate, in one run

```
starting the relay on 127.0.0.1:35000
  relay pid 46436, listening
killing the relay at 5s
restarting the relay at 12s

health over the run:
  t=  0.8 connected=True  generation=1 reconnects=0 bytes=736    backoff_ms=500  attempts=0
  t=  2.7 connected=True  generation=1 reconnects=0 bytes=2288   backoff_ms=500  attempts=0
  t=  3.9 connected=False generation=1 reconnects=0 bytes=2848   backoff_ms=1000 attempts=1
  t=  6.7 connected=False generation=1 reconnects=0 bytes=2848   backoff_ms=2000 attempts=2
  t=  7.3 connected=False generation=1 reconnects=0 bytes=2848   backoff_ms=4000 attempts=3
  t=  9.7 connected=False generation=1 reconnects=0 bytes=2848   backoff_ms=5000 attempts=4
  t= 10.6 connected=True  generation=2 reconnects=1 bytes=3008   backoff_ms=500  attempts=0
  t= 20.0 connected=True  generation=2 reconnects=1 bytes=9680   backoff_ms=500  attempts=0

criterion 4: connected and received 2288 bytes before the kill
criterion 5: disconnected within 1s, backoff reached 5000ms over 5 samples
criterion 6: reconnected by t=13s, generation 1 then 2, reconnects=1
  and bytes resumed, 2288 before the kill to 9680 after

Connector gate: 0 failures
```

Criterion 6 is a sequence rather than a state, so no capture can show it. The health line the
player emits once a second is what carries it, and the gate orchestrates the sequence against one
running process.

## PLAN 4.9's criteria, all of them

| # | criterion | where |
| --- | --- | --- |
| 1 | the backoff schedule is exact | `test_reconnect.cpp`, chunk 13 |
| 2 | an absent relay is not a spin loop | `test_reconnect.cpp` and `test_tcp_transport.cpp`, chunk 13 |
| 3 | zero bytes sent, two ways | socket measurement and a source check, chunk 13 |
| 4 | live values over the real socket | the connector gate, and the live gate on screen |
| 5 | killing the relay marks signals stale | the connector gate, and the live gate on screen |
| 6 | restarting reconnects with no player restart | the connector gate |
| 7 | the wall-clock backoff run | `--no-skip`, chunk 13: 15 attempts, 0.0156 s of CPU over 60 s |
| 8 | both suites green, no drop | dashboard-spec 30 cases and 1,002,201; signal-core 148 and 112,876 |
| 9 | doctor and Pester | 0 and 76/76 |
| 10 | UBT builds, earlier gates intact | Win64 and Android succeeded; capture, dial and live gates 0 failures |

## Files added or changed

- `DashboardSpec`: `Document::DefinitionPackText`, serializing the bundle's pack back to JSON, and
  `FDashPackage::DefinitionPackText` over it. PLAN 3.4 in support of 4.9.
- `UnRealDashCore/Private/Acquisition/DashAcquisition.{h,cpp}`: the TCP construction path, the
  supervisor loop, and the reconnect count. PLAN 4.9.
- `UnRealDashCore/Public/UnRealDashCore/DashAcquisition.h`: `FDashTcpOptions`, backoff in health.
- `UnRealDashCore/.../DashBindingTable.{h,cpp}`: the signal numbering is supplied rather than
  assumed. PLAN 4.9.
- `SignalCore/Public/SignalCore/TcpTransport.h`: `Connecting()`.
- `UnRealDash/Private/Package/DashPackageScreen.{h,cpp}`: `-connector=`, `-connector-host=`,
  `-connector-port=`, `-udash-quit-after=`, and the health line. PLAN 4.9.
- `tools/gen-chunk15-fixture.py`, `scripts/run-connector-gate.ps1`, `tests/fixtures/...`.

## What the build found

### The reconnect never happened, and the cause was the retry itself

The first working connector connected, noticed the relay dying, and backed off correctly to the
five second ceiling. It then never came back, through fifteen seconds of a relay that was up and
listening.

`AcquisitionPipeline::Reconnect` calls `Disconnect` before `Connect`. A non-blocking connect to
loopback reports in progress rather than connected, so every scheduled attempt opened a socket,
was told to wait, and then five seconds later **closed the socket that was about to succeed** and
opened another one. The link could never come back.

The fix separates two things the schedule had conflated. The backoff governs *starting* an attempt;
it has nothing to say about *finishing* one. A connect in flight is now polled every tick through
`Start`, which does not disconnect, and `Reconnect` is only for getting a fresh socket after a real
drop. `TcpTransport::Connecting()` is what lets the caller tell the two apart.

This is the failure chunk 13's spec revision anticipated in principle, under C2, and still shipped
in practice. Knowing that a non-blocking connect reports in progress was not the same as following
it through to what `Reconnect` would do about it.

### The reconnect counter counted the wrong thing

Once it reconnected, `reconnects` stayed at zero. Only `AcquisitionPipeline::Reconnect` increments
that counter, and the reconnection completed through `Start`, so the count missed exactly the event
it exists to report. It is now counted where the link actually comes back, and the pipeline's own
counter is left meaning what it says: the number of `Reconnect` calls.

### Two gate defects, both found by the gate failing rather than by reading it

A leftover relay from an earlier manual run held port 35000, so the gate's own relay exited
instantly and the only symptom was `Cannot bind argument to parameter 'Id'` several seconds later.
The gate now checks the port before starting, says so plainly, and cleans up in a `finally` so a
run that dies part way does not poison the next one.

And `$player = Start-Process -FilePath $Player` quietly did nothing useful: PowerShell variable
names are case insensitive, so that is the `[string]$Player` parameter, and assigning a Process to
it coerced it straight back to a string. The symptom was `WaitForExit` failing on a String twenty
seconds later.

### The health timeline was not monotonic

`t=` was computed as ticks multiplied by the current frame delta, which went backwards between
lines whenever a frame ran long. A log whose own timeline cannot be trusted is worse than no
timeline. Accumulated now.

## What was not run, and why

- **No device run.** The gate is a loopback relay on the workstation.
- **The simulator and file replay connectors are not selectable.** `-connector=` accepts `tcp` and
  refuses anything else rather than falling back. The scenario source has its own switch from chunk
  14 and a file replay is a small addition on the same seam; neither is needed for 4.9's gate and
  wiring a flag to nothing would be worse than leaving it out.
- **The connector gate does not run in CI**, for the same reasons as the other capture gates, plus
  it binds a loopback port.
- **`freshness_deadline_ms` from the signals document is not read on the TCP path.** Every signal
  gets 500 ms, matching the scenario source. Reading it would mean a second join between the
  document's signals and the pack's fields, and 4.9's gate turns on staleness happening at all
  rather than on its exact instant.

## Known limitations

- **The definition pack's field names must match the document's signal names.** That is the join,
  and nothing checks it beyond a binding failing to resolve, which is a readable error naming the
  binding's pointer.
- **Name resolution happens inside `Connect`.** With a numeric host, which is the default and what
  the gate uses, nothing resolves. A slow name server would delay a retry rather than stall
  acquisition, but it is still a blocking call on the acquisition thread.
- **An indicator lights on a non-zero reading.** A rule result driving it is PLAN 5.x.
