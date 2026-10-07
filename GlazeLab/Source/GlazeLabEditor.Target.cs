using UnrealBuildTool;

public class GlazeLabEditorTarget : TargetRules
{
	public GlazeLabEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GlazeLab");
	}
}
