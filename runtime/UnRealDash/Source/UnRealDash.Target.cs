using UnrealBuildTool;
public class UnRealDashTarget : TargetRules
{
    public UnRealDashTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        CppStandard = CppStandardVersion.Cpp20;
        ExtraModuleNames.AddRange(new[] { "UnRealDash" });
    }
}
