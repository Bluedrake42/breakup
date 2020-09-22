// Copyright 2020 Phyronnaz

#include "VoxelGraphNode.h"
#include "VoxelGraphGenerator.h"
#include "VoxelNode.h"
#include "VoxelNodes/VoxelLocalVariables.h"
#include "VoxelNodes/VoxelGraphMacro.h"
#include "VoxelGraphNodes/VoxelGraphNode_Knot.h"

#include "VoxelGraphEditorUtilities.h"
#include "IVoxelGraphEditorToolkit.h"
#include "VoxelGraphEditorCommands.h"
#include "VoxelEdGraph.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphEditorActions.h"

#include "Engine/Font.h"
#include "ScopedTransaction.h"
#include "Editor/EditorEngine.h"
#include "Toolkits/AssetEditorManager.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Launch/Resources/Version.h"

void UVoxelGraphNode::SetVoxelNode(UVoxelNode* InNode)
{
	check(InNode);
	VoxelNode = InNode;
	bCanRenameNode = VoxelNode->CanRenameNode();
}

void UVoxelGraphNode::PostCopyNode()
{
	// Make sure the VoxelNode goes back to being owned by the WorldGenerator after copying.
	ResetVoxelNodeOwner();
}

void UVoxelGraphNode::CreateInputPin()
{
	const int32 PinIndex = GetInputCount();

	UEdGraphPin* NewPin = CreatePin(EGPD_Input, FVoxelPinCategory::GetName(VoxelNode->GetInputPinCategory(PinIndex)), FName(), nullptr, VoxelNode->GetInputPinName(PinIndex));

	if (NewPin->PinName.IsNone())
	{
		// Makes sure pin has a name for lookup purposes but user will never see it
		NewPin->PinName = CreateUniquePinName(TEXT("Input"));
		NewPin->PinFriendlyName = FText::FromString(TEXT(" "));
	}

	NewPin->DefaultValue = VoxelNode->GetInputPinDefaultValue(PinIndex);
	if (NewPin->DefaultValue.IsEmpty())
	{
		NewPin->DefaultValue = FVoxelPinCategory::GetDefaultValue(VoxelNode->GetInputPinCategory(PinIndex));
	}
}

void UVoxelGraphNode::CreateOutputPin()
{
	const int32 PinIndex = GetOutputCount();

	UEdGraphPin* NewPin = CreatePin(EGPD_Output, FVoxelPinCategory::GetName(VoxelNode->GetOutputPinCategory(PinIndex)), FName(), nullptr, VoxelNode->GetOutputPinName(PinIndex));

	if (NewPin->PinName.IsNone())
	{
		// Makes sure pin has a name for lookup purposes but user will never see it
		NewPin->PinName = CreateUniquePinName(TEXT("Output"));
		NewPin->PinFriendlyName = FText::FromString(TEXT(" "));
	}
}

void UVoxelGraphNode::AddInputPin()
{
	const FScopedTransaction Transaction(VOXEL_LOCTEXT("Add Input Pin"));
	Modify();
	const int32 Increment = VoxelNode->GetInputPinsIncrement();
	VoxelNode->InputPinCount += Increment;
	ensure(VoxelNode->InputPinCount <= VoxelNode->GetMaxInputPins());
	for (int32 Index = 0; Index < Increment; ++Index)
	{
		CreateInputPin();
	}

	VoxelNode->OnInputPinCountModified();

	UVoxelGraphGenerator* Generator = CastChecked<UVoxelEdGraph>(GetGraph())->GetWorldGenerator();
	Generator->CompileVoxelNodesFromGraphNodes();

	// Refresh the current graph, so the pins can be updated
	GetGraph()->NotifyGraphChanged();
}

