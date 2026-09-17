# Build chunk 10: the package loader

Status: draft for review, then a Codex build session. Covers PLAN.md task 4.4. Read PLAN.md (task 4.4, tasks 3.4 and 3.6, task 4.0's player.json, task 4.5's note that it owns the primitives, the Sequencing block) and docs/ARCHITECTURE.md before writing anything. PLAN.md is the authority on what; this file adds the exact mechanisms, proof commands and implementation constraints. If the two disagree, PLAN.md wins and the disagreement goes in the report.

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
- `tests/fixtures/packages/` holds 43 cases in `cases.json`, each with a `name` and an expected `code`. `well-formed` is the valid one.
- **17 of the 43 carry `"archive_only": true` and have no meaningful directory form.** They are the ones a filesystem cannot represent: malformed zip bytes, zip64 structures, ZIP central-directory bounds, DOS attribute and reparse flags, and path forms no filesystem will create (`absolute-path`, `drive-letter`, `path-traversal`, `symlink`, `duplicate-normalized`, `case-collision`, `unicode-case-collision`, `unix-device-entry`). A `<name>/` directory exists on disk for some of them, but it does not reproduce the defect and must not be treated as a second form.

  The archive-versus-directory parity in PLAN 4.4's gate therefore applies to the **26 cases without the flag**. Read the flag from `cases.json`; do not hardcode the list, and do not count directories on disk.
- `cases.json` entries may also carry a `profile` key, which selects `mobile` or `desktop` and therefore which texture budget applies. Honour it rather than running every case under one profile.
- `Document` is opaque: it exposes only `Storage&`. Whatever the widget builder needs must come through an accessor added in deliverable 2, not by reaching into `Storage`.
- Unreal's UBT compiles `.c` files in a module, so `miniz.c` can be built in-module. Confirm this rather than assuming it.

## Deliverables

### 1. `DashboardSpec` as an Unreal module

Follow the arrangement PLAN 4.2 established for `SignalCore` exactly, because it is what keeps the CMake build as the engine-independence enforcement:

- Move the sources to `runtime/UnRealDash/Source/DashboardSpec/` (`Public/dashboard_spec/` for the existing `include/dashboard_spec/` headers, `Private/` for `src/`), plus a `DashboardSpec.Build.cs` and one module-registration file carrying `IMPLEMENT_MODULE`.
- Change `packages/dashboard-spec/CMakeLists.txt` to compile those same files from their new location. `packages/dashboard-spec/` keeps its `CMakeLists.txt`, its doctest suite, its `schema/` directory, its tools and its README. Its build must stay green on `windows-latest` and `ubuntu-latest` with exceptions and RTTI off, and its case and assertion counts must not drop.
- `DashboardSpec.Build.cs`: `CppStandard = Cpp20`, `bEnableExceptions = false`, `bUseRTTI = false`, public dependency on `SignalCore`, private on `Core`, and system include paths to `third_party/rapidjson/include` and `third_party/valijson/include`. Compile `third_party/miniz/miniz.c` in the module. Do not define `_HAS_EXCEPTIONS` by hand; UBT sets it from `bEnableExceptions`, and chunk 01 already recorded that lesson in `SignalCore.Build.cs`.
- No Unreal header, Unreal type or engine macro enters any `DashboardSpec` source file except the module-registration file, which the CMake build excludes. Extend chunk 08's layering check in `scripts/doctor.ps1` to cover this module on the same terms as `SignalCore`.

**Prove `std::filesystem` on Android before building anything else.** Unreal's Android toolchain is not guaranteed to link it, and the entire directory-form path depends on it. Build the module for Android ARM64 as the **first** thing in this chunk and report the result. If it does not link, stop and report it; do not work around it by disabling the directory form, because the rejection-code parity in the gate requires both forms.

### 2. Give the reader a way to hand back what it read

Add to `PackageReader` an overload that returns content alongside the verdict. Keep the existing `Read(path, profile)` signature working and behaving identically, because `dashboard-spec-validate` and the 43-case test already depend on it.

```
class LoadedPackage {            // owns its storage; move-only; no copy
  public:
    const Document& Doc() const;
    // Asset bytes by normalized manifest name. Empty span when absent.
    std::span<const std::uint8_t> Asset(std::string_view normalized_name) const;
    std::span<const std::string_view> AssetNames() const;
};
Error PackageReader::Load(std::string_view path, Profile profile, LoadedPackage& out) const;
```

