# Plan: UnRealDash Stage 0 feasibility and Stage 1 entry
_Locked via claudex-loop - by Claude + Vincent Royer_

## Goal

Answer one question with measurements instead of opinion: is Unreal Engine the right runtime for a data-driven, 3D-capable vehicle dashboard that has to run on a cheap Android head unit?

Stage 0 of the roadmap in PROJECT-PLAN section 10 is complete when all of the following hold:

1. A packaged player renders two dashboards on Windows x64 and on the reference Android device: a control dashboard defined entirely by JSON, and a hand-authored showcase scene.
2. A Linux x86-64 package builds from the Windows workstation.
3. Editing `dashboard.json` changes the control dashboard and editing `showcase.json` changes the showcase, in both cases without rebuilding or repackaging the player.
4. The disconnect scenario marks affected instruments stale within the configured per-signal deadline, and no instrument continues animating fabricated values.
5. The metrics table in PROJECT-PLAN section 8 is filled with measured numbers, recorded with build id, device, resolution, power mode, temperature and sample size, covering both the Vulkan and the OpenGL ES 3.2 rendering paths on the head unit, a 60-minute thermal run on the head unit, an 8-hour desktop soak on Windows, and resident memory sampled over both runs.
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
| Linux cross toolchain | v26, clang 20.1.8, Rocky Linux 8 base. Used by UBT for Linux packaging only; no CMake cross-build uses it |
| C++ standard | C++20 for the signal-core and dashboard-spec sources in both the CMake build and the Unreal build; Unreal's own standard for the rest of the player module |
| Python | 3.12 in CI; 3.14 is installed locally, and the tools must run on both |

### WP0 Workstation and toolchain

No engine work starts until `scripts/doctor.ps1` passes. WP0 is mostly `[human]` because installers need a signed-in Epic account, UAC prompts and disk decisions.

**0.1 [human] Free disk space on C: to at least 150 GB.**
Deliverable: free space on C: at or above 150 GB before any engine install starts.
Gate: `powershell -Command "(Get-PSDrive C).Free/1GB"` prints a number at or above 150.
The 150 GB figure is a pre-install capacity check and is what `doctor.ps1 -Profile toolchain` enforces before 0.3 and 0.4 run. It is not a standing precondition for every build: once the toolchain is installed, most of that space is consumed by the toolchain itself. The standing check afterwards is build headroom, a chosen default of 30 GB free on C:, enforced by `doctor.ps1` on every build and packaging run. Both numbers are chosen defaults, not measurements; 6.2 and 6.8 record what a cook actually consumed and the report carries that number.
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

**0.7 [agent:opus] Write `scripts/doctor.ps1` with operation-specific profiles.**
Deliverable: one script with `-Profile toolchain|device`. The `toolchain` profile prints every pinned version above, Git LFS status, and free disk on C: against the 30 GB build-headroom default (or against the 150 GB pre-install capacity figure when `-PreInstall` is passed). The `device` profile adds ADB reachability of the head unit and the device's own free storage. Both exit non-zero on any miss inside the selected profile. No profile makes a Windows or Linux build depend on the car being awake.
Gate: `pwsh -File scripts/doctor.ps1 -Profile toolchain` exits 1 on a deliberately renamed CMake and 0 when everything is present, with the head unit powered off; `-Profile device` exits 1 with the head unit powered off and 0 when it is reachable. Each run prints a table with one row per checked component and the profile name in the header. This task starts immediately and does not wait for WP0 installs; it is written against the pin table and is what proves the installs afterwards.

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
Deliverable: `.github/workflows/signal-core.yml` building and testing on `windows-latest` and `ubuntu-latest` with CMake and Ninja, plus a third job on `ubuntu-latest` that builds the same sources with `-fsanitize=thread` and runs the 4.3 acquisition stress test. Every `actions/checkout` step in this workflow sets `lfs: false`.
Gate: the workflow is green on all three jobs, the test step prints a non-zero assertion count, and the ThreadSanitizer job reports zero data races. A `git config --get lfs.*` probe in the job log confirms no LFS objects were fetched.

**1.5 [agent:opus] GitHub Actions workflow for the spec and the Python tools.**
Deliverable: `.github/workflows/spec-tools.yml` running the Python schema validator over the fixture corpus and `pytest` over `tools/`. Every `actions/checkout` step in this workflow sets `lfs: false`.
Gate: green on `ubuntu-latest` with Python 3.12, and the validator step reports the count of valid and invalid fixtures it processed, both non-zero. No LFS objects are fetched by any job in this workflow.

**1.6 [agent:opus] Local build and packaging script skeletons.**
Deliverable: `scripts/build.ps1`, `package-windows.ps1`, `package-android.ps1`, `package-linux.ps1`, `deploy-deck.ps1`, `capture-metrics.ps1`, each of which calls `doctor.ps1` first and refuses to run if it fails. `build.ps1`, `package-windows.ps1`, `package-android.ps1` and `package-linux.ps1` call `-Profile toolchain`. Only `deploy-deck.ps1` and `capture-metrics.ps1` call `-Profile device`.
Gate: each script run with `-WhatIf` prints the exact UAT command line it would execute and exits 0; each script run with a deliberately failed doctor exits non-zero without invoking UAT; and `build.ps1`, `package-windows.ps1` and `package-linux.ps1` each complete a `-WhatIf` run with the head unit powered off.
Note: no self-hosted runner in Stage 0. Unreal builds are local only. This is a deliberate cost decision, restated under **Key decisions**.

**1.7 [agent:fable] Verify CI from a clean clone.**
Deliverable: a note in `docs/reports/stage0-feasibility.md` recording that both workflows pass on a fresh checkout with no local state.
Gate: a workflow run triggered from a branch with no cached dependencies is green.

### WP2 signal-core v0

The signal-core sources are plain C++20 with no Unreal headers and no Unreal types. They live at `runtime/UnRealDash/Source/SignalCore/` and `packages/signal-core` compiles the same files with CMake plus Ninja, buildable and testable on a machine with no engine installed. That constraint is the point: it keeps CI free, keeps the rule logic testable without a GPU, and keeps the option of a non-Unreal player open. 4.2 and decision 6 cover why there is no prebuilt library.

The library is exception-free by design: every fallible operation returns an error code or a result type, and nothing in signal-core throws. The CMake build compiles with exceptions and RTTI off, which is what keeps that property honest.

Test framework: **doctest**, configured with `DOCTEST_CONFIG_NO_EXCEPTIONS` to match. Reason: single header, vendored into `third_party/`, no package manager and no separate compiled library step, and the fastest compile of the credible options. Catch2 v3 is the alternative and is better documented, but it is a compiled library that adds a dependency-resolution step to a repository whose CI is meant to run with nothing installed beyond a compiler and CMake.

**2.1 [agent:opus] Sample and quality types.**
Deliverable: `Signal`, `SignalId`, `Sample` with value, unit, source identity, monotonic sequence, monotonic receive timestamp, optional source timestamp, and quality in `{valid, stale, unavailable, invalid}`.
Gate: unit tests assert that a default-constructed `Sample` is `unavailable` and that zero is never used as a missing-data stand-in; a `static_assert(!std::is_constructible_v<Sample, Value, Unit>)` in the test translation unit proves a sample cannot be constructed without a receive timestamp, which is a compile-time check rather than a runtime doctest assertion.

**2.2 [agent:opus] Signal registry with per-signal freshness deadlines and a single-writer concurrency contract.**
Deliverable: a registry holding the latest sample per signal, with a per-signal deadline evaluated against an injectable monotonic clock. Ownership is explicit: the acquisition side is the only writer. Readers never touch writer state directly; they take a double-buffered snapshot of the whole registry, published by the writer and swapped by the reader once per frame, so a reader sees one coherent set of values and never blocks on acquisition. Every sample carries a connection generation counter supplied by its connector; the registry rejects a sample whose generation is older than the current one, so a delayed sample queued before a reconnect cannot overwrite a newer value after it.
Gate: a test advances a fake clock past a 500 ms deadline and asserts the signal transitions `valid -> stale` without any new sample arriving, and that a wall-clock jump backwards or forwards does not change the outcome. A second test publishes a sample from generation N after the registry has advanced to generation N+1 and asserts the sample is rejected and counted, not applied. A third test asserts that a snapshot taken mid-write contains only values from one publication, never a mix.

**2.3 [agent:opus] Unit normalization at the boundary.**
Deliverable: conversion to SI internally (kelvin, pascal, m/s, rad/s) with declared source units at the connector edge; display units applied at render time only.
Gate: a table-driven test covering degC/degF/K, kPa/bar/psi, km/h/mph/m/s, rpm/rad/s round-trips within `|a - b| <= max(1e-5, 1e-6 * max(|a|, |b|))`, an absolute floor plus a relative term so that a round trip through zero degrees Celsius or zero kPa does not divide by zero; and a test asserting that an unknown unit string is rejected rather than passed through. The table includes zero and near-zero rows in every affine unit family precisely because the relative term degenerates there.

**2.4 [agent:opus] Rule engine over a JSON expression tree, unit-aware and quality-aware.**
Deliverable: an AST evaluator with a whitelisted operation set (comparison, arithmetic, boolean, `min`/`max`/`clamp`, `abs`, signal reference, literal), a depth limit of 32, a total node limit of 512 per rule, and no string-parsed expressions anywhere. Every literal carries a unit; a dimensionless literal must say so explicitly rather than by omission. Comparison and additive arithmetic require unit-compatible operands and multiplicative arithmetic composes units; compatibility is checked once at load time against the registry's declared signal units, not per evaluation. Literals are converted to SI at load time by the same conversion table as 2.3, so a threshold authored in degC is compared against a kelvin-normalized signal correctly rather than numerically. Any evaluation producing a non-finite result, including division by zero and overflow, yields quality `invalid` rather than a number.
Depth limit 32 and node limit 512 are chosen defaults, not measured ceilings.
Gate: tests prove that an unknown operation name is a load-time error naming the offending node path, that depth 33 and node count 513 are each rejected with the offending node path, that comparing a pressure literal against a temperature signal is a load-time error naming both units, that a threshold literal authored in degC against a kelvin-normalized signal evaluates at the correct physical point, that division by zero and an overflowing product each produce `invalid` rather than a finite number, and that evaluating an expression referencing a missing signal returns the declared missing-input result rather than a default number.

