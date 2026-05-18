// Fill out your copyright notice in the Description page of Project Settings.


#include "ReplicatedRagdollComponent.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "AnimNode_ReplicatedRagdoll.h"

#if WITH_EDITOR
#include "UObject/Script.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#endif

#define LOCTEXT_NAMESPACE "ReplicatedRagdollComponent"

UReplicatedRagdollComponent::UReplicatedRagdollComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;

	SetIsReplicatedByDefault(true);
}

void UReplicatedRagdollComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_EDITOR
	CheckRequirements();
#endif

	if (GetOwner()->HasAuthority() && GetNetMode() != ENetMode::NM_Standalone)
	{
		if (ShouldCaptureRagdoll())
		{
			CaptureRagdoll();
		}
		else
		{
			ClearRagdoll();
		}
	}

	AnimDataHandle->WriteEvaluateAnimation(ShouldApplyRagdoll());
}

void UReplicatedRagdollComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, AnimData, Params);
}

void UReplicatedRagdollComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (!AnimDataHandle.IsValid())
	{
		AnimDataHandle = MakeShared<decltype(AnimDataHandle)::ElementType>();
	}
}

void UReplicatedRagdollComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving() && Ar.ArIsSaveGame)
	{
		CaptureRagdoll();
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading() && Ar.ArIsSaveGame)
	{
		ApplyRagdoll();
	}
}

USkeletalMeshComponent* UReplicatedRagdollComponent::GetSkeletalMesh() const
{
#if WITH_EDITOR
	if (!AttachParentFlag)
	{
		if (!GetAttachParent() || !GetAttachParent()->IsA<USkeletalMeshComponent>())
		{
			AttachParentFlag.SetCalledError();

			const FText Text = FText::Format(
				LOCTEXT("InvalidAttachment", "ReplicatedRagdollComponent '{0}' is attached to an invalid parent. It must be attached to a SkeletalMeshComponent!"),
				FText::FromName(GetFName()));

			FMessageLog MessageLog("PIE");
			MessageLog.Error()
				->AddToken(FTextToken::Create(Text))
				->AddToken(FUObjectToken::Create(GetOwner()));
			MessageLog.Open(EMessageSeverity::Error);
		}
	}
#endif

	return Cast<USkeletalMeshComponent>(GetAttachParent());
}

void UReplicatedRagdollComponent::ClearRagdoll()
{
	if (!AnimData.ComponentSpaceTransforms.IsEmpty())
	{
		AnimData.ComponentSpaceTransforms.Empty();
		MARK_PROPERTY_DIRTY_FROM_NAME(UReplicatedRagdollComponent, AnimData, this);

		AnimDataHandle->WriteRagdollData(AnimData);
	}
}

void UReplicatedRagdollComponent::CaptureRagdoll()
{
	if (USkeletalMeshComponent* SkeletalMesh = GetSkeletalMesh())
	{
		AnimData.CapturePose(SkeletalMesh);
		MARK_PROPERTY_DIRTY_FROM_NAME(UReplicatedRagdollComponent, AnimData, this);
		AnimDataHandle->WriteRagdollData(AnimData);
	}
}

void UReplicatedRagdollComponent::ApplyRagdoll()
{
	if (USkeletalMeshComponent* SkeletalMesh = GetSkeletalMesh())
	{
		AnimData.ApplyPose(SkeletalMesh);
	}
}

void UReplicatedRagdollComponent::SimulateNonReplicatedBones() const
{
	if (USkeletalMeshComponent* SkeletalMesh = GetSkeletalMesh())
	{
		if (ReplicationOptions.BoneFilterType == EReplicatedBoneFilterType::AllowList)
		{
			for (const FName& Bone : ReplicationOptions.BoneAllowList)
			{
				SkeletalMesh->SetAllBodiesBelowSimulatePhysics(Bone, true, false);
			}
		}
		else
		{
			for (const FName& Bone : ReplicationOptions.BoneDenyList)
			{
				SkeletalMesh->SetAllBodiesBelowSimulatePhysics(Bone, true, true);
			}
		}
	}
}

bool UReplicatedRagdollComponent::ShouldApplyRagdoll_Implementation()
{
	return true;
}

bool UReplicatedRagdollComponent::ShouldCaptureRagdoll_Implementation()
{
	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMesh();
	if (SkeletalMesh == nullptr)
	{
		return false;
	}

	if (!SkeletalMesh->IsAnySimulatingPhysics())
	{
		return false;
	}

	return true;
}

void UReplicatedRagdollComponent::OnRep_AnimData()
{
	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMesh();
	if (SkeletalMesh == nullptr)
	{
		return;
	}

	if (!AnimDataHandle.IsValid())
	{
		AnimDataHandle = MakeShared<decltype(AnimDataHandle)::ElementType>();
	}
	AnimDataHandle->WriteRagdollData(AnimData);
}

