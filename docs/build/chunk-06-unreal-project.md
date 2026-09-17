# Build chunk 06: Unreal project skeleton

Status: frozen spec for a Codex build session. Covers PLAN.md tasks 4.1 and 4.2 (all three halves). Read PLAN.md (those tasks, the Approach preamble, the pin table, Key decisions 5 and 6, risk R-N) and docs/ARCHITECTURE.md before writing anything. PLAN.md is the authority on what; this file adds the exact file layout, the settings, the commands and the proof. If the two disagree, PLAN.md wins and the disagreement goes in the report.

This is the first chunk that compiles Unreal code. The toolchain is now installed and verified: `scripts/doctor.ps1` exits 0 for the `workstation`, `android` and `linux` profiles.

## Goal

An Unreal C++ project that builds for Windows, Android ARM64 and Linux x86-64, compiling the existing engine-independent signal-core sources through Unreal's own toolchain for each target, and proving those sources behave the same in the engine as they do in the standalone CMake build. Nothing renders yet and no dashboard is loaded; this chunk is the compile-and-link foundation the player is built on.

## Scope note, and a deliberate deviation from PLAN.md 4.1

PLAN.md 4.1's deliverable names a `UnRealDashCore` module "wrapping `signal-core` and `dashboard-spec`". This chunk wraps **signal-core only**. Reason: dashboard-spec's sources live under `packages/dashboard-spec/` with three vendored libraries at the repository root, so wrapping it means relocating those sources into the Unreal `Source/` tree and getting RapidJSON, valijson and miniz through Unreal's warnings-as-errors build. That work belongs with task 4.4, the package loader, which is the first task that needs to read a document inside the engine. 4.1's gate is unaffected: it asks for an editor binary, a Win64 binary, no warnings promoted to errors in the project's own modules, and the toolchain-isolation build. Report this deviation.

## Deliverables

### 1. Project and targets

```
runtime/UnRealDash/UnRealDash.uproject
runtime/UnRealDash/Source/UnRealDash.Target.cs
runtime/UnRealDash/Source/UnRealDashEditor.Target.cs
runtime/UnRealDash/Source/UnRealDash/UnRealDash.Build.cs
runtime/UnRealDash/Source/UnRealDash/UnRealDashModule.cpp
runtime/UnRealDash/Source/UnRealDash/Public/UnRealDashLog.h
runtime/UnRealDash/Source/UnRealDash/Private/UnRealDashLog.cpp
runtime/UnRealDash/Source/UnRealDashCore/UnRealDashCore.Build.cs
runtime/UnRealDash/Source/UnRealDashCore/UnRealDashCoreModule.cpp
runtime/UnRealDash/Source/UnRealDashCore/Public/UnRealDashCore/SignalCoreAdapter.h
runtime/UnRealDash/Source/UnRealDashCore/Private/SignalCoreAdapter.cpp
runtime/UnRealDash/Source/UnRealDashCore/Private/SignalCoreSmokeCommandlet.h
runtime/UnRealDash/Source/UnRealDashCore/Private/SignalCoreSmokeCommandlet.cpp
runtime/UnRealDash/Config/DefaultEngine.ini
runtime/UnRealDash/Config/DefaultGame.ini
runtime/UnRealDash/Config/DefaultInput.ini
```

`UnRealDash.uproject`: `"EngineAssociation": "5.8"`, `"Modules"` listing `UnRealDash` (type `Runtime`, loading phase `Default`), `UnRealDashCore` (type `Runtime`, `PreDefault`) and `SignalCore` (type `Runtime`, `PreDefault`), and no plugins beyond what an empty C++ project needs. Do not enable Starter Content or any editor-only plugin.

