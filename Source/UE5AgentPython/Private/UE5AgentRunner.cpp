// Copyright (c) UE5AgentPython. All Rights Reserved.

#include "UE5AgentRunner.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IHttpRequest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
	constexpr float kRequestTimeoutSeconds = 120.0f;

	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Body)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
		{
			return nullptr;
		}
		return Obj;
	}

	FString SerializeJson(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	TSharedRef<FJsonObject> MakeMessage(const FString& Role, const FString& Content)
	{
		TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), Role);
		Msg->SetStringField(TEXT("content"), Content);
		return Msg;
	}
}

FUE5AgentRunner::FUE5AgentRunner()
{
}

FString FUE5AgentRunner::ProviderDisplayName(EAIProvider Provider)
{
	switch (Provider)
	{
	case EAIProvider::AnthropicClaude: return TEXT("Anthropic Claude");
	case EAIProvider::OpenAI:          return TEXT("OpenAI");
	case EAIProvider::GoogleGemini:    return TEXT("Google Gemini");
	}
	return TEXT("Anthropic Claude");
}

EAIProvider FUE5AgentRunner::ProviderFromDisplayName(const FString& DisplayName)
{
	if (DisplayName == TEXT("OpenAI"))          return EAIProvider::OpenAI;
	if (DisplayName == TEXT("Google Gemini"))   return EAIProvider::GoogleGemini;
	return EAIProvider::AnthropicClaude;
}

FString FUE5AgentRunner::StripMarkdownFences(const FString& Text)
{
	FString S = Text;
	S.TrimStartAndEndInline();

	// Leading fence
	if (S.StartsWith(TEXT("```")))
	{
		int32 NewlineIdx = INDEX_NONE;
		if (S.FindChar(TEXT('\n'), NewlineIdx))
		{
			S = S.Mid(NewlineIdx + 1);
		}
		else
		{
			S = S.Mid(3);
		}
	}

	// Trailing fence
	S.TrimEndInline();
	if (S.EndsWith(TEXT("```")))
	{
		S = S.LeftChop(3);
	}

	S.TrimStartAndEndInline();
	return S;
}

// ---------------------------------------------------------------------------
// Model list fetching
// ---------------------------------------------------------------------------

void FUE5AgentRunner::FetchModels(const FAIProviderConfig& Config, FOnModelListFetched OnComplete)
{
	switch (Config.Provider)
	{
	case EAIProvider::AnthropicClaude: FetchModelsAnthropic(Config, OnComplete); break;
	case EAIProvider::OpenAI:          FetchModelsOpenAI(Config, OnComplete);    break;
	case EAIProvider::GoogleGemini:    FetchModelsGemini(Config, OnComplete);    break;
	}
}

void FUE5AgentRunner::FetchModelsAnthropic(const FAIProviderConfig& Config, FOnModelListFetched OnComplete)
{
	TSharedRef<IHttpRequest> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(TEXT("https://api.anthropic.com/v1/models"));
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("x-api-key"), Config.APIKey);
	Req->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	Req->SetTimeout(kRequestTimeoutSeconds);

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bOK)
		{
			TArray<FString> Models;
			int32 Status = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (!bOK || !Response.IsValid() || Status < 200 || Status >= 300)
			{
				OnComplete.ExecuteIfBound(false, Status, Models);
				return;
			}

			TSharedPtr<FJsonObject> Obj = ParseJsonObject(Response->GetContentAsString());
			if (!Obj.IsValid())
			{
				OnComplete.ExecuteIfBound(false, Status, Models);
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
			if (Obj->TryGetArrayField(TEXT("data"), Data) && Data)
			{
				for (const TSharedPtr<FJsonValue>& V : *Data)
				{
					const TSharedPtr<FJsonObject>* Entry = nullptr;
					if (V->TryGetObject(Entry) && Entry && Entry->IsValid())
					{
						FString Id;
						if ((*Entry)->TryGetStringField(TEXT("id"), Id) && !Id.IsEmpty())
						{
							Models.Add(Id);
						}
					}
				}
			}

			Models.Sort();
			OnComplete.ExecuteIfBound(true, Status, Models);
		});

	Req->ProcessRequest();
}

