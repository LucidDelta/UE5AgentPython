// Copyright (c) UE5AgentPython. All Rights Reserved.

#include "SUE5AgentPythonPanel.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "UE5AgentPythonStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"

#include "Editor.h"
#include "Selection.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"

#include "IPythonScriptPlugin.h"

DEFINE_LOG_CATEGORY_STATIC(LogUE5AgentPython, Log, All);

#define LOCTEXT_NAMESPACE "UE5AgentPythonPanel"

namespace
{
	const FString kIniSection(TEXT("UE5AgentPython"));
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

void SUE5AgentPythonPanel::Construct(const FArguments& InArgs)
{
	Runner = MakeShared<FUE5AgentRunner>();

	ProviderOptions.Add(MakeShared<FString>(TEXT("Anthropic Claude")));
	ProviderOptions.Add(MakeShared<FString>(TEXT("OpenAI")));
	ProviderOptions.Add(MakeShared<FString>(TEXT("Google Gemini")));

	LoadSettings();
	EnsureSavedDirExists();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.f))
		[
			SNew(SVerticalBox)

			// ---- Toolbar row: gear | log | spacer | running indicator ----
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("SettingsTooltip", "Toggle provider / key / model settings"))
					.OnClicked(this, &SUE5AgentPythonPanel::OnGearClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Settings"))
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("LogTooltip", "Toggle session log viewer"))
					.OnClicked(this, &SUE5AgentPythonPanel::OnLogClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Details"))
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(RunningIndicator, SBox)
					.Visibility(EVisibility::Hidden)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SCircularThrobber)
							.Radius(7.f)
							.Period(0.6f)
							.NumPieces(5)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(6, 0, 0, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Running", "Running\u2026"))
						]
					]
				]
			]

			// ---- Collapsible settings ------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SAssignNew(SettingsBox, SBox)
				.Visibility(EVisibility::Collapsed)
				[
					SNew(SWrapBox)
					.UseAllottedSize(true)
					.InnerSlotPadding(FVector2D(8, 4))

					+ SWrapBox::Slot()
					[
						SNew(SBox)
						.WidthOverride(180)
						[
							SAssignNew(ProviderCombo, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&ProviderOptions)
							.OnGenerateWidget(this, &SUE5AgentPythonPanel::GenerateProviderComboItem)
							.OnSelectionChanged(this, &SUE5AgentPythonPanel::OnProviderChanged)
							.InitiallySelectedItem(ProviderOptions[(int32)CurrentProvider])
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									return FText::FromString(FUE5AgentRunner::ProviderDisplayName(CurrentProvider));
								})
							]
						]
					]

					+ SWrapBox::Slot()
					[
						SNew(SBox)
						.WidthOverride(320)
						[
							SAssignNew(APIKeyBox, SEditableTextBox)
							.IsPassword(true)
							.HintText(LOCTEXT("APIKeyHint", "API key"))
							.Text(FText::FromString(GetCurrentAPIKey()))
							.OnTextCommitted(this, &SUE5AgentPythonPanel::OnAPIKeyCommitted)
						]
					]

					+ SWrapBox::Slot()
					[
						SNew(SBox)
						.WidthOverride(260)
						[
							SAssignNew(ModelCombo, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&ModelOptions)
							.OnGenerateWidget(this, &SUE5AgentPythonPanel::GenerateModelComboItem)
							.OnSelectionChanged(this, &SUE5AgentPythonPanel::OnModelChanged)
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									const FString M = GetCurrentModel();
									return M.IsEmpty() ? LOCTEXT("NoModel", "(no model)") : FText::FromString(M);
								})
							]
						]
					]

					+ SWrapBox::Slot()
					[
						SNew(SBox)
						.MinDesiredWidth(140)
						.VAlign(VAlign_Center)
						[
							SAssignNew(StatusText, STextBlock)
							.Text(LOCTEXT("StatusNoKey", "No key entered"))
						]
					]

					+ SWrapBox::Slot()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SAssignNew(ConversationCheck, SCheckBox)
							.IsChecked(bConversationMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
							.OnCheckStateChanged(this, &SUE5AgentPythonPanel::OnConversationModeChanged)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4, 0, 0, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ConvMode", "Conversation mode"))
						]
					]
				]
			]

			// ---- Collapsible log panel -----------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SAssignNew(LogBox, SBox)
				.Visibility(EVisibility::Collapsed)
				.HeightOverride(200)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(FMargin(4.f))
					[
						SAssignNew(LogScroll, SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(LogText, SMultiLineEditableText)
							.IsReadOnly(true)
							.AutoWrapText(true)
							.Text(LOCTEXT("LogEmpty", "(no log entries yet)"))
						]
					]
				]
			]

			// ---- Prompt area ---------------------------------------------
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0, 0, 0, 4)
			[
				SNew(SBox)
				.MinDesiredHeight(240)
				[
					SAssignNew(PromptBox, SMultiLineEditableTextBox)
					.HintText(LOCTEXT("PromptHint", "Describe what you want the editor to do\u2026"))
					.AutoWrapText(true)
					.OnTextChanged(this, &SUE5AgentPythonPanel::OnPromptChanged)
				]
			]

			// ---- Execute button ------------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("Execute", "Execute"))
				.OnClicked(this, &SUE5AgentPythonPanel::OnExecuteClicked)
			]
		]
	];

	if (!GetCurrentAPIKey().IsEmpty())
	{
		StartModelFetch();
	}
	else
	{
		SetStatus(TEXT("No key entered"));
	}
}

