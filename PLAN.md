# Plan: UnRealDash Stage 0 feasibility and Stage 1 entry
_Locked via claudex-loop - by Claude + Vincent Royer_

## Goal

Answer one question with measurements instead of opinion: is Unreal Engine the right runtime for a data-driven, 3D-capable vehicle dashboard that has to run on a cheap Android head unit?

Stage 0 of the roadmap in PROJECT-PLAN section 10 is complete when all of the following hold:

1. A packaged player renders two dashboards on Windows x64 and on the reference Android device: a control dashboard defined entirely by JSON, and a hand-authored showcase scene.
2. A Linux x86-64 package builds from the Windows workstation.
3. Editing `dashboard.json` changes the control dashboard and editing `showcase.json` changes the showcase, in both cases without rebuilding or repackaging the player.
4. The disconnect scenario marks affected instruments stale within the configured per-signal deadline, and no instrument continues animating fabricated values.
5. The metrics table in PROJECT-PLAN section 8 is filled with measured numbers, recorded with build id, device, resolution, power mode, temperature and sample size, covering both the Vulkan and the OpenGL ES 3.2 rendering paths on the head unit, a 60-minute thermal run, and resident memory sampled over that run.
6. `docs/reports/stage0-feasibility.md` exists and carries an explicit go/no-go recommendation on Unreal for this product.

Stage 1 entry is the first slice of PROJECT-PLAN section 10 stage 1: a localhost command channel and package staging with atomic activation and rollback, designed against what Stage 0 measured rather than against what this plan guesses.

Everything in PROJECT-PLAN sections 1, 2, 6 and 7 that concerns Studio, AI authoring, or remote preview is outside this plan. See **Out of scope**.

## Approach

The owner installs a large toolchain while agents build the engine-independent pieces that do not need it: the signal library, the document schema, the Python tools and CI. Unreal work starts when the toolchain is verified. Device work starts when there is a package to put on the device.

Every task has an ID, a `[human]` or `[agent]` tag, a model tier for agent tasks, one deliverable, and a gate. A task is not done until its gate produces the stated output. Gates that name a number are targets unless the text says "measured".

Pinned versions, all from `docs/research/2026-09-15-toolchain-claudex-research.md` unless noted:

