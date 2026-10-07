using UnrealBuildTool;

public class GlazeLabTarget : TargetRules
{
	public GlazeLabTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GlazeLab");
	}
}
