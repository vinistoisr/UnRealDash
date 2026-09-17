# Chunk 10 package loader: advisory build report

Date: 2026-09-17. PLAN task 4.4. Implemented against revision 3.

Criteria 1 to 7 mean the chunk is **built**. Criterion 8 makes it **complete**. This session cannot claim either: UBT compilation/linking, engine execution, Linux CI and the unmodified full Pester gate remain unverified or sandbox-blocked. **Device half not run.** The status is not yet verified as built, and not complete.

The Windows standalone build passes. The rebuilt move-only baseline has 23 doctest cases and 1,001,768 assertions; final XML has 28 cases and 1,002,083 assertions, all passing. The older pre-existing executable had 17 cases and a failure on windows-attributes-entry; it was stale and was not used as the authoritative baseline.

The portable Load gate derives 7 accepted, 36 rejected, 26 both-form and 17 archive-only cases from cases.json at runtime. All pass, including verdict/code/pointer parity and asset byte equality for accepted both-form cases. These are portable tests, not evidence that the engine wrapper has executed.

Files added or changed (all implementation changes serve PLAN 4.4):

- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/DefinitionPackBuilder.h`: single authoritative source moved from packages/dashboard-spec/include/dashboard_spec/DefinitionPackBuilder.h; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Document.h`: single authoritative source moved from packages/dashboard-spec/include/dashboard_spec/Document.h; typed component/property accessors; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Errors.h`: single authoritative source moved from packages/dashboard-spec/include/dashboard_spec/Errors.h; stable unreadable code 45 and allocation-safe error text; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Limits.h`: single authoritative source moved from packages/dashboard-spec/include/dashboard_spec/Limits.h; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/PackageReader.h`: single authoritative source moved from packages/dashboard-spec/include/dashboard_spec/PackageReader.h; shared Validate, owning Load and lazy asset cache; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/RuleTreeBuilder.h`: single authoritative source moved from packages/dashboard-spec/include/dashboard_spec/RuleTreeBuilder.h; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/SemanticPass.h`: single authoritative source moved from packages/dashboard-spec/include/dashboard_spec/SemanticPass.h; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Validator.h`: single authoritative source moved from packages/dashboard-spec/include/dashboard_spec/Validator.h; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/BoundedParse.cpp`: single authoritative source moved from packages/dashboard-spec/src/BoundedParse.cpp; bounded caller-owned asset buffer reads; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/DefinitionPackBuilder.cpp`: single authoritative source moved from packages/dashboard-spec/src/DefinitionPackBuilder.cpp; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/Errors.cpp`: single authoritative source moved from packages/dashboard-spec/src/Errors.cpp; stable unreadable code 45 and allocation-safe error text; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/ImageHeader.cpp`: single authoritative source moved from packages/dashboard-spec/src/ImageHeader.cpp; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/Internal.h`: single authoritative source moved from packages/dashboard-spec/src/Internal.h; bounded caller-owned asset buffer reads; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/PackageInternal.h`: single authoritative source moved from packages/dashboard-spec/src/PackageInternal.h; bounded caller-owned asset buffer reads; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/PackageReader.cpp`: single authoritative source moved from packages/dashboard-spec/src/PackageReader.cpp; shared Validate, owning Load and lazy asset cache; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/PathResolver.cpp`: single authoritative source moved from packages/dashboard-spec/src/PathResolver.cpp; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/RuleTreeBuilder.cpp`: single authoritative source moved from packages/dashboard-spec/src/RuleTreeBuilder.cpp; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/SchemaValidation.cpp`: single authoritative source moved from packages/dashboard-spec/src/SchemaValidation.cpp; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/SemanticPass.cpp`: single authoritative source moved from packages/dashboard-spec/src/SemanticPass.cpp; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/validate_main.cpp`: single authoritative source moved from packages/dashboard-spec/src/validate_main.cpp; CLI body excluded from UBT; PLAN 4.4.
- `.github/workflows/signal-core.yml`: source-authority guard and doctest XML totals; PLAN 4.4.
- `.github/workflows/spec-tools.yml`: source-authority guard and doctest XML totals; PLAN 4.4.
- `packages/dashboard-spec/CMakeLists.txt`: compile moved sources with Public include root; private test includes use explicit relative paths; PLAN 4.4.
- `packages/dashboard-spec/README.md`: document source authority, ownership and schema staging; PLAN 4.4.
- `packages/dashboard-spec/tests/TestSupport.h`: follow moved private header; PLAN 4.4.
- `packages/dashboard-spec/tests/test_loaded_package.cpp`: corpus/parity, lifetime/moves, typed access, lazy/cache/link/bound checks, Read cost and allocation-failure tests; PLAN 4.4.
- `packages/dashboard-spec/tests/test_package_reader.cpp`: follow moved private header; PLAN 4.4.
- `packages/dashboard-spec/tests/test_schema.cpp`: follow moved error vocabulary header; PLAN 4.4.
- `runtime/UnRealDash/Config/DefaultEngine.ini`: default to the package HUD game mode; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/DashboardSpec.Build.cs`: portable UBT module, vendor dependencies, compiler exports and pinned NonUFS schemas; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/DashboardSpecModule.cpp`: sole engine registration translation unit; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/DocumentView.cpp`: opaque typed document/component/property/theme accessors; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Private/Miniz.c`: UBT compiles the existing vendor implementation, without a copied implementation; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Export.h`: compiler-only DLL linkage annotations, no Unreal macro; PLAN 4.4.
- `runtime/UnRealDash/Source/DashboardSpec/README.md`: document portable module boundary; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPackageScreen.cpp`: command-line startup, placeholder viewport and five-field rejection display; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDash/Private/Package/DashPackageScreen.h`: command-line startup, placeholder viewport and five-field rejection display; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDash/UnRealDash.Build.cs`: UMG and Slate dependencies; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDashCore/Private/Package/ComponentRegistry.cpp`: pinned builder context, explicit placeholder registrations and ID-based parent resolution; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDashCore/Private/Package/DashPackageLoader.cpp`: engine-only package/value/error API and physical schema-path resolver; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDashCore/Private/Package/DashPackageLoader.h`: engine-only package/value/error API and physical schema-path resolver; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDashCore/Private/Package/DashPackageTestCommandlet.cpp`: engine fixture gate using expected verdicts, profiles and archive-only flags; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDashCore/Private/Package/DashPackageTestCommandlet.h`: engine fixture gate using expected verdicts, profiles and archive-only flags; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDashCore/Public/UnRealDashCore/ComponentRegistry.h`: pinned builder context, explicit placeholder registrations and ID-based parent resolution; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDashCore/Public/UnRealDashCore/DashPackageLoader.h`: engine-only package/value/error API and physical schema-path resolver; PLAN 4.4.
- `runtime/UnRealDash/Source/UnRealDashCore/UnRealDashCore.Build.cs`: DashboardSpec, UMG, Slate and fixture-commandlet dependencies; PLAN 4.4.
- `runtime/UnRealDash/UnRealDash.uproject`: register DashboardSpec runtime module; PLAN 4.4.
- `scripts/README.md`: engine fixture commandlet and render-gate instructions; PLAN 4.4.
- `scripts/SignalCoreLayering.ps1`: permit downward dependency; prohibit game/public-wrapper leaks and engine dependencies in DashboardSpec; PLAN 4.4.
- `scripts/doctest-summary.py`: gate case/assertion totals from doctest XML; PLAN 4.4.
- `scripts/doctor.ps1`: run extended layering check; PLAN 4.4.
- `scripts/tests/dashboard-spec-layering.Tests.ps1`: layering, source move and schema staging regression checks; PLAN 4.4.
- `scripts/verify-chunk10-facts.py`: retain all eight original schema-macro consumers and admit the new load test; remainder of supplied checker preserved; PLAN 4.4.
- `tools/dashboard_spec/errors.py`: add stable error code 45 without renumbering; PLAN 4.4.
- `tools/tests/test_semantic.py`: cross-language source metadata check follows moved headers; production Python schema path unchanged; PLAN 4.4.
- `docs/reports/logs/chunk-10/android-build-retry.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/android-build.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/baseline.xml`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/cmake-build-test.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/cmake-build.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/cmake-final.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/diff-check.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/doctest-final.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/doctor-android.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/doctor-workstation-vs-path.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/doctor-workstation.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/facts-final.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/facts.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/final.xml`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/git-status.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/move-baseline.xml`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/pester-no-registry-final.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/pester-no-registry.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/pester.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/pytest-workspace-temp.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/pytest.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/source-move.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/logs/chunk-10/win64-build.txt`: advisory proof output; PLAN 4.4.
- `docs/reports/2026-09-17-chunk-10-package-loader.md`: this report; PLAN 4.4.