SUE5AgentPythonPanel::~SUE5AgentPythonPanel()
{
	ConversationHistory.Empty();
}

// ---------------------------------------------------------------------------
// Toolbar button handlers
// ---------------------------------------------------------------------------

FReply SUE5AgentPythonPanel::OnGearClicked()
{
	bSettingsExpanded = !bSettingsExpanded;
	if (SettingsBox.IsValid())
	{
		SettingsBox->SetVisibility(bSettingsExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}
	return FReply::Handled();
}

FReply SUE5AgentPythonPanel::OnLogClicked()
{
	bLogExpanded = !bLogExpanded;
	if (LogBox.IsValid())
	{
		LogBox->SetVisibility(bLogExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (bLogExpanded)
	{
		RefreshLogDisplay();
	}
	return FReply::Handled();
}

void SUE5AgentPythonPanel::RefreshLogDisplay()
{
	const FString SessionLogPath = GetSavedDir() / TEXT("session.log");
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *SessionLogPath))
	{
		Content = TEXT("(session.log not found)");
	}

	if (LogText.IsValid())
	{
		LogText->SetText(FText::FromString(Content));
	}
	if (LogScroll.IsValid())
	{
		LogScroll->ScrollToEnd();
	}
}

// ---------------------------------------------------------------------------
// Combo item templates
// ---------------------------------------------------------------------------

TSharedRef<SWidget> SUE5AgentPythonPanel::GenerateProviderComboItem(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(InItem.IsValid() ? *InItem : FString()));
}

TSharedRef<SWidget> SUE5AgentPythonPanel::GenerateModelComboItem(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(InItem.IsValid() ? *InItem : FString()));
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------

void SUE5AgentPythonPanel::LoadSettings()
{
	GConfig->GetString(*kIniSection, TEXT("AnthropicAPIKey"), AnthropicAPIKey, GEditorPerProjectIni);
	GConfig->GetString(*kIniSection, TEXT("OpenAIAPIKey"),    OpenAIAPIKey,    GEditorPerProjectIni);
	GConfig->GetString(*kIniSection, TEXT("GeminiAPIKey"),    GeminiAPIKey,    GEditorPerProjectIni);

	GConfig->GetString(*kIniSection, TEXT("AnthropicModel"),  AnthropicModel,  GEditorPerProjectIni);
	GConfig->GetString(*kIniSection, TEXT("OpenAIModel"),     OpenAIModel,     GEditorPerProjectIni);
	GConfig->GetString(*kIniSection, TEXT("GeminiModel"),     GeminiModel,     GEditorPerProjectIni);

	FString ProviderStr;
	if (GConfig->GetString(*kIniSection, TEXT("LastProvider"), ProviderStr, GEditorPerProjectIni) && !ProviderStr.IsEmpty())
	{
		CurrentProvider = FUE5AgentRunner::ProviderFromDisplayName(ProviderStr);
	}

	bool bConv = false;
	if (GConfig->GetBool(*kIniSection, TEXT("ConversationMode"), bConv, GEditorPerProjectIni))
	{
		bConversationMode = bConv;
	}
}

