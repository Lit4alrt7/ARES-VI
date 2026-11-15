using UnrealBuildTool;
using System.Collections.Generic;

public class MarsSurvivalTarget : TargetRules
{
	public MarsSurvivalTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MarsSurvival");

		// UE 5.3 does not support MSVC 14.5x (VS 2026). Pin the VS 2022 toolset.
		WindowsPlatform.CompilerVersion = "14.44.35207";
	}
}
