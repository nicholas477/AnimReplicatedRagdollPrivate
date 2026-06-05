// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AnimReplicatedRagdollSettings.generated.h"

/**
 * 
 */
UCLASS(config = Editor, defaultconfig)
class ANIMREPLICATEDRAGDOLL_API UAnimReplicatedRagdollSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	// Log an error when a replicated ragdoll component isn't attached to a skeletal mesh component.
	UPROPERTY(Config, EditAnywhere, Category = "Errors|Replicated Ragdoll Component")
	bool bLogAttachParentErrors = true;

	// Log an error when the replication settings aren't correct for a replicated ragdoll component.
	UPROPERTY(Config, EditAnywhere, Category = "Errors|Replicated Ragdoll Component")
	bool bLogReplicationErrors = true;

	// Log an error when there's no anim instance on the client to play back the replicated ragdoll pose.
	UPROPERTY(Config, EditAnywhere, Category = "Errors|Replicated Ragdoll Component")
	bool bLogAnimInstanceErrors = true;

	// Log an error when there's no replicated ragdoll node inside the skeletal mesh's anim graph to play back the replicated ragdoll pose on the client.
	UPROPERTY(Config, EditAnywhere, Category = "Errors|Replicated Ragdoll Component")
	bool bLogAnimNodeErrors = true;

	// Log an error when the skeletal mesh does not have the proper tick visibility settings to capture the ragdoll pose on dedicated servers.
	UPROPERTY(Config, EditAnywhere, Category = "Errors|Replicated Ragdoll Component")
	bool bLogSkeletalMeshTickVisibilityErrors = true;

	// Log an error when the replicated ragdoll anim node tries to read bone data for a bone index that is out of bounds of the current skeletal mesh's reference skeleton.
	UPROPERTY(Config, EditAnywhere, Category = "Errors|Replicated Ragdoll Anim Node")
	bool bLogInvalidBoneIndexErrors = true;

	// Log an error when the client is simulating physics for a bone that the replicated ragdoll anim node has data for.
	UPROPERTY(Config, EditAnywhere, Category = "Errors|Replicated Ragdoll Anim Node")
	bool bLogSimulatedBoneErrors = true;
};
