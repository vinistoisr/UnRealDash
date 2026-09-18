"""Compare a screenshot capture against a checked-in reference.

This is the single comparator for every screenshot gate in PLAN 4.5 and 4.6. A spec review of
chunk 11 rejected "matches within a per-pixel tolerance" as unbuildable, because max-delta,
mean-delta, RMS and percentage-of-pixels policies give opposite answers on the same pair of images.
So the rule is fixed here, in one place, and every criterion uses it.

The rule
--------
* Both images must have identical dimensions. A size mismatch fails immediately and is never
  resolved by resizing, because a resize invents pixels and would hide a layout change.
* Comparison is on 8-bit RGB. **Alpha is ignored**: the captures are opaque, and an alpha channel
  that differs invisibly would fail the gate for no reason a human could see.
* Per pixel, d = max over the three channels of abs(reference - candidate).
* Two numbers are reported on every run, pass or fail:
      worst  = max d over all pixels
      moved  = fraction of pixels with d > NOISE
* Pass requires BOTH worst <= WORST_LIMIT and moved <= MOVED_LIMIT.

Why two numbers rather than one
-------------------------------
A single max-delta rule fails on one antialiased glyph edge, which differs between driver versions
for reasons nobody can act on. A single percentage rule lets an entire component change colour as
long as it is small on screen. Requiring both means text edges may differ slightly while any
component that actually changes appearance is caught.

The thresholds only mean something in proportion to the thing being measured, so chunk 11 pins the
fixture geometry and writes the arithmetic down. If a fixture changes size, redo that arithmetic.

Exit codes: 0 pass, 1 fail, 2 usage or unreadable input, 3 environment mismatch (reported as
not run, never as a pass and never as a failure).
"""

import argparse
import json
import os
import sys

try:
    from PIL import Image, ImageChops
except ImportError:  # pragma: no cover - environment problem, not a comparison result
    sys.stderr.write("Pillow is required: python -m pip install pillow\n")
    sys.exit(2)

NOISE = 8            # per-pixel delta below which a difference is not counted as moved
WORST_LIMIT = 48     # a single pixel may differ this much, for antialiased edges
MOVED_LIMIT = 0.001  # fraction of pixels allowed to exceed NOISE


def load_rgb(path):
    """Load as 8-bit RGB. Alpha is dropped rather than composited: compositing against an assumed
    background would invent colour, and these captures are opaque anyway."""
    with Image.open(path) as image:
        return image.convert("RGB").copy()


def compare(reference_path, candidate_path):
    reference = load_rgb(reference_path)
    candidate = load_rgb(candidate_path)
    if reference.size != candidate.size:
        return {"ok": False, "reason": "size", "reference_size": reference.size,
                "candidate_size": candidate.size, "worst": None, "moved": None}

    total = reference.width * reference.height
    # ImageChops rather than a per-pixel Python loop: a 1280x720 frame is 921,600 pixels and the
    # loop version took seconds per comparison, which discourages running the gate.
    difference = ImageChops.difference(reference, candidate)
    red, green, blue = difference.split()
    # Per-pixel max across the three channels, as a single band.
    per_pixel = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    worst = per_pixel.getextrema()[1]
    # Everything above NOISE becomes 255, everything else 0, then count the 255s.
    over = per_pixel.point(lambda value: 255 if value > NOISE else 0)
    moved = over.histogram()[255]

    fraction = moved / total if total else 0.0
    return {"ok": worst <= WORST_LIMIT and fraction <= MOVED_LIMIT, "reason": None,
            "worst": worst, "moved": fraction, "moved_pixels": moved, "total_pixels": total}


def check_environment(reference_dir, actual):
    """A capture only compares meaningfully against a reference made on the same machine. GPU,
    driver and font rasterisation all move these numbers, and the hosted CI runners have no GPU.
    A mismatch is reported as not run, so nobody reads it as a pass."""
    path = os.path.join(reference_dir, "capture-environment.json")
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as handle:
        expected = json.load(handle)
    differences = [k for k, v in expected.items() if actual.get(k) and actual[k] != v]
    return differences


