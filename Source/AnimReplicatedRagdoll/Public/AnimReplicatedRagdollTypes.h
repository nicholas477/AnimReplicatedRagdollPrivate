// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/ReplicatedState.h"
#include "AnimReplicatedRagdollTypes.generated.h"

class UReplicatedRagdollComponent;
struct FReplicatedRagdollNetHeader;

USTRUCT(BlueprintType)
struct ANIMREPLICATEDRAGDOLL_API FReplicatedRagdollData
{
	GENERATED_BODY()

	UPROPERTY(NotReplicated, BlueprintReadWrite, VisibleInstanceOnly)
	TMap<int32, FTransform> ComponentSpaceTransforms;

	void CapturePose(const USkeletalMeshComponent* SkeletalMesh);
	void ApplyPose(USkeletalMeshComponent* SkeletalMesh);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);
	bool NetDeltaSerialize(UReplicatedRagdollComponent* RagdollComponent, INetDeltaBaseState* OldState, TSharedPtr<INetDeltaBaseState>* NewState, TArray<uint8>& OutData) const;

	bool Serialize(FArchive& Ar)
	{
		Ar << ComponentSpaceTransforms;
		return true;
	}

	int32 GetMaxBoneIndex() const
	{
		int32 MaxIndex = INDEX_NONE;
		for (const TPair<int32, FTransform>& Pair : ComponentSpaceTransforms)
		{
			if (Pair.Key > MaxIndex)
			{
				MaxIndex = Pair.Key;
			}
		}
		return MaxIndex;
	}

	struct FGatherBonesParams
	{
		/** Pointer to the previous base state. Used when writing. */
		INetDeltaBaseState* OldState = nullptr;

		/** SharedPtr to new base state created by NetDeltaSerialize. Used when writing.*/
		TSharedPtr<INetDeltaBaseState>* NewState = nullptr;

		const FTransform SkeletalMeshTransform;
		const TMap<int32, FTransform>& ComponentSpaceTransforms;
		TMap<uint32, FVector>& BoneLocations;
		TMap<uint32, FQuat>& BoneRotations;
		TArray<uint32>& DeletedBones;
		const FReplicatedRagdollNetHeader& Header;
		const TArray<int32>& AllowedBones;
		const FReplicatedRagdollOptions& ReplicationOptions;
	};

	static void GatherBones(const FGatherBonesParams& Params);
	static void SerializeBones(TMap<uint32, FVector>& Locations, TMap<uint32, FQuat>& Rotations, TArray<uint32>& DeletedBones, FArchive& Archive, FReplicatedRagdollNetHeader& Header);
};

// The size of the unsigned integer for bone indicies.
// // This is used to save bandwidth when replicating bone transforms by using a smaller integer type for the bone index when possible.
enum class EBoneIndexFormat : uint8
{
	Byte,
	Short
};

static FArchive& operator<<(FArchive& Ar, FReplicatedRagdollData& Value)
{
	Value.Serialize(Ar);
	return Ar;
}

template<> struct TStructOpsTypeTraits<FReplicatedRagdollData> : public TStructOpsTypeTraitsBase2<FReplicatedRagdollData>
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithSerializer = true,
	};
};

UENUM(BlueprintType)
enum class EReplicatedBoneFilterType : uint8
{
	AllowList UMETA(ToolTip = "ONLY allow the bones in this list to replicate to clients"),
	DenyList UMETA(ToolTip = "Replicate all bones EXCEPT the ones in this list")
};

USTRUCT(BlueprintType)
struct ANIMREPLICATEDRAGDOLL_API FReplicatedRagdollBoneFilter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Bone;

	operator FName() const { return Bone; }
	operator const FName&() const { return Bone; }

	bool operator==(const FReplicatedRagdollBoneFilter& Other) const { return Bone == Other.Bone; }
	bool operator!=(const FReplicatedRagdollBoneFilter& Other) const { return Bone != Other.Bone; }

	FReplicatedRagdollBoneFilter& operator=(const FName& Other)
	{
		Bone = Other;
		return *this;
	}
};

static uint32 GetTypeHash(const FReplicatedRagdollBoneFilter& BoneFilter)
{
	return GetTypeHash(BoneFilter.Bone);
}

USTRUCT(BlueprintType)
struct ANIMREPLICATEDRAGDOLL_API FReplicatedRagdollOptions
{
	GENERATED_BODY()

	// The level of quantization to apply to the location component of the replicated bone transforms.
	// Higher levels of quantization will result in less network bandwidth used, but more noticeable snapping of the replicated bones.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EVectorQuantization LocationQuantizationLevel = EVectorQuantization::RoundOneDecimal;

	// The level of quantization to apply to the rotation component of the replicated bone transforms.
	// Higher levels of quantization will result in less network bandwidth used, but more noticeable rotational snapping of the replicated bones.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	ERotatorQuantization RotationQuantizationLevel = ERotatorQuantization::ByteComponents;

	// Whether to use an allow list or a deny list when determining which bones should be replicated to the client.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EReplicatedBoneFilterType BoneFilterType = EReplicatedBoneFilterType::DenyList;

	// A list of the only bones that are sent to the client. If BoneFilterType is set to AllowList then only the bones in this list will be replicated to the client.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(EditCondition="BoneFilterType == EReplicatedBoneFilterType::AllowList"))
	TSet<FReplicatedRagdollBoneFilter> BoneAllowList;

	// A list of bones that are never sent to the client, if the bone filter type is set to DenyList.
	// The bone, and also any child bones of that bone, will not be replicated.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "BoneFilterType == EReplicatedBoneFilterType::DenyList"))
	TSet<FReplicatedRagdollBoneFilter> BoneDenyList;

	// If true, then the bones will be sent to the client in world space rather than component space
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bReplicateBonesInWorldSpace = true;

	bool ShouldReplicateBone(const USkeletalMeshComponent* SkeletalMesh, int32 BoneIndex) const;
	TArray<int32> GetBonesToReplicate(const USkeletalMeshComponent* SkeletalMesh) const;
};

struct ANIMREPLICATEDRAGDOLL_API FReplicatedRagdollNetHeader
{
	uint8 BoneIndexFormat : 1;
	uint8 LocationQuantizationLevel : 2;
	uint8 RotationQuantizationLevel : 2;
	uint8 bBonesInWorldSpace : 1;

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