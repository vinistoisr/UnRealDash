# Plan Review Log: UnRealDash Stage 0 feasibility and Stage 1 entry
Phases 0-1 (recon + interrogation) complete - plan locked with the user on 2026-09-15. REVIEWERS=codex,gemini,deepseek. MAX_ROUNDS=5.
Reviewer models: Codex gpt-6-astra (codex-cli 0.154.0); Gemini gemini-3.8-flash-high, effort high (agy 1.2.3); DeepSeek deepseek-v4-pro, effort high.

## Round 1 - Plan revision 74575df12265 (commit 66763d4)
### Codex (gpt-6-astra)
The plan still has material problems. I read all requested documents, did not read `PLAN-REVIEW-LOG.md`, and made no changes.

1. **High — The package limits do not bound runtime resource consumption.** Tasks 3.6 and 4.4 limit ZIP expansion, but a small PNG can require enormous decoded texture memory; a shallow expression can contain huge numbers of nodes; component counts and history buffers remain unbounded. Unpacked directories also lack an explicit equivalent limit contract. These inputs can exhaust memory or stall rendering while passing the stated gates.  
   **Fix:** Apply shared ZIP/directory limits for document bytes, nesting, total expression nodes/evaluation work, component counts, history samples, image dimensions, and aggregate decoded CPU/GPU memory before allocation.

2. **High — Archive-path validation does not secure asset references inside documents.** Task 3.6 checks archive entry names, but nothing requires an image reference in `dashboard.json` to remain inside the package. A normal archive containing a document that references `../../outside.png` could pass the archive gate. Duplicate normalized entries, case collisions, and symlinks also have no defined handling.  
   **Fix:** Resolve every document asset reference through one package-root resolver, reject links and ambiguous normalized paths, and test both packed and unpacked inputs.

3. **High — Heartbeat handling fixes only one way stale data masquerades as fresh.** Task 2.10 excludes synthetic status records, but `PILOT-INTEGRATION.md` explicitly says repeated ordinary frames may contain held measurements. Updating receive time on each repeated frame can keep those values valid indefinitely. Neither the pack subset nor the acceptance gates define measurement-age or unsupported-sentinel handling.  
   **Fix:** Define per-field age/validity semantics, preserve unknown acquisition age and uncertainty visibly, reject diagnostic sentinels before conversion, and test held-value traffic separately from heartbeat traffic.

4. **High — The threading contract permits stale resurrection and lost warnings.** Task 4.3 specifies a bounded queue and a drop policy, but not registry ownership, coherent snapshots, reconnect generations, or rejection of queued samples from an old connection. A delayed sample can overwrite a newer value after reconnect. Dropping samples before rule evaluation can also erase a brief warning condition. Publishing at 200 Hz does not inherently require drops merely because rendering runs at 60 Hz.  
   **Fix:** Specify registry ownership, receive-time stamping, source/session ordering and warning evaluation before display coalescing; force consumer stalls and reconnects in the queue tests.

5. **High — Rules lack the semantics needed to produce trustworthy warnings.** Tasks 2.3–2.5 do not define dimensional checking, literal units, divide-by-zero/overflow behavior, invalid-quality propagation, or precedence between debounce and missing-input policies. A threshold authored in Celsius can be compared against normalized kelvin, and `hold_last` can retain an apparently valid “safe” result after its input disappears.  
   **Fix:** Define typed/unit-aware AST semantics, finite-number checks, quality propagation, and explicit initialization/reset/expiry rules for hysteresis, debounce and `hold_last`, with boundary fixtures.

