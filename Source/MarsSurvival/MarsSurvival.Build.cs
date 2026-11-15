using UnrealBuildTool;

public class MarsSurvival : ModuleRules
{
	public MarsSurvival(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Allow includes like "Base/MarsBaseModule.h" from anywhere in the module.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"HTTP",
			"Json",
			"JsonUtilities",
			"ProceduralMeshComponent",
			"UMG",
			"Slate",
			"SlateCore",
			"AudioMixer"
		});
	}
}
