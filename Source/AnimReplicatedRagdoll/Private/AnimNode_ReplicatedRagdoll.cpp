// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNode_ReplicatedRagdoll.h"

#include "Animation/AnimInstanceProxy.h"

#if WITH_EDITOR
#include "UObject/Script.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#endif

void FAnimNode_ReplicatedRagdoll::PreUpdate(const UAnimInstance* InAnimInstance)
{
	// cache the currently used skeletal mesh's bone names
	if (InAnimInstance->GetSkelMeshComponent() && InAnimInstance->GetSkelMeshComponent()->IsRegistered())
	{
		USkeletalMeshComponent* SkeletalMeshComponent = InAnimInstance->GetSkelMeshComponent();
		if (SkeletalMeshComponent)
		{
			for (TObjectPtr<USceneComponent> Component : SkeletalMeshComponent->GetAttachChildren())
			{
				if (UReplicatedRagdollComponent* RagdollComponent = Cast<UReplicatedRagdollComponent>(Component))
				{
					AnimDataHandle = RagdollComponent->GetAnimDataHandle();

					if (AnimDataHandle.IsValid())
					{
						// On server, we want to capture the current pose of the ragdoll and write it to the handle
						if (AnimDataHandle->ReadCurrentRagdollData().ComponentSpaceTransforms.Num() > 0)
						{
							FReplicatedRagdollData CurrentData;
							CurrentData.CapturePose(SkeletalMeshComponent);
							AnimDataHandle->WriteCurrentRagdollData(CurrentData);
						}
						else
						{
							AnimDataHandle->WriteCurrentRagdollData(AnimDataHandle->ReadRagdollData());
						}
					}

					break;
				}
			}
		}

	}

#if WITH_EDITOR
	CheckForSimulatedBones(InAnimInstance);
#endif
}

void FAnimNode_ReplicatedRagdoll::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_Base::Initialize_AnyThread(Context);

	ComponentPose.Initialize(Context);
}

void FAnimNode_ReplicatedRagdoll::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	FAnimNode_Base::CacheBones_AnyThread(Context);
	//InitializeBoneReferences(Context.AnimInstanceProxy->GetRequiredBones());
	ComponentPose.CacheBones(Context);
}

void FAnimNode_ReplicatedRagdoll::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateComponentSpace_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(ReplicatedRagdoll, !IsInGameThread());

	Super::EvaluateComponentSpace_AnyThread(Output);
	ComponentPose.EvaluateComponentSpace(Output);

	if (AnimDataHandle.IsValid())
	{
		if (!AnimDataHandle->ReadEvaluateAnimation() || AnimDataHandle->ReadRagdollData().ComponentSpaceTransforms.Num() == 0)
		{
			return;
		}

		FReplicatedRagdollData Transforms = AnimDataHandle->GetInterpedRagdollData(Output.AnimInstanceProxy->GetDeltaSeconds(), InterpSpeed);
		if (Transforms.ComponentSpaceTransforms.Num() == 0)
		{
			// if we don't have interped data then just go to the target
			Transforms = AnimDataHandle->ReadRagdollData();
		}

		{
			TArray<FBoneTransform> BoneTransforms;
			BoneTransforms.Reserve(Transforms.ComponentSpaceTransforms.Num());

			for (const TPair<int32, FTransform>& RagdollTransform : Transforms.ComponentSpaceTransforms)
			{
				FBoneTransform BoneTransform;
				BoneTransform.BoneIndex = FCompactPoseBoneIndex(RagdollTransform.Key);
				BoneTransform.Transform = RagdollTransform.Value;

				BoneTransforms.Add(BoneTransform);
			}

			Output.Pose.LocalBlendCSBoneTransforms(BoneTransforms, 1.0f);
		}
	}
}

void FAnimNode_ReplicatedRagdoll::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);
	Super::Update_AnyThread(Context);

	GetEvaluateGraphExposedInputs().Execute(Context);

	ComponentPose.Update(Context);
}

#if WITH_EDITOR
void FAnimNode_ReplicatedRagdoll::CheckForSimulatedBones(const UAnimInstance* InAnimInstance) const
{
	if (!AnimDataHandle.IsValid())
	{
		return;
	}

	if (!AnimDataHandle->ReadEvaluateAnimation())
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = InAnimInstance->GetSkelMeshComponent();
	if (!SkeletalMeshComponent)
	{
		return;
	}

	if (!SkeletalMeshComponent->IsRegistered())
	{
		return;
	}

	// Don't check for simulated bones on server since the server is the one simulating the physics and writing the data
	if (!SkeletalMeshComponent->GetOwner() || SkeletalMeshComponent->GetOwner()->HasAuthority())
	{
		return;
	}

	const auto RagdollData = AnimDataHandle->ReadCurrentRagdollData();
	for (const TPair<int32, FTransform>& RagdollTransform : RagdollData.ComponentSpaceTransforms)
	{
		const FName BoneName = SkeletalMeshComponent->GetBoneName(RagdollTransform.Key);
		if (BoneName == NAME_None)
		{
			const FText Text = FText::Format(NSLOCTEXT("AnimNode_ReplicatedRagdoll", "InvalidBoneName", "AnimReplicatedRagdoll:Invalid bone name for bone index {0}"), FText::AsNumber(RagdollTransform.Key));

			FMessageLog MessageLog("PIE");
			MessageLog.Error()
				->AddToken(FTextToken::Create(Text));
			MessageLog.Open(EMessageSeverity::Error);

			continue;
		}

		if (SkeletalMeshComponent->IsSimulatingPhysics(BoneName))
		{
			const FText Text = FText::Format(NSLOCTEXT("AnimNode_ReplicatedRagdoll", "SimulatedBone", "AnimReplicatedRagdoll: Bone {0} is currently simulating physics on client when it has a replicated ragdoll."), FText::FromName(BoneName));
			FMessageLog MessageLog("PIE");
			MessageLog.Warning()
				->AddToken(FTextToken::Create(Text));
			MessageLog.Open(EMessageSeverity::Warning);
		};
	}
}
#endif
