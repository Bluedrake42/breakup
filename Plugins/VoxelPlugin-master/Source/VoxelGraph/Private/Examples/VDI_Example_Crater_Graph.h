// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGeneratedWorldGeneratorsIncludes.h"
#include "VDI_Example_Crater_Graph.generated.h"

UCLASS(Blueprintable)
class UVDI_Example_Crater_Graph : public UVoxelGraphGeneratorHelper
{
	GENERATED_BODY()
	
public:
	
	UVDI_Example_Crater_Graph();
	virtual TMap<FName, int32> GetDefaultSeeds() const override;
	virtual TVoxelSharedRef<FVoxelTransformableWorldGeneratorInstance> GetTransformableInstance() override;
};
