// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RoadOfDangun : ModuleRules
{
	public RoadOfDangun(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput"});

		// RoadOfDangun_API 매크로 정의
		PublicDefinitions.Add("RoadOfDangun_API=");
    }
}
