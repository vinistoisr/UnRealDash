# Build chunk 10: the package loader

Status: revision 3. A build session refused revision 2 on a prerequisite error: the spec said 1 accepted case and 42 rejected, where cases.json has 7 and 36. It also found that moving the schemas would break the Python tools rule in ARCHITECTURE.md line 22, so they are now staged rather than moved. Revision 2 followed a DeepSeek spec review that returned VERDICT: REVISE with one blocker and four majors, and after the Android filesystem question was answered on the device rather than left as a risk. Frozen for a Codex build session. Covers PLAN.md task 4.4. Read PLAN.md (task 4.4, tasks 3.4 and 3.6, task 4.0's player.json, task 4.5's note that it owns the primitives, the Sequencing block) and docs/ARCHITECTURE.md before writing anything. PLAN.md is the authority on what; this file adds the exact mechanisms, proof commands and implementation constraints. If the two disagree, PLAN.md wins and the disagreement goes in the report.

## Goal

This is where `dashboard-spec` finally reaches the engine. Everything it needs already exists and is proven: the bounded parser, the schema validator, the semantic pass, the limit set, the path resolver and the package reader all shipped in chunk 03, and `tests/fixtures/packages/` already holds 43 cases in both packed and unpacked form with their expected codes in `cases.json`.

What does not exist is any of it inside Unreal. `dashboard-spec` is a CMake static library today. This chunk makes it a UBT module, gives the reader a way to hand back what it read instead of only a verdict, and puts a readable error on screen when a package is rejected.

It does **not** draw any dashboard primitive. That is 4.5.

## Facts this chunk depends on

Read out of the tree, not assumed. If one is wrong, stop and report it rather than inventing a replacement.

- `packages/dashboard-spec` builds as C++20 with exceptions and RTTI off (`_HAS_EXCEPTIONS=0`, `VALIJSON_USE_EXCEPTIONS=0`), links `signal_core` publicly and `miniz` privately, and takes `rapidjson` and `valijson` as system include paths from `third_party/`.
- `PackageReader::Read(std::string_view path, Profile profile)` returns an `Error` and **nothing else**. It validates; it does not hand back the document or the assets. Extending it is deliverable 2.
- One `PackageReader::Read` already handles both forms: it branches on `std::filesystem::is_directory` at `src/PackageReader.cpp:127`. docs/ARCHITECTURE.md requires exactly one interface, one bound set and one path resolver for both. Do not add a second reader.
- `std::filesystem` is load-bearing for security, not convenience. `src/PackageReader.cpp` uses `symlink_status`, `is_symlink`, `hard_link_count` and `canonical` to implement the traversal, symlink, hard-link and case-collision rejections. **Do not replace it with an injected file-system interface.** Unreal's `IPlatformFile` does not expose hard-link counts or symlink status, so the substitution would silently weaken checks that 43 fixtures currently prove. This is the one place where docs/ARCHITECTURE.md's "inject the file system" rule loses to a stronger requirement, and the report must say so.
- `Error` carries `code`, `pointer` and `message`, and `CodeName(ErrorCode)` returns a stable string. The on-screen error in PLAN 4.4's gate is built from these three; no new error type is needed.
- `tests/fixtures/packages/` holds 43 cases in `cases.json`, each with a `name` and an expected `code`. **An empty `code` means the case must be ACCEPTED.**

  The split is **7 accepted and 36 rejected**, not 1 and 42. An earlier revision of this spec said `well-formed` was the only valid one, reasoning from its name; that is wrong and a build session correctly refused to proceed on it. The seven accepted cases are `gray-image`, `normalized-manifest-key`, `normalized-reference-dot`, `normalized-reference-slashes`, `rgb-image`, `well-formed` and `windows-attributes-entry`. They exist to prove that valid-but-awkward forms are **not** rejected, which is exactly the kind of case a loader tends to break.

  Cross-tabulated against `archive_only`, because the criteria need both axes:

  | | accepted | rejected | total |
  | --- | ---: | ---: | ---: |
  | both forms | 6 | 20 | 26 |
  | archive only | 1 | 16 | 17 |
  | **total** | **7** | **36** | **43** |

  The one accepted archive-only case is `windows-attributes-entry`. Derive all of these from `cases.json` at runtime rather than hardcoding them; the numbers are here so a miscount is visible rather than silent.
