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
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("_HAS_EXCEPTIONS=0");
        }
        PublicDependencyModuleNames.Add("Core");
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
    }
}
