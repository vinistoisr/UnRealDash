using UnrealBuildTool;
using System.IO;
public class DashboardSpec : ModuleRules
{
    public DashboardSpec(ReadOnlyTargetRules Target) : base(Target)
    {
        CppStandard = CppStandardVersion.Cpp20;
        bUseUnity = false;
        if (Target.LinkType == TargetLinkType.Modular)
        {
            PublicDefinitions.Add("DASHBOARD_SPEC_SHARED=1");
            PrivateDefinitions.Add("DASHBOARD_SPEC_BUILD=1");
        }
        bEnableExceptions = false;
        bUseRTTI = false;
        // No PCHUsage override, matching SignalCore. With NoPCHs nothing force-includes
        // HAL/Platform.h, so UBT's SIGNALCORE_API=DLLIMPORT expands to an undefined token and
        // every signal-core header fails to parse in a modular (editor) build. The monolithic
        // game target hides this because the API macros are empty there.
        PublicDependencyModuleNames.Add("SignalCore");
        PrivateDependencyModuleNames.Add("Core");
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        string Repo = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../.."));
        PublicSystemIncludePaths.AddRange(new[] {
            Path.Combine(Repo, "third_party/rapidjson/include"),
            Path.Combine(Repo, "third_party/valijson/include"),
            Path.Combine(Repo, "third_party/miniz") });
        PrivateDefinitions.AddRange(new[] { "VALIJSON_USE_EXCEPTIONS=0", "RAPIDJSON_HAS_STDSTRING=1", "DASHBOARD_SPEC_UBT=1" });
        foreach (string Schema in Directory.GetFiles(Path.Combine(Repo, "packages/dashboard-spec/schema"), "*.schema.json"))
            RuntimeDependencies.Add("$(ProjectDir)/Schema/" + Path.GetFileName(Schema), Schema, StagedFileType.NonUFS);
    }
}
