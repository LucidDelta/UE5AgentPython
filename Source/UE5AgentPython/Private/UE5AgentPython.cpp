// Copyright (c) UE5AgentPython. All Rights Reserved.

#include "UE5AgentPython.h"
#include "SUE5AgentPythonPanel.h"
#include "UE5AgentPythonStyle.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "UE5AgentPython"

const FName FUE5AgentPythonModule::PanelTabName(TEXT("UE5AgentPythonPanel"));

void FUE5AgentPythonModule::StartupModule()
{
	FUE5AgentPythonStyle::Initialize();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			PanelTabName,
			FOnSpawnTab::CreateRaw(this, &FUE5AgentPythonModule::SpawnPanelTab))
		.SetDisplayName(LOCTEXT("PanelTabTitle", "UE5 Agent Python"))
		.SetTooltipText(LOCTEXT("PanelTabTooltip", "Open the UE5 Agent Python scripting assistant."))
		.SetIcon(FSlateIcon(FUE5AgentPythonStyle::GetStyleSetName(), "UE5AgentPython.MenuIcon"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUE5AgentPythonModule::RegisterMenus));
}

void FUE5AgentPythonModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	if (UToolMenus* Menus = UToolMenus::Get())
	{
		Menus->UnregisterOwnerByName(TEXT("UE5AgentPythonMenuOwner"));
	}

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PanelTabName);
	}

	FUE5AgentPythonStyle::Shutdown();
}

void FUE5AgentPythonModule::RegisterMenus()
{
	// Clear any previous registration (handles live-coding reloads and stale entries
	// from older builds that used different section names).
	UToolMenus::Get()->UnregisterOwnerByName(TEXT("UE5AgentPythonMenuOwner"));

	FToolMenuOwnerScoped OwnerScoped(FName(TEXT("UE5AgentPythonMenuOwner")));

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	if (!ToolsMenu)
	{
		return;
	}

	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("Tools"));

	Section.AddMenuEntry(
		"UE5AgentPython_OpenPanel",
		LOCTEXT("UE5AgentPythonMenuEntry", "UE5 Agent Python"),
		LOCTEXT("UE5AgentPythonMenuTooltip", "Open the UE5 Agent Python scripting assistant."),
		FSlateIcon(FUE5AgentPythonStyle::GetStyleSetName(), "UE5AgentPython.MenuIcon"),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FUE5AgentPythonModule::PanelTabName);
		})));
}

TSharedRef<SDockTab> FUE5AgentPythonModule::SpawnPanelTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("PanelTabTitle", "UE5 Agent Python"))
		[
			SNew(SUE5AgentPythonPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUE5AgentPythonModule, UE5AgentPython)
