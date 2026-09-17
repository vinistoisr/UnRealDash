# Chunk 06 Unreal project skeleton report

The requested source implementation is present. Chunk 06 is not proven complete in this sandbox. No Unreal editor or game binary was produced. PLAN.md task 4.1 and all three target gates of 4.2 are not met: UBT fails during startup before compiling the project. The engine commandlet did not reach its checks. The standalone subset passes, the include scan is empty, and standalone regressions pass.

## Files added or changed

- `runtime/UnRealDash/UnRealDash.uproject`: Added UE 5.8 project and three runtime module declarations. PLAN.md 4.1.
- `runtime/UnRealDash/Source/UnRealDash.Target.cs`: Added C++20 game target with latest build and include settings. PLAN.md 4.1.
- `runtime/UnRealDash/Source/UnRealDashEditor.Target.cs`: Added C++20 editor target with latest build and include settings. PLAN.md 4.1.
- `runtime/UnRealDash/Source/UnRealDash/UnRealDash.Build.cs`: Added primary module dependencies and disabled unity builds. PLAN.md 4.1.
- `runtime/UnRealDash/Source/UnRealDash/UnRealDashModule.cpp`: Added primary game module registration. PLAN.md 4.1.
- `runtime/UnRealDash/Source/UnRealDash/Public/UnRealDashLog.h`: Added logging category declaration. PLAN.md 4.1.
- `runtime/UnRealDash/Source/UnRealDash/Private/UnRealDashLog.cpp`: Added logging category definition. PLAN.md 4.1.
- `runtime/UnRealDash/Source/UnRealDashCore/UnRealDashCore.Build.cs`: Added downward module dependencies and disabled unity builds. PLAN.md 4.1, 4.2.
- `runtime/UnRealDash/Source/UnRealDashCore/UnRealDashCoreModule.cpp`: Added core module registration. PLAN.md 4.1, 4.2.
- `runtime/UnRealDash/Source/UnRealDashCore/Public/UnRealDashCore/SignalCoreAdapter.h`: Added sample adapter struct and quality and age string declarations. PLAN.md 4.2.
- `runtime/UnRealDash/Source/UnRealDashCore/Private/SignalCoreAdapter.cpp`: Added sample conversion and FString labels. PLAN.md 4.2.
- `runtime/UnRealDash/Source/UnRealDashCore/Private/SignalCoreSmokeCommandlet.h`: Added SignalCoreSmoke UCommandlet declaration. PLAN.md 4.2.
- `runtime/UnRealDash/Source/UnRealDashCore/Private/SignalCoreSmokeCommandlet.cpp`: Added shared-subset runner, assertion counting and adapter logging. PLAN.md 4.2.
- `runtime/UnRealDash/Source/SignalCore/Public/SignalCore/SmokeSubset.h`: Added engine-independent shared freshness, temperature and hysteresis checks. PLAN.md 4.2.
- `runtime/UnRealDash/Config/DefaultEngine.ini`: Added specified renderer, Android, Linux and log settings. PLAN.md 4.1.
- `runtime/UnRealDash/Config/DefaultGame.ini`: Added project identity and Development packaging settings. PLAN.md 4.1.
- `runtime/UnRealDash/Config/DefaultInput.ini`: Added input configuration with no bindings. PLAN.md 4.1.
- `scripts/build.ps1`: Extended real Build.bat execution, target selection and temporary toolchain isolation; retained legacy preview arguments. PLAN.md 4.1, 4.2.
- `packages/signal-core/tools/smoke-subset.cpp`: Added standalone shared-subset executable runner. PLAN.md 4.2.
- `packages/signal-core/CMakeLists.txt`: Added subset executable and CTest registration. PLAN.md 4.2.
- `docs/reports/chunk-06-unreal-project.md`: Added this implementation and proof report. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/01-doctor.txt`: Captured output for `pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; "exit=$LASTEXITCODE"`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/02-doctor-vs-tools.txt`: Captured output for `pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; "exit=$LASTEXITCODE"`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/03-editor.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target editor -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/04-game.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target game -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/05-isolation.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target game -Configuration Development -Isolate`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/06-include-scan.txt`: Captured output for `Get-ChildItem -Recurse runtime/UnRealDash/Source/SignalCore -Include *.h,*.cpp |`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/07-cmake.txt`: Captured output for `$build = if (Test-Path packages/signal-core/build/default) { 'packages/signal-core/build/default' } else { 'packages/signal-core/build/vs' }`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/08-cmake-devshell.txt`: Captured output for `cmake --build $build --config RelWithDebInfo`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/09-subset.txt`: Captured output for `$subset = Get-ChildItem -Recurse $build -Filter signal-core-smoke-subset.exe | Select-Object -First 1`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/10-cmake-msvc-env.txt`: Captured output for `cmake --build $build --config RelWithDebInfo`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/11-subset.txt`: Captured output for `$subset = Get-ChildItem -Recurse $build -Filter signal-core-smoke-subset.exe | Select-Object -First 1`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/12-commandlet.txt`: Captured output for `& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ``. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/13-android.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Android -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/14-linux.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Linux -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/15-pester.txt`: Captured output for `pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI"`. PLAN.md 1.6 regression for 4.1.
- `docs/reports/chunk-06-proof/16-ubt-startup.txt`: Captured output for `& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe' 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll' -help`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/17-editor-no-crash-dialog.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target editor -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/18-game-no-crash-dialog.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target game -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/19-environment-scopes.txt`: Captured output for `foreach ($name in @('ANDROID_HOME','ANDROID_SDK_ROOT','JAVA_HOME','NDKROOT','NDK_ROOT','LINUX_MULTIARCH_ROOT')) {`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/20-android-scoped-env.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Android -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/21-linux-scoped-env.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Linux -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/22-android-installed-paths.txt`: Captured output for `pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Android -Configuration Development`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/23-pester-no-registry.txt`: Captured output for `pwsh -NoProfile -Command '$configuration = New-PesterConfiguration; $configuration.Run.Path = "scripts/tests"; $configuration.Run.Exit = $true; $configuration.TestRegistry.Enabled = $false; Invoke-Pester -Configuration $configuration' `. PLAN.md 1.6 regression for 4.1.
- `docs/reports/chunk-06-proof/24-ctest.txt`: Captured output for `ctest --test-dir packages/signal-core/build/default --output-on-failure`. PLAN.md 4.1, 4.2.
- `docs/reports/chunk-06-proof/25-doctest.txt`: Captured output for `& packages/signal-core/build/default/signal-core-tests.exe`. PLAN.md 4.1, 4.2.

Generated, ignored build products are under `packages/signal-core/build/default/` and Unreal's `Intermediate/` and `Saved/` directories. They are not source deliverables. No prohibited source paths, Git configuration, staging area or commits were changed. `git status --short` was rejected by Git's ownership check for the sandbox account; no safe-directory configuration was added.

## Proof output verbatim

Every requested proof command was attempted. Output files capture combined standard output and standard error; the harness appends `exit=<code>` where shown. These markers are harness output, not the tested programs' own output. Failed attempts and successful retries are both retained. An empty include-scan block means the command produced no output.

The standalone build directory was `packages/signal-core/build/default`, generator `Ninja`, cached build type `RelWithDebInfo`, compiler MSVC 14.44.35207 from VS 2022 Build Tools. The preset's cached Ninja executable is the Build Tools copy. The subset executable is in the build root.

The original WinGet CMake 4.4.3 and Ninja paths could not be resolved under this sandbox account. Proof retries prepended these installed Visual Studio directories to process PATH:

```text
C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin
C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja
```

Those report CMake 3.31.6-msvc6 and Ninja 1.12.1. The unchanged doctor accepts both. No tool was installed and no network was used.

The standalone build required the ordinary MSVC environment. Developer PowerShell initialization failed in this shell, so the successful retry set process variables directly from the installed toolchain:

```text
INCLUDE=C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include;C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt;C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared;C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um
LIB=C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/lib/x64;C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64;C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64
PATH additions=C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64;C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64
```

I read ANDROID_HOME, ANDROID_SDK_ROOT, JAVA_HOME, NDKROOT and NDK_ROOT from user scope and LINUX_MULTIARCH_ROOT from machine scope, as requested. All five user-scope values were empty for CodexSandboxOffline. The machine value was present. The final Android attempt used the paths found in the installed tools and doctor output:

```text
JAVA_HOME=C:/Program Files/Android/Android Studio/jbr
ANDROID_HOME=C:/Users/Vincent/AppData/Local/Android/Sdk
ANDROID_SDK_ROOT=C:/Users/Vincent/AppData/Local/Android/Sdk
NDKROOT=C:/Users/Vincent/AppData/Local/Android/Sdk/ndk/27.2.12479018
NDK_ROOT=C:/Users/Vincent/AppData/Local/Android/Sdk/ndk/27.2.12479018
```

For the UBT diagnostic and later build attempts, the parent PowerShell process called the Windows `SetErrorMode(2)` API to suppress native crash dialogs. This allowed startup failures to return an exit code instead of leaving crashed processes alive. It changes no filesystem or registry permissions. The original editor attempt was left running for several minutes and had no compiler output; only after a separate UBT startup diagnostic reproduced the access-denied exception were the initial failed UBT processes stopped. No compiler process was stopped.

### 01-doctor.txt

Initial environment; WinGet links were inaccessible.

```powershell
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; "exit=$LASTEXITCODE"
```

```text
Doctor profile: workstation

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.76 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 Exception calling "ResolveLinkTarget" with "1" argument(s): "Access to the     FAIL
                                                                                 path 'C:\Users\Vincent\AppData\Local\Microsoft\WinGet\Links\cmake.exe' is
                                                                                 denied."
ninja                  available                                                 Exception calling "ResolveLinkTarget" with "1" argument(s): "Access to the     FAIL
                                                                                 path 'C:\Users\Vincent\AppData\Local\Microsoft\WinGet\Links\ninja.exe' is
                                                                                 denied."
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS


exit=1
```

### 02-doctor-vs-tools.txt

After prepending the installed Visual Studio CMake and Ninja directories to process PATH.

```powershell
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; "exit=$LASTEXITCODE"
```

```text
Doctor profile: workstation

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.74 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS


exit=0
```

### 03-editor.txt

Initial editor attempt. The UBT process was terminated after the separate startup diagnostic established an access-denied exception before compilation. Its exit=-1 is termination, not a compiler result.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target editor -Configuration Development
```

```text
Doctor profile: workstation
Doctor profile: workstation

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.74 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS


& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnRealDashEditor Win64 Development -project="C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject" -waitmutex
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDashEditor Win64 Development -project=C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject -waitmutex
exit=-1
```

### 04-game.txt

Initial game attempt. The UBT process was terminated after the same startup symptom. Its exit=-1 is termination, not a compiler result.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target game -Configuration Development
```

```text
Doctor profile: workstation
Doctor profile: workstation

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.74 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS


& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnRealDash Win64 Development -project="C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject" -waitmutex
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDash Win64 Development -project=C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject -waitmutex
exit=-1
```

### 05-isolation.txt

Crash dialogs disabled in the parent process. All six isolation directories were created empty; the doctor passed, but UBT startup failed.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target game -Configuration Development -Isolate
```

```text
Isolation: ANDROID_HOME=C:\Users\Vincent\AppData\Local\Temp\UnRealDash-isolate-325fb94626af419f8ce9630ac370f4f5\ANDROID_HOME (empty)
Isolation: ANDROID_SDK_ROOT=C:\Users\Vincent\AppData\Local\Temp\UnRealDash-isolate-325fb94626af419f8ce9630ac370f4f5\ANDROID_SDK_ROOT (empty)
Isolation: NDKROOT=C:\Users\Vincent\AppData\Local\Temp\UnRealDash-isolate-325fb94626af419f8ce9630ac370f4f5\NDKROOT (empty)
Isolation: NDK_ROOT=C:\Users\Vincent\AppData\Local\Temp\UnRealDash-isolate-325fb94626af419f8ce9630ac370f4f5\NDK_ROOT (empty)
Isolation: JAVA_HOME=C:\Users\Vincent\AppData\Local\Temp\UnRealDash-isolate-325fb94626af419f8ce9630ac370f4f5\JAVA_HOME (empty)
Isolation: LINUX_MULTIARCH_ROOT=C:\Users\Vincent\AppData\Local\Temp\UnRealDash-isolate-325fb94626af419f8ce9630ac370f4f5\LINUX_MULTIARCH_ROOT (empty)
Doctor profile: workstation
Doctor profile: workstation

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.74 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS


& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnRealDash Win64 Development -project="C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject" -waitmutex
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDash Win64 Development -project=C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject -waitmutex
exit=-532462766
```

### 06-include-scan.txt

The output is empty: no matches.

```powershell
Get-ChildItem -Recurse runtime/UnRealDash/Source/SignalCore -Include *.h,*.cpp |
  Where-Object { $_.Name -ne 'SignalCoreModule.cpp' } |
  Select-String -Pattern '^\s*#\s*include\s*[<"](Core|CoreMinimal|CoreTypes|Engine|HAL|Modules|UObject|Containers|Templates|Misc|Math|Logging|GenericPlatform|Windows)'
```

```text
```

### 07-cmake.txt

Initial build without an initialized MSVC include/library environment.

```powershell
$build = if (Test-Path packages/signal-core/build/default) { 'packages/signal-core/build/default' } else { 'packages/signal-core/build/vs' }
cmake --build $build --config RelWithDebInfo
```

```text
[0/2] Re-checking globbed directories...
-- GLOB mismatch!
[1/2] Re-running CMake...
-- Compiler: MSVC 19.44.35223.0
-- signal_core: C++20, exceptions OFF, RTTI OFF, warnings as errors
-- Configuring done (0.2s)
-- Generating done (0.1s)
-- Build files have been written to: C:/Users/Vincent/UnRealDash/packages/signal-core/build/default
[0/4] Re-checking globbed directories...
[1/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_rule_state.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_rule_state.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_rule_state.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_rule_state.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[2/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_units.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_units.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_units.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_units.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[3/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Scenarios.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Scenarios.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Scenarios.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Scenarios.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[4/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SnapshotExchange.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/SnapshotExchange.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SnapshotExchange.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SnapshotExchange.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[5/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_threading_stress.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_threading_stress.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_threading_stress.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_threading_stress.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[6/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_rule_engine.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_rule_engine.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_rule_engine.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_rule_engine.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[7/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\DefinitionPack.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/DefinitionPack.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\DefinitionPack.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\DefinitionPack.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[8/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Recording.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Recording.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Recording.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Recording.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[9/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_recording.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_recording.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_recording.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_recording.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[10/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_definition_pack.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_definition_pack.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_definition_pack.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_definition_pack.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[11/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_interpolator.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_interpolator.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_interpolator.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_interpolator.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[12/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\BinaryTelemetryV1.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/BinaryTelemetryV1.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\BinaryTelemetryV1.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\BinaryTelemetryV1.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[13/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_snapshot_exchange.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_snapshot_exchange.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_snapshot_exchange.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_snapshot_exchange.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[14/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_binary_telemetry_v1.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_binary_telemetry_v1.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_binary_telemetry_v1.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_binary_telemetry_v1.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[15/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_registry.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_registry.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_registry.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_registry.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[16/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\JsonLinesText.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/JsonLinesText.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\JsonLinesText.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\JsonLinesText.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[17/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_scenarios.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_scenarios.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_scenarios.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_scenarios.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[18/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Interpolator.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Interpolator.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Interpolator.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Interpolator.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[19/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Units.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Units.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Units.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Units.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[20/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_sample.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_sample.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_sample.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_sample.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[21/36] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_heartbeat_and_held.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_heartbeat_and_held.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_heartbeat_and_held.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_heartbeat_and_held.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[22/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SignalRegistry.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/SignalRegistry.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SignalRegistry.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SignalRegistry.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[23/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Errors.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Errors.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Errors.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Errors.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[24/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\RuleEngine.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/RuleEngine.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\RuleEngine.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\RuleEngine.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[25/36] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\ExpirySchedule.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/ExpirySchedule.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\ExpirySchedule.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\ExpirySchedule.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[26/36] Building CXX object CMakeFiles\signal-core-scenario.dir\tools\scenario-dump.cpp.obj
FAILED: CMakeFiles/signal-core-scenario.dir/tools/scenario-dump.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-scenario.dir\tools\scenario-dump.cpp.obj /FdCMakeFiles\signal-core-scenario.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tools\scenario-dump.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
ninja: build stopped: subcommand failed.
exit=1
```

### 08-cmake-devshell.txt

The attempted Visual Studio Developer PowerShell initialization did not populate the required environment. Its startup also printed: vswhere.exe was not recognized; Microsoft was unexpected at this time. This build still lacked cstdint.

```powershell
cmake --build $build --config RelWithDebInfo
```

```text
[0/2] Re-checking globbed directories...
[1/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SnapshotExchange.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/SnapshotExchange.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SnapshotExchange.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SnapshotExchange.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[2/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_registry.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_registry.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_registry.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_registry.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[3/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Units.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Units.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Units.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Units.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[4/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Errors.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Errors.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Errors.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Errors.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[5/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_interpolator.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_interpolator.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_interpolator.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_interpolator.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[6/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_units.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_units.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_units.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_units.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[7/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\RuleEngine.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/RuleEngine.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\RuleEngine.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\RuleEngine.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[8/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_heartbeat_and_held.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_heartbeat_and_held.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_heartbeat_and_held.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_heartbeat_and_held.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[9/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\BinaryTelemetryV1.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/BinaryTelemetryV1.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\BinaryTelemetryV1.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\BinaryTelemetryV1.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[10/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Recording.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Recording.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Recording.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Recording.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[11/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_recording.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_recording.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_recording.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_recording.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[12/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\ExpirySchedule.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/ExpirySchedule.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\ExpirySchedule.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\ExpirySchedule.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[13/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_definition_pack.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_definition_pack.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_definition_pack.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_definition_pack.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[14/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Interpolator.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Interpolator.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Interpolator.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Interpolator.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[15/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Scenarios.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/Scenarios.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Scenarios.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Scenarios.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[16/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_binary_telemetry_v1.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_binary_telemetry_v1.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_binary_telemetry_v1.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_binary_telemetry_v1.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[17/34] Building CXX object CMakeFiles\signal-core-scenario.dir\tools\scenario-dump.cpp.obj
FAILED: CMakeFiles/signal-core-scenario.dir/tools/scenario-dump.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-scenario.dir\tools\scenario-dump.cpp.obj /FdCMakeFiles\signal-core-scenario.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tools\scenario-dump.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[18/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_sample.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_sample.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_sample.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_sample.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[19/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_snapshot_exchange.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_snapshot_exchange.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_snapshot_exchange.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_snapshot_exchange.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[20/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_rule_state.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_rule_state.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_rule_state.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_rule_state.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[21/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_scenarios.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_scenarios.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_scenarios.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_scenarios.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[22/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\DefinitionPack.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/DefinitionPack.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\DefinitionPack.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\DefinitionPack.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[23/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\JsonLinesText.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/JsonLinesText.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\JsonLinesText.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\JsonLinesText.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[24/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_rule_engine.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_rule_engine.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_rule_engine.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_rule_engine.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[25/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SignalRegistry.cpp.obj
FAILED: CMakeFiles/signal_core.dir/C_/Users/Vincent/UnRealDash/runtime/UnRealDash/Source/SignalCore/Private/SignalRegistry.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Private -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SignalRegistry.cpp.obj /FdCMakeFiles\signal_core.dir\signal_core.pdb /FS -c C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SignalRegistry.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
[26/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_threading_stress.cpp.obj
FAILED: CMakeFiles/signal-core-tests.dir/tests/test_threading_stress.cpp.obj 
C:\PROGRA~2\MICROS~4\2022\BUILDT~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\cl.exe  /nologo /TP -DDOCTEST_CONFIG_NO_EXCEPTIONS -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS -D_CRT_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=0 -IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\runtime\UnRealDash\Source\SignalCore\Public -external:IC:\Users\Vincent\UnRealDash\packages\signal-core\..\..\third_party\doctest -external:W0 -std:c++20 -MD -Zi /EHs-c- /GR- /W4 /WX /permissive- /showIncludes /FoCMakeFiles\signal-core-tests.dir\tests\test_threading_stress.cpp.obj /FdCMakeFiles\signal-core-tests.dir\ /FS -c C:\Users\Vincent\UnRealDash\packages\signal-core\tests\test_threading_stress.cpp
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Public\SignalCore/Errors.h(3): fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
ninja: build stopped: subcommand failed.
exit=1
```

### 09-subset.txt

No subset executable existed after that failed build. PowerShell rejected the empty invocation: The expression after & in a pipeline element produced an object that was not valid. This file contains only the appended exit marker, not executable output.

```powershell
$subset = Get-ChildItem -Recurse $build -Filter signal-core-smoke-subset.exe | Select-Object -First 1
& $subset.FullName
```

```text
exit=1
```

### 10-cmake-msvc-env.txt

Successful retry with MSVC and Windows SDK INCLUDE, LIB and PATH set in the process, as detailed below.

```powershell
cmake --build $build --config RelWithDebInfo
```

```text
[0/2] Re-checking globbed directories...
[1/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Errors.cpp.obj
[2/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\DefinitionPack.cpp.obj
[3/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Units.cpp.obj
[4/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_definition_pack.cpp.obj
[5/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Recording.cpp.obj
[6/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SnapshotExchange.cpp.obj
[7/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Interpolator.cpp.obj
[8/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\JsonLinesText.cpp.obj
[9/34] Building CXX object CMakeFiles\signal-core-scenario.dir\tools\scenario-dump.cpp.obj
[10/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_sample.cpp.obj
[11/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\SignalRegistry.cpp.obj
[12/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\BinaryTelemetryV1.cpp.obj
[13/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_scenarios.cpp.obj
[14/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_rule_engine.cpp.obj
[15/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_units.cpp.obj
[16/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\ExpirySchedule.cpp.obj
[17/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\Scenarios.cpp.obj
[18/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_registry.cpp.obj
[19/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_rule_state.cpp.obj
[20/34] Building CXX object CMakeFiles\signal_core.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\SignalCore\Private\RuleEngine.cpp.obj
[21/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_interpolator.cpp.obj
[22/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_snapshot_exchange.cpp.obj
[23/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_heartbeat_and_held.cpp.obj
[24/34] Linking CXX static library signal_core.lib
[25/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_threading_stress.cpp.obj
[26/34] Linking CXX executable signal-core-scenario.exe
[27/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_binary_telemetry_v1.cpp.obj
[28/34] Building CXX object CMakeFiles\signal-core-tests.dir\tests\test_recording.cpp.obj
[29/34] Building CXX object CMakeFiles\signal-core-smoke-subset.dir\tools\smoke-subset.cpp.obj
[30/34] Linking CXX executable signal-core-smoke-subset.exe
[31/34] Linking CXX executable signal-core-tests.exe
[32/34] Building CXX object CMakeFiles\signal-core-telemetry-fuzz.dir\tools\telemetry-fuzz.cpp.obj
[33/34] Linking CXX executable signal-core-telemetry-fuzz.exe
exit=0
```

### 11-subset.txt

Successful standalone subset run.

```powershell
$subset = Get-ChildItem -Recurse $build -Filter signal-core-smoke-subset.exe | Select-Object -First 1
& $subset.FullName
```

```text
SignalCoreSmoke assertions=83 failures=0
exit=0
```

### 12-commandlet.txt

The requested command ran, but engine startup failed on the derived-data cache before the subset could execute.

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\runtime\UnRealDash\UnRealDash.uproject" -run=SignalCoreSmoke -unattended -nopause -nosplash
```

```text
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" -Mode=ValidatePlatforms -OutputSDKs  -AllPlatforms  -project="C:/Users/Vincent/UnRealDash/runtime/UnRealDash/UnRealDash.uproject" -log="C:/Users/Vincent/UnRealDash/runtime/UnRealDash/Saved/Logs/AutoSDKInfo.txt" -verbose -timestamps
[2026.09.17-05.04.25:137][  0]LogInit: Display: Running engine for game: UnRealDash
[2026.09.17-05.04.25:137][  0]LogCore: Display: Requested channels: 'cpu,gpu,frame,log,bookmark,screenshot,region'
[2026.09.17-05.04.25:137][  0]LogTrace: Display: Control listening on port 1985
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : platform="Windows"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : config="Development"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : buildversion="++UE5+Release-5.8-CL-56702186"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : engineversion="5.8.2-56702186+++UE5+Release-5.8"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : enginereleaseversion="5.8.2"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : os="Windows 11 (25H2) [10.0.26220.9472] "
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : cpu="GenuineIntel|13th Gen Intel(R) Core(TM) i7-13700"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : pgoenabled="0"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : pgoprofilingenabled="0"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : ltoenabled="0"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : asan="0"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : mergedmodules="0"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : compacttset="0"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : commandline="" C:\Users\Vincent\UnRealDash/runtime/UnRealDash/UnRealDash.uproject -run=SignalCoreSmoke -unattended -nopause -nosplash""
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : loginid="f0ba5f724863a69fcee6fc87e717ef17"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : llm="0"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : zenstreaming="0"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : systemresolution.resx="1280"
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : systemresolution.resy="720"
[2026.09.17-05.04.25:137][  0]LogAudio: Display: Audio Device Manager not initializing due to all audio being disabled. If this is not intentional, please check command line arguments for "-nosound".
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading Android ini files took 0.05 seconds
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading IOS ini files took 0.05 seconds
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading VulkanPC ini files took 0.05 seconds
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading Mac ini files took 0.06 seconds
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading VisionOS ini files took 0.06 seconds
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading TVOS ini files took 0.07 seconds
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading Unix ini files took 0.07 seconds
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading Windows ini files took 0.07 seconds
[2026.09.17-05.04.25:137][  0]LogConfig: Display: Loading Linux ini files took 0.07 seconds
[2026.09.17-05.04.25:137][  0]LogAsyncCompilation: Display: MemorySharingInfo: UsedMemory=335MiB, MaxMemoryAvailable=74828MiB
[2026.09.17-05.04.25:137][  0]RenderDocPlugin: Display: RenderDoc plugin will not be loaded. Use '-AttachRenderDoc' on the cmd line or enable 'renderdoc.AutoAttach' in the plugin settings.
[2026.09.17-05.04.25:137][  0]LogAnalytics: Display: Provider StudioTelemetry.Provider.Log is disabled for this application
[2026.09.17-05.04.25:137][  0]LogAnalytics: Display: Provider StudioTelemetry.Provider.CSV is disabled for this application
[2026.09.17-05.04.25:137][  0]LogAnalytics: Display: Provider StudioTelemetry.Provider.Horde is disabled for this application
[2026.09.17-05.04.25:137][  0]LogAnalytics: Display: Provider StudioTelemetry.Provider.Log is disabled for this application
[2026.09.17-05.04.25:137][  0]LogAnalytics: Display: Provider StudioTelemetry.Provider.CSV is disabled for this application
[2026.09.17-05.04.25:137][  0]LogAnalytics: Display: Provider StudioTelemetry.Provider.Horde is disabled for this application
[2026.09.17-05.04.25:137][  0]LogSsl: Warning: Unable to open ROOT certificate store. Platform provided certificates will not be used
[2026.09.17-05.04.25:137][  0]LogStreaming: Display: AsyncLoading2 - Created: Async Loading Thread: false, Async Post Load: false, Multithreaded: false
[2026.09.17-05.04.25:137][  0]LogHAL: Display: Platform has ~ 64 GB [68263014400 / 68719476736 / 64], which maps to Largest [LargestMinGB=32, LargerMinGB=12, DefaultMinGB=8, SmallerMinGB=6, SmallestMinGB=0)
[2026.09.17-05.04.25:137][  0]LogCsvProfiler: Display: Metadata set : extradevelopmentmemorymb="0"
[2026.09.17-05.04.25:137][  0]LogObj: Display: Attempting to load config data for Default__DownlinkBandwidthManagerConfig before the Class has been constructed/registered/linked (likely during module loading or early startup). This will result in the load silently failing and should be fixed.
[2026.09.17-05.04.25:412][  0]LogObj: Display: Attempting to load config data for Default__SlateThemeManager before the Class has been constructed/registered/linked (likely during module loading or early startup). This will result in the load silently failing and should be fixed.
[2026.09.17-05.04.25:533][  0]LogCsvProfiler: Display: Metadata set : verbatimrhiname="Null"
[2026.09.17-05.04.25:533][  0]LogCsvProfiler: Display: Metadata set : rhiname="Null"
[2026.09.17-05.04.25:533][  0]LogCsvProfiler: Display: Metadata set : rhifeaturelevel="SM6"
[2026.09.17-05.04.25:533][  0]LogCsvProfiler: Display: Metadata set : shaderplatform="PCD3D_SM6"
[2026.09.17-05.04.25:533][  0]LogCsvProfiler: Display: Metadata set : gpu=""
[2026.09.17-05.04.25:533][  0]LogCsvProfiler: Display: Metadata set : gpudriver=""
[2026.09.17-05.04.25:538][  0]LogTextureFormatASTC: Display: ASTCEnc version 5.0.1 library loaded
[2026.09.17-05.04.25:538][  0]LogTextureFormatManager: Display: Loaded Base TextureFormat: TextureFormatASTC
[2026.09.17-05.04.25:538][  0]LogTextureFormatManager: Display: Loaded Base TextureFormat: TextureFormatDXT
[2026.09.17-05.04.25:538][  0]LogTextureFormatManager: Display: Loaded Base TextureFormat: TextureFormatETC2
[2026.09.17-05.04.25:538][  0]LogTextureFormatManager: Display: Loaded Base TextureFormat: TextureFormatIntelISPCTexComp
[2026.09.17-05.04.25:538][  0]LogTextureFormatOodle: Display: Oodle Texture TFO init; latest sdk version = 2.9.16
[2026.09.17-05.04.25:538][  0]LogTextureFormatOodle: Display: Oodle Texture loading DLL: oo2tex_win64_2.9.16.dll
[2026.09.17-05.04.25:539][  0]LogTextureFormatManager: Display: Loaded Base TextureFormat: TextureFormatOodle
[2026.09.17-05.04.25:539][  0]LogTextureFormatManager: Display: Loaded Base TextureFormat: TextureFormatUncompressed
[2026.09.17-05.04.25:575][  0]LogTargetPlatformManager: Display: Failed to SetupSDK for platform 'Android'
[2026.09.17-05.04.25:600][  0]LogTargetPlatformManager: Display: Failed to SetupSDK for platform 'IOS'
[2026.09.17-05.04.25:624][  0]LogTargetPlatformManager: Display: Failed to SetupSDK for platform 'Linux'
[2026.09.17-05.04.25:649][  0]LogTargetPlatformManager: Display: Failed to SetupSDK for platform 'Mac'
[2026.09.17-05.04.25:650][  0]LogTargetPlatformManager: Display: Failed to SetupSDK for platform 'TVOS'
[2026.09.17-05.04.25:675][  0]LogTargetPlatformManager: Display: Loaded TargetPlatform 'Windows'
[2026.09.17-05.04.25:675][  0]LogTargetPlatformManager: Display: Loaded TargetPlatform 'WindowsEditor'
[2026.09.17-05.04.25:675][  0]LogTargetPlatformManager: Display: Loaded TargetPlatform 'WindowsServer'
[2026.09.17-05.04.25:675][  0]LogTargetPlatformManager: Display: Loaded TargetPlatform 'WindowsClient'
[2026.09.17-05.04.25:675][  0]LogTargetPlatformManager: Display: Building Assets For WindowsEditor
[2026.09.17-05.04.25:678][  0]LogAudioDebug: Display: Lib vorbis DLL was dynamically loaded.
[2026.09.17-05.04.25:916][  0]LogDerivedDataCache: Display: ../../../Engine/DerivedDataCache/Compressed.ddp: Opened pak cache for reading. (1679 MiB)
[2026.09.17-05.04.25:916][  0]LogZenServiceInstance: Warning: Skipping Zen config default=C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Data due to an invalid path
[2026.09.17-05.04.25:916][  0]LogZenServiceInstance: Warning: Unable to determine a valid Zen data path
[2026.09.17-05.04.25:916][  0]LogZenServiceInstance: Warning: Unreal Zen Storage Server is unable to determine a valid data path
[2026.09.17-05.04.25:917][  0]LogZenServiceInstance: Display: Launching zen utility 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/zen.exe service status'.
[2026.09.17-05.04.26:022][  0]LogZenServiceInstance: Display: Removing installed plugin 'AndroidPortForwarder2' as it is no longer referenced by any workspace
[2026.09.17-05.04.26:022][  0]LogZenServiceInstance: Display: Writing plugin version info to 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zen_plugin_versions.json'
[2026.09.17-05.04.26:023][  0]LogZenServiceInstance: Display: Read zen version cache file from 'C:/Users/Vincent/AppData/Local/UnrealEngine/5.8/Saved/Zen/zen.version', version: '5.8.13-202605190912-windows-x64-release-094f62c2'
[2026.09.17-05.04.26:023][  0]LogZenServiceInstance: Display: Read zen version cache file from 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zen.version', version: '5.8.13-202605190912-windows-x64-release-094f62c2'
[2026.09.17-05.04.26:023][  0]LogZenServiceInstance: Display: Installed service at 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe' is up to date
[2026.09.17-05.04.26:034][  0]LogZenServiceInstance: Display: Writing plugin configuration to 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zen_plugins_v1.json'
[2026.09.17-05.04.26:034][  0]LogZenServiceInstance: Display: Lock file '.lock' is not active, nothing to do
[2026.09.17-05.04.26:034][  0]LogZenServiceInstance: Display: Requesting shut down of zenserver process 42104 runnning on effective port 8558
[2026.09.17-05.04.26:280][  0]LogZenServiceInstance: Display: Successfully shut down zenserver process with pid 42104
[2026.09.17-05.04.26:280][  0]LogZenServiceInstance: Display: Launching executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe', working dir 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install', data dir '', args '--port 8558 --data-dir  --no-sentry --http-forceloopback  --child-id Zen_8116_Startup'
[2026.09.17-05.04.31:283][  0]LogZenServiceInstance: Warning: Failed launch service using executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe' on port 8558
[2026.09.17-05.04.31:784][  0]LogZenServiceInstance: Display: Lock file '.lock' is not active, nothing to do
[2026.09.17-05.04.31:784][  0]LogZenServiceInstance: Display: Launching executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe', working dir 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install', data dir '', args '--port 8558 --data-dir  --no-sentry --http-forceloopback  --child-id Zen_8116_Startup'
[2026.09.17-05.04.36:801][  0]LogZenServiceInstance: Warning: Failed launch service using executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe' on port 8558
[2026.09.17-05.04.37:302][  0]LogZenServiceInstance: Display: Lock file '.lock' is not active, nothing to do
[2026.09.17-05.04.37:302][  0]LogZenServiceInstance: Display: Launching executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe', working dir 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install', data dir '', args '--port 8558 --data-dir  --no-sentry --http-forceloopback  --child-id Zen_8116_Startup'
[2026.09.17-05.04.42:305][  0]LogZenServiceInstance: Warning: Failed launch service using executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe' on port 8558
[2026.09.17-05.04.42:806][  0]LogZenServiceInstance: Display: Lock file '.lock' is not active, nothing to do
[2026.09.17-05.04.42:806][  0]LogZenServiceInstance: Display: Launching executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe', working dir 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install', data dir '', args '--port 8558 --data-dir  --no-sentry --http-forceloopback  --child-id Zen_8116_Startup'
[2026.09.17-05.04.47:808][  0]LogZenServiceInstance: Warning: Failed launch service using executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe' on port 8558
[2026.09.17-05.04.48:309][  0]LogZenServiceInstance: Display: Lock file '.lock' is not active, nothing to do
[2026.09.17-05.04.48:309][  0]LogZenServiceInstance: Display: Launching executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe', working dir 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install', data dir '', args '--port 8558 --data-dir  --no-sentry --http-forceloopback  --child-id Zen_8116_Startup'
[2026.09.17-05.04.53:311][  0]LogZenServiceInstance: Warning: Failed launch service using executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe' on port 8558
[2026.09.17-05.04.53:812][  0]LogZenServiceInstance: Display: Lock file '.lock' is not active, nothing to do
[2026.09.17-05.04.53:812][  0]LogZenServiceInstance: Display: Launching executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe', working dir 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install', data dir '', args '--port 8558 --data-dir  --no-sentry --http-forceloopback  --child-id Zen_8116_Startup'
[2026.09.17-05.04.58:815][  0]LogZenServiceInstance: Warning: Failed launch service using executable 'C:/Users/Vincent/AppData/Local/UnrealEngine/Common/Zen/Install/zenserver.exe' on port 8558
[2026.09.17-05.04.58:815][  0]LogZenServiceInstance: Warning: Local ZenServer AutoLaunch initialization timed out waiting for service to become healthy, waited 32.781 seconds
[2026.09.17-05.05.01:050][  0]LogDerivedDataCache: Display: ZenLocal: Unable to reach ZenServer HTTP service at localhost with namespace ue.ddc. ErrorCode: Connect, Error: , Status: 0, Response: .
[2026.09.17-05.05.01:050][  0]LogDerivedDataCache: Display: ZenLocal: Readiness check failed. It will be deactivated until responsiveness improves. If this is consistent, consider disabling this cache store through the use of the '-ddc=NoZenLocalFallback' or '-ddc=InstalledNoZenLocalFallback' commandline arguments.
[2026.09.17-05.05.01:052][  0]LogDerivedDataCache: Warning: C:/Users/Vincent/AppData/Local/UnrealEngine/Common/DerivedDataCache: Failed to write to C:/Users/Vincent/AppData/Local/UnrealEngine/Common/DerivedDataCache/TestData/1DD74003448EF0465E760EBC58D6BE74/TestData/0/TestData_4kb.dat. WriteError: 3 (The system cannot find the path specified.)
[2026.09.17-05.05.01:053][  0]LogDerivedDataCache: Display: C:/Users/Vincent/AppData/Local/UnrealEngine/Common/DerivedDataCache: Performance: Latency=0.04ms. RandomReadSpeed=1404.57MBs, RandomWriteSpeed=0.00MBs. Assigned SpeedClass 'Local'
[2026.09.17-05.05.01:053][  0]LogWindows: Error: appError called: Fatal error: [File:D:\build\++UE5\Sync\Engine\Source\Developer\DerivedDataCache\Private\DerivedDataBackends.cpp] [Line: 813] 
Unable to use cache graph 'Installed' because it has no writable nodes available. Add -DDC-ForceMemoryCache to the command line to bypass this if you need access to the editor settings to fix the cache configuration.





[2026.09.17-05.05.01:431][  0]LogWindows: Error: === Critical error: ===
[2026.09.17-05.05.01:431][  0]LogWindows: Error: 
[2026.09.17-05.05.01:431][  0]LogWindows: Error: Fatal error: [File:D:\build\++UE5\Sync\Engine\Source\Developer\DerivedDataCache\Private\DerivedDataBackends.cpp] [Line: 813] 
[2026.09.17-05.05.01:431][  0]LogWindows: Error: Unable to use cache graph 'Installed' because it has no writable nodes available. Add -DDC-ForceMemoryCache to the command line to bypass this if you need access to the editor settings to fix the cache configuration.
[2026.09.17-05.05.01:431][  0]LogWindows: Error: 
[2026.09.17-05.05.01:431][  0]LogWindows: Error: 
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ffa2a4d604a UnrealEditor-DerivedDataCache.dll!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ffa2a19378c UnrealEditor-DerivedDataCache.dll!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ffa2a1bbf8c UnrealEditor-DerivedDataCache.dll!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ffa2a1cc378 UnrealEditor-DerivedDataCache.dll!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ff668ced062 UnrealEditor-Cmd.exe!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ff668cdc263 UnrealEditor-Cmd.exe!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ff668cdc5ba UnrealEditor-Cmd.exe!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ff668ce1256 UnrealEditor-Cmd.exe!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ff668cf5c74 UnrealEditor-Cmd.exe!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ff668cf838a UnrealEditor-Cmd.exe!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: [Callstack] 0x00007ffb215acdf7 KERNEL32.DLL!UnknownFunction []
[2026.09.17-05.05.01:431][  0]LogWindows: Error: 
exit=3
```

### 13-android.txt

Initial process environment lacked JAVA_HOME and Java on PATH.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Android -Configuration Development
```

```text
Doctor profile: android
Doctor profile: android

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.74 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS
Android Studio         installed, with a bundled JDK (jbr)                       C:\Program Files\Android\Android Studio; version                               PASS
                                                                                 AI-261.26222.65.2614.16204760; bundled runtime: True
Android NDK            the version in the engine's Android_SDK.json              r27c (27.2.12479018)                                                           PASS
JDK                    a JDK on JAVA_HOME or the search PATH, major version 17   java not found on search PATH                                                  FAIL
                       or newer
Android SDK            the platform, build-tools and cmake in the engine's       C:\Users\Vincent\AppData\Local\Android\Sdk; android-36: True; build-tools      PASS
                       Android_SDK.json                                          36.0.0: True; cmake 3.22.1: True


Doctor failed; stopping.
exit=1
```

### 14-linux.txt

Initial process environment lacked LINUX_MULTIARCH_ROOT.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Linux -Configuration Development
```

```text
Doctor profile: linux
Doctor profile: linux

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.74 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS
Linux cross toolchain  v26; clang 20.1.8; Rocky Linux 8; UBT only                LINUX_MULTIARCH_ROOT is unset                                                  FAIL
LINUX_MULTIARCH_ROOT   existing directory                                        not found                                                                      FAIL


Doctor failed; stopping.
exit=1
```

### 15-pester.txt

Exact requested Pester command. Framework initialization failed because TestRegistry requires a denied registry write; its reported 51 failures are not 51 executed failing assertions.

```powershell
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI"
```

```text

Running tests from 1 files.
[-] C:\Users\Vincent\UnRealDash\scripts\tests\Foundation.Tests.ps1 failed with:
System.Management.Automation.RuntimeException: Framework failed:
Result 1 - Error 1:Exception: System.Exception: Was not able to create a Pester Registry key for TestRegistry at
'Microsoft.PowerShell.Core\Registry::HKEY_CURRENT_USER\Software\Pester' System.Security.SecurityException: Requested
registry access is not allowed.    at Microsoft.Win32.RegistryKey.OpenSubKey(String name, Boolean writable)    at
Microsoft.PowerShell.Commands.RegistryWrapper.OpenSubKey(String name, Boolean writable)    at
Microsoft.PowerShell.Commands.RegistryProvider.GetRegkeyForPath(String path, Boolean writeAccess)    at
Microsoft.PowerShell.Commands.RegistryProvider.GetRegkeyForPathWriteIfError(String path, Boolean writeAccess)

at Get-TempRegistry, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 16396
at New-RandomTempRegistry, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 23780
at New-TestRegistry, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 23686
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 23855
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2097
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2071
at Invoke-ScriptBlock, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2260
at Invoke-Block, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 843
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 1710
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.ScriptScope.ps1: line 9
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 3459
at Invoke-InNewScriptScope, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 3466
at Invoke-ContainerRun, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 1713
at Invoke-Test, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2778
at Invoke-Pester<End>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 5632
at <ScriptBlock>, <No file>: line 1

Result 2 - Error 1:Exception: System.Exception: Was not able to create a Pester Registry key for TestRegistry at
'Microsoft.PowerShell.Core\Registry::HKEY_CURRENT_USER\Software\Pester' System.Security.SecurityException: Requested
registry access is not allowed.    at Microsoft.Win32.RegistryKey.OpenSubKey(String name, Boolean writable)    at
Microsoft.PowerShell.Commands.RegistryWrapper.OpenSubKey(String name, Boolean writable)    at
Microsoft.PowerShell.Commands.RegistryProvider.GetRegkeyForPath(String path, Boolean writeAccess)    at
Microsoft.PowerShell.Commands.RegistryProvider.GetRegkeyForPathWriteIfError(String path, Boolean writeAccess)

at Get-TempRegistry, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 16396
at New-RandomTempRegistry, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 23780
at New-TestRegistry, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 23686
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 23877
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2130
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2071
at Invoke-ScriptBlock, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2260
at Invoke-Block, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 967
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 1710
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.ScriptScope.ps1: line 9
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 3459
at Invoke-InNewScriptScope, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 3466
at Invoke-ContainerRun, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 1713
at Invoke-Test, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2778
at Invoke-Pester<End>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 5632
at <ScriptBlock>, <No file>: line 1

at Assert-Success, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 1935
at Invoke-Block, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 980
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 1710
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.ScriptScope.ps1: line 9
at <ScriptBlock>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 3459
at Invoke-InNewScriptScope, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 3466
at Invoke-ContainerRun, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 1713
at Invoke-Test, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 2778
at Invoke-Pester<End>, C:\Users\Vincent\Documents\PowerShell\Modules\Pester\6.2.0\Pester.psm1: line 5632
at <ScriptBlock>, <No file>: line 1
Tests completed in 497ms
Tests Passed: 0, Failed: 51, Skipped: 0, Inconclusive: 0, NotRun: 0
Container failed: 1
  - C:\Users\Vincent\UnRealDash\scripts\tests\Foundation.Tests.ps1
exit=52
```

### 16-ubt-startup.txt

Direct startup diagnostic, with native crash dialogs disabled. It reproduces the trace backup access-denied exception without compiling anything.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe' 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll' -help
```

```text
Unhandled exception. System.UnauthorizedAccessException: Access to the path is denied.
   at System.IO.FileSystem.MoveFile(String sourceFullPath, String destFullPath, Boolean overwrite)
   at EpicGames.Core.FileReference.Move(FileReference sourceLocation, FileReference targetLocation)
   at EpicGames.Core.Log.BackupLogFile(FileReference outputFile)
   at UnrealBuildTool.UnrealBuildTool.Main(String[] ArgumentsArray)
exit=-532462766
```

### 17-editor-no-crash-dialog.txt

Editor retry with native crash dialogs disabled. The process now exits on the startup exception.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target editor -Configuration Development
```

```text
Doctor profile: workstation
Doctor profile: workstation

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.74 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS


& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnRealDashEditor Win64 Development -project="C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject" -waitmutex
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDashEditor Win64 Development -project=C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject -waitmutex
exit=-532462766
```

### 18-game-no-crash-dialog.txt

Game retry with native crash dialogs disabled.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target game -Configuration Development
```

```text
Doctor profile: workstation
Doctor profile: workstation

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.74 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS


& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnRealDash Win64 Development -project="C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject" -waitmutex
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDash Win64 Development -project=C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject -waitmutex
exit=-532462766
```

### 19-environment-scopes.txt

Requested environment scope recovery. User scope here belongs to CodexSandboxOffline, not Vincent.

```powershell
foreach ($name in @('ANDROID_HOME','ANDROID_SDK_ROOT','JAVA_HOME','NDKROOT','NDK_ROOT','LINUX_MULTIARCH_ROOT')) {
    $scope = if ($name -eq 'LINUX_MULTIARCH_ROOT') { 'Machine' } else { 'User' }
    $value = [Environment]::GetEnvironmentVariable($name, $scope)
    "$scope $name=$value"
    if ($value) { [Environment]::SetEnvironmentVariable($name, $value, 'Process') }
}
```

```text
User ANDROID_HOME=
User ANDROID_SDK_ROOT=
User JAVA_HOME=
User NDKROOT=
User NDK_ROOT=
Machine LINUX_MULTIARCH_ROOT=C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8\
```

### 20-android-scoped-env.txt

Retry after scoped reads. The user-scope values were empty, so the Java doctor check still failed.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Android -Configuration Development
```

```text
Doctor profile: android
Doctor profile: android

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.75 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS
Android Studio         installed, with a bundled JDK (jbr)                       C:\Program Files\Android\Android Studio; version                               PASS
                                                                                 AI-261.26222.65.2614.16204760; bundled runtime: True
Android NDK            the version in the engine's Android_SDK.json              r27c (27.2.12479018)                                                           PASS
JDK                    a JDK on JAVA_HOME or the search PATH, major version 17   java not found on search PATH                                                  FAIL
                       or newer
Android SDK            the platform, build-tools and cmake in the engine's       C:\Users\Vincent\AppData\Local\Android\Sdk; android-36: True; build-tools      PASS
                       Android_SDK.json                                          36.0.0: True; cmake 3.22.1: True


Doctor failed; stopping.
exit=1
```

### 21-linux-scoped-env.txt

Retry with the machine-scope Linux root. All doctor rows passed, then UBT startup failed.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Linux -Configuration Development
```

```text
Doctor profile: linux
Doctor profile: linux

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.75 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS
Linux cross toolchain  v26; clang 20.1.8; Rocky Linux 8; UBT only                clang version 20.1.8 (github.com/llvm/llvm-project                             PASS
                                                                                 87f0227cb60147a26a1eeb4fb06e3b505e9c7261) Target: x86_64-pc-windows-msvc
                                                                                 Thread model: posix InstalledDir:
                                                                                 C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8\x86_64-unknown-linux-gnu\bin
LINUX_MULTIARCH_ROOT   existing directory                                        C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8\                              PASS


& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnRealDash Linux Development -project="C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject" -waitmutex
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDash Linux Development -project=C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject -waitmutex
exit=-532462766
```

### 22-android-installed-paths.txt

Retry using installed SDK, NDK and Android Studio JDK paths in process environment. All doctor rows passed, then UBT startup failed.

```powershell
pwsh -NoProfile -File scripts/build.ps1 -Target game -Platform Android -Configuration Development
```

```text
Doctor profile: android
Doctor profile: android

Component              Expected                                                  Found                                                                          Status
---------              --------                                                  -----                                                                          ------
C: free space          >= 30 GB                                                  538.76 GB                                                                      PASS
Git LFS                available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                  available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                 (kitware.com/cmake).
ninja                  available                                                 1.12.1                                                                         PASS
Visual Studio          2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset           14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK            10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine          5.8.2; Launcher binary; Android and Linux platforms       5.8.2                                                                          PASS
Android Studio         installed, with a bundled JDK (jbr)                       C:\Program Files\Android\Android Studio; version                               PASS
                                                                                 AI-261.26222.65.2614.16204760; bundled runtime: True
Android NDK            the version in the engine's Android_SDK.json              r27c (27.2.12479018)                                                           PASS
JDK                    a JDK on JAVA_HOME or the search PATH, major version 17   JAVA_HOME (C:/Program Files/Android/Android Studio/jbr); openjdk version       PASS
                       or newer                                                  "25.0.3" 2026-04-21
Android SDK            the platform, build-tools and cmake in the engine's       C:/Users/Vincent/AppData/Local/Android/Sdk; android-36: True; build-tools      PASS
                       Android_SDK.json                                          36.0.0: True; cmake 3.22.1: True


& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnRealDash Android Development -project="C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject" -waitmutex
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDash Android Development -project=C:\Users\Vincent\UnRealDash\runtime\UnRealDash\UnRealDash.uproject -waitmutex
exit=-532462766
```

### 23-pester-no-registry.txt

Supplemental Pester run with its unused TestRegistry fixture disabled. All 51 tests ran; 49 passed and 2 failed.

```powershell
pwsh -NoProfile -Command '$configuration = New-PesterConfiguration; $configuration.Run.Path = "scripts/tests"; $configuration.Run.Exit = $true; $configuration.TestRegistry.Enabled = $false; Invoke-Pester -Configuration $configuration'
```

```text

Running tests from 1 files.
[-] Doctor profiles and seams.PreInstall passes on this machine 568ms
 Expected 0, because Doctor profile: preinstall
 Component              Expected                                                  Found                                                                          Status
 ---------              --------                                                  -----                                                                          ------
 C: free space          >= 150 GB                                                 538.76 GB                                                                      PASS
 git                    available                                                 git version 2.45.1.windows.1                                                   PASS
 winget                 present on PATH                                           C:\Users\Vincent\AppData\Local\Microsoft\WindowsApps\winget.exe                PASS
 PowerShell             >= 7                                                      7.6.6                                                                          PASS
 Signed-in shell        Windows user identity                                     LENOVO-BOX\CodexSandboxOffline                                                 PASS
 User PATH writable     user Environment registry key writable                    Exception calling "OpenSubKey" with "2" argument(s): "Requested registry       FAIL
                                                                                  access is not allowed.", but got 1.
 at $result.Code | Should -Be 0 -Because $result.Text, C:\Users\Vincent\UnRealDash\scripts\tests\Foundation.Tests.ps1:117
[-] Skeleton doctor gates.does not build without WhatIf even with a passing doctor 634ms
 Expected 1, but got -532462766.
 at $result.Code | Should -Be 1, C:\Users\Vincent\UnRealDash\scripts\tests\Foundation.Tests.ps1:376
Tests completed in 30.88s
Tests Passed: 49, Failed: 2, Skipped: 0, Inconclusive: 0, NotRun: 0
exit=2
```

### 24-ctest.txt

Supplemental standalone regression run: all 14 CTest entries passed.

```powershell
ctest --test-dir packages/signal-core/build/default --output-on-failure
```

```text
Internal ctest changing into directory: C:/Users/Vincent/UnRealDash/packages/signal-core/build/default
Test project C:/Users/Vincent/UnRealDash/packages/signal-core/build/default
      Start  1: smoke-subset
 1/14 Test  #1: smoke-subset .....................   Passed    0.01 sec
      Start  2: test_binary_telemetry_v1.cpp
 2/14 Test  #2: test_binary_telemetry_v1.cpp .....   Passed    0.30 sec
      Start  3: test_definition_pack.cpp
 3/14 Test  #3: test_definition_pack.cpp .........   Passed    0.01 sec
      Start  4: test_heartbeat_and_held.cpp
 4/14 Test  #4: test_heartbeat_and_held.cpp ......   Passed    0.01 sec
      Start  5: test_interpolator.cpp
 5/14 Test  #5: test_interpolator.cpp ............   Passed    0.01 sec
      Start  6: test_recording.cpp
 6/14 Test  #6: test_recording.cpp ...............   Passed    0.05 sec
      Start  7: test_registry.cpp
 7/14 Test  #7: test_registry.cpp ................   Passed    0.01 sec
      Start  8: test_rule_engine.cpp
 8/14 Test  #8: test_rule_engine.cpp .............   Passed    0.01 sec
      Start  9: test_rule_state.cpp
 9/14 Test  #9: test_rule_state.cpp ..............   Passed    0.01 sec
      Start 10: test_sample.cpp
10/14 Test #10: test_sample.cpp ..................   Passed    0.01 sec
      Start 11: test_scenarios.cpp
11/14 Test #11: test_scenarios.cpp ...............   Passed    0.12 sec
      Start 12: test_snapshot_exchange.cpp
12/14 Test #12: test_snapshot_exchange.cpp .......   Passed    0.02 sec
      Start 13: test_threading_stress.cpp
13/14 Test #13: test_threading_stress.cpp ........   Passed    0.16 sec
      Start 14: test_units.cpp
14/14 Test #14: test_units.cpp ...................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 14

Total Test time (real) =   0.75 sec
exit=0
```

### 25-doctest.txt

Supplemental full doctest run: 120 cases and 111640 assertions passed.

```powershell
& packages/signal-core/build/default/signal-core-tests.exe
```

```text
[doctest] doctest version is "2.4.12"
[doctest] run with "--help" for options
===============================================================================
[doctest] test cases:    120 |    120 passed | 0 failed | 0 skipped
[doctest] assertions: 111640 | 111640 passed | 0 failed |
[doctest] Status: SUCCESS!
exit=0
```

## Assertion counts side by side

| Check | Standalone CMake subset | In-engine commandlet |
| --- | --- | --- |
| Assertions | 83 | Not measured; startup failed before checks |
| Failures | 0 | Not measured |
| Exit | 0 | 3 from engine startup |

The counts cannot be declared equal. Both implementations call the same `RunSmokeSubset` header, but only the standalone execution was measured. The full doctest count of 111640 is separate and is not the comparison count.

## Changes forced on chunk 02 files

None. `SignalCore.Build.cs` and `SignalCoreModule.cpp` were left unchanged. UBT never reached compilation, so there is no compiler error justifying a change to either file. The only addition under the restricted SignalCore directory is the explicitly authorized public `SmokeSubset.h`.

## Ambiguities and choices

- The new `-Target editor|game` API conflicts with chunk 01's `-Target win64|android|linux` preview calls. The script accepts the old platform aliases as well as the new target names. A conflicting explicit `-Platform` is rejected.
- The spec requires preserving the old RunUAT `-WhatIf` preview exactly while real execution now uses Build.bat. The preview therefore remains the original Development BuildCookRun line, including when new target/configuration parameters are supplied. Real execution uses the selected target, platform and configuration. This difference follows the explicit preview requirement.
- The existing Pester test `does not build without WhatIf even with a passing doctor` requires real execution to remain unimplemented. That is incompatible with this chunk's main build-script deliverable. The real build is implemented; the old test is retained and its failure reported. Tests were outside the authorized edit scope.
- Shipping editor targets are not supported by Unreal, so that combination is rejected before invoking the doctor. Editor targets on non-Win64 platforms and isolation on non-Win64 targets are also rejected with a factual error.
- The spec gives no exact sample-adapter field list. The adapter preserves the sample's value, unit, source, sequence, receive/source timestamps, source-time presence, quality, age evidence and generation in a plain struct. It adds only the requested string conversions, with no widget API.
- The spec does not set the subset assertion count, temperature input set or hysteresis band. The shared header uses the six existing standalone temperature round-trip inputs and exactly the existing tolerance, a 100 degC literal and 2 K hysteresis band. It covers below, at and above the threshold, holding through the band and clearing below it. Setup and result checks total 83 assertions.
- `DefaultInput.ini` has no specified settings, so it contains a comment and no bindings. The required `Engine.Engine` section is present; category verbosity is set under `Core.Log`, where Unreal reads category overrides.
- The spec's last ownership sentence says it touches nothing else, but its Constraints section explicitly permits `docs/reports/` and its proof requires a full report. Report and proof files are stored there.
- The spec's pointer from rendering settings to Key decision 5 does not match that decision's topic, which is runtime C++ UI. The exact renderer values in the frozen spec were used; no renderer redesign was made.

## Deviations and reasons

- As expressly directed by the frozen spec, UnRealDashCore wraps signal-core only. The dashboard-spec half of PLAN.md 4.1 remains deferred to 4.4's package loader. This is the planned scope deviation, not an implementation omission hidden as completion.
- Proof used the accessible Visual Studio copies of CMake and Ninja instead of the sandbox-inaccessible WinGet installations. The actual versions, build directory and generator are recorded above.
- Android's installed runtime reports OpenJDK 25.0.3, and its SDK is android-36. The existing doctor and frozen spec use the installed engine manifest. Project MinSDKVersion remains 26 and TargetSDKVersion remains 35 exactly as requested. Nothing in PLAN.md or the doctor was changed.
- The exact Pester command could not initialize TestRegistry. A supplemental run disabled that unused fixture without editing the suite: 49 passed and 2 failed. One failure is the sandbox's denied user-PATH registry write; the other is the incompatible old skeleton expectation. Zero Pester failures could not be achieved faithfully within this scope.
- The three Win64 builds, Android build and Linux build did not produce binaries. The doctor gates passed after environment recovery, but UBT terminated during startup with exit -532462766. A direct UBT diagnostic reports `System.UnauthorizedAccessException` in `Log.BackupLogFile`; the installed source performs this trace backup in the user settings directory before processing normal build options. No engine source, permissions or user configuration was changed to work around that restriction.
- The exact commandlet command exited 3 because Unreal's installed derived-data cache graph had no writable nodes. No in-engine assertion count was produced, and no equality claim is made.

## Anything not completed

No Unreal compilation, link or binary qualification was possible in this run. The new Unreal source files and the pre-existing SignalCore module rules remain unverified by UBT for Win64, Android ARM64 and Linux x86-64. The isolation path was exercised and the workstation doctor passed with all six variables pointing to newly created empty directories, but the required successful isolation build was not achieved. No in-engine subset executed. The requested zero-failure Pester gate was not achieved. Hosted Windows/Linux CMake CI was not run; CI changes and network access were outside scope.

No smoke scene, device run, widget, package loader, dashboard-spec Unreal module, APK or CI modification was added.
