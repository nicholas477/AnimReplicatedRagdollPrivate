// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

namespace AnimReplicatedRagdollHelpers
{
	void QuantizeAndWriteLocation(FBitArchive& Writer, const FVector& Location, EVectorQuantization LocationQuantization);
	void QuantizeAndWriteRotation(FBitArchive& Writer, const FRotator& Rotation, ERotatorQuantization RotationQuantization);
	void ReadAndDequantizeLocation(FBitArchive& Reader, FVector& Location, EVectorQuantization LocationQuantization);
	void ReadAndDequantizeRotation(FBitArchive& Reader, FRotator& Rotation, ERotatorQuantization RotationQuantization);
}