Both `.Target.cs` files set `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `CppStandard = CppStandardVersion.Cpp20`, and `ExtraModuleNames.AddRange(new[] { "UnRealDash" })`. The game target is `TargetType.Game`; the editor target is `TargetType.Editor`.

`SignalCore.Build.cs` and `SignalCoreModule.cpp` already exist from chunk 02, written to the pin and never compiled. This chunk is where they are proven. Change them only if they do not compile, and report any change with the compiler error that forced it.

### 2. Module rules

`UnRealDash.Build.cs`: `PublicDependencyModuleNames` = `Core`, `CoreUObject`, `Engine`, `UnRealDashCore`. `bUseUnity = false`. No RTTI, no exceptions (the engine defaults; do not enable them).

`UnRealDashCore.Build.cs`: `PublicDependencyModuleNames` = `Core`, `SignalCore`. `PrivateDependencyModuleNames` = `CoreUObject`, `Engine`. `bUseUnity = false`.

`bUseUnity = false` on the project's own modules is deliberate: unity builds hide missing includes, and the signal-core sources must keep compiling stand-alone.

The adapter in `UnRealDashCore` is the only place that converts `signal_core` types into engine types. It lives in the player module, not in signal-core, per PLAN.md 4.2. For this chunk it needs only enough surface to support the smoke commandlet: converting a `signal_core::Sample` to a small plain struct the engine side can hold, and converting `signal_core::Quality` and `AgeEvidence` to `FString` for logging. Do not invent a widget-facing API here; 4.3 and 4.5 define that.

### 3. Rendering and platform settings

`Config/DefaultEngine.ini`:

- `[/Script/Engine.RendererSettings]`: `r.Mobile.ShadingPath=0`, `r.Lumen.Supported=False`, `r.Nanite.ProjectEnabled=False`, `r.RayTracing=False`, `r.SkinCache.CompileShaders=False`, `r.Mobile.AntiAliasing=1`, `r.DefaultFeature.MotionBlur=False`, `r.DefaultFeature.Bloom=False`, `r.DefaultFeature.AmbientOcclusion=False`, `r.DefaultFeature.AutoExposure=False`. These follow PLAN.md Key decision 5 and section 8: no dependency on Lumen, Nanite or ray tracing.
- `[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]`: `PackageName=com.unrealdash.player`, `MinSDKVersion=26`, `TargetSDKVersion=35`, `bBuildForArm64=True`, `bBuildForX8664=False`, `bSupportsVulkan=True`, `bBuildForES31=True` (the OpenGL ES 3.2 path), `bAndroidVoiceEnabled=False`, and `ExtraPermissions=android.permission.INTERNET`. The two RHIs are both enabled in the project; the packaging scripts select which one an APK ships with, in a later chunk. `MinSDKVersion=26` and `TargetSDKVersion=35` are the project build settings PLAN.md 6.2's gate reads out of the built manifest; they are not the installed SDK platform, which is android-36 per the engine manifest.
- `[/Script/LinuxTargetPlatform.LinuxTargetSettings]`: `TargetArchitecture=X86_64UnknownLinuxGnu`, `SpirvOptimization=False`.
- `[/Script/Engine.Engine]` and `[Core.Log]`: a `LogUnRealDash` category at `Log` verbosity.

`Config/DefaultGame.ini`: project id, description, and `[/Script/UnrealEd.ProjectPackagingSettings]` with `BuildConfiguration=PPBC_Development`, `FullRebuild=False`, `UsePakFile=True`, `bCompressed=True`.

### 4. The smoke commandlet (PLAN.md 4.2, Win64 half)

`SignalCoreSmokeCommandlet` is a `UCommandlet` registered as `SignalCoreSmoke`. It runs a representative subset of the standalone checks **in-process**, against the same sources the CMake build compiles:

- freshness: a registry with one signal on a 500 ms deadline, a sample at 5 ms, a fake clock advanced past 505 ms, asserting the transition to `stale`;
- unit normalization: degC, degF and kelvin round-trips within the tolerance the standalone tests use, plus a rejected unknown unit string;
- rule evaluation: a threshold rule with hysteresis over a literal carrying a unit, asserting the output at a value below, at and above the threshold.

It counts assertions itself (a small `Check(bool, name)` helper incrementing a pass or fail counter), prints exactly one line `SignalCoreSmoke assertions=<n> failures=<n>`, and returns 0 only when failures is 0. **The assertion count must equal the count the standalone build reports for the same subset**, which is what makes the comparison meaningful rather than a version string. To make that comparison exact, add a matching subset runner to the standalone side: `packages/signal-core/tools/smoke-subset.cpp` building an executable `signal-core-smoke-subset` that performs the same three groups with the same `Check` helper and prints the same line format. Both must print the same assertion count. Keep the two implementations in one header shared by both, `runtime/UnRealDash/Source/SignalCore/Public/SignalCore/SmokeSubset.h`, so they cannot drift: the header holds the checks as a function taking a callback, and each side supplies its own reporting. It is public because a different module, `UnRealDashCore`, includes it, and docs/ARCHITECTURE.md forbids including another module's `Private/` headers. Like every other signal-core header it must contain no Unreal types, so the CMake build still compiles it.

Do not put the 2.13 stress harness in this commandlet. PLAN.md 4.2 assigns it to 4.3.

### 5. Build script

`scripts/build.ps1` exists from chunk 01 as a skeleton that previews a `RunUAT` command line under `-WhatIf`. Extend it so that without `-WhatIf` it actually builds, using `Engine/Build/BatchFiles/Build.bat` for module builds (not `RunUAT BuildCookRun`, which packages): `Build.bat <Target> <Platform> <Configuration> -project="<uproject>" -waitmutex`. Keep the doctor gate, the profile selection and the `-WhatIf` preview exactly as they are; every chunk-01 Pester test must still pass. Add:

- `-Platform Win64|Android|Linux`, default `Win64`. The doctor profile follows the platform, as chunk 01 already wires it: `Win64` selects `workstation`, `Android` selects `android`, `Linux` selects `linux`;
- `-Target editor|game`, default `game`. The editor target builds for `Win64` only; asking for an editor build on another platform is an error;
- `-Configuration Development|Shipping`, default `Development`;
- `-Isolate`, which clears `ANDROID_HOME`, `ANDROID_SDK_ROOT`, `NDKROOT`, `NDK_ROOT`, `JAVA_HOME` and `LINUX_MULTIARCH_ROOT` for the duration of the build by pointing them at empty directories it creates under the system temp directory, then restores them. This is what PLAN.md 4.1's gate calls for.

## Constraints

- Do not run `git commit`, `git push`, `git add`, or change git configuration. Claude commits.
- Do not modify PLAN.md, PLAN-REVIEW-LOG.md, anything under `docs/` except `docs/reports/`, `third_party/`, `packages/dashboard-spec/`, `tools/`, or `tests/fixtures/`. You may modify `packages/signal-core/CMakeLists.txt` (to add the subset tool) and add `packages/signal-core/tools/smoke-subset.cpp`. You may modify `runtime/UnRealDash/Source/SignalCore/` only to add `Public/SignalCore/SmokeSubset.h` and to fix a genuine compile error in `SignalCore.Build.cs` or `SignalCoreModule.cpp`, which must be reported with the error.
- Nothing may require elevation.
- Plain factual language in every file and every string a user can see. No em dashes anywhere. No marketing words. Do not name any commercial dashboard product.
- Follow docs/ARCHITECTURE.md: dependencies point downward, signal-core stays free of Unreal headers outside its module-registration file, errors are values, no global mutable state.
- Environment: UE 5.8.2 at `C:\Program Files\Epic Games\UE_5.8`, VS 2022 Community 17.14 with MSVC 14.44, Windows SDK 10.0.26100, Android SDK android-36 with NDK 27.2.12479018 and JDK 25 from Android Studio, Linux cross toolchain v26 at `C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8`, pwsh 7.6, Python 3.14, cmake 4.4.3, ninja 1.13. The doctor passes `workstation`, `android` and `linux`. Environment variables set at user scope: `ANDROID_HOME`, `ANDROID_SDK_ROOT`, `JAVA_HOME`, `NDKROOT`, `NDK_ROOT`; `LINUX_MULTIARCH_ROOT` at machine scope. A shell started before those were set will not see them; read them from the environment with `[Environment]::GetEnvironmentVariable(name, scope)` if a build cannot find a toolchain.
- This chunk owns `runtime/UnRealDash/` (except `Source/SignalCore/`, restricted above), `scripts/build.ps1`, `packages/signal-core/tools/smoke-subset.cpp` and `packages/signal-core/CMakeLists.txt`. It touches nothing else.

## Non-goals

Task 4.0's smoke scene and its device runs, 4.0b, 4.3's acquisition threading, 4.4's package loader, any widget, any dashboard document, any packaged APK, dashboard-spec's Unreal module, and the CI workflow changes that would run these builds on hosted runners (no hosted runner has an engine).

## Proof (run all of these and paste full output)

From the repository root in `pwsh`. If a build cannot find a toolchain, read the variables from the user and machine scopes as described above and say in the report that you did.

```
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; "exit=$LASTEXITCODE"

