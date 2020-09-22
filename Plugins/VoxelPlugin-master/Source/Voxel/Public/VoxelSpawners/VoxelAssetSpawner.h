// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelIntBox.h"
#include "VoxelSpawners/VoxelBasicSpawner.h"
#include "VoxelWorldGenerators/VoxelWorldGeneratorPicker.h"
#include "VoxelAssetSpawner.generated.h"

class UVoxelAssetSpawner;
class UVoxelTransformableWorldGenerator;
class FVoxelTransformableWorldGeneratorInstance;
class FVoxelAssetSpawnerProxy;

template<typename T>
class TVoxelDataItemWrapper;

struct FVoxelAssetItem;


UCLASS()
class VOXEL_API UVoxelAssetSpawner : public UVoxelBasicSpawner
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	FVoxelTransformableWorldGeneratorPicker Generator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	FVoxelIntBox GeneratorLocalBounds = FVoxelIntBox(-25, 25);

	// The voxel world seeds will be sent to the generator.
	// Add the names of the seeds you want to be randomized here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	TArray<FName> Seeds;

	// All generators are created at begin play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings", meta = (ClampMin = 1))
	int32 NumberOfDifferentSeedsToUse = 1;

	// Priority of the spawned assets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General Settings")
	bool bRoundAssetPosition = false;
	
public:
	//~ Begin UVoxelSpawner Interface
#if WITH_EDITOR
	virtual bool NeedsToRebuild(UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent) override { return Object == Generator.GetObject(); }
#endif
};