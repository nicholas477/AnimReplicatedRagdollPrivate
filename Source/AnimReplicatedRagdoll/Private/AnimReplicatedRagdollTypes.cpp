// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimReplicatedRagdollTypes.h"

#include "RRSkeletalMeshComponent.h"
#include "ReplicatedRagdollComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Net/Core/Serialization/QuantizedVectorSerialization.h"

UE_DISABLE_OPTIMIZATION

void FReplicatedRagdollData::CapturePose(const USkeletalMeshComponent* SkeletalMesh, bool bOptimizeCapture)
{
	check(SkeletalMesh != nullptr);
	const TArray<FTransform> BoneTransforms = SkeletalMesh->GetComponentSpaceTransforms();

	const ENetMode NetMode = SkeletalMesh->GetNetMode();
	if (bOptimizeCapture && (NetMode == ENetMode::NM_DedicatedServer || NetMode == ENetMode::NM_ListenServer) && (BoneTransforms.Num() == ComponentSpaceTransforms.Num()))
	{
		for (int32 i = 0; i < BoneTransforms.Num(); ++i)
		{
			ComponentSpaceTransforms.Add(i, BoneTransforms[i]);
			//MarkItemDirty(Transform);
		}
	}
	else
	{
		//ComponentSpaceTransforms.SetNum(BoneTransforms.Num());
		for (int32 i = 0; i < BoneTransforms.Num(); ++i)
		{
			ComponentSpaceTransforms.Add(i, BoneTransforms[i]);
		}
		//MarkArrayDirty();
	}
}

void FReplicatedRagdollData::ApplyPose(USkeletalMeshComponent* SkeletalMesh)
{
	check(SkeletalMesh != nullptr);
	TArray<FTransform>& BoneTransforms = SkeletalMesh->GetEditableComponentSpaceTransforms();

	if (BoneTransforms.Num() == ComponentSpaceTransforms.Num())
	{
		for (int32 i = 0; i < ComponentSpaceTransforms.Num(); ++i)
		{
			BoneTransforms[i] = ComponentSpaceTransforms[i];
		}
		((URRSkeletalMeshComponent*)SkeletalMesh)->ApplyEditedComponentSpaceTransforms();
	}
}

struct FReplicatedRagdollNetHeader
{
	// God help you if you need more than 268 million bones
	uint32 NumBones : 28;
	uint32 LocationQuantizationLevel : 2;
	uint32 RotationQuantizationLevel : 2;

	EVectorQuantization GetLocationQuantization() const
	{
		return static_cast<EVectorQuantization>(LocationQuantizationLevel);
	}

	ERotatorQuantization GetRotationQuantization() const
	{
		return static_cast<ERotatorQuantization>(RotationQuantizationLevel);
	}
};

static_assert(sizeof(FReplicatedRagdollNetHeader) == 4, "FReplicatedRagdollNetHeader should be 4 bytes across all platforms");

static void QuantizeAndWriteTransform(FBitWriter& Writer, const FTransform& Transform, EVectorQuantization LocationQuantization, ERotatorQuantization RotationQuantization)
{
	FVector Location = Transform.GetLocation();
	FRotator Rotation = Transform.GetRotation().Rotator();
	switch (LocationQuantization)
	{
	case EVectorQuantization::RoundWholeNumber:
		SerializePackedVector<1, 24>(Location, Writer);
		break;
	case EVectorQuantization::RoundOneDecimal:
		SerializePackedVector<10, 27>(Location, Writer);
		break;
	case EVectorQuantization::RoundTwoDecimals:
		SerializePackedVector<100, 30>(Location, Writer);
		break;
	default:
		break;
	}
	switch (RotationQuantization)
	{
	case ERotatorQuantization::ByteComponents:
		Rotation.SerializeCompressed(Writer);
		break;
	case ERotatorQuantization::ShortComponents:
		Rotation.SerializeCompressedShort(Writer);
		break;
	default:
		break;
	}
}