- **17 of the 43 carry `"archive_only": true` and have no meaningful directory form.** They are the ones a filesystem cannot represent: malformed zip bytes, zip64 structures, ZIP central-directory bounds, DOS attribute and reparse flags, and path forms no filesystem will create (`absolute-path`, `drive-letter`, `path-traversal`, `symlink`, `duplicate-normalized`, `case-collision`, `unicode-case-collision`, `unix-device-entry`). A `<name>/` directory exists on disk for some of them, but it does not reproduce the defect and must not be treated as a second form.

  The archive-versus-directory parity in PLAN 4.4's gate therefore applies to the **26 cases without the flag**. Read the flag from `cases.json` at runtime rather than hardcoding it, and do not count directories on disk. The two sets as they stand today, so a miscount is visible rather than silent:

  **archive_only, 17:** `absolute-path`, `case-collision`, `central-directory-byte-bound`, `central-directory-entry-count`, `dos-reparse-entry`, `drive-letter`, `duplicate-normalized`, `json-underdeclared-size`, `malformed-zip`, `path-traversal`, `symlink`, `unicode-case-collision`, `unix-device-entry`, `windows-attributes-entry`, `zip-bomb`, `zip64-locator-outside-file`, `zip64-small-legacy`.

  **both forms, 26:** the remainder, including `well-formed`.
- `cases.json` entries may also carry a `profile` key, which selects `mobile` or `desktop` and therefore which texture budget applies. Honour it rather than running every case under one profile.
- **Every file read in `dashboard-spec` goes through one function**, `ReadBounded` in `src/BoundedParse.cpp:10`, and it uses `std::ifstream` over a `std::filesystem::path`. Its callers are the schema loader (`SchemaValidation.cpp`), the directory-form package entry reader (`PackageReader.cpp`), the CLI (`validate_main.cpp`) and the doctest suite's own `tests/support/TestSupport.h`. Verify the list with `scripts/verify-chunk10-facts.py` rather than trusting this sentence.

  This is the single most important Android fact in this chunk. `std::ifstream` reads the **real filesystem**. A file staged as UFS lives inside the `.pak` and `std::ifstream` cannot see it at all, so anything this library must read has to be staged as **non-UFS** or pushed to the device. The `-udash=` package is already a real file, so it is fine; the schema files are not, and deliverable 1a fixes that.

  Do not "fix" this by routing `ReadBounded` through `IPlatformFile`. That would drag an Unreal type into a library the CMake build compiles with no engine on the path, which breaks the enforcement in docs/ARCHITECTURE.md.
- The five schema files live in `packages/dashboard-spec/schema/`. `DASHBOARD_SCHEMA_DIR` is a compile-time absolute path defined in `CMakeLists.txt`. It is a build-machine path and is meaningless on a device, so the engine must supply a real runtime path; `Validator`'s constructor takes the directory as a string.

  **It is not only used by the CLI.** An earlier revision of this spec said so, from a grep scoped to `src/` and `include/` that never looked in `tests/` or `tools/`. It is used by **eight** files: `validate_main.cpp`, `tools/telemetry-decode-jsonl.cpp`, and six test files (`test_corpus`, `test_definition_pack_builder`, `test_malformed_harness`, `test_package_reader`, `test_schema`, `test_semantic`). Changing or removing it would break the entire doctest suite, which is why deliverable 1a leaves it alone.
- `Document` is opaque: it exposes only `Storage&`. Whatever the widget builder needs must come through an accessor added in deliverable 2, not by reaching into `Storage`.
- Unreal's UBT compiles `.c` files in a module, so `miniz.c` builds in-module. Checked, not assumed: `Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildModuleCPP.cs:3078` lists `.c` among the compiled extensions and line 3129 branches on it. `third_party/miniz/miniz.c` is present.
- The CMake presets are named **`default`** (and `tsan` for signal-core), not `windows-msvc`, and each presets file lives inside its package directory, so preset commands run from there. This is what CI already uses in `signal-core.yml` and `spec-tools.yml`.

## Deliverables

### 1. `DashboardSpec` as an Unreal module

Follow the arrangement PLAN 4.2 established for `SignalCore` exactly, because it is what keeps the CMake build as the engine-independence enforcement:

