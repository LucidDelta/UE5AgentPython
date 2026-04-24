// Copyright (c) UE5AgentPython. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "UE5AgentRunner.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class STextBlock;
class SCheckBox;
class SBox;
class SScrollBox;
class SMultiLineEditableText;

class SUE5AgentPythonPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUE5AgentPythonPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SUE5AgentPythonPanel();

private:
	// ---- Event handlers ----
	void OnProviderChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnAPIKeyCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnModelChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnConversationModeChanged(ECheckBoxState NewState);
	void OnPromptChanged(const FText& NewText);
	FReply OnExecuteClicked();
	FReply OnGearClicked();
	FReply OnLogClicked();
	FReply OnClearLogClicked();

	TSharedRef<SWidget> GenerateProviderComboItem(TSharedPtr<FString> InItem);
	TSharedRef<SWidget> GenerateModelComboItem(TSharedPtr<FString> InItem);

	// ---- Logic ----
	void LoadSettings();
	void SaveSettings();
	FString GetCurrentAPIKey() const;
	FString GetCurrentModel() const;
	void StartModelFetch();
	void SetStatus(const FString& Text);
	void RefreshLogDisplay();

	FString BuildContextBlock() const;
	void ExecutePythonFromResponse(const FString& RawResponse);
	void AppendLog(const FString& Section, const FString& Text);
	FString GetSavedDir() const;
	void EnsureSavedDirExists() const;

	FString BuildSystemPrompt() const;

	// ---- State ----
	EAIProvider CurrentProvider  = EAIProvider::AnthropicClaude;

	FString AnthropicAPIKey;
	FString OpenAIAPIKey;
	FString GeminiAPIKey;

	FString AnthropicModel;
	FString OpenAIModel;
	FString GeminiModel;

	bool bConversationMode  = false;
	bool bSettingsExpanded  = false;
	bool bLogExpanded       = false;
	TArray<TPair<FString, FString>> ConversationHistory;

	FString CurrentPrompt;
	bool bRequestInFlight = false;

	TSharedPtr<FUE5AgentRunner> Runner;

	// ---- Widgets / combo data ----
	TArray<TSharedPtr<FString>> ProviderOptions;
	TArray<TSharedPtr<FString>> ModelOptions;

	TSharedPtr<SBox>                           SettingsBox;
	TSharedPtr<SBox>                           LogBox;
	TSharedPtr<SScrollBox>                     LogScroll;
	TSharedPtr<SMultiLineEditableText>         LogText;
	TSharedPtr<SBox>                           RunningIndicator;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ProviderCombo;
	TSharedPtr<SEditableTextBox>               APIKeyBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ModelCombo;
	TSharedPtr<STextBlock>                     StatusText;
	TSharedPtr<SCheckBox>                      ConversationCheck;
	TSharedPtr<SMultiLineEditableTextBox>       PromptBox;
};
