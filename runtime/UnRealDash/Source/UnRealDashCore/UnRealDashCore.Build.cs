using UnrealBuildTool;
public class UnRealDashCore : ModuleRules
{
    public UnRealDashCore(ReadOnlyTargetRules Target) : base(Target)
    {
        bUseUnity = false;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "SignalCore", "UMG" });
        // ImageWrapper, RenderCore and RHI arrive with the image primitive: it decodes a package PNG
        // into a transient texture through the path PLAN 4.0 proved on the device.
        PrivateDependencyModuleNames.AddRange(new[] { "CoreUObject", "Engine", "DashboardSpec", "Json", "Slate", "SlateCore", "ImageWrapper", "RenderCore", "RHI" });
    }
}
