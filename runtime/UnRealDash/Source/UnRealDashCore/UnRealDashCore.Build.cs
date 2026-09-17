using UnrealBuildTool;
public class UnRealDashCore : ModuleRules
{
    public UnRealDashCore(ReadOnlyTargetRules Target) : base(Target)
    {
        bUseUnity = false;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "SignalCore", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "CoreUObject", "Engine", "DashboardSpec", "Json", "Slate", "SlateCore" });
    }
}
