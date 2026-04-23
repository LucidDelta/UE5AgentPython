// Copyright (c) UE5AgentPython. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Interfaces/IHttpRequest.h"

enum class EAIProvider : uint8
{
	AnthropicClaude,
	OpenAI,
	GoogleGemini
};

struct FAIProviderConfig
{
	EAIProvider Provider = EAIProvider::AnthropicClaude;
	FString     APIKey;
	FString     Model;
};

DECLARE_DELEGATE_ThreeParams(FOnModelListFetched, bool /*bSuccess*/, int32 /*HttpStatus*/, const TArray<FString>& /*Models*/);
DECLARE_DELEGATE_ThreeParams(FOnCompletionFetched, bool /*bSuccess*/, int32 /*HttpStatus*/, const FString& /*ResponseText*/);

class FUE5AgentRunner : public TSharedFromThis<FUE5AgentRunner>
{
public:
	FUE5AgentRunner();

	/** Fetch the list of chat-capable models from the current provider. */
	void FetchModels(const FAIProviderConfig& Config, FOnModelListFetched OnComplete);

	/**
	 * Send a chat completion. PriorTurns is a list of (userPrompt, assistantResponse) pairs
	 * to include before the new UserPrompt. Pass an empty array for single-shot requests.
	 */
	void SendCompletion(
		const FAIProviderConfig& Config,
		const FString& SystemPrompt,
		const TArray<TPair<FString, FString>>& PriorTurns,
		const FString& UserPrompt,
		FOnCompletionFetched OnComplete);

	/** Strip leading/trailing markdown code fences (```python ... ```). */
	static FString StripMarkdownFences(const FString& Text);

	/** Provider display name helpers. */
	static FString ProviderDisplayName(EAIProvider Provider);
	static EAIProvider ProviderFromDisplayName(const FString& DisplayName);

private:
	void FetchModelsAnthropic(const FAIProviderConfig& Config, FOnModelListFetched OnComplete);
	void FetchModelsOpenAI(const FAIProviderConfig& Config, FOnModelListFetched OnComplete);
	void FetchModelsGemini(const FAIProviderConfig& Config, FOnModelListFetched OnComplete);

	void SendCompletionAnthropic(
		const FAIProviderConfig& Config,
		const FString& SystemPrompt,
		const TArray<TPair<FString, FString>>& PriorTurns,
		const FString& UserPrompt,
		FOnCompletionFetched OnComplete);

	void SendCompletionOpenAICompat(
		const FAIProviderConfig& Config,
		const FString& Endpoint,
		const FString& SystemPrompt,
		const TArray<TPair<FString, FString>>& PriorTurns,
		const FString& UserPrompt,
		FOnCompletionFetched OnComplete);
};