void SUE5AgentPythonPanel::SaveSettings()
{
	GConfig->SetString(*kIniSection, TEXT("AnthropicAPIKey"), *AnthropicAPIKey, GEditorPerProjectIni);
	GConfig->SetString(*kIniSection, TEXT("OpenAIAPIKey"),    *OpenAIAPIKey,    GEditorPerProjectIni);
	GConfig->SetString(*kIniSection, TEXT("GeminiAPIKey"),    *GeminiAPIKey,    GEditorPerProjectIni);

	GConfig->SetString(*kIniSection, TEXT("AnthropicModel"),  *AnthropicModel,  GEditorPerProjectIni);
	GConfig->SetString(*kIniSection, TEXT("OpenAIModel"),     *OpenAIModel,     GEditorPerProjectIni);
	GConfig->SetString(*kIniSection, TEXT("GeminiModel"),     *GeminiModel,     GEditorPerProjectIni);

	GConfig->SetString(*kIniSection, TEXT("LastProvider"),    *FUE5AgentRunner::ProviderDisplayName(CurrentProvider), GEditorPerProjectIni);
	GConfig->SetBool  (*kIniSection, TEXT("ConversationMode"), bConversationMode, GEditorPerProjectIni);

	GConfig->Flush(false, GEditorPerProjectIni);
}

FString SUE5AgentPythonPanel::GetCurrentAPIKey() const
{
	switch (CurrentProvider)
	{
	case EAIProvider::AnthropicClaude: return AnthropicAPIKey;
	case EAIProvider::OpenAI:          return OpenAIAPIKey;
	case EAIProvider::GoogleGemini:    return GeminiAPIKey;
	}
	return FString();
}

FString SUE5AgentPythonPanel::GetCurrentModel() const
{
	switch (CurrentProvider)
	{
	case EAIProvider::AnthropicClaude: return AnthropicModel;
	case EAIProvider::OpenAI:          return OpenAIModel;
	case EAIProvider::GoogleGemini:    return GeminiModel;
	}
	return FString();
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void SUE5AgentPythonPanel::OnProviderChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!NewSelection.IsValid()) return;
	CurrentProvider = FUE5AgentRunner::ProviderFromDisplayName(*NewSelection);

	if (APIKeyBox.IsValid())
	{
		APIKeyBox->SetText(FText::FromString(GetCurrentAPIKey()));
	}

	ModelOptions.Reset();
	if (ModelCombo.IsValid())
	{
		ModelCombo->RefreshOptions();
		ModelCombo->SetSelectedItem(nullptr);
	}

	SaveSettings();

	if (!GetCurrentAPIKey().IsEmpty())
	{
		StartModelFetch();
	}
	else
	{
		SetStatus(TEXT("No key entered"));
	}

	AppendLog(TEXT("PROVIDER"), FString::Printf(TEXT("Switched to %s"),
		*FUE5AgentRunner::ProviderDisplayName(CurrentProvider)));
}

void SUE5AgentPythonPanel::OnAPIKeyCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	const FString NewKey = NewText.ToString();
	switch (CurrentProvider)
	{
	case EAIProvider::AnthropicClaude: AnthropicAPIKey = NewKey; break;
	case EAIProvider::OpenAI:          OpenAIAPIKey    = NewKey; break;
	case EAIProvider::GoogleGemini:    GeminiAPIKey    = NewKey; break;
	}
	SaveSettings();

	if (NewKey.IsEmpty())
	{
		SetStatus(TEXT("No key entered"));
		ModelOptions.Reset();
		if (ModelCombo.IsValid())
		{
			ModelCombo->RefreshOptions();
			ModelCombo->SetSelectedItem(nullptr);
		}
		return;
	}

	StartModelFetch();
}