The working-tree diff also contains edits to the frozen spec text which this implementation did not make; they were preserved. The supplied facts checker was extended only for the new ninth schema-macro consumer. The old src/include directories are absent. Git mv was denied an index lock, so native Move-Item performed the actual moves; git status shows deletions plus untracked destinations until staging. No forwarding headers or source copies remain.

Proof output, verbatim. Commands used PowerShell 7. CMake used the default preset from packages/dashboard-spec with vcvars64. The per-user WinGet launchers were denied; the successful run prepended the installed Visual Studio CMake 3.31.6-msvc6 and Ninja 1.12.1 directories to process PATH. No persistent environment changes were made.

Source-move audit

```text
packages/dashboard-spec/include/dashboard_spec/DefinitionPackBuilder.h -> runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/DefinitionPackBuilder.h
packages/dashboard-spec/include/dashboard_spec/Document.h -> runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Document.h
packages/dashboard-spec/include/dashboard_spec/Errors.h -> runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Errors.h
packages/dashboard-spec/include/dashboard_spec/Limits.h -> runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Limits.h
packages/dashboard-spec/include/dashboard_spec/PackageReader.h -> runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/PackageReader.h
packages/dashboard-spec/include/dashboard_spec/RuleTreeBuilder.h -> runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/RuleTreeBuilder.h
packages/dashboard-spec/include/dashboard_spec/SemanticPass.h -> runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/SemanticPass.h
packages/dashboard-spec/include/dashboard_spec/Validator.h -> runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/Validator.h
packages/dashboard-spec/src/BoundedParse.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/BoundedParse.cpp
packages/dashboard-spec/src/DefinitionPackBuilder.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/DefinitionPackBuilder.cpp
packages/dashboard-spec/src/Errors.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/Errors.cpp
packages/dashboard-spec/src/ImageHeader.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/ImageHeader.cpp
packages/dashboard-spec/src/Internal.h -> runtime/UnRealDash/Source/DashboardSpec/Private/Internal.h
packages/dashboard-spec/src/PackageInternal.h -> runtime/UnRealDash/Source/DashboardSpec/Private/PackageInternal.h
packages/dashboard-spec/src/PackageReader.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/PackageReader.cpp
packages/dashboard-spec/src/PathResolver.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/PathResolver.cpp
packages/dashboard-spec/src/RuleTreeBuilder.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/RuleTreeBuilder.cpp
packages/dashboard-spec/src/SchemaValidation.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/SchemaValidation.cpp
packages/dashboard-spec/src/SemanticPass.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/SemanticPass.cpp
packages/dashboard-spec/src/validate_main.cpp -> runtime/UnRealDash/Source/DashboardSpec/Private/validate_main.cpp
PASS: 20 source files moved; old include/src directories absent; no forwarding headers.
```

