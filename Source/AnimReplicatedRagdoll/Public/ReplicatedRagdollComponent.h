// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AnimReplicatedRagdollTypes.h"
#include "AnimReplicatedRagdollHelpers.h"
#include "ReplicatedRagdollComponent.generated.h"

class UReplicatedRagdollComponent;
class FReplicatedRagdollNetState;

struct ANIMREPLICATEDRAGDOLL_API FRagdollAnimData
{
	FReplicatedRagdollData ReadCurrentRagdollData() const
	{
		FReadScopeLock Lock(DataLock);
		return CurrentData;
	}

	void WriteCurrentRagdollData(const FReplicatedRagdollData& NewData)
	{
		FWriteScopeLock Lock(DataLock);
		CurrentData = NewData;
	}

	FReplicatedRagdollData ReadRagdollData() const
	{
		FReadScopeLock Lock(DataLock);
		return Data;
	}

	void WriteRagdollData(const FReplicatedRagdollData& NewData)
	{
		FWriteScopeLock Lock(DataLock);
		Data = NewData;
	}

	bool ReadEvaluateAnimation() const
	{
		FReadScopeLock Lock(DataLock);
		return bEvaluateAnimation;
	}

	void WriteEvaluateAnimation(bool bNewEvaluateAnimation)
	{
		FWriteScopeLock Lock(DataLock);
		bEvaluateAnimation = bNewEvaluateAnimation;
	}

	FReplicatedRagdollData GetInterpedRagdollData(float DeltaTime, float InterpSpeed) const;

protected:
	mutable FRWLock DataLock;

	// This is what the ragdoll is CURRENTLY at on the client side. This is written in pre-update.
	FReplicatedRagdollData CurrentData;

	// This is the target data that we are interping towards. This is replicated from the server to the client.
	FReplicatedRagdollData Data;

	bool bEvaluateAnimation;
};

UCLASS( ClassGroup=(Ragdoll), meta = (BlueprintSpawnableComponent))
class ANIMREPLICATEDRAGDOLL_API UReplicatedRagdollComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	UReplicatedRagdollComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void InitializeComponent() override;
	virtual void Serialize(FArchive& Ar) override;

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	virtual USkeletalMeshComponent* GetSkeletalMesh() const;

	// Empties the ragdoll data
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void ClearRagdoll();

	// Reads the bone transforms from the skeletal mesh and copies them into AnimData.
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void CaptureRagdoll();

	// Sets the skeletal mesh bone transforms to the bone transforms in AnimData.
	//
	// This only works if the skeletal mesh is in simulate physics mode.
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void ApplyRagdoll();

	// Called on tick on the server to decide if the ragdoll should be captured
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ragdoll")
	bool ShouldCaptureRagdoll();

	// Called on tick to decide if the ragdoll animation node should apply the replicated
	// bone data.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ragdoll")
	bool ShouldApplyRagdoll();

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void SimulateNonReplicatedBones() const;

	TSharedPtr<FRagdollAnimData> GetAnimDataHandle() const { return AnimDataHandle; }

	FReplicatedRagdollOptions GetReplicationOptions() const { return ReplicationOptions; };


	struct FSerializedAnimData
	{
		TSharedPtr<FReplicatedRagdollNetState> NewNetState;
		TSharedPtr<TArray<uint8>> SerializedData;
		int64 NumBitsWritten;
		int64 ReplicationKey = -1;
	};
	bool GetSerializedAnimData(const UNetConnection* Connection, FSerializedAnimData& OutAnimData) const;
	void UpdateNetState(const UNetConnection* Connection, TSharedPtr<FReplicatedRagdollNetState> NewState) const
	{
		NetState.Add(Connection, NewState);
	}

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ragdoll", ReplicatedUsing=OnRep_AnimData)
	FReplicatedRagdollData AnimData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ragdoll")
	FReplicatedRagdollOptions ReplicationOptions;

	// This is the anim data that the animation system actually uses.
	TSharedPtr<FRagdollAnimData> AnimDataHandle;

	UFUNCTION()
	void OnRep_AnimData();

#if WITH_EDITOR
	mutable AnimReplicatedRagdollHelpers::FPIEEditorErrorFlag AttachParentFlag;
	mutable AnimReplicatedRagdollHelpers::FPIEEditorErrorFlag IsReplicatedFlag;
	mutable AnimReplicatedRagdollHelpers::FPIEEditorErrorFlag AnimInstanceFlag;
	mutable AnimReplicatedRagdollHelpers::FPIEEditorErrorFlag OwnerIsReplicatedFlag;
	mutable AnimReplicatedRagdollHelpers::FPIEEditorErrorFlag TickOptionFlag;
	mutable AnimReplicatedRagdollHelpers::FPIEEditorErrorFlag FoundAnimNodeFlag;
	virtual void CheckRequirements() const;
#endif

	void KickoffAnimDataSerialization();
	mutable TMap<const UNetConnection*, TSharedPtr<FReplicatedRagdollNetState>> NetState;

	struct FSerializedAnimDataMap
	{
		mutable FRWLock Lock;
		TMap<const UNetConnection*, FSerializedAnimData> Map;
	};
	TSharedPtr<FSerializedAnimDataMap> SerializedAnimData;
};
