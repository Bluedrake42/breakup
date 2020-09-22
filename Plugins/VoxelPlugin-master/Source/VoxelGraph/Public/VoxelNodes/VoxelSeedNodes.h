// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelNodeHelper.h"
#include "VoxelNodeHelperMacros.h"
#include "VoxelSeedNodes.generated.h"

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelSeedNode : public UVoxelNodeHelper
{
	GENERATED_BODY()
	
public:
	UVoxelSeedNode();
};

// Seed parameter
UCLASS(DisplayName = "Seed", Category = "Seed")
class VOXELGRAPH_API UVoxelNode_Seed : public UVoxelSeedNode
{
	GENERATED_BODY()
	GENERATED_VOXELNODE_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Voxel")
	int32 DefaultValue = 1443;

	UPROPERTY(EditAnywhere, Category = "Voxel")
	FName Name = "SeedName";

	UVoxelNode_Seed();

	//~ Begin UVoxelNode Interface
	virtual FText GetTitle() const override;
	virtual bool CanRenameNode() const override;
	virtual FString GetEditableName() const override;
	virtual void SetEditableName(const FString& NewName) override;
	//~ End UVoxelNode Interface
};

// Combine seeds by hashing them
UCLASS(DisplayName = "Hash Seeds", Category = "Seed", meta = (Keywords = "combine add seed"))
class VOXELGRAPH_API UVoxelNode_AddSeeds : public UVoxelSeedNode
{
	GENERATED_BODY()
	GENERATED_VOXELNODE_BODY()
	COMPACT_VOXELNODE("HASH")

	UVoxelNode_AddSeeds();
};

// Make several new seeds from a single one
UCLASS(DisplayName = "Make Seeds", Category = "Seed", meta = (Keywords = "make combine add seed"))
class VOXELGRAPH_API UVoxelNode_MakeSeeds : public UVoxelSeedNode
{
	GENERATED_BODY()
	GENERATED_VOXELNODE_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Voxel", meta = (ClampMin = 2, ClampMax = 32, ReconstructNode))
	int32 NumOutputs = 5;

	UVoxelNode_MakeSeeds();
	
	//~ Begin UVoxelNode Interface
	virtual int32 GetOutputPinsCount() const override { return NumOutputs; }
	//~ End UVoxelNode Interface
};