6. **High — Draft-7 is being assigned semantic checks it does not generally provide.** Task 3.1 requires the schema to reject duplicate component IDs. For an array/tree of component objects, `uniqueItems` compares whole objects: two different objects sharing an ID remain distinct. Cross-document references, inherited aspect policy, and `byte_offset + field_width <= frame.length` similarly need a deliberate representation or semantic validation. [Draft-7 specification](https://json-schema.org/draft-07/draft-handrews-json-schema-validation-01)  
   **Fix:** Add an explicit semantic-validation pass shared by both tools, or use an ID-keyed component map plus validated references; include duplicate-ID/different-properties and out-of-frame-field fixtures.

7. **High — The engine-free validator track contradicts its implementation and fallback.** Task 3.4 uses Unreal’s bundled RapidJSON and a `Build.cs` gate, while 3.5 runs on engine-free hosted CI and the sequencing section says WP3 needs no Unreal installation. If R-G’s structural-only fallback is used, the identical-validator parity gate cannot honestly remain satisfied.  
   **Fix:** Build a standalone validator against pinned standalone dependencies in CI, add a separate Unreal compilation check, and explicitly replace or block affected gates if the fallback is taken.

8. **High — The doctor can block builds immediately after successful installation.** Task 0.1 makes 150 GB a *pre-install* requirement; 0.7 exits on any miss; 1.6 runs that doctor before every build. Installing the estimated toolchain consumes much of those 150 GB. The same doctor also requires head-unit ADB reachability, making an offline Windows build depend on car availability.  
   **Fix:** Give the doctor operation-specific profiles and separate pre-install capacity from post-install build headroom; require ADB only for device operations.

9. **High — Android feasibility is tested after most of the application has been built.** The first APK follows the complete control dashboard and substantial showcase work. A fundamental Android packaging, driver or runtime failure is therefore discovered after implementing schemas, nine primitives, connectors and authored content. Task 4.2 additionally builds all three static libraries before any UBT invocation.  
   **Fix:** Move a minimal packaged Android scene with one live value, one imported texture and one representative material under both RHIs immediately after toolchain setup, and build libraries only for the requested target.

10. **High — The metrics producer cannot populate the promised metrics table.** Task 4.8 does not produce receive-to-present latency, process-launch-to-first-usable-value timing, or GPU-upload completion timing, yet 5.6 and 6.5 depend on those measurements. Rolling p95/p99 values cannot reconstruct whole-run percentiles. Writing only on exit loses evidence on crashes or termination.  
    **Fix:** Define timestamp endpoints and clock domains, persist raw frame/sample events incrementally, instrument presentation/upload completion explicitly, and compute run-level statistics from those events.

11. **High — Device qualification can pass without proving the important operating behavior.** Task 6.6 requires one showcase thermal run, without naming its RHI; the eight-hour desktop soak has no task; wake behavior appears in the test strategy without an executable gate. “Memory grew monotonically” misses a leak that grows overall while occasionally dropping. APK manifests and screenshots also do not prove which RHI actually ran.  
    **Fix:** Specify the dashboard/RHI qualification matrix, log the active RHI and driver, add suspend/resume and desktop-soak runs, and assess post-warm-up memory growth with a stated trend criterion.

12. **High — Several visual gates cannot establish their claimed result.** A single screenshot cannot prove that a needle stopped moving (5.7); three captures cannot prove speed remained visible throughout a transition (5.5). In 4.11/5.9, unchanged executable hashes and differing screenshots can pass because telemetry changed—or because cooked content changed while the executable stayed identical. The edit checks also do not explicitly require Android execution.  
    **Fix:** Capture frame sequences or state traces for temporal assertions, freeze scenario time for edit comparisons, hash all player/cooked artifacts, and run the edit gates on both Windows and Android.

13. **Medium — Replay round-trip fidelity does not prove replay correctness, and the format loses sample data.** Task 2.1 includes sequence and optional source timestamp, but 2.7’s recording format omits them. No contract defines timestamp rebasing, idle gaps, EOF, seek, loop or reconnect behavior. A byte-identical round trip can still produce incorrect freshness and debounce results when replayed.  
    **Fix:** Version a recording format preserving relevant sample/session metadata, define a replay clock and lifecycle semantics, and compare freshness/rule transitions at known times including gaps and EOF.

14. **High — The Android file-and-command workflow is unspecified.** The plan relies on `-udash`, editable external JSON, screenshots, CSV extraction and two APK variants, but never defines the Shipping app’s writable/readable locations, argument delivery, or install/update behavior. Windows edit success does not establish that the unrooted Android package supports the same workflow.  
    **Fix:** Make the early APK spike demonstrate the exact ADB/import, launch-argument, JSON-edit and artifact-export procedure using the intended Shipping configuration.

15. **Medium — The protocol’s stated frame count is internally inconsistent, and the pack contract is ambiguous.** Task 2.9 and A6 call `0xC80` through `0xC94` “22 frames”; the inclusive range contains **21** identifiers. The schema also includes both type and signedness without resolving contradictions, and does not clearly distinguish wire-frame length from payload length/offset origin. Differential matching cannot resolve an omitted identifier unless the fixture covers it.  
    **Fix:** Enumerate authoritative IDs and define offset origin, length meaning and signedness precedence, then test every ID and field boundary against the reference decoder.

16. **Medium — The Stage 1 design gate leaves rollback and authorization materially underspecified.** Task 7.3 names pairing, origin rejection and failed launches, but its gate only checks disk layout, ordering and a counter. It does not require authenticated commands, screenshot/load path restrictions, concurrent-activation behavior, a definition of “successful launch,” or stationary-only activation from `PROJECT-PLAN.md`. A package that launches and immediately crashes could be promoted prematurely.  
    **Fix:** Require the design note to specify command authorization and a crash-consistent activation state machine, including serialized activation, stationary gating, health-based promotion and a crash-point test matrix.

VERDICT: REVISE

### Gemini (gemini-3.8-flash-high, effort=high)
_Note: the first call read the repository (171k input tokens) then attempted RunCommand, which the sandbox denied, so it returned no text. The same conversation was resumed with an instruction to finish using file reads only; the critique below is that resumed turn._

# Independent Adversarial Review: UnRealDash Stage 0 Plan

This review evaluates [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md) against the supporting architectural documents under `docs/` ([`PROJECT-PLAN.md`](file:///C:/Users/Vincent/UnRealDash/docs/PROJECT-PLAN.md), [`CREATIVE-DIRECTION.md`](file:///C:/Users/Vincent/UnRealDash/docs/CREATIVE-DIRECTION.md), [`CONNECTORS.md`](file:///C:/Users/Vincent/UnRealDash/docs/CONNECTORS.md), [`PLUGIN-EXPERIENCE.md`](file:///C:/Users/Vincent/UnRealDash/docs/PLUGIN-EXPERIENCE.md), [`PILOT-INTEGRATION.md`](file:///C:/Users/Vincent/UnRealDash/docs/PILOT-INTEGRATION.md), [`RESEARCH.md`](file:///C:/Users/Vincent/UnRealDash/docs/RESEARCH.md), and [`2026-09-15-toolchain-claudex-research.md`](file:///C:/Users/Vincent/UnRealDash/docs/research/2026-09-15-toolchain-claudex-research.md)).

---

## 1. Critical Sequencing & Packaging Dependency Flaws

### Finding 1.1: Packaged Executables (WP6) Are Sequenced Before Core Showcase Features & Telemetry Wiring
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L414-L424) (Sequencing block lines 414–424).
* **Flaw**:
  The dependency graph specifies:
  ```text
  6.1  <- 4.10, 5.7
  6.2  <- 6.1, 0.5
  6.3  <- 6.2
  6.4  <- 6.2, 6.3, 0.8
  6.6  <- 6.4, 6.5
  5.9  <- 5.8, 6.1
  ```
  `6.1` (Package for Windows x64) depends only on `4.10` and `5.7`. It **omits** dependencies on:
  - `5.4` (Luminous band driven by RPM dynamic material instance)
  - `5.5` (Mode transition into compact layout with history graph)
  - `5.6` (Day/night themes and startup reveal)
  - `5.8` (`showcase.json` sidecar parameter loader)
  - `4.9` (TCP client connector wiring for live/mock telemetry)
  
  Because `6.2` and `6.3` (Android APK packages) derive from `6.1`, the APKs deployed to the device in `6.4` and measured in the 60-minute thermal run in `6.6` will be packaged **without** reactive materials, without layout transitions, without themes, without `showcase.json`, and without the TCP connector. Furthermore, Task `5.9` asserts that editing `showcase.json` changes rendering in the packaged binary without a rebuild, but `6.1` is permitted to package before `5.8` is even integrated.
* **Concise Fix**:
  Update line 414 to gate packaging on all showcase deliverables and connectors:
  `6.1 <- 4.9, 4.10, 5.4, 5.5, 5.6, 5.7, 5.8`.

---

## 2. Platform, Execution & Security Holes

### Finding 2.1: Workstation Deadlock in `doctor.ps1` Caused by In-Car ADB Dependency
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L74-L77), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L108-L111) (Tasks 0.7 and 1.6).
* **Flaw**:
  Task 0.7 specifies that `doctor.ps1` verifies "ADB reachability of the head unit" and "exits non-zero on any miss." Task 1.6 specifies that `build.ps1`, `package-windows.ps1`, and `package-linux.ps1` each invoke `doctor.ps1` first and refuse to run if it fails.
  The reference head unit is installed in a vehicle in the driveway (Tasks 0.8, A3, R-D). Whenever the car is turned off, asleep, or disconnected from Wi-Fi, `doctor.ps1` will fail. As a result, developers cannot compile `signal-core`, build the Windows editor, or package Linux on their workstation unless the car's ignition and network ADB are active.