**2.5 [agent:opus] Hysteresis, debounce and missing-input policy, with a fixed evaluation order.**
Deliverable: per-rule hysteresis band, debounce window, and an explicit missing-input policy in `{hold_last, treat_unavailable, force_warn}`. Quality propagates: if any input to a rule is `unavailable` or `invalid`, the missing-input policy fires rather than the arithmetic. `hold_last` carries a mandatory maximum hold duration, after which the result becomes `unavailable` regardless of policy, so a rule cannot report a stale "safe" value forever. The evaluation order is fixed and documented as: quality check, missing-input policy, evaluate, hysteresis, debounce. Hysteresis and debounce state, and any `hold_last` timer, reset when the connection generation changes, so a reconnect does not inherit pre-disconnect latching.
Gate: a test drives a value oscillating across a threshold at 20 Hz and asserts the rule output changes at most twice over the window; a second test asserts a rule with `treat_unavailable` reports unavailable, not false, when its input goes stale; a third asserts a `hold_last` rule flips to `unavailable` at its maximum hold duration and not before; a fourth asserts an `invalid` input triggers the missing-input policy rather than being evaluated as a number; a fifth asserts hysteresis and debounce state clear on a generation change. Boundary fixtures cover a value exactly on the threshold, exactly on each hysteresis band edge, and a transition arriving exactly at the debounce window boundary.

**2.6 [agent:opus] Display-only bounded interpolation.**
Deliverable: an interpolator with a per-signal maximum extrapolation window, disabled by construction for discrete signals, and with no path into rule evaluation.
Gate: a test asserts a rule evaluates on the raw sample while the display value differs; a second asserts the interpolator stops advancing at the window bound and does not cross into the stale period; a discrete signal configured for interpolation is a configuration error.

**2.7 [agent:opus] Record and replay format, version 1.**
Deliverable: JSON Lines, one sample per line, with a format version field in a header line and, per sample, monotonic receive timestamp, signal id, value, unit, source, quality, monotonic sequence number, optional source timestamp and connection generation. The full `Sample` from 2.1 round-trips; nothing that affects freshness, ordering or reset semantics is dropped by the writer. Writer and reader in `signal-core`.
Replay semantics, specified rather than left to the implementation: the reader rebases the recorded monotonic timestamps onto the injectable clock at start so the first sample plays at t=0; inter-sample gaps are preserved at their recorded durations rather than compressed, so an idle gap produces a real stale transition on replay; at end of file the replay holds the last sample and lets each signal go stale at its own deadline, and does not emit a disconnect unless one was recorded; an explicit loop flag restarts the file and increments the connection generation so rule and debounce state resets exactly as it would on a real reconnect. Seek is out of scope for Stage 0.
Gate: record a 60-second scenario, replay it, and assert the replayed sample stream is byte-identical after a round trip through writer and reader. A second test replays a file containing a 3-second gap and asserts the affected signal reports `stale` at its deadline inside the gap and `valid` again on the next sample, with the transition times checked against the injected clock. A third test asserts the hold-then-stale behaviour after EOF at the configured deadline. A fourth test asserts that replaying with the loop flag resets hysteresis, debounce and `hold_last` state at the wrap point.

**2.8 [agent:opus] Deterministic scenario generator.**
Deliverable: `idle`, `acceleration`, `high_temperature`, `missing_signal`, `disconnect`, `reconnect`, `stale_heartbeat`, each seeded and reproducible.
Gate: running each scenario twice with the same seed produces identical JSONL output (`fc /b` on Windows, `cmp` on Linux, no differences).

**2.9 [agent:opus] binary-telemetry-v1 parser.**
Deliverable: a receive-only parser for the existing 16-byte frame format: tag `44 33 22 11`, little-endian u32 frame id in the range 0xC80 to 0xC94 inclusive (21 identifiers), 8-byte payload as four little-endian u16 words, per-frame signed default with per-value override.
Pack contract, made explicit so the parser, the fixture pack, the converter and the reference decoder cannot disagree:
- The pack enumerates the valid frame identifiers one by one. The parser's valid set is exactly that enumeration, not a range test, so an identifier inside the numeric span but absent from the pack is treated as unknown.
- A field's byte offset origin is payload byte 0, not the start of the wire frame; a field's declared length is in payload bytes; the pack's frame `length` is the payload length, and the wire frame is that length plus the 8-byte tag and identifier header.
- `type` carries the field width. Where `type` and an explicit per-value signedness disagree, the per-value signedness wins over the frame default and over the type's own sign; the width always comes from `type`.
- Conversion is affine: `physical = raw * scale + offset`, with `offset` defaulting to 0. The existing schema's `V*<float>` scaling expressions convert to a numeric `scale` with `offset` 0 at conversion time in 3.7; the parser never evaluates an expression string.
- A field may declare an acquisition semantic of `live` or `held` and an optional list of raw sentinel values. A raw value matching a sentinel is rejected before conversion and publishes as `unavailable`, never as a converted number. A `held` field publishes with an age-unknown marker and a repeated identical payload on a `held` field does not refresh the signal's receive timestamp, so a held measurement cannot stay `valid` indefinitely because the relay keeps resending it.
Resynchronization after garbage requires all of: the 4-byte tag, a frame identifier present in the pack's enumerated set, and, when at least 16 further bytes are buffered, the next tag exactly 16 bytes later. A tag-shaped byte sequence inside a payload therefore does not lock the parser onto payload data.
Definitions come from a loaded definition pack, never from hardcoded tables. The tests load a hand-written pack at `tests/fixtures/packs/binary-telemetry-v1.test.json` that validates against 3.1's `definition-pack.schema.json` and covers a signed frame, an unsigned-override field, a scaled field, a field with a non-zero offset, a `held` field and a field with a sentinel list; it is a test fixture, not the converted schema from 3.7.
Gate: tests cover split records across two reads, two coalesced records in one read, a corrupted tag followed by a valid record, a payload containing the byte sequence `44 33 22 11` which must not be mistaken for a frame start, a negative temperature via the signed default, an unsigned override field inside an otherwise signed frame, a field with a non-zero offset decoding to the expected physical value, a sentinel raw value publishing `unavailable` rather than a converted number, and a frame id absent from the pack's enumerated set being dropped with a counter increment rather than an exception.

**2.10 [agent:opus] Relay heartbeat handling and held-value traffic.**
Deliverable: explicit classification of the existing relay's synthetic status records so they update transport health but never update a signal's receive timestamp. Separately, handling for ordinary frames carrying held measurements: a repeated identical payload on a field declared `held` updates transport health and acquisition health but does not refresh that signal's receive timestamp, and the signal renders with the age-unknown marker for as long as the value is held.
Gate: two independent tests. First, 30 seconds of heartbeat-only traffic leaves transport state `connected` while every mapped signal transitions to `stale` at its deadline. Second, 30 seconds of ordinary well-formed frames whose `held` fields repeat an identical payload leaves transport state `connected` and acquisition state receiving, while those signals still transition to `stale` at their deadlines and carry the age-unknown marker throughout; a `live` field in the same frame stream stays `valid`. These are the two most important correctness tests in WP2: a live socket is not evidence of fresh data, and neither is a well-formed frame.

**2.11 [agent:opus] Malformed-input harness.**
Deliverable: a fuzz-style harness feeding random and mutated bytes to the parser with a bounded corpus checked into `tests/fixtures/`.
Gate: no crash, no unbounded allocation and no hang under two run modes. In CI the harness stops at whichever comes first of 1 million mutated inputs or 30 seconds, and prints the count it reached so a throttled runner produces a recorded number rather than a timeout. Offline the same harness runs 10 million inputs and its result is recorded in the Stage 0 report.

**2.12 [agent:fable] Differential check against the owner's reference decoder.**
Deliverable: a comparison report for the synthetic record set, new parser output versus the existing reference decoder output, per field. The synthetic set covers every one of the 21 enumerated frame identifiers and, within each frame, every field's boundary values: minimum, maximum, zero, and the value one least-significant bit either side of any sign boundary.
Gate: every enumerated identifier and every field appears in the report, and every field matches within the declared scaling precision, or each mismatch is listed with a written cause. An identifier or field absent from the synthetic set fails this gate rather than passing silently. Run offline from the owner's separate repository; nothing from it is copied into this one.

### WP3 dashboard-spec v0