Android preflight: pwsh -NoProfile -File scripts/doctor.ps1 -Profile android (exit 1; ran during module wiring)

```text
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:6: signal_core::DefinitionPack pack{};
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:10: std::vector<signal_core::FieldDefinition> fields;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:11: std::vector<signal_core::FrameDefinition> frames;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:15: const signal_core::DefinitionPack &DefinitionPackBuilder::Pack() const { return storage_->pack; }
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:26: if (value && value->IsString() && !signal_core::ParseUnit(Text(*value)).Ok())
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:59: signal_core::FieldDefinition field;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:70: const auto unit = signal_core::ParseUnit(Text(value["unit"]));
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:75: Text(value["acquisition"]) == "held" ? signal_core::Acquisition::held : signal_core::Acquisition::live;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:91: Text(frame["role"]) == "status" ? signal_core::FrameRole::status : signal_core::FrameRole::telemetry,
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:93: std::span<const signal_core::FieldDefinition>(s.fields).subspan(start)});
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:95: signal_core::DefinitionPack pack{s.id.c_str(), s.version.c_str(), root["api"].GetUint(), s.frames};
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp:96: const auto status = signal_core::ValidatePack(pack);
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:6: std::vector<signal_core::ExpressionNode> nodes;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:8: signal_core::RuleDefinition rule;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:12: const signal_core::RuleDefinition &RuleTreeBuilder::Rule() const { return storage_->rule; }
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:53: signal_core::ExpressionNode node;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:59: node.operation = static_cast<signal_core::Operation>(found - std::begin(operations));
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:64: auto unit = signal_core::ParseUnit(Text(value["unit"]));
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:113: auto unit = signal_core::ParseUnit(Text(hysteresis["unit"]));
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:126: result.missing_input_policy = policy == "hold_last"    ? signal_core::MissingInputPolicy::hold_last
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:127: : policy == "force_warn" ? signal_core::MissingInputPolicy::force_warn
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp:128: : signal_core::MissingInputPolicy::treat_unavailable;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Public\dashboard_spec\DefinitionPackBuilder.h:2: #include "SignalCore/DefinitionPack.h"
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Public\dashboard_spec\DefinitionPackBuilder.h:13: const signal_core::DefinitionPack &Pack() const;
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Public\dashboard_spec\RuleTreeBuilder.h:2: #include "SignalCore/RuleEngine.h"
C:\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Public\dashboard_spec\RuleTreeBuilder.h:19: const signal_core::RuleDefinition &Rule() const;
Doctor profile: android

Component                Expected                                                  Found                                                                          Status
---------                --------                                                  -----                                                                          ------
C: free space            >= 30 GB                                                  487.20 GB                                                                      PASS
Git LFS                  available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                    available                                                 Exception calling "ResolveLinkTarget" with "1" argument(s): "Access to the     FAIL
                                                                                   path 'C:\Users\Vincent\AppData\Local\Microsoft\WinGet\Links\cmake.exe' is
                                                                                   denied."
ninja                    available                                                 Exception calling "ResolveLinkTarget" with "1" argument(s): "Access to the     FAIL
                                                                                   path 'C:\Users\Vincent\AppData\Local\Microsoft\WinGet\Links\ninja.exe' is
                                                                                   denied."
Visual Studio            2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset             14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK              10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine            5.8.2, Launcher binary install                            5.8.2                                                                          PASS
Engine Android support   UnrealBuildTool reports Android VALID                     tool timed out after 15 seconds                                                FAIL
Android Studio           installed, with a bundled JDK (jbr)                       C:\Program Files\Android\Android Studio; version                               PASS
                                                                                   AI-261.26222.65.2614.16204760; bundled JDK 25.0.3
Android NDK              the version in the engine's Android_SDK.json              r27c (27.2.12479018)                                                           PASS
JDK                      a JDK on JAVA_HOME or the search PATH, major version 17   JAVA_HOME                                                                      PASS
                         or newer                                                  (C:\Users\Vincent\AppData\Local\Programs\Microsoft\jdk-17.0.10.7-hotspot);
                                                                                   openjdk version "17.0.10" 2024-01-16 LTS
Project JDK              UNREALDASH_JAVA_HOME naming a JDK in the supported        C:\Users\Vincent\AppData\Local\Programs\Microsoft\jdk-17.0.10.7-hotspot        PASS
                         major-version range                                       (process); openjdk version "17.0.10" 2024-01-16 LTS
Android SDK              the platform, build-tools and cmake in the engine's       C:\Users\Vincent\AppData\Local\Android\Sdk; android-36: True; build-tools      PASS
                         Android_SDK.json                                          36.0.0: True; cmake 3.22.1: True
SignalCore layering      Only UnRealDashCore calls SignalCore                      26 token violations                                                            FAIL
```

Early Android ARM64: Build.bat UnRealDash Android Development -project=C:/Users/Vincent/UnRealDash/runtime/UnRealDash/UnRealDash.uproject -architecture=arm64 -waitmutex (interrupted after remaining at UBT startup)

