// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelTools/VoxelToolHelpers.h"
#include "VoxelTools/VoxelSurfaceTools.h"

class FVoxelSurfaceToolsImpl
{
public:
	static void ApplyConstantStrengthImpl(
		TArray<FVoxelSurfaceEditsVoxel>& Voxels,
		const float Strength)
	{
		VOXEL_TOOL_FUNCTION_COUNTER(Voxels.Num());
		for (auto& Voxel : Voxels)
		{
			Voxel.Strength *= Strength;
		}
	}
	template<typename T>
	static void ApplyStrengthFunctionImpl(
		TArray<FVoxelSurfaceEditsVoxel>& Voxels,
		T GetStrength)
	{
		VOXEL_TOOL_FUNCTION_COUNTER(Voxels.Num());
		int32 Num = 0;
		for (const auto& Voxel : Voxels)
		{
			const float Strength = GetStrength(Voxel);
			if (Strength != 0)
			{
				auto& NewVoxel = Voxels[Num++];
				NewVoxel = Voxel;
				NewVoxel.Strength *= Strength;
			}
		}
		check(Num <= Voxels.Num());
		Voxels.SetNum(Num, false);
	}
	template<typename T>
	static void ApplyDistanceStrengthFunctionImpl(
		TArray<FVoxelSurfaceEditsVoxel>& Voxels,
		const FVoxelVector& Center,
		bool b2D,
		T GetStrengthFromDistance)
	{
		if (b2D)
		{
			ApplyStrengthFunctionImpl(Voxels, [=](const FVoxelSurfaceEditsVoxel& Voxel)
			{
				return GetStrengthFromDistance(FVector2D::Distance(FVector2D(Center.X, Center.Y), FVector2D(Voxel.Position.X, Voxel.Position.Y)));
			});
		}
		else
		{
			ApplyStrengthFunctionImpl(Voxels, [=](const FVoxelSurfaceEditsVoxel& Voxel)
			{
				return GetStrengthFromDistance(FVoxelVector::Distance(Center, FVoxelVector(Voxel.Position)));
			});
		}
	}

	// Should always be called last if bExactDistanceField = true
	template<typename T>
	static void ApplySDFImpl(
		const FVoxelSurfaceEditsVoxelsInfo& Info,
		TArray<FVoxelSurfaceEditsVoxel>& Voxels,
		EVoxelSDFMergeMode MergeMode,
		T GetDistance)
	{
		VOXEL_TOOL_FUNCTION_COUNTER(Voxels.Num());
		for (auto& Voxel : Voxels)
		{
			const float CurrentDistance = Voxel.Value;
			const float OtherDistance = GetDistance(FVector(Voxel.Position));
			const float WantedDistance =
				MergeMode == EVoxelSDFMergeMode::Union
				? FMath::Min(CurrentDistance, OtherDistance)
				: MergeMode == EVoxelSDFMergeMode::Intersection
				? FMath::Max(OtherDistance, CurrentDistance)
				: OtherDistance; // Override

			if (Info.bHasExactDistanceField)
			{
				// No strength should be applied after ApplySDFImpl if we want a good result
				const float IntermediateDistance = FMath::Lerp(CurrentDistance, WantedDistance, Voxel.Strength);
				Voxel.Strength = IntermediateDistance - CurrentDistance;
			}
			else
			{
				const float Difference = WantedDistance - Voxel.Value;
				// We cannot go too fast if we didn't compute the exact distance field
				Voxel.Strength *= FMath::Clamp(Difference, -1.f, 1.f);
			}
		}
	}
	static void ApplyTerraceImpl(
		TArray<FVoxelSurfaceEditsVoxel>& Voxels,
		const int32 TerraceHeightInVoxels,
		const float Angle,
		const int32 ImmutableVoxels)
	{
		VOXEL_TOOL_FUNCTION_COUNTER(Voxels.Num());
		
		if (!ensure(TerraceHeightInVoxels >= 1)) return;
		const float AngleLimit = FMath::DegreesToRadians(Angle);

		int32 NewNum = 0;
		for (auto& Voxel : Voxels)
		{
			const int32 RelativePosition = FVoxelUtilities::PositiveMod(Voxel.Position.Z, TerraceHeightInVoxels);
			if (RelativePosition < ImmutableVoxels) continue;
			
			const float VoxelAngle = FMath::Acos(Voxel.Normal.Z); // Dot product with UpVector. 0 when facing up, PI when facing down
			ensure(VoxelAngle >= 0);
			if (AngleLimit < VoxelAngle) continue;

			// We want 1 went facing up, 0 when facing > angle limit
			const float Strength = FMath::Max((AngleLimit - VoxelAngle) / AngleLimit, 0.f);

			auto NewVoxel = Voxel;
			NewVoxel.Strength *= Strength;
			Voxels[NewNum++] = NewVoxel;
		}
		check(NewNum <= Voxels.Num());
		Voxels.SetNum(NewNum, false);
	}
	static void ApplyFlattenImpl(
		const FVoxelSurfaceEditsVoxelsInfo& Info,
		TArray<FVoxelSurfaceEditsVoxel>& Voxels,
		const FPlane& Plane,
		EVoxelSDFMergeMode MergeMode)
	{
		ApplySDFImpl(Info, Voxels, MergeMode, [&](const FVector& Position) { return Plane.PlaneDot(Position); });
	}
};