// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("Replicated Ragdoll"), STATGROUP_ReplicatedRagdoll, STATCAT_Advanced);

// Location quantization stats
DECLARE_DWORD_COUNTER_STAT(TEXT("Bone Locations (RoundWholeNumber)"), STAT_RagdollBoneLocations_RoundWholeNumber, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Bone Locations (RoundOneDecimal)"), STAT_RagdollBoneLocations_RoundOneDecimal, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Bone Locations (RoundTwoDecimals)"), STAT_RagdollBoneLocations_RoundTwoDecimals, STATGROUP_ReplicatedRagdoll);

// Rotation quantization stats
DECLARE_DWORD_COUNTER_STAT(TEXT("Bone Rotations (ByteComponents)"), STAT_RagdollBoneRotations_ByteComponents, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Bone Rotations (ShortComponents)"), STAT_RagdollBoneRotations_ShortComponents, STATGROUP_ReplicatedRagdoll);
