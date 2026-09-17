using UnrealBuildTool;
using System.IO;
public class SignalCore : ModuleRules
{
    public SignalCore(ReadOnlyTargetRules Target) : base(Target)
    {
        CppStandard = CppStandardVersion.Cpp20;
        bUseUnity = false;
        bEnableExceptions = false;
        bUseRTTI = false;
        FPSemantics = FPSemanticsMode.Precise;
        // Engine floating-point behavior is unverified until PLAN.md 4.2's in-engine
        // commandlet compares scenario output with CMake, including contraction behavior.
        // Do not define _HAS_EXCEPTIONS here. UnrealBuildTool already sets it from bEnableExceptions,
        // and defining it again both warns (C4005) and breaks the standard headers this module's
        // consumers include: <stdexcept> compiled against a conflicting value loses _RAISE and the
        // _Doraise overrides. The CMake build sets it for its own translation units instead.
        PublicDependencyModuleNames.Add("Core");
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
    }
}
