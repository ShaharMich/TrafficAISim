// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TrafficAISim : ModuleRules
{
	public TrafficAISim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"Chaos",
			"UMG",
			"Slate",
			"AIModule"
		});

		PublicIncludePaths.AddRange(new string[] {
			"TrafficAISim",
			"TrafficAISim/SportsCar",
			"TrafficAISim/OffroadCar",
			"TrafficAISim/Variant_Offroad",
			"TrafficAISim/Variant_TimeTrial",
			"TrafficAISim/Variant_TimeTrial/UI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
