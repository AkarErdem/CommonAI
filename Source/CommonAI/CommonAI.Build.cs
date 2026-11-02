using UnrealBuildTool;

public class CommonAI : ModuleRules
{
    public CommonAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Consumers use only engine and CommonAI contracts; the vendor remains an implementation detail.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "AIModule",
                "GameplayTags"
            }
        );
        PrivateDependencyModuleNames.Add("AthenaAI");
    }
}