This is a **move, not a copy.** After it, exactly one copy of each source file exists in the repository, and `packages/dashboard-spec/include/` and `packages/dashboard-spec/src/` are **deleted**. Do not leave forwarding headers. Every step below is required; a partial move leaves a tree where CMake and UBT disagree about which file is authoritative.

1. `git mv packages/dashboard-spec/include/dashboard_spec/*` to `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/`, and `packages/dashboard-spec/src/*` to `runtime/UnRealDash/Source/DashboardSpec/Private/`. Delete the emptied directories.
2. The public headers include each other as `dashboard_spec/Foo.h`, so the include root is the directory **containing** `dashboard_spec/`. Both builds must see the same root: UBT gets `PublicIncludePaths.Add(.../DashboardSpec/Public)` and CMake gets `target_include_directories(dashboard_spec PUBLIC .../DashboardSpec/Public)`. `Private/` is an include root for neither; `Private/Internal.h` and `Private/PackageInternal.h` are included by path relative to the including file, exactly as they are today.
3. Update `packages/dashboard-spec/CMakeLists.txt`'s source list to the new paths, and exclude the module-registration file.
4. `DASHBOARD_SCHEMA_DIR` is unchanged: the schemas stay where they are, per deliverable 1a.
5. The doctest suite and `tests/fixtures/` do **not** move. Check every relative path the tests and tools resolve against `CMAKE_CURRENT_SOURCE_DIR` or `REPO_ROOT` and fix any that assumed the sources sat beside them. `REPO_ROOT` is already defined in the CMakeLists for this purpose.
6. Update `.github/workflows/spec-tools.yml` and `signal-core.yml` for any path that names `packages/dashboard-spec/src` or `include`. Both workflows are currently green; they must stay green.

`packages/dashboard-spec/` keeps its `CMakeLists.txt`, its `CMakePresets.json`, its doctest suite, its tools and its README. Its build must stay green on `windows-latest` and `ubuntu-latest` with exceptions and RTTI off, and its case and assertion counts must not drop. Take those counts from the doctest reporter, not from reading the source.
- `DashboardSpec.Build.cs`: `CppStandard = Cpp20`, `bEnableExceptions = false`, `bUseRTTI = false`, public dependency on `SignalCore`, private on `Core`, and system include paths to `third_party/rapidjson/include` and `third_party/valijson/include`. Compile `third_party/miniz/miniz.c` in the module. Do not define `_HAS_EXCEPTIONS` by hand; UBT sets it from `bEnableExceptions`, and chunk 01 already recorded that lesson in `SignalCore.Build.cs`.
- No Unreal header, Unreal type or engine macro enters any `DashboardSpec` source file except the module-registration file, which the CMake build excludes. Extend chunk 08's layering check in `scripts/doctor.ps1` to cover this module on the same terms as `SignalCore`.

#### 1a. Get the schema files onto the device as real files

`Validator` reads its five schemas with `std::ifstream`, so they must exist on the filesystem of whatever machine runs the player.

**The schemas do not move.** They stay at `packages/dashboard-spec/schema/`. An earlier revision of this spec moved them to `runtime/UnRealDash/Schema/`, which would have broken an explicit rule: docs/ARCHITECTURE.md line 22 says Python tools under `tools/` depend on `packages/dashboard-spec/schema/` and **never import from the runtime tree**, and `tools/dashboard_spec/schema.py:14` resolves that exact path. Moving the schemas would have forced a Python tool to read the runtime tree to keep working.

Staging, not moving, is the answer:

- In `DashboardSpec.Build.cs`, add each of the five schema files as a `RuntimeDependency` whose **source** is `packages/dashboard-spec/schema/<name>.schema.json` and whose **staged destination is pinned** to `$(ProjectDir)/Schema/<name>.schema.json`, with `StagedFileType.NonUFS` so packaging puts them on the filesystem rather than inside the `.pak`.

  **This is not sufficient on Android, proven on the device 2026-09-17.** The `RuntimeDependency` stages correctly to `Saved/StagedBuilds/Android/UnRealDash/Schema/`, but with `bPackageDataInsideApk=True` the APK carries only the cooked UFS data, as `assets/main.obb.png`. A listing of the built APK finds **zero** schema entries, the directory never appears on the device, and every package is rejected with `E_SCHEMA` "cannot read schema dashboard". Pushing the five files by hand makes the same fixture pass, so the loader is fine and only the delivery is broken. See `docs/reports/2026-09-17-device-package-gate.md` for the evidence and three candidate fixes; the choice is the owner's because one of them changes `Validator`'s constructor. Do not let the destination default: the default mirrors the source path, and the resolver below would look in the right place and find nothing. That failure is invisible on Windows, where the repository copy exists anyway, and only appears on device.
