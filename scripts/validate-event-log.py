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
import sys
from pathlib import Path

# Columns every record carries in addition to its declared ones.
IMPLICIT = {"type"}
# Timestamp columns, checked for presence, positivity and monotonicity per record type. Named
# rather than inferred from a prefix, so a column added later is a deliberate decision.
TIMESTAMPS = {"t", "t_start", "t_present", "t_recv", "t_armed", "t_fired"}


def fail(problems: list[str], message: str) -> None:
    problems.append(message)


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
    parser.add_argument("--quiet", action="store_true")
    arguments = parser.parse_args()

    if not arguments.log.exists():
        print(f"No such log: {arguments.log}")
        return 2
    raw = arguments.log.read_bytes()
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

        for column in schema[kind]:
            if column not in TIMESTAMPS:
                continue
            value = record[column]
            if not isinstance(value, (int, float)) or isinstance(value, bool):
                fail(problems, f"line {number}: {kind}.{column} is not a number")
                continue
            if value <= 0:
                fail(problems, f"line {number}: {kind}.{column} is not positive")
                continue
            key = (kind, column)
            if key in last_time and value < last_time[key]:
                fail(problems, f"line {number}: {kind}.{column} went backwards")
            last_time[key] = value
            first, _ = spans.get(kind, (value, value))
            spans[kind] = (first, value)
            if column == schema[kind][0] or (kind not in primary and column in TIMESTAMPS):
                primary.setdefault(kind, []).append(value)

    if not schema:
        fail(problems, "the log declares no schema")
    if header is None:
        fail(problems, "the log has no header record")
    if trailing_partial > 1:
        fail(problems, "more than one trailing partial line")

    for kind in [t for t in arguments.expect_types.split(",") if t]:
        if counts.get(kind, 0) == 0:
            fail(problems, f"no rows of type {kind!r}")

    for entry in [c for c in arguments.cadence.split(",") if c]:
        name, _, wanted = entry.partition("=")
        stamps = primary.get(name, [])
        if len(stamps) < 2:
            fail(problems, f"cannot measure the cadence of {name!r}: {len(stamps)} timestamped rows")
            continue
        mean = (stamps[-1] - stamps[0]) / 1e9 / (len(stamps) - 1)
        target = float(wanted)
        if abs(mean - target) > target * arguments.cadence_tolerance:
            fail(problems, f"{name} rows are {mean:.4f}s apart, not {target:.4f}s "
                           f"within {arguments.cadence_tolerance:.1%}")
        elif not arguments.quiet:
            print(f"cadence  {name} {mean:.4f}s between rows, target {target:.4f}s")

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
