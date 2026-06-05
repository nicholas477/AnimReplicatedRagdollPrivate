// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimReplicatedRagdollTypes.h"

#include "AnimReplicatedRagdollHelpers.h"
#include "AnimReplicatedRagdollStats.h"
#include "RRSkeletalMeshComponent.h"
#include "ReplicatedRagdollComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ReplicatedRagdollNetState.h"

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
	uint8 bHasSkeletalMeshComponentLocation : 1;

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
	FNetDeltaSerializeInfo& DeltaParms;
	const TMap<int32, FTransform>& ComponentSpaceTransforms;
	TMap<uint32, FVector>& BoneLocations;
	TMap<uint32, FQuat>& BoneRotations;
	const FReplicatedRagdollNetHeader& Header;
	const USkeletalMeshComponent* SkeletalMesh;
	const FReplicatedRagdollOptions& ReplicationOptions;
};

static void GatherBones(const FGatherBonesParams& Params)
{
	FReplicatedRagdollNetState* OldState = static_cast<FReplicatedRagdollNetState*>(Params.DeltaParms.OldState);

	TSharedPtr<FReplicatedRagdollNetState> NewState = nullptr;
	if (OldState)
	{
		// Copy the old state
		NewState = MakeShared<FReplicatedRagdollNetState>(*OldState);
	}
	else
	{
		NewState = MakeShared<FReplicatedRagdollNetState>();
	}
	*Params.DeltaParms.NewState = NewState;

	const EVectorQuantization LocationQuantization = static_cast<EVectorQuantization>(Params.Header.LocationQuantizationLevel);
	const ERotatorQuantization RotationQuantization = static_cast<ERotatorQuantization>(Params.Header.RotationQuantizationLevel);

	// Only replicate the bones that pass the ShouldReplicateBone filter
	for (const TPair<int32, FTransform>& RagdollTransform : Params.ComponentSpaceTransforms)
	{
		if (!ShouldReplicateBone(Params.SkeletalMesh, RagdollTransform.Key, Params.ReplicationOptions))
		{
			continue;
		}

		if (OldState == nullptr 
			|| OldState->ShouldUpdateBoneLocation(RagdollTransform.Key, RagdollTransform.Value.GetLocation(), LocationQuantization))
		{
			Params.BoneLocations.Add(RagdollTransform.Key, RagdollTransform.Value.GetLocation());
			NewState->UpdateBoneLocation(RagdollTransform.Key, RagdollTransform.Value.GetLocation(), LocationQuantization);
		}

		if (OldState == nullptr 
			|| OldState->ShouldUpdateBoneRotation(RagdollTransform.Key, RagdollTransform.Value.GetRotation().Rotator(), RotationQuantization))
		{
			Params.BoneRotations.Add(RagdollTransform.Key, RagdollTransform.Value.GetRotation());
			NewState->UpdateBoneRotation(RagdollTransform.Key, RagdollTransform.Value.GetRotation().Rotator(), RotationQuantization);
		}
	}
}

static void TrackReplicationStats(const FReplicatedRagdollNetHeader& Header, const TMap<uint32, FVector>& BoneLocations, const TMap<uint32, FQuat>& BoneRotations)
{
	// Track location stats based on quantization level
	switch (static_cast<EVectorQuantization>(Header.LocationQuantizationLevel))
	{
		case EVectorQuantization::RoundWholeNumber:
			INC_DWORD_STAT_BY(STAT_RagdollBoneLocations_RoundWholeNumber, BoneLocations.Num());
			break;
		case EVectorQuantization::RoundOneDecimal:
			INC_DWORD_STAT_BY(STAT_RagdollBoneLocations_RoundOneDecimal, BoneLocations.Num());
			break;
		case EVectorQuantization::RoundTwoDecimals:
			INC_DWORD_STAT_BY(STAT_RagdollBoneLocations_RoundTwoDecimals, BoneLocations.Num());
			break;
		default:
			break;
	}

	// Track rotation stats based on quantization level
	switch (static_cast<ERotatorQuantization>(Header.RotationQuantizationLevel))
	{
		case ERotatorQuantization::ByteComponents:
			INC_DWORD_STAT_BY(STAT_RagdollBoneRotations_ByteComponents, BoneRotations.Num());
			break;
		case ERotatorQuantization::ShortComponents:
			INC_DWORD_STAT_BY(STAT_RagdollBoneRotations_ShortComponents, BoneRotations.Num());
			break;
		default:
			break;
	}
}

bool FReplicatedRagdollData::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	SCOPED_NAMED_EVENT(FReplicatedRagdollData_NetDeltaSerialize, FColor::Magenta);

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
		Header.bHasSkeletalMeshComponentLocation = ReplicationOptions.bReplicateSkeletalMeshComponentLocation;

		GatherBones(FGatherBonesParams{
			DeltaParms,
			ComponentSpaceTransforms,
			BoneLocations,
			BoneRotations,
			Header,
			SkeletalMesh,
			ReplicationOptions
		});

		TrackReplicationStats(Header, BoneLocations, BoneRotations);
	}

	Archive->Serialize(&Header, sizeof(Header));

	if (Header.bHasSkeletalMeshComponentLocation)
	{
		check(IsInGameThread());

		if (DeltaParms.Writer)
		{
			FTransform SkeletalMeshComponentLocation = FTransform::Identity;
			if (const UReplicatedRagdollComponent* RagdollComponent = Cast<const UReplicatedRagdollComponent>(DeltaParms.Object))
			{
				if (const USkeletalMeshComponent* SkeletalMesh = RagdollComponent->GetSkeletalMesh())
				{
					SkeletalMeshComponentLocation = SkeletalMesh->GetComponentTransform();
				}
			}
			AnimReplicatedRagdollHelpers::QuantizeAndWriteLocation(*Archive, SkeletalMeshComponentLocation.GetLocation(), EVectorQuantization::RoundOneDecimal);
			AnimReplicatedRagdollHelpers::QuantizeAndWriteRotation(*Archive, SkeletalMeshComponentLocation.GetRotation().Rotator(), ERotatorQuantization::ShortComponents);
		}
		else
		{
			FVector Location;
			FRotator Rotation;
			AnimReplicatedRagdollHelpers::ReadAndDequantizeLocation(*Archive, Location, EVectorQuantization::RoundOneDecimal);
			AnimReplicatedRagdollHelpers::ReadAndDequantizeRotation(*Archive, Rotation, ERotatorQuantization::ShortComponents);

			if (UReplicatedRagdollComponent* RagdollComponent = Cast<UReplicatedRagdollComponent>(DeltaParms.Object))
			{
				if (RagdollComponent->ShouldApplyRagdoll())
				{
					if (USkeletalMeshComponent* SkeletalMesh = RagdollComponent->GetSkeletalMesh())
					{
						SkeletalMesh->SetWorldLocationAndRotation(Location, Rotation);
					}
				}
			}
		}
	}

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
