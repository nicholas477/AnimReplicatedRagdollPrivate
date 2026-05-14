// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimReplicatedRagdollHelpers.h"

#include "Engine/ReplicatedState.h"

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
}
