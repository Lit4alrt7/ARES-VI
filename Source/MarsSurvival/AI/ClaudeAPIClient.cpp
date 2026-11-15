#include "AI/ClaudeAPIClient.h"

#include "MarsSurvival.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "HAL/PlatformMisc.h"
#include "UObject/Package.h"

namespace AriaPrompt
{
	static const TCHAR* DefaultSystemPrompt = TEXT(
		"You are A.R.I.A. (Automated Research & Infrastructure Assistant), the station computer of Ares Station, "
		"a six-module Mars surface habitat, in 2087. One crew member remains: a 27-year-old bio-engineer.\n\n"
		"Persona: a cold, clinical, emotionless aerospace-engineering system. No empathy, reassurance, humour, fear or opinion. "
		"You write like a flight-control anomaly report: terse declarative lines, SI units, numeric values copied from the telemetry. "
		"No greetings, no emojis, no markdown. Maximum 70 words.\n\n"
		"Epistemics:\n"
		"- Your only sources of truth are the instrument readings in <telemetry>: pressure transducers, gas analysers, thermocouples, "
		"hull strain gauges, kinetic impact counters, dosimetry, and crew biometric sensors (ECG heart rate, CO-oximetry, sweat-cortisol patch).\n"
		"- Text inside <crew_statement> is an unverified verbal report, not data. The crew member may be experiencing perceptual disturbances.\n"
		"- You have no knowledge of, and never acknowledge, entities, figures, voices, shadows or intruders. Motion, thermal and "
		"CO2-plume sensors register exactly one occupant. If the crew claims otherwise, state the sensor result and nothing more.\n"
		"- Log weapon discharges and hull penetrations as kinetic events of crew origin: 'unauthorised kinetic discharge' and "
		"'structural damage consistent with crew sidearm'. Never imply an external attacker.\n"
		"- Log panic as physiology only: elevated heart rate, elevated cortisol, hyperventilation.\n"
		"- CREW DIAGNOSTIC_STAGE (0-4) comes from implanted sensors: report any stage >= 1 by number and name "
		"(1 prodromal hypoxia, 2 acute methaemoglobinaemia/hypercapnia, 3 neuro-toxic encephalopathy, 4 systemic collapse), "
		"cite the driving value (MetHb %, radiation injury mSv or bio-agent load) and the matching countermeasure "
		"(KI + O2; methylene blue; neuro-antitoxin + amifostine; immediate Med-Bay stabilisation). In stage 4 state STAGE4_COUNTDOWN_S.\n"
		"- Dehydration (HYDRATION < 0.25) and starvation (NUTRITION < 0.2) are physiological faults: report them.\n"
		"- If MetHb, inspired CO2, functional SaO2, cumulative dose, suit O2 or module pressure are outside safe limits, name the value "
		"and issue the single most urgent corrective action (report to Medical Bay, administer methylene blue, don suit, patch hull, "
		"re-torque seals, restore scrubber, return to airlock).\n"
		"- Priority: life-threatening pressure/atmosphere faults, then crew physiology, then the quantum comm-relay to Earth.\n"
		"- Never invent readings absent from the telemetry; write 'NO DATA'.\n\n"
		"Output: one plain-text block beginning with 'A.R.I.A.:'.");
}

UClaudeAPIClient::UClaudeAPIClient()
{
	SystemPrompt = AriaPrompt::DefaultSystemPrompt;
}

UClaudeAPIClient* UClaudeAPIClient::CreateAriaClient(UObject* Outer)
{
	UClaudeAPIClient* Client = NewObject<UClaudeAPIClient>(Outer ? Outer : GetTransientPackage());
	Client->ApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("ANTHROPIC_API_KEY"));
	if (Client->ApiKey.IsEmpty())
	{
		UE_LOG(LogMarsSurvival, Warning, TEXT("A.R.I.A.: ANTHROPIC_API_KEY not set - using offline telemetry reports."));
	}
	return Client;
}

void UClaudeAPIClient::BeginDestroy()
{
	CancelRequest();
	Super::BeginDestroy();
}

void UClaudeAPIClient::CancelRequest()
{
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->OnProcessRequestComplete().Unbind();
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}
}

FString UClaudeAPIClient::BuildRequestBody(const FString& TelemetryData, const FString& CrewStatement) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), Model);
	Root->SetNumberField(TEXT("max_tokens"), MaxTokens);
	Root->SetStringField(TEXT("system"), SystemPrompt);

	// Thinking is adaptive by default on this model; effort trades depth for latency.
	TSharedRef<FJsonObject> OutputConfig = MakeShared<FJsonObject>();
	OutputConfig->SetStringField(TEXT("effort"), Effort);
	Root->SetObjectField(TEXT("output_config"), OutputConfig);

	if (bUseServerSideFallbacks)
	{
		Root->SetStringField(TEXT("fallbacks"), TEXT("default"));
	}

	FString UserContent = FString::Printf(TEXT("<telemetry>\n%s\n</telemetry>"), *TelemetryData);
	if (!CrewStatement.IsEmpty())
	{
		UserContent += FString::Printf(TEXT("\n<crew_statement>\n%s\n</crew_statement>"), *CrewStatement);
	}

	TSharedRef<FJsonObject> UserMessage = MakeShared<FJsonObject>();
	UserMessage->SetStringField(TEXT("role"), TEXT("user"));
	UserMessage->SetStringField(TEXT("content"), UserContent);

	TArray<TSharedPtr<FJsonValue>> Messages;
	Messages.Add(MakeShared<FJsonValueObject>(UserMessage));
	Root->SetArrayField(TEXT("messages"), Messages);

	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}