void FUE5AgentRunner::FetchModelsOpenAI(const FAIProviderConfig& Config, FOnModelListFetched OnComplete)
{
	TSharedRef<IHttpRequest> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(TEXT("https://api.openai.com/v1/models"));
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Config.APIKey));
	Req->SetTimeout(kRequestTimeoutSeconds);

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bOK)
		{
			TArray<FString> Models;
			int32 Status = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (!bOK || !Response.IsValid() || Status < 200 || Status >= 300)
			{
				OnComplete.ExecuteIfBound(false, Status, Models);
				return;
			}

			TSharedPtr<FJsonObject> Obj = ParseJsonObject(Response->GetContentAsString());
			if (!Obj.IsValid())
			{
				OnComplete.ExecuteIfBound(false, Status, Models);
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
			if (Obj->TryGetArrayField(TEXT("data"), Data) && Data)
			{
				for (const TSharedPtr<FJsonValue>& V : *Data)
				{
					const TSharedPtr<FJsonObject>* Entry = nullptr;
					if (!V->TryGetObject(Entry) || !Entry || !Entry->IsValid())
					{
						continue;
					}
					FString Id;
					if (!(*Entry)->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
					{
						continue;
					}

					const bool bIncludePrefix =
						Id.StartsWith(TEXT("gpt-")) ||
						Id.StartsWith(TEXT("o1")) ||
						Id.StartsWith(TEXT("o3")) ||
						Id.StartsWith(TEXT("o4"));
					if (!bIncludePrefix)
					{
						continue;
					}

					const bool bBlocked =
						Id.Contains(TEXT("whisper")) ||
						Id.Contains(TEXT("tts")) ||
						Id.Contains(TEXT("dall-e")) ||
						Id.Contains(TEXT("text-embedding")) ||
						Id.Contains(TEXT("babbage")) ||
						Id.Contains(TEXT("davinci"));
					if (bBlocked)
					{
						continue;
					}

					Models.Add(Id);
				}
			}

			Models.Sort();
			OnComplete.ExecuteIfBound(true, Status, Models);
		});

	Req->ProcessRequest();
}

void FUE5AgentRunner::FetchModelsGemini(const FAIProviderConfig& Config, FOnModelListFetched OnComplete)
{
	TSharedRef<IHttpRequest> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models?key=%s"), *Config.APIKey));
	Req->SetVerb(TEXT("GET"));
	Req->SetTimeout(kRequestTimeoutSeconds);

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bOK)
		{
			TArray<FString> Models;
			int32 Status = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (!bOK || !Response.IsValid() || Status < 200 || Status >= 300)
			{
				OnComplete.ExecuteIfBound(false, Status, Models);
				return;
			}

			TSharedPtr<FJsonObject> Obj = ParseJsonObject(Response->GetContentAsString());
			if (!Obj.IsValid())
			{
				OnComplete.ExecuteIfBound(false, Status, Models);
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
			if (Obj->TryGetArrayField(TEXT("models"), Data) && Data)
			{
				for (const TSharedPtr<FJsonValue>& V : *Data)
				{
					const TSharedPtr<FJsonObject>* Entry = nullptr;
					if (!V->TryGetObject(Entry) || !Entry || !Entry->IsValid())
					{
						continue;
					}

					const TArray<TSharedPtr<FJsonValue>>* Methods = nullptr;
					bool bGenerateContent = false;
					if ((*Entry)->TryGetArrayField(TEXT("supportedGenerationMethods"), Methods) && Methods)
					{
						for (const TSharedPtr<FJsonValue>& M : *Methods)
						{
							FString Method;
							if (M->TryGetString(Method) && Method == TEXT("generateContent"))
							{
								bGenerateContent = true;
								break;
							}
						}
					}
					if (!bGenerateContent)
					{
						continue;
					}

					FString Name;
					if (!(*Entry)->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
					{
						continue;
					}

					if (Name.StartsWith(TEXT("models/")))
					{
						Name = Name.Mid(7);
					}

					Models.Add(Name);
				}
			}

			Models.Sort();
			OnComplete.ExecuteIfBound(true, Status, Models);
		});

	Req->ProcessRequest();
}

// ---------------------------------------------------------------------------
// Completion
// ---------------------------------------------------------------------------

void FUE5AgentRunner::SendCompletion(
	const FAIProviderConfig& Config,
	const FString& SystemPrompt,
	const TArray<TPair<FString, FString>>& PriorTurns,
	const FString& UserPrompt,
	FOnCompletionFetched OnComplete)
{
	switch (Config.Provider)
	{
	case EAIProvider::AnthropicClaude:
		SendCompletionAnthropic(Config, SystemPrompt, PriorTurns, UserPrompt, OnComplete);
		break;
	case EAIProvider::OpenAI:
		SendCompletionOpenAICompat(Config, TEXT("https://api.openai.com/v1/chat/completions"),
			SystemPrompt, PriorTurns, UserPrompt, OnComplete);
		break;
	case EAIProvider::GoogleGemini:
		SendCompletionOpenAICompat(Config, TEXT("https://generativelanguage.googleapis.com/v1beta/openai/chat/completions"),
			SystemPrompt, PriorTurns, UserPrompt, OnComplete);
		break;
	}
}