**3.1 [agent:opus] JSON Schema draft-7 for the five documents, plus a shared semantic pass.**
Deliverable: `manifest.schema.json`, `dashboard.schema.json`, `signals.schema.json`, `showcase.schema.json` and `definition-pack.schema.json` in `packages/dashboard-spec/schema/`. The definition-pack schema covers the subset of the PLUGIN-EXPERIENCE.md format that binary-telemetry-v1 needs: pack id and version, input type, an explicit enumeration of frame identifiers with each frame's payload length, and fields with byte offset measured from payload byte 0, type carrying the field width, byte order, per-value signedness, `scale`, `offset`, unit, an acquisition semantic in `{live, held}` and an optional list of raw sentinel values. Conversion is affine, `physical = raw * scale + offset`. Bit fields and multiplexing are not in the Stage 0 schema.
Components are an ID-keyed map rather than an array of objects carrying an `id` property. Draft-7 cannot express uniqueness of a property across array items: `uniqueItems` compares whole objects, so two components with the same ID and any other difference both survive. Making the ID the object key moves duplicate detection into the JSON parser, where both implementations can be configured to reject a duplicate key rather than silently keeping one.
Alongside the schemas, a written specification of a **semantic pass** that runs after schema validation and is implemented identically by the Python validator in 3.3 and the C++ validator in 3.4: duplicate object keys rejected at parse time; every signal binding, rule signal reference, theme token reference and page reference resolved against the documents actually present; every definition-pack field checked so that `byte_offset + field_width <= frame.payload_length`; aspect policy resolved through container inheritance so an inherited `stretch` on a circular gauge is caught, not just a directly declared one; expression node totals and depth checked against the 2.4 limits. Each failure reports a JSON pointer, the same pointer in both implementations.
Coverage for Stage 0: stable component IDs; a reference viewport with explicit anchoring, scaling, clipping and aspect policy including an aspect policy value that forbids non-uniform scaling of circular gauges; the primitives text/numeric readout, image, line/shape, analog dial, arc/bar gauge, indicator, short history graph, container, page switch; day/night theme tokens; explicit missing-data rendering per component including a distinct age-unknown presentation for a signal fed by a `held` field; bindings to signals; rules as the WP2 AST.
Gate: every schema validates as draft-7 itself; schema plus semantic pass rejects a document with a duplicate component key, a document whose two same-ID components differ in their other properties, a document with a circular gauge under a directly declared `stretch` aspect policy, a document with a circular gauge inheriting `stretch` from its container, a definition pack whose field runs past its frame's payload length, and a rule referencing a signal no loaded document declares. Each rejection names the JSON pointer.

**3.2 [agent:opus] Shared fixture corpus.**
Deliverable: `tests/fixtures/documents/valid/*.json` and `.../invalid/*.json`, each invalid fixture carrying a sibling `.expected` file naming the JSON pointer that must fail. The invalid set includes the hostile and semantic cases specifically: a document nested past the depth limit, a document past the node-count limit, a document with an over-long string, a duplicate component key, two same-ID components differing in other properties, a definition-pack field running past its frame's payload length, an inherited `stretch` aspect policy on a circular gauge, an asset reference containing `..`, an absolute asset path, a drive-lettered asset path, two asset references normalizing to the same entry, and two entries differing only in case.
Gate: at least 20 valid and 30 invalid fixtures; every invalid fixture's failure points at the pointer in its `.expected` file; and every limit named in 3.6 and every check named in 3.1's semantic pass has at least one invalid fixture exercising it.

**3.3 [agent:opus] Python validator.**
Deliverable: `tools/validate-dashboard.py` using `jsonschema`, usable as a CLI and as a CI step, running schema validation followed by the 3.1 semantic pass and enforcing the 3.6 document limits before either. Duplicate object keys are rejected at parse time through a custom `object_pairs_hook`, since Python's default JSON decoder keeps the last duplicate silently.
Gate: `python tools/validate-dashboard.py tests/fixtures/documents` exits 0 and prints counts of valid and invalid fixtures matching the corpus contents; it exits non-zero if any fixture lands on the wrong side; and every semantic-pass fixture fails at the JSON pointer in its `.expected` file rather than merely failing.

**3.4 [agent:opus] C++ validator, engine-independent, compiled twice.**
Deliverable: one set of C++ validator sources in `packages/dashboard-spec/src/`, implementing schema validation plus the 3.1 semantic pass and the 3.6 document limits, with **RapidJSON** and **valijson** vendored into `packages/dashboard-spec/third_party/` at pinned revisions rather than taken from the engine. Two builds of the same sources:
- An engine-independent CMake target in `packages/dashboard-spec`, built with exceptions and RTTI off, producing a command-line validator that runs on hosted CI with no Unreal install. This is the build that proves the validator is engine-independent, and it is the one 3.5 runs.
- An Unreal module that compiles the same source files through UBT, so the player and the CI validator are the same code rather than two implementations that happen to agree today.
RapidJSON is configured to report duplicate object keys as errors rather than accepting the last one, matching 3.3.
Exceptions stay off by default. Unreal supports `bEnableExceptions` per module, and if valijson turns out to require exceptions, that flag is set on this one isolated validator module and nowhere else in the player. It is not set speculatively: a mobile player module pays code-size and unwind cost for exceptions, and a feasibility build should not pay it unless the no-exceptions build has actually failed to compile.
Deliverable also includes a capped malformed-document harness for this validator, mirroring 2.11's shape: mutated and randomly generated JSON fed to the C++ validator with a bounded corpus in `tests/fixtures/`, stopping at whichever comes first of 1 million inputs or 30 seconds in CI.
Gate: the CMake target builds with exceptions and RTTI off on `windows-latest` and `ubuntu-latest`; the same corpus run through it produces identical pass/fail results and identical failing pointers to the Python validator; the Unreal module compiles the same sources on Win64; and the malformed-document harness completes its CI budget with no crash, no unbounded allocation and no hang. If the no-exceptions CMake build fails, the gate is met instead by the exceptions-enabled isolated module plus a recorded note of why, and the engine-independent CI build is restored in Stage 1.

**3.5 [agent:opus] Validator parity test in CI.**
Deliverable: a CI job on hosted runners that runs the Python validator and the engine-independent C++ validator from 3.4 over the corpus and diffs their outputs, pass/fail and failing pointer per fixture.
Gate: the diff is empty. Any schema or semantic-pass change that does not update both validators fails this job. This is the mechanism that stops the two implementations drifting. If neither the no-exceptions CMake build nor an exceptions-enabled build of the C++ validator compiles, this task is **blocked**, not satisfied by a reduced check: there is no structural-only fallback, and a Stage 0 that cannot validate documents in the player says so in the report.

**3.6 [agent:opus] `.udash` package reader with bounds, and one asset path resolver.**
Deliverable: a ZIP reader enforcing bounded expansion before activation, and a single package-root resolver through which every asset reference in every loaded document passes.
Stage 0 limits, all chosen defaults rather than measured ones. They are one shared limit set, applied identically to a `.udash` archive and to an unpacked directory given with `-udash=`, so the unpacked path is not an unbounded bypass of the packed one:

| Limit | Default |
| --- | --- |
| Total expanded size | 64 MB |
| Entry count | 4096 |
| Bytes per asset | 16 MB |
| Bytes per JSON document | 4 MB |
| JSON nesting depth | 64 |
| JSON node count per document | 200,000 |
| JSON string length | 64 KB |
| Expression depth | 32 |
| Expression nodes per document | 20,000 |
| Components per document | 2000 |
| History samples per graph component | 4096 |
| Image pixel dimensions | 4096 by 4096 |
| Aggregate decoded texture budget, mobile profile | 192 MB |
| Aggregate decoded texture budget, desktop profile | 512 MB |

The JSON limits are enforced during parsing, before schema validation, so a deeply nested document cannot overflow the parser stack on the way to being rejected. The decoded texture budget is computed from declared pixel dimensions and format before any decode or upload is attempted, and a package whose declared images exceed the active profile's budget is rejected rather than partially loaded.
Path handling: every asset reference resolves through one resolver rooted at the package root. The resolver rejects any reference or archive entry that is absolute, contains `..` after normalization, contains a drive letter, is a symbolic or hard link, resolves outside the root, duplicates another entry after normalization, or collides with another entry under case-insensitive comparison. There is no second path that reaches an asset; a document cannot name a file the archive gate never inspected.
Gate: a test corpus is rejected with a distinct error code per case for the zip bomb, the `../../etc/passwd` entry, the 20 MB asset, the 5000-entry archive, the document nested past 64, the document past the node-count limit, the over-long string, the over-wide image, the declared texture set over the mobile budget, the document referencing `../../outside.png` from inside an otherwise well-formed archive, the entry pair colliding after normalization and the entry pair colliding only by case. Every one of those cases is run twice, once as a `.udash` archive and once as an unpacked directory, and both runs produce the same error code. A well-formed package loads in both forms.

**3.7 [agent:opus] Offline converter for the existing schema.**
Deliverable: `tools/convert-existing-schema.py`, reading the owner's XML from a path given on the command line and emitting a UnRealDash definition pack in the format sketched in PLUGIN-EXPERIENCE.md: an explicit enumeration of frame ids, each frame's payload length, and per field the offset from payload byte 0, type, byte order, scale, offset, unit and signedness.
Gate: the emitted pack validates against the definition-pack schema, enumerates every frame id the source XML defines, and the WP2 parser loaded with it decodes the synthetic record set to values matching the reference decoder. Scaling expressions of the form `V*<float>` are converted to a numeric `scale` field with `offset` 0 at conversion time; the player never evaluates an expression string from that XML, and the XML is never read by the player. Source XML stays in the owner's repository; reuse of its content is the owner's decision and is recorded in **Assumptions**.

**3.8 [agent:fable] Arbitrate any parity mismatch between the two validators.**
Deliverable: for each mismatch, a written ruling on which validator is correct and which fixture or schema changes.
Gate: zero unresolved mismatches before WP4 starts consuming the schema.

### WP4 Player: control dashboard

`runtime/UnRealDash/` is a C++ Unreal project. The control dashboard UI is built in C++ at runtime from the loaded document as a UMG widget tree. No hand-authored widget Blueprints for the control dashboard. Binary content in this work package is limited to what the engine requires to open a project.

