"""Print and gate doctest totals from the XML reporter, never source text."""
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
assertions = root.find("OverallResultsAsserts")
cases = root.find("OverallResultsTestCases")
if assertions is None or cases is None:
    raise SystemExit("Missing doctest totals")
a = {key: int(assertions.attrib[key]) for key in ("successes", "failures")}
c = {key: int(cases.attrib[key]) for key in ("successes", "failures")}
print(f"cases={sum(c.values())} failed_cases={c['failures']} "
      f"assertions={sum(a.values())} failed_assertions={a['failures']}")
raise SystemExit(0 if a["successes"] and c["successes"] and not a["failures"] and not c["failures"] else 1)