```text
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDash Android Development -project=C:/Users/Vincent/UnRealDash/runtime/UnRealDash/UnRealDash.uproject -architecture=arm64 -waitmutex
```

Android retry with workspace-local DOTNET_CLI_HOME and explicit workspace log path (same stall; interrupted)

```text
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDash Android Development -project=C:/Users/Vincent/UnRealDash/runtime/UnRealDash/UnRealDash.uproject -architecture=arm64 -waitmutex -log=C:/Users/Vincent/UnRealDash/docs/reports/logs/chunk-10/ubt-android.txt
```

Win64 Development Build.bat attempt (same startup stall; interrupted)

```text
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnRealDash Win64 Development -project=C:/Users/Vincent/UnRealDash/runtime/UnRealDash/UnRealDash.uproject -log=C:/Users/Vincent/UnRealDash/docs/reports/logs/chunk-10/ubt-win64.txt
```

cmake --preset default; cmake --build --preset default; ctest --preset default --output-on-failure, after vcvars64

```text
Preset CMake variables:

  CMAKE_BUILD_TYPE="RelWithDebInfo"

-- Compiler: MSVC 19.44.35223.0
-- signal_core: C++20, exceptions OFF, RTTI OFF, warnings as errors
-- dashboard_spec: C++20, exceptions OFF, RTTI OFF, warnings as errors
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: C:/Users/Vincent/UnRealDash/packages/dashboard-spec/build/default
[0/2] Re-checking globbed directories...
[1/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\Errors.cpp.obj
[2/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_definition_pack_builder.cpp.obj
[3/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DocumentView.cpp.obj
[4/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\ImageHeader.cpp.obj
[5/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_bounded_parse.cpp.obj
[6/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_malformed_harness.cpp.obj
[7/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_corpus.cpp.obj
[8/26] Building CXX object CMakeFiles\telemetry-decode-jsonl.dir\tools\telemetry-decode-jsonl.cpp.obj
[9/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_package_reader.cpp.obj
[10/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_rule_tree_builder.cpp.obj
[11/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\PathResolver.cpp.obj
[12/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_semantic.cpp.obj
[13/26] Building CXX object CMakeFiles\dashboard-spec-validate.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\validate_main.cpp.obj
[14/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_schema.cpp.obj
[15/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\BoundedParse.cpp.obj
[16/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\DefinitionPackBuilder.cpp.obj
[17/26] Building CXX object CMakeFiles\dashboard-spec-tests.dir\tests\test_loaded_package.cpp.obj
[18/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\RuleTreeBuilder.cpp.obj
[19/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\PackageReader.cpp.obj
[20/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\SemanticPass.cpp.obj
[21/26] Building CXX object CMakeFiles\dashboard_spec.dir\C_\Users\Vincent\UnRealDash\runtime\UnRealDash\Source\DashboardSpec\Private\SchemaValidation.cpp.obj
[22/26] Linking CXX static library dashboard_spec.lib
[23/26] Linking CXX executable dashboard-spec-validate.exe
[24/26] Linking CXX executable telemetry-decode-jsonl.exe
[25/26] Linking CXX executable dashboard-spec-tests.exe
Test project C:/Users/Vincent/UnRealDash/packages/dashboard-spec/build/default
    Start 1: test_bounded_parse.cpp
1/9 Test #1: test_bounded_parse.cpp .............   Passed    0.04 sec
    Start 2: test_corpus.cpp
2/9 Test #2: test_corpus.cpp ....................   Passed    0.63 sec
    Start 3: test_definition_pack_builder.cpp
3/9 Test #3: test_definition_pack_builder.cpp ...   Passed    0.05 sec
    Start 4: test_loaded_package.cpp
4/9 Test #4: test_loaded_package.cpp ............   Passed    0.54 sec
    Start 5: test_malformed_harness.cpp
5/9 Test #5: test_malformed_harness.cpp .........   Passed    7.63 sec
    Start 6: test_package_reader.cpp
6/9 Test #6: test_package_reader.cpp ............   Passed    0.48 sec
    Start 7: test_rule_tree_builder.cpp
7/9 Test #7: test_rule_tree_builder.cpp .........   Passed    0.01 sec
    Start 8: test_schema.cpp
8/9 Test #8: test_schema.cpp ....................   Passed    0.03 sec
    Start 9: test_semantic.cpp
9/9 Test #9: test_semantic.cpp ..................   Passed    0.05 sec

100% tests passed, 0 tests failed out of 9

Total Test time (real) =   9.48 sec
exit=0
```

dashboard-spec-tests --reporters=xml --out=.../final.xml; python scripts/doctest-summary.py .../final.xml

```text
Load corpus accepted=7 rejected=36 both=26 archive_only=17
Malformed harness inputs: 1000000
cases=28 failed_cases=0 assertions=1002083 failed_assertions=0
exit=0
```

pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation, original PATH (exit 1)

```text
Doctor profile: workstation

Component                Expected                                                  Found                                                                          Status
---------                --------                                                  -----                                                                          ------
C: free space            >= 30 GB                                                  487.14 GB                                                                      PASS
Git LFS                  available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                    available                                                 Exception calling "ResolveLinkTarget" with "1" argument(s): "Access to the     FAIL
                                                                                   path 'C:\Users\Vincent\AppData\Local\Microsoft\WinGet\Links\cmake.exe' is
                                                                                   denied."
ninja                    available                                                 Exception calling "ResolveLinkTarget" with "1" argument(s): "Access to the     FAIL
                                                                                   path 'C:\Users\Vincent\AppData\Local\Microsoft\WinGet\Links\ninja.exe' is
                                                                                   denied."
Visual Studio            2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset             14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK              10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine            5.8.2, Launcher binary install                            5.8.2                                                                          PASS
Portable library         Game uses wrappers; DashboardSpec has no engine types     0 token violations                                                             PASS
layering
```

