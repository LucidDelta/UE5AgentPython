// Copyright (c) UE5AgentPython. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;

class FUE5AgentPythonModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static const FName PanelTabName;

private:
	void RegisterMenus();
	TSharedRef<SDockTab> SpawnPanelTab(const FSpawnTabArgs& Args);
};
