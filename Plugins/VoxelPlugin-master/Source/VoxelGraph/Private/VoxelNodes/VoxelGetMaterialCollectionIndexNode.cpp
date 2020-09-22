// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelGetMaterialCollectionIndexNode.h"
#include "CppTranslation/VoxelVariables.h"
#include "NodeFunctions/VoxelNodeFunctions.h"
#include "VoxelWorldGenerators/VoxelWorldGeneratorInit.h"
#include "VoxelRender/MaterialCollections/VoxelMaterialCollectionBase.h"
#include "VoxelGraphGenerator.h"

#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "UObject/Package.h"
#include "AssetData.h"
#include "VoxelGraphPreviewSettings.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"

UVoxelNode_GetMaterialCollectionIndex::UVoxelNode_GetMaterialCollectionIndex()
{
	SetOutputs(EVoxelPinCategory::Int);
}

FText UVoxelNode_GetMaterialCollectionIndex::GetTitle() const
{
	return FText::Format(VOXEL_LOCTEXT("Get Material Collection Index: {0}"), Super::GetTitle());
}

UObject* UVoxelNode_GetMaterialCollectionIndex::GetAsset() const
{
	return Material;
}

UClass* UVoxelNode_GetMaterialCollectionIndex::GetAssetClass() const
{
	return UObject::StaticClass();
}

void UVoxelNode_GetMaterialCollectionIndex::SetAsset(UObject* Object)
{
	Material = Cast<UMaterialInterface>(Object);

	UpdatePreview(false);
}

bool UVoxelNode_GetMaterialCollectionIndex::ShouldFilterAsset(const FAssetData& Asset) const
{
	if (!ensure(Graph) || !Graph->PreviewSettings->MaterialCollection)
	{
		return true;
	}

	return Graph->PreviewSettings->MaterialCollection->GetMaterialIndex(Asset.AssetName) == -1;
}

