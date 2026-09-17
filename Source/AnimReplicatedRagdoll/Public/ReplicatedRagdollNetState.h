// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreNet.h"
#include "Misc/Optional.h"
#include "Engine/ReplicatedState.h"

/** Custom INetDeltaBaseState used by ragdoll component for keeping track of client state */
class FReplicatedRagdollNetState : public INetDeltaBaseState
{
public:
	FReplicatedRagdollNetState()
		: BoneData()
	{
	}

	bool ShouldUpdateBoneLocation(int32 BoneIndex, const FVector& NewLocation, EVectorQuantization QuantizationLevel) const;
	bool ShouldUpdateBoneRotation(int32 BoneIndex, const FRotator& NewRotation, ERotatorQuantization QuantizationLevel) const;

	void UpdateBoneLocation(int32 BoneIndex, const FVector& NewLocation, EVectorQuantization QuantizationLevel);
	void UpdateBoneRotation(int32 BoneIndex, const FRotator& NewRotaton, ERotatorQuantization QuantizationLevel);

	void RemoveBones(const TArray<uint32>& BoneIndices);

	virtual bool IsStateEqual(INetDeltaBaseState* Other) override;

	struct FReplicatedBoneData
	{
		bool bHasLocationData = false;
		EVectorQuantization LocationQuantizationLevel;
		FVector LastLocation;

		bool bHasRotationData = false;
		ERotatorQuantization RotationQuantizationLevel;
		FRotator LastRotation;

		bool operator==(const FReplicatedBoneData& Other) const
		{
			return FPlatformMemory::Memcmp(this, &Other, sizeof(FReplicatedBoneData)) == 0;
		}
	};

	const TMap<int32, FReplicatedBoneData>& GetBoneData() const
	{
		return BoneData;
	}
protected:
	TMap<int32, FReplicatedBoneData> BoneData;
};