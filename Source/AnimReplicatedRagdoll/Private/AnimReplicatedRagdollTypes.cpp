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
	
	Mesh = SkeletalMesh->GetSkeletalMeshAsset();

	ReplicationKey++;
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

	ReplicationKey++;
}

static_assert(sizeof(FReplicatedRagdollNetHeader) == 1, "FReplicatedRagdollNetHeader should be 1 byte across all platforms");

static bool ShouldReplicateBone(const USkeletalMeshComponent* SkeletalMesh, int32 BoneIndex, const FReplicatedRagdollOptions& Options)
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

void FReplicatedRagdollData::SerializeBones(TMap<uint32, FVector>& Locations, TMap<uint32, FQuat>& Rotations, TArray<uint32>& DeletedBones, FArchive& Archive, FReplicatedRagdollNetHeader& Header)
{
	SCOPED_NAMED_EVENT(ReplicatedRagdollComponent_NetworkEncode_SerializeBones, FColor::Orange);

	const size_t BoneIndexBitSize = Header.GetNumBitsForBoneIndex();
	uint32 NumLocationUpdates = Locations.Num();
	uint32 NumRotationUpdates = Rotations.Num();
	uint32 NumBonesDeleted = DeletedBones.Num();

	Header.CheckNumBitsSufficientForNum(NumLocationUpdates);
	Header.CheckNumBitsSufficientForNum(NumRotationUpdates);
	/*Header.CheckNumBitsSufficientForNum(NumBonesDeleted);*/

	Archive.SerializeBits((void*)&NumLocationUpdates, BoneIndexBitSize);
	Archive.SerializeBits((void*)&NumRotationUpdates, BoneIndexBitSize);
	Archive.Serialize((void*)&NumBonesDeleted, 2);

	if (Archive.IsLoading())
	{
		for (uint32 i = 0; i < NumLocationUpdates; ++i)
		{
			uint32 BoneIndex = 0;
			FVector Location;
			Archive.SerializeBits((void*)&BoneIndex, BoneIndexBitSize);
			AnimReplicatedRagdollHelpers::ReadAndDequantizeLocation(Archive, Location, Header.GetLocationQuantization());

			Locations.Add(BoneIndex, Location);
		}

		for (uint32 i = 0; i < NumRotationUpdates; ++i)
		{
			uint32 BoneIndex = 0;
			FRotator Rotation;
			Archive.SerializeBits((void*)&BoneIndex, BoneIndexBitSize);
			AnimReplicatedRagdollHelpers::ReadAndDequantizeRotation(Archive, Rotation, Header.GetRotationQuantization());
			Rotations.Add(BoneIndex, Rotation.Quaternion());
		}

		for (uint32 i = 0; i < NumBonesDeleted; ++i)
		{
			uint32 BoneIndex = 0;
			Archive.SerializeBits((void*)&BoneIndex, BoneIndexBitSize);
			DeletedBones.Add(BoneIndex);
		}
	}
	else if (Archive.IsSaving())
	{
		for (const TPair<uint32, FVector>& Pair : Locations)
		{
			Archive.SerializeBits((void*)&Pair.Key, BoneIndexBitSize);
			AnimReplicatedRagdollHelpers::QuantizeAndWriteLocation(Archive, Pair.Value, Header.GetLocationQuantization());
		}

		for (const TPair<uint32, FQuat>& Pair : Rotations)
		{
			Archive.SerializeBits((void*)&Pair.Key, BoneIndexBitSize);
			AnimReplicatedRagdollHelpers::QuantizeAndWriteRotation(Archive, Pair.Value.Rotator(), Header.GetRotationQuantization());
		}

		for (uint32 i = 0; i < NumBonesDeleted; ++i)
		{
			Archive.SerializeBits((void*)&DeletedBones[i], BoneIndexBitSize);
		}
	}
	else
	{
		checkNoEntry();
	}
}

