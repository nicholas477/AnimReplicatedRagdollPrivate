// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimReplicatedRagdollEditor.h"
#include "PropertyEditorModule.h"
#include "Details/FReplicatedRagdollBoneFilterDetails.h"
#include "AnimReplicatedRagdollTypes.h"

#define LOCTEXT_NAMESPACE "FAnimReplicatedRagdollEditorModule"

void FAnimReplicatedRagdollEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->RegisterCustomPropertyTypeLayout(
			FReplicatedRagdollBoneFilter::StaticStruct()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FReplicatedRagdollBoneFilterDetails::MakeInstance)
		);
	}
}

void FAnimReplicatedRagdollEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(FReplicatedRagdollBoneFilter::StaticStruct()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAnimReplicatedRagdollEditorModule, AnimReplicatedRagdollEditor)