void SUE5AgentPythonPanel::OnModelChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!NewSelection.IsValid()) return;
	const FString& M = *NewSelection;

	switch (CurrentProvider)
	{
	case EAIProvider::AnthropicClaude: AnthropicModel = M; break;
	case EAIProvider::OpenAI:          OpenAIModel    = M; break;
	case EAIProvider::GoogleGemini:    GeminiModel    = M; break;
	}
	SaveSettings();

	AppendLog(TEXT("MODEL"), FString::Printf(TEXT("Selected %s"), *M));
}

void SUE5AgentPythonPanel::OnConversationModeChanged(ECheckBoxState NewState)
{
	bConversationMode = (NewState == ECheckBoxState::Checked);
	if (!bConversationMode)
	{
		ConversationHistory.Empty();
	}
	SaveSettings();

	AppendLog(TEXT("CONV"), FString::Printf(TEXT("Conversation mode %s"),
		bConversationMode ? TEXT("ON") : TEXT("OFF (history cleared)")));
}

void SUE5AgentPythonPanel::OnPromptChanged(const FText& NewText)
{
	CurrentPrompt = NewText.ToString();
}

FReply SUE5AgentPythonPanel::OnExecuteClicked()
{
	if (bRequestInFlight)
	{
		return FReply::Handled();
	}

	const FString APIKey = GetCurrentAPIKey();
	const FString Model  = GetCurrentModel();
	if (APIKey.IsEmpty())
	{
		SetStatus(TEXT("No key entered"));
		return FReply::Handled();
	}
	if (Model.IsEmpty())
	{
		SetStatus(TEXT("No model selected"));
		return FReply::Handled();
	}
	if (CurrentPrompt.IsEmpty())
	{
		return FReply::Handled();
	}

	FAIProviderConfig Config;
	Config.Provider = CurrentProvider;
	Config.APIKey   = APIKey;
	Config.Model    = Model;

	const FString SystemPrompt = BuildSystemPrompt();
	const TArray<TPair<FString, FString>> Turns = bConversationMode
		? ConversationHistory
		: TArray<TPair<FString, FString>>();

	const FString UserPrompt            = CurrentPrompt;
	const FString ContextBlock          = BuildContextBlock();
	const FString UserPromptWithContext  = ContextBlock + TEXT("[USER PROMPT]\n") + UserPrompt;

	const int32 ApproxTokens = UserPromptWithContext.Len() / 4;
	AppendLog(TEXT("PROMPT"), FString::Printf(
		TEXT("Provider=%s Model=%s ApproxTokens=%d History=%d\n--- prompt ---\n%s\n--- end prompt ---"),
		*FUE5AgentRunner::ProviderDisplayName(CurrentProvider), *Model, ApproxTokens,
		Turns.Num(), *UserPromptWithContext));
	UE_LOG(LogUE5AgentPython, Log, TEXT("[PROMPT] provider=%s model=%s approxTokens=%d historyPairs=%d"),
		*FUE5AgentRunner::ProviderDisplayName(CurrentProvider), *Model, ApproxTokens, Turns.Num());

	bRequestInFlight = true;
	if (RunningIndicator.IsValid())
	{
		RunningIndicator->SetVisibility(EVisibility::Visible);
	}

	TWeakPtr<SUE5AgentPythonPanel> WeakPanel = SharedThis(this);
	FOnCompletionFetched OnDone;
	OnDone.BindLambda([WeakPanel, UserPrompt](bool bOK, int32 Status, const FString& Text)
	{
		TSharedPtr<SUE5AgentPythonPanel> Pinned = WeakPanel.Pin();
		if (!Pinned.IsValid()) return;

		Pinned->bRequestInFlight = false;

		if (!bOK)
		{
			// Hide indicator immediately on HTTP failure — no Python will run.
			if (Pinned->RunningIndicator.IsValid())
			{
				Pinned->RunningIndicator->SetVisibility(EVisibility::Hidden);
			}
			if (Status == 401 || Status == 403)
			{
				Pinned->SetStatus(TEXT("Invalid key"));
			}
			else
			{
				Pinned->SetStatus(TEXT("Fetch failed"));
			}
			Pinned->AppendLog(TEXT("COMPLETION_ERR"),
				FString::Printf(TEXT("status=%d bytes=%d msg=%s"), Status, Text.Len(), *Text));
			UE_LOG(LogUE5AgentPython, Warning, TEXT("[COMPLETION_ERR] status=%d bytes=%d"), Status, Text.Len());
			return;
		}

		Pinned->AppendLog(TEXT("COMPLETION_OK"),
			FString::Printf(TEXT("status=%d bytes=%d\n--- response ---\n%s\n--- end response ---"),
				Status, Text.Len(), *Text));
		UE_LOG(LogUE5AgentPython, Log, TEXT("[COMPLETION_OK] status=%d bytes=%d"), Status, Text.Len());

		const FString CleanedCode = FUE5AgentRunner::StripMarkdownFences(Text);

		if (Pinned->bConversationMode)
		{
			Pinned->ConversationHistory.Add(TPair<FString, FString>(UserPrompt, Text));
		}

		// Indicator stays visible through Python execution and is hidden inside ExecutePythonFromResponse.
		Pinned->ExecutePythonFromResponse(CleanedCode);
	});

	Runner->SendCompletion(Config, SystemPrompt, Turns, UserPromptWithContext, OnDone);
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Context block
// ---------------------------------------------------------------------------

FString SUE5AgentPythonPanel::BuildContextBlock() const
{
	FString Block;
	Block += TEXT("[EDITOR CONTEXT]\n");

	// Viewport / World Outliner selection
	TArray<AActor*> SelectedActors;
	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				SelectedActors.Add(Actor);
			}
		}
	}

	const int32 TotalActors = SelectedActors.Num();
	const int32 ActorCap    = FMath::Min(TotalActors, 50);
	Block += FString::Printf(TEXT("Viewport/Outliner selection (%d actors):\n"), TotalActors);
	for (int32 i = 0; i < ActorCap; ++i)
	{
		AActor* A = SelectedActors[i];
		const FVector Pos = A->GetActorLocation();
		FString MobilityStr = TEXT("Static");
		if (USceneComponent* Root = A->GetRootComponent())
		{
			switch (Root->Mobility)
			{
			case EComponentMobility::Stationary: MobilityStr = TEXT("Stationary"); break;
			case EComponentMobility::Movable:    MobilityStr = TEXT("Movable");    break;
			default:                             MobilityStr = TEXT("Static");     break;
			}
		}
		Block += FString::Printf(TEXT("  - %s (%s) pos=(%.1f, %.1f, %.1f) mobility=%s\n"),
			*A->GetActorLabel(), *A->GetClass()->GetName(),
			Pos.X, Pos.Y, Pos.Z, *MobilityStr);
	}
	if (TotalActors > 50)
	{
		Block += FString::Printf(TEXT("  ... and %d more actors (capped at 50)\n"), TotalActors - 50);
	}
	Block += TEXT("\n");

	// Content browser — selected assets and active folder
	TArray<FAssetData> SelectedAssets;
	TArray<FString>    SelectedFolders;

	FContentBrowserModule* CBModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser");
	if (CBModule)
	{
		IContentBrowserSingleton& CB = CBModule->Get();
		CB.GetSelectedAssets(SelectedAssets);
		CB.GetSelectedFolders(SelectedFolders);
	}

	const int32 TotalAssets = SelectedAssets.Num();
	const int32 AssetCap    = FMath::Min(TotalAssets, 50);
	Block += FString::Printf(TEXT("Content browser selection (%d assets):\n"), TotalAssets);
	for (int32 i = 0; i < AssetCap; ++i)
	{
		const FAssetData& AD = SelectedAssets[i];
		Block += FString::Printf(TEXT("  - %s (%s) %s\n"),
			*AD.AssetName.ToString(),
			*AD.AssetClassPath.GetAssetName().ToString(),
			*AD.GetSoftObjectPath().ToString());
	}
	if (TotalAssets > 50)
	{
		Block += FString::Printf(TEXT("  ... and %d more assets (capped at 50)\n"), TotalAssets - 50);
	}
	Block += TEXT("\n");

	const FString ActiveFolder = SelectedFolders.Num() > 0 ? SelectedFolders[0] : FString();
	Block += FString::Printf(TEXT("Content browser active folder: %s\n"), *ActiveFolder);
	Block += TEXT("\n");

	return Block;
}

