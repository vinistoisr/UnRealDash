"""Validates a PLAN 4.8 event log against the schema the log itself declares.

The schema lives in the file rather than only in a document, so this tool checks a log against
what that run said it would write. A row of a type the schema does not declare is an error, and so
is a row whose columns do not match the declared list: both are how a record type quietly changes
shape and takes every count computed from it with it.

Exits 0 when the log is valid, 1 when it is not, and 2 when it could not be read at all.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

# Columns every record carries in addition to its declared ones.
IMPLICIT = {"type"}
# Known timestamp columns. Additional declared t_ columns receive the same checks so a new
# record cannot quietly bypass positivity and monotonicity validation.
TIMESTAMPS = {"t", "t_start", "t_present", "t_recv", "t_acquire", "t_submit", "t_armed",
              "t_decode_start", "t_decode_end", "t_upload_start", "t_first_usable"}
COLUMNS = {
    "frame": "frame t_start frame_ns t_present missed resident_bytes texture_bytes",
    "sampled": "t resident_bytes texture_bytes handles temperature_c",
    "receive": "sample signal t_recv seq generation quality age_evidence",
    "acquire": "frame t_acquire samples",
    "submit": "frame t_submit",
    "present": "frame t_present signals",
    "expiry_armed": "expiry kind signal sample generation t_armed becomes",
    "expiry_status": "expiry kind status t_fired reason by",
    "asset_import": "asset bytes width height t_decode_start t_decode_end t_upload_start t_first_usable",
    "first_usable": "frame t_present",
    "launch": "launch_counter first_usable_counter counter_frequency process_start_ticks "
              "process_ticks_per_second first_usable_boot_ns am_start_output am_launch_state",
}
PRIMARY = {"frame": "t_start", "sampled": "t", "receive": "t_recv", "acquire": "t_acquire",
           "submit": "t_submit", "present": "t_present", "expiry_armed": "t_armed",
           "expiry_status": "t_fired", "asset_import": "t_first_usable", "first_usable": "t_present"}

# t_fired is null only on a cancellation; a firing must carry a positive monotonic endpoint.
NULLABLE_TIMESTAMPS = {"t_fired"}
# Mirrors signal_core::Quality and CancelReason.
STALE = 1
CANCEL_REASONS = {0: "re-armed", 1: "generation", 2: "run_end"}


def fail(problems: list[str], message: str) -> None:
    problems.append(message)


def classify(receives, receive_order, acquires, submitted, presents, problems, quiet) -> None:
    """PLAN 4.8's four-state partition, tested in the declared precedence order.

    Precedence rather than four independently worded conditions is what makes it one state per
    sample: an acquisition row settles states 3 and 4 permanently, because once a frame has held a
    sample it was never merely superseded, whatever was published afterwards.

    This is a post-run pass and never live state. The game thread holds a snapshot from acquire
    until submit, and a newer sample arriving inside that interval would look like supersession of
    a sample the renderer is already drawing.
    """
    # Which frames held each sample, and which frames rendered it.
    held: dict[int, list[int]] = {}
    for frame, samples in acquires.items():
        for sample in samples:
            held.setdefault(sample, []).append(frame)
    rendered: dict[int, list[int]] = {}
    # A signal bound to a visible component appears in every present row; one that is not appears
    # in none. That is the only way to tell "never reached the screen" from "nothing draws it".
    bound_signals: set[int] = set()
    for frame, (entries, _when) in presents.items():
        for entry in entries:
            bound_signals.add(entry["signal"])
            if entry["sample"] is not None:
                rendered.setdefault(entry["sample"], []).append(frame)

    # Criterion: every id in an acquisition or present row resolves to a receive row.
    for source, table in (("acquire", held), ("present", rendered)):
        for sample in table:
            if sample not in receives:
                problems.append(f"{source} row names sample {sample}, which has no receive row")

    # The newest sample per signal at the end of the run, for state 4.
    newest: dict[int, int] = {}
    for sample in receive_order:
        newest[receives[sample]["signal"]] = sample

    states: dict[int, str] = {}
    reasons: dict[str, int] = {}
    for sample in receive_order:
        signal = receives[sample]["signal"]
        if sample in rendered:
            states[sample] = "presented"
        elif sample in held:
            states[sample] = "acquired-not-displayed"
            # No submit row first. It is a fact the rows state outright, while "bound to no
            # visible component" is INFERRED from a signal appearing in no present row, and a run
            # with no present rows at all makes every signal look unbound. A suppressed-submit
            # run is exactly that run.
            if not any(frame in submitted for frame in held[sample]):
                reason = "no submit row"
            elif signal not in bound_signals:
                reason = "bound to no visible component"
            else:
                reason = "submitted, the run ended before it presented"
            reasons[reason] = reasons.get(reason, 0) + 1
        elif newest.get(signal) != sample:
            states[sample] = "superseded-unacquired"
        else:
            states[sample] = "latest-unacquired"

    counts: dict[str, int] = {}
    for state in states.values():
        counts[state] = counts.get(state, 0) + 1
    total = sum(counts.values())
    if total != len(receive_order):
        problems.append(f"the four states cover {total} samples, not the {len(receive_order)} published")

    # Per signal too, which is where a partition that only sums in aggregate falls over.
    per_signal: dict[int, int] = {}
    for sample in receive_order:
        per_signal[receives[sample]["signal"]] = per_signal.get(receives[sample]["signal"], 0) + 1
    for signal, published in per_signal.items():
        assigned = sum(1 for s in receive_order if receives[s]["signal"] == signal and s in states)
        if assigned != published:
            problems.append(f"signal {signal}: {assigned} states for {published} published samples")

    # One observation per presented sample, at its EARLIEST presentation. Later frames showing the
    # same sample create no further observation, so the figure measures arrival latency rather
    # than how long something stayed on screen.
    observations = 0
    repeated = 0
    for sample, frames in rendered.items():
        observations += 1
        if len(frames) > 1:
            repeated += 1

    if not quiet:
        print("lifecycle")
        for state in ("presented", "acquired-not-displayed", "superseded-unacquired", "latest-unacquired"):
            print(f"  {state:<24} {counts.get(state, 0):>8}")
        print(f"  {'sum':<24} {total:>8} of {len(receive_order)} published")
        for reason, count in sorted(reasons.items()):
            print(f"    acquired-not-displayed: {reason}: {count}")
        print(f"  receive-to-present observations {observations}, "
              f"{repeated} of them displayed across more than one frame")


def expiries(receives, armed, statuses, presents, problems, quiet) -> None:
    """PLAN 4.8's expiry rows and expiry-to-present latency.

    The start endpoint is a `fired` status row and nothing else. A cancelled expiry yields no
    observation, so a deadline moved by a later arrival cannot produce a fictitious interval. The
    end endpoint is matched on the expiry identifier rather than on the signal, or a later
    episode's frame would close an earlier expiry.
    """
    if not armed and not statuses:
        if not quiet:
            print("expiries  none armed in this run")
        return

    # Exactly one status row per arming, which is a property of ExpirySchedule and is checked
    # here against a real run rather than trusted.
    unresolved = []
    for serial, row in armed.items():
        rows = statuses.get(serial, [])
        if len(rows) > 1:
            problems.append(f"expiry {serial} has {len(rows)} status rows, not one")
        elif not rows:
            # After a hard kill this is expected: the arming was flushed and its status was not.
            unresolved.append(serial)
    for serial in statuses:
        if serial not in armed:
            problems.append(f"expiry {serial} has a status row and no armed row")

    # PLAN's own clause: an armed expiry names a signal with a receive row and a deadline later
    # than that row's receive timestamp.
    by_signal: dict[int, list[int]] = {}
    for sample, row in receives.items():
        by_signal.setdefault(row["signal"], []).append(sample)
    for serial, row in armed.items():
        # Rule expiries name a rule, not a signal, and are never latency endpoints.
        if row["kind"] != 0:
            continue
        if row["signal"] not in by_signal:
            problems.append(f"expiry {serial} names signal {row['signal']}, which has no receive row")
            continue
        source = receives.get(row["sample"])
        if source is None:
            problems.append(f"expiry {serial} was armed by sample {row['sample']}, which has no receive row")
        elif row["t_armed"] <= source["t_recv"]:
            problems.append(f"expiry {serial} has a deadline at or before its sample's receive time")

    # The FIRST frame that displays a firing is the end endpoint, so the scan is in frame order
    # and the first hit per expiry wins.
    shown: dict[int, tuple[int, int]] = {}
    for frame in sorted(presents.keys()):
        entries, when = presents[frame]
        for entry in entries:
            serial = entry.get("expiry")
            if serial and entry.get("quality") == STALE and serial not in shown:
                shown[serial] = (frame, when)

    observations = []
    not_displayed = 0
    fired = 0
    cancelled_by_reason: dict[str, int] = {}
    for serial, rows in statuses.items():
        row = rows[0]
        if row["status"] == "cancelled":
            reason = CANCEL_REASONS.get(row.get("reason"), str(row.get("reason")))
            cancelled_by_reason[reason] = cancelled_by_reason.get(reason, 0) + 1
            if serial in shown:
                problems.append(f"expiry {serial} was cancelled and a present row displays it")
            continue
        if row["status"] != "fired":
            problems.append(f"expiry {serial} has status {row['status']!r}")
            continue
        if armed.get(serial, {}).get("kind") != 0:
            # Rule expiries fire and are never latency endpoints.
            continue
        fired += 1
        if row.get("t_fired") is None:
            problems.append(f"expiry {serial} fired with no timestamp")
            continue
        seen = shown.get(serial)
        if seen is None:
            # Decided over the whole trace with no cutoff at the next valid sample: a frame that
            # acquired the stale snapshot before recovery still presents it afterwards, and that
            # delayed presentation is exactly the latency being measured.
            not_displayed += 1
            continue
        frame, when = seen
        latency = (when - row["t_fired"]) / 1e6
        if latency < 0:
            problems.append(f"expiry {serial} was displayed before it fired")
        observations.append(latency)

    if not quiet:
        print("expiries")
        print(f"  armed                    {len(armed):>8}")
        print(f"  fired                    {fired:>8}")
        for reason, count in sorted(cancelled_by_reason.items()):
            print(f"  cancelled, {reason:<14}{count:>8}")
        print(f"  unresolved               {len(unresolved):>8}")
        print(f"  displayed firings        {len(observations):>8}")
        print(f"  fired-not-displayed      {not_displayed:>8}")
        if observations:
            ordered = sorted(observations)
            index = min(len(ordered) - 1, int(round(0.95 * (len(ordered) - 1))))
            print(f"  expiry-to-present        median {ordered[len(ordered) // 2]:.2f} ms, "
                  f"p95 {ordered[index]:.2f} ms")


def positive(value):
    return (type(value) is int and 0 < value <= 2**64 - 1) or (type(value) is float and math.isfinite(value) and value > 0)


def row_values(record, problems, number):
    """Reject malformed join keys before they can turn a diagnostic into a Python exception."""
    kind = record["type"]
    invalid = []
    for key in ("frame", "sample", "signal", "expiry", "seq", "generation", "quality", "age_evidence"):
        if key in record and (type(record[key]) is not int or record[key] < 0):
            invalid.append(key)
    if kind == "acquire" and (not isinstance(record["samples"], list) or
            any(type(x) is not int or x <= 0 for x in record["samples"])):
        invalid.append("samples")
    if kind == "present":
        entries = record["signals"]
        if not isinstance(entries, list):
            invalid.append("signals")
        else:
            for entry in entries:
                if (not isinstance(entry, dict) or
                        set(entry) != {"signal", "sample", "quality", "expiry", "derived_from"} or
                        type(entry["signal"]) is not int or entry["signal"] < 0 or
                        type(entry["quality"]) is not int or entry["quality"] not in range(4) or
                        any(entry[k] is not None and (type(entry[k]) is not int or entry[k] <= 0)
                            for k in ("sample", "expiry", "derived_from"))):
                    invalid.append("signals entry")
    if kind in ("expiry_armed", "expiry_status"):
        if type(record["kind"]) is not int or record["kind"] not in (0, 1, 2):
            invalid.append("kind (expected freshness=0, hold_last=1 or debounce=2)")
    if kind == "expiry_status":
        if record["status"] not in ("fired", "cancelled"):
            invalid.append("status")
        if record["status"] == "cancelled" and record["t_fired"] is not None:
            invalid.append("t_fired (must be null on cancellation)")
    if invalid:
        problems.append(f"line {number}: {kind} invalid {', '.join(invalid)}")
    return not invalid


def check_chunk20(rows, presents, armed, statuses, header, arguments, problems):
    imports = rows.get("asset_import", [])
    assets = set()
    for row in imports:
        name = row["asset"]
        if not isinstance(name, str) or not name:
            problems.append("asset import needs a package-relative identifier")
            continue
        if name in assets:
            problems.append(f"asset {name!r} has more than one import row")
        assets.add(name)
        if name.startswith(("/", "\\")) or ".." in name.split("/") or ":" in name:
            problems.append(f"asset {name!r} is not package-relative")
        for column in ("bytes", "width", "height"):
            if type(row[column]) is not int or row[column] <= 0:
                problems.append(f"asset {name!r}: {column} must be a positive integer")
        stamps = [row[c] for c in ("t_decode_start", "t_decode_end", "t_upload_start", "t_first_usable")]
        if all(positive(t) for t in stamps) and not (stamps[0] <= stamps[1] <= stamps[2] < stamps[3]):
            problems.append(f"asset {name!r}: decode, upload and completion are out of order")
    if arguments.expect_assets:
        wanted = set(arguments.expect_assets.split(","))
        if wanted != assets:
            problems.append(f"asset imports differ: missing {sorted(wanted-assets)}, extra {sorted(assets-wanted)}")

    first = rows.get("first_usable", [])
    launches = rows.get("launch", [])
    valid = [(when, frame) for frame, (entries, when) in presents.items()
             if positive(when) and any(entry["quality"] == 0 for entry in entries)]
    if "first_usable" in arguments.declared_types and valid and len(first) != 1:
        problems.append(f"expected one first_usable record, got {len(first)}")
    if len(first) > 1 or (first and not valid):
        problems.append("first_usable must identify exactly the earliest valid presentation")
    if first and valid:
        when, frame = min(valid)
        if first[0]["frame"] != frame or first[0]["t_present"] != when:
            problems.append("first_usable does not name the earliest present row with rendered quality 0")
    if len(launches) != len(first):
        problems.append("launch and first_usable records must occur together, once")
    uncertainty = header.get("uncertainty", {})
    if not isinstance(uncertainty, dict):
        uncertainty = {}
    for launch in launches:
        for column in ("first_usable_counter", "counter_frequency"):
            if type(launch[column]) is not int or launch[column] <= 0:
                problems.append(f"launch.{column} must be a positive counter value")
        for column in ("launch_counter", "process_start_ticks", "process_ticks_per_second", "first_usable_boot_ns"):
            if launch[column] is not None and (type(launch[column]) is not int or launch[column] <= 0):
                problems.append(f"launch.{column} must be positive when measured")
        start, end, frequency = (launch[k] for k in ("launch_counter", "first_usable_counter", "counter_frequency"))
        if all(positive(v) for v in (start, end, frequency)):
            seconds = (end - start) / frequency
            if seconds <= 0:
                problems.append("launch-to-first-usable must be positive")
            if not arguments.quiet:
                print(f"launch   {seconds:.6f}s from launch request; under 30s: {seconds < 30}")
        elif arguments.launch_required and "windows" in str(header.get("platform", "")).lower():
            problems.append("Windows launch measurement has no launcher counter")
        if "windows" in str(header.get("platform", "")).lower():
            if not uncertainty.get("windows"):
                problems.append("header has no Windows launch uncertainty text")
            if arguments.launch_required and not positive(uncertainty.get("windows_launch_overhead_counter")):
                problems.append("Windows launch uncertainty was not measured")
        if launch["am_start_output"] is not None and not isinstance(launch["am_start_output"], str):
            problems.append("launch.am_start_output must be text or null")
        if launch["am_launch_state"] is not None and launch["am_launch_state"] not in ("COLD", "WARM", "HOT"):
            problems.append("launch.am_launch_state must be COLD, WARM, HOT or null")
        if "android" in str(header.get("platform", "")).lower():
            ticks, tick_hz, boot_ns = (launch[k] for k in
                                      ("process_start_ticks", "process_ticks_per_second", "first_usable_boot_ns"))
            if all(positive(v) for v in (ticks, tick_hz, boot_ns)):
                seconds = boot_ns / 1e9 - ticks / tick_hz
                if seconds <= 0:
                    problems.append("Android first usable must follow process start on the boot-time clock")
                if not arguments.quiet:
                    print(f"launch   {seconds:.6f}s from process start; tick granularity {1/tick_hz:.6f}s")
            if not uncertainty.get("android"):
                problems.append("header has no Android launch uncertainty text")
            if arguments.launch_required:
                for column in ("process_start_ticks", "process_ticks_per_second", "first_usable_boot_ns"):
                    if not positive(launch[column]): problems.append(f"Android launch missing {column}")
                if not launch["am_start_output"] or launch["am_launch_state"] not in ("COLD", "WARM", "HOT"):
                    problems.append("Android launch missing am start -W output or launch state")
                if not positive(uncertainty.get("android_tick_ns")):
                    problems.append("Android uncertainty has no process-start tick granularity")
                spread = uncertainty.get("android_am_spread_ns")
                if type(spread) not in (int, float) or not math.isfinite(spread) or spread < 0:
                    problems.append("Android uncertainty has no observed spread against am start -W")
    if arguments.launch_required and not launches:
        problems.append("no launch measurement")

    declared = header.get("declared_rules", [])
    rule_ids = set()
    for rule in declared if isinstance(declared, list) else []:
        if isinstance(rule, dict) and type(rule.get("id")) is int and isinstance(rule.get("name"), str):
            if arguments.document_rules is None or rule["name"] not in arguments.document_rules:
                problems.append(f"rule {rule['name']!r} is not proven declared in the loaded document; supply --document")
            else:
                rule_ids.add(rule["id"])
    for serial, row in armed.items():
        matched = statuses.get(serial, [])
        if row["kind"] in (1, 2):
            if row["signal"] not in rule_ids:
                problems.append(f"rule expiry {serial} names undeclared rule {row['signal']}")
            if len(matched) != 1:
                problems.append(f"rule expiry {serial} has {len(matched)} status rows, not exactly one")
        if arguments.orderly and len(matched) != 1:
            problems.append(f"orderly run: expiry {serial} has {len(matched)} status rows, not exactly one")
        for status in matched:
            if status["kind"] != row["kind"]:
                problems.append(f"expiry {serial} status kind differs from arming")
            if status["status"] == "fired" and positive(status["t_fired"]) and positive(row["t_armed"]):
                if status["t_fired"] < row["t_armed"]:
                    problems.append(f"expiry {serial} fired before its deadline")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--min-seconds", type=float, default=None,
                        help="fail unless the log spans at least this many seconds")
    parser.add_argument("--expect-types", default="",
                        help="comma separated record types that must each appear at least once")
    parser.add_argument("--cadence", default="",
                        help="TYPE=SECONDS[,TYPE=SECONDS]: the mean interval between rows of that "
                             "type, measured from its own timestamp column, must be within "
                             "--cadence-tolerance of SECONDS")
    parser.add_argument("--cadence-tolerance", type=float, default=0.01)
    parser.add_argument("--lifecycle", action="store_true",
                        help="classify every published sample into PLAN 4.8's four states and "
                             "check that the four counts sum to the published count")
    parser.add_argument("--survived-kill", action="store_true",
                        help="require 29 seconds in each of frame, sampled, receive, acquire, submit and present")
    parser.add_argument("--all-types", action="store_true", help="require a row of every declared record type")
    parser.add_argument("--orderly", action="store_true", help="require one status for every armed expiry")
    parser.add_argument("--launch-required", action="store_true", help="require measured launch endpoints and uncertainty")
    parser.add_argument("--expect-assets", default="", help="comma separated package-relative imported asset identifiers")
    parser.add_argument("--document", type=Path, help="loaded dashboard.json, used to verify declared rule names")
    parser.add_argument("--quiet", action="store_true")
    arguments = parser.parse_args()

    if not arguments.log.exists():
        print(f"No such log: {arguments.log}")
        return 2
    try:
        raw = arguments.log.read_bytes()
        arguments.document_rules = None
        if arguments.document:
            document = json.loads(arguments.document.read_text(encoding="utf-8-sig"))
            arguments.document_rules = document.get("dashboard", document).get("rules", {})
            if not isinstance(arguments.document_rules, dict):
                print("Document rules must be an object")
                return 2
    except (OSError, ValueError, AttributeError) as error:
        print(f"Cannot read input: {error}")
        return 2
    if not raw:
        print(f"Empty log: {arguments.log}")
        return 2

    text = raw.decode("utf-8", errors="replace")
    lines = text.split("\n")
    # A kill can land inside a write, so at most one trailing line may be incomplete. Two would
    # mean something worse than a kill happened.
    trailing_partial = 0
    if lines and lines[-1] == "":
        lines.pop()
    elif lines:
        try:
            json.loads(lines[-1])
        except json.JSONDecodeError:
            trailing_partial = 1
            lines.pop()

    problems: list[str] = []
    schema: dict[str, list[str]] = {}
    header: dict | None = None
    counts: dict[str, int] = {}
    # Per type, so two record types written by two threads do not have to interleave in order.
    last_time: dict[tuple[str, str], float] = {}
    spans: dict[str, tuple[float, float]] = {}
    # The primary timestamp per type, used for cadence. A stream that wrote the right NUMBER of
    # rows at the wrong rate passes a row count and fails this.
    primary: dict[str, list[float]] = {}

    rows: dict[str, list[dict]] = {}
    # Semantic joins are validated even when lifecycle reporting is not requested.
    receives: dict[int, dict] = {}
    receive_order: list[int] = []
    acquires: dict[int, list[int]] = {}
    submitted: set[int] = set()
    presents: dict[int, list[dict]] = {}
    armed: dict[int, dict] = {}
    statuses: dict[int, list[dict]] = {}

    for number, line in enumerate(lines, start=1):
        if not line.strip():
            fail(problems, f"line {number}: blank")
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError as error:
            fail(problems, f"line {number}: not JSON ({error.msg})")
            continue
        if not isinstance(record, dict):
            fail(problems, f"line {number}: not an object")
            continue
        kind = record.get("type")
        if not isinstance(kind, str):
            fail(problems, f"line {number}: no type field")
            continue
        counts[kind] = counts.get(kind, 0) + 1

        if kind == "schema":
            if number != 1:
                fail(problems, f"line {number}: the schema record must be first")
            records = record.get("records")
            if not isinstance(records, dict):
                fail(problems, f"line {number}: the schema record declares no records object")
                continue
            for name, columns in records.items():
                if not isinstance(columns, list) or not all(isinstance(c, str) for c in columns):
                    fail(problems, f"line {number}: schema for {name} is not a list of column names")
                    continue
                if len(columns) != len(set(columns)):
                    fail(problems, f"line {number}: schema for {name} has duplicate columns")
                if name in COLUMNS and columns != COLUMNS[name].split():
                    fail(problems, f"line {number}: schema for {name} differs from its frozen columns")
                schema[name] = columns
            continue
        if kind == "header":
            if number != 2:
                fail(problems, f"line {number}: the header record must be second")
            header = record
            continue

        if kind not in schema:
            fail(problems, f"line {number}: record type {kind!r} is not in the declared schema")
            continue
        expected = set(schema[kind]) | IMPLICIT
        actual = set(record.keys())
        if actual != expected:
            missing = sorted(expected - actual)
            extra = sorted(actual - expected)
            fail(problems, f"line {number}: {kind} columns differ; missing {missing}, unexpected {extra}")
            continue

        if kind in COLUMNS and not set(COLUMNS[kind].split()).issubset(record):
            continue
        if not row_values(record, problems, number):
            continue
        rows.setdefault(kind, []).append(record)
        if kind == "receive":
            if record["sample"] in receives:
                fail(problems, f"line {number}: duplicate receive sample")
            receives[record["sample"]] = record
            receive_order.append(record["sample"])
        elif kind == "acquire":
            acquires[record["frame"]] = record["samples"]
        elif kind == "submit":
            submitted.add(record["frame"])
        elif kind == "present":
            if record["frame"] in presents:
                fail(problems, f"line {number}: duplicate present frame")
            presents[record["frame"]] = (record["signals"], record["t_present"])
        elif kind == "expiry_armed":
            if record["expiry"] in armed:
                fail(problems, f"line {number}: duplicate expiry arming")
            armed[record["expiry"]] = record
        elif kind == "expiry_status":
            statuses.setdefault(record["expiry"], []).append(record)

        for column in schema[kind]:
            if column not in TIMESTAMPS | NULLABLE_TIMESTAMPS and not column.startswith("t_"):
                continue
            value = record[column]
            if column == "t_fired" and record.get("status") == "cancelled" and value is None:
                continue
            if not positive(value):
                fail(problems, f"line {number}: {kind}.{column} is not finite and positive")
                continue
            key = (kind, column)
            if key in last_time and value < last_time[key]:
                fail(problems, f"line {number}: {kind}.{column} went backwards")
            last_time[key] = value
            primary_column = PRIMARY.get(kind, next((c for c in schema[kind] if c.startswith("t_")), None))
            if column == primary_column:
                primary.setdefault(kind, []).append(value)
                first, _ = spans.get(kind, (value, value))
                spans[kind] = (first, value)

    if not schema:
        fail(problems, "the log declares no schema")
    if header is None:
        fail(problems, "the log has no header record")
    if trailing_partial > 1:
        fail(problems, "more than one trailing partial line")

    arguments.declared_types = schema
    wanted_types = set(t for t in arguments.expect_types.split(",") if t)
    if arguments.all_types:
        wanted_types.update(schema)
        wanted_types.update(COLUMNS)
    for kind in wanted_types:
        if counts.get(kind, 0) == 0:
            fail(problems, f"no rows of type {kind!r}")

    for entry in [c for c in arguments.cadence.split(",") if c]:
        name, _, wanted = entry.partition("=")
        stamps = primary.get(name, [])
        if len(stamps) < 2:
            fail(problems, f"cannot measure the cadence of {name!r}: {len(stamps)} timestamped rows")
            continue
        mean = (stamps[-1] - stamps[0]) / 1e9 / (len(stamps) - 1)
        try:
            target = float(wanted)
        except ValueError:
            fail(problems, f"invalid cadence target {entry!r}")
            continue
        if not positive(target):
            fail(problems, f"invalid cadence target {entry!r}")
            continue
        if abs(mean - target) > target * arguments.cadence_tolerance:
            fail(problems, f"{name} rows are {mean:.4f}s apart, not {target:.4f}s "
                           f"within {arguments.cadence_tolerance:.1%}")
        elif not arguments.quiet:
            print(f"cadence  {name} {mean:.4f}s between rows, target {target:.4f}s")

    classify(receives, receive_order, acquires, submitted, presents, problems, arguments.quiet or not arguments.lifecycle)
    # Timestamp failures are already diagnosed. Avoid arithmetic on malformed endpoints in joins.
    endpoints_ok = all(positive(r["t_recv"]) for r in receives.values()) and all(
        positive(r["t_armed"]) for r in armed.values()) and all(positive(t) for _, t in presents.values())
    endpoints_ok = endpoints_ok and all(r["status"] == "cancelled" or positive(r["t_fired"])
                                       for group in statuses.values() for r in group)
    if endpoints_ok:
        expiries(receives, armed, statuses, presents, problems, arguments.quiet)
    check_chunk20(rows, presents, armed, statuses, header or {}, arguments, problems)
    if arguments.survived_kill:
        for kind in ("frame", "sampled", "receive", "acquire", "submit", "present"):
            stamps = primary.get(kind, [])
            span = (stamps[-1] - stamps[0]) / 1e9 if len(stamps) > 1 else 0
            if span < 29:
                fail(problems, f"killed run: {kind} spans {span:.3f}s, needs at least 29s")

    elapsed = 0.0
    for kind, (first, last) in spans.items():
        elapsed = max(elapsed, (last - first) / 1e9)
    if arguments.min_seconds is not None and elapsed < arguments.min_seconds:
        fail(problems, f"the log spans {elapsed:.2f}s, needs at least {arguments.min_seconds:.2f}s")

    if not arguments.quiet:
        print(f"log      {arguments.log}")
        print(f"bytes    {len(raw)}")
        print(f"elapsed  {elapsed:.2f}s")
        if elapsed > 0:
            print(f"rate     {len(raw) / elapsed:.0f} bytes/s, {len(raw) / elapsed * 3600 * 8 / 1e9:.2f} GB per 8 hours")
        for kind in sorted(counts):
            per_second = counts[kind] / elapsed if elapsed > 0 else 0.0
            print(f"  {kind:<10} {counts[kind]:>8} rows  {per_second:>8.2f}/s")
        if trailing_partial:
            print("  one trailing partial line, dropped; a kill inside a write looks like this")
        for entry in problems:
            print(f"FAIL {entry}")

    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
