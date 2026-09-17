# Build chunk 04: CI

Status: frozen spec for a Codex build session. Covers PLAN.md tasks 1.4 (both parts), 1.5, and the part of 1.7 an agent can prepare. Read PLAN.md (those tasks, the Approach preamble, the pin table, the Sequencing block, the Test strategy section, Key decision 13, risk R-E) and docs/ARCHITECTURE.md before writing anything. PLAN.md is the authority on what; this file adds the exact job layout, the exact command lines, the action pins and the proof commands. If the two disagree, PLAN.md wins and the disagreement goes in the report.

## Where this chunk sits

This chunk runs **after chunk 03**, not between chunks 02 and 03. PLAN.md's Sequencing block allows 1.4's base job as soon as 2.1 exists, but `spec-tools.yml` references `tools/requirements.txt`, the three generators, `tools/tests/`, `tools/validate-dashboard.py`, the `packages/dashboard-spec` CMake preset and `scripts/validate-parity.ps1`, all of which chunk 03 delivers, and PLAN.md 1.5's own dependency is 3.3. Writing that workflow first would leave every step in it undryrunnable. Both parts of 1.4 land together too, because chunk 02 already delivered 2.13's stress harness for the ThreadSanitizer job to run.

## Goal

Two workflow files that build and test everything in the repository that does not need Unreal, on hosted runners, with no LFS traffic, no caching and no self-hosted runner. Every step's command line is one you can run locally today, and this session's proof is that local run plus a lint. The green run on GitHub is Claude's gate after push, not yours.

## Deliverables

### 1. `.github/workflows/signal-core.yml` (task 1.4)

Triggers: `push` on all branches, `pull_request`, and `workflow_dispatch`. `permissions: contents: read` at the top level. `concurrency` keyed on `${{ github.workflow }}-${{ github.ref }}` with `cancel-in-progress: true`.

**Job `build`**, matrix `os: [windows-latest, ubuntu-latest]`, `fail-fast: false`. Steps in order:

1. `actions/checkout` with `lfs: false` (see the pin table below).
2. On `ubuntu-latest` only, `command -v ninja || { sudo apt-get update && sudo apt-get install -y ninja-build; }`. Guarded so a runner image that already carries Ninja does no work.
3. On `windows-latest` only, `ilammy/msvc-dev-cmd` (see **MSVC environment** below).
4. Step `Toolchain versions`: `cmake --version && ninja --version && cmake --version | head -1`, and on Windows also `cl 2>&1 | head -2`. This step exists so a missing tool fails with its own name in the log instead of inside a confusing configure error.
5. Step `No LFS objects in the checkout` (see **The LFS evidence** below).
6. `cmake --preset default` in `packages/signal-core`, then `cmake --build --preset default`, then `ctest --preset default --output-on-failure`, each its own step so the log names which one failed. `shell: bash` on every step of this workflow, on both operating systems, so one script text serves both legs.
7. Step `Assertion count`: run the test binary directly and check the count (see **The assertion-count check** below).

**Job `tsan`**, `runs-on: ubuntu-latest`, `needs: build`. Steps: checkout with `lfs: false`; the guarded Ninja install; `cmake --preset tsan` and `cmake --build --preset tsan` in `packages/signal-core`; then the 2.13 stress test alone, under ThreadSanitizer:

```bash
export TSAN_OPTIONS="halt_on_error=1:exitcode=66:second_deadlock_stack=1"
./packages/signal-core/build/tsan/signal-core-tests \
  --source-file=*test_threading_stress.cpp --reporters=console 2>&1 | tee tsan.log
grep -q "WARNING: ThreadSanitizer" tsan.log && { echo "FAIL: data race reported"; exit 1; }
```

Then the same assertion-count check against `tsan.log`, so the ThreadSanitizer part of 1.4's gate reports its own non-zero assertion count. `needs: build` is deliberate: a plain build failure should not also spend a sanitizer run.

The `tsan` configure preset carries chunk 02's `"condition"` restricting it to Linux, so this job is the only place it is ever used.

### 2. `.github/workflows/spec-tools.yml` (tasks 1.5 and 3.5)

Same triggers, same `permissions`, same `concurrency` key pattern. Three jobs, all `runs-on: ubuntu-latest`.

**Job `python-tools`.** Checkout with `lfs: false`; the LFS evidence step; `actions/setup-python` with `python-version: '3.12'`; `python -m pip install --upgrade pip` then `python -m pip install -r tools/requirements.txt`; `python -m pip freeze | grep -Ei '^(jsonschema|pytest)'` so the resolved versions are in the log, which is what PLAN.md's pin row for Python asks for. Then, each as its own step:

- `python tools/gen-image-fixtures.py`, `python tools/gen-document-fixtures.py`, `python tools/gen-package-fixtures.py`. The image generator step must print the number of PNGs it produced; if the landed script prints nothing, pipe it through `tee gen.log` and fail the step when no count line appears, rather than editing chunk 03's script.
- `python -m pytest tools/tests -q`.
- `python tools/validate-dashboard.py tests/fixtures/documents`, piped through `tee validate.log`, then an inline check with `grep -Eo` that a valid count and an invalid count were both printed and both are greater than zero. If the landed report format does not print two integers, report it and fail the step rather than changing chunk 03's output.

**Job `cpp-validator`.** Checkout with `lfs: false`; guarded Ninja install; `cmake --preset default` and `cmake --build --preset default` in `packages/dashboard-spec`; `ctest --preset default --output-on-failure`; the assertion-count check on `dashboard-spec-tests`. This is the engine-independent CMake build PLAN.md 3.4 and 3.5 require on hosted CI, so it must not depend on any Unreal path.

**Job `parity` (task 3.5).** Checkout with `lfs: false`; the LFS evidence step; `actions/setup-python` 3.12; install `tools/requirements.txt`; the guarded Ninja install; the three generators; the `packages/dashboard-spec` CMake build; then:

```yaml
- name: Validator parity
  shell: pwsh
  run: |
    ./scripts/validate-parity.ps1 `
      -PythonExe python `
      -ValidatorExe packages/dashboard-spec/build/default/dashboard-spec-validate `
      -CorpusPath tests/fixtures/documents
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

`pwsh` is preinstalled on the Ubuntu hosted image, which is what lets one script serve CI and a contributor's laptop, per docs/ARCHITECTURE.md's testing shape. The job rebuilds the validator and regenerates the fixtures itself rather than taking them from `cpp-validator` as an artifact: artifact passing would add two more pinned actions and a second place a stale file can enter a run, and the build is a few translation units. The job fails on any non-empty diff, which is the whole mechanism of 3.5, so it carries no `continue-on-error`.

### 3. Action pins

Every action is referenced by full commit SHA with the tag in a trailing comment, and nothing else is referenced at all. Write these strings verbatim:

| Action | Pin line to write |
| --- | --- |
| Checkout | `uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683 # v4.2.2` |
| Python | `uses: actions/setup-python@0b93645e9fea7318ecaed2b359559ac225c90a2b # v5.3.0` |
| MSVC environment | `uses: ilammy/msvc-dev-cmd@0b201ec74fa43914dc39ae48a89fd1d8cb592756 # v1.13.0` |

You have no network access and cannot resolve a tag to a SHA, so these are given rather than looked up. **Claude verifies each SHA against its tag before push.** List the three pin lines verbatim in your report so that check is a diff and not a re-reading of the files. Do not add any other action, and do not replace a pinned SHA with a tag or a branch.

### 4. MSVC environment on `windows-latest`

Use `ilammy/msvc-dev-cmd`, pinned as above, as the third step of the Windows leg. Reason, stated here because PLAN.md leaves the mechanism open: the action exports the compiler environment into the job, so configure, build, ctest and the assertion-count check stay four separately named steps, and the gate wants the assertion count printed by a step of its own. The alternative, `vswhere` followed by `cmd /c "call vcvars64.bat && ..."`, has to collapse those four commands into one shell string because a `call` does not survive the end of a step, and the failing command then has no name in the log. Second reason, the install path: this workstation has Build Tools 2022 at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` while the hosted image carries a different edition at a different path, so a hardcoded `vcvars64.bat` path would work locally and fail in CI. The action resolves whichever edition is present.

Recorded fallback, written as a comment above the step and nowhere else: if that action becomes unavailable, the replacement is the single-step form in chunk 02's proof block, `cmd /c "call <vcvars64 path> && cmake --preset default && cmake --build --preset default && ctest --preset default"`, with the path discovered by `vswhere -products * -latest -property installationPath` in the same step.

### 5. Caching policy

No caching anywhere in either workflow: no `actions/cache`, no `setup-python` cache input, no `ccache`. Write this as a comment at the top of each file with the reason. PLAN.md 1.7's gate is a green run "triggered from a branch with no cached dependencies", so a cache works directly against the gate the workflow has to satisfy; the work being cached is a vendored header-only test framework, a handful of translation units and two Python wheels; and a cache is a restore path that can serve a stale artifact into a job whose purpose is to prove a clean clone builds, which is the kind of silent fallback docs/ARCHITECTURE.md forbids.

### 6. The LFS evidence (tasks 1.4 and 1.5)

Every `actions/checkout` in both files sets `lfs: false`. Per Key decision 13 that is also the action's own default, so write a comment on each occurrence saying the setting documents intent rather than fixing a download that would otherwise happen.

The check is the absence of LFS objects in the checked-out tree. Chunk 01 committed `tests/fixtures/images/lfs-probe.png` through LFS, so its pointer text is the probe. One step, `shell: bash`, in the `build` job on both operating systems and in `python-tools` and `parity`:

```bash
probe="tests/fixtures/images/lfs-probe.png"
head -c 45 "$probe" | grep -q "git-lfs.github.com/spec/v1" \
  || { echo "FAIL: $probe is not an LFS pointer, so LFS content was fetched"; exit 1; }