Same workstation doctor with installed VS CMake/Ninja on process PATH (exit 0)

```text
Doctor profile: workstation

Component                Expected                                                  Found                                                                          Status
---------                --------                                                  -----                                                                          ------
C: free space            >= 30 GB                                                  487.13 GB                                                                      PASS
Git LFS                  available                                                 git-lfs/3.5.1 (GitHub; windows amd64; go 1.21.7; git e237bb3a)                 PASS
cmake                    available                                                 cmake version 3.31.6-msvc6 CMake suite maintained and supported by Kitware     PASS
                                                                                   (kitware.com/cmake).
ninja                    available                                                 1.12.1                                                                         PASS
Visual Studio            2022 Community 17.14+; both C++ workloads                 17.14.37710.0                                                                  PASS
MSVC toolset             14.44 (UE minimum 14.38; recommendation 14.50)            14.44.35207, 14.44.35207                                                       PASS
Windows SDK              10.0.26100                                                C:\Program Files (x86)\Windows Kits\10\; Include versions: 10.0.26100.0        PASS
Unreal Engine            5.8.2, Launcher binary install                            5.8.2                                                                          PASS
SignalCore layering      SignalCore + DashboardSpec layering                       0 token violations                                                             PASS
```

pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI -Output Detailed" (exit 80; TestRegistry setup denied)

```text
Pester v6.2.0

Running tests from 4 files.

Running tests from 'C:\Users\Vincent\UnRealDash\scripts\tests\AcquisitionLayering.Tests.ps1'
[-] C:\Users\Vincent\UnRealDash\scripts\tests\AcquisitionLayering.Tests.ps1 failed with:
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

Running tests from 'C:\Users\Vincent\UnRealDash\scripts\tests\dashboard-spec-layering.Tests.ps1'
[-] C:\Users\Vincent\UnRealDash\scripts\tests\dashboard-spec-layering.Tests.ps1 failed with:
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

Running tests from 'C:\Users\Vincent\UnRealDash\scripts\tests\Foundation.Tests.ps1'
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

Running tests from 'C:\Users\Vincent\UnRealDash\scripts\tests\SmokePackage.Tests.ps1'
[-] C:\Users\Vincent\UnRealDash\scripts\tests\SmokePackage.Tests.ps1 failed with:
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
Tests completed in 851ms
Tests Passed: 0, Failed: 76, Skipped: 0, Inconclusive: 0, NotRun: 0
Container failed: 4
  - C:\Users\Vincent\UnRealDash\scripts\tests\AcquisitionLayering.Tests.ps1
  - C:\Users\Vincent\UnRealDash\scripts\tests\dashboard-spec-layering.Tests.ps1
  - C:\Users\Vincent\UnRealDash\scripts\tests\Foundation.Tests.ps1
  - C:\Users\Vincent\UnRealDash\scripts\tests\SmokePackage.Tests.ps1
```

Diagnostic only: Pester TestRegistry.Enabled=false, still Invoke-Pester -Path scripts/tests -CI -Output Detailed (exit 1)

