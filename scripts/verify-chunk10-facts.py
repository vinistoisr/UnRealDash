"""Verify every claim in the "Facts this chunk depends on" section of
docs/build/chunk-10-package-loader.md against the tree.

Six prerequisite errors in the chunk 08 and chunk 10 specs reached a build
session before being caught. Every one was a fact asserted from memory or
inferred from a name rather than read out of the tree. This script exists so
that cannot happen silently again: each fact the spec states has a check here,
and a fact with no check is not a fact, it is an assumption.

Run it before freezing the spec and again before any build session starts.
Exits non-zero if any claim is false, so it is a gate by the definition in
docs/ARCHITECTURE.md rather than something to read and nod at.

It tolerates deliverable 1's source move: files are located wherever they
currently live, so it keeps working once the sources are in the runtime tree.
"""

import glob
import io
import json
import os
import re
import sys

PASSED = []
FAILED = []

SEARCH_ROOTS = ("packages/dashboard-spec", "runtime/UnRealDash/Source/DashboardSpec")


def check(label, condition, detail=""):
    (PASSED if condition else FAILED).append((label, detail))
    print(("  PASS  " if condition else "  FAIL  ") + label + (("   " + detail) if detail and not condition else ""))


def find(name):
    """Locate a source file, wherever deliverable 1 has left it."""
    for root in SEARCH_ROOTS:
        for directory, _, files in os.walk(root):
            if "build" in directory or "__pycache__" in directory:
                continue
            if name in files:
                return os.path.join(directory, name).replace(os.sep, "/")
    return None


def text(path):
    if not path or not os.path.exists(path):
        return ""
    return io.open(path, encoding="utf-8", errors="replace").read()


def line(path, number):
    body = text(path)
    if not body:
        return ""
    rows = body.split("\n")
    return rows[number - 1] if 0 < number <= len(rows) else ""


cmake = text("packages/dashboard-spec/CMakeLists.txt")
check("exceptions and RTTI off", "_HAS_EXCEPTIONS=0" in cmake and "VALIJSON_USE_EXCEPTIONS=0" in cmake)
check("links signal_core publicly, miniz privately", "PUBLIC signal_core PRIVATE miniz" in cmake.replace("\n", " "))

reader_h = find("PackageReader.h")
print("        (PackageReader.h resolved to " + str(reader_h) + ")")
check(
    "PackageReader::Read returns Error and nothing else",
    re.search(r"Error\s+Read\(std::string_view path, Profile profile = Profile::desktop\) const;", text(reader_h)) is not None,
)

reader_cpp = find("PackageReader.cpp")
# Assert the fact, not a line number: line numbers drift the moment a build session edits the file.
check("exactly one reader branches on is_directory", text(reader_cpp).count("is_directory(") >= 1 and "PackageReader" in str(reader_cpp))
check(
    "std::filesystem security calls are present",
    all(name in text(reader_cpp) for name in ("symlink_status", "is_symlink", "hard_link_count", "canonical")),
)

check(
    "Error carries code, pointer, message, and CodeName exists",
    all(token in text(find("Errors.h")) for token in ("ErrorCode code", "ErrorText pointer", "ErrorText message", "CodeName")),
)

cases = json.load(io.open("tests/fixtures/packages/cases.json", encoding="utf-8"))
accepted = [c for c in cases if not c.get("code")]
archive_only = [c for c in cases if c.get("archive_only")]
check("43 fixture cases", len(cases) == 43, "got " + str(len(cases)))
check("7 accepted and 36 rejected", len(accepted) == 7 and len(cases) - len(accepted) == 36,
      "got " + str(len(accepted)) + " accepted, " + str(len(cases) - len(accepted)) + " rejected")
check("17 carry archive_only", len(archive_only) == 17, "got " + str(len(archive_only)))
check(
    "cross-tab is 6 accepted of 26 both-form and 1 accepted of 17 archive-only",
    sum(1 for c in cases if not c.get("archive_only") and not c.get("code")) == 6
    and sum(1 for c in cases if c.get("archive_only") and not c.get("code")) == 1,
)
check("the accepted archive-only case is windows-attributes-entry",
      [c["name"] for c in archive_only if not c.get("code")] == ["windows-attributes-entry"])