**4.0 [agent:opus] Android and Windows smoke spike, on the desk.**
This runs immediately after the project skeleton exists and the Android toolchain is installed, before the schema, the primitives, the connectors or any authored content are built on top of it. Its purpose is to find out early whether an Unreal package from this project runs on Android at all and whether the on-device workflow the rest of the plan assumes works on an unrooted Android device. Discovering either failure after nine primitives and a showcase have been built is the expensive order. The desk device is the owner's Pixel 10 Pro (Tensor G5, Imagination PowerVR GPU, Android 16, Vulkan 1.3 or later), plugged into the workstation while the owner is present. It proves packaging and the file, argument and export workflow; it proves nothing about the head unit's Mali-G57 driver, which is what 4.0b is for.
Deliverable: a minimal scene containing exactly three things that represent the rest of the plan's rendering surface: one live value from the in-process simulator, one runtime-imported PNG texture, and one dynamic material instance driven by that value. Packaged for Windows x64 and for Android ARM64 under both RHIs, Vulkan and OpenGL ES 3.2, and run on the head unit. The spike proves the on-device workflow end to end:
- `android.permission.INTERNET` is declared through `DefaultEngine.ini` or a UPL fragment and is present in the built manifest, so the 4.9 TCP connector is not blocked by an OS permission failure found in WP6.
- Packages, documents, screenshots and metrics CSVs live under app-specific external storage, `/sdcard/Android/data/<package>/files/`, which needs no runtime permission and is reachable by non-root `adb push` and `adb pull`. `run-as` is unavailable on this device for a Shipping build, so nothing depends on `/data/data/<package>/`.
- Launch arguments arrive two ways: through `UECommandLine.txt` in Development configuration, and through a `player.json` config file read from app-specific external storage in every configuration including Shipping.
- One Shipping build launches and selects its package, scenario and output paths from `player.json` alone, with no command line at all.
Gate: on Windows, the packaged scene renders and writes a metrics CSV. On the desk device, both the Vulkan APK and the GLES APK install, launch, render the value and the textured material, and write a screenshot and a CSV to app-specific external storage that `adb pull` retrieves without root. `aapt dump badging` shows `android.permission.INTERNET`. A Development build takes its arguments from `UECommandLine.txt`, and a Shipping build takes them from `player.json` with no command line. Any launch failure produces a committed `logcat` under `docs/reports/logs/` and is recorded as a Stage 0 finding rather than worked around silently. If the desk device is unavailable, 4.0 and 4.0b merge into one owner-assisted run on the head unit.

**4.0b [human] The same smoke spike on the head unit.**
Deliverable: the two APKs from 4.0, unchanged, installed and run on the head unit over network ADB under both RHIs.
Gate: both APKs install, launch, render the value and the textured material, and write a screenshot and a CSV that `adb pull` retrieves; the active RHI and the GPU driver string are logged from inside the app and match the intended path. Any crash produces a committed `logcat` under `docs/reports/logs/`. If both RHIs fail to render on the head unit, that result stops WP5 and the device half of WP6 and goes to the owner before more is built on the assumption that they work; WP4's data-driven work may continue against Windows and the desk device in the meantime, because it is portable to whatever runtime the report recommends.

**4.1 [agent:opus] Unreal C++ project skeleton.**
Deliverable: project with a primary game module, a `UnRealDashCore` module wrapping `signal-core` and `dashboard-spec`, Windows/Android/Linux target settings, and mobile rendering configured without Lumen, Nanite or ray tracing.
Gate: `scripts/build.ps1` produces a Development Editor binary and a Development Win64 binary; the build log shows zero warnings promoted to errors in the project's own modules.

**4.2 [agent:opus] Compile `signal-core` directly into the Unreal module.**
There are no prebuilt `signal-core` libraries and no CMake toolchain files. UBT compiles the sources itself, with Epic's own compiler, sysroot, standard library and runtime flags for each target, which removes the ABI boundary rather than pinning both sides of it.
Deliverable: the signal-core sources live at `runtime/UnRealDash/Source/SignalCore/`, a plain Unreal module whose source files include no Unreal headers, no Unreal types and no engine macros. The one exception is a single module-registration file carrying the `IMPLEMENT_MODULE` boilerplate, and the CMake build excludes that file. `packages/signal-core/` holds `CMakeLists.txt`, the doctest suite and a README, and compiles those same source files from their location in the runtime tree. That CMake build is what enforces engine independence: if someone adds an Unreal header to a signal-core source file, the hosted CI build stops compiling, immediately and visibly.
The CMake build compiles with exceptions and RTTI off. signal-core is exception-free by design: every fallible operation returns an error code or a result type, and nothing in the library throws. doctest is configured with `DOCTEST_CONFIG_NO_EXCEPTIONS`, so the test suite reports failures without unwinding.
Deliverable also includes a thin adapter translating `signal-core` types into whatever the render side consumes, living in the player module rather than in signal-core.
Gate: `packages/signal-core`'s CMake build compiles the same files with exceptions and RTTI off on `windows-latest` and `ubuntu-latest` and its doctest suite passes; a grep across `runtime/UnRealDash/Source/SignalCore/` finds no Unreal header include outside the excluded module-registration file; UBT builds the module for Win64, Android ARM64 and Linux x86-64; and an in-engine smoke commandlet runs a representative subset of the signal-core checks in-process on Win64, covering freshness transitions, unit normalization and rule evaluation, and prints the assertion count and the failure count. The gate is the assertion count matching the CMake run's count for the same subset with zero failures, not a version string.

**4.3 [agent:opus] Connector interface and acquisition threading.**
Deliverable: a C++ connector interface with discover/connect/start/stop/reconnect/health, running off the game thread, publishing into the registry with receive time, source identity and connection generation.
The threading contract is explicit. The acquisition side owns the registry and is its only writer. Every sample that arrives is evaluated against the rules on the acquisition side, at sample rate, before any coalescing for display: a warning condition that exists for one sample at 200 Hz latches and is not lost because the renderer only looked 16 ms later. Latched warning state clears on acknowledgement or on a generation change, per 2.5. The bounded queue and its drop policy sit between acquisition and display, not between the wire and rule evaluation, so dropping for display cost never erases a rule transition. The game thread reads the double-buffered snapshot from 2.2, swaps it once per frame, and never blocks on the acquisition thread, a mutex held across I/O, or the socket.
Gate: a test scenario publishing at 200 Hz into a 60 Hz renderer shows a bounded queue depth, a non-zero recorded display-coalescing count, every rule transition present in the rule output despite the coalescing, and no growth in resident memory over 10 minutes. A second test stalls the consumer for 2 seconds mid-stream and asserts the queue stays bounded, the producer never blocks and no rule transition is lost. A third test forces a reconnect while samples from the previous generation are still queued and asserts none of them is applied. This whole test runs under ThreadSanitizer on the Linux CI runner per 1.4 and must report zero data races.

**4.4 [agent:opus] Package loader.**
Deliverable: load a `.udash` package or an unpacked directory from `-udash=<path>`, apply the 3.6 limit set and the 3.6 asset resolver to both forms, validate through the 3.4 C++ validator and its semantic pass, and build the widget tree.
Gate: launching with a valid package renders, in both packed and unpacked form; launching with each of the WP3 rejection cases shows a readable on-screen error naming the file and the JSON pointer and does not crash; the same rejection cases produce the same error codes whether given as an archive or as a directory. The device half of this gate runs on the head unit using the 4.0 workflow, not only on Windows.

**4.5 [agent:opus] Runtime UMG builder for the Stage 0 primitives.**
Deliverable: C++ construction of text/numeric readout, image, line/shape, indicator, container and page switch, with day/night tokens and explicit missing-data rendering, including the distinct age-unknown presentation for a signal fed by a `held` definition-pack field, visibly different from both a valid reading and a stale one.
Gate: a fixture document exercising every primitive renders; a screenshot captured via `-screenshot=` matches a checked-in reference within a per-pixel tolerance recorded in the test; and three captures of the same numeric readout under valid, held and stale conditions are distinguishable from one another by the same per-pixel comparison.

**4.6 [agent:opus] Analog dial, arc/bar gauge and short history graph.**
Deliverable: the three remaining primitives, with the dial's aspect policy enforced so a circular gauge is never non-uniformly scaled.
Gate: rendering the same document at 1280x720 and at 1280x660 produces a dial whose measured width and height ratio stays within 1 percent of 1.0 in both captures.

**4.7 [agent:opus] Command-line surface.**
Deliverable: `-udash=<path>`, `-scenario=<name>`, `-replay=<file>`, `-replay-loop`, `-connector=<sim|replay|tcp>`, `-connector-host=<ip>`, `-connector-port=<port>`, `-screenshot=<path>`, `-quit-after=<seconds>`, `-profile=<mobile|desktop>`, `-freeze-scenario-time`, and on Android `-rhi=<vulkan|gles>`. `-connector-host` and `-connector-port` default to 127.0.0.1 and 35000. Every flag is also settable as a key in the `player.json` config file from 4.0, since a Shipping Android build has no command line; the command line wins where both are present.
Bench runs from the device against a relay on the workstation use `adb reverse tcp:35000 tcp:35000` so the default loopback host keeps working on the device; the alternative is `-connector-host=<workstation ip>` over driveway Wi-Fi. Both are documented in `scripts/README.md` and the `adb reverse` form is the one 6.9 uses, because it does not depend on the car's Wi-Fi association.
Gate: each flag has a test in `scripts/capture-metrics.ps1`'s smoke mode; an unknown flag prints the supported list and exits non-zero rather than being ignored; an unknown key in `player.json` does the same; and the same run is reproduced once from the command line and once from `player.json` with identical results.