```text
Pester v6.2.0

Running tests from 4 files.

Running tests from 'C:\Users\Vincent\UnRealDash\scripts\tests\AcquisitionLayering.Tests.ps1'
Describing PLAN 4.3 SignalCore layering
  [+] names file and line for each forbidden token outside the allowed modules 99ms
  [+] accepts the engine sample seam and the actual source tree 19ms
  [+] keeps the layering gate wired into doctor 6ms

Running tests from 'C:\Users\Vincent\UnRealDash\scripts\tests\dashboard-spec-layering.Tests.ps1'
Describing PLAN 4.4 DashboardSpec layering
  [+] allows the lower library dependency and registration exception only 23ms
  [+] accepts the current source tree and is wired into doctor 56ms
  [+] keeps one authoritative copy and pinned non-UFS schemas 18ms

Running tests from 'C:\Users\Vincent\UnRealDash\scripts\tests\Foundation.Tests.ps1'
Describing Doctor profiles and seams
  [+] workstation prints the PLAN 0.7 row set plus the PLAN 4.3 layering gate 1.49s
  [+] android prints the PLAN 0.7 row set plus the PLAN 4.3 layering gate 16.5s
  [+] linux prints the PLAN 0.7 row set plus the PLAN 4.3 layering gate 15.88s
  [+] device prints the PLAN 0.7 row set plus the PLAN 4.3 layering gate 1.13s
  [+] detects NDK membership added to workstation contrary to PLAN 0.7 812ms
  [+] SearchPath without cmake fails workstation and identifies cmake 572ms
  [-] PreInstall passes on this machine 517ms
   Expected 0, because Doctor profile: preinstall
   Component                Expected                                                  Found                                                                          Status
   ---------                --------                                                  -----                                                                          ------
   C: free space            >= 150 GB                                                 487.13 GB                                                                      PASS
   git                      available                                                 git version 2.45.1.windows.1                                                   PASS
   winget                   present on PATH                                           C:\Users\Vincent\AppData\Local\Microsoft\WindowsApps\winget.exe                PASS
   PowerShell               >= 7                                                      7.6.6                                                                          PASS
   Signed-in shell          Windows user identity                                     LENOVO-BOX\CodexSandboxOffline                                                 PASS
   User PATH writable       user Environment registry key writable                    Exception calling "OpenSubKey" with "2" argument(s): "Requested registry       FAIL
                                                                                      access is not allowed.", but got 1.
   at $result.Code | Should -Be 0 -Because $result.Text, C:\Users\Vincent\UnRealDash\scripts\tests\Foundation.Tests.ps1:121
  [+] PreInstall checks only capacity and installer prerequisites 508ms
  [+] PinsFile controls membership and expected matching without changing defaults 825ms
  [+] fails closed for missing, invalid and empty pin files 754ms

Describing Deterministic doctor transitions
  [+] workstation cmake changes from FAIL to PASS with a cmd wrapper on SearchPath 790ms
  [+] android NDK changes from FAIL to PASS when the engine manifest version appears 756ms
  [+] linux root and toolchain change from FAIL to PASS using the internal version marker 803ms
  [+] rejects clang 20.1.80 against the 20.1.8 pin 435ms
  [+] device reachability and storage change from FAIL to PASS with the configured fake serial 1.62s
  [+] workstation capacity changes from PASS to FAIL with a 999999 GB minimum 887ms
  [+] preinstall capacity changes from PASS to FAIL with a 999999 GB minimum 765ms
  [+] the project JDK row enforces both ends of the range gradle supports 2.89s
  [+] JDK prefers JAVA_HOME and enforces the major-version floor 2.37s
  [+] engine platform support is decided by UnrealBuildTool, not by the file layout 826ms
  [+] engine platform support is decided by UnrealBuildTool, not by the file layout 838ms
  [+] Windows SDK kits_root override changes from FAIL to PASS when pinned Include exists 725ms
  [+] Android SDK requires the platform, build-tools and cmake the engine manifest names 1.54s
  [+] SDK and NDK use the shared ANDROID_SDK_ROOT root fallback 385ms
  [+] SDK and NDK use the shared LOCALAPPDATA root fallback 393ms

Describing Skeleton doctor gates
  [+] build win64 previews Win64 only after a passing workstation doctor 513ms
  [+] build android previews Android only after a passing android doctor 491ms
  [+] build linux previews Linux only after a passing linux doctor 493ms
  [+] package-windows  previews Win64 only after a passing workstation doctor 502ms
  [+] package-android  previews Android only after a passing android doctor 507ms
  [+] package-linux  previews Linux only after a passing linux doctor 479ms
  [+] deploy-deck  previews Android only after a passing device doctor 490ms
  [+] capture-metrics windows previews Win64 only after a passing workstation doctor 483ms
  [+] capture-metrics device previews Android only after a passing device doctor 482ms
  [+] capture-metrics soak previews Win64 only after a passing workstation doctor 478ms
  [+] build win64 stops without UAT when its doctor fails 514ms
  [+] build android stops without UAT when its doctor fails 487ms
  [+] build linux stops without UAT when its doctor fails 489ms
  [+] package-windows  stops without UAT when its doctor fails 480ms
  [+] package-android  stops without UAT when its doctor fails 484ms
  [+] package-linux  stops without UAT when its doctor fails 469ms
  [+] deploy-deck  stops without UAT when its doctor fails 480ms
  [+] capture-metrics windows stops without UAT when its doctor fails 468ms
  [+] capture-metrics device stops without UAT when its doctor fails 474ms
  [+] capture-metrics soak stops without UAT when its doctor fails 474ms
  [+] build.ps1 previews the module build it would run, not a packaging command 974ms
  [+] fails closed when the doctor file is missing 406ms

Describing Repository foundation
  [+] has each specified directory README and root policy file 12ms
  [+] tracks every required binary extension with LFS and normalizes text 5ms
  [+] contains a real 16 by 16 PNG probe 5ms
  [+] provides provisional licenses and links them 3ms
  [+] every component name fits the rendered table column 6ms
  [+] stores each PLAN pin once, with detection and membership data 25ms
  [+] parses every script and contains no PowerShell 7-only conditional syntax 68ms

Running tests from 'C:\Users\Vincent\UnRealDash\scripts\tests\SmokePackage.Tests.ps1'
Describing Smoke packaging
  [+] previews complete Win64 Development packaging 509ms
  [+] previews complete Win64 Shipping packaging 500ms
  [+] previews complete Android Development packaging 500ms
  [+] previews complete Android Shipping packaging 502ms
  [+] previews RHI overrides differing only in the two booleans and uses separate archives 1s
  [+] requires -Rhi without prompting 210ms
  [+] stops package-windows on a failed doctor before UAT 473ms
  [+] stops package-android on a failed doctor before UAT 476ms
  [+] stops package-android on a failed doctor before UAT 483ms

Describing Real smoke packaging
Fake doctor PASS: android
  [+] captures exact Android ini bytes at UAT invocation and cleans up success 268ms
Fake doctor PASS: android
  [+] preserves UAT exit 7 and cleans up the Android ini 229ms
Fake doctor PASS: workstation
  [+] never creates an ini for Windows packaging 245ms
Fake doctor PASS: android
  [+] preserves a pre-existing ini and never invokes UAT 219ms
Fake doctor PASS: android
Fake doctor PASS: android
  [+] reports a cleanup failure and preserves an earlier UAT failure 476ms
Fake doctor PASS: android
Fake doctor PASS: android
  [+] propagates arbitrary UAT exit codes 3 then 0 460ms
Fake doctor PASS: android
  [+] refuses an archive containing ampersand without invoking UAT 230ms
Tests completed in 74.26s
Tests Passed: 75, Failed: 1, Skipped: 0, Inconclusive: 0, NotRun: 0
```

