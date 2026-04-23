// Copyright (c) UE5AgentPython. All Rights Reserved.

#include "UE5AgentPythonStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> FUE5AgentPythonStyle::StyleInstance = nullptr;

void FUE5AgentPythonStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FUE5AgentPythonStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

FName FUE5AgentPythonStyle::GetStyleSetName()
{
	static FName Name(TEXT("UE5AgentPythonStyle"));
	return Name;
}

const ISlateStyle& FUE5AgentPythonStyle::Get()
{
	return *StyleInstance;
}

TSharedRef<FSlateStyleSet> FUE5AgentPythonStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE5AgentPython"));
	const FString ResourcesDir = Plugin.IsValid()
		? (Plugin->GetBaseDir() / TEXT("Resources"))
		: FPaths::EngineContentDir();

	Style->Set("UE5AgentPython.MenuIcon",
		new FSlateVectorImageBrush(ResourcesDir / TEXT("UE5AgentPython_MenuIcon.svg"), FVector2D(16.f, 16.f)));

	return Style;
}
