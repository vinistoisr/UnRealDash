"""Compare synthetic binary telemetry records with the owner's reference decoder."""
import argparse
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys


def converter_module():
    spec = importlib.util.spec_from_file_location("schema_converter", Path(__file__).with_name("convert-existing-schema.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def boundary_records(pack):
    """Exercise each field independently; retain one record for empty frames."""
    records = []
    for frame in pack["frames"]:
        payloads = {bytes(8)}
        for field in frame["signals"]:
            width = {"uint8": 1, "uint16": 2, "uint32": 4}[field["type"]]
            bits = width * 8
            # Includes signed min/max, unsigned min/max, and both sign-boundary words.
            values = {0, (1 << bits) - 1, (1 << (bits - 1)) - 1, 1 << (bits - 1)}
            for raw in values:
                payload = bytearray(8)
                start = field["byte_offset"]
                payload[start:start + width] = raw.to_bytes(width, field["byte_order"])
                payloads.add(bytes(payload))
        for payload in sorted(payloads):
            wire = bytes.fromhex("44332211") + frame["id"].to_bytes(4, "little") + payload
            records.append({"frame_id": frame["id"], "bytes": wire.hex()})
    return records


def encode_jsonl(rows):
    return "".join(json.dumps(row, sort_keys=True, allow_nan=False) + "\n" for row in rows)


def decode_jsonl(text):
    return [json.loads(line) for line in text.splitlines() if line.strip()]


def compare(pack, root, reference, executable, pack_path, xml_path):
    converter = converter_module()
    expected_pack, notes = converter.convert(root, reference, pack["id"], pack["version"])
    if pack != expected_pack:
        raise ValueError("pack differs from the owner's schema XML conversion; identifier/field coverage gate failed")
    records = boundary_records(pack)
    if {r["frame_id"] for r in records} != {f["id"] for f in pack["frames"]}:
        raise ValueError("synthetic identifier coverage gate failed")
    result = subprocess.run([str(Path(executable).resolve()), "--pack", str(pack_path)], input=encode_jsonl(records), text=True, capture_output=True, check=False)
    if result.returncode:
        raise ValueError(f"C++ decoder exited {result.returncode}: {result.stderr}")
    rows = decode_jsonl(result.stdout)
    counters = json.loads(result.stderr.strip())
    actual = {}
    for row in rows:
        key = (row["stream_offset"], row["frame_id"], row["field"])
        if key in actual:
            raise ValueError("duplicate C++ output row")
        actual[key] = row
    schema = reference.load(xml_path)
    definitions = {f["id"]: f for f in pack["frames"]}
    if set(schema.frames) != set(definitions):
        raise ValueError("reference and pack identifier sets differ")
    report = ["# Differential telemetry check", "", "Source: the owner's schema XML and reference decoder, imported by absolute path without copying either input.",
              "Physical values remain in declared units. No SI normalization is performed.",
              "Tolerance: abs(a-b) <= max(1e-9, 1e-9 * max(abs(a), abs(b))).",
              "The reference decoder does not return units, quality or age evidence. These columns are checked against the converted declarations.",
              "The C++ tool appends a four-byte tag delimiter to confirm a single-record stream, then calls EndOfStream.", "",
              "| Identifier | Field | Record | Raw | Reference | Parser | Unit | Result and cause |",
              "| --- | --- | ---: | ---: | ---: | ---: | --- | --- |"]
    mismatches = 0
    reference_fields = 0
    visited = set()
    for index, record in enumerate(records):
        fid = record["frame_id"]
        payload = bytes.fromhex(record["bytes"])[8:]
        decoded = schema.decode(fid, payload)
        fields = definitions[fid]["signals"]
        if decoded is None or set(decoded) != {f["name"] for f in fields}:
            raise ValueError(f"reference field coverage differs at 0x{fid:X}")
        for field in fields:
            name = field["name"]
            visited.add((fid, name))
            reference_fields += 1
            row = actual.pop((index * 16, fid, name), None)
            expected = decoded[name]
            width = {"uint8": 1, "uint16": 2, "uint32": 4}[field["type"]]
            raw = int.from_bytes(payload[field["byte_offset"]:field["byte_offset"]+width], field["byte_order"], signed=field["signed"])
            cause = []
            if row is None:
                cause.append("missing parser output")
            else:
                value = row["physical"]
                if value is None or not math.isfinite(value) or abs(expected-value) > max(1e-9, 1e-9*max(abs(expected), abs(value))):
                    cause.append("affine value differs")
                if row["raw"] != raw: cause.append("raw signedness or width differs")
                if row["unit"] != field["unit"]: cause.append("declared unit differs")
                if row["quality"] != "valid": cause.append("quality differs")
                if row["age_evidence"] != ("unknown" if field["acquisition"] == "held" else "measured"): cause.append("age evidence differs")
            mismatches += bool(cause)
            verdict = "MISMATCH: " + "; ".join(cause) if cause else "PASS"
            report.append(f"| 0x{fid:X} | {name} | {index} | {raw} | {expected:.17g} | {row['physical'] if row else 'missing'} | {field['unit']} | {verdict} |")
    if actual:
        raise ValueError("unexpected parser outputs; output coverage gate failed")
    if visited != {(f["id"], v["name"]) for f in pack["frames"] for v in f["signals"]}:
        raise ValueError("synthetic field coverage gate failed")
    report += ["", "## Identifiers with no comparable output", ""]
    report += ["- " + note + "; reference produces no output by design; parser enumeration excludes this identifier." for note in notes if note.startswith("skipped")]
    for frame in pack["frames"]:
        if not frame["signals"]:
            report.append(f"- 0x{frame['id']:X}: all fields are display-only; reference returns an empty mapping; parser accepts the frame and publishes no samples.")
    report += ["", "## A12 unconfirmed unit conventions", "",
               "These conventions are listed rather than converted. Matching decoders cannot confirm a shared physical convention.", ""]
    for frame in pack["frames"]:
        for field in frame["signals"]:
            name = field["name"]
            pressure = field["unit"] in ("Pa", "kPa", "bar", "psi")
            temperature = field["unit"] in ("K", "degC", "degF")
            if pressure or temperature:
                convention = "absolute versus gauge pressure and declared unit need owner confirmation" if pressure else "temperature unit and scaling need owner confirmation"
                report.append(f"- {name}: {field['unit']}; {convention}.")
    report += ["", "## Counters and gate", "", f"Synthetic records: {len(records)}.", f"Reference decode calls: {len(records)}; output fields: {reference_fields}; unknown identifiers: 0.",
               "Reference module exposes no native counters; the preceding counts are wrapper observations.",
               "Parser counters: `" + json.dumps(counters, sort_keys=True) + "`.",
               f"Mismatches: {mismatches}. Unexplained mismatches: {mismatches}.",
               "Status fields are compared as ordinary decoded values. Every difference fails the gate."]
    if counters["frames_emitted"] != len(records) or counters["samples_published"] != reference_fields:
        raise ValueError("C++ counter coverage gate failed")
    return "\n".join(report) + "\n", mismatches, mismatches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("xml", "reference-decoder", "pack", "decoder-exe", "report"):
        parser.add_argument("--" + option, required=True)
    args = parser.parse_args()
    try:
        converter = converter_module()
        reference = converter.load_reference(args.reference_decoder)
        report, mismatches, unexplained = compare(json.loads(Path(args.pack).read_text(encoding="utf-8")), converter.read_xml(args.xml), reference, args.decoder_exe, args.pack, args.xml)
        Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        Path(args.report).write_text(report, encoding="utf-8", newline="\n")
        print(f"Differential check: mismatches={mismatches}, unexplained_mismatches={unexplained}")
        print(f"Report: {args.report}")
        return int(bool(unexplained))
    except (ValueError, KeyError, OSError) as error:
        print(f"Differential check failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
