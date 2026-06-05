// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimReplicatedRagdollHelpers.h"

#include "Engine/ReplicatedState.h"
#include "AnimReplicatedRagdollSettings.h"

namespace AnimReplicatedRagdollHelpers
{
	void QuantizeAndWriteLocation(FArchive& Writer, const FVector& Location, EVectorQuantization LocationQuantization)
	{
		FVector LocationToWrite = Location;
		switch (LocationQuantization)
		{
		case EVectorQuantization::RoundWholeNumber:
			SerializePackedVector<1, 24>(LocationToWrite, Writer);
			break;
		case EVectorQuantization::RoundOneDecimal:
			SerializePackedVector<10, 27>(LocationToWrite, Writer);
			break;
		case EVectorQuantization::RoundTwoDecimals:
			SerializePackedVector<100, 30>(LocationToWrite, Writer);
			break;
		default:
			break;
		}
	}

	void QuantizeAndWriteRotation(FArchive& Writer, const FRotator& Rotation, ERotatorQuantization RotationQuantization)
	{
		FRotator RotationToWrite = Rotation;
		switch (RotationQuantization)
		{
		case ERotatorQuantization::ByteComponents:
			RotationToWrite.SerializeCompressed(Writer);
			break;
		case ERotatorQuantization::ShortComponents:
			RotationToWrite.SerializeCompressedShort(Writer);
			break;
		default:
			break;
		}
	}

	void ReadAndDequantizeLocation(FArchive& Reader, FVector& Location, EVectorQuantization LocationQuantization)
	{
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
	}

	void ReadAndDequantizeRotation(FArchive& Reader, FRotator& Rotation, ERotatorQuantization RotationQuantization)
	{
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
	}

	FPIEEditorErrorFlag::FPIEEditorErrorFlag(const FName& InSettingFlag)
		: SettingFlag(InSettingFlag)
	{
#if WITH_EDITOR
		if (!OnPIEExitHandle.IsValid())
		{
			OnPIEExitHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FPIEEditorErrorFlag::ResetError);
		}
#endif
	}

	FPIEEditorErrorFlag::~FPIEEditorErrorFlag()
	{
#if WITH_EDITOR
		if (OnPIEExitHandle.IsValid())
		{
			FEditorDelegates::ShutdownPIE.Remove(OnPIEExitHandle);
			OnPIEExitHandle.Reset();
		}
#endif
	}

	FPIEEditorErrorFlag::FPIEEditorErrorFlag(const FPIEEditorErrorFlag& Other)
	{
		bHasCalledError = Other.bHasCalledError;

#if WITH_EDITOR
		if (OnPIEExitHandle.IsValid())
		{
			FEditorDelegates::ShutdownPIE.Remove(OnPIEExitHandle);
			OnPIEExitHandle.Reset();
		}

		if (OnPIEExitHandle.IsValid())
		{
			OnPIEExitHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FPIEEditorErrorFlag::ResetError);
		}
#endif
	}

	FPIEEditorErrorFlag& FPIEEditorErrorFlag::operator=(const FPIEEditorErrorFlag& Other)
	{
		bHasCalledError = Other.bHasCalledError;

#if WITH_EDITOR
		if (OnPIEExitHandle.IsValid())
		{
			FEditorDelegates::ShutdownPIE.Remove(OnPIEExitHandle);
			OnPIEExitHandle.Reset();
		}

		if (Other.OnPIEExitHandle.IsValid())
		{
			OnPIEExitHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FPIEEditorErrorFlag::ResetError);
		}
#endif

		return *this;
	}

	FPIEEditorErrorFlag::FPIEEditorErrorFlag(FPIEEditorErrorFlag&& Other)
	{
		bHasCalledError = Other.bHasCalledError;

#if WITH_EDITOR
		if (OnPIEExitHandle.IsValid())
		{
			FEditorDelegates::ShutdownPIE.Remove(OnPIEExitHandle);
			OnPIEExitHandle.Reset();
		}

		if (Other.OnPIEExitHandle.IsValid())
		{
			OnPIEExitHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FPIEEditorErrorFlag::ResetError);
		}
#endif
	}

	FPIEEditorErrorFlag& FPIEEditorErrorFlag::operator=(FPIEEditorErrorFlag&& Other)
	{
		bHasCalledError = Other.bHasCalledError;

#if WITH_EDITOR
		if (OnPIEExitHandle.IsValid())
		{
			FEditorDelegates::ShutdownPIE.Remove(OnPIEExitHandle);
			OnPIEExitHandle.Reset();
		}

		if (Other.OnPIEExitHandle.IsValid())
		{
			OnPIEExitHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FPIEEditorErrorFlag::ResetError);
		}
#endif

		return *this;
	}

	FPIEEditorErrorFlag::operator bool() const
	{
		return bHasCalledError && GetErrorFlagEnabled();
	}

	void FPIEEditorErrorFlag::ResetError(bool)
	{
		bHasCalledError = false;
	}

	bool FPIEEditorErrorFlag::GetErrorFlagEnabled() const
	{
		if (SettingFlag.IsNone())
		{
			return true;
		}

		const UAnimReplicatedRagdollSettings* Settings = GetDefault<UAnimReplicatedRagdollSettings>();
		const FBoolProperty* Property = CastField<FBoolProperty>(Settings->GetClass()->FindPropertyByName(SettingFlag));
		if (Property == nullptr)
		{
			return true;
		}

		bool bErrorFlagEnabled = false;
		Property->GetValue_InContainer(Settings, &bErrorFlagEnabled);

		return bErrorFlagEnabled;
	}
}