# 4.1: editor and game binaries, and the toolchain-isolation build
pwsh -NoProfile -File scripts/build.ps1 -Target editor -Configuration Development
pwsh -NoProfile -File scripts/build.ps1 -Target game -Configuration Development
pwsh -NoProfile -File scripts/build.ps1 -Target game -Configuration Development -Isolate

# 4.2 Win64 half: engine independence, the standalone subset, and the in-engine subset
Get-ChildItem -Recurse runtime/UnRealDash/Source/SignalCore -Include *.h,*.cpp |
  Where-Object { $_.Name -ne 'SignalCoreModule.cpp' } |
  Select-String -Pattern '^\s*#\s*include\s*[<"](Core|CoreMinimal|CoreTypes|Engine|HAL|Modules|UObject|Containers|Templates|Misc|Math|Logging|GenericPlatform|Windows)'
$build = if (Test-Path packages/signal-core/build/default) { 'packages/signal-core/build/default' } else { 'packages/signal-core/build/vs' }
cmake --build $build --config RelWithDebInfo
# Ninja puts the executable in the build root; the Visual Studio generator puts it under <config>/.
$subset = Get-ChildItem -Recurse $build -Filter signal-core-smoke-subset.exe | Select-Object -First 1
& $subset.FullName
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\runtime\UnRealDash\UnRealDash.uproject" -run=SignalCoreSmoke -unattended -nopause -nosplash

# 4.2 Android and Linux halves
pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Android -Configuration Development
pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Linux -Configuration Development

# chunk 01 regression
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI"
```

Expected: the doctor exits 0; all three Win64 builds succeed and the isolation build succeeds with every Android and Linux variable pointing at an empty directory; the include scan prints nothing; the standalone subset and the commandlet print the same assertion count with zero failures; the Android and Linux builds succeed; Pester reports zero failures. The proof locates the standalone subset executable by search, because the Ninja preset and the Visual Studio generator put it in different places; say which build directory and generator you used.

## Report format

End with: files added or changed, one line each giving path, what it is and the PLAN.md task it serves; the proof output verbatim; the two assertion counts side by side; any change forced on chunk 02's files with the compiler error that forced it; every place this spec or PLAN.md was ambiguous and what you chose; any deviation with its reason; and anything you could not do.
