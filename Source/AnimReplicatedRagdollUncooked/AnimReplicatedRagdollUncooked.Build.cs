// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimReplicatedRagdollUncooked : ModuleRules
{
    public AnimReplicatedRagdollUncooked(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AnimGraph",
                "BlueprintGraph",
                "AnimReplicatedRagdoll",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
            }
        );
    }
}