void UVoxelGraphNode::RemoveInputPin(UEdGraphPin* InGraphPin)
{
	const FScopedTransaction Transaction(VOXEL_LOCTEXT("Delete Input Pin"));
	Modify();

	for (auto* InputPin : GetInputPins())
	{
		if (InGraphPin == InputPin)
		{
			InGraphPin->MarkPendingKill();
			Pins.Remove(InGraphPin);

			const int32 Increment = VoxelNode->GetInputPinsIncrement();
			if (Increment > 1)
			{
				const int32 PinIndex = VoxelNode->GetInputPinIndex(InGraphPin->PinId);
				if (ensure(PinIndex != -1))
				{
					// Below = higher index!
					const int32 PinsBelow = (VoxelNode->InputPinCount - 1 - PinIndex) % Increment;
					const int32 PinsAbove = Increment - 1 - PinsBelow;
					for (int32 Index = PinIndex - PinsAbove; Index <= PinIndex + PinsBelow; Index++)
					{
						if (ensure(VoxelNode->InputPins.IsValidIndex(Index)))
						{
							const auto PinId = VoxelNode->InputPins[Index].PinId;
							Pins.RemoveAll([&](auto& ArrayPin) { return ArrayPin->PinId == PinId; });
						}
					}
				}
			}
			
			// also remove the VoxelNode child node so ordering matches
			VoxelNode->Modify();
			VoxelNode->InputPinCount -= Increment;
			ensure(VoxelNode->InputPinCount >= VoxelNode->GetMinInputPins());
			break;
		}
	}

	VoxelNode->OnInputPinCountModified();

	UVoxelGraphGenerator* Generator = CastChecked<UVoxelEdGraph>(GetGraph())->GetWorldGenerator();
	Generator->CompileVoxelNodesFromGraphNodes();

	// Refresh the current graph, so the pins can be updated
	GetGraph()->NotifyGraphChanged();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelGraphNode::CanSplitPin_Voxel(const UEdGraphPin& Pin) const
{
	return const_cast<UVoxelGraphNode*>(this)->TrySplitPin(const_cast<UEdGraphPin&>(Pin), true);
}

bool UVoxelGraphNode::CanCombinePin(const UEdGraphPin& Pin) const
{
	return const_cast<UVoxelGraphNode*>(this)->TryCombinePin(const_cast<UEdGraphPin&>(Pin), true);
}

bool UVoxelGraphNode::TrySplitPin(UEdGraphPin& Pin, bool bOnlyCheck)
{
	ensure(!Pin.bHidden);
	if (Pin.SubPins.Num() == 0 || Pin.LinkedTo.Num() > 0)
	{
		return false;
	}

	if (bOnlyCheck)
	{
		return true;
	}

	TArray<FString> SubDefaultValues;
	Pin.DefaultValue.ParseIntoArray(SubDefaultValues, TEXT(","));

	for (int32 Index = 0; Index < Pin.SubPins.Num(); Index++)
	{
		auto* SubPin = Pin.SubPins[Index];
		ensure(SubPin->bHidden);
		ensure(SubPin->ParentPin == &Pin);
		SubPin->bHidden = false;
		SubPin->ParentPin = nullptr;
		SubPin->DefaultValue = SubDefaultValues.IsValidIndex(Index) ? SubDefaultValues[Index] : "";
	}
	Pin.SubPins.Empty();

	ensure(RemovePin(&Pin));

	GetGraph()->NotifyGraphChanged();

	return true;
}

bool UVoxelGraphNode::TryCombinePin(UEdGraphPin& Pin, bool bOnlyCheck)
{
	ensure(!Pin.bHidden);

	const auto NeighborPins = Pin.Direction == EGPD_Input ? GetInputPins() : GetOutputPins();
	const int32 PinIndex = NeighborPins.Find(&Pin);

	if (!ensure(PinIndex != -1))
	{
		return false;
	}

	const auto CheckStart = [&](int32 Index)
	{
		if (!NeighborPins.IsValidIndex(Index) ||
			!NeighborPins.IsValidIndex(Index + 2))
		{
			return false;
		}

		FString Name = NeighborPins[Index]->GetName();
		if (!Name.RemoveFromStart("X"))
		{
			return false;
		}
		return
			NeighborPins[Index + 1]->GetName() == "Y" + Name &&
			NeighborPins[Index + 2]->GetName() == "Z" + Name;
	};
	const auto CheckEnd = [&](int32 Index)
	{
		if (!NeighborPins.IsValidIndex(Index) ||
			!NeighborPins.IsValidIndex(Index + 2))
		{
			return false;
		}

		FString Name = NeighborPins[Index]->GetName();
		if (!Name.RemoveFromEnd("X"))
		{
			return false;
		}
		return
			NeighborPins[Index + 1]->GetName() == Name + "Y" &&
			NeighborPins[Index + 2]->GetName() == Name + "Z";
	};

	int32 IndexX = -1;
	bool bIsStart = false;
	for (int32 Index = PinIndex - 2; Index <= PinIndex; Index++)
	{
		if (CheckStart(Index))
		{
			bIsStart = true;
			IndexX = Index;
			break;
		}
		if (CheckEnd(Index))
		{
			bIsStart = false;
			IndexX = Index;
			break;
		}
	}

	if (IndexX == -1)
	{
		return false;
	}
	
	for (int32 Index = 0; Index < 3; Index++)
	{
		if (NeighborPins[IndexX + Index]->LinkedTo.Num() > 0)
		{
			return false;
		}
	}

	if (bOnlyCheck)
	{
		return true;
	}

	FString ParentPinName = NeighborPins[IndexX]->GetName();
	if (bIsStart)
	{
		ensure(ParentPinName.RemoveFromStart("X"));
		ParentPinName.RemoveFromStart(".");
	}
	else 
	{
		ensure(ParentPinName.RemoveFromEnd("X"));
		ParentPinName.RemoveFromEnd(".");
	}

	auto* ParentPin = CreatePin(Pin.Direction, FVoxelPinCategory::GetName(EVoxelPinCategory::Vector), FName(), nullptr, *ParentPinName);
	Pins.Pop(false);

	FVector DefaultValue;
	for (int32 Index = 0; Index < 3; Index++)
	{
		auto* SubPin = NeighborPins[IndexX + Index];
		SubPin->bHidden = true;
		SubPin->ParentPin = ParentPin;

		DefaultValue[Index] = FCString::Atof(*SubPin->DefaultValue);

		ParentPin->SubPins.Add(SubPin);
	}
	ParentPin->DefaultValue = FString::Printf(TEXT("%f,%f,%f"), DefaultValue.X, DefaultValue.Y, DefaultValue.Z);

	// Add the parent before the sub pins
	const int32 InsertIndex = Pins.Find(NeighborPins[IndexX]);
	check(InsertIndex != -1);
	Pins.Insert(ParentPin, InsertIndex);

	GetGraph()->NotifyGraphChanged();

	return true;
}

void UVoxelGraphNode::CombineAll()
{
	const auto Copy = Pins;
	for (auto& Pin : Copy)
	{
		if (!Pin->bHidden)
		{
			TryCombinePin(*Pin, false);
		}
	}
}

bool UVoxelGraphNode::HasVectorPin(UVoxelNode& Node, EEdGraphPinDirection Direction)
{
	TArray<FString> Names;

	if (Direction == EGPD_Input)
	{
		const int32 InputCount = Node.GetMinInputPins();
		for (int32 Index = 0; Index < InputCount; Index++)
		{
			Names.Add(Node.GetInputPinName(Index).ToString());
		}
	}
	else
	{
		const int32 OutputCount = Node.GetOutputPinsCount();
		for (int32 Index = 0; Index < OutputCount; Index++)
		{
			Names.Add(Node.GetOutputPinName(Index).ToString());
		}
	}
	
	const auto CheckStart = [&](int32 Index)
	{
		FString Name = Names[Index];
		if (!Name.RemoveFromStart("X"))
		{
			return false;
		}
		return
			Names[Index + 1] == "Y" + Name &&
			Names[Index + 2] == "Z" + Name;
	};
	const auto CheckEnd = [&](int32 Index)
	{
		FString Name = Names[Index];
		if (!Name.RemoveFromEnd("X"))
		{
			return false;
		}
		return
			Names[Index + 1] == Name + "Y" &&
			Names[Index + 2] == Name + "Z";
	};

	for (int32 Index = 0; Index < Names.Num() - 2; Index++)
	{
		if (CheckStart(Index) || CheckEnd(Index))
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelGraphNode::CanAddInputPin() const
{
	if (VoxelNode)
	{
		const int32 MinPins = VoxelNode->GetMinInputPins();
		const int32 MaxPins = VoxelNode->GetMaxInputPins();
		if (MinPins == MaxPins)
		{
			return false;
		}
		else
		{
			return GetInputCount() < MaxPins;
		}
	}
	else
	{
		return false;
	}
}

bool UVoxelGraphNode::IsCompact() const
{
	return VoxelNode && VoxelNode->IsCompact();
}

FLinearColor UVoxelGraphNode::GetNodeBodyColor() const
{
	if (!IsNodeEnabled())
	{
		return FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);
	}
	if (VoxelNode)
	{
		for (auto& Pin : Pins)
		{
			if (Pin->bIsDiffing)
			{
				return FLinearColor(0.f, 0.f, 1.0f, 1.f);
			}
		}
		return VoxelNode->GetNodeBodyColor();
	}
	return FLinearColor::White;
}

bool UVoxelGraphNode::IsOutdated() const
{
	int32 InputIndex = 0;
	int32 OutputIndex = 0;
	for (auto* Pin : Pins)
	{
		if (Pin->SubPins.Num() > 0)
		{
			continue;
		}

		if (Pin->Direction == EGPD_Input)
		{
			if (FVoxelPinCategory::GetName(VoxelNode->GetInputPinCategory(InputIndex)) != Pin->PinType.PinCategory)
			{
				return true;
			}
			const FName PinName = VoxelNode->GetInputPinName(InputIndex);
			if (!PinName.IsNone() && PinName != Pin->PinName)
			{
				return true;
			}
			InputIndex++;
		}
		else
		{
			check(Pin->Direction == EGPD_Output);
			if (FVoxelPinCategory::GetName(VoxelNode->GetOutputPinCategory(OutputIndex)) != Pin->PinType.PinCategory)
			{
				return true;
			}
			const FName PinName = VoxelNode->GetOutputPinName(OutputIndex);
			if (!PinName.IsNone() && PinName != Pin->PinName)
			{
				return true;
			}
			OutputIndex++;
		}
	}
	return false;
}

void UVoxelGraphNode::CreateInputPins()
{
	if (VoxelNode)
	{
		VoxelNode->InputPinCount = FMath::Clamp(VoxelNode->InputPinCount, VoxelNode->GetMinInputPins(), VoxelNode->GetMaxInputPins());
		while (GetInputCount() < VoxelNode->InputPinCount)
		{
			CreateInputPin();
		}
	}
}

void UVoxelGraphNode::CreateOutputPins()
{
	if (VoxelNode)
	{
		while (GetOutputCount() < VoxelNode->GetOutputPinsCount())
		{
			CreateOutputPin();
		}
	}
}

FText UVoxelGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (VoxelNode)
	{
		if (TitleType == ENodeTitleType::EditableTitle)
		{
			return FText::FromString(VoxelNode->GetEditableName());
		}
		else
		{
			return VoxelNode->GetTitle();
		}
	}
	else
	{
		return Super::GetNodeTitle(TitleType);
	}
}

FLinearColor UVoxelGraphNode::GetNodeTitleColor() const
{
	if (VoxelNode)
	{
		return VoxelNode->GetColor();
	}
	else
	{
		return FLinearColor::Gray;
	}
}

void UVoxelGraphNode::PrepareForCopying()
{
	if (VoxelNode)
	{
		// Temporarily take ownership of the VoxelNode, so that it is not deleted when cutting
		VoxelNode->Rename(NULL, this, REN_DontCreateRedirectors);
	}
}

#if ENGINE_MINOR_VERSION < 24
void UVoxelGraphNode::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (Context.Pin)
	{
		if (VoxelNode)
		{
			// If on an input that can be deleted, show option
			if (Context.Pin->Direction == EGPD_Input) 
			{
				const int32 MinPins = VoxelNode->GetMinInputPins();
				const int32 MaxPins = VoxelNode->GetMaxInputPins();
				if (MinPins != MaxPins && MinPins < VoxelNode->InputPinCount) 
				{
					Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().DeleteInput);
				}
			}
			// Preview
			if (Context.Pin->Direction == EGPD_Output && FVoxelPinCategory::FromString(Context.Pin->PinType.PinCategory) == EVoxelPinCategory::Float)
			{
				Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().TogglePinPreview);
			}
			if (CanSplitPin_Voxel(*Context.Pin))
			{
				Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().SplitPin);
			}
			if (CanCombinePin(*Context.Pin))
			{
				Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().CombinePin);
			}
		}
	}
	else if (Context.Node)
	{		
		// Add a 'Convert to Local Variables' option to reroute nodes
		if (auto* Knot = Cast<UVoxelGraphNode_Knot>(this))
		{
			EVoxelPinCategory Category = FVoxelPinCategory::FromString(Knot->GetInputPin()->PinType.PinCategory);
			if (Category != EVoxelPinCategory::Exec && 
				Category != EVoxelPinCategory::Wildcard && 
				Category != EVoxelPinCategory::Vector)
			{
				Context.MenuBuilder->BeginSection("MaterialEditorMenu1");
				{
					Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().ConvertRerouteToVariables);
				}
				Context.MenuBuilder->EndSection();
			}
		}

		if (VoxelNode)
		{		
			// Add local variables selection & conversion to reroute nodes
			if (VoxelNode->IsA(UVoxelLocalVariableBase::StaticClass()))
			{
				Context.MenuBuilder->BeginSection("MaterialEditorMenu1");
				{
					if (VoxelNode->IsA(UVoxelLocalVariableDeclaration::StaticClass()))
					{
						Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().SelectLocalVariableUsages);
					}
					if (VoxelNode->IsA(UVoxelLocalVariableUsage::StaticClass()))
					{
						Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().SelectLocalVariableDeclaration);
					}
					Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().ConvertVariablesToReroute);
				}
				Context.MenuBuilder->EndSection();
			}
		}


		Context.MenuBuilder->BeginSection("VoxelGraphNodeEdit");
		{
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Delete);
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Cut);
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Copy);
			Context.MenuBuilder->AddMenuEntry(FGenericCommands::Get().Duplicate);
		}
		Context.MenuBuilder->EndSection();
		
		Context.MenuBuilder->BeginSection("VoxelGraphNodeMisc");
		{
			Context.MenuBuilder->AddMenuEntry(FVoxelGraphEditorCommands::Get().ReconstructNode);
		}
		Context.MenuBuilder->EndSection();

		Context.MenuBuilder->AddSubMenu(VOXEL_LOCTEXT("Alignment"), FText(), FNewMenuDelegate::CreateLambda([](FMenuBuilder& InMenuBuilder) {

			InMenuBuilder.BeginSection("EdGraphSchemaAlignment", VOXEL_LOCTEXT("Align"));
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesTop);
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesMiddle);
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesBottom);
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesLeft);
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesCenter);
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesRight);
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections);
			InMenuBuilder.EndSection();

			InMenuBuilder.BeginSection("EdGraphSchemaDistribution", VOXEL_LOCTEXT("Distribution"));
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesHorizontally);
			InMenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesVertically);
			InMenuBuilder.EndSection();
		}));
	}
}
#endif