if [ -d .git/lfs/objects ] && [ -n "$(find .git/lfs/objects -type f -print -quit)" ]; then
  echo "FAIL: .git/lfs/objects holds fetched objects"; exit 1
fi
echo "PASS: no LFS objects in the checkout"
echo "supporting evidence, configuration only:"
git config --get-regexp '^lfs\.' || echo "(no lfs.* configuration present)"
```

The `git config` probe is printed as supporting evidence and never decides the step, because configuration does not establish what was fetched. Its non-zero exit when no key matches must not fail the job, which is what the `|| echo` is for.

### 7. The assertion-count check

doctest's console reporter ends with a line of the form `[doctest] assertions: N | N passed | 0 failed |`. One inline bash block, used in three places, `signal-core-tests` twice and `dashboard-spec-tests` once:

```bash
bin=$(find "$1" -maxdepth 1 -type f \( -name "$2" -o -name "$2.exe" \) | head -1)
[ -n "$bin" ] || { echo "FAIL: no test binary under $1"; exit 1; }
"$bin" --reporters=console 2>&1 | tee run.log
line=$(grep -E '^\[doctest\] assertions:' run.log | tail -1)
[ -n "$line" ] || { echo "FAIL: no doctest assertion summary"; exit 1; }
count=$(echo "$line" | sed -E 's/.*assertions: *([0-9]+).*/\1/')
failed=$(echo "$line" | sed -E 's/.*\| *([0-9]+) failed.*/\1/')
echo "assertions=$count failed=$failed"
[ "$count" -gt 0 ] || { echo "FAIL: zero assertions"; exit 1; }
[ "$failed" -eq 0 ] || { echo "FAIL: $failed assertions failed"; exit 1; }
```

Chunk 04 owns no directory under `scripts/`, so this stays inline in each job rather than becoming a script or a composite action. `ctest` runs first and is the gate on per-file failures; this step exists because the gate also requires the count to be printed and checked, which `ctest` does not do.

### 8. `docs/reports/stage0-feasibility.md` skeleton (task 1.7)

A skeleton only. No numbers, no claims, no filled cells. Sections, in this order, each with one line stating what fills it and which PLAN.md task produces it:

1. `# Stage 0 feasibility`, first line marking the file a skeleton written by chunk 04 and completed by PLAN.md 7.1.
2. `## Stage 0 exit conditions` listing PLAN.md's six conditions from **Goal**, each with `Status: not yet assessed` and an empty evidence link.
3. `## Metrics table` with the column set from PLAN.md 6.5 in the header and a single row reading `not measured (blocked by 6.5)` in every cell, so the format 7.1 requires is already the format in the file.
4. `## CI from a clean clone (task 1.7)` carrying exactly this placeholder and nothing more: `Not yet recorded. Claude fills this after the first green run of both workflows on a branch with no cached dependencies, naming the run ids of signal-core.yml and spec-tools.yml.`
5. `## Findings`, `## What failed`, `## What never ran`, each empty with a one-line note naming 7.1 as the task that fills it.
6. `## Go or no-go recommendation`, one line naming 7.2 as its owner and no recommendation in it.

Do not write a recommendation, a measurement, or a target presented as a measurement anywhere in this file. PLAN.md 6.10 exists to catch that.

## Constraints