static void ReadAndDequantizeTransform(FBitReader& Reader, FTransform& Transform, EVectorQuantization LocationQuantization, ERotatorQuantization RotationQuantization)
{
	FVector Location;
	FRotator Rotation;
	switch (LocationQuantization)
	{
	case EVectorQuantization::RoundWholeNumber:
		SerializePackedVector<1, 24>(Location, Reader);
		break;
	case EVectorQuantization::RoundOneDecimal:
		SerializePackedVector<10, 27>(Location, Reader);
		break;
	case EVectorQuantization::RoundTwoDecimals:
		SerializePackedVector<100, 30>(Location, Reader);
		break;
	default:
		break;
	}
	switch (RotationQuantization)
	{
	case ERotatorQuantization::ByteComponents:
		Rotation.SerializeCompressed(Reader);
		break;
	case ERotatorQuantization::ShortComponents:
		Rotation.SerializeCompressedShort(Reader);
		break;
	default:
		break;
	}
	Transform.SetLocation(Location);
	Transform.SetRotation(Rotation.Quaternion());
}

bool ShouldReplicateBone(const USkeletalMeshComponent* SkeletalMesh, int32 BoneIndex, const FReplicatedRagdollOptions& Options)
{
	if (!SkeletalMesh)
	{
		return true;
	}

	if (Options.BoneFilterType == EReplicatedBoneFilterType::AllowList)
	{
		return Options.BoneAllowList.Contains(SkeletalMesh->GetBoneName(BoneIndex));
	}
	else
	{
		return !Options.BoneDenyList.Contains(SkeletalMesh->GetBoneName(BoneIndex));
	}
}

bool FReplicatedRagdollData::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	FReplicatedRagdollNetHeader Header;
	if (DeltaParms.Writer)
	{
		const USkeletalMeshComponent* SkeletalMesh = nullptr;
		FReplicatedRagdollOptions ReplicationOptions;
		if (const UReplicatedRagdollComponent* RagdollComponent = Cast<const UReplicatedRagdollComponent>(DeltaParms.Object))
		{
			ReplicationOptions = RagdollComponent->GetReplicationOptions();
			SkeletalMesh = RagdollComponent->GetSkeletalMesh();
		}

		Header.LocationQuantizationLevel = static_cast<uint8>(ReplicationOptions.LocationQuantizationLevel);
		Header.RotationQuantizationLevel = static_cast<uint8>(ReplicationOptions.RotationQuantizationLevel);

		// Only replicate the bones that pass the ShouldReplicateBone filter
		TMap<uint16, FTransform> Bones;
		for (const TPair<int32, FTransform>& RagdollTransform : ComponentSpaceTransforms)
		{
			if (ShouldReplicateBone(SkeletalMesh, RagdollTransform.Key, ReplicationOptions))
			{
				ensure(static_cast<uint16>(RagdollTransform.Key) == RagdollTransform.Key);
				Bones.Add(static_cast<uint16>(RagdollTransform.Key), RagdollTransform.Value);
			}
		}

		Header.NumBones = Bones.Num();

		// Actual serialization to bytes happens here
		DeltaParms.Writer->Serialize(&Header, sizeof(Header));
		for (const TPair<uint16, FTransform>& RagdollTransform : Bones)
		{
			DeltaParms.Writer->Serialize((void*)&RagdollTransform.Key, sizeof(RagdollTransform.Key));
			QuantizeAndWriteTransform(*DeltaParms.Writer, RagdollTransform.Value, Header.GetLocationQuantization(), Header.GetRotationQuantization());
		}
	}
	else if (DeltaParms.Reader)
	{
		DeltaParms.Reader->Serialize(&Header, sizeof(Header));
		for (uint32 i = 0; i < Header.NumBones; ++i)
		{
			uint16 BoneIndex = 0;
			FTransform Transform;
			DeltaParms.Reader->Serialize((void*)&BoneIndex, sizeof(BoneIndex));
		
			ReadAndDequantizeTransform(*DeltaParms.Reader, Transform, Header.GetLocationQuantization(), Header.GetRotationQuantization());

			ComponentSpaceTransforms.Add(BoneIndex, Transform);
		}
	}

	return true;
}

UE_ENABLE_OPTIMIZATION
