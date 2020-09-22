// Copyright 2020 Phyronnaz

#include "VoxelNode.h"
#include "IVoxelGraphEditor.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "EdGraph/EdGraphNode.h"

#if WITH_EDITOR
void UVoxelGraphNodeInterface::PostLoad()
{
	Super::PostLoad();
	ErrorMsg.Empty();
}

void UVoxelGraphNodeInterface::ReconstructNode()
{
	Super::ReconstructNode();

	InfoMsg.Empty();
	WarningMsg.Empty();
	ErrorMsg.Empty();
}
#endif

int32 UVoxelNode::GetInputPinIndex(const FGuid& PinId)
{
	for (int32 I = 0; I < InputPins.Num(); I++)
	{
		if (InputPins[I].PinId == PinId)
		{
			return I;
		}
	}
	return -1;
}

int32 UVoxelNode::GetOutputPinIndex(const FGuid& PinId)
{
	for (int32 I = 0; I < OutputPins.Num(); I++)
	{
		if (OutputPins[I].PinId == PinId)
		{
			return I;
		}
	}
	return -1;
}

bool UVoxelNode::HasInputPinWithCategory(EVoxelPinCategory Category) const
{
	for (int32 i = 0; i < GetMinInputPins(); i++)
	{
		if (GetInputPinCategory(i) == Category)
		{
			return true;
		}
	}
	return false;
}

bool UVoxelNode::HasOutputPinWithCategory(EVoxelPinCategory Category) const
{
	for (int32 i = 0; i < GetOutputPinsCount(); i++)
	{
		if (GetOutputPinCategory(i) == Category)
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FText UVoxelNode::GetTitle() const
{
#if WITH_EDITOR
	return GetClass()->GetDisplayNameText();
#else
	return FText::GetEmpty();
#endif
}

FText UVoxelNode::GetTooltip() const
{
#if WITH_EDITOR
	return GetClass()->GetToolTipText();
#else
	return FText::GetEmpty();
#endif
}


void UVoxelNode::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	if (IsOutdated())
	{
		ErrorReporter.AddMessageToNode(this, "outdated node, please right click and press Reconstruct Node", EVoxelGraphNodeMessageType::Error);
	}
}

#if WITH_EDITOR
void UVoxelNode::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (Graph && GraphNode && PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		bool bReconstructNode = false;
		for (auto* Property : PropertyChangedEvent.PropertyChain)
		{
			if (Property && Property->HasMetaData(STATIC_FNAME("ReconstructNode")))
			{
				bReconstructNode = true;
				break;
			}
		}
		UpdatePreview(bReconstructNode);
	}

	MarkPackageDirty();
}

void UVoxelNode::PostLoad()
{
	Super::PostLoad();
	// Make sure voxel nodes are transactional (so they work with undo system)
	SetFlags(RF_Transactional);

	InputPinCount = FMath::Clamp(InputPinCount, GetMinInputPins(), GetMaxInputPins());

	if (IsOutdated())
	{
		FVoxelGraphErrorReporter::AddMessageToNodeInternal(this, "outdated node, please right click and press Reconstruct Node", EVoxelGraphNodeMessageType::Error);
	}
}
#endif

void UVoxelNode::UpdatePreview(bool bReconstructNode) const
{
#if WITH_EDITOR
	if (bReconstructNode)
	{
		// Reconstruct before updating preview to have the right output count
		GraphNode->ReconstructNode();
		Graph->CompileVoxelNodesFromGraphNodes();
	}

	IVoxelGraphEditor::GetVoxelGraphEditor()->UpdatePreview(Graph, EVoxelGraphPreviewFlags::UpdateTextures);
#endif
}

bool UVoxelNode::IsOutdated() const
{
	if (InputPins.Num() < GetMinInputPins() ||
		InputPins.Num() > GetMaxInputPins() ||
		InputPins.Num() != InputPinCount ||
		OutputPins.Num() != GetOutputPinsCount())
	{
		return true;
	}
	
#if WITH_EDITORONLY_DATA
	if (GraphNode && GraphNode->IsOutdated())
	{
		return true;
	}
#endif

	return false;
}