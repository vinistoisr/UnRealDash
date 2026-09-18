"""A local binary-telemetry-v1 relay, for the PLAN 4.9 connector gate.

It listens on 127.0.0.1:35000, accepts one client, and streams frames until it is killed. The gate
uses it three ways: running, to show live values; killed, to show every mapped signal going stale at
its deadline; and restarted, to show the connector reconnecting with no player restart.

The frame, read out of the framer rather than guessed:

    bytes 0..3   tag 44 33 22 11          (BinaryTelemetryV1.cpp, Tag)
    bytes 4..7   frame id, little endian  (BinaryTelemetryV1.cpp, Identifier)
    bytes 8..15  payload, 8 bytes         (the sink call: bytes.data() + 8, length 8)

Sixteen bytes exactly, which is also where the connector's 320 frame ring capacity comes from: a
4 KiB read holds at most 256 of them.

**It never reads from the socket.** The connector is receive-only, and a relay that drained its
input could not tell the difference between a connector that sends nothing and one that sends
something it ignored. Zero-bytes-sent is therefore measured on the connector side instead, by
packages/signal-core/tests/test_tcp_transport.cpp, which counts what actually arrives at a server
socket, and by the receive-only source check in scripts/doctor.ps1.
"""
import argparse
import math
import socket
import struct
import sys
import time

TAG = b"\x44\x33\x22\x11"
DEFAULT_FRAME = 3200          # tests/fixtures/packs/binary-telemetry-v1.test.json
DEFAULT_PORT = 35000


def frame(frame_id, payload):
    if len(payload) != 8:
        raise ValueError("payload must be 8 bytes, got " + str(len(payload)))
    return TAG + struct.pack("<I", frame_id) + payload


def payload_at(elapsed, period):
    """Four little-endian uint16 fields, matching the test pack's frame 3200.

    Field 1 is the one the gate measures, and it sweeps a full triangle over `period` seconds so a
    capture taken at any moment inside the first half shows a gauge somewhere other than its
    minimum. A triangle rather than a sine because its value at a given time is obvious to anyone
    reading a capture and arguing about what the gauge should show.
    """
    phase = (elapsed % period) / period
    ramp = phase * 2 if phase < 0.5 else (1.0 - phase) * 2
    live = int(ramp * 65535)
    return struct.pack("<HHHH", 4242, live, int(ramp * 10000), 40 + int(ramp * 200))


def serve(port, frame_id, rate, period, limit):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", port))
    listener.listen(1)
    # Unbuffered so a gate script can wait on this line before launching the player, instead of
    # sleeping a guessed amount and hoping.
    print("mock-relay listening on 127.0.0.1:" + str(port), flush=True)

    started = time.monotonic()
    sent = 0
    while True:
        listener.settimeout(1.0)
        try:
            client, _ = listener.accept()
        except socket.timeout:
            if limit and time.monotonic() - started > limit:
                break
            continue
        print("mock-relay client connected", flush=True)
        client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        interval = 1.0 / rate
        try:
            while True:
                elapsed = time.monotonic() - started
                if limit and elapsed > limit:
                    break
                client.sendall(frame(frame_id, payload_at(elapsed, period)))
                sent += 1
                time.sleep(interval)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, OSError):
            print("mock-relay client went away after " + str(sent) + " frames", flush=True)
        finally:
            client.close()
        if limit and time.monotonic() - started > limit:
            break
    listener.close()
    print("mock-relay sent " + str(sent) + " frames", flush=True)


def main():
    parser = argparse.ArgumentParser(description="Local binary-telemetry-v1 relay for the PLAN 4.9 gate.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--frame-id", type=int, default=DEFAULT_FRAME)
    parser.add_argument("--rate", type=float, default=50.0, help="frames per second")
    parser.add_argument("--period", type=float, default=20.0, help="seconds for one full sweep")
    parser.add_argument("--seconds", type=float, default=0.0, help="exit after this long; 0 runs until killed")
    arguments = parser.parse_args()
    try:
        serve(arguments.port, arguments.frame_id, arguments.rate, arguments.period, arguments.seconds)
    except KeyboardInterrupt:
        print("mock-relay stopped", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