`Load` runs the identical validation `Read` runs, in the identical order, and populates `out` only when the returned `Error` is `Ok()`. Implement `Read` in terms of `Load` so the two can never diverge; a second copy of the validation sequence is the defect this deliverable exists to prevent.

Assets are returned as bytes. `LoadedPackage` does not decode images and does not know what a texture is.

For the directory form, `Asset` reads the file lazily on first request and caches it, still subject to `limits::asset_size`. For the archive form the bytes come from the already-decompressed entry. Both forms must apply the same bound, and a bound violation is the same code in both.

### 3. The engine-side loader (`UnRealDashCore`)

`Private/Package/DashPackageLoader.{h,cpp}` and `Public/UnRealDashCore/DashPackageLoader.h`.

- `bool LoadPackage(const FString& Path, EDashProfile Profile, FDashPackage& Out, FDashLoadError& OutError)`.
- `FDashLoadError` is a plain engine struct: `FString CodeName`, `int32 Code`, `FString Pointer`, `FString Message`, `FString Path`. It exposes **no `dashboard_spec::` type**, for the same layering reason `FFrameSnapshot` exposes no `signal_core::` type in chunk 08. `UnRealDashCore` is the only module that may include either library's headers.
- `Path` comes from `-udash=<path>`. Per PLAN 4.7 the flag itself is not this chunk's, but the loader must accept the value; wire it minimally so the gate can run and note that 4.7 owns the flag surface.
- Profile selects the 3.6 texture budget: `mobile` on Android, `desktop` on Windows, overridable. Do not invent a third profile.

### 4. The component registry and the widget tree

docs/ARCHITECTURE.md requires that adding a primitive means registering one builder and that nothing switches on a type string outside the registry. Build that seam now, because 4.5 and 4.6 both plug into it and a `switch` here would have to be torn out twice.

- `FComponentRegistry` maps a component type name to a builder callable. Registration is explicit at startup. An unknown type name is an error naming the type and the component's JSON pointer, not a silent skip.
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
- The schema files in `packages/dashboard-spec/schema/` must reach a packaged build. State how you did it and prove it on Windows; the device proof is Claude's.
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
3. All 43 fixtures load through the engine loader in archive form, each under the profile `cases.json` gives it. `well-formed` succeeds; the other 42 are rejected with the code `cases.json` expects.
4. For each of the **26 cases without `archive_only`**, the code and the JSON pointer are **identical** between the archive form and the directory form. A mismatch is a failure, not a note. The 17 archive-only cases are run in archive form only, and the report states that count so a future reader can tell a deliberate exclusion from a skipped test.
5. Launching with `well-formed` renders the placeholder widget tree, in both packed and unpacked form.
6. Launching with each of the 42 rejection cases shows the on-screen error naming the file and the pointer, and does not crash. Run all 42, not a sample.
7. `scripts/doctor.ps1 -Profile workstation` exits 0 including the extended layering check, and the full Pester suite passes with no reduction in count.
8. The device half of the PLAN 4.4 gate runs on the desk device (the Pixel), not the head unit, per PLAN 4.4. **This requires the phone connected over USB and is Claude's to run, not Codex's.** If the phone is unavailable the criterion is reported as not run, and the chunk is not complete until it is.

## Proof

Codex runs these and pastes the output verbatim. Codex has no engine, no Android toolchain and no device.

```
cmake --preset windows-msvc && cmake --build --preset windows-msvc
ctest --preset windows-msvc --output-on-failure
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -Output Detailed"
git status --short
```

`[claude]` The UBT builds for Win64 and Android, the 43-case load run in both forms with the code and pointer comparison, the placeholder render, the 42 on-screen error checks, and the desk-device half.

Codex's pasted proof is advisory. Claude re-runs everything.

## Report format

End with: files added or changed (one line each: path, what, which PLAN.md task), the proof output verbatim for what you ran, the Android `std::filesystem` result, how the schema files reach a packaged build, an explicit list of what you did not run and why, any deviation from PLAN.md or this spec with the reason, and anything you could not do.
