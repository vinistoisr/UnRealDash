using UnrealBuildTool;
public class UnRealDash : ModuleRules
{
    public UnRealDash(ReadOnlyTargetRules Target) : base(Target)
    {
        bUseUnity = false;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnRealDashCore" });
    }
}
