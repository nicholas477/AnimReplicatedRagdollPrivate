// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("Replicated Ragdoll"), STATGROUP_ReplicatedRagdoll, STATCAT_Advanced);

DECLARE_DWORD_COUNTER_STAT(TEXT("Multithreaded Encode Tasks"), STAT_RagdollMultithreadedEncodeTasks, STATGROUP_ReplicatedRagdoll);

// Location quantization stats
DECLARE_DWORD_COUNTER_STAT(TEXT("Decoding: Bone Locations (RoundWholeNumber)"), STAT_Decoding_RagdollBoneLocations_RoundWholeNumber, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Decoding: Bone Locations (RoundOneDecimal)"), STAT_Decoding_RagdollBoneLocations_RoundOneDecimal, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Decoding: Bone Locations (RoundTwoDecimals)"), STAT_Decoding_RagdollBoneLocations_RoundTwoDecimals, STATGROUP_ReplicatedRagdoll);

DECLARE_DWORD_COUNTER_STAT(TEXT("Encoding: Bone Locations (RoundWholeNumber)"), STAT_Encoding_RagdollBoneLocations_RoundWholeNumber, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Encoding: Bone Locations (RoundOneDecimal)"), STAT_Encoding_RagdollBoneLocations_RoundOneDecimal, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Encoding: Bone Locations (RoundTwoDecimals)"), STAT_Encoding_RagdollBoneLocations_RoundTwoDecimals, STATGROUP_ReplicatedRagdoll);

// Rotation quantization stats
DECLARE_DWORD_COUNTER_STAT(TEXT("Decoding: Bone Rotations (ByteComponents)"), STAT_Decoding_RagdollBoneRotations_ByteComponents, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Decoding: Bone Rotations (ShortComponents)"), STAT_Decoding_RagdollBoneRotations_ShortComponents, STATGROUP_ReplicatedRagdoll);

DECLARE_DWORD_COUNTER_STAT(TEXT("Encoding: Bone Rotations (ByteComponents)"), STAT_Encoding_RagdollBoneRotations_ByteComponents, STATGROUP_ReplicatedRagdoll);
DECLARE_DWORD_COUNTER_STAT(TEXT("Encoding: Bone Rotations (ShortComponents)"), STAT_Encoding_RagdollBoneRotations_ShortComponents, STATGROUP_ReplicatedRagdoll);
