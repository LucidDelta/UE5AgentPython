// Copyright (c) UE5AgentPython. All Rights Reserved.

using UnrealBuildTool;

public class UE5AgentPython : ModuleRules
{
	public UE5AgentPython(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EditorStyle",
			"EditorWidgets",
			"LevelEditor",
			"ToolMenus",
			"ContentBrowser",
			"ContentBrowserData",
			"UnrealEd",
			"PythonScriptPlugin",
			"Projects",
			"ApplicationCore",
			"WorkspaceMenuStructure",
			"EditorFramework",
			"AssetRegistry",
			"AssetTools",
			"DesktopPlatform",
			"HTTP",
			"Json",
			"JsonUtilities"
		});
	}
}
