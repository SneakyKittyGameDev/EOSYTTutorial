// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EOSYTTutorial : ModuleRules
{
	public EOSYTTutorial(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "OnlineSubsystem", "OnlineSubsystemEOS" });
	}
}
