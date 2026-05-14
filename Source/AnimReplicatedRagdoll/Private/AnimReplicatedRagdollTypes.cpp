// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimReplicatedRagdollTypes.h"

#include "AnimReplicatedRagdollHelpers.h"
#include "RRSkeletalMeshComponent.h"
#include "ReplicatedRagdollComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Net/Core/Serialization/QuantizedVectorSerialization.h"

UE_DISABLE_OPTIMIZATION

void FReplicatedRagdollData::CapturePose(const USkeletalMeshComponent* SkeletalMesh)
{
	check(SkeletalMesh != nullptr);
	const TArray<FTransform>& BoneTransforms = SkeletalMesh->GetComponentSpaceTransforms();

	for (int32 i = 0; i < BoneTransforms.Num(); ++i)
	{
		ComponentSpaceTransforms.Add(i, BoneTransforms[i]);
	}
}

void FReplicatedRagdollData::ApplyPose(USkeletalMeshComponent* SkeletalMesh)
{
	check(SkeletalMesh != nullptr);
	TArray<FTransform>& BoneTransforms = SkeletalMesh->GetEditableComponentSpaceTransforms();

	for (const TPair<int32, FTransform>& Pair : ComponentSpaceTransforms)
	{
		if (BoneTransforms.IsValidIndex(Pair.Key))
		{
			BoneTransforms[Pair.Key] = Pair.Value;
		}
	}
	((URRSkeletalMeshComponent*)SkeletalMesh)->ApplyEditedComponentSpaceTransforms();
}

// The size of the unsigned integer for bone indicies.
// // This is used to save bandwidth when replicating bone transforms by using a smaller integer type for the bone index when possible.
enum class EBoneIndexFormat : uint8
{
	Byte,
	Short
};

struct FReplicatedRagdollNetHeader
{
	uint8 BoneIndexFormat : 1;
	uint8 LocationQuantizationLevel : 2;
	uint8 RotationQuantizationLevel : 2;

	EVectorQuantization GetLocationQuantization() const
	{
		return static_cast<EVectorQuantization>(LocationQuantizationLevel);
	}

	ERotatorQuantization GetRotationQuantization() const
	{
		return static_cast<ERotatorQuantization>(RotationQuantizationLevel);
	}

	uint32 GetBoneIndexFormatSize() const
	{
		switch (BoneIndexFormat)
		{
		case static_cast<uint8>(EBoneIndexFormat::Byte):
			return sizeof(uint8);
		case static_cast<uint8>(EBoneIndexFormat::Short):
			return sizeof(uint16);
		default:
			return 0;
		}
	}
};

static_assert(sizeof(FReplicatedRagdollNetHeader) == 1, "FReplicatedRagdollNetHeader should be 1 byte across all platforms");

bool ShouldReplicateBone(const USkeletalMeshComponent* SkeletalMesh, int32 BoneIndex, const FReplicatedRagdollOptions& Options)
{
	if (!SkeletalMesh)
	{
		return true;
	}

	const FName BoneName = SkeletalMesh->GetBoneName(BoneIndex);

	if (Options.BoneFilterType == EReplicatedBoneFilterType::AllowList)
	{
		for (const FName& AllowedBoneIndex : Options.BoneAllowList)
		{
			if (BoneName == AllowedBoneIndex || SkeletalMesh->BoneIsChildOf(AllowedBoneIndex, BoneName))
			{
				return true;
			}
		}

		return false;
	}
	else
	{
		for (const FName& DeniedBoneIndex : Options.BoneDenyList)
		{
			if (BoneName == DeniedBoneIndex || SkeletalMesh->BoneIsChildOf(BoneName, DeniedBoneIndex))
			{
				return false;
			}
		}

		return true;
	}
}

