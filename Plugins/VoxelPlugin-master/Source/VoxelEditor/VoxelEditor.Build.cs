// Copyright 2020 Phyronnaz

using System.IO;
using UnrealBuildTool;

public class VoxelEditor : ModuleRules
{
    public VoxelEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnforceIWYU = true;
        bLegacyPublicIncludePaths = false;

#if UE_4_24_OR_LATER
#else
#endif

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "VoxelGraphEditor",
                "AssetRegistry",
            });

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "Voxel",
                "VoxelGraph",
                "VoxelEditorDefault",
                "Engine",
                "Landscape",
                "LandscapeEditor",
                "PlacementMode",
                "AdvancedPreviewScene",
                "DesktopPlatform",
                "UnrealEd",
                "InputCore",
                "ImageWrapper",
                "Slate",
                "SlateCore",
                "PropertyEditor",
                "EditorStyle",
                "Projects",
                "RHI",
                "MessageLog",
                "RawMesh",
                "DetailCustomizations",
                "WorkspaceMenuStructure",
                "BlueprintGraph",
                "KismetCompiler",
                "ApplicationCore",
                "EngineSettings"
            });

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "VoxelGraphEditor"
            });
    }
}