FReplicatedRagdollData FRagdollAnimData::GetInterpedRagdollData(float DeltaTime, float InterpSpeed) const
{
	FReplicatedRagdollData OutData;

	// This is what the bones are currently at on the client side. This is what we will interp from
	const FReplicatedRagdollData CurrentDataCopy = ReadCurrentRagdollData();

	// This is the target data that we are interping towards. This is what we will interp to
	const FReplicatedRagdollData DataCopy = ReadRagdollData();

	for (const TPair<int32, FTransform>& Pair : DataCopy.ComponentSpaceTransforms)
	{
		FTransform& OutTransform = OutData.ComponentSpaceTransforms.FindOrAdd(Pair.Key, Pair.Value);

		const FTransform* CurrentTransform = CurrentDataCopy.ComponentSpaceTransforms.Find(Pair.Key);
		if (!CurrentTransform)
		{
			continue;
		}

		OutTransform.SetLocation(FMath::VInterpTo(
			CurrentTransform->GetLocation(),
			DataCopy.ComponentSpaceTransforms[Pair.Key].GetLocation(),
			DeltaTime,
			InterpSpeed
		));

		OutTransform.SetRotation(
			FMath::RInterpTo(
				CurrentTransform->GetRotation().Rotator(),
				DataCopy.ComponentSpaceTransforms[Pair.Key].GetRotation().Rotator(),
				DeltaTime,
				InterpSpeed
			).Quaternion()
		);
	}

	return OutData;
}

#if WITH_EDITOR
void UReplicatedRagdollComponent::CheckRequirements() const
{
	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMesh();
	if (!SkeletalMesh)
	{
		return;
	}

	if (GetNetMode() != ENetMode::NM_Standalone)
	{
		if (!IsReplicatedFlag && !GetIsReplicated())
		{
			IsReplicatedFlag.SetCalledError();

			const FText Text = FText::Format(
				LOCTEXT("NotReplicated", "ReplicatedRagdollComponent '{0}' is not replicated! The component must be set to replicate for the ragdoll replication to work correctly!"),
				FText::FromName(GetFName()));

			FMessageLog MessageLog("PIE");
			MessageLog.Error()
				->AddToken(FTextToken::Create(Text))
				->AddToken(FUObjectToken::Create(GetOwner()));
			MessageLog.Open(EMessageSeverity::Error);
		}

		if (!OwnerIsReplicatedFlag &&!GetOwner()->GetIsReplicated())
		{
			OwnerIsReplicatedFlag.SetCalledError();

			const FText Text = FText::Format(
				LOCTEXT("OwnerNotReplicated", "ReplicatedRagdollComponent '{0}' is attached to an Actor that is not replicated. The owner Actor must be set to replicate for the ragdoll replication to work correctly!"),
				FText::FromName(GetFName()));

			FMessageLog MessageLog("PIE");
			MessageLog.Error()
				->AddToken(FTextToken::Create(Text))
				->AddToken(FUObjectToken::Create(GetOwner()));
			MessageLog.Open(EMessageSeverity::Error);
		}
	}

	UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstanceFlag && !AnimInstance && GetNetMode() == ENetMode::NM_Client)
	{
		AnimInstanceFlag.SetCalledError();

		const FText Text = FText::Format(
			LOCTEXT("NoAnimInstance", "ReplicatedRagdollComponent '{0}' is attached to a SkeletalMeshComponent with no AnimInstance on client. The component requires a valid AnimInstance for the ragdoll replication to work correctly!"),
			FText::FromName(GetFName()));

		FMessageLog MessageLog("PIE");
		MessageLog.Error()
			->AddToken(FTextToken::Create(Text))
			->AddToken(FUObjectToken::Create(GetOwner()));
		MessageLog.Open(EMessageSeverity::Error);
	}

	if (!FoundAnimNodeFlag && AnimInstance)
	{
		const UClass* AnimClass = AnimInstance->GetClass();
		if (AnimClass)
		{
			bool bFoundNode = false;
			// Search through the animation blueprint's properties for FAnimNode_ReplicatedRagdoll
			for (TFieldIterator<FStructProperty> It(AnimClass); It; ++It)
			{
				if (It->Struct && It->Struct->IsChildOf(FAnimNode_ReplicatedRagdoll::StaticStruct()))
				{
					bFoundNode = true;
					break;
				}
			}

			if (!bFoundNode)
			{
				FoundAnimNodeFlag.SetCalledError();

				const FText Text = FText::Format(
					LOCTEXT("NoAnimNode", "ReplicatedRagdollComponent '{0}' could not find an FAnimNode_ReplicatedRagdoll in the AnimInstance of the attached SkeletalMeshComponent. The animation blueprint must contain an FAnimNode_ReplicatedRagdoll node for the ragdoll replication to work correctly!"),
					FText::FromName(GetFName()));

				FMessageLog MessageLog("PIE");
				MessageLog.Error()
					->AddToken(FTextToken::Create(Text))
					->AddToken(FUObjectToken::Create(GetOwner()))
					->AddToken(FUObjectToken::Create(AnimClass));
				MessageLog.Open(EMessageSeverity::Error);
			}
		}
	}

	if (!GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return;
	}

	if (!TickOptionFlag && SkeletalMesh->VisibilityBasedAnimTickOption != EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones)
	{
		TickOptionFlag.SetCalledError();

		const FText Text = FText::Format(
			LOCTEXT("InvalidTickOption", "ReplicatedRagdollComponent '{0}' is attached to a SkeletalMeshComponent with an invalid VisibilityBasedAnimTickOption on dedicated server. The option must be set to 'AlwaysTickPoseAndRefreshBones' for the ragdoll replication to work correctly!"),
			FText::FromName(GetFName()));

		FMessageLog MessageLog("PIE");
		MessageLog.Error()
			->AddToken(FTextToken::Create(Text))
			->AddToken(FUObjectToken::Create(GetOwner()));
		MessageLog.Open(EMessageSeverity::Error);
	}
}
#endif

#undef LOCTEXT_NAMESPACE