* **Concise Fix**:
  Make ADB device reachability in `doctor.ps1` conditional behind an explicit parameter (e.g. `scripts/doctor.ps1 -RequireDevice`). Only `deploy-deck.ps1` and device test runners should pass `-RequireDevice`; local workstation build and packaging scripts must run with device checks disabled.

### Finding 2.2: Missing Android Network Permission and Incompatible Storage Paths on Android 13
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L224-L227), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L245-L247), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L305-L315) (Tasks 4.4, 4.7, 4.9, 6.2–6.4).
* **Flaw**:
  The reference DUDU7 deck runs an unrooted Android 13 environment (API 33, target SDK 35):
  1. **Network Permission**: Task 4.9 connects to `127.0.0.1:35000`. `PLAN.md` never declares or verifies `android.permission.INTERNET` in `AndroidManifest.xml` (or `DefaultEngine.ini` / UPL). On Android, opening a TCP socket without this permission throws an immediate OS-level `SecurityException` / `EACCES (Permission denied)`.
  2. **Scoped Storage / File Access**: Task 4.7 defines `-udash=<path>` and `-screenshot=<path>`, while Task 4.8 writes a CSV on exit. On Android 13 with target SDK 35, arbitrary external storage access under `/sdcard/` is denied. Because Shipping APKs are not debuggable (`run-as` is disabled on unrooted devices), ADB cannot push or pull from `/data/data/<package_name>/`.
* **Concise Fix**:
  In Task 4.1/6.2, configure `DefaultEngine.ini` to declare `android.permission.INTERNET`. Pin on-device package loading, screenshot captures, and metrics CSV output to app-specific external storage (`/sdcard/Android/data/<package_name>/files/`), which requires no runtime permissions and is accessible via non-root `adb push` and `adb pull`.

---

## 3. Concurrency, Race Conditions & Communication Edge Cases

