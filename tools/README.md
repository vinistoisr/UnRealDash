# Offline tools

Python validators, converters and fixture generators belong here (PLAN.md 3.x).
They may depend on packages/dashboard-spec/schema and tests/fixtures only.
Runtime imports and player code do not belong here.

Chunk 03 provides `validate-dashboard.py` over the `dashboard_spec` package
(PLAN.md 3.3), with draft-7 schemas from `packages/dashboard-spec/schema` and
shared corpus expectations from `tests/fixtures/documents`.

From the repository root, run:

```text
python tools/gen-image-fixtures.py
python tools/gen-document-fixtures.py
python tools/gen-package-fixtures.py
python -m pytest tools/tests -q
python tools/validate-dashboard.py tests/fixtures/documents
pwsh -NoProfile -File scripts/validate-parity.ps1
```

The generators are standard-library-only and deterministic. The image writer
uses `zlib` and `struct` rather than PLAN.md 1.5's proposed Pillow dependency.
Run them in the order shown: package cases consume generated images and large
documents. Generated files are ignored; no new binary needs Git LFS. Tests hash
the outputs of two generator runs and compare them byte for byte.

The validator accepts a file or a `valid/` plus `invalid/` corpus directory,
`--report <path>`, `--profile mobile|desktop`, and `--json`. A corpus run checks
both the code and raw RFC 6901 pointer in every two-line `.expected` sibling.
Nonzero exit means an invalid single file, an expectation mismatch, missing
corpus sides, or an argument error. The report has four tab-separated fields
per fixture; details of the format and document-set envelope are in the package
README. Python does not open `.udash` packages. The C++ CLI's `--package` mode
owns that check.

`requirements.txt` retains jsonschema and pytest version ranges for Python 3.12
and 3.14. The local proof uses the already installed packages and performs no
installation. The resolved versions and complete proof output are given in the session report. No runtime module is imported by
these tools. Converters, recording tools, CI jobs and engine integration remain
outside chunk 03.
