using UnrealBuildTool;

public class GlazeEditor : ModuleRules
{
	public GlazeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"UnrealEd", "MaterialEditor", "AssetTools", "AssetRegistry", "ContentBrowser",
			"ToolMenus", "Slate", "SlateCore", "InputCore", "PropertyEditor", "EditorFramework",
			"Json", "JsonUtilities", "Projects"
		});
	}
}