### Finding 3.1: Undefined Concurrency and Data Race in `SignalRegistry` Ingestion
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L127-L130), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L220-L223) (Tasks 2.2 and 4.3).
* **Flaw**:
  Task 4.3 runs the connector on a background acquisition thread publishing samples into `SignalRegistry`, while Tasks 4.5, 4.6, and 5.4 read from `SignalRegistry` on Unreal's Game Thread during Slate widget ticks.
  `PLAN.md` provides no synchronization contract for `SignalRegistry`. Concurrent reads and writes without synchronization produce a C++ data race (undefined behavior). Conversely, coarse mutex locking between the 200 Hz TCP acquisition thread and the 60 Hz rendering thread will introduce priority inversion and frame drops on the head unit's big.LITTLE CPU cores.
* **Concise Fix**:
  Specify a lock-free or double-buffered exchange in `packages/signal-core` (e.g. atomic per-signal sample slots or a double-buffered snapshot swapped at the start of each Game Thread frame). Guarantee that the Game Thread never blocks on acquisition I/O or mutex locks.

### Finding 3.2: TCP Connector Reconnection Spin Loop and Hardcoded Workstation IP
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L237-L239), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L245-L247) (Tasks 4.7 and 4.9).
* **Flaw**:
  1. Task 4.9 tests reconnecting after killing the mock relay, but specifies no socket timeout or backoff policy. Synchronous blocking `connect()` calls or unthrottled reconnect loops on disconnect will block thread pools or spin-wait CPU cycles.
  2. Task 4.9 hardcodes `127.0.0.1:35000`. During bench testing (Task 6.9 step 3), the Android player must connect to a telemetry replay stream hosted on the workstation across driveway Wi-Fi, or require `adb reverse tcp:35000 tcp:35000`. Task 4.7 omits host/port configuration from the command-line surface.
* **Concise Fix**:
  Implement non-blocking socket polling with exponential backoff (e.g. 500 ms to 5 s) on disconnect. In Task 4.7, add `-connector-host=<ip>` and `-connector-port=<port>` (defaulting to `127.0.0.1:35000`), and document `adb reverse tcp:35000 tcp:35000` for Android bench testing.

---

## 4. Telemetry Parser & Schema Specification Defects

### Finding 4.1: Missing Affine Offset in `binary-telemetry-v1` Parser
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L155-L158), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L173-L176) (Tasks 2.9 and 3.1).
* **Flaw**:
  Task 2.9 states the parser handles "scaling of the form `V*<float>`". It omits the `offset` parameter. However, standard vehicle telemetry channels (e.g., coolant temperature in [`PLUGIN-EXPERIENCE.md`](file:///C:/Users/Vincent/UnRealDash/docs/PLUGIN-EXPERIENCE.md#L55-L60) line 58: `offset: -40, unit: degC`, and `definition-pack.schema.json` in Task 3.1) require affine transformations: `physical = (raw * scale) + offset`. Without offset evaluation, temperatures and gauge pressures decode incorrectly.
* **Concise Fix**:
  Update Task 2.9 to explicitly evaluate affine conversions: `physical_value = (raw_value * scale) + offset`.

### Finding 4.2: Vulnerability to False Sync-Lock on Payload Bytes
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L155-L158) (Task 2.9).
* **Flaw**:
  The frame format uses a 4-byte sync tag `44 33 22 11` followed by a 4-byte frame ID and 8 bytes of payload. In an 8-byte binary payload, sensor values (e.g. RPM, pressure, or timestamps) can coincide with the sequence `0x44 0x33 0x22 0x11`. Searching only for the 4-byte tag on desynchronization causes the parser to lock onto payload data, corrupting downstream frame parsing.
* **Concise Fix**:
  Require the parser's resynchronization logic to validate both the 4-byte sync tag and that the succeeding 32-bit frame ID falls within the valid range `[0xC80, 0xC94]`, verifying that the next sync tag appears exactly 16 bytes later before declaring sync locked.

---

## 5. Observability Gaps & Incomplete Roadmap Alignment

### Finding 5.1: Omission of 8-Hour Desktop Soak Test Mandated by PROJECT-PLAN Section 8
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L14-L15), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L322-L325), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L357-L360) (Goal 5, Task 6.6, Task 7.4).
* **Flaw**:
  [`PROJECT-PLAN.md`](file:///C:/Users/Vincent/UnRealDash/docs/PROJECT-PLAN.md#L149) section 8 specifies:
  > "Steady operation: 60-minute thermal test plus 8-hour desktop soak; no monotonic memory growth"
  
  Goal Condition 5 and Task 7.4 mandate that every metric in PROJECT-PLAN section 8 be filled with measured numbers with "zero silent omissions." However, WP6 includes only Task 6.6 (the 60-minute in-car thermal run) and completely omits the 8-hour desktop soak test.
* **Concise Fix**:
  Add an explicit task in WP6 (e.g. Task 6.6b) for the 8-hour desktop soak test running an automated scenario on Windows to verify long-term memory stability, or formally document this scope reduction under Decision 16.

### Finding 5.2: Missing Instrumentation for Cold Launch Time and Receive-to-Present Latency
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L240-L243), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L318-L321) (Tasks 4.8 and 6.5).
* **Flaw**:
  [`PROJECT-PLAN.md`](file:///C:/Users/Vincent/UnRealDash/docs/PROJECT-PLAN.md#L146-L148) section 8 mandates measuring:
  - "Receive-to-present latency: p95 below 100 ms"
  - "Cold application launch: First usable dashboard within 5 seconds"
  
  Task 4.8 enumerates the metrics CSV columns: `fps, frame time p95 and p99 over a rolling window, missed frames, resident memory, texture memory, and per-signal freshness state`. Neither `receive_to_present_latency` nor process startup duration is captured in the CSV or by `capture-metrics.ps1`.
* **Concise Fix**:
  Add `receive_to_present_latency` to the CSV in Task 4.8 (computed as `present_timestamp - sample.receive_timestamp`). In Task 6.5, measure cold startup duration using `am start -W <package>` on Android and process creation delta to first Slate draw on Windows.

---

## 6. Flaky, Ill-Conditioned, and Untestable Acceptance Gates

### Finding 6.1: Flaky Gates in Fuzzing, Numerical Testing, and Performance Reproducibility
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L123-L126), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L131-L134), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L163-L166), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L240-L243) (Gates 2.1, 2.3, 2.11, 4.8).
* **Flaw**:
  1. **Gate 2.3 (Division by Zero in Relative Error)**: Tests `degC/degF/K, kPa/bar/psi` round-trips within `1e-6 relative error`. For affine scales around 0 °C (or 0 kPa / 0 km/h), relative error evaluates to `|0 - 0| / 0` (undefined / NaN).
  2. **Gate 2.11 (Fuzzing Rate Contradiction)**: Mandates "10 million mutated inputs produce no crash... the harness runs in CI in a capped 60-second mode." Processing 166,666 inputs/sec in a virtualized, throttled 2-vCPU GitHub Actions runner risks frequent timeouts and flakiness.
  3. **Gate 4.8 (Nondeterministic p95 Reproducibility)**: Requires the p95 column across two 60-second runs to be "reproducible within 10%". Background OS interrupts, DWM compositing, garbage collection, and mobile thermal throttling routinely introduce >10% variance in p95 tail latencies.
  4. **Gate 2.1 (Untestable Negative Compilation)**: Requires unit tests to assert that "constructing a sample without a receive timestamp fails to compile," which cannot be checked inside a runtime `doctest` suite.