void FUE5AgentRunner::SendCompletionAnthropic(
	const FAIProviderConfig& Config,
	const FString& SystemPrompt,
	const TArray<TPair<FString, FString>>& PriorTurns,
	const FString& UserPrompt,
	FOnCompletionFetched OnComplete)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Config.Model);
	Body->SetNumberField(TEXT("max_tokens"), 4096);
	if (!SystemPrompt.IsEmpty())
	{
		Body->SetStringField(TEXT("system"), SystemPrompt);
	}

	TArray<TSharedPtr<FJsonValue>> Messages;
	for (const TPair<FString, FString>& Turn : PriorTurns)
	{
		Messages.Add(MakeShared<FJsonValueObject>(MakeMessage(TEXT("user"), Turn.Key)));
		Messages.Add(MakeShared<FJsonValueObject>(MakeMessage(TEXT("assistant"), Turn.Value)));
	}
	Messages.Add(MakeShared<FJsonValueObject>(MakeMessage(TEXT("user"), UserPrompt)));
	Body->SetArrayField(TEXT("messages"), Messages);

	const FString Payload = SerializeJson(Body);

	TSharedRef<IHttpRequest> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(TEXT("https://api.anthropic.com/v1/messages"));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("x-api-key"), Config.APIKey);
	Req->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	Req->SetHeader(TEXT("content-type"), TEXT("application/json"));
	Req->SetContentAsString(Payload);
	Req->SetTimeout(kRequestTimeoutSeconds);

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bOK)
		{
			int32 Status = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (!bOK || !Response.IsValid() || Status < 200 || Status >= 300)
			{
				FString Err = Response.IsValid() ? Response->GetContentAsString() : TEXT("no response");
				OnComplete.ExecuteIfBound(false, Status, Err);
				return;
			}

			TSharedPtr<FJsonObject> Obj = ParseJsonObject(Response->GetContentAsString());
			if (!Obj.IsValid())
			{
				OnComplete.ExecuteIfBound(false, Status, TEXT("parse error"));
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
			if (Obj->TryGetArrayField(TEXT("content"), Content) && Content && Content->Num() > 0)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				if ((*Content)[0]->TryGetObject(Entry) && Entry && Entry->IsValid())
				{
					FString Text;
					if ((*Entry)->TryGetStringField(TEXT("text"), Text))
					{
						OnComplete.ExecuteIfBound(true, Status, Text);
						return;
					}
				}
			}

			OnComplete.ExecuteIfBound(false, Status, TEXT("missing content[0].text"));
		});

	Req->ProcessRequest();
}

void FUE5AgentRunner::SendCompletionOpenAICompat(
	const FAIProviderConfig& Config,
	const FString& Endpoint,
	const FString& SystemPrompt,
	const TArray<TPair<FString, FString>>& PriorTurns,
	const FString& UserPrompt,
	FOnCompletionFetched OnComplete)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Config.Model);

	TArray<TSharedPtr<FJsonValue>> Messages;
	if (!SystemPrompt.IsEmpty())
	{
		Messages.Add(MakeShared<FJsonValueObject>(MakeMessage(TEXT("system"), SystemPrompt)));
	}
	for (const TPair<FString, FString>& Turn : PriorTurns)
	{
		Messages.Add(MakeShared<FJsonValueObject>(MakeMessage(TEXT("user"), Turn.Key)));
		Messages.Add(MakeShared<FJsonValueObject>(MakeMessage(TEXT("assistant"), Turn.Value)));
	}
	Messages.Add(MakeShared<FJsonValueObject>(MakeMessage(TEXT("user"), UserPrompt)));
	Body->SetArrayField(TEXT("messages"), Messages);

	const FString Payload = SerializeJson(Body);

	TSharedRef<IHttpRequest> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Endpoint);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Config.APIKey));
	Req->SetHeader(TEXT("content-type"), TEXT("application/json"));
	Req->SetContentAsString(Payload);
	Req->SetTimeout(kRequestTimeoutSeconds);

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bOK)
		{
			int32 Status = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (!bOK || !Response.IsValid() || Status < 200 || Status >= 300)
			{
				FString Err = Response.IsValid() ? Response->GetContentAsString() : TEXT("no response");
				OnComplete.ExecuteIfBound(false, Status, Err);
				return;
			}

			TSharedPtr<FJsonObject> Obj = ParseJsonObject(Response->GetContentAsString());
			if (!Obj.IsValid())
			{
				OnComplete.ExecuteIfBound(false, Status, TEXT("parse error"));
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
			if (Obj->TryGetArrayField(TEXT("choices"), Choices) && Choices && Choices->Num() > 0)
			{
				const TSharedPtr<FJsonObject>* Choice = nullptr;
				if ((*Choices)[0]->TryGetObject(Choice) && Choice && Choice->IsValid())
				{
					const TSharedPtr<FJsonObject>* Message = nullptr;
					if ((*Choice)->TryGetObjectField(TEXT("message"), Message) && Message && Message->IsValid())
					{
						FString Text;
						if ((*Message)->TryGetStringField(TEXT("content"), Text))
						{
							OnComplete.ExecuteIfBound(true, Status, Text);
							return;
						}
					}
				}
			}

			OnComplete.ExecuteIfBound(false, Status, TEXT("missing choices[0].message.content"));
		});

	Req->ProcessRequest();
}