**4.8 [agent:opus] Debug overlay and raw event output.**
Deliverable: an on-screen overlay showing fps, rolling frame time p95 and p99, missed frames, resident memory, texture memory and per-signal freshness state, plus a raw event log appended incrementally to disk and flushed at least once per second, so a crash or a kill leaves the evidence up to that second rather than an empty file. Rolling percentiles are for the overlay only; nothing downstream consumes them.
The event log carries per-frame rows (frame index, frame start, frame time, present timestamp, missed-frame flag, resident memory, texture memory) and per-sample rows (signal id, receive timestamp, sequence, connection generation, quality, and the present timestamp of the first frame that displayed it). From those two row types 6.5 computes whole-run percentiles; a rolling p95 cannot be re-aggregated into a run p95, which is why the raw rows exist.
Timestamp definitions, so the numbers mean the same thing on both platforms: **receive-to-present** is the sample's receive timestamp subtracted from the present callback timestamp of the first frame that displayed that sample, both taken from the same monotonic clock in the same process. **Launch-to-first-usable-value** is process start to a logged first-usable-frame event, where first-usable means the first present callback for a frame in which at least one bound signal rendered a `valid` value; on Android process start comes from `am start -W`, on Windows from the process creation time, and the event log records both endpoints.
Gate: a 60-second run writes an event log whose per-frame row count matches the frame count within 1 percent and whose per-sample rows cover every published sample; the log validates against its declared column schema; every timestamp column is present, monotonic and positive; and a run killed at 30 seconds leaves a readable log containing at least 29 seconds of rows. Run-to-run percentile stability is deliberately not a gate: p95 tail latency varies with background OS work, compositor behaviour and thermal state by more than any threshold worth writing down.

**4.9 [agent:opus] Wire the three Stage 0 connectors.**
Deliverable: the in-process simulator, the file replay connector, and a TCP client connector for the `-connector-host`/`-connector-port` endpoint, defaulting to `127.0.0.1:35000`, speaking binary-telemetry-v1, receive-only, using the WP2 parser and the WP3-generated definition pack. The socket is non-blocking and polled; `connect` never blocks the acquisition thread. On disconnect the connector retries with exponential backoff from 500 ms to a 5 s ceiling rather than spinning, increments the connection generation on each successful reconnect, and reports its backoff state through the health interface.
Gate: with a local mock relay replaying recorded bytes, the player shows live values; killing the mock marks every mapped signal stale within its deadline; restarting the mock reconnects without a player restart. A test with the relay absent for 60 seconds asserts the retry intervals follow the backoff schedule to the 5 s ceiling and that the acquisition thread's CPU time over that window stays near zero, so an absent relay is not a spin loop. The TCP connector never writes to the socket, proven by a test that asserts zero bytes sent over a 5-minute session.

**4.10 [agent:opus] Control dashboard example document.**
Deliverable: `examples/control-dashboard/` with 12 active instruments plus indicators at a 1280x720 reference viewport, per the PROJECT-PLAN section 8 standard-dashboard gate.
Gate: the document validates, renders, and its component count is at least 12 instruments plus 4 indicators.

**4.11 [agent:fable] Verify edit-without-rebuild for the control dashboard, on Windows and on the device.**
Deliverable: a recorded procedure and, per platform, two screenshots showing a `dashboard.json` edit taking effect in the packaged player with no rebuild and no repackage. The Android half uses the 4.0 workflow: `adb push` the edited document into app-specific external storage, relaunch, `adb pull` the screenshot.
Gate: both captures are taken with `-freeze-scenario-time` at the same frozen scenario timestamp, so a difference cannot come from telemetry advancing between them; a recursive hash over the entire packaged directory, executable plus cooked content plus every packaged asset, is identical before and after the edit, so a changed cooked asset cannot masquerade as a document-driven change; and the two screenshots differ. The gate is met only when it passes on Windows and on the head unit.

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
Deliverable: a state transition over a configurable duration, with interruption handling and a reduced-motion setting. The player emits a per-frame state trace during the transition carrying transition progress, the visible-value flag for the speed readout, and each instrument's transition state.
Gate: triggering the transition twice in rapid succession leaves no instrument mid-transition, asserted from the state trace at every frame after the settle point rather than from a capture. The speed value's visible-value flag is true on **every** frame of the trace from trigger to completion, which is what "stays on screen throughout" means; three captures cannot establish it. Screenshots at 0 ms, mid-transition and after completion are kept as evidence for the report, not as the gate.

**5.6 [agent:opus] Day and night themes, and a skippable startup reveal.**
Deliverable: coordinated palette, lighting and material response per theme; a startup reveal that any input skips and that never delays the first usable value.
Gate: with the reveal enabled, the first-usable-frame event occurs no later than with the reveal disabled, read from 4.8's event log across five runs each.

**5.7 [agent:opus] Disconnect scenario marks instruments stale.**
Deliverable: an explicit stale presentation for the showcase instruments, distinct from a valid reading. The per-frame state trace from 5.5 carries each needle's angle and each instrument's quality presentation.
Gate: running `-scenario=disconnect` produces a state trace in which, from the configured deadline onwards, every affected instrument is marked stale on every frame and every affected needle angle is unchanged frame over frame for the remainder of the run. A single capture cannot prove a needle stopped moving; the trace can. A capture at the deadline plus 100 ms is kept as evidence for the report.

**5.8 [agent:opus] `showcase.json` sidecar with a bounded parameter set.**
Deliverable: material scalar values, transition durations, page selection and colour tokens, all schema-validated against declared ranges, with no ability to add geometry. Out-of-range values are **rejected at load**, not clamped. Clamping accepts a document the author did not mean and renders something they did not ask for; for a bounded authored parameter set, refusing to load and naming the parameter is the behaviour that surfaces the mistake.
Gate: an out-of-range value is rejected at load with a message naming the parameter and its JSON pointer, and nothing renders from that document; no code path clamps a sidecar value, asserted by a test that feeds a value one unit outside each declared range and requires a rejection for each; a value inside the range changes the render.

**5.9 [agent:fable] Verify edit-without-rebuild for the showcase.**
Deliverable: two screenshots per platform and the procedure, as in 4.11, for a `showcase.json` edit, on Windows and on the head unit.
Gate: as 4.11. Captures taken with `-freeze-scenario-time` at the same frozen timestamp, a recursive hash over the whole packaged directory unchanged, screenshots differ, and the gate met on both platforms.

### WP6 Packaging and device qualification

This work package produces the numbers. Nothing before it may be quoted as a measurement.

**6.1 [agent:opus] Package for Windows x64.**
Deliverable: a Shipping Win64 package produced by `scripts/package-windows.ps1`.
Gate: the packaged executable launches with `-udash=examples/control-dashboard -scenario=idle -quit-after=60` and writes a complete 4.8 event log.

**6.2 [agent:opus] Package for Android ARM64, Vulkan.**
Deliverable: an APK with Vulkan as the default RHI, minimum SDK 26, target SDK 35.
Gate: `aapt dump badging` shows `minSdkVersion:'26'` and `targetSdkVersion:'35'`, and the APK installs on the device.

**6.3 [agent:opus] Package for Android ARM64, OpenGL ES 3.2.**
Deliverable: a second APK with GLES 3.2 as the packaged rendering path.
Gate: the APK installs and its manifest shows the GLES 3.2 requirement. Both APKs are kept; this is the fallback path named in **Risks**, not an afterthought.

**6.4 [human] Deploy and launch on the reference device across the qualification matrix.**
Deliverable: the full matrix run on the head unit: two dashboards (control, showcase) by two RHIs (Vulkan, GLES 3.2), four cells. Each cell produces a screenshot, an event log from 4.8, and a startup record. The application logs its **active** RHI name, the GPU driver version string and the graphics API version from inside the process at startup into the event log; an APK manifest and a screenshot do not establish which RHI actually ran, since a Vulkan package can fall back at runtime.
Suspend and resume is a gate, not a note: the device is put to sleep and woken with the player running, in each cell.
Gate: all four matrix cells produce a screenshot, an event log and a logged active-RHI and driver string that matches the RHI the cell intended, or the mismatch is recorded as a finding. In each cell, suspend and resume returns to a rendering player within 5 seconds with signal freshness correctly re-evaluated rather than resuming with pre-suspend values shown as valid. Any crash produces a `logcat` capture committed under `docs/reports/logs/`. A cell that cannot run at all is recorded as failed, not omitted.
Cost note: the device is installed in the car. Every render check on it is a trip to the driveway over network ADB.

**6.5 [agent:opus] Metrics capture harness.**
Deliverable: `scripts/capture-metrics.ps1` driving a scenario on a named target and emitting the PROJECT-PLAN section 8 table, with build id, device, resolution, power mode, active RHI, driver string, temperature and sample size in the header. Every run-level statistic is computed here from 4.8's raw per-frame and per-sample rows over the whole run: frame time p95 and p99, missed-frame count, receive-to-present p95, resident and texture memory trend. The harness never copies a rolling percentile out of the overlay.
Launch timing: on Android the harness reads launch-to-first-usable-value from `am start -W` plus the first-usable-frame event; on Windows from process creation time plus the same event.
Temperature sources are named per platform rather than assumed: Android through `adb shell dumpsys thermalservice`, Windows GPU through `nvidia-smi`. Where a platform exposes no source for a cell, the harness writes `not measured` into that cell and records why in the header.
Gate: running it against a Windows package and against the device produces two tables with identical column sets and no blank cells, where `not measured` counts as a filled cell and an empty cell does not. Every percentile in the table is recomputed from the raw rows by a second, independent script run over the same event log and matches.

**6.6 [human] 60-minute thermal run on the device, once per working RHI.**
Deliverable: a continuous 60-minute run of the showcase with the thermal zone and resident memory sampled throughout, run once under Vulkan and once under GLES 3.2. Each run names its RHI in the header from the active-RHI string logged in-process, not from which APK was installed. If 6.4 found one RHI unusable, that run is recorded as not run with the reason, and the other still happens.
Gate: per run, an event log with at least 3600 memory and temperature samples, a plot or table of temperature and memory over time, and an explicit memory criterion: the least-squares slope of resident memory over the window from 10 minutes to 60 minutes, reported in MB per hour with its confidence interval, together with the peak and the final value. "Grew monotonically" is not the criterion, because a leak that grows overall while dropping at each garbage collection or texture eviction is not monotonic and is still a leak. Frame time p95 at or below 16.7 ms on this device is a **target**, not a promise; the report records what it actually was.