def self_test():
    """Prove the comparator can fail, and fail for the right reasons.

    A gate nobody has watched fail is not a gate. This project has already shipped two assertions
    that could not fail, so the comparator ships with the evidence that it can.
    """
    import tempfile

    failures = []

    def check(label, condition, detail=""):
        print(("  PASS  " if condition else "  FAIL  ") + label + (("   " + detail) if detail and not condition else ""))
        if not condition:
            failures.append(label)

    with tempfile.TemporaryDirectory() as directory:
        def write(name, size, colour, patch=None):
            image = Image.new("RGB", size, colour)
            if patch:
                box, patch_colour = patch
                for x in range(box[0], box[2]):
                    for y in range(box[1], box[3]):
                        image.putpixel((x, y), patch_colour)
            path = os.path.join(directory, name)
            image.save(path)
            return path

        size = (320, 180)          # 57,600 pixels, the chunk 11 readout
        base = (12, 14, 18)

        identical_a = write("a.png", size, base)
        identical_b = write("b.png", size, base)
        result = compare(identical_a, identical_b)
        check("identical images pass with zero difference",
              result["ok"] and result["worst"] == 0 and result["moved"] == 0.0, str(result))

        # One pixel changed far beyond the noise floor but well inside the worst limit: the kind of
        # difference an antialiased edge produces. Must still pass.
        edge = write("edge.png", size, base, ((0, 0, 1, 1), (12, 14, 58)))
        result = compare(identical_a, edge)
        check("a single edge pixel within the worst limit passes",
              result["ok"] and result["moved_pixels"] == 1, str(result))

        # A pixel beyond the worst limit fails even though almost nothing moved. This is the half of
        # the rule that catches a small but severe corruption.
        hot = write("hot.png", size, base, ((0, 0, 1, 1), (255, 14, 18)))
        result = compare(identical_a, hot)
        check("one pixel past the worst limit fails", not result["ok"] and result["worst"] > WORST_LIMIT, str(result))

        # 0.2 percent of the frame moved, twice the limit, with every delta small. This is the half
        # of the rule that catches a whole component shifting slightly.
        many = write("many.png", size, base, ((0, 0, 120, 1), (12, 14, 40)))
        result = compare(identical_a, many)
        check("0.2 percent of pixels moved fails on area alone",
              not result["ok"] and result["worst"] <= WORST_LIMIT and result["moved"] > MOVED_LIMIT, str(result))

        # The chunk 11 mutation: recolour the whole 320x180 readout fill. Must fail by a wide margin
        # rather than scraping over the line, which is what proves the gate works rather than that
        # the comparator is twitchy.
        mutated = write("mutated.png", size, (255, 0, 255))
        result = compare(identical_a, mutated)
        check("the named mutation exceeds ten times the moved limit",
              not result["ok"] and result["moved"] > 10 * MOVED_LIMIT, str(result))

        mismatched = write("small.png", (320, 179), base)
        result = compare(identical_a, mismatched)
        check("a size mismatch fails and is never resized", not result["ok"] and result["reason"] == "size", str(result))

    print("")
    print("  self-test: " + ("all checks passed" if not failures else str(len(failures)) + " FAILED"))
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description="Compare a capture against a reference.")
    parser.add_argument("reference", nargs="?")
    parser.add_argument("candidate", nargs="?")
    parser.add_argument("--self-test", action="store_true", help="prove the comparator can fail")
    parser.add_argument("--gpu", default="", help="running GPU name, checked against the reference environment")
    parser.add_argument("--driver", default="", help="running driver version")
    arguments = parser.parse_args()

    if arguments.self_test:
        return self_test()
    if not arguments.reference or not arguments.candidate:
        parser.error("reference and candidate are required unless --self-test is given")
    for path in (arguments.reference, arguments.candidate):
        if not os.path.exists(path):
            sys.stderr.write("missing: " + path + "\n")
            return 2

    differences = check_environment(os.path.dirname(arguments.reference),
                                    {"gpu": arguments.gpu, "driver": arguments.driver})
    if differences:
        print("NOT RUN: reference was captured on a different " + ", ".join(differences))
        return 3

    result = compare(arguments.reference, arguments.candidate)
    if result["reason"] == "size":
        print("FAIL size mismatch reference={0} candidate={1}".format(result["reference_size"], result["candidate_size"]))
        return 1
    print("{0} worst={1} moved={2:.6f} ({3} of {4} pixels)  limits worst<={5} moved<={6}".format(
        "PASS" if result["ok"] else "FAIL", result["worst"], result["moved"],
        result["moved_pixels"], result["total_pixels"], WORST_LIMIT, MOVED_LIMIT))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