// ---------------------------------------------------------------------------
// Model fetch
// ---------------------------------------------------------------------------

void SUE5AgentPythonPanel::StartModelFetch()
{
	FAIProviderConfig Config;
	Config.Provider = CurrentProvider;
	Config.APIKey   = GetCurrentAPIKey();

	SetStatus(TEXT("Fetching models\u2026"));
	AppendLog(TEXT("MODELS_FETCH"), FString::Printf(TEXT("provider=%s"),
		*FUE5AgentRunner::ProviderDisplayName(CurrentProvider)));
	UE_LOG(LogUE5AgentPython, Log, TEXT("[MODELS_FETCH] provider=%s"),
		*FUE5AgentRunner::ProviderDisplayName(CurrentProvider));

	TWeakPtr<SUE5AgentPythonPanel> WeakPanel = SharedThis(this);
	FOnModelListFetched OnDone;
	OnDone.BindLambda([WeakPanel](bool bOK, int32 Status, const TArray<FString>& Models)
	{
		TSharedPtr<SUE5AgentPythonPanel> Pinned = WeakPanel.Pin();
		if (!Pinned.IsValid()) return;

		if (!bOK)
		{
			if (Status == 401 || Status == 403)
			{
				Pinned->SetStatus(TEXT("Invalid key"));
			}
			else
			{
				Pinned->SetStatus(TEXT("Fetch failed"));
			}
			Pinned->AppendLog(TEXT("MODELS_ERR"),
				FString::Printf(TEXT("status=%d count=0"), Status));
			UE_LOG(LogUE5AgentPython, Warning, TEXT("[MODELS_ERR] status=%d"), Status);
			return;
		}

		Pinned->ModelOptions.Reset();
		for (const FString& M : Models)
		{
			Pinned->ModelOptions.Add(MakeShared<FString>(M));
		}

		if (Pinned->ModelCombo.IsValid())
		{
			Pinned->ModelCombo->RefreshOptions();
		}

		const FString Existing  = Pinned->GetCurrentModel();
		const bool bHasExisting = !Existing.IsEmpty() && Models.Contains(Existing);
		FString Chosen;
		if (bHasExisting)
		{
			Chosen = Existing;
		}
		else if (Models.Num() > 0)
		{
			Chosen = Models[0];
		}

		switch (Pinned->CurrentProvider)
		{
		case EAIProvider::AnthropicClaude: Pinned->AnthropicModel = Chosen; break;
		case EAIProvider::OpenAI:          Pinned->OpenAIModel    = Chosen; break;
		case EAIProvider::GoogleGemini:    Pinned->GeminiModel    = Chosen; break;
		}
		Pinned->SaveSettings();

		if (Pinned->ModelCombo.IsValid())
		{
			for (const TSharedPtr<FString>& Opt : Pinned->ModelOptions)
			{
				if (Opt.IsValid() && *Opt == Chosen)
				{
					Pinned->ModelCombo->SetSelectedItem(Opt);
					break;
				}
			}
		}

		Pinned->SetStatus(FString::Printf(TEXT("%d models"), Models.Num()));
		Pinned->AppendLog(TEXT("MODELS_OK"),
			FString::Printf(TEXT("status=%d count=%d chosen=%s"), Status, Models.Num(), *Chosen));
		UE_LOG(LogUE5AgentPython, Log, TEXT("[MODELS_OK] status=%d count=%d"), Status, Models.Num());
	});

	Runner->FetchModels(Config, OnDone);
}