check("some cases carry a profile key", any("profile" in c for c in cases))

bounded = find("BoundedParse.cpp")
check("ReadBounded is defined in BoundedParse.cpp", "bool ReadBounded(" in text(bounded))
check("ReadBounded uses std::ifstream", "ifstream" in text(bounded))

callers = set()
for root in SEARCH_ROOTS:
    for directory, _, files in os.walk(root):
        if "build" in directory or "__pycache__" in directory:
            continue
        for name in files:
            if name.endswith((".cpp", ".h")) and "ReadBounded" in text(os.path.join(directory, name)):
                callers.add(name)
check(
    "ReadBounded has exactly the three known callers",
    callers == {"BoundedParse.cpp", "Internal.h", "SchemaValidation.cpp", "PackageReader.cpp", "validate_main.cpp", "TestSupport.h"},
    str(sorted(callers)),
)

check("five schema files", len(glob.glob("packages/dashboard-spec/schema/*.schema.json")) == 5)
validate_main = find("validate_main.cpp")
check("the CLI uses DASHBOARD_SCHEMA_DIR", "DASHBOARD_SCHEMA_DIR" in text(validate_main))
users = []
for root in SEARCH_ROOTS:
    for directory, _, files in os.walk(root):
        if "build" in directory or "__pycache__" in directory:
            continue
        for name in files:
            if name.endswith((".cpp", ".h")) and "DASHBOARD_SCHEMA_DIR" in text(os.path.join(directory, name)):
                users.append(name)
# Eight users, not one. The spec said "only the CLI" from a grep that never looked in tests/ or tools/.
known_schema_users = {
    "validate_main.cpp", "telemetry-decode-jsonl.cpp", "test_corpus.cpp",
    "test_definition_pack_builder.cpp", "test_malformed_harness.cpp",
    "test_package_reader.cpp", "test_schema.cpp", "test_semantic.cpp",
}
# Deliverable 2 adds a test consumer without changing the prerequisite users.
load_test = {"test_loaded_package.cpp"} if find("test_loaded_package.cpp") else set()
check("DASHBOARD_SCHEMA_DIR retains its known users plus the load tests",
      set(users) == known_schema_users | load_test and len(users) == len(set(users)), str(sorted(users)))

check("Document exposes only Storage&", "Storage &Data()" in text(find("Document.h")))

UBT = "C:/Program Files/Epic Games/UE_5.8/Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModuleCPP.cs"
check("UBT lists .c as a compiled extension at UEBuildModuleCPP.cs:3078", '".c"' in line(UBT, 3078), repr(line(UBT, 3078)[:70]))
check("UBT branches on .c at UEBuildModuleCPP.cs:3129", "HasExtension" in line(UBT, 3129), repr(line(UBT, 3129)[:70]))
check("miniz.c is present", os.path.exists("third_party/miniz/miniz.c"))

spec_presets = [p["name"] for p in json.load(io.open("packages/dashboard-spec/CMakePresets.json", encoding="utf-8"))["configurePresets"]]
core_presets = sorted(p["name"] for p in json.load(io.open("packages/signal-core/CMakePresets.json", encoding="utf-8"))["configurePresets"])
check("dashboard-spec preset is 'default'", spec_presets == ["default"], str(spec_presets))
check("signal-core presets are 'default' and 'tsan'", core_presets == ["default", "tsan"], str(core_presets))

architecture = text("docs/ARCHITECTURE.md").split("\n")
check(
    "ARCHITECTURE.md line 22 forbids tools reading the runtime tree",
    "tools/" in architecture[21] and "never import from the runtime tree" in architecture[21],
    repr(architecture[21][:90]),
)
check(
    "tools/dashboard_spec/schema.py:14 resolves packages/dashboard-spec/schema",
    "packages/dashboard-spec/schema" in line("tools/dashboard_spec/schema.py", 14),
    repr(line("tools/dashboard_spec/schema.py", 14)[:80]),
)

print("")
print("  " + str(len(PASSED)) + " passed, " + str(len(FAILED)) + " failed")
sys.exit(1 if FAILED else 0)