| Component | Pin |
| --- | --- |
| Unreal Engine | 5.8.2, Epic Games Launcher binary install |
| Visual Studio | 2022 Community 17.14+, "Desktop development with C++" and "Game development with C++" |
| MSVC toolset | 14.44 (installed; 14.38 is the UE 5.8 minimum, 14.50 the recommendation) |
| Windows SDK | 10.0.26100 (installed; matches Epic's recommendation) |
| Android Studio | Koala 2024.1.2 Patch 1, via Turnkey |
| Android NDK | r27c |
| JDK | OpenJDK 21.0.3 |
| Android SDK | target 35, compile minimum 34, installable minimum API 26 |
| Linux cross toolchain | v26, clang 20.1.8, Rocky Linux 8 base |
| C++ standard | C++20 for `packages/signal-core`, Unreal's own standard inside the player module |
| Python | 3.12 in CI; 3.14 is installed locally, and the tools must run on both |

### WP0 Workstation and toolchain

No engine work starts until `scripts/doctor.ps1` passes. WP0 is mostly `[human]` because installers need a signed-in Epic account, UAC prompts and disk decisions.

**0.1 [human] Free disk space on C: to at least 150 GB.**
Deliverable: free space on C: at or above 150 GB before any engine install starts.
Gate: `powershell -Command "(Get-PSDrive C).Free/1GB"` prints a number at or above 150.
Sizing basis, all approximate and from community reports rather than an Epic spec sheet: UE 5.8.2 with the Android target platform around 70 GB, VS 2022 Community with the two C++ workloads 15 to 20 GB, Android Studio plus NDK r27c plus JDK 21 plus SDK 35 around 10 GB. Derived Data Cache and `Intermediate/` grow on top of that during the first cooks and are the reason for the headroom rather than a 100 GB floor. Recon on 2026-09-15 found 99 GB free.

**0.2 [agent:opus] Install the small tools: Git LFS, CMake and Ninja.**
Deliverable: `git lfs`, `cmake` and `ninja` resolvable on PATH, installed with `winget install GitHub.GitLFS Kitware.CMake Ninja-build.Ninja`; the owner clicks any UAC prompt that appears.
Gate: `git lfs version && cmake --version && ninja --version` all exit 0 in a fresh shell. CMake was confirmed absent on 2026-09-15; the other two are unverified.

**0.3 [human] Install Visual Studio 2022 Community 17.14+ with both C++ workloads.**
Deliverable: full VS 2022 IDE alongside the existing Build Tools 2022.
Gate: `vswhere -latest -products * -requires Microsoft.VisualStudio.Workload.NativeGame -property installationVersion` prints 17.14 or higher.
Reason: Epic's setup documentation lists only the full IDE with the "Desktop development with C++" and "Game development with C++" workloads. A Build-Tools-only install is not an Epic-documented path. The existing Build Tools install stays; it is what `packages/signal-core` compiles against in the local CMake build.

**0.4 [human] Install UE 5.8.2 from the Epic Games Launcher with Android, Linux and Windows targets.**
Deliverable: engine at a recorded install path with the three target platforms present.
Gate: `<EngineDir>\Engine\Build\Build.version` reports MajorVersion 5, MinorVersion 8, PatchVersion 2, and `Engine\Platforms` contains the Android and Linux directories.

**0.5 [human] Run Turnkey to install the Android SDK, NDK, JDK and Android Studio.**
Deliverable: Android toolchain at the versions Turnkey pins for 5.8.2.
Gate: `RunUAT Turnkey -command=VerifySdk -platform=Android -UpdateIfNeeded` reports Valid, and the printed NDK is r27c and the JDK is 21.x. If the printed JDK is not 21.0.3 exactly, record what it is and treat the research note's patch-level figure as superseded by the live manifest.

**0.6 [human] Install the Linux cross-compile toolchain v26 (clang 20.1.8).**
Deliverable: `v26_clang-20.1.8-rockylinux8` installed and `LINUX_MULTIARCH_ROOT` set.
Gate: `RunUAT Turnkey -command=VerifySdk -platform=Linux` reports Valid and names v26.
Reason for pinning exactly: the research notes record v24/clang-19 as broken for recent 5.x releases, so an older toolchain zip is not a substitute.

**0.7 [agent:opus] Write `scripts/doctor.ps1`.**
Deliverable: one script that prints every pinned version above plus free disk on C:, ADB reachability of the head unit, and Git LFS status, then exits non-zero on any miss.
Gate: `pwsh -File scripts/doctor.ps1` prints a table with one row per pinned component and exits 1 on a deliberately renamed CMake, 0 when everything is present. This task starts immediately and does not wait for WP0 installs; it is written against the pin table and is what proves the installs afterwards.

**0.8 [human] Establish network ADB to the reference device and record its facts.**
Deliverable: `docs/reports/device-dudu7.md` with model, Android build fingerprint, total and available RAM, GPU renderer string, reported Vulkan API version, panel resolution, usable viewport with and without the navigation bar, and thermal zone names.
Gate: `adb -s <host:port> shell dumpsys SurfaceFlinger | findstr GLES` and `adb shell getprop ro.build.fingerprint` both return output, and the file contains every field listed. Device facts currently in the docs (1280x720 panel, 1280x660 usable, Mali-G57 MP4, Vulkan 1.1, Android 13, unrooted) are documented assumptions, not measurements on this unit.

**0.9 [agent:fable] Verify the doctor output against the pin table.**
Deliverable: a short pass/fail note appended to `docs/reports/device-dudu7.md`.
Gate: every row in `doctor.ps1` output matches the pin table in this document, or the mismatch is written down with the reason it is acceptable.

### WP1 Repository scaffold and CI

**1.1 [agent:opus] Create the repository layout from PROJECT-PLAN section 11 for the directories Stage 0 actually uses.**
Deliverable: `packages/signal-core/`, `packages/dashboard-spec/`, `runtime/UnRealDash/`, `connectors/`, `tools/`, `tests/fixtures/`, `examples/`, `docs/reports/`, `scripts/`, each with a README that states what belongs there. No empty placeholder source files.
Gate: `git ls-files` after the scaffold commit lists only READMEs, `.gitignore`, `.gitattributes` and `.github/` config; the commit message maps every added path to the task ID in this plan that will fill it, and no path is unmapped.

**1.2 [agent:opus] Add `.gitattributes` with Git LFS tracking before the first binary commit.**
Deliverable: LFS tracking for `*.uasset`, `*.umap`, `*.png`, `*.jpg`, `*.fbx`, `*.wav`, `*.ttf`, `*.exr`, `*.tga`.
Gate: after committing one test PNG, `git lfs ls-files` lists it and `git cat-file -p HEAD:<path>` shows an LFS pointer, not image bytes.

**1.3 [agent:opus] Add `LICENSE` (MIT) and `LICENSES-ASSETS.md` (CC-BY-4.0 for original example assets), both marked provisional.**
Deliverable: both files, plus a README line stating that engine and third-party assets stay under their own terms.
Gate: `LICENSE` contains the MIT text with the current year and the owner's name; `README.md` links both files.

**1.4 [agent:opus] GitHub Actions workflow for `signal-core`.**
Deliverable: `.github/workflows/signal-core.yml` building and testing on `windows-latest` and `ubuntu-latest` with CMake and Ninja.
Gate: the workflow is green on both runners and the test step prints a non-zero assertion count.

**1.5 [agent:opus] GitHub Actions workflow for the spec and the Python tools.**
Deliverable: `.github/workflows/spec-tools.yml` running the Python schema validator over the fixture corpus and `pytest` over `tools/`.
Gate: green on `ubuntu-latest` with Python 3.12, and the validator step reports the count of valid and invalid fixtures it processed, both non-zero.

**1.6 [agent:opus] Local build and packaging script skeletons.**
Deliverable: `scripts/build.ps1`, `package-windows.ps1`, `package-android.ps1`, `package-linux.ps1`, `deploy-deck.ps1`, `capture-metrics.ps1`, each of which calls `doctor.ps1` first and refuses to run if it fails.
Gate: each script run with `-WhatIf` prints the exact UAT command line it would execute and exits 0; each script run with a deliberately failed doctor exits non-zero without invoking UAT.
Note: no self-hosted runner in Stage 0. Unreal builds are local only. This is a deliberate cost decision, restated under **Key decisions**.

**1.7 [agent:fable] Verify CI from a clean clone.**
Deliverable: a note in `docs/reports/stage0-feasibility.md` recording that both workflows pass on a fresh checkout with no local state.
Gate: a workflow run triggered from a branch with no cached dependencies is green.

### WP2 signal-core v0

`packages/signal-core` is a plain C++20 static library. CMake plus Ninja, no Unreal headers, no Unreal types, buildable and testable on a machine with no engine installed. That constraint is the point: it keeps CI free, keeps the rule logic testable without a GPU, and keeps the option of a non-Unreal player open.

Test framework: **doctest**. Reason: single header, vendored into `third_party/`, no package manager and no separate compiled library step, and the fastest compile of the credible options. Catch2 v3 is the alternative and is better documented, but it is a compiled library that adds a dependency-resolution step to a repository whose CI is meant to run with nothing installed beyond a compiler and CMake.

**2.1 [agent:opus] Sample and quality types.**
Deliverable: `Signal`, `SignalId`, `Sample` with value, unit, source identity, monotonic sequence, monotonic receive timestamp, optional source timestamp, and quality in `{valid, stale, unavailable, invalid}`.
Gate: unit tests assert that a default-constructed `Sample` is `unavailable`, that zero is never used as a missing-data stand-in, and that constructing a sample without a receive timestamp fails to compile.

**2.2 [agent:opus] Signal registry with per-signal freshness deadlines.**
Deliverable: a registry holding the latest sample per signal, with a per-signal deadline evaluated against an injectable monotonic clock.
Gate: a test advances a fake clock past a 500 ms deadline and asserts the signal transitions `valid -> stale` without any new sample arriving, and that a wall-clock jump backwards or forwards does not change the outcome.

**2.3 [agent:opus] Unit normalization at the boundary.**
Deliverable: conversion to SI internally (kelvin, pascal, m/s, rad/s) with declared source units at the connector edge; display units applied at render time only.
Gate: a table-driven test covering degC/degF/K, kPa/bar/psi, km/h/mph/m/s, rpm/rad/s round-trips within 1e-6 relative error, and a test asserting that an unknown unit string is rejected rather than passed through.

**2.4 [agent:opus] Rule engine over a JSON expression tree.**
Deliverable: an AST evaluator with a whitelisted operation set (comparison, arithmetic, boolean, `min`/`max`/`clamp`, `abs`, signal reference, literal), a depth limit of 32, and no string-parsed expressions anywhere.
Gate: tests prove that an unknown operation name is a load-time error naming the offending node path, that depth 33 is rejected, and that evaluating an expression referencing a missing signal returns the declared missing-input result rather than a default number.

**2.5 [agent:opus] Hysteresis, debounce and missing-input policy.**
Deliverable: per-rule hysteresis band, debounce window, and an explicit missing-input policy in `{hold_last, treat_unavailable, force_warn}`.
Gate: a test drives a value oscillating across a threshold at 20 Hz and asserts the rule output changes at most twice over the window; a second test asserts a rule with `treat_unavailable` reports unavailable, not false, when its input goes stale.

**2.6 [agent:opus] Display-only bounded interpolation.**
Deliverable: an interpolator with a per-signal maximum extrapolation window, disabled by construction for discrete signals, and with no path into rule evaluation.
Gate: a test asserts a rule evaluates on the raw sample while the display value differs; a second asserts the interpolator stops advancing at the window bound and does not cross into the stale period; a discrete signal configured for interpolation is a configuration error.

**2.7 [agent:opus] Record and replay format.**
Deliverable: JSON Lines, one sample per line, with monotonic timestamp, signal id, value, unit, source and quality. Writer and reader in `signal-core`.
Gate: record a 60-second scenario, replay it, and assert the replayed sample stream is byte-identical after a round trip through writer and reader.

**2.8 [agent:opus] Deterministic scenario generator.**
Deliverable: `idle`, `acceleration`, `high_temperature`, `missing_signal`, `disconnect`, `reconnect`, `stale_heartbeat`, each seeded and reproducible.
Gate: running each scenario twice with the same seed produces identical JSONL output (`fc /b` on Windows, `cmp` on Linux, no differences).

**2.9 [agent:opus] binary-telemetry-v1 parser.**
Deliverable: a receive-only parser for the existing 16-byte frame format: tag `44 33 22 11`, little-endian u32 frame id in the range 0xC80 to 0xC94 (22 frames), 8-byte payload as four little-endian u16 words, per-frame signed default with per-value override, scaling of the form `V*<float>`. Resynchronization on the tag after garbage. Definitions come from a loaded definition pack, never from hardcoded tables. The tests load a hand-written pack at `tests/fixtures/packs/binary-telemetry-v1.test.json` that validates against 3.1's `definition-pack.schema.json` and covers a signed frame, an unsigned-override field and a scaled field; it is a test fixture, not the converted schema from 3.7.
Gate: tests cover split records across two reads, two coalesced records in one read, a corrupted tag followed by a valid record, a negative temperature via the signed default, an unsigned override field inside an otherwise signed frame, and a frame id outside the known range being dropped with a counter increment rather than an exception.

**2.10 [agent:opus] Relay heartbeat handling.**
Deliverable: explicit classification of the existing relay's synthetic status records so they update transport health but never update a signal's receive timestamp.
Gate: a test feeds 30 seconds of heartbeat-only traffic and asserts that transport state stays `connected` while every mapped signal transitions to `stale` at its deadline. This is the single most important correctness test in WP2: a live socket is not evidence of fresh data.

**2.11 [agent:opus] Malformed-input harness.**
Deliverable: a fuzz-style harness feeding random and mutated bytes to the parser with a bounded corpus checked into `tests/fixtures/`.
Gate: 10 million mutated inputs produce no crash, no unbounded allocation, and no hang; the harness runs in CI in a capped 60-second mode.

**2.12 [agent:fable] Differential check against the owner's reference decoder.**
Deliverable: a comparison report for the synthetic record set, new parser output versus the existing reference decoder output, per field.
Gate: every field matches within the declared scaling precision, or each mismatch is listed with a written cause. Run offline from the owner's separate repository; nothing from it is copied into this one.

### WP3 dashboard-spec v0

**3.1 [agent:opus] JSON Schema draft-7 for the five documents.**
Deliverable: `manifest.schema.json`, `dashboard.schema.json`, `signals.schema.json`, `showcase.schema.json` and `definition-pack.schema.json` in `packages/dashboard-spec/schema/`. The definition-pack schema covers the subset of the PLUGIN-EXPERIENCE.md format that binary-telemetry-v1 needs: pack id and version, input type, frames with id and length, fields with byte offset, type, byte order, signedness, scale, offset and unit. Bit fields and multiplexing are not in the Stage 0 schema.
Coverage for Stage 0: stable component IDs; a reference viewport with explicit anchoring, scaling, clipping and aspect policy including an aspect policy value that forbids non-uniform scaling of circular gauges; the primitives text/numeric readout, image, line/shape, analog dial, arc/bar gauge, indicator, short history graph, container, page switch; day/night theme tokens; explicit missing-data rendering per component; bindings to signals; rules as the WP2 AST.
Gate: every schema validates as draft-7 itself, and `dashboard.schema.json` rejects a document with a duplicate component ID and one with a circular gauge under a `stretch` aspect policy.

**3.2 [agent:opus] Shared fixture corpus.**
Deliverable: `tests/fixtures/documents/valid/*.json` and `.../invalid/*.json`, each invalid fixture carrying a sibling `.expected` file naming the JSON pointer that must fail.
Gate: at least 20 valid and 30 invalid fixtures; every invalid fixture's failure points at the pointer in its `.expected` file.

**3.3 [agent:opus] Python validator.**
Deliverable: `tools/validate-dashboard.py` using `jsonschema`, usable as a CLI and as a CI step.
Gate: `python tools/validate-dashboard.py tests/fixtures/documents` exits 0 and prints counts of valid and invalid fixtures matching the corpus contents; it exits non-zero if any fixture lands on the wrong side.

**3.4 [agent:opus] C++ validator in the player.**
Deliverable: a `dashboard-spec` C++ target using **valijson** over Unreal's bundled RapidJSON, built with no RTTI and no exceptions.
Gate: the same corpus run through the C++ validator produces identical pass/fail results and identical failing pointers to the Python validator. Build.cs does not set `bUseRTTI` or `bEnableExceptions`.
Unverified: whether valijson's RapidJSON adapter compiles cleanly under Unreal's default flags. If it does not, see the fallback in **Risks**.

**3.5 [agent:opus] Validator parity test in CI.**
Deliverable: a CI job that runs both validators over the corpus and diffs their outputs.
Gate: the diff is empty. Any schema change that does not update both validators fails this job. This is the mechanism that stops the two implementations drifting.

**3.6 [agent:opus] `.udash` package reader with bounds.**
Deliverable: a ZIP reader enforcing bounded expansion before activation.
Stage 0 limits, chosen defaults rather than measured ones: 64 MB total expanded size, 4096 entries, 16 MB per asset, expression depth 32, and rejection of any entry path that is absolute, contains `..`, contains a drive letter, or resolves outside the extraction root.
Gate: a test corpus with a zip bomb, a `../../etc/passwd` entry, a 20 MB asset and a 5000-entry archive is rejected with four distinct error codes. A well-formed package loads.

**3.7 [agent:opus] Offline converter for the existing schema.**
Deliverable: `tools/convert-existing-schema.py`, reading the owner's XML from a path given on the command line and emitting a UnRealDash definition pack in the format sketched in PLUGIN-EXPERIENCE.md: frame ids, field offsets, types, byte order, scale, unit, signedness.
Gate: the emitted pack validates against the definition-pack schema, and the WP2 parser loaded with it decodes the synthetic record set to values matching the reference decoder. Scaling expressions of the form `V*<float>` are converted to a numeric `scale` field at conversion time; the player never evaluates an expression string from that XML, and the XML is never read by the player. Source XML stays in the owner's repository; reuse of its content is the owner's decision and is recorded in **Assumptions**.

**3.8 [agent:fable] Arbitrate any parity mismatch between the two validators.**
Deliverable: for each mismatch, a written ruling on which validator is correct and which fixture or schema changes.
Gate: zero unresolved mismatches before WP4 starts consuming the schema.

### WP4 Player: control dashboard

`runtime/UnRealDash/` is a C++ Unreal project. The control dashboard UI is built in C++ at runtime from the loaded document as a UMG widget tree. No hand-authored widget Blueprints for the control dashboard. Binary content in this work package is limited to what the engine requires to open a project.

**4.1 [agent:opus] Unreal C++ project skeleton.**
Deliverable: project with a primary game module, a `UnRealDashCore` module wrapping `signal-core` and `dashboard-spec`, Windows/Android/Linux target settings, and mobile rendering configured without Lumen, Nanite or ray tracing.
Gate: `scripts/build.ps1` produces a Development Editor binary and a Development Win64 binary; the build log shows zero warnings promoted to errors in the project's own modules.

**4.2 [agent:opus] Consume `signal-core` as a static library from the Unreal module.**
Deliverable: CMake toolchain files under `packages/signal-core/cmake/` for Win64 (MSVC 14.44), Android ARM64 (the NDK r27c `android.toolchain.cmake`, `-stdlib=libc++`) and Linux x86-64 (Epic's v26 clang with its sysroot and `-stdlib=libc++`), a `scripts/build.ps1` step that builds all three static libraries before UBT runs, and a `Build.cs` that links the library for the current target through `PublicAdditionalLibraries`, plus a thin adapter translating `signal-core` types into whatever the render side consumes. The C++ standard library and its ABI must match what Unreal links on each target; this is risk R-N.
Gate: the same doctest suite result is reproduced by a small in-engine smoke commandlet that calls into the linked library and prints the library version, on Win64; the Android and Linux libraries are produced by the same CMake project through the toolchain files and `llvm-nm` shows the expected exported symbols in each; the library's headers appear in no Unreal-typed public header.

**4.3 [agent:opus] Connector interface and acquisition threading.**
Deliverable: a C++ connector interface with discover/connect/start/stop/reconnect/health, running off the game thread with a bounded queue and an explicit drop policy, publishing into the registry with receive time and source identity.
Gate: a test scenario publishing at 200 Hz into a 60 Hz renderer shows a bounded queue depth, a non-zero recorded drop count, and no growth in resident memory over 10 minutes.

**4.4 [agent:opus] Package loader.**
Deliverable: load a `.udash` package or an unpacked directory from `-udash=<path>`, validate it, and build the widget tree.
Gate: launching with a valid package renders; launching with each of the four WP3 rejection cases shows a readable on-screen error naming the file and field and does not crash.

**4.5 [agent:opus] Runtime UMG builder for the Stage 0 primitives.**
Deliverable: C++ construction of text/numeric readout, image, line/shape, indicator, container and page switch, with day/night tokens and explicit missing-data rendering.
Gate: a fixture document exercising every primitive renders; a screenshot captured via `-screenshot=` matches a checked-in reference within a per-pixel tolerance recorded in the test.

**4.6 [agent:opus] Analog dial, arc/bar gauge and short history graph.**
Deliverable: the three remaining primitives, with the dial's aspect policy enforced so a circular gauge is never non-uniformly scaled.
Gate: rendering the same document at 1280x720 and at 1280x660 produces a dial whose measured width and height ratio stays within 1 percent of 1.0 in both captures.

**4.7 [agent:opus] Command-line surface.**
Deliverable: `-udash=<path>`, `-scenario=<name>`, `-replay=<file>`, `-connector=<sim|replay|tcp>`, `-screenshot=<path>`, `-quit-after=<seconds>`, `-profile=<mobile|desktop>`, and on Android `-rhi=<vulkan|gles>`.
Gate: each flag has a test in `scripts/capture-metrics.ps1`'s smoke mode; an unknown flag prints the supported list and exits non-zero rather than being ignored.

**4.8 [agent:opus] Debug overlay and metrics output.**
Deliverable: an on-screen overlay and a CSV written on exit, both carrying fps, frame time p95 and p99 over a rolling window, missed frames, resident memory, texture memory, and per-signal freshness state.
Gate: a 60-second run writes a CSV whose row count matches the sample rate times the duration within 1 percent, and whose p95 column is reproducible across two runs of the same deterministic scenario within 10 percent.

**4.9 [agent:opus] Wire the three Stage 0 connectors.**
Deliverable: the in-process simulator, the file replay connector, and a TCP client connector for `127.0.0.1:35000` speaking binary-telemetry-v1, receive-only, using the WP2 parser and the WP3-generated definition pack.
Gate: with a local mock relay replaying recorded bytes, the player shows live values; killing the mock marks every mapped signal stale within its deadline; restarting the mock reconnects without a player restart. The TCP connector never writes to the socket, proven by a test that asserts zero bytes sent over a 5-minute session.

**4.10 [agent:opus] Control dashboard example document.**
Deliverable: `examples/control-dashboard/` with 12 active instruments plus indicators at a 1280x720 reference viewport, per the PROJECT-PLAN section 8 standard-dashboard gate.
Gate: the document validates, renders, and its component count is at least 12 instruments plus 4 indicators.

**4.11 [agent:fable] Verify edit-without-rebuild for the control dashboard.**
Deliverable: a recorded procedure and two screenshots showing a `dashboard.json` edit taking effect in the packaged player with no rebuild and no repackage.
Gate: the packaged executable's file hash is identical before and after the edit, and the two screenshots differ.

### WP5 Player: showcase

The showcase is hand-authored in the Unreal project. It is not schema-driven in Stage 0. Its editable surface is a sidecar, `showcase.json`, exposing a bounded parameter set. The full 3D component schema is a Stage 1 deliverable, designed after WP6 reports real frame times.

**5.1 [agent:opus] Editor Python scripts for the authored content.**
Deliverable: scripts under `tools/editor/` that create the master materials and import the meshes with the exact parameter names the runtime binds to, so the human authoring step is reproducible rather than remembered.
Gate: running the scripts on a clean project produces assets whose parameter names match a checked-in manifest exactly; a missing or renamed parameter fails the script with the parameter name in the message.

**5.2 [human] Author the master materials and meshes in the editor.**
Deliverable: a small set of master materials (ring, needle, luminous band, glass overlay) and meshes (two dial bodies, ring geometry) committed through Git LFS.
Gate: `git lfs ls-files` lists every new `.uasset`, the total added LFS bytes are under 100 MB, and 5.1's manifest check passes against the committed assets.
Content budget is deliberate: GitHub's free LFS quota is a hard constraint and is named in **Risks**.

**5.3 [agent:opus] Showcase scene: two layered 3D instruments around a central speed and gear overlay.**
Deliverable: scene actors, a controlled camera, and a screen-space overlay for the values that must stay crisp.
Gate: at 1280x720 on the workstation the scene renders with the overlay text legible in a captured screenshot at 100 percent zoom, and a reviewer can read the speed value in the capture.

**5.4 [agent:opus] Luminous band driven by RPM through a dynamic material instance.**
Deliverable: a signal-to-material-parameter binding with authored ranges, declared in `showcase.json`.
Gate: replaying the `acceleration` scenario produces monotonically increasing band intensity over the RPM ramp, verified from three captures at known scenario timestamps.

**5.5 [agent:opus] Mode transition into a compact layout with a history graph.**
Deliverable: a state transition over a configurable duration, with interruption handling and a reduced-motion setting.
Gate: triggering the transition twice in rapid succession does not leave an instrument mid-transition; the speed value stays on screen throughout, verified by captures at 0 ms, mid-transition and after completion.

**5.6 [agent:opus] Day and night themes, and a skippable startup reveal.**
Deliverable: coordinated palette, lighting and material response per theme; a startup reveal that any input skips and that never delays the first usable value.
Gate: with the reveal enabled, the first frame showing a live speed value occurs no later than with the reveal disabled, measured from the metrics CSV across five runs each.

**5.7 [agent:opus] Disconnect scenario marks instruments stale.**
Deliverable: an explicit stale presentation for the showcase instruments, distinct from a valid reading.
Gate: running `-scenario=disconnect` produces a capture in which affected instruments are visibly marked and no needle continues to move; the capture is taken at the configured deadline plus 100 ms.

**5.8 [agent:opus] `showcase.json` sidecar with a bounded parameter set.**
Deliverable: material scalar values, transition durations, page selection and colour tokens, all schema-validated, all range-clamped, with no ability to add geometry.
Gate: an out-of-range value is rejected at load with a message naming the parameter; a value inside the range changes the render.

**5.9 [agent:fable] Verify edit-without-rebuild for the showcase.**
Deliverable: two screenshots and the procedure, as in 4.11, for a `showcase.json` edit.
Gate: packaged executable hash unchanged, screenshots differ.

### WP6 Packaging and device qualification

This work package produces the numbers. Nothing before it may be quoted as a measurement.

**6.1 [agent:opus] Package for Windows x64.**
Deliverable: a Shipping Win64 package produced by `scripts/package-windows.ps1`.
Gate: the packaged executable launches with `-udash=examples/control-dashboard -scenario=idle -quit-after=60` and writes a metrics CSV.

**6.2 [agent:opus] Package for Android ARM64, Vulkan.**
Deliverable: an APK with Vulkan as the default RHI, minimum SDK 26, target SDK 35.
Gate: `aapt dump badging` shows `minSdkVersion:'26'` and `targetSdkVersion:'35'`, and the APK installs on the device.

**6.3 [agent:opus] Package for Android ARM64, OpenGL ES 3.2.**
Deliverable: a second APK with GLES 3.2 as the packaged rendering path.
Gate: the APK installs and its manifest shows the GLES 3.2 requirement. Both APKs are kept; this is the fallback path named in **Risks**, not an afterthought.

**6.4 [human] Deploy and launch on the reference device, both RHIs.**
Deliverable: both dashboards running on the head unit under Vulkan and under GLES.
Gate: four screenshots pulled from the device (control and showcase, Vulkan and GLES) plus the four metrics CSVs. Any crash produces a `logcat` capture committed under `docs/reports/logs/`.
Cost note: the device is installed in the car. Every render check on it is a trip to the driveway over network ADB.

**6.5 [agent:opus] Metrics capture harness.**
Deliverable: `scripts/capture-metrics.ps1` driving a scenario on a named target and emitting the PROJECT-PLAN section 8 table populated from the CSVs, with build id, device, resolution, power mode, temperature and sample size in the header.
Gate: running it against a Windows package and against the device produces two tables with identical column sets and no blank cells.

**6.6 [human] 60-minute thermal run on the device.**
Deliverable: a continuous 60-minute run of the showcase with the thermal zone and resident memory sampled throughout.
Gate: a CSV with at least 3600 samples, a plot or table of temperature and memory over time, and an explicit statement of whether resident memory grew monotonically. Frame time p95 at or below 16.7 ms on this device is a **target**, not a promise; the report records what it actually was.

**6.7 [agent:opus] Runtime texture import cost.**
Deliverable: a measurement of decode time and GPU upload time for runtime-imported PNG assets at the sizes the showcase uses, on both Windows and the device.
Gate: a table of decode ms and upload ms per asset size on both platforms, with at least 20 samples per cell. PROJECT-PLAN section 4 flags this as unvalidated; this task is what validates it.

**6.8 [agent:opus] Linux x86-64 package.**
Deliverable: a Linux Shipping package cross-compiled from Windows with toolchain v26.
Gate: `scripts/package-linux.ps1` exits 0 and produces an ELF binary; `file` reports x86-64 ELF. Running it under WSL2 is attempted and the result recorded, but **is not a gate**: WSL2 Vulkan via Mesa dzn is documented as testing-only and can silently fall back to software rendering, so a successful or failed run there proves little either way.

**6.9 [human] Bench-first integration with the existing relay.**
Deliverable: the PILOT-INTEGRATION.md sequence executed in order: synthetic records against the new parser compared to the reference decoder (already covered by 2.12); disconnect, fragmentation, idle heartbeat, held data and reconnect exercised on the bench; workstation render against a replayed stream; device package; then the live relay in the car.
Gate: before two display applications run against the relay at once, a coexistence test records whether the existing dashboard client and the UnRealDash client can both hold a connection, and what backpressure each sees. If coexistence fails, the live-relay step runs with the existing client stopped and that is written into the report.
Direct USB is out of scope. The Feather firmware, its ECU polling and the existing logging are not touched.

**6.10 [agent:fable] Audit the metrics table.**
Deliverable: a pass over every number in the Stage 0 metrics table checking that each one is traceable to a CSV and a run, and that no target has been written in as if it were a measurement.
Gate: every cell cites a source file and a run id, or is marked "not measured".

### WP7 Stage 0 report and Stage 1 entry

**7.1 [agent:opus] Write `docs/reports/stage0-feasibility.md`.**
Deliverable: the PROJECT-PLAN section 8 metrics table filled in, the six Stage 0 exit conditions each marked met or not met with evidence, the Mali Vulkan result on both RHIs, the thermal and memory curves, the texture import numbers, and a list of everything that failed.
Gate: every exit condition in **Goal** above has an explicit met/not-met line with a linked artifact.

**7.2 [human] Go/no-go recommendation on Unreal.**
Deliverable: a signed recommendation section in the same report.
Gate: the section names the specific measured results that drive the recommendation, and states what would have to be true to reverse it.

**7.3 [agent:opus] Stage 1 entry design note.**
Deliverable: `docs/STAGE1-ENTRY.md` covering the localhost WebSocket command channel (load package, run scenario, screenshot; localhost-only binding, origin rejection, pairing) and package staging with atomic activation and rollback (stage, validate, activate, keep previous, fall back after repeated failed launches).
Gate: the note specifies the on-disk layout for staged and active packages, the activation ordering that makes a torn write survivable, and the failed-launch counter that triggers rollback. No implementation in Stage 0.

**7.4 [agent:fable] Audit the report against PROJECT-PLAN sections 8 and 10.**
Deliverable: a checklist confirming each stage 0 exit criterion in PROJECT-PLAN is addressed, with any deviation written down.
Gate: zero silent omissions.

### Sequencing and parallelism

Dependencies, not dates. Anything with no unmet dependency can start.

```
0.7 doctor.ps1        -> (no deps, start now)
0.1 disk              -> 0.3, 0.4
0.2 small tools       -> 2.x (cmake/ninja), 1.2 (lfs)
0.3 VS Community      -> 0.4
0.4 UE 5.8.2          -> 0.5, 0.6, 4.1
0.5 Turnkey Android   -> 6.2, 6.3
0.6 Linux toolchain   -> 6.8
0.8 device recon      -> 6.4, 6.6, 6.9
0.9 verify            <- 0.7, 0.1-0.6

1.1 scaffold          -> everything in 1.x, 2.x, 3.x
1.2 LFS               -> 5.2
1.4 CI signal-core    <- 2.1
1.5 CI spec+tools     <- 3.3
1.6 script skeletons  <- 0.7
1.7 clean-clone CI    <- 1.4, 1.5

2.1 -> 2.2 -> 2.3 -> 2.4 -> 2.5, 2.6
2.7 -> 2.8
2.9 <- 3.1 (definition-pack schema for the fixture pack) ; 2.9 -> 2.10, 2.11
2.12 <- 2.9, 3.7

3.1 -> 3.2 -> 3.3, 3.4 -> 3.5 -> 3.8
3.6 <- 3.1
3.7 <- 3.1, 2.9

4.1  <- 0.4, 0.3, 0.9
4.2  <- 4.1, 2.1-2.6
4.3  <- 4.2
4.4  <- 4.2, 3.4, 3.6
4.5  <- 4.4
4.6  <- 4.5
4.7  <- 4.4
4.8  <- 4.7
4.9  <- 4.3, 2.9, 2.10, 3.7
4.10 <- 4.6
4.11 <- 4.10, 6.1

5.1  <- 4.1
5.2  <- 5.1, 1.2
5.3  <- 5.2, 4.4
5.4  <- 5.3, 5.8
5.5  <- 5.3, 4.6
5.6  <- 5.3
5.7  <- 5.3, 2.10
5.8  <- 3.1, 5.3
5.9  <- 5.8, 6.1

6.1  <- 4.10, 5.7
6.2  <- 6.1, 0.5
6.3  <- 6.2
6.4  <- 6.2, 6.3, 0.8
6.5  <- 4.8, 6.1
6.6  <- 6.4, 6.5
6.7  <- 6.4
6.8  <- 6.1, 0.6
6.9  <- 4.9, 6.4
6.10 <- 6.5, 6.6, 6.7, 6.8, 6.9

7.1  <- 6.10, 4.11, 5.9
7.2  <- 7.1
7.3  <- 7.1
7.4  <- 7.1
```

Two independent tracks run from day one. The owner works 0.1 through 0.6 and 0.8. Agents work 0.7, all of WP1, all of WP2 except 2.9/2.10/2.12, and all of WP3. Those two tracks meet at 4.1. Nothing in WP2 or WP3 needs Unreal installed, which is the whole reason `signal-core` and `dashboard-spec` are engine-independent.

The second convergence point is 6.4. Until a package exists and the owner walks to the car, there are no device numbers and no claim about frame rate on the head unit is permitted.

### Test strategy

Five layers, cheapest first.

1. **Unit, in CI, no engine.** doctest over `signal-core` on `windows-latest` and `ubuntu-latest`. Covers types, units, freshness, rules, hysteresis, interpolation bounds, replay round-trip, scenario determinism, frame parsing, heartbeat classification, malformed input. This is where most correctness lives and it costs nothing per run.
2. **Schema parity, in CI.** One corpus, two validators, diffed. A schema change that touches only one validator fails the build. This is the only defence against the Python tools and the player disagreeing about what a valid document is.
3. **Deterministic render checks, local.** Screenshot captures at known scenario timestamps through `-screenshot=` and `-quit-after=`, compared against checked-in references with a stated per-pixel tolerance. Fixed camera, seeded scenario. Used for the primitives, the dial aspect ratio, the transition, the stale presentation.
4. **Device qualification, in the car.** Both RHIs, both dashboards, the 60-minute thermal run, memory over time, texture import cost, wake and viewport behaviour. Manual, expensive, and the only source of the numbers the report is allowed to quote.
5. **Bench integration, then live.** The PILOT-INTEGRATION.md order: synthetic against reference decoder, then fault injection, then workstation render, then device package, then the live relay with the coexistence check first.

What is deliberately not tested in Stage 0: end-to-end display latency with external high-speed observation. PROJECT-PLAN section 8 is right that software timestamps do not measure panel scanout, but the equipment for the real measurement is not in hand. Stage 0 reports receive-to-present measured in software and labels it as excluding scanout.

## Key decisions and tradeoffs

Decisions 2, 3, 5, 6, 7, 8, 9, 10, 11 and 12 below are **locked via escape hatch (owner deferred to the planning panel)**. They are technical judgements made on the owner's behalf and are the ones reviewers should attack hardest.

**1. Everything installs on C:.** Rejected: a second drive for the engine and DDC. Reason: none is available, and splitting engine from project across volumes complicates every script path. Cost: a hard 150 GB precondition and a real risk of running out mid-cook.

**2. UE 5.8.2 binary from the launcher, not a source build.** *(locked via escape hatch)* Rejected: a GitHub source build, which would allow engine patches for the Mali Vulkan risk. Reason: a source build adds tens of GB and hours of compile time for a feasibility stage, and Epic states 5.8 is the last UE5 major release before UE6, so the pin is stable. Cost: if a Mali crash needs an engine fix, Stage 0 cannot apply one and must fall back to GLES.

**3. Android via Turnkey, with both Vulkan and GLES 3.2 packaged and tested.** *(locked via escape hatch)* Rejected: Vulkan only, which is the default and cheaper to test. Reason: the research file records Mali plus UE Vulkan crashes as an active bug class (Mali-G76 shader compile crash on 5.8, RHIThread crashes on Mali in 5.6), and the target GPU is a Mali-G57 capped at Vulkan 1.1. Cost: two APKs, two deployment runs, two metric sets.

**4. Linux is cross-compiled from Windows and the gate is "builds", not "runs".** Rejected: standing up a Linux machine. Reason: no Linux hardware exists, and Stage 0's Linux question is whether the packaging path works. WSL2 is attempted opportunistically only, because Mesa dzn is testing-only and can fall back to software rendering without erroring, which makes a pass there uninformative. Cost: Linux is unvalidated as a runtime at the end of Stage 0 and the report must say so.

**5. Control-dashboard UI is built in C++ at runtime, not authored as widget Blueprints.** *(locked via escape hatch)* Rejected: widget Blueprints, faster to iterate in the editor. Reason: layout must come from the document, so a Blueprint tree would be a second source of truth, and Blueprints are binary assets that cannot be diffed. Cost: slower initial UI work, no editor preview of the control dashboard, and every layout bug is a C++ debugging session.

**6. `signal-core` is a plain C++20 static library with CMake, Ninja and doctest.** *(locked via escape hatch)* Rejected: putting the signal and rule logic inside an Unreal module, which would remove a build system. Reason: CI would then need an engine install, which hosted runners do not have, and the rule logic would be untestable without a GPU. doctest over Catch2 v3 because it is a single vendored header with no package resolution step. Cost: a second build system, an ABI boundary, three CMake toolchain files, and a `Build.cs` linking a per-target prebuilt library. Fallback if the ABI boundary bites (R-N): compile the same `signal-core` sources inside the Unreal module through a generated file list, keeping CMake for the hosted CI tests only.

**7. Two validators, one corpus.** *(locked via escape hatch)* Rejected: validating only in Python at authoring time and trusting documents in the player. Reason: the player will load packages the tools never saw, and PROJECT-PLAN section 7 requires bounded validation before activation. Cost: valijson plus RapidJSON in the player, a parity job in CI, and the R-G risk that the adapter does not fit Unreal's build flags.

**8. Rules are a JSON expression tree, not a text grammar.** *(locked via escape hatch)* Rejected: a small expression language with a parser. Reason: an AST is schema-validatable, has no parser surface to fuzz, and matches PROJECT-PLAN section 5. Cost: hand-authoring rules is verbose, acceptable because Studio will generate these later.

**9. The control dashboard is data-driven from day one; the showcase is hand-authored with a bounded JSON sidecar.** *(locked via escape hatch)* Rejected: a full 3D component schema in Stage 0. Reason: designing a schema for mesh instances, cameras, transform hierarchies, timelines and state machines before any frame-time data exists would be guessing, and producing that data is Stage 0's job. Cost: the showcase is not portable at the end of Stage 0, and its edit-without-rebuild claim is limited to the sidecar's parameter set. The generic vehicle silhouette from CREATIVE-DIRECTION.md is a stretch item, not a gate.

**10. Git LFS from the first content commit.** *(locked via escape hatch)* Rejected: committing binaries directly, or keeping assets out of the repository. Reason: `.uasset` and `.umap` are binary and unmergeable, and a repository that stores them raw becomes unclonable. Cost: GitHub's free LFS quota is a real ceiling (the figure has changed over time; 1.2 records the current storage and bandwidth allowance from the account's billing page), so showcase content is budgeted under 100 MB and every contributor needs `git lfs`.

**11. Two connector families in Stage 0: simulator plus replay, and a receive-only TCP client for binary-telemetry-v1.** *(locked via escape hatch)* Rejected: starting with generic Bluetooth OBD-II, which PROJECT-PLAN section 5 names as the first broadly useful product target. Reason: Stage 0 needs a real, already-working data source for the device test, and OBD-II needs adapter purchases, vehicle testing and a session layer that has nothing to do with whether Unreal renders acceptably. Cost: Stage 0 produces no evidence about generic OBD-II and the report must not imply otherwise.

**12. No staging or rollback and no command channel in Stage 0.** *(locked via escape hatch)* Rejected: building the staging and rollback machinery now, since PROJECT-PLAN section 7 requires it eventually. Reason: loading a package from a command-line path is enough to prove edit-without-rebuild, and the atomic-activation design should be informed by what the device's storage actually does. Cost: the Stage 0 player has no recovery from a bad package other than a different `-udash=`. 7.3 designs the real mechanism.

**13. CI on hosted runners only; Unreal builds are local.** Rejected: a self-hosted runner on the workstation. Reason: an Unreal build agent needs the whole toolchain, holds the machine's GPU and disk, and makes the workstation a single point of failure for CI. Cost: no automated regression on the packaged player. Every packaging result in Stage 0 is a local run recorded by hand.

**14. MIT for code, CC-BY-4.0 for original example assets, both provisional.** Rejected: deferring the licence choice, as PROJECT-PLAN section 9 leaves it open. Reason: a public repository with no LICENSE file is not contributable. Provisional so the owner can change it before any external contribution arrives.

**15. Device test cadence: the head unit only, over network ADB from the driveway.** Rejected: an old phone as a desk device. The owner offered a 2016 ZTE Axon 7 (Snapdragon 820, Adreno 530, Android 8.0 at best) and a 2015 Huawei Y6 (32-bit, Android 5.1, 1 GB RAM). The Y6 is below UE 5.8's installable floor. The Axon 7 would run the OpenGL ES 3.2 path only, on a different GPU driver family from the head unit's Mali-G57, so it would exercise neither the Vulkan path nor the Mali bug class; the owner chose to keep one test device. Cost: every Android render iteration is a walk to the car; R-D covers the mitigation.

**16. Stage 0 exit is the six conditions in Goal, taken from PROJECT-PLAN sections 8 and 10.** No alternative considered; these are the roadmap's own criteria.

**17. Repository layout is PROJECT-PLAN section 11 as-is.** Rejected: a flatter layout for a stage that uses half the directories. Reason: renaming later breaks every path in every script and document. Cost: some directories sit near-empty, each with a README explaining what lands there.

**18. Bench-first for the car, per PILOT-INTEGRATION.md.** Rejected: connecting to the live relay early for a real demo sooner. Reason: the relay is in a working car with a working dashboard client, and a coexistence failure costs the owner his existing instrumentation. Order: synthetic, fault injection, workstation, device package, then live relay with coexistence checked first.

## Assumptions

Each is a belief this plan depends on, with its source and how it would be falsified.

**A1. Workstation environment.** Windows 11 Pro, RTX A2000 12 GB, 64 GB RAM. Installed: VS Build Tools 2022 17.14 with MSVC 14.44 and Windows SDK 10.0.26100, Node 24, Python 3.14, Go 1.26. Not installed: Unreal Engine, Epic Games Launcher, full Visual Studio IDE, Android Studio, Android SDK, NDK, JDK, CMake. C: had 99 GB free and the owner is clearing more. *Source: environment scan 2026-09-15.* Falsified by `doctor.ps1` in 0.7.

**A2. Toolchain pins.** Every version in the pin table. *Source: `docs/research/2026-09-15-toolchain-claudex-research.md`.* The JDK patch level (21.0.3) is flagged in that file as worth re-verifying against the live Turnkey manifest; task 0.5 does that. The UE install footprint figures are community reports, not an Epic spec, and are explicitly unverified.

**A3. Reference device.** DUDU7 head unit, Unisoc UIS7870 (4x Cortex-A76, 4x Cortex-A55, Mali-G57 MP4), Android 13, unrooted, 1280x720 panel with 1280x660 usable when the navigation bar shows, USB host, reachable over network ADB, installed in the car. *Source: `docs/PILOT-INTEGRATION.md` and the research file; the RAM and storage configuration is from retail listings and is low confidence.* Task 0.8 replaces all of this with measurements.

**A4. Vulkan ceiling.** Mali-G57 is first-generation Valhall and tops out at Vulkan 1.1, which meets UE 5.8's device minimum exactly with no headroom. *Source: research file, citing developer.arm.com.* Falsified by the `dumpsys` output in 0.8.

**A5. Existing telemetry path.** An Android USB bridge app exposes the acquisition board on local TCP 2323 with Telnet framing; a Python relay consumes that and re-serves a raw stream on `127.0.0.1:35000`; the relay emits synthetic status records to keep clients alive when the board goes quiet. A live socket is therefore not evidence of fresh data. *Source: `docs/PILOT-INTEGRATION.md`, derived from read-only inspection of the owner's separate repository.* The installed revisions on the device have not been confirmed against that inspection; 6.9 confirms them before the live step.

**A6. Frame format.** 16 bytes: tag `44 33 22 11`, little-endian u32 frame id in 0xC80 to 0xC94 (22 frames), 8-byte payload as four little-endian u16 words, per-frame signed default with per-value override, scaling of the form `V*<float>`. *Source: the owner's schema decoder.* Falsified by 2.12's differential check.

**A7. Schema reuse is the owner's call.** The source-of-truth XML is the single `board/*.xml` schema file in the owner's separate `scirocco-dash` repository on the workstation. It is owner-authored, so reusing its content is his decision. This plan converts it offline in 3.7 and the player never reads it. If reuse is declined, the definition pack is re-derived from the documented frame format by hand and 3.7's gate changes to a hand-written pack.

**A8. The head unit is the only Android test device.** *Source: owner, 2026-09-15.* No desk device is assumed anywhere in this plan.

**A9. No Linux machine exists.** *Source: environment scan 2026-09-15.* This is why 6.8's gate is "builds" and not "runs".

**A10. The owner is available for the `[human]` tasks and for car access.** Tasks 0.1 to 0.6, 0.8, 5.2, 6.4, 6.6, 6.9 and 7.2 cannot be done by an agent. If car access is unavailable for an extended period, WP6 stalls and Stage 0 cannot exit, since every device number in the report comes from it.

**A11. No skill packs or MCP tools beyond standard coding tools are needed.** File editing, shell, git, a compiler, Python and ADB cover every agent task in this plan.

**A12. Unit conventions in the existing schema are not fully settled.** Boost and rail pressure absolute-versus-gauge, and the exact unit of several temperature and pressure channels, need confirmation against the owner's notes before the values are presented as physical measurements. *Source: `docs/PILOT-INTEGRATION.md`.* Until confirmed, affected signals render with their unit label taken from the definition pack and are listed in the report as unconfirmed.

## Risks and open questions

**R-A. Mali-G57 Vulkan stability on UE 5.8.** Mali plus UE Vulkan crashes are an active bug class on Epic's tracker, including a 5.8 shader-compile crash inside `libGLES_mali.so` on Mali-G76 and RHIThread crashes on Mali in 5.6. Nothing specific to UIS7870 was found, which is absence of evidence, not evidence of stability. *Test:* 6.2 and 6.3 package both RHIs, 6.4 runs both on the device, and any crash produces a committed `logcat`. *Mitigation:* GLES 3.2 is a first-class packaged path, not a footnote. If both paths crash, that is a genuine Stage 0 no-go input and the report says so.

**R-B. Device RAM and thermal headroom are unknown.** PROJECT-PLAN section 8 targets under 750 MB resident on the standard profile; nothing is known about what this unit has free after its own OS. *Test:* 0.8 records total and available RAM; 6.6's 60-minute run records temperature and memory over time. *Mitigation:* if the showcase does not fit, the report records the measured ceiling rather than reducing the showcase until it passes.

**R-C. Disk exhaustion during DDC and Intermediate growth.** The 150 GB precondition is based on unverified community footprint figures. A cook that fills C: can corrupt the DDC and cost a rebuild. *Test:* `doctor.ps1` prints free space and every packaging script calls it first. *Mitigation:* a documented DDC purge step in `scripts/`, and the owner clears space before 0.4 rather than during 6.2.

**R-D. The device is in the car.** Every render check is a trip to the driveway, which makes the Android iteration loop expensive and biases the work toward batching changes, which in turn makes failures harder to attribute. *Mitigation:* network ADB so the trip is short; batch device runs behind a scripted checklist in 6.5; and a rule that no Android-specific change goes to the car before it passes the Windows screenshot checks in layer 3 of the test strategy.

**R-E. GitHub LFS quota.** The free allowance is limited and has changed over time; the current storage and monthly bandwidth figures are read from the account's billing page in 1.2 and written into `packages/dashboard-spec/README.md`. Unreal content and packaged artifacts exceed any free tier quickly. *Test:* 5.2's gate caps added LFS bytes at 100 MB. *Mitigation:* packaged builds and captures are never committed; screenshots in the report are downscaled; if the quota is hit, the owner decides between paid data packs and moving content out of the repository.

**R-F. Runtime texture import cost on Android.** PROJECT-PLAN section 4 flags decoded memory, upload time and platform behaviour as unvalidated. If runtime PNG import is slow on this GPU, the data-driven image primitive is compromised. *Test:* 6.7 measures decode and upload per asset size on both platforms with at least 20 samples per cell. *Mitigation:* if it is too slow, Stage 1 moves image assets to a cooked-content path and the report records the threshold at which that becomes necessary.

**R-G. valijson under Unreal's build flags.** Whether valijson's RapidJSON adapter compiles with RTTI and exceptions both off is **unverified**. *Test:* 3.4's gate fails the build if it does not. *Fallback, Stage 0 only and clearly labelled as such:* documents are validated by the Python validator at authoring and packaging time, and the player performs a lightweight structural check (required keys, types, ID uniqueness, depth and size bounds) instead of full schema validation. If that fallback is taken, the report states plainly that the player is not schema-validating and that Stage 1 must resolve it, because a player that trusts its input is not acceptable beyond a feasibility stage.

**R-H. Relay heartbeats masquerading as fresh data.** The relay emits synthetic status records when the board goes quiet. A naive parser marks every signal fresh forever. *Test:* 2.10 is a dedicated test asserting that 30 seconds of heartbeat-only traffic leaves transport health connected while every mapped signal goes stale. *Mitigation:* transport health, acquisition health and per-signal validity are three separate states in the registry, never one indicator.

**R-I. Unit conventions in the existing schema.** Absolute versus gauge pressure, and several temperature channels, are not confirmed. Presenting an unconfirmed conversion as a measurement is a correctness failure, not a cosmetic one. *Test:* 2.12's differential check catches disagreement with the reference decoder but cannot catch a convention both share. *Mitigation:* A12's rule, plus an explicit unconfirmed list in the Stage 0 report.

**R-J. 60 Hz on the head unit is a target, not a promise.** PROJECT-PLAN section 8 states its table is proposed engineering gates, not measured results. No frame rate claim in this plan is a measurement until 6.5 produces one. *Mitigation:* 6.10's audit exists specifically to catch a target that has been written into the report as if it were measured.

**R-K. Android Studio and NDK drift versus Epic's pin.** Android Studio updates itself; a drifted NDK or JDK produces build failures that look like project bugs. *Test:* `doctor.ps1` prints the resolved NDK and JDK every run and fails on a mismatch with the pin table. *Mitigation:* automatic updates disabled in Android Studio; the resolved versions recorded in the Stage 0 report.

**R-L. Editor-authored assets are binary.** Master materials and meshes from 5.2 cannot be diffed or merged, so two people editing the same material is a lost edit. *Mitigation:* 5.1's editor Python scripts make the authoring reproducible from code, LFS keeps the repository clonable, and the parameter-name manifest turns a silent binding break into a script failure.

**R-N. C++ ABI mismatch between the CMake-built `signal-core` and the Unreal module.** Unreal links libc++ on Android through the NDK and on Linux through its own v26 toolchain; on Win64 it uses MSVC's runtime with specific settings. A static library built with a different standard library, exception model, or runtime flags links but misbehaves, or fails to link with unhelpful errors. *Test:* 4.2's gate builds all three libraries from one CMake project through the toolchain files and checks exported symbols; the in-engine smoke commandlet proves the Win64 link. *Mitigation:* the toolchain files pin `-stdlib=libc++` and the exact compilers Unreal uses; the public `signal-core` API avoids passing standard-library containers across the boundary (plain structs, spans and callbacks only). *Fallback:* compile the same sources inside the Unreal module through a generated file list and keep CMake for CI tests only, as noted under decision 6.

**R-M. Two display clients on one relay.** Running the UnRealDash client alongside the existing dashboard client may starve one or both. *Test:* 6.9's coexistence check runs before any two-client test. *Mitigation:* if coexistence fails, the live-relay run happens with the existing client stopped, and that constraint goes in the report rather than being engineered around in Stage 0.

**Open questions for the owner:**

1. Does he approve reusing the content of his XML schema in this public repository (A7)?
2. Is the first real installation supplementary or eventually a cluster replacement? PROJECT-PLAN section 12 leaves this open; Stage 0 assumes supplementary and nothing in this plan depends on the answer.
3. Are MIT and CC-BY-4.0 acceptable as more than provisional?

## Out of scope

Not built, not designed, not measured in this plan:

- Studio, the authoring service, and everything in PROJECT-PLAN sections 2 and 6.
- Any AI provider integration, model adapter, prompt-driven editing or revision history.
- Bluetooth OBD-II in any form, including ELM-compatible sessions and BLE profiles.
- Aftermarket ECU decoders: MegaSquirt, AEM, MoTeC and the research backlog families.
- Direct USB CDC transport to the acquisition board. The existing bridge keeps USB ownership.
- The documented WebSocket signal feed.
- The WebAssembly module host and the portable protocol module level from PLUGIN-EXPERIENCE.md. The research file's WAMR, wasm3 and wasmtime comparison informs a later spike, not this one.
- Package signing, publisher provenance and any registry, optional or otherwise.
- Pixel Streaming and remote preview.
- Primary instrument cluster replacement, telltales, and anything with a regulatory dimension.
- Calendar estimates. Sizing in this plan is dependency order and gates only.
- Any claim of measured performance before WP6 produces numbers. Every figure in PROJECT-PLAN section 8 is a target until 6.5 and 6.6 replace it.