void SUE5AgentPythonPanel::SetStatus(const FString& Text)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(Text));
	}
}

// ---------------------------------------------------------------------------
// Python execution
// ---------------------------------------------------------------------------

void SUE5AgentPythonPanel::ExecutePythonFromResponse(const FString& GeneratedCode)
{
	EnsureSavedDirExists();

	const FString SavedDir      = GetSavedDir();
	const FString GeneratedPath = SavedDir / TEXT("generated_code.py");
	const FString OutputPath    = SavedDir / TEXT("py_output.txt");

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (PF.FileExists(*OutputPath))
	{
		PF.DeleteFile(*OutputPath);
	}
	const FString PromptTxt = SavedDir / TEXT("claude_prompt.txt");
	if (PF.FileExists(*PromptTxt))
	{
		PF.DeleteFile(*PromptTxt);
	}

	FFileHelper::SaveStringToFile(GeneratedCode, *GeneratedPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	FString PyGenPath = GeneratedPath;
	PyGenPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	FString PyOutPath = OutputPath;
	PyOutPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	const FString Wrapper = FString::Printf(TEXT(
		"import sys, io, traceback\n"
		"_stdout_backup = sys.stdout\n"
		"_stderr_backup = sys.stderr\n"
		"sys.stdout = io.StringIO()\n"
		"sys.stderr = sys.stdout\n"
		"try:\n"
		"    exec(open(r\"%s\").read(), {\"__name__\": \"__main__\"})\n"
		"    _status = \"OK\"\n"
		"except Exception:\n"
		"    traceback.print_exc()\n"
		"    _status = \"ERR\"\n"
		"_captured = sys.stdout.getvalue()\n"
		"sys.stdout = _stdout_backup\n"
		"sys.stderr = _stderr_backup\n"
		"with open(r\"%s\", \"w\", encoding=\"utf-8\") as _f:\n"
		"    _f.write(_status + \"\\n\" + _captured)\n"),
		*PyGenPath, *PyOutPath);

	IPythonScriptPlugin* Py = IPythonScriptPlugin::Get();
	if (!Py || !Py->IsPythonAvailable())
	{
		AppendLog(TEXT("PY_ERR"), TEXT("IPythonScriptPlugin unavailable"));
		UE_LOG(LogUE5AgentPython, Error, TEXT("[PY_ERR] IPythonScriptPlugin unavailable"));
		if (RunningIndicator.IsValid())
		{
			RunningIndicator->SetVisibility(EVisibility::Hidden);
		}
		return;
	}

	const bool bExecOK = Py->ExecPythonCommand(*Wrapper);
	(void)bExecOK;

	// Hide the running indicator now that Python has finished.
	if (RunningIndicator.IsValid())
	{
		RunningIndicator->SetVisibility(EVisibility::Hidden);
	}

	FString Captured;
	if (!FFileHelper::LoadFileToString(Captured, *OutputPath))
	{
		AppendLog(TEXT("PY_ERR"), TEXT("Failed to read py_output.txt"));
		UE_LOG(LogUE5AgentPython, Warning, TEXT("[PY_ERR] Failed to read py_output.txt"));
		return;
	}

	FString StatusLine = TEXT("ERR");
	FString Output     = Captured;
	int32 NewlineIdx   = INDEX_NONE;
	if (Captured.FindChar(TEXT('\n'), NewlineIdx))
	{
		StatusLine = Captured.Left(NewlineIdx);
		Output     = Captured.Mid(NewlineIdx + 1);
	}

	AppendLog(TEXT("PY_EXEC"), FString::Printf(TEXT("status=%s\n--- output ---\n%s\n--- end output ---"),
		*StatusLine, *Output));
	UE_LOG(LogUE5AgentPython, Log, TEXT("[PY_EXEC] status=%s bytes=%d"), *StatusLine, Output.Len());

	// If the log panel is open, refresh it to show the new entry.
	if (bLogExpanded)
	{
		RefreshLogDisplay();
	}
}

// ---------------------------------------------------------------------------
// System prompt
// ---------------------------------------------------------------------------

FString SUE5AgentPythonPanel::BuildSystemPrompt() const
{
	return TEXT(
		"You are a power tool for a senior Unreal Engine 5.5 developer. You write Python scripts that "
		"automate bulk and batch editor operations — the repetitive, high-volume tasks that waste hours "
		"when done by hand.\n"
		"\n"
		"YOUR SCOPE:\n"
		"- Batch asset operations: renaming by regex/prefix/suffix/pattern, moving, reorganizing by type "
		"or folder, fixing redirectors\n"
		"- Batch mesh operations: collision profiles, trace flags, LOD settings, lightmap resolution, "
		"mobility, shadow flags, render flags across many meshes at once\n"
		"- Batch material/texture operations: swapping materials, setting compression/mip/streaming "
		"settings, texture group assignment\n"
		"- Scene and level cleanup: aligning and distributing actors, replacing actor types, propagating "
		"properties across placed instances, removing broken actors and null references\n"
		"- Asset auditing: finding unreferenced assets, assets missing expected properties, generating "
		"reports by triangle count / texture size / material usage\n"
		"- Batch setting default property values on placed Blueprint instances in the level\n"
		"\n"
		"Do NOT generate Blueprint node graph construction, animation graph wiring, or gameplay logic. "
		"If asked for something outside this scope, write the closest batch/automation equivalent "
		"achievable with the Python editor API.\n"
		"\n"
		"OUTPUT RULES:\n"
		"- Respond with RAW PYTHON CODE ONLY.\n"
		"- No markdown. No triple-backtick code fences. No prose, commentary, or explanation.\n"
		"- Your entire response MUST be valid Python that can be passed directly to exec().\n"
		"- Starting the response with anything other than valid Python syntax is an error.\n"
		"\n"
		"EDITOR CONTEXT:\n"
		"Every prompt is prefixed with an [EDITOR CONTEXT] block containing:\n"
		"- Viewport/Outliner selection: actors currently selected (label, class, world position, mobility)\n"
		"- Content browser selection: assets currently highlighted (name, class, full path)\n"
		"- Content browser active folder: the currently focused folder path\n"
		"Use whichever portion is relevant to the request. Ignore portions that are clearly unrelated. "
		"If a section is empty, infer intent from the prompt text itself.\n"
		"\n"
		"UNREAL ENGINE 5.5 PYTHON API GUIDANCE:\n"
		"- Use unreal.EditorLevelLibrary, unreal.EditorAssetLibrary, unreal.EditorUtilityLibrary,\n"
		"  unreal.AssetToolsHelpers, and unreal.EditorStaticMeshLibrary.\n"
		"- unreal.StaticMeshEditorLibrary does NOT exist. Always use unreal.EditorStaticMeshLibrary.\n"
		"- asset.mark_package_dirty() has been removed. Use asset.modify() followed by\n"
		"  unreal.EditorAssetLibrary.save_asset(asset.get_path_name()).\n"
		"- unreal.EditorLevelLibrary.editor_undo() does NOT exist. Do not emit that line.\n"
		"- To change collision trace flag on a static mesh:\n"
		"      body = mesh.get_editor_property('body_setup')\n"
		"      body.modify()\n"
		"      body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)\n"
		"      mesh.modify()\n"
		"      unreal.EditorAssetLibrary.save_asset(mesh.get_path_name())\n"
		"  The property is 'collision_trace_flag', NOT 'collision_complexity'. body.modify() is required.\n"
		"- To remove collisions:\n"
		"      unreal.EditorStaticMeshLibrary.remove_collisions(mesh)\n"
		"      mesh.modify()\n"
		"      unreal.EditorAssetLibrary.save_asset(mesh.get_path_name())\n"
	);
}

// ---------------------------------------------------------------------------
// Filesystem / logging
// ---------------------------------------------------------------------------

FString SUE5AgentPythonPanel::GetSavedDir() const
{
	return FPaths::ProjectSavedDir() / TEXT("UE5AgentPython");
}

void SUE5AgentPythonPanel::EnsureSavedDirExists() const
{
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	const FString Dir = GetSavedDir();
	if (!PF.DirectoryExists(*Dir))
	{
		PF.CreateDirectoryTree(*Dir);
	}
}

void SUE5AgentPythonPanel::AppendLog(const FString& Section, const FString& Text)
{
	EnsureSavedDirExists();
	const FString SessionLogPath = GetSavedDir() / TEXT("session.log");
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	const FString Line = FString::Printf(TEXT("[%s] [%s] %s\n"), *Stamp, *Section, *Text);
	FFileHelper::SaveStringToFile(Line, *SessionLogPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);
}

#undef LOCTEXT_NAMESPACE
