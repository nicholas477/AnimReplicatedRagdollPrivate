// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "AnimReplicatedRagdollTypes.generated.h"

/** Custom INetDeltaBaseState used by Fast Array Serialization */
class FReplicatedRagdollNetState : public INetDeltaBaseState
{
public:
	FReplicatedRagdollNetState()
		: BoneReplicationKeys()
		, ArrayReplicationKey(INDEX_NONE)
	{
	}

	virtual bool IsStateEqual(INetDeltaBaseState* OtherState)
	{
		FReplicatedRagdollNetState* Other = static_cast<FReplicatedRagdollNetState*>(OtherState);
		for (auto It = BoneReplicationKeys.CreateIterator(); It; ++It)
		{
			auto Ptr = Other->BoneReplicationKeys.Find(It.Key());
			if (!Ptr || *Ptr != It.Value())
			{
				return false;
			}
		}
		return true;
	}

	TMap<int32, int32> BoneReplicationKeys;

	int32 ArrayReplicationKey;
};

USTRUCT(BlueprintType)
struct FReplicatedRagdollData
{
	GENERATED_BODY()

	UPROPERTY(NotReplicated, BlueprintReadWrite, VisibleInstanceOnly)
	TMap<int32, FTransform> ComponentSpaceTransforms;

	void CapturePose(const USkeletalMeshComponent* SkeletalMesh, bool bOptimizeCapture = true);
	void ApplyPose(USkeletalMeshComponent* SkeletalMesh);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	bool Serialize(FArchive& Ar)
	{
		Ar << ComponentSpaceTransforms;
		return true;
	}
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
	AllowList,
	DenyList
};

USTRUCT(BlueprintType)
struct FReplicatedRagdollOptions
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

	// How big does the location difference have to be between the server transform and the client transform for an update to get sent
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta=(Units="cm"))
	float LocationUpdateThreshold = 1.f;

	// How big does the rotation difference have to be between the server transform and the client transform for an update to get sent
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta=(Units="degrees"))
	float RotationUpdateThreshold = 1.f;

	// Whether to use an allow list or a deny list when determining which bones should be replicated to the client.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EReplicatedBoneFilterType BoneFilterType = EReplicatedBoneFilterType::DenyList;

	// A list of the only bones that are sent to the client. If BoneFilterType is set to AllowList then only the bones in this list will be replicated to the client.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(EditCondition="BoneFilterType == EReplicatedBoneFilterType::AllowList"))
	TSet<FName> BoneAllowList;

	// A list of bones that are never sent to the client, if the bone filter type is set to DenyList.
	// The bone, and also any child bones of that bone, will not be replicated.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "BoneFilterType == EReplicatedBoneFilterType::DenyList"))
	TSet<FName> BoneDenyList;
};