python -m pytest tools/tests -q --basetemp=.tmp/pytest-chunk10 (exit 0; parent scratch directory created first)

```text
.......................................s..........................       [100%]
65 passed, 1 skipped in 11.74s
```

python scripts/verify-chunk10-facts.py

```text
  PASS  exceptions and RTTI off
  PASS  links signal_core publicly, miniz privately
        (PackageReader.h resolved to runtime/UnRealDash/Source/DashboardSpec/Public/dashboard_spec/PackageReader.h)
  PASS  PackageReader::Read returns Error and nothing else
  PASS  exactly one reader branches on is_directory
  PASS  std::filesystem security calls are present
  PASS  Error carries code, pointer, message, and CodeName exists
  PASS  43 fixture cases
  PASS  7 accepted and 36 rejected
  PASS  17 carry archive_only
  PASS  cross-tab is 6 accepted of 26 both-form and 1 accepted of 17 archive-only
  PASS  the accepted archive-only case is windows-attributes-entry
  PASS  some cases carry a profile key
  PASS  ReadBounded is defined in BoundedParse.cpp
  PASS  ReadBounded uses std::ifstream
  PASS  ReadBounded has exactly the three known callers
  PASS  five schema files
  PASS  the CLI uses DASHBOARD_SCHEMA_DIR
  PASS  DASHBOARD_SCHEMA_DIR retains its known users plus the load tests
  PASS  Document exposes only Storage&
  PASS  UBT lists .c as a compiled extension at UEBuildModuleCPP.cs:3078
  PASS  UBT branches on .c at UEBuildModuleCPP.cs:3129
  PASS  miniz.c is present
  PASS  dashboard-spec preset is 'default'
  PASS  signal-core presets are 'default' and 'tsan'
  PASS  ARCHITECTURE.md line 22 forbids tools reading the runtime tree
  PASS  tools/dashboard_spec/schema.py:14 resolves packages/dashboard-spec/schema

  26 passed, 0 failed
exit=0
```

git -c safe.directory=C:/Users/Vincent/UnRealDash diff --check

```text
warning: in the working copy of '.github/workflows/signal-core.yml', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of '.github/workflows/spec-tools.yml', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'packages/dashboard-spec/CMakeLists.txt', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'packages/dashboard-spec/README.md', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'packages/dashboard-spec/tests/TestSupport.h', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'packages/dashboard-spec/tests/test_package_reader.cpp', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'packages/dashboard-spec/tests/test_schema.cpp', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'runtime/UnRealDash/Config/DefaultEngine.ini', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'runtime/UnRealDash/Source/UnRealDash/UnRealDash.Build.cs', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'runtime/UnRealDash/Source/UnRealDashCore/UnRealDashCore.Build.cs', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'runtime/UnRealDash/UnRealDash.uproject', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'scripts/README.md', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'tools/dashboard_spec/errors.py', CRLF will be replaced by LF the next time Git touches it
warning: in the working copy of 'tools/tests/test_semantic.py', CRLF will be replaced by LF the next time Git touches it
exit=0
```

git -c safe.directory=C:/Users/Vincent/UnRealDash status --short (before writing this report)

```text
warning: unable to access 'C:\Users\Vincent/.config/git/ignore': Permission denied
warning: unable to access 'C:\Users\Vincent/.config/git/ignore': Permission denied
 M .github/workflows/signal-core.yml
 M .github/workflows/spec-tools.yml
 M docs/build/chunk-10-package-loader.md
 M packages/dashboard-spec/CMakeLists.txt
 M packages/dashboard-spec/README.md
 D packages/dashboard-spec/include/dashboard_spec/DefinitionPackBuilder.h
 D packages/dashboard-spec/include/dashboard_spec/Document.h
 D packages/dashboard-spec/include/dashboard_spec/Errors.h
 D packages/dashboard-spec/include/dashboard_spec/Limits.h
 D packages/dashboard-spec/include/dashboard_spec/PackageReader.h
 D packages/dashboard-spec/include/dashboard_spec/RuleTreeBuilder.h
 D packages/dashboard-spec/include/dashboard_spec/SemanticPass.h
 D packages/dashboard-spec/include/dashboard_spec/Validator.h
 D packages/dashboard-spec/src/BoundedParse.cpp
 D packages/dashboard-spec/src/DefinitionPackBuilder.cpp
 D packages/dashboard-spec/src/Errors.cpp
 D packages/dashboard-spec/src/ImageHeader.cpp
 D packages/dashboard-spec/src/Internal.h
 D packages/dashboard-spec/src/PackageInternal.h
 D packages/dashboard-spec/src/PackageReader.cpp
 D packages/dashboard-spec/src/PathResolver.cpp
 D packages/dashboard-spec/src/RuleTreeBuilder.cpp
 D packages/dashboard-spec/src/SchemaValidation.cpp
 D packages/dashboard-spec/src/SemanticPass.cpp
 D packages/dashboard-spec/src/validate_main.cpp
 M packages/dashboard-spec/tests/TestSupport.h
 M packages/dashboard-spec/tests/test_package_reader.cpp
 M packages/dashboard-spec/tests/test_schema.cpp
 M runtime/UnRealDash/Config/DefaultEngine.ini
 M runtime/UnRealDash/Source/UnRealDash/UnRealDash.Build.cs
 M runtime/UnRealDash/Source/UnRealDashCore/UnRealDashCore.Build.cs
 M runtime/UnRealDash/UnRealDash.uproject
 M scripts/README.md
 M scripts/SignalCoreLayering.ps1
 M scripts/doctor.ps1
 M tools/dashboard_spec/errors.py
 M tools/tests/test_semantic.py
?? .tmp/
?? docs/reports/logs/chunk-10/
?? packages/dashboard-spec/tests/test_loaded_package.cpp
?? runtime/UnRealDash/Source/DashboardSpec/
?? runtime/UnRealDash/Source/UnRealDash/Private/Package/
?? runtime/UnRealDash/Source/UnRealDashCore/Private/Package/
?? runtime/UnRealDash/Source/UnRealDashCore/Public/UnRealDashCore/ComponentRegistry.h
?? runtime/UnRealDash/Source/UnRealDashCore/Public/UnRealDashCore/DashPackageLoader.h
?? scripts/doctest-summary.py
?? scripts/tests/dashboard-spec-layering.Tests.ps1
?? scripts/verify-chunk10-facts.py
```

