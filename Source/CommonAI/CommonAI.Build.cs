using UnrealBuildTool;

public class CommonAI : ModuleRules
{
    public CommonAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Public API dependencies for Athena-derived classes, Unreal AI types and gameplay tags.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "AIModule",
                "GameplayTags",
                "AthenaAI"
            }
        );
    }
}