* **Concise Fix**:
  - In 2.3, use standard floating-point tolerances with an absolute floor: `|a - b| <= max(1e-5, 1e-6 * max(|a|, |b|))`.
  - In 2.11, cap CI fuzzing at 1,000,000 iterations or a fixed 30-second time budget; reserve 10M iterations for offline runs.
  - In 4.8, gate on CSV schema validity, complete row counts, and positive values rather than a 10% variance threshold.
  - In 2.1, verify non-constructibility via C++20 `static_assert(!std::is_constructible_v<Sample, ...>)`.

---

## 7. Technical Attacks on Decisions Locked via Escape Hatch

### Attack on Decision 6 & Task 4.2: External CMake Precompilation vs Direct UBT Compilation
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L216-L219), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L461-L464), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L541-L542) (Decision 6, Task 4.2, Risk R-N).
* **Vulnerability**:
  Decision 6 precompiles `signal-core` as external static libraries via CMake using three custom toolchain files on Windows (MSVC 14.44, Android NDK r27c `android.toolchain.cmake`, and Epic's v26 clang Linux sysroot). Cross-compiling for Linux and Android with Epic's custom sysroot using standalone CMake on Windows is notoriously fragile and vulnerable to libc++ ABI and runtime library mismatches (`/MD` vs `/MT`, `-fno-rtti`, `-fno-exceptions`), as acknowledged in Risk R-N.
  The rationale that "CI would need an engine install" is a false dilemma. `packages/signal-core` can retain its standalone `CMakeLists.txt` for fast GitHub Actions CI without forcing Unreal to link it as precompiled binary archives.
* **Concise Fix**:
  Have `UnRealDashCore.Build.cs` compile the `signal-core` C++ source files directly via UnrealBuildTool. UBT automatically applies Epic's exact compiler, sysroot, libc++, and runtime flags across all three platforms without ABI friction. Retain CMake solely for standalone CI in `packages/signal-core/`.

### Attack on Decision 7 & Task 3.4: Artificial Prohibition of Exceptions in Player JSON Validation
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L186-L190), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L463-L464), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L529-L530) (Decision 7, Task 3.4, Risk R-G).
* **Vulnerability**:
  Task 3.4 forces `valijson` over RapidJSON with `bUseRTTI = false` and `bEnableExceptions = false`. Risk R-G admits that compiling `valijson` without exceptions is unverified, and plans a fallback to **drop schema validation in the player entirely**, reverting to a shallow structural check.
  Unreal Engine's build tool explicitly supports `bEnableExceptions = true;` on a per-module basis in `Build.cs` specifically to accommodate third-party C++ libraries. Banning exceptions in `UnRealDashCore.Build.cs` creates an artificial failure mode that forces the player to abandon schema validation.
* **Concise Fix**:
  Allow `bEnableExceptions = true;` in `UnRealDashCore.Build.cs` (or wrap `valijson` in an isolated third-party engine module). Verify compilation in WP1 so the player never regresses to loading unvalidated JSON.

### Attack on Decision 10 & Tasks 1.2, 1.4, 1.5: GitHub Actions LFS Bandwidth Exhaustion
* **Location**: [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L92-L95), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L100-L107), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L469-L470), [`PLAN.md`](file:///C:/Users/Vincent/UnRealDash/PLAN.md#L525-L527) (Decision 10, Tasks 1.2, 1.4, 1.5, Risk R-E).
* **Vulnerability**:
  Decision 10 places all binaries (`*.uasset`, `*.umap`, `*.png`, etc.) in Git LFS from the first commit. Standard GitHub accounts provide a free allowance of only **1 GB of LFS bandwidth per month**.
  Tasks 1.4 (`signal-core.yml`) and 1.5 (`spec-tools.yml`) execute on GitHub Actions. If `actions/checkout` downloads LFS objects by default, a 100 MB asset commit will exhaust the entire monthly bandwidth allocation in just 10 CI runs, breaking all repository clones and checkouts.
* **Concise Fix**:
  Explicitly configure `with: lfs: false` in `actions/checkout` across all GitHub Actions workflows (`signal-core.yml` and `spec-tools.yml`). Engine-independent CI must never fetch LFS media.

---

VERDICT: REVISE


### DeepSeek (deepseek-v4-pro, effort=high, tool_calls=22, elapsed=275s)
_Note: the first call hit the output token cap with no verdict (truncated=true); it was rerun as a fresh conversation with --max-tokens 65536, per the harness contract. The critique below is that rerun._

Reviewed `PLAN.md` and the planning docs under `docs/`; I did not read `PLAN-REVIEW-LOG.md`.

## Material findings

1. **Doctor couples all packaging to device reachability.**  
   `scripts/doctor.ps1` must print ADB reachability and exits non-zero on any miss (`PLAN.md:75-77`). Every packaging script calls `doctor.ps1` and refuses to run if it fails (`PLAN.md:109-111`). This means Windows and Linux packages cannot be produced unless the in-car head unit is on network ADB, even though those packages do not use the device. The sequencing block also omits `0.8` as a dependency of `0.9`, even though `0.9` verifies doctor output and appends to the file created by `0.8` (`PLAN.md:373-374`).  
   **Fix:** Make `doctor.ps1` profile-aware: build/package scripts require only workstation toolchain checks; deploy/capture scripts additionally require ADB. Add `0.8` as a dependency of `0.9`.

2. **The duplicate-component-ID gate is not expressible in JSON Schema draft-7.**  
   `PLAN.md:174-176` requires `dashboard.schema.json` itself to reject a document with a duplicate component ID. Draft-7 has no keyword that enforces uniqueness of an object property across array items (`uniqueItems` does not compare object fields). Both `jsonschema` and valijson implement draft-7, so this gate is untestable as written.  
   **Fix:** Move ID uniqueness to an explicit post-parse invariant implemented identically in both validators, and phrase the gate as “schema plus validator invariant rejects duplicate IDs,” not “schema rejects.”

3. **C++ validator parity CI cannot run as specified.**  
   Task 3.4 builds the C++ validator through Unreal (`Build.cs`, no RTTI/exceptions) (`PLAN.md:186-188`), while task 3.5 requires a hosted CI job running both validators (`PLAN.md:191-193`). Decision 13 says CI is hosted-only and Unreal builds are local (`PLAN.md:475-477`); hosted runners have no Unreal install.  
   **Fix:** Build the C++ validator as an engine-independent CMake target using a vendored RapidJSON/valijson and run that in CI, then consume the same sources from the Unreal module; otherwise reclassify 3.5 as a local UBT gate and update the test strategy.

4. **Signal registry concurrency is unspecified.**  
   `2.2` defines a registry holding latest samples (`PLAN.md:127-129`), and `4.3` publishes into that registry from an acquisition thread running off the game thread (`PLAN.md:221-223`). No locking, atomic, ownership, or publication model is specified, so the renderer and connector thread can race on sample state.  
   **Fix:** Specify the concurrency model (for example, bounded queue drained by the game thread, or a mutex/lock-free single-writer registry) and add a multi-threaded stress/TSAN test to the CI layer.

5. **Frame ID range count is internally inconsistent.**  
   `PLAN.md:156` and `PLAN.md:501` say frame IDs are `0xC80` to `0xC94 (22 frames)`, but that inclusive range contains 21 IDs (`0xC94 - 0xC80 + 1 = 21`). The parser, fixture pack, and reference decoder will disagree on the valid frame set.  
   **Fix:** Correct the range to `0xC80..0xC95` inclusive or change the count to 21, and update the parser tests, definition-pack fixtures, and A6.

6. **JSON parse depth is unbounded despite the package bounds.**  
   `3.6` bounds expanded size, entry count, asset size, expression depth, and path traversal (`PLAN.md:195-198`), but does not bound JSON nesting depth, node count, or string size. A small deeply nested package can still stack-overflow the RapidJSON/valijson path before schema validation. The malformed-input fuzzing is only for the binary telemetry parser.  
   **Fix:** Define and enforce maximum JSON nesting/depth, node count, and string length in both validators, add deep-JSON malicious fixtures to the corpus, and include a capped malformed-document harness for the C++ validator.

7. **Showcase sidecar behavior is contradictory: “range-clamped” versus “rejected.”**  
   `PLAN.md:289-291` says the bounded parameter set is “all range-clamped” but the gate requires an out-of-range value to be “rejected at load.” Clamping accepts and adjusts; rejection does not load.  
   **Fix:** Pick one behavior (schema-level rejection is cleaner for bounded authored parameters) and make the gate test that behavior consistently.

8. **The signal-core ABI gate is too weak to catch boundary breakage.**  
   `PLAN.md:217-218` claims the smoke commandlet reproduces the doctest suite result, but the gate only requires it to print the library version. That will not detect ABI miscompilation, wrong `-stdlib`, or exception/runtime mismatch in signal-core behavior across the Unreal boundary.  
   **Fix:** Require the in-engine commandlet to run a representative signal-core test subset (freshness, normalization, rule evaluation) and fail on assertion mismatch, or report the full doctest assertion/failure count.

9. **Temperature measurement path is not defined for the metrics gate.**  
   `6.5` requires every table cell filled, including temperature, for both Windows and device runs (`PLAN.md:318-320`), but the only thermal sampling specified is device thermal-zone sampling in `6.6` (`PLAN.md:322-324`). Windows GPU/CPU temperature may not be available without an explicit source.  
   **Fix:** Specify the per-platform temperature source (for example Android `dumpsys thermalservice` and Windows `nvidia-smi`/SMART sensor) and make the no-blank-cell gate conditional on source availability, recording “not measured” where it is unavailable.

VERDICT: REVISE

### Claude's synthesis

Finding keys: C = Codex (numbered as in its reply), G = Gemini (section numbers), D = DeepSeek (numbered).

- Accepted, resource bounds (C1, D6): 3.6 gains limits for document bytes, JSON nesting depth, JSON node count, string length, total expression nodes, component count, history samples per graph, image pixel dimensions and an aggregate decoded-texture budget per profile; the same limits apply to unpacked directories; the C++ validator gets its own capped malformed-document harness and deep-JSON fixtures join the corpus.
- Accepted, asset path resolution (C2): every asset reference in a document resolves through one package-root resolver; links, `..`, absolute paths, drive letters, duplicate normalized entries and case collisions are rejected; fixtures cover packed and unpacked inputs.
- Accepted, held values and sentinels (C3): definition-pack fields carry an acquisition semantic (`live` or `held`) and an optional list of raw sentinel values; held fields render with an explicit age-unknown marker and never count as a new measurement on a repeated identical payload; sentinels are rejected before conversion; held-value traffic is tested separately from heartbeat traffic (new test in 2.10).
- Accepted, registry and threading contract (C4, G3.1, D4): single-writer registry owned by the acquisition side; rules evaluate per sample on the acquisition side before any display coalescing, with latched warning state, so a brief condition is not lost; the game thread reads a double-buffered snapshot swapped once per frame and never blocks on acquisition; samples carry a connection generation and a sample from a previous generation is rejected after reconnect; queue tests force consumer stalls and reconnects; a ThreadSanitizer stress test runs on the Linux CI runner.
- Accepted, rule semantics (C5): literals carry units; comparisons and arithmetic require unit-compatible operands, checked at load; non-finite results become `invalid`; quality propagates (any `unavailable` or `invalid` input triggers the missing-input policy); `hold_last` has a maximum hold duration after which the result becomes `unavailable`; evaluation order is fixed as quality check, missing-input policy, evaluate, hysteresis, debounce, with reset semantics on reconnect; boundary fixtures added.
- Accepted, semantic validation pass (C6, D2): components become an ID-keyed map; both validators run an identical post-parse semantic pass (duplicate keys at parse time, reference resolution, field offset plus width within frame length, aspect policy inheritance); every gate that said "the schema rejects" now says "schema plus semantic pass rejects".
- Accepted, validator build path (C7, D3, G7 on decision 7): the C++ validator is an engine-independent CMake target in `packages/dashboard-spec` with vendored RapidJSON and valijson, compiled with exceptions and RTTI off and tested on hosted CI; the Unreal module compiles the same sources; if valijson turns out to need exceptions, `bEnableExceptions` is enabled on that one isolated module rather than dropping validation. The structural-only fallback in R-G is removed; if neither path compiles, 3.5 is blocked, not satisfied.
- Accepted, doctor profiles (C8, G2.1, D1): `doctor.ps1 -Profile toolchain|device`; the 150 GB figure is pre-install capacity, post-install build headroom is a separate 30 GB check; ADB reachability is required only by `deploy-deck.ps1` and `capture-metrics.ps1`; 0.9 depends on 0.8.
- Accepted, early Android spike (C9, C14, G2.2): new task 4.0 packages a minimal scene (one live simulator value, one runtime-imported texture, one dynamic material) under both RHIs on the head unit immediately after 4.1 and 0.5, and proves the on-device workflow: `android.permission.INTERNET` declared, packages, screenshots and CSVs under app-specific external storage `/sdcard/Android/data/<package>/files/` reachable by non-root `adb push` and `adb pull`, arguments via `UECommandLine.txt` in Development configuration and a `player.json` config file in every configuration, plus one Shipping build that launches from the config file alone.
- Accepted, metrics (C10, G5.2): 4.8 appends raw per-frame and per-sample events incrementally with a flush every second; run-level percentiles are computed by 6.5 from those events; receive-to-present is defined as sample receive timestamp to the frame present callback on the same monotonic clock; launch-to-first-usable-value is measured from process start (`am start -W` on Android) to a logged first-usable-frame event; texture upload completion is instrumented in 6.7.
- Accepted, qualification matrix (C11, G5.1): 6.4 becomes a matrix of control and showcase under Vulkan and GLES with the active RHI and driver strings logged from inside the app; 6.6 names its RHI and runs once per working RHI; new task 6.6b is the 8-hour desktop soak from PROJECT-PLAN section 8; suspend/resume on the device is a gate in 6.4; the memory criterion is a stated trend (slope over the post-warm-up window) rather than "monotonic".
- Accepted, temporal and edit gates (C12): 5.5 and 5.7 assert on per-frame state traces (needle angle, transition progress, visible-value flag) rather than single captures; 4.11 and 5.9 freeze scenario time, hash the whole packaged directory (executable plus cooked content) and run on both Windows and the device.
- Accepted, replay format (C13): recording format v1 keeps sequence, optional source timestamp and connection generation; replay clock rebasing, gap preservation, EOF behaviour (hold then stale) and loop flag are specified and tested at known times.
- Accepted, frame count and pack contract (C15, D5): the range 0xC80 to 0xC94 inclusive is 21 identifiers; the pack enumerates them; offset origin is payload byte 0, length is payload bytes, per-value signedness overrides the frame default, type carries width; the differential check covers every ID and every field boundary.
- Accepted, Stage 1 design gate (C16): 7.3 must specify command authorization (pairing token), serialized activation, a stationary-only hook, health-based promotion after N clean launches, a definition of "successful launch", and a crash-point test matrix.
- Accepted, sequencing (G1.1): 6.1 depends on 4.9, 4.10, 5.4, 5.5, 5.6, 5.7 and 5.8.
- Accepted, connector networking (G3.2): non-blocking connect with exponential backoff 500 ms to 5 s; `-connector-host` and `-connector-port` flags defaulting to 127.0.0.1:35000; `adb reverse` documented for bench runs.
- Accepted, affine conversion (G4.1): physical = raw times scale plus offset, with a fixture.
- Accepted, resync rule (G4.2): resync requires the tag, a frame id in the pack's set and, when bytes are available, the next tag 16 bytes later.
- Accepted, flaky gates (G6.1): 2.3 uses an absolute floor in the tolerance; 2.11 runs 1 million inputs or 30 seconds in CI and 10 million offline; 4.8's reproducibility gate becomes schema validity, row count and positive values; 2.1's negative-compile check becomes a `static_assert` on `std::is_constructible_v`.
- Accepted, decision 6 rewritten (G7 on decision 6, D8): `signal-core` sources live in the Unreal module `runtime/UnRealDash/Source/SignalCore/` and contain no Unreal headers; UBT compiles them with Epic's own flags for every target, so there are no prebuilt libraries and no toolchain files; `packages/signal-core/` holds the CMake project, doctest suite and CI build that compile the same files with exceptions and RTTI off, which is what enforces engine independence; 4.2's in-engine commandlet runs a representative subset of the signal-core checks and reports assertion counts, not just a version string. R-N shrinks to the residual risk that the two compilers disagree, caught by the commandlet.
- Accepted, LFS in CI (G7 on decision 10): every `actions/checkout` sets `lfs: false`.
- Accepted, sidecar behaviour (D7): out-of-range sidecar values are rejected at load; no clamping.
- Accepted, temperature source (D9): Android `dumpsys thermalservice`; Windows GPU via `nvidia-smi`; a cell with no available source is recorded as "not measured" and the no-blank-cell gate allows that value.
- Partially accepted (G7 on decision 7): exceptions are not enabled by default; they are enabled only on the isolated validator module and only if the no-exceptions CMake build fails. Reason: exceptions on a mobile player module have a code-size and unwind cost that a feasibility build should not pay unless it must.
- Rejected: none. Every finding was either a real defect or a real ambiguity.
- Reviewer disagreement: Codex accepted prebuilt `signal-core` libraries and asked for per-target builds; Gemini argued they should not exist; DeepSeek asked for a stronger ABI gate. Disposition: Gemini's approach is adopted, DeepSeek's stronger commandlet check is kept, and Codex's per-target request is moot.
