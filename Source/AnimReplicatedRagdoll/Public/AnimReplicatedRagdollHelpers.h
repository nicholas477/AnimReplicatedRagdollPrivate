// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

namespace AnimReplicatedRagdollHelpers
{
	void QuantizeAndWriteLocation(FArchive& Writer, const FVector& Location, EVectorQuantization LocationQuantization);
	void QuantizeAndWriteRotation(FArchive& Writer, const FRotator& Rotation, ERotatorQuantization RotationQuantization);
	void ReadAndDequantizeLocation(FArchive& Reader, FVector& Location, EVectorQuantization LocationQuantization);
	void ReadAndDequantizeRotation(FArchive& Reader, FRotator& Rotation, ERotatorQuantization RotationQuantization);
}