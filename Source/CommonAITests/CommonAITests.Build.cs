using UnrealBuildTool;

public class CommonAITests : ModuleRules
{
    public CommonAITests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "CommonAI" });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AIModule", "GameplayTags", "GameplayTasks", "NavigationSystem", "SmartObjectsModule", "AthenaAI",
            "UMG", "Slate", "SlateCore", "Projects"
        });
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AnimationBlueprintLibrary" });
        }
    }
}
