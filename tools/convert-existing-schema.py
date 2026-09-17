"""Offline, deterministic conversion of the owner's schema XML."""
import argparse
from datetime import datetime, timezone
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ACQUISITION = {}  # Awaiting the owner's held-channel list.
UNITS = {
    "": "dimensionless", "K": "K", "C": "degC", "\u00b0C": "degC",
    "degC": "degC", "F": "degF", "\u00b0F": "degF", "degF": "degF",
    "Pa": "Pa", "kPa": "kPa", "kPA": "kPa", "bar": "bar", "psi": "psi",
    "m/s": "m/s", "km/h": "km/h", "kmh": "km/h", "mph": "mph",
    "rpm": "rpm", "rad/s": "rad/s", "dimensionless": "dimensionless",
}


def load_reference(path):
    path = Path(path)
    if not path.is_absolute():
        raise ValueError("reference decoder path must be absolute")
    spec = importlib.util.spec_from_file_location("owner_reference", path)
    if spec is None or spec.loader is None:
        raise ValueError("cannot load reference decoder")
    module = importlib.util.module_from_spec(spec)
    # Read-only owner inputs: importing must not create a cache beside the module.
    previous = sys.dont_write_bytecode
    try:
        sys.dont_write_bytecode = True
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    for attribute in ("_DISPLAY_ONLY", "_TARGETID_NAMES"):
        if not hasattr(module, attribute):
            raise ValueError(f"reference decoder missing {attribute}")
    return module


def read_xml(path):
    return ET.fromstring(re.sub(r"<!--.*?-->", "", Path(path).read_text(encoding="utf-8"), flags=re.S))


def encode_pack(pack):
    return (json.dumps(pack, sort_keys=True, indent=2, ensure_ascii=True, allow_nan=False) + "\n").encode("utf-8")


def convert(root, reference, pack_id, version):
    pack = dict(id=pack_id, version=version, api=1, input="binary-telemetry-v1", frames=[])
    notes = []
    names, ids = set(), set()
    for frame in root.iter("frame"):
        fid = int(frame.attrib["id"], 0)
        label = f"0x{fid:X}"
        if fid in ids:
            raise ValueError(f"duplicate frame {label}")
        ids.add(fid)
        if "timeout" in frame.attrib:
            notes.append(f"timeout {label}: {frame.attrib['timeout']}")
        if "writeInterval" in frame.attrib or fid in reference._DISPLAY_ONLY:
            reason = "write-direction" if "writeInterval" in frame.attrib else "display-only frame"
            notes.append(f"skipped {label}: {reason}")
            continue
        for key in ("endianness", "endianess"):
            if key in frame.attrib and frame.attrib[key] != "little":
                raise ValueError(f"frame {label}: {key} must be little")
        length = int(frame.get("size", "8"))
        if length != 8:
            raise ValueError(f"frame {label}: payload length must be 8")
        output = dict(id=fid, id_format="extended", length=length,
                      role="status" if fid == 0xC82 else "telemetry", signals=[])
        for value in frame.iter("value"):
            if value.get("displayOnly") == "true":
                continue
            if "name" in value.attrib:
                name = value.attrib["name"]
            else:
                target = int(value.attrib["targetId"])
                if target not in reference._TARGETID_NAMES:
                    raise ValueError(f"frame {label}: unknown targetId {target}")
                name = reference._TARGETID_NAMES[target]
            name = re.sub(r"[^a-zA-Z0-9]+", "_", name).strip("_").lower()
            if not name or name in names:
                raise ValueError(f"duplicate or empty name {name!r} in frame {label}")
            names.add(name)
            width = int(value.get("length", "2"))
            offset = int(value.attrib["offset"])
            if width not in (1, 2) or offset < 0 or offset + width > length:
                raise ValueError(f"frame {label} offset {offset}: invalid field width or extent")
            signed = value.get("signed", frame.get("signed", "false")) == "true"
            scale = 1.0
            if "conversion" in value.attrib:
                expression = value.attrib["conversion"]
                match = re.fullmatch(r"\s*V\s*\*\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)\s*", expression)
                if not match:
                    raise ValueError(f"frame {label} offset {offset}: unsupported conversion {expression!r}")
                scale = float(match[1])
            if not math.isfinite(scale) or scale == 0:
                raise ValueError(f"frame {label} offset {offset}: invalid scale")
            declared = value.get("units", value.get("unit", ""))
            unit = UNITS.get(declared, "dimensionless")
            if declared not in UNITS:
                notes.append(f"unit-unrepresented {name}: {declared}")
            acquisition = "held" if fid == 0xC82 else ACQUISITION.get(name, "live")
            output["signals"].append(dict(name=name, byte_offset=offset, type=f"uint{width * 8}",
                                          byte_order="little", signed=signed, scale=scale, offset=0.0,
                                          unit=unit, acquisition=acquisition, sentinels=[]))
            notes.append(f"acquisition {name}: {acquisition}")
            if "enum" in value.attrib:
                notes.append(f"enum-awaiting-sentinel-decision {name}")
        output["signals"].sort(key=lambda field: field["byte_offset"])
        if not output["signals"]:
            notes.append(f"empty-frame {label}: emitted with empty signals array; schema permits it")
        pack["frames"].append(output)
    pack["frames"].sort(key=lambda frame: frame["id"])
    if not pack["frames"]:
        raise ValueError("no receive frames")
    return pack, notes


def conversion_report(pack, notes):
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    date = datetime.fromtimestamp(int(epoch), timezone.utc).date() if epoch is not None else datetime.now(timezone.utc).date()
    lines = ["# Definition pack conversion", "",
             "Source: the owner's schema XML (path withheld).",
             f"Conversion date (UTC): {date}.",
             f"Pack: `{pack['id']}` version `{pack['version']}`, API 1.", "",
             "Status role: 0xC82, the board status identifier repeated by the relay as a heartbeat.",
             "Its fields publish on every status record with refreshed receive timestamps and unknown age evidence.",
             "Status traffic keeps transport connected, never counts toward acquisition health, and never refreshes telemetry-role signals.",
             "Its fields are held. All other frames have telemetry role.",
             "The acquisition override table is empty pending owner confirmation; other fields default to live.",
             "Sentinel lists are empty pending owner decisions about enum-carrying fields.",
             "The schema permits empty signals arrays; receive frames with no retained fields are emitted.",
             "Signed fields use unsigned width types plus the explicit signed property required by the landed schema.", "",
             "Emitted identifiers: " + ", ".join(f"0x{f['id']:X}" for f in pack["frames"]) + ".",
             f"Identifiers: {len(pack['frames'])}; fields: {sum(len(f['signals']) for f in pack['frames'])}.", ""]
    return "\n".join(lines + ["- " + note for note in notes]) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("xml", "reference-decoder", "out", "report", "pack-id", "pack-version"):
        parser.add_argument("--" + key, required=True)
    args = parser.parse_args()
    try:
        reference = load_reference(args.reference_decoder)
        pack, notes = convert(read_xml(args.xml), reference, args.pack_id, args.pack_version)
        for path in (args.out, args.report):
            Path(path).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_bytes(encode_pack(pack))
        Path(args.report).write_text(conversion_report(pack, notes), encoding="utf-8", newline="\n")
        print(f"Converted {len(pack['frames'])} identifiers, {sum(len(f['signals']) for f in pack['frames'])} fields")
        for note in notes:
            if note.startswith(("skipped", "unit-unrepresented", "enum-", "empty-frame")):
                print(note)
        return 0
    except (ValueError, KeyError, OSError, ET.ParseError) as error:
        print(f"Conversion failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