FText UVoxelGraphNode::GetTooltipText() const
{
	if (VoxelNode)
	{
		return VoxelNode->GetTooltip();
	}
	else
	{
		return GetNodeTitle(ENodeTitleType::ListView);
	}
}

FString UVoxelGraphNode::GetDocumentationExcerptName() const
{
	// Default the node to searching for an excerpt named for the C++ node class name, including the U prefix.
	// This is done so that the excerpt name in the doc file can be found by find-in-files when searching for the full class name.
	UClass* MyClass = (VoxelNode != NULL) ? VoxelNode->GetClass() : this->GetClass();
	return FString::Printf(TEXT("%s%s"), MyClass->GetPrefixCPP(), *MyClass->GetName());
}

bool UVoxelGraphNode::CanUserDeleteNode() const
{
	return !VoxelNode || VoxelNode->CanUserDeleteNode();
}

bool UVoxelGraphNode::CanDuplicateNode() const
{
	return !VoxelNode || VoxelNode->CanDuplicateNode();
}

bool UVoxelGraphNode::CanJumpToDefinition() const
{
	return VoxelNode && ((VoxelNode->IsA(UVoxelGraphMacroNode::StaticClass()) && CastChecked<UVoxelGraphMacroNode>(VoxelNode)->Macro) || VoxelNode->IsA<UVoxelLocalVariableUsage>());
}