- Sandbox session, no network access at any point. The pinned SHAs above are given for that reason.
- Do not run `git commit`, `git push`, `git add`, or change git configuration. Claude commits.
- Do not modify PLAN.md or PLAN-REVIEW-LOG.md, and do not modify anything under `docs/` except the one file this chunk owns, `docs/reports/stage0-feasibility.md`.
- **This chunk owns `.github/` and `docs/reports/stage0-feasibility.md`, and touches nothing else.** Chunk 01 owns `scripts/` and the repository root, chunk 02 owns `runtime/UnRealDash/Source/SignalCore/` and `packages/signal-core/`, chunk 03 owns `packages/dashboard-spec/`, `tools/` and the document and package fixtures. Do not edit a CMake file, a script, a schema, a test or a fixture to make a workflow step pass. If a step cannot pass without such an edit, write the step as specified, leave it failing locally, and report which file would have to change and why. A red step with a named cause is worth more than a green one produced by editing another chunk's deliverable.
- Nothing in this chunk may require elevation.
- Plain factual language in every file and every string that reaches a log. No em dashes anywhere. No marketing words. Do not name any commercial dashboard product.
- No self-hosted runner, no Unreal step, no packaging step, no device step, per Key decision 13. Neither workflow may reference an engine path, a UAT command or an Android tool.
- Environment pins: `pwsh` 7.6 locally, Python 3.14 locally and 3.12 in CI, CMake 4.4.3 per-user, Ninja 1.13, MSVC 14.44 from Build Tools 2022 at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`. No Unreal Engine is installed on this machine.
- Both YAML files use two-space indentation, LF endings and no tab characters, and every `run:` step sets an explicit `shell:`.

## Non-goals

An Unreal build job. A packaging job. Any job that needs the device. Caching. Artifact upload. A release workflow. Branch protection or required-check configuration, which is a repository setting and not a file. Editing `scripts/validate-parity.ps1`, the generators, the validators or any CMake file. Recording the clean-clone result, which is Claude's after the first green run.

## Proof

`act` is not installed and no container runtime is available here, so a workflow cannot be executed locally. The proof is three parts: a lint of both files, a local dry run of every step's command line, and a list of the steps that cannot run on this machine with the reason for each.

**Part 1, lint.** Run both, paste both outputs:

```
python -c "import yaml,sys;[yaml.safe_load(open(p,encoding='utf-8')) for p in ('.github/workflows/signal-core.yml','.github/workflows/spec-tools.yml')];print('yaml parse ok')"
C:\Users\Vincent\go\bin\actionlint.exe .github/workflows/signal-core.yml .github/workflows/spec-tools.yml; echo "actionlint exit=$LASTEXITCODE"
```

`actionlint` is installed at that path on this machine. If it is not resolvable from the sandbox, say "actionlint not run" and give the reason; do not install anything.

**Part 2, local dry run.** For each `run:` step in both files, execute the same command line locally and paste the output. Enter the MSVC environment first, the same way chunk 02's proof block does:

```
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" && cd packages\signal-core && cmake --preset default && cmake --build --preset default && ctest --preset default --output-on-failure"
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" && cd packages\dashboard-spec && cmake --preset default && cmake --build --preset default && ctest --preset default --output-on-failure"
```

Then, from the repository root, the assertion-count block against `packages/signal-core/build/default` and `packages/dashboard-spec/build/default`, the LFS evidence block, and the Python side:

```
python -m pip freeze | Select-String -Pattern '^(jsonschema|pytest)'
python tools/gen-image-fixtures.py
python tools/gen-document-fixtures.py
python tools/gen-package-fixtures.py
python -m pytest tools/tests -q
python tools/validate-dashboard.py tests/fixtures/documents; "exit=$LASTEXITCODE"
pwsh -NoProfile -File scripts/validate-parity.ps1; "exit=$LASTEXITCODE"
```

Run the bash blocks with `bash -lc` so the exact text in the workflow is what runs, not a PowerShell translation of it. Local Python is 3.14 and CI is 3.12; record that difference rather than changing the workflow.

**Part 3, what cannot run here.** Report each of these as not run, with the reason, and do not mark any of them as passing: the `tsan` preset, which chunk 02's presets restrict to Linux; the guarded `apt-get` Ninja install; `ilammy/msvc-dev-cmd`, which is a hosted-runner action; `actions/checkout` and `actions/setup-python`; and the Ubuntu legs of every matrix step.

**The green-run gate is Claude's, after push.** PLAN.md 1.4's and 1.5's gates are satisfied by a green run on GitHub's runners, and PLAN.md 1.7's gate by a green run from a branch with no cached dependencies. Nothing in this session can establish any of the three. Your report states that plainly and does not claim a gate met.

## Report format

End with: files added or changed, one line each giving path, what it is, and the PLAN.md task it serves; the three action pin lines verbatim; the lint output and the local dry-run output verbatim; the assertion counts you observed locally for `signal-core-tests` and `dashboard-spec-tests`; the valid and invalid fixture counts the Python validator printed; the list of steps not run with a reason for each; every place this spec or PLAN.md was ambiguous and what you chose; every file outside this chunk's ownership that would have to change for a step to pass, if any; and an explicit statement that the green run on GitHub has not happened and is Claude's gate.