void FReplicatedRagdollData::GatherBones(const FReplicatedRagdollData::FGatherBonesParams& Params)
{
	SCOPED_NAMED_EVENT(ReplicatedRagdollComponent_NetworkEncode_GatherBones, FColor::Orange);
	const FReplicatedRagdollNetState* OldState = static_cast<const FReplicatedRagdollNetState*>(Params.OldState);

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
	*Params.NewState = NewState;

	const EVectorQuantization LocationQuantization = Params.Header.GetLocationQuantization();
	const ERotatorQuantization RotationQuantization = Params.Header.GetRotationQuantization();
	
	if (OldState)
	{
		// Delete bones that aren't there anymore
		for (const TPair<int32, FReplicatedRagdollNetState::FReplicatedBoneData>& OldRagdollTransform : OldState->GetBoneData())
		{
			if (!Params.ComponentSpaceTransforms.Contains(OldRagdollTransform.Key))
			{
				Params.DeletedBones.Add(OldRagdollTransform.Key);
			}
		}

		NewState->RemoveBones(Params.DeletedBones);
	}

	// Only replicate the bones that pass the ShouldReplicateBone filter
	for (const TPair<int32, FTransform>& RagdollTransform : Params.ComponentSpaceTransforms)
	{
		if (!Params.AllowedBones.Contains(RagdollTransform.Key))
		{
			continue;
		}

		if (OldState == nullptr 
			|| OldState->ShouldUpdateBoneLocation(RagdollTransform.Key, RagdollTransform.Value.GetLocation(), LocationQuantization))
		{
			FVector Location = RagdollTransform.Value.GetLocation();
			if (Params.Header.GetBonesInWorldSpace())
			{
				Location = Params.SkeletalMeshTransform.TransformPositionNoScale(Location);
			}

			Params.BoneLocations.Add(RagdollTransform.Key, Location);
			NewState->UpdateBoneLocation(RagdollTransform.Key, Location, LocationQuantization);
		}

		if (OldState == nullptr 
			|| OldState->ShouldUpdateBoneRotation(RagdollTransform.Key, RagdollTransform.Value.GetRotation().Rotator(), RotationQuantization))
		{
			FRotator Rotation = RagdollTransform.Value.GetRotation().Rotator();
			if (Params.Header.GetBonesInWorldSpace())
			{
				Rotation = Params.SkeletalMeshTransform.TransformRotation(Rotation.Quaternion()).Rotator();
			}

			Params.BoneRotations.Add(RagdollTransform.Key, Rotation.Quaternion());
			NewState->UpdateBoneRotation(RagdollTransform.Key, Rotation, RotationQuantization);
		}
	}
}

void FReplicatedRagdollData::TrackReplicationStats(const FReplicatedRagdollNetHeader& Header, const TMap<uint32, FVector>& BoneLocations, const TMap<uint32, FQuat>& BoneRotations, bool bWriting)
{
	if (bWriting)
	{
		// Track location stats based on quantization level
		switch (Header.GetLocationQuantization())
		{
		case EVectorQuantization::RoundWholeNumber:
			INC_DWORD_STAT_BY(STAT_Encoding_RagdollBoneLocations_RoundWholeNumber, BoneLocations.Num());
			break;
		case EVectorQuantization::RoundOneDecimal:
			INC_DWORD_STAT_BY(STAT_Encoding_RagdollBoneLocations_RoundOneDecimal, BoneLocations.Num());
			break;
		case EVectorQuantization::RoundTwoDecimals:
			INC_DWORD_STAT_BY(STAT_Encoding_RagdollBoneLocations_RoundTwoDecimals, BoneLocations.Num());
			break;
		default:
			break;
		}

		// Track rotation stats based on quantization level
		switch (Header.GetRotationQuantization())
		{
		case ERotatorQuantization::ByteComponents:
			INC_DWORD_STAT_BY(STAT_Encoding_RagdollBoneRotations_ByteComponents, BoneRotations.Num());
			break;
		case ERotatorQuantization::ShortComponents:
			INC_DWORD_STAT_BY(STAT_Encoding_RagdollBoneRotations_ShortComponents, BoneRotations.Num());
			break;
		default:
			break;
		}
	}
	else
	{
		// Track location stats based on quantization level
		switch (Header.GetLocationQuantization())
		{
		case EVectorQuantization::RoundWholeNumber:
			INC_DWORD_STAT_BY(STAT_Decoding_RagdollBoneLocations_RoundWholeNumber, BoneLocations.Num());
			break;
		case EVectorQuantization::RoundOneDecimal:
			INC_DWORD_STAT_BY(STAT_Decoding_RagdollBoneLocations_RoundOneDecimal, BoneLocations.Num());
			break;
		case EVectorQuantization::RoundTwoDecimals:
			INC_DWORD_STAT_BY(STAT_Decoding_RagdollBoneLocations_RoundTwoDecimals, BoneLocations.Num());
			break;
		default:
			break;
		}

		// Track rotation stats based on quantization level
		switch (Header.GetRotationQuantization())
		{
		case ERotatorQuantization::ByteComponents:
			INC_DWORD_STAT_BY(STAT_Decoding_RagdollBoneRotations_ByteComponents, BoneRotations.Num());
			break;
		case ERotatorQuantization::ShortComponents:
			INC_DWORD_STAT_BY(STAT_Decoding_RagdollBoneRotations_ShortComponents, BoneRotations.Num());
			break;
		default:
			break;
		}
	}
}