**6.6b [human] 8-hour desktop soak.**
PROJECT-PLAN section 8 asks for a 60-minute thermal test **and** an 8-hour desktop soak. Only the first had a task; this is the second. It is cheap: it runs unattended on the workstation overnight and needs no car access.
Deliverable: a continuous 8-hour run of the showcase on the Windows package driving a deterministic scenario, with the event log from 4.8 appended throughout and resident memory, texture memory and handle count sampled at least once per second.
Gate: the run completes 8 hours without a crash, a hang or a rendering stall longer than 1 second; the event log is continuous with no gap greater than 2 seconds; and the resident-memory slope over the window from 30 minutes to 8 hours is reported in MB per hour with its confidence interval, alongside the peak and the final value, using the same criterion as 6.6. A crash or a hang before 8 hours is recorded with its elapsed time and the log up to that point, and is a Stage 0 finding rather than a retry until it passes.

**6.7 [agent:opus] Runtime texture import cost.**
Deliverable: a measurement of decode time and GPU upload time for runtime-imported PNG assets at the sizes the showcase uses, on both Windows and the device. Upload is instrumented to its **completion**, not to the submission call: the event log records the timestamp at which the texture is usable for rendering, using the RHI's own completion signal, because an asynchronous submit returns long before the upload lands and would report a time that is not the cost.
Gate: a table of decode ms and upload-to-usable ms per asset size on both platforms, with at least 20 samples per cell, every sample traceable to an event-log row. PROJECT-PLAN section 4 flags this as unvalidated; this task is what validates it.

**6.8 [agent:opus] Linux x86-64 package.**
Deliverable: a Linux Shipping package cross-compiled from Windows with toolchain v26.
Gate: `scripts/package-linux.ps1` exits 0 and produces an ELF binary; `file` reports x86-64 ELF. Running it under WSL2 is attempted and the result recorded, but **is not a gate**: WSL2 Vulkan via Mesa dzn is documented as testing-only and can silently fall back to software rendering, so a successful or failed run there proves little either way.

**6.9 [human] Bench-first integration with the existing relay.**
Deliverable: the PILOT-INTEGRATION.md sequence executed in order: synthetic records against the new parser compared to the reference decoder (already covered by 2.12); disconnect, fragmentation, idle heartbeat, held data and reconnect exercised on the bench; workstation render against a replayed stream; device package; then the live relay in the car.
Gate: before two display applications run against the relay at once, a coexistence test records whether the existing dashboard client and the UnRealDash client can both hold a connection, and what backpressure each sees. If coexistence fails, the live-relay step runs with the existing client stopped and that is written into the report.
Direct USB is out of scope. The Feather firmware, its ECU polling and the existing logging are not touched.

**6.10 [agent:fable] Audit the metrics table.**
Deliverable: a pass over every number in the Stage 0 metrics table checking that each one is traceable to an event log and a run, that every percentile was computed from raw rows rather than copied from an overlay, and that no target has been written in as if it were a measurement.
Gate: every cell cites a source file and a run id, or is marked "not measured" with the reason no source existed.

### WP7 Stage 0 report and Stage 1 entry

**7.1 [agent:opus] Write `docs/reports/stage0-feasibility.md`.**
Deliverable: the PROJECT-PLAN section 8 metrics table filled in, the six Stage 0 exit conditions each marked met or not met with evidence, the Mali Vulkan result on both RHIs, the thermal and memory curves, the texture import numbers, and a list of everything that failed.
Gate: every exit condition in **Goal** above has an explicit met/not-met line with a linked artifact.

**7.2 [human] Go/no-go recommendation on Unreal.**
Deliverable: a signed recommendation section in the same report.
Gate: the section names the specific measured results that drive the recommendation, and states what would have to be true to reverse it.

**7.3 [agent:opus] Stage 1 entry design note.**
Deliverable: `docs/STAGE1-ENTRY.md` covering the localhost WebSocket command channel (load package, run scenario, screenshot; localhost-only binding, origin rejection, pairing) and package staging with atomic activation and rollback (stage, validate, activate, keep previous, fall back after repeated failed launches).
Gate: the note specifies all of the following, and a missing item fails the gate.
- **Command authorization.** Every command carries a pairing token established out of band; an unauthenticated command is refused. Localhost binding and origin rejection are necessary and not sufficient, since any local process can open a loopback socket.
- **Path restriction.** The load and screenshot commands accept paths only inside declared staging and output roots, resolved through the 3.6 resolver, so the command channel cannot be used to read or write arbitrary files.
- **Serialized activation.** Activation is single-flight. A second activation arriving during one in progress is refused or queued, never interleaved, and the note says which.
- **Definition of a successful launch.** Written explicitly, as a launch that reached the first-usable-frame event from 4.8 and stayed running for a stated minimum, not merely a process that started.
- **Health-based promotion.** A staged package becomes the default only after N consecutive successful launches by that definition; N is named. A package that launches and crashes shortly after is not promoted.
- **Stationary-only activation.** PROJECT-PLAN's constraint that an activation does not happen while the vehicle is moving, with the signal that establishes stationary and the behaviour when that signal is unavailable.
- **On-disk layout and crash-point test matrix.** The layout for staged and active packages, the activation ordering that makes a torn write survivable, the failed-launch counter that triggers rollback, and a matrix enumerating each point at which power can be lost during activation with the state the system must recover to from each one.
No implementation in Stage 0.

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
0.5 Turnkey Android   -> 4.0, 6.2, 6.3
0.6 Linux toolchain   -> 6.8
0.8 device recon      -> 4.0b, 6.4, 6.6, 6.9
0.9 verify            <- 0.7, 0.8, 0.1-0.6

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
3.4 CMake half        <- 3.1, 3.2 (no engine)
3.4 Unreal-module half<- 4.1
3.6 <- 3.1
3.7 <- 3.1, 2.9

4.1  <- 0.4, 0.3, 0.9
4.0  <- 4.1, 0.5
4.0b <- 4.0, 0.8
4.2  <- 4.1, 2.1-2.6
4.3  <- 4.2
4.4  <- 4.2, 3.4, 3.6, 4.0
4.5  <- 4.4
4.6  <- 4.5
4.7  <- 4.4
4.8  <- 4.7
4.9  <- 4.3, 2.9, 2.10, 3.7
4.10 <- 4.6
4.11 <- 4.10, 6.1

5.1  <- 4.1, 4.0b
5.2  <- 5.1, 1.2
5.3  <- 5.2, 4.4
5.4  <- 5.3, 5.8
5.5  <- 5.3, 4.6
5.6  <- 5.3
5.7  <- 5.3, 2.10
5.8  <- 3.1, 5.3
5.9  <- 5.8, 6.1

6.1  <- 4.9, 4.10, 5.4, 5.5, 5.6, 5.7, 5.8
6.2  <- 6.1, 0.5, 4.0b
6.3  <- 6.2
6.4  <- 6.2, 6.3, 0.8
6.5  <- 4.8, 6.1
6.6  <- 6.4, 6.5
6.6b <- 6.1, 6.5
6.7  <- 6.4
6.8  <- 6.1, 0.6
6.9  <- 4.9, 6.4
6.10 <- 6.5, 6.6, 6.6b, 6.7, 6.8, 6.9

