using UnrealBuildTool;

public class FutureEarth : ModuleRules
{
	public FutureEarth(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new[] { "HTTP", "Json", "Slate", "SlateCore" });
	}
}
