// Fill out your copyright notice in the Description page of Project Settings.


#include "ReplicatedRagdollNetState.h"

#include "AnimReplicatedRagdollHelpers.h"

static FVector QuantizeLocation(const FVector& InLocation, EVectorQuantization QuantizationLevel)
{
	TArray<uint8> QuantizedLocationData;
	{
		FMemoryWriter Writer(QuantizedLocationData);

		AnimReplicatedRagdollHelpers::QuantizeAndWriteLocation(Writer, InLocation, QuantizationLevel);

		Writer.Close();
	}

	{
		FMemoryReader Reader(QuantizedLocationData);

		FVector OutVector;
		AnimReplicatedRagdollHelpers::ReadAndDequantizeLocation(Reader, OutVector, QuantizationLevel);
		Reader.Close();
		return OutVector;
	}
}

static FRotator QuantizeRotation(const FRotator& InRotation, ERotatorQuantization QuantizationLevel)
{
	TArray<uint8> QuantizedRotationData;
	{
		FMemoryWriter Writer(QuantizedRotationData);

		AnimReplicatedRagdollHelpers::QuantizeAndWriteRotation(Writer, InRotation, QuantizationLevel);
		Writer.Close();
	}

	{
		FMemoryReader Reader(QuantizedRotationData);

		FRotator OutRotation;
		AnimReplicatedRagdollHelpers::ReadAndDequantizeRotation(Reader, OutRotation, QuantizationLevel);
		Reader.Close();
		return OutRotation;
	}
}

bool FReplicatedRagdollNetState::ShouldUpdateBoneLocation(int32 BoneIndex, const FVector& NewLocation, EVectorQuantization QuantizationLevel) const
{
	const FReplicatedBoneData* ExistingBoneData = BoneData.Find(BoneIndex);
	if (!ExistingBoneData)
	{
		return true;
	}

	if (!ExistingBoneData->bHasLocationData)
	{
		return true;
	}

	const FVector QuantizedNewLocation = QuantizeLocation(NewLocation, QuantizationLevel);
	return QuantizedNewLocation != ExistingBoneData->LastLocation;
}

bool FReplicatedRagdollNetState::ShouldUpdateBoneRotation(int32 BoneIndex, const FRotator& NewRotation, ERotatorQuantization QuantizationLevel) const
{
	const FReplicatedBoneData* ExistingBoneData = BoneData.Find(BoneIndex);
	if (!ExistingBoneData)
	{
		return true;
	}

	if (!ExistingBoneData->bHasRotationData)
	{
		return true;
	}

	const FRotator QuantizedNewRotation = QuantizeRotation(NewRotation, QuantizationLevel);
	return QuantizedNewRotation != ExistingBoneData->LastRotation;
}

void FReplicatedRagdollNetState::UpdateBoneLocation(int32 BoneIndex, const FVector& NewLocation, EVectorQuantization QuantizationLevel)
{
	FReplicatedBoneData& NewBoneData = BoneData.FindOrAdd(BoneIndex);
	NewBoneData.bHasLocationData = true;
	NewBoneData.LocationQuantizationLevel = QuantizationLevel;
	NewBoneData.LastLocation = QuantizeLocation(NewLocation, QuantizationLevel);
}

void FReplicatedRagdollNetState::UpdateBoneRotation(int32 BoneIndex, const FRotator& NewRotaton, ERotatorQuantization QuantizationLevel)
{
	FReplicatedBoneData& NewBoneData = BoneData.FindOrAdd(BoneIndex);
	NewBoneData.bHasRotationData = true;
	NewBoneData.RotationQuantizationLevel = QuantizationLevel;
	NewBoneData.LastRotation = QuantizeRotation(NewRotaton, QuantizationLevel);
}

void FReplicatedRagdollNetState::RemoveBones(const TArray<uint32>& BoneIndices)
{
	for (uint32 BoneIndex : BoneIndices)
	{
		BoneData.Remove(BoneIndex);
	}
}

bool FReplicatedRagdollNetState::IsStateEqual(INetDeltaBaseState* Other)
{
	FReplicatedRagdollNetState* OtherState = static_cast<FReplicatedRagdollNetState*>(Other);

	for (auto It = BoneData.CreateIterator(); It; ++It)
	{
		auto Ptr = OtherState->BoneData.Find(It.Key());
		if (!Ptr || *Ptr != It.Value())
		{
			return false;
		}
	}
	return true;
}