void UVoxelGraphNode::JumpToDefinition() const
{
	if (auto* Macro = Cast<UVoxelGraphMacroNode>(VoxelNode))
	{
#if ENGINE_MINOR_VERSION < 24
		FAssetEditorManager::Get().OpenEditorForAsset(Macro->Macro);
#else
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Macro->Macro);
#endif
	}
	else if (auto* Usage = Cast<UVoxelLocalVariableUsage>(VoxelNode))
	{
		if (Usage->Declaration)
		{
			FVoxelGraphEditorUtilities::GetIVoxelEditorForGraph(VoxelNode->Graph->VoxelGraph)->SelectNodesAndZoomToFit({ Usage->Declaration->GraphNode });
		}
	}
}

void UVoxelGraphNode::OnRenameNode(const FString& NewName)
{
	if (VoxelNode)
	{
		VoxelNode->Modify();
		VoxelNode->SetEditableName(NewName);
		VoxelNode->MarkPackageDirty();
	}
}

void UVoxelGraphNode::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	if (!VoxelNode)
	{
		return;
	}
	
	TArray<FGuid> PinIds;

	PinIds.Add(Pin.PinId);
	for (auto& SubPin : Pin.SubPins)
	{
		PinIds.Add(SubPin->PinId);
	}

	for (auto& PinId : PinIds)
	{
		int32 Index = VoxelNode->GetInputPinIndex(PinId);
		if (Index != -1)
		{
			if (!HoverTextOut.IsEmpty()) HoverTextOut += "\n";
			HoverTextOut += VoxelNode->GetInputPinToolTip(Index);
		}
		else
		{
			Index = VoxelNode->GetOutputPinIndex(PinId);
			if (Index != -1)
			{
				if (!HoverTextOut.IsEmpty()) HoverTextOut += "\n";
				HoverTextOut += VoxelNode->GetOutputPinToolTip(Index);
			}
		}
	}
}