- `UnRealDashCore` resolves the directory as `FPaths::ProjectDir()` joined with `Schema`, passed through `IPlatformFile::GetPlatformPhysical().ConvertToAbsolutePathForExternalAppForRead`. Use the **read** variant, not the write one. The 4.0 smoke spike used `...ForWrite` because it was resolving an output directory; this is an input directory, and on Android the two can resolve to different physical roots. Log the resolved path once at startup.
- `DASHBOARD_SCHEMA_DIR`, the CMake build and the Python tools are all unchanged by this, which is the point of staging rather than moving.
- Prove it on the device, not just on Windows: list the five files at the resolved path on the Pixel, and read one of them with the validator, before claiming this works.

**`std::filesystem` and `std::ifstream` are already proven on the device.** This was measured on 2026-09-17 on the Pixel 10 Pro, Vulkan, Development, by a probe in the 4.0 smoke spike reading a real pushed file under the app-specific external storage path. Results, all with `error=0`:

```
probe ifstream open=true read_bytes=4096
probe is_directory=true
probe symlink_status is_symlink=false
probe hard_link_count=1
probe canonical path=/storage/emulated/0/Android/data/com.unrealdash.player/files/UnrealGame/UnRealDash/UnRealDash/Saved/smoke.png
probe file_size=94346
probe recursive_directory_iterator entries=32
```

`hard_link_count` and `canonical` are the two that the traversal and symlink rejections depend on, and both work. So the directory form is viable on Android and this chunk does not need a fallback. You still build the module for Android ARM64 as the **first** step, because linking the whole library is a bigger surface than one probe, and you report that result either way.

### 2. Give the reader a way to hand back what it read

Add to `PackageReader` an overload that returns content alongside the verdict. Keep the existing `Read(path, profile)` signature working and behaving identically, because `dashboard-spec-validate` and the 43-case test already depend on it.

```
class LoadedPackage {
  public:
    LoadedPackage();
    ~LoadedPackage();
    LoadedPackage(LoadedPackage &&) noexcept;             // move-only
    LoadedPackage &operator=(LoadedPackage &&) noexcept;
    LoadedPackage(const LoadedPackage &) = delete;
    LoadedPackage &operator=(const LoadedPackage &) = delete;

    const Document &Doc() const;
    // Errors are values here too. Returns E_PKG_ASSET_NOT_IN_PACKAGE when the name is unknown,
    // and E_PKG_ASSET_UNREADABLE when a directory-form read or an allocation fails.
    Error Asset(std::string_view normalized_name, std::span<const std::uint8_t> &out) const;
    std::span<const std::string_view> AssetNames() const;

  private:
    struct Storage;
    Storage *storage_;      // owns the document, the asset bytes and the name table
};
Error PackageReader::Load(std::string_view path, Profile profile, LoadedPackage &out) const;
```

**`LoadedPackage` owns everything it hands out and outlives the `PackageReader` that produced it.** It must not hold a pointer into the reader, the `Validator`, or a temporary buffer. Every span it returns points into `Storage`, and stays valid until the `LoadedPackage` is destroyed or moved from. This is stated because the alternative, spans into the reader, dangles the moment the reader goes out of scope and the compiler will not catch it.

`Asset` is `const` and caches lazily for the directory form, so the cache member is `mutable`. Say so in the header.

`Asset` returns `Error` rather than a bare span because the library compiles with exceptions off: a failed allocation for a 16 MiB asset cannot throw, and a bare span would have to report it as "absent", which is a silent fallback. Add `E_PKG_ASSET_UNREADABLE` to `ErrorCode` for that case; it is the only new code this chunk adds, and it takes the next free numeric value. Do not renumber anything.

**Do not implement `Read` by calling `Load` and discarding the result.** That was in an earlier revision of this spec and it is incoherent: `Read` is documented as behaving identically to today, and today it returns a verdict without materializing the document or decompressing every asset. `Load`-then-discard would silently turn a validate-only call into a full load, change which limit violations surface at which stage, and re-point the 43 fixtures at a different code path.