static void SerializeBones(TMap<uint32, FVector>& Locations, TMap<uint32, FQuat>& Rotations, FBitArchive& Archive, FReplicatedRagdollNetHeader& Header)
{
	const size_t BoneIndexSize = Header.GetBoneIndexFormatSize();
	uint32 NumLocationUpdates = Locations.Num();
	uint32 NumRotationUpdates = Rotations.Num();

	Archive.Serialize((void*)&NumLocationUpdates, BoneIndexSize);
	Archive.Serialize((void*)&NumRotationUpdates, BoneIndexSize);

	if (Archive.IsLoading())
	{
		for (uint32 i = 0; i < NumLocationUpdates; ++i)
		{
			uint32 BoneIndex = 0;
			FVector Location;
			Archive.Serialize((void*)&BoneIndex, BoneIndexSize);
			AnimReplicatedRagdollHelpers::ReadAndDequantizeLocation(Archive, Location, Header.GetLocationQuantization());

			Locations.Add(BoneIndex, Location);
		}

		for (uint32 i = 0; i < NumRotationUpdates; ++i)
		{
			uint32 BoneIndex = 0;
			FRotator Rotation;
			Archive.Serialize((void*)&BoneIndex, BoneIndexSize);
			AnimReplicatedRagdollHelpers::ReadAndDequantizeRotation(Archive, Rotation, Header.GetRotationQuantization());
			Rotations.Add(BoneIndex, Rotation.Quaternion());
		}
	}
	else if (Archive.IsSaving())
	{
		for (const TPair<uint32, FVector>& Pair : Locations)
		{
			Archive.Serialize((void*)&Pair.Key, BoneIndexSize);
			AnimReplicatedRagdollHelpers::QuantizeAndWriteLocation(Archive, Pair.Value, Header.GetLocationQuantization());
		}

		for (const TPair<uint32, FQuat>& Pair : Rotations)
		{
			Archive.Serialize((void*)&Pair.Key, BoneIndexSize);
			AnimReplicatedRagdollHelpers::QuantizeAndWriteRotation(Archive, Pair.Value.Rotator(), Header.GetRotationQuantization());
		}
	}
	else
	{
		checkNoEntry();
	}
}

struct FGatherBonesParams
{
	const TMap<int32, FTransform>& ComponentSpaceTransforms;
	TMap<uint32, FVector>& BoneLocations;
	TMap<uint32, FQuat>& BoneRotations;
	const FReplicatedRagdollNetHeader& Header;
	const USkeletalMeshComponent* SkeletalMesh;
	const FReplicatedRagdollOptions& ReplicationOptions;
};

static void GatherBones(const FGatherBonesParams& Params)
{
	// Only replicate the bones that pass the ShouldReplicateBone filter
	for (const TPair<int32, FTransform>& RagdollTransform : Params.ComponentSpaceTransforms)
	{
		if (ShouldReplicateBone(Params.SkeletalMesh, RagdollTransform.Key, Params.ReplicationOptions))
		{
			Params.BoneLocations.Add(RagdollTransform.Key, RagdollTransform.Value.GetLocation());
			Params.BoneRotations.Add(RagdollTransform.Key, RagdollTransform.Value.GetRotation());
		}
	}
}

bool FReplicatedRagdollData::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	FBitArchive* Archive = DeltaParms.Reader ? static_cast<FBitArchive*>(DeltaParms.Reader) : static_cast<FBitArchive*>(DeltaParms.Writer);
	if (Archive == nullptr)
	{
		return false;
	}

	FReplicatedRagdollNetHeader Header;
	TMap<uint32, FVector> BoneLocations;
	TMap<uint32, FQuat> BoneRotations;

	if (DeltaParms.Writer)
	{
		const USkeletalMeshComponent* SkeletalMesh = nullptr;
		FReplicatedRagdollOptions ReplicationOptions;
		if (const UReplicatedRagdollComponent* RagdollComponent = Cast<const UReplicatedRagdollComponent>(DeltaParms.Object))
		{
			ReplicationOptions = RagdollComponent->GetReplicationOptions();
			SkeletalMesh = RagdollComponent->GetSkeletalMesh();
		}
		Header.BoneIndexFormat = GetMaxBoneIndex() < 256 ? static_cast<uint8>(EBoneIndexFormat::Byte) : static_cast<uint8>(EBoneIndexFormat::Short);
		Header.LocationQuantizationLevel = static_cast<uint8>(ReplicationOptions.LocationQuantizationLevel);
		Header.RotationQuantizationLevel = static_cast<uint8>(ReplicationOptions.RotationQuantizationLevel);

		GatherBones(FGatherBonesParams{
			ComponentSpaceTransforms,
			BoneLocations,
			BoneRotations,
			Header,
			SkeletalMesh,
			ReplicationOptions
		});
	}

	Archive->Serialize(&Header, sizeof(Header));
	SerializeBones(BoneLocations, BoneRotations, *Archive, Header);

	if (DeltaParms.Reader)
	{
		for (const TPair<uint32, FVector>& BoneLocation : BoneLocations)
		{
			FTransform& Transform = ComponentSpaceTransforms.FindOrAdd(BoneLocation.Key);
			Transform.SetLocation(BoneLocation.Value);
		}
		
		for (const TPair<uint32, FQuat>& BoneRotation : BoneRotations)
		{
			FTransform& Transform = ComponentSpaceTransforms.FindOrAdd(BoneRotation.Key);
			Transform.SetRotation(BoneRotation.Value);
		}
	}

	return true;
}

UE_ENABLE_OPTIMIZATION