void UVoxelGraphNode::PostLoad()
{
	Super::PostLoad();

	// Fixup any VoxelNode back pointers that may be out of date
	if (VoxelNode)
	{
		VoxelNode->GraphNode = this;
	}

	for (int32 Index = 0; Index < Pins.Num(); ++Index)
	{
		UEdGraphPin* Pin = Pins[Index];
		Pin->PinType.bIsConst = false;
		Pin->PinType.ContainerType = EPinContainerType::None; // Remove preview
		if (Pin->PinName.IsNone())
		{
			// Makes sure pin has a name for lookup purposes but user will never see it
			if (Pin->Direction == EGPD_Input)
			{
				Pin->PinName = CreateUniquePinName(TEXT("Input"));
			}
			else
			{
				Pin->PinName = CreateUniquePinName(TEXT("Output"));
			}
			Pin->PinFriendlyName = FText::FromString(TEXT(" "));
		}
	}
}

void UVoxelGraphNode::PostEditImport()
{
	// Make sure this VoxelNode is owned by the WorldGenerator it's being pasted into.
	ResetVoxelNodeOwner();
}

void UVoxelGraphNode::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		CreateNewGuid();
	}
}

void UVoxelGraphNode::ResetVoxelNodeOwner()
{
	if (VoxelNode)
	{
		UVoxelGraphGenerator* Generator = CastChecked<UVoxelEdGraph>(GetGraph())->GetWorldGenerator();

		if (VoxelNode->GetOuter() != Generator)
		{
			// Ensures VoxelNode is owned by the WorldGenerator
			VoxelNode->Rename(NULL, Generator, REN_DontCreateRedirectors);
		}

		// Set up the back pointer for newly created voxel nodes
		VoxelNode->GraphNode = this;
	}
}