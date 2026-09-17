using UnrealBuildTool;
public class UnRealDashTarget : TargetRules
{
    public UnRealDashTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        CppStandard = CppStandardVersion.Cpp20;
        // Shipping keeps the engine's default logging. Turning logging on in Shipping changes the
        // engine build environment, which a Launcher install cannot provide, and the only way past
        // that check is bOverrideBuildEnvironment, which links project code against engine binaries
        // built with different settings. PLAN 4.0's Shipping requirement does not need logs: a
        // Shipping build proves it read player.json by writing its exports to the distinctive
        // output_directory that file names, with no command line to have supplied it.
        ExtraModuleNames.AddRange(new[] { "UnRealDash" });
    }
}