bool FReplicatedRagdollData::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	const FString ReadingWriting = DeltaParms.Reader ? TEXT("Decoding") : (DeltaParms.Writer ? TEXT("Encoding") : TEXT("Unknown"));
	SCOPED_NAMED_EVENT_FSTRING(FString::Printf(TEXT("FReplicatedRagdollData_NetDeltaSerialize_%s"), *ReadingWriting), FColor::Magenta);

	FBitArchive* Archive = DeltaParms.Reader ? static_cast<FBitArchive*>(DeltaParms.Reader) : static_cast<FBitArchive*>(DeltaParms.Writer);
	if (Archive == nullptr)
	{
		return false;
	}

	FReplicatedRagdollNetHeader Header;
	TMap<uint32, FVector> BoneLocations;
	TMap<uint32, FQuat> BoneRotations;
	TArray<uint32> DeletedBones;

	const USkeletalMeshComponent* SkeletalMesh = nullptr;
	if (const UReplicatedRagdollComponent* RagdollComponent = Cast<const UReplicatedRagdollComponent>(DeltaParms.Object))
	{
		if (Archive->IsSaving())
		{
			// Fast path, skip serialization if we have already serialized the data for this connection
			UReplicatedRagdollComponent::FSerializedAnimData SerializedData;
			if (RagdollComponent->GetSerializedAnimData(DeltaParms.Connection, SerializedData))
			{
				if (SerializedData.ReplicationKey == ReplicationKey)
				{
					RagdollComponent->UpdateNetState(DeltaParms.Connection, SerializedData.NewNetState);
					*DeltaParms.NewState = SerializedData.NewNetState;

					Archive->SerializeBits(SerializedData.SerializedData->GetData(), SerializedData.NumBitsWritten);
					return true;
				}
			}
		}
		SkeletalMesh = RagdollComponent->GetSkeletalMesh();
	}
	
	if (SkeletalMesh == nullptr)
	{
		return false;
	}

	// Move this to some multithreaded thing
	if (DeltaParms.Writer)
	{
		FReplicatedRagdollOptions ReplicationOptions;
		if (const UReplicatedRagdollComponent* RagdollComponent = Cast<const UReplicatedRagdollComponent>(DeltaParms.Object))
		{
			ReplicationOptions = RagdollComponent->GetReplicationOptions();
		}
		Header.SetNumBitsForBoneIndex(GetMaxBoneIndex());
		Header.SetLocationQuantization(ReplicationOptions.LocationQuantizationLevel);
		Header.SetRotationQuantization(ReplicationOptions.RotationQuantizationLevel);
		Header.SetBonesInWorldSpace(ReplicationOptions.bReplicateBonesInWorldSpace);

		const TArray<int32> AllowedBones = ReplicationOptions.GetBonesToReplicate(SkeletalMesh);
		GatherBones(FGatherBonesParams{
			DeltaParms.OldState,
			DeltaParms.NewState,
			SkeletalMesh->GetComponentTransform(),
			ComponentSpaceTransforms,
			BoneLocations,
			BoneRotations,
			DeletedBones,
			Header,
			AllowedBones,
			ReplicationOptions
		});

		TrackReplicationStats(Header, BoneLocations, BoneRotations, true);
	}

	Archive->Serialize(&Header, sizeof(Header));

	SerializeBones(BoneLocations, BoneRotations, DeletedBones, *Archive, Header);

	if (DeltaParms.Reader)
	{
		for (uint32 DeletedBone : DeletedBones)
		{
			ComponentSpaceTransforms.Remove(DeletedBone);
		}

		for (const TPair<uint32, FVector>& BoneLocation : BoneLocations)
		{
			FVector Location = BoneLocation.Value;
			if (Header.GetBonesInWorldSpace() && SkeletalMesh)
			{
				Location = SkeletalMesh->GetComponentTransform().InverseTransformPositionNoScale(Location);
			}

			FTransform& Transform = ComponentSpaceTransforms.FindOrAdd(BoneLocation.Key);
			Transform.SetLocation(Location);
		}

		for (const TPair<uint32, FQuat>& BoneRotation : BoneRotations)
		{
			FRotator Rotation = BoneRotation.Value.Rotator();
			if (Header.GetBonesInWorldSpace() && SkeletalMesh)
			{
				Rotation = SkeletalMesh->GetComponentTransform().InverseTransformRotation(Rotation.Quaternion()).Rotator();
			}

			FTransform& Transform = ComponentSpaceTransforms.FindOrAdd(BoneRotation.Key);
			Transform.SetRotation(Rotation.Quaternion());
		}

		TrackReplicationStats(Header, BoneLocations, BoneRotations, false);
	}

	return true;
}

UE_ENABLE_OPTIMIZATION

bool FReplicatedRagdollOptions::ShouldReplicateBone(const USkeletalMeshComponent* SkeletalMesh, int32 BoneIndex) const
{
	return ::ShouldReplicateBone(SkeletalMesh, BoneIndex, *this);
}

TArray<int32> FReplicatedRagdollOptions::GetBonesToReplicate(const USkeletalMeshComponent* SkeletalMesh) const
{
	TArray<int32> BonesToReplicate;
	if (!SkeletalMesh)
	{
		return BonesToReplicate;
	}
	const int32 NumBones = SkeletalMesh->GetNumBones();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		if (ShouldReplicateBone(SkeletalMesh, BoneIndex))
		{
			BonesToReplicate.Add(BoneIndex);
		}
	}
	return BonesToReplicate;
}
