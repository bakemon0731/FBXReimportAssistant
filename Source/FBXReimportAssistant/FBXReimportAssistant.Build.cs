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
				// Unreal Editor
				"UnrealEd",

				// UI
				"Slate",
				"SlateCore",
				"ToolMenus",

				// Content Browser
				"ContentBrowser",

				// Asset Registry
				"AssetRegistry",

				// Interchange
				"InterchangeEngine",
				"InterchangePipelines"
			}
		);
	}
}