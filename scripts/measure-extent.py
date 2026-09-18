"""Measure the extent of one component in a capture, by colour.

`compare-capture.py` answers "did this frame change". The PLAN 4.6 gate asks a different question:
"how big is this thing, and is it the same shape at two viewport heights". A dial that is scaled
non-uniformly still matches its own reference perfectly, so the comparator cannot see the defect
this gate exists to catch. This measures instead.

It finds every pixel within a per-channel tolerance of a target colour, reports the bounding box of
those pixels, and reports `ratio = width / height`. The caller picks colours that appear on exactly
one component, so a measurement can only be of the thing it names.

Two failure modes this refuses to have
--------------------------------------
* **Measuring nothing and calling it a pass.** A frame where the component failed to draw contains
  no matching pixels, and a tool that reported "ratio 1.0, 0 pixels, ok" would pass the gate for a
  blank screen. So a match count below `--minimum-pixels` is a failure, and the floor is required
  rather than defaulted.
* **Measuring two things at once.** If the bounding box is far larger than the matched pixel count
  can account for, the match has probably spread across two components. `--fill-floor` fails when
  the matched pixels cover less than that fraction of their own bounding box.

Exit codes: 0 measured, 1 measurement failed a floor, 2 usage or unreadable input.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:  # pragma: no cover - environment problem, not a measurement result
    sys.stderr.write("Pillow is required: python -m pip install pillow\n")
    sys.exit(2)


def parse_colour(text):
    value = text.lstrip("#")
    if len(value) != 6:
        raise ValueError("colour must be #rrggbb, got: " + text)
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def measure(path, colour, tolerance):
    """Bounding box of every pixel within `tolerance` per channel of `colour`."""
    with Image.open(path) as handle:
        image = handle.convert("RGB")
        width, height = image.size
        pixels = image.load()

        low = tuple(max(0, channel - tolerance) for channel in colour)
        high = tuple(min(255, channel + tolerance) for channel in colour)

        left, top, right, bottom, count = width, height, -1, -1, 0
        for y in range(height):
            for x in range(width):
                red, green, blue = pixels[x, y]
                if low[0] <= red <= high[0] and low[1] <= green <= high[1] and low[2] <= blue <= high[2]:
                    count += 1
                    if x < left: left = x
                    if x > right: right = x
                    if y < top: top = y
                    if y > bottom: bottom = y

    if count == 0:
        return {"count": 0, "box": None, "width": 0, "height": 0, "ratio": None, "fill": 0.0}
    box_width = right - left + 1
    box_height = bottom - top + 1
    return {
        "count": count,
        "box": (left, top, right, bottom),
        "width": box_width,
        "height": box_height,
        "ratio": box_width / box_height,
        "fill": count / (box_width * box_height),
    }


def self_test():
    """Prove the tool measures what it claims, and prove it can fail.

    A gate nobody has watched fail is not a gate. This project has already shipped two assertions
    that could not fail, so every measuring tool in it arrives with the evidence that it can.
    """
    import tempfile

    failures = []

    def check(label, condition, detail=""):
        print(("  PASS  " if condition else "  FAIL  ") + label + (("   " + detail) if detail and not condition else ""))
        if not condition:
            failures.append(label)

    target = (255, 0, 255)

    with tempfile.TemporaryDirectory() as directory:
        def write(name, size, rectangle):
            image = Image.new("RGB", size, (0, 0, 0))
            if rectangle:
                x0, y0, x1, y1 = rectangle
                for x in range(x0, x1):
                    for y in range(y0, y1):
                        image.putpixel((x, y), target)
            path = os.path.join(directory, name)
            image.save(path)
            return path

        square = write("square.png", (400, 400), (50, 60, 250, 260))
        result = measure(square, target, 8)
        check("a 200 by 200 square measures exactly 200 by 200 at ratio 1.0",
              result["width"] == 200 and result["height"] == 200 and result["ratio"] == 1.0
              and result["count"] == 40000, str(result))

        # The defect this gate exists to catch: the same square scaled on one axis only.
        stretched = write("stretched.png", (400, 400), (50, 60, 250, 243))
        result = measure(stretched, target, 8)
        check("a non-uniformly scaled square reports a ratio away from 1.0",
              abs(result["ratio"] - 1.0) > 0.01, str(result))
        print("        measured ratio {0:.4f}, which is {1:.2f} percent off".format(
            result["ratio"], abs(result["ratio"] - 1.0) * 100))

        empty = write("empty.png", (400, 400), None)
        result = measure(empty, target, 8)
        check("a frame with no matching pixel measures nothing rather than passing",
              result["count"] == 0 and result["ratio"] is None, str(result))

        # Two separated blobs of one colour: the bounding box spans both, so the fill fraction
        # collapses. This is what catches a colour that is not unique to one component.
        two = Image.new("RGB", (400, 400), (0, 0, 0))
        for x in range(10, 40):
            for y in range(10, 40):
                two.putpixel((x, y), target)
        for x in range(360, 390):
            for y in range(360, 390):
                two.putpixel((x, y), target)
        two_path = os.path.join(directory, "two.png")
        two.save(two_path)
        result = measure(two_path, target, 8)
        check("two separated blobs of one colour collapse the fill fraction",
              result["fill"] < 0.05, str(result))

        # Tolerance is per channel and inclusive at the edge.
        near = Image.new("RGB", (20, 20), (247, 8, 247))
        near_path = os.path.join(directory, "near.png")
        near.save(near_path)
        result = measure(near_path, target, 8)
        check("a colour exactly at the tolerance edge still matches", result["count"] == 400, str(result))
        result = measure(near_path, target, 7)
        check("a colour one step outside the tolerance does not match", result["count"] == 0, str(result))

    print("")
    print("  self-test: " + ("all checks passed" if not failures else str(len(failures)) + " FAILED"))
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description="Measure a component's extent in a capture, by colour.")
    parser.add_argument("capture", nargs="?")
    parser.add_argument("--colour", help="target colour as #rrggbb")
    parser.add_argument("--tolerance", type=int, default=8,
                        help="per-channel tolerance; the default matches compare-capture.py's noise floor")
    parser.add_argument("--minimum-pixels", type=int,
                        help="fail if fewer pixels match; required, because measuring nothing must not pass")
    parser.add_argument("--fill-floor", type=float, default=0.0,
                        help="fail if the matched pixels cover less than this fraction of their bounding box")
    parser.add_argument("--self-test", action="store_true", help="prove the tool measures and can fail")
    arguments = parser.parse_args()

    if arguments.self_test:
        return self_test()
    if not arguments.capture or not arguments.colour:
        parser.error("capture and --colour are required unless --self-test is given")
    if arguments.minimum_pixels is None:
        parser.error("--minimum-pixels is required: a measurement of nothing must never read as a pass")
    if not os.path.exists(arguments.capture):
        sys.stderr.write("missing: " + arguments.capture + "\n")
        return 2

    result = measure(arguments.capture, parse_colour(arguments.colour), arguments.tolerance)
    if result["count"] == 0:
        print("FAIL no pixel within {0} of {1} in {2}".format(
            arguments.tolerance, arguments.colour, os.path.basename(arguments.capture)))
        return 1

    ok = result["count"] >= arguments.minimum_pixels and result["fill"] >= arguments.fill_floor
    print("{0} {1} colour={2} box={3} width={4} height={5} ratio={6:.6f} pixels={7} fill={8:.3f}".format(
        "MEASURED" if ok else "FAIL", os.path.basename(arguments.capture), arguments.colour,
        result["box"], result["width"], result["height"], result["ratio"], result["count"], result["fill"]))
    if result["count"] < arguments.minimum_pixels:
        print("  pixels {0} is below the floor of {1}".format(result["count"], arguments.minimum_pixels))
    if result["fill"] < arguments.fill_floor:
        print("  fill {0:.3f} is below the floor of {1:.3f}: the colour may match more than one component".format(
            result["fill"], arguments.fill_floor))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