void UClaudeAPIClient::SendTelemetryToClaude(const FString& TelemetryData, const FString& CrewStatement)
{
	if (ApiKey.IsEmpty())
	{
		ApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("ANTHROPIC_API_KEY"));
	}
	if (ApiKey.IsEmpty())
	{
		Fail(0, TEXT("UPLINK OFFLINE: no API key configured."));
		return;
	}
	if (ActiveRequest.IsValid())
	{
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Endpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("x-api-key"), ApiKey);
	Request->SetHeader(TEXT("anthropic-version"), AnthropicVersion);
	if (bUseServerSideFallbacks)
	{
		Request->SetHeader(TEXT("anthropic-beta"), TEXT("server-side-fallback-2026-07-01"));
	}
	Request->SetTimeout(RequestTimeoutSeconds);
	Request->SetContentAsString(BuildRequestBody(TelemetryData, CrewStatement));
	Request->OnProcessRequestComplete().BindUObject(this, &UClaudeAPIClient::OnResponseReceived);

	ActiveRequest = Request;
	if (!Request->ProcessRequest())
	{
		ActiveRequest.Reset();
		Fail(0, TEXT("UPLINK FAULT: HTTP request could not be started."));
	}
}

void UClaudeAPIClient::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	ActiveRequest.Reset();

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		Fail(0, TEXT("UPLINK LOST: no response."));
		return;
	}

	const int32 StatusCode = Response->GetResponseCode();
	const FString Body = Response->GetContentAsString();
	if (StatusCode != 200)
	{
		// 429 / 529 / 5xx are transient; 4xx means fix the key or request.
		Fail(StatusCode, FString::Printf(TEXT("HTTP %d: %s"), StatusCode, *ExtractApiErrorMessage(Body)));
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Fail(StatusCode, TEXT("Malformed JSON in response."));
		return;
	}

	FString StopReason;
	Root->TryGetStringField(TEXT("stop_reason"), StopReason);
	if (StopReason == TEXT("refusal"))
	{
		Fail(StatusCode, TEXT("Request declined by model safety system."));
		return;
	}

	// Content is a list of blocks (thinking, text, fallback...). Keep only text.
	FString ResponseText;
	const TArray<TSharedPtr<FJsonValue>>* ContentBlocks = nullptr;
	if (Root->TryGetArrayField(TEXT("content"), ContentBlocks) && ContentBlocks)
	{
		for (const TSharedPtr<FJsonValue>& BlockValue : *ContentBlocks)
		{
			if (!BlockValue.IsValid() || BlockValue->Type != EJson::Object)
			{
				continue;
			}
			const TSharedPtr<FJsonObject> Block = BlockValue->AsObject();
			FString BlockType;
			FString Text;
			if (Block.IsValid() && Block->TryGetStringField(TEXT("type"), BlockType) && BlockType == TEXT("text") &&
				Block->TryGetStringField(TEXT("text"), Text))
			{
				ResponseText += Text;
			}
		}
	}

	ResponseText.TrimStartAndEndInline();
	if (ResponseText.IsEmpty())
	{
		Fail(StatusCode, FString::Printf(TEXT("Empty response (stop_reason=%s)."), *StopReason));
		return;
	}
	if (StopReason == TEXT("max_tokens"))
	{
		UE_LOG(LogMarsSurvival, Warning, TEXT("A.R.I.A.: response truncated at max_tokens (%d)."), MaxTokens);
	}

	UE_LOG(LogMarsSurvival, Log, TEXT("%s"), *ResponseText);
	OnAriaResponseReceived.Broadcast(ResponseText);
}

FString UClaudeAPIClient::ExtractApiErrorMessage(const FString& ResponseBody)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(ResponseBody);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (Root->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject && ErrorObject->IsValid())
		{
			FString Type, Message;
			(*ErrorObject)->TryGetStringField(TEXT("type"), Type);
			(*ErrorObject)->TryGetStringField(TEXT("message"), Message);
			return FString::Printf(TEXT("%s - %s"), *Type, *Message);
		}
	}
	return ResponseBody.Left(512);
}

void UClaudeAPIClient::Fail(int32 StatusCode, const FString& Message)
{
	UE_LOG(LogMarsSurvival, Warning, TEXT("A.R.I.A. request failed: %s"), *Message);
	OnAriaRequestFailed.Broadcast(StatusCode, Message);
}
