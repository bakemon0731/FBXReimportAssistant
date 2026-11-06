
using UnrealBuildTool;

public class FBXReimportAssistant : ModuleRules
{
    public FBXReimportAssistant(ReadOnlyTargetRules Target)
        : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "UnrealEd",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "ContentBrowser"
            }
        );
    }
}