Instead, **both call one shared private function**:

```
Error Validate(std::string_view path, Profile profile, LoadedPackage* out) const;
```

`Read` passes `nullptr` and materializes nothing, so its behaviour and its cost are unchanged. `Load` passes `&out`, and the only difference is that the materializing branches run. There is exactly one validation sequence, in one order, which is what this deliverable is actually for.

Assets are returned as bytes. `LoadedPackage` does not decode images and does not know what a texture is.

For the directory form, `Asset` reads the file lazily on first request and caches it, still subject to `limits::asset_size`. For the archive form the bytes come from the already-decompressed entry. Both forms must apply the same bound, and a bound violation is the same code in both.

### 3. The engine-side loader (`UnRealDashCore`)

`Private/Package/DashPackageLoader.{h,cpp}` and `Public/UnRealDashCore/DashPackageLoader.h`.

- `bool LoadPackage(const FString& Path, EDashProfile Profile, FDashPackage& Out, FDashLoadError& OutError)`.
- `FDashLoadError` is a plain engine struct: `FString CodeName`, `int32 Code`, `FString Pointer`, `FString Message`, `FString Path`. It exposes **no `dashboard_spec::` type**, for the same layering reason `FFrameSnapshot` exposes no `signal_core::` type in chunk 08. `UnRealDashCore` is the only module that may include either library's headers.
- `Path` comes from `-udash=<path>`. PLAN 4.7 owns the flag **surface**: the full flag set, the unknown-flag rejection, and the `player.json` equivalence. This chunk wires exactly one thing, and the boundary is precise so 4.7 has nothing to undo: read `-udash=` from the command line in the game module at startup and pass the value to `LoadPackage`. Do not add a flag table, do not validate unknown flags, do not touch `player.json`. If `-udash=` is absent, log that and show the same on-screen message path deliverable 5 defines, rather than loading a built-in document.
- Profile selects the 3.6 texture budget: `mobile` on Android, `desktop` on Windows, overridable. Do not invent a third profile.

### 4. The component registry and the widget tree

docs/ARCHITECTURE.md requires that adding a primitive means registering one builder and that nothing switches on a type string outside the registry. Build that seam now, because 4.5 and 4.6 both plug into it and a `switch` here would have to be torn out twice.

- `FComponentRegistry` maps a component type name to a builder callable. Registration is explicit at startup. An unknown type name is an error naming the type and the component's JSON pointer, not a silent skip.

  **Pin the callable's signature now**, because a later task that has to widen it is a later task editing this registry, which is the thing the seam exists to prevent:

  ```cpp
  struct FComponentContext
  {
      const FDashComponent& Component;   // this node: type, id, parent id, properties
      const FDashPackage&   Package;     // assets and theme tokens, for asset resolution
      EDashProfile          Profile;     // mobile or desktop
      UWidget*              Parent;      // already built, may be null for the root
  };
  using FComponentBuilder = TFunction<UWidget*(const FComponentContext&, FDashLoadError&)>;
  ```

  A builder returns `nullptr` and fills `OutError` on failure. Everything a primitive needs comes through `FComponentContext`, so adding a primitive adds one registration and nothing else.

- `FDashComponent` is the engine-side view of one node: type name, stable id, parent id, JSON pointer, and typed property access. It exposes **no `dashboard_spec::` type**, for the same layering reason as `FDashLoadError`. `Document` is opaque, so deliverable 2 adds the accessor that walks the component section and yields these nodes; pin that accessor here rather than leaving 4.5 to invent it.
- `FWidgetTreeBuilder` walks the document's component tree and calls the registry for each node, producing a parent/child `UWidget` tree with the document's IDs attached. It resolves parents by ID and never assumes a component's position in the tree.
- This chunk registers **one** builder: a placeholder that renders the component's type name and ID in a box. That is enough to satisfy "renders" in the 4.4 gate and it is deliberately ugly. 4.5 replaces it by registering real builders; it must not have to modify the builder or the registry to do so.

### 5. The rejection display

On a rejected package the player shows an on-screen error and does not crash. It names, on separate lines: the file path as given, `CodeName(code)`, the numeric code, the JSON pointer, and the message. It must be readable at 1280x720 and on the device, which means no clipping and no reliance on a scroll the user cannot perform.

The same rejection must produce the same `code` and the same `pointer` whether the input was the archive or the directory. That equality is the gate, and it is why `Read` and `Load` share one validation path.

A rejected package leaves the player running and showing the error. It does not exit, assert, or fall back to a built-in document. Silent fallbacks are forbidden by docs/ARCHITECTURE.md.

## Constraints

- No `dashboard_spec::` or `signal_core::` name appears in the `UnRealDash` game module. `UnRealDashCore` is the only wrapper. The doctor check from chunk 08 enforces it.
- Nothing in `DashboardSpec/` gains an Unreal header outside the module-registration file.
- Errors are values everywhere. No `check()`, `verify()` or `ensure()` on any path a hostile package can reach. A malformed package is expected input, not a programming error.
- The schema files must reach a packaged build as real files, per deliverable 1a. State how you did it. The device proof is Claude's.
- No em dashes in any file, comment or printed string.

## Non-goals

- No dashboard primitive. No dial, gauge, readout, graph or theme token. That is 4.5 and 4.6.
- No command-line surface. That is 4.7.
- No package staging or rollback. That is Stage 1.
- No image decode to `UTexture2D` beyond what the placeholder needs, which is nothing.
- No head-unit work.

## Pass/fail criteria

1. `DashboardSpec` builds under UBT for Win64 **and** Android ARM64, and the Android result is reported explicitly whichever way it goes.
2. The CMake `dashboard-spec` build stays green on Windows and Linux with exceptions and RTTI off, and its doctest case and assertion counts do not drop.
3. All 43 fixtures load through the engine loader in archive form, each under the profile `cases.json` gives it. The **7 accepted** cases load successfully and the **36 rejected** cases fail with the code `cases.json` expects. An accepted case that is rejected is a failure, and so is the reverse.
4. For each of the **26 cases without `archive_only`**, both forms reach the same verdict and, when that verdict is a rejection, carry an **identical** code and JSON pointer. A mismatch is a failure, not a note.

   Two traps to close, because tail equality alone is a weaker gate than it looks:
   - A case that is rejected in one form and **accepted** in the other fails this criterion. Do not compare codes only among the cases that happened to reject in both.
   - The **6 accepted both-form cases** must be accepted in both forms. A gate that only checks rejections would pass if every case were rejected for the same reason, so the accepted cases are what stop that.

   The 17 archive-only cases run in archive form only. The report states that count, so a deliberate exclusion cannot later be mistaken for a skipped test.
5. Launching with `well-formed` renders the placeholder widget tree, in both packed and unpacked form.
6. Launching with each of the **36 rejection** cases shows the on-screen error naming the file and the pointer, and does not crash. Run all 36, not a sample. Launching with each of the 7 accepted cases renders instead of showing an error.
7. `scripts/doctor.ps1 -Profile workstation` exits 0 including the extended layering check, and the full Pester suite passes with no reduction in count.
8. The device half of the PLAN 4.4 gate runs on the desk device (the Pixel), not the head unit, per PLAN 4.4. **This requires the phone connected over USB and is Claude's to run, not Codex's.**

   Criteria 1 to 7 together mean the chunk is **built**. Criterion 8 is what makes it **complete**. A build session that satisfies 1 to 7 reports "built, device half not run" and is not claiming the chunk is done; only the device run closes it. State it in exactly those terms so the two are never conflated.

## Proof

Codex runs these and pastes the output verbatim. Codex has no engine, no Android toolchain and no device.

```
cd packages/dashboard-spec && cmake --preset default && cmake --build --preset default
cd packages/dashboard-spec && ctest --preset default --output-on-failure
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI -Output Detailed"
git status --short
```

`[claude]` The UBT builds for Win64 and Android, the 43-case load run in both forms with the code and pointer comparison, the placeholder render, the 42 on-screen error checks, and the desk-device half.

Codex's pasted proof is advisory. Claude re-runs everything.

## Report format

End with: files added or changed (one line each: path, what, which PLAN.md task), the proof output verbatim for what you ran, the Android `std::filesystem` result, how the schema files reach a packaged build, an explicit list of what you did not run and why, any deviation from PLAN.md or this spec with the reason, and anything you could not do.
