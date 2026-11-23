using UnrealBuildTool;

public class FutureEarthEditorTarget : TargetRules
{
	public FutureEarthEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("FutureEarth");
	}
}
