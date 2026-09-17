using UnrealBuildTool;
public class UnRealDashEditorTarget : TargetRules
{
    public UnRealDashEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        CppStandard = CppStandardVersion.Cpp20;
        ExtraModuleNames.AddRange(new[] { "UnRealDash", "UnRealDashSmokeEditor" });
    }
}