Android standard-library result: the existing docs/reports/2026-09-17-android-stdlib-probe.md records successful ifstream, is_directory, symlink_status, hard_link_count, canonical, file_size and recursive iteration on the Pixel with error=0. This session did not repeat that device proof. The whole-library ARM64 build was attempted early, before Load and engine-wrapper work; the sandbox run never advanced beyond UBT startup and produced no compiler/linker result. Therefore whole-library Android linking is UNKNOWN, not failed and not passed. No replacement filesystem or linking workaround was introduced.

Schema delivery: DashboardSpec.Build.cs enumerates the five unchanged packages/dashboard-spec/schema/*.schema.json files, pins each destination to $(ProjectDir)/Schema/<filename>, and marks it StagedFileType.NonUFS. The wrapper joins FPaths::ProjectDir() and Schema, resolves it with IPlatformFile::GetPlatformPhysical().ConvertToAbsolutePathForExternalAppForRead, and logs it at startup. The device file listing and validator read at that resolved path remain unproven.

Not run or not completed:

- UBT Win64/Android compile and link completion: both stalled at UBT startup inside this sandbox. No compiler diagnostics or verified engine binary were produced.
- Linux hosted CMake and both hosted workflows: no Linux runner or remote CI execution in this session. Workflow source roots and XML reporting were updated, but remote green status is not claimed.
- Engine 43-case archive / 26-case directory commandlet: added, not executed because the updated engine binary could not be built here. The 17 archive-only cases are deliberate exclusions from directory parity, not skipped archive cases.
- Win64 launches for all 36 rejections and 7 acceptances, plus well-formed in both forms: not run on the new code. On-screen visibility, readability, crash behavior and actual widget rendering remain gates for Claude.
- Pixel/device half, schema listing, schema validator read and mobile rendering: phone unavailable by user instruction; criterion 8 is reserved for Claude.
- Unmodified full Pester pass: its required -CI invocation exited 80 because the sandbox cannot create Pester registry keys. With only Pester TestRegistry disabled, all 76 tests ran: 75 passed and the existing PreInstall test failed because the sandbox cannot open the user Environment registry key for writing. No tests were removed, weakened or skipped.

Deviations and reasons:

- std::filesystem is retained rather than injected. Its symlink status, canonical-path and hard-link-count checks are stronger than the architecture injection rule here, as the frozen spec explicitly requires.
- PLAN 3.4 names the former source location. The explicit chunk-10 source-move requirement and PLAN 4.2 arrangement now supply the shared source location; schema and production Python paths remain unchanged.
- The fact prose overstates ReadBounded coverage: existing ArchiveGate reads ZIP metadata directly with ifstream, and miniz reads archives. Those pre-existing security/ZIP paths were retained. The new lazy directory payload overload stays in ReadBounded and uses caller-owned nothrow storage.
- The source move used native filesystem moves because the sandbox denied .git/index.lock. No index write, commit or source copy was substituted.
- The CLI source remains in Private as required by the move, guarded by a library-owned UBT define so its main function and build-machine schema macro are absent from the engine build. The CMake CLI still compiles that sole file.
- UBT compiles miniz through a .c include wrapper pointing at the sole vendored implementation. The vendor implementation is neither copied nor modified.
- Compiler-only export annotations support modular Windows linkage without Unreal macros or headers in portable sources.
- Python test metadata checks follow the moved headers. Production Python tools still resolve only the original schema and fixture locations and import no runtime module.
- An intermediate XML run overlapped Python fixture regeneration and saw transient entry-count archive errors. The final sequential C++ run passed with zero failures. The initial pytest run also hit the default temp-directory permission denial; a workspace temp directory produced 65 passes and one existing skip.
- Proof-section references to no installed engine/toolchain and 42 on-screen rejections are stale. This machine has the installed engine/toolchain; sandbox startup limits prevented UBT proof. cases.json yields 36 rejection cases, which is the implemented gate.

Anything not possible: criteria 1 to 7 are not all proven, so this is not reported as "built, device half not run". It is not complete. Claude must rerun the advisory proofs and finish the engine, hosted Linux, unrestricted Pester and device gates. Temporary pytest artifacts remain under .tmp/pytest-chunk10: automatic approval review rejected their deletion with only "blocked by policy" as the stated reason. No permission escalation was attempted.