7.1  <- 6.10, 4.11, 5.9
7.2  <- 7.1
7.3  <- 7.1
7.4  <- 7.1
```

Two independent tracks run from day one. The owner works 0.1, 0.3 through 0.6 and 0.8. Agents work 0.7, all of WP1, all of WP2 except 2.9/2.10/2.12, and all of WP3 except the Unreal-module half of 3.4. Those two tracks meet at 4.1. Nothing in WP2 or WP3's CMake half needs Unreal installed, which is the whole reason `signal-core` and `dashboard-spec` compile without it.

The first device convergence point is 4.0 and 4.0b, deliberately early and deliberately small. 4.0 runs at the desk on the Pixel and shakes out packaging and the on-device workflow; 4.0b then costs one trip to the car and is the only thing standing between "Unreal packages and runs on this head unit" being an assumption and being a measurement. WP4 continues on top of 4.0; the showcase (WP5) and the device half of WP6 wait for 4.0b.

The second convergence point is 6.4. Until a full package exists and the owner walks to the car again, there are no device performance numbers and no claim about frame rate on the head unit is permitted.

### Test strategy

Five layers, cheapest first.

1. **Unit, in CI, no engine.** doctest over `signal-core` on `windows-latest` and `ubuntu-latest`. Covers types, units, freshness, rules, hysteresis, interpolation bounds, replay round-trip and replay lifecycle, scenario determinism, frame parsing, heartbeat classification, held-value classification, malformed input. A third job on the `ubuntu-latest` runner rebuilds the same sources with ThreadSanitizer and runs the 4.3 acquisition stress test, which is the only place the single-writer registry, the bounded queue under consumer stall and the reconnect generation check are exercised concurrently. This is where most correctness lives and it costs nothing per run.
2. **Schema parity, in CI.** One corpus, two validators, diffed, both running schema validation plus the identical semantic pass. A schema or semantic-pass change that touches only one validator fails the build. This is the only defence against the Python tools and the player disagreeing about what a valid document is.
3. **Deterministic render and state checks, local.** Two kinds. Screenshot captures at known scenario timestamps through `-screenshot=` and `-quit-after=`, compared against checked-in references with a stated per-pixel tolerance, for things a still frame can establish: the primitives, the dial aspect ratio, the three quality presentations. And per-frame state traces, for things a still frame cannot: the transition assertions in 5.5 and the needle-stopped assertion in 5.7 read a trace of transition progress, visible-value flags and needle angles across every frame of the run, because a claim about what happened throughout an interval cannot be proven from three samples of it. Fixed camera, seeded scenario, frozen scenario time where the comparison demands it.
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

**6. One set of signal-core sources, compiled by two build systems, with no prebuilt libraries.** *(locked via escape hatch)* The sources live at `runtime/UnRealDash/Source/SignalCore/` as a plain Unreal module that includes no Unreal headers except one module-registration file, which the CMake build excludes. UBT compiles them for Win64, Android ARM64 and Linux x86-64 with Epic's own compiler, sysroot, standard library and runtime flags. `packages/signal-core/` holds `CMakeLists.txt`, the doctest suite and a README, and compiles the same files with exceptions and RTTI off for hosted CI.
Rejected: shipping `signal-core` as three prebuilt static libraries linked through `PublicAdditionalLibraries`, which was the earlier plan. Reason: that arrangement created an ABI boundary and then asked three hand-written CMake toolchain files to hold both sides of it in agreement, including cross-compiling to Android and to Epic's Linux sysroot from Windows. Letting UBT compile the sources removes the boundary instead of pinning it, and costs nothing that the CMake build was providing.
Also rejected: putting the logic inside the player module with no CMake build at all. Reason: CI would then need an engine install, which hosted runners do not have, and the rule logic would be untestable without a GPU. The CMake build is what keeps the sources engine-independent: an Unreal header added to a signal-core file breaks the hosted CI build immediately and visibly.
doctest over Catch2 v3 because it is a single vendored header with no package resolution step. It is configured with `DOCTEST_CONFIG_NO_EXCEPTIONS`, which suits a library that is exception-free by design: every fallible operation in `signal-core` returns an error code or a result type and nothing throws.
Cost: a second build system and a discipline that the two file lists stay in step, enforced by CI rather than by attention. Residual risk is R-N: two compilers can still disagree about the same source, and 4.2's in-engine commandlet is the test for that.

**7. Two validators, one corpus, and the C++ one is engine-independent.** *(locked via escape hatch)* Rejected: validating only in Python at authoring time and trusting documents in the player. Reason: the player will load packages the tools never saw, and PROJECT-PLAN section 7 requires bounded validation before activation.
The C++ validator is a CMake target in `packages/dashboard-spec` with RapidJSON and valijson vendored at pinned revisions, built with exceptions and RTTI off, and run on hosted CI. The Unreal module compiles the same sources. Rejected: taking RapidJSON from the engine, which is what made the earlier version of this decision depend on an Unreal install for a job that runs on hosted runners with no engine.
Exceptions stay off by default. Unreal does support `bEnableExceptions` per module, and if the no-exceptions build fails to compile, that flag is set on the isolated validator module and nowhere else. It is not enabled pre-emptively: exceptions on a mobile player module cost code size and unwind tables that a feasibility build should not pay unless the cheaper path has actually failed. Cost: vendored third-party sources to keep current, a parity job in CI, and the residual R-G risk that neither build compiles, in which case 3.5 is blocked rather than downgraded.

**8. Rules are a JSON expression tree, not a text grammar.** *(locked via escape hatch)* Rejected: a small expression language with a parser. Reason: an AST is schema-validatable, has no parser surface to fuzz, and matches PROJECT-PLAN section 5. Cost: hand-authoring rules is verbose, acceptable because Studio will generate these later.

**9. The control dashboard is data-driven from day one; the showcase is hand-authored with a bounded JSON sidecar.** *(locked via escape hatch)* Rejected: a full 3D component schema in Stage 0. Reason: designing a schema for mesh instances, cameras, transform hierarchies, timelines and state machines before any frame-time data exists would be guessing, and producing that data is Stage 0's job. Cost: the showcase is not portable at the end of Stage 0, and its edit-without-rebuild claim is limited to the sidecar's parameter set. The generic vehicle silhouette from CREATIVE-DIRECTION.md is a stretch item, not a gate.

**10. Git LFS from the first content commit.** *(locked via escape hatch)* Rejected: committing binaries directly, or keeping assets out of the repository. Reason: `.uasset` and `.umap` are binary and unmergeable, and a repository that stores them raw becomes unclonable. Cost: GitHub's free LFS quota is a real ceiling (the figure has changed over time; 1.2 records the current storage and bandwidth allowance from the account's billing page), so showcase content is budgeted under 100 MB and every contributor needs `git lfs`.

**11. Two connector families in Stage 0: simulator plus replay, and a receive-only TCP client for binary-telemetry-v1.** *(locked via escape hatch)* Rejected: starting with generic Bluetooth OBD-II, which PROJECT-PLAN section 5 names as the first broadly useful product target. Reason: Stage 0 needs a real, already-working data source for the device test, and OBD-II needs adapter purchases, vehicle testing and a session layer that has nothing to do with whether Unreal renders acceptably. Cost: Stage 0 produces no evidence about generic OBD-II and the report must not imply otherwise.

**12. No staging or rollback and no command channel in Stage 0.** *(locked via escape hatch)* Rejected: building the staging and rollback machinery now, since PROJECT-PLAN section 7 requires it eventually. Reason: loading a package from a command-line path is enough to prove edit-without-rebuild, and the atomic-activation design should be informed by what the device's storage actually does. Cost: the Stage 0 player has no recovery from a bad package other than a different `-udash=`. 7.3 designs the real mechanism.

**13. CI on hosted runners only; Unreal builds are local; CI never fetches LFS.** Rejected: a self-hosted runner on the workstation. Reason: an Unreal build agent needs the whole toolchain, holds the machine's GPU and disk, and makes the workstation a single point of failure for CI. Cost: no automated regression on the packaged player. Every packaging result in Stage 0 is a local run recorded by hand.
Every `actions/checkout` in every workflow sets `lfs: false`. The hosted jobs compile C++ sources, run Python and diff JSON; none of them needs a `.uasset`, a mesh or a PNG. GitHub's free LFS bandwidth allowance is monthly and modest, and a checkout that pulls the showcase content on every push would spend it in a handful of runs and then break clones for everyone, including the owner. This is a one-line setting that is easy to omit, which is why it is written down as a decision rather than left to the workflow author.

**14. MIT for code, CC-BY-4.0 for original example assets, both provisional.** Rejected: deferring the licence choice, as PROJECT-PLAN section 9 leaves it open. Reason: a public repository with no LICENSE file is not contributable. Provisional so the owner can change it before any external contribution arrives.

**15. Device test cadence: the head unit qualifies; the owner's Pixel 10 Pro is the desk device.** Rejected: buying a desk device, and two older phones the owner offered (a 2016 ZTE Axon 7, GLES-only on an Adreno 530, and a 2015 Huawei Y6, 32-bit and below the engine floor). The Pixel 10 Pro (Tensor G5, Imagination PowerVR GPU, Android 16, Vulkan 1.3 or later) is plugged into the workstation while the owner is present. It runs both RHIs and the full on-device file, argument and export workflow, so 4.0 and packaging iteration happen at the desk and the car is visited for 4.0b, 6.4, 6.6 and 6.9. Limits: PowerVR is a different driver family from the head unit's Mali-G57, so the Pixel says nothing about the Mali bug class, and it is far faster than the head unit, so any frame time measured on it is an optimistic bound and never a result. No qualification number in the report comes from the Pixel. Cost: the Pixel is available only when the owner is at the desk; 4.0 states what happens when it is not.

**16. Stage 0 exit is the six conditions in Goal, taken from PROJECT-PLAN sections 8 and 10.** No alternative considered; these are the roadmap's own criteria.

**17. Repository layout is PROJECT-PLAN section 11 as-is.** Rejected: a flatter layout for a stage that uses half the directories. Reason: renaming later breaks every path in every script and document. Cost: some directories sit near-empty, each with a README explaining what lands there.

**18. Bench-first for the car, per PILOT-INTEGRATION.md.** Rejected: connecting to the live relay early for a real demo sooner. Reason: the relay is in a working car with a working dashboard client, and a coexistence failure costs the owner his existing instrumentation. Order: synthetic, fault injection, workstation, device package, then live relay with coexistence checked first.

## Assumptions

Each is a belief this plan depends on, with its source and how it would be falsified.

**A1. Workstation environment.** Windows 11 Pro, RTX A2000 12 GB, 64 GB RAM. Installed: VS Build Tools 2022 17.14 with MSVC 14.44 and Windows SDK 10.0.26100, Node 24, Python 3.14, Go 1.26. Not installed: Unreal Engine, Epic Games Launcher, full Visual Studio IDE, Android Studio, Android SDK, NDK, JDK, CMake. C: had 99 GB free and the owner is clearing more. *Source: environment scan 2026-09-15.* Falsified by `doctor.ps1` in 0.7.

**A2. Toolchain pins.** Every version in the pin table. *Source: `docs/research/2026-09-15-toolchain-claudex-research.md`.* The JDK patch level (21.0.3) is flagged in that file as worth re-verifying against the live Turnkey manifest; task 0.5 does that. The UE install footprint figures are community reports, not an Epic spec, and are explicitly unverified.

**A3. Reference device.** DUDU7 head unit, Unisoc UIS7870 (4x Cortex-A76, 4x Cortex-A55, Mali-G57 MP4), Android 13, unrooted, 1280x720 panel with 1280x660 usable when the navigation bar shows, USB host, reachable over network ADB, installed in the car. *Source: `docs/PILOT-INTEGRATION.md` and the research file; the RAM and storage configuration is from retail listings and is low confidence.* Task 0.8 replaces all of this with measurements.

**A4. Vulkan ceiling.** Mali-G57 is first-generation Valhall and tops out at Vulkan 1.1, which meets UE 5.8's device minimum exactly with no headroom. *Source: research file, citing developer.arm.com.* Falsified by the `dumpsys` output in 0.8.

**A5. Existing telemetry path.** An Android USB bridge app exposes the acquisition board on local TCP 2323 with Telnet framing; a Python relay consumes that and re-serves a raw stream on `127.0.0.1:35000`; the relay emits synthetic status records to keep clients alive when the board goes quiet, and ordinary frames may repeat a held measurement when a channel stops updating. A live socket is therefore not evidence of fresh data, and neither is a well-formed frame. *Source: `docs/PILOT-INTEGRATION.md`, derived from read-only inspection of the owner's separate repository.* The installed revisions on the device have not been confirmed against that inspection; 6.9 confirms them before the live step.

**A6. Frame format.** 16 bytes: tag `44 33 22 11`, little-endian u32 frame id in 0xC80 to 0xC94 inclusive (21 identifiers), 8-byte payload as four little-endian u16 words, per-frame signed default with per-value override, scaling of the form `V*<float>` converted offline to an affine `scale` and `offset`. The identifier count is 21, not 22: `0xC94 - 0xC80 + 1 = 21`. The pack enumerates the identifiers explicitly rather than expressing them as a range, so the parser, the fixture pack, the converter and the reference decoder cannot disagree about the boundary. *Source: the owner's schema decoder.* Falsified by 2.12's differential check, which covers every enumerated identifier.

**A7. Schema reuse is the owner's call.** The source-of-truth XML is the single `board/*.xml` schema file in the owner's separate `scirocco-dash` repository on the workstation. It is owner-authored, so reusing its content is his decision. This plan converts it offline in 3.7 and the player never reads it. If reuse is declined, the definition pack is re-derived from the documented frame format by hand and 3.7's gate changes to a hand-written pack.

**A8. The head unit is the only qualifying Android device; the Pixel 10 Pro is the desk device.** *Source: owner, 2026-09-15.* The Pixel is available when the owner is at the desk and is used by 4.0 and for packaging iteration only. If it is unavailable, 4.0 merges into 4.0b on the head unit. No qualification number comes from it.

**A9. No Linux machine exists.** *Source: environment scan 2026-09-15.* This is why 6.8's gate is "builds" and not "runs".

**A10. The owner is available for the `[human]` tasks and for car access.** Tasks 0.1 to 0.6, 0.8, 5.2, 6.4, 6.6, 6.9 and 7.2 cannot be done by an agent. If car access is unavailable for an extended period, WP6 stalls and Stage 0 cannot exit, since every device number in the report comes from it.

**A11. No skill packs or MCP tools beyond standard coding tools are needed.** File editing, shell, git, a compiler, Python and ADB cover every agent task in this plan.

**A12. Unit conventions in the existing schema are not fully settled.** Boost and rail pressure absolute-versus-gauge, and the exact unit of several temperature and pressure channels, need confirmation against the owner's notes before the values are presented as physical measurements. *Source: `docs/PILOT-INTEGRATION.md`.* Until confirmed, affected signals render with their unit label taken from the definition pack and are listed in the report as unconfirmed.

## Risks and open questions

**R-A. Mali-G57 Vulkan stability on UE 5.8.** Mali plus UE Vulkan crashes are an active bug class on Epic's tracker, including a 5.8 shader-compile crash inside `libGLES_mali.so` on Mali-G76 and RHIThread crashes on Mali in 5.6. Nothing specific to UIS7870 was found, which is absence of evidence, not evidence of stability. *Test:* 6.2 and 6.3 package both RHIs, 6.4 runs both on the device, and any crash produces a committed `logcat`. *Mitigation:* GLES 3.2 is a first-class packaged path, not a footnote. If both paths crash, that is a genuine Stage 0 no-go input and the report says so.

**R-B. Device RAM and thermal headroom are unknown.** PROJECT-PLAN section 8 targets under 750 MB resident on the standard profile; nothing is known about what this unit has free after its own OS. *Test:* 0.8 records total and available RAM; 6.6's 60-minute run records temperature and memory over time. *Mitigation:* if the showcase does not fit, the report records the measured ceiling rather than reducing the showcase until it passes.

**R-C. Disk exhaustion during DDC and Intermediate growth.** The 150 GB precondition is based on unverified community footprint figures. A cook that fills C: can corrupt the DDC and cost a rebuild. *Test:* `doctor.ps1 -Profile toolchain` prints free space against the 30 GB build-headroom default and every packaging script calls it first; `-PreInstall` checks the 150 GB capacity figure before 0.3 and 0.4 only. Separating the two stops the doctor from blocking every build immediately after a successful install has consumed the capacity it was checking for. *Mitigation:* a documented DDC purge step in `scripts/`, and the owner clears space before 0.4 rather than during 6.2.

**R-D. The device is in the car.** Every render check is a trip to the driveway, which makes the Android iteration loop expensive and biases the work toward batching changes, which in turn makes failures harder to attribute. *Mitigation:* network ADB so the trip is short; batch device runs behind a scripted checklist in 6.5; the Pixel 10 Pro at the desk for the 4.0 spike and for packaging iteration, and a rule that no Android-specific change goes to the car before it passes on the Pixel and passes the Windows screenshot checks in layer 3 of the test strategy.

**R-E. GitHub LFS quota.** The free allowance is limited and has changed over time; the current storage and monthly bandwidth figures are read from the account's billing page in 1.2 and written into `packages/dashboard-spec/README.md`. Unreal content and packaged artifacts exceed any free tier quickly. *Test:* 5.2's gate caps added LFS bytes at 100 MB. *Mitigation:* packaged builds and captures are never committed; screenshots in the report are downscaled; if the quota is hit, the owner decides between paid data packs and moving content out of the repository.

**R-F. Runtime texture import cost on Android.** PROJECT-PLAN section 4 flags decoded memory, upload time and platform behaviour as unvalidated. If runtime PNG import is slow on this GPU, the data-driven image primitive is compromised. *Test:* 6.7 measures decode and upload per asset size on both platforms with at least 20 samples per cell. *Mitigation:* if it is too slow, Stage 1 moves image assets to a cooked-content path and the report records the threshold at which that becomes necessary.

**R-G. valijson with exceptions and RTTI off.** Whether valijson's RapidJSON adapter compiles with RTTI and exceptions both off is **unverified**. *Test:* 3.4's engine-independent CMake target is built with both off on `windows-latest` and `ubuntu-latest`, so this is answered on hosted CI early, without an engine and without a device, rather than surfacing inside a player build. *First response if it fails:* enable `bEnableExceptions` on the isolated Unreal validator module and the equivalent flag on the CMake target, which keeps full schema validation at the cost of exceptions in one module. *There is no structural-only fallback.* The earlier version of this plan allowed the player to fall back to a required-keys-and-bounds check; that has been removed, because a parity gate cannot honestly stay green while the two validators are no longer doing the same job. If neither the no-exceptions nor the exceptions-enabled build compiles, 3.5 is **blocked**, the player has no schema validation, and that is a Stage 0 finding the report states plainly rather than a gate quietly redefined to fit.

**R-H. Heartbeats and held values masquerading as fresh data.** Two distinct mechanisms, both ending with an instrument showing a confident number that is not current. The relay emits synthetic status records when the board goes quiet, and a naive parser marks every signal fresh forever. Separately, PILOT-INTEGRATION.md records that ordinary well-formed frames may repeat a held measurement when a channel stops updating, so filtering out the synthetic records alone does not solve it. *Test:* 2.10 carries two independent tests, one for heartbeat-only traffic and one for repeated identical payloads on `held` fields, each asserting that transport stays connected while the mapped signals go stale at their deadlines. 2.9 adds sentinel rejection so a diagnostic raw value never becomes a converted reading. *Mitigation:* transport health, acquisition health and per-signal validity are three separate states in the registry, never one indicator; `held` fields carry an age-unknown presentation distinct from both valid and stale, so an operator can see that the number is real but its age is not known.

**R-I. Unit conventions in the existing schema.** Absolute versus gauge pressure, and several temperature channels, are not confirmed. Presenting an unconfirmed conversion as a measurement is a correctness failure, not a cosmetic one. *Test:* 2.12's differential check catches disagreement with the reference decoder but cannot catch a convention both share. *Mitigation:* A12's rule, plus an explicit unconfirmed list in the Stage 0 report.

**R-J. 60 Hz on the head unit is a target, not a promise.** PROJECT-PLAN section 8 states its table is proposed engineering gates, not measured results. No frame rate claim in this plan is a measurement until 6.5 produces one. *Mitigation:* 6.10's audit exists specifically to catch a target that has been written into the report as if it were measured.

**R-K. Android Studio and NDK drift versus Epic's pin.** Android Studio updates itself; a drifted NDK or JDK produces build failures that look like project bugs. *Test:* `doctor.ps1` prints the resolved NDK and JDK every run and fails on a mismatch with the pin table. *Mitigation:* automatic updates disabled in Android Studio; the resolved versions recorded in the Stage 0 report.

**R-L. Editor-authored assets are binary.** Master materials and meshes from 5.2 cannot be diffed or merged, so two people editing the same material is a lost edit. *Mitigation:* 5.1's editor Python scripts make the authoring reproducible from code, LFS keeps the repository clonable, and the parameter-name manifest turns a silent binding break into a script failure.

**R-N. The two compilers disagree about the same signal-core sources.** There is no longer an ABI boundary: UBT compiles the sources itself with Epic's flags, so there is no prebuilt library whose standard library or exception model can mismatch. What remains is narrower. The same C++20 sources are compiled by the CMake build, with exceptions and RTTI off, and by UBT with Epic's own flags, and the two can still differ in warning levels, standard-library implementation, floating-point behaviour or undefined-behaviour outcomes. Code that passes doctest under one can misbehave under the other. *Test:* 4.2's in-engine smoke commandlet runs a representative subset of the signal-core checks in-process on Win64 and compares its assertion count against the CMake run's count for the same subset, failing on any difference. That is the test: a version string would not have caught it. *Mitigation:* the `signal-core` public API passes plain structs, spans and callbacks rather than standard-library containers, so the two builds share as little implementation-defined surface as possible; the CMake build treats warnings as errors so the stricter of the two configurations fails first, in CI, before the engine build is attempted.

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
