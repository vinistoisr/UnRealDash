using UnrealBuildTool;
using System.IO;
public class UnRealDash : ModuleRules
{
    public UnRealDash(ReadOnlyTargetRules Target) : base(Target)
    {
        bUseUnity = false;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnRealDashCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "UMG", "Slate", "SlateCore", "Json", "ImageWrapper", "RenderCore", "RHI", "ApplicationCore", "ProceduralMeshComponent", "InputCore" });
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // Supplies org.gradle.java.home. See the comment in the file for why the JDK gradle
            // uses cannot be left to JAVA_HOME.
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "UnRealDash_UPL.xml"));
        }
    }
}
