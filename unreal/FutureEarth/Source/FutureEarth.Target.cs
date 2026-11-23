using UnrealBuildTool;

public class FutureEarthTarget : TargetRules
{
	public FutureEarthTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("FutureEarth");
	}
}
