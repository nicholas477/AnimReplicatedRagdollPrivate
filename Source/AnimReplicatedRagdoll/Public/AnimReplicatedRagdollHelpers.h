// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

namespace AnimReplicatedRagdollHelpers
{
	void QuantizeAndWriteLocation(FArchive& Writer, const FVector& Location, EVectorQuantization LocationQuantization);
	void QuantizeAndWriteRotation(FArchive& Writer, const FRotator& Rotation, ERotatorQuantization RotationQuantization);
	void ReadAndDequantizeLocation(FArchive& Reader, FVector& Location, EVectorQuantization LocationQuantization);
	void ReadAndDequantizeRotation(FArchive& Reader, FRotator& Rotation, ERotatorQuantization RotationQuantization);

	// Helper for tracking error throwing on PIE.
	// Makes it so that an error is only thrown once per PIE session.
	struct FPIEEditorErrorFlag
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="InSettingFlag">The name of the boolean parameter in UAnimReplicatedRagdollSettings that enables this error flag</param>
		FPIEEditorErrorFlag(const FName& InSettingFlag = NAME_None);
		~FPIEEditorErrorFlag();

		FPIEEditorErrorFlag(const FPIEEditorErrorFlag&);
		FPIEEditorErrorFlag& operator=(const FPIEEditorErrorFlag&);

		FPIEEditorErrorFlag(FPIEEditorErrorFlag&&);
		FPIEEditorErrorFlag& operator=(FPIEEditorErrorFlag&&);

		void SetSettingFlag(const FName& InSettingFlag) { SettingFlag = InSettingFlag; }

		void SetCalledError() {
			bHasCalledError = true;
		}
		operator bool() const;

	protected:
		FName SettingFlag = NAME_None;
		bool bHasCalledError = false;
		mutable FDelegateHandle OnPIEExitHandle;

		void ResetError(bool);
		bool GetErrorFlagEnabled() const;
	};
}