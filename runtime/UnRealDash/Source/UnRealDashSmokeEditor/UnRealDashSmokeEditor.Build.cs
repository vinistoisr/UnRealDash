using UnrealBuildTool;
public class UnRealDashSmokeEditor : ModuleRules
{
    public UnRealDashSmokeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        bUseUnity = false;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "RenderCore", "RHI" });
    }
}

