#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/IHttpRequest.h"
#include "ClaudeAPIClient.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAriaResponseReceived, const FString&, ResponseText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAriaRequestFailed, int32, HttpStatusCode, const FString&, ErrorMessage);

/**
 * HTTP client for A.R.I.A. (Automated Research & Infrastructure Assistant) on the Claude Messages API.
 *
 * API key: ANTHROPIC_API_KEY environment variable, or SetApiKey() at runtime. Never a UPROPERTY,
 * so it is not serialised into assets or packaged config. Anything shipped in a client build can be
 * extracted - for a public release, route requests through your own backend instead.
 */
UCLASS(BlueprintType, Blueprintable, Config = Game)
class MARSSURVIVAL_API UClaudeAPIClient : public UObject
{
	GENERATED_BODY()

public:
	UClaudeAPIClient();

	/** Caller must keep the returned object referenced (UPROPERTY). */
	UFUNCTION(BlueprintCallable, Category = "ARIA", meta = (DefaultToSelf = "Outer"))
	static UClaudeAPIClient* CreateAriaClient(UObject* Outer);

	/**
	 * Send sensor telemetry (and optionally what the crew member said) to A.R.I.A.
	 * Result on OnAriaResponseReceived / OnAriaRequestFailed (game thread). Dropped if a request is in flight.
	 */
	UFUNCTION(BlueprintCallable, Category = "ARIA")
	void SendTelemetryToClaude(const FString& TelemetryData, const FString& CrewStatement = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "ARIA")
	void SetApiKey(const FString& InApiKey) { ApiKey = InApiKey; }

	UFUNCTION(BlueprintPure, Category = "ARIA")
	bool HasApiKey() const { return !ApiKey.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "ARIA")
	bool IsRequestInFlight() const { return ActiveRequest.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "ARIA")
	void CancelRequest();

	/** Parses the Messages API JSON and broadcasts the text content. */
	void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	UPROPERTY(BlueprintAssignable, Category = "ARIA")
	FOnAriaResponseReceived OnAriaResponseReceived;

	UPROPERTY(BlueprintAssignable, Category = "ARIA")
	FOnAriaRequestFailed OnAriaRequestFailed;

	// ---- Config (DefaultGame.ini: [/Script/MarsSurvival.ClaudeAPIClient]) ----

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ARIA|Config")
	FString Endpoint = TEXT("https://api.anthropic.com/v1/messages");

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ARIA|Config")
	FString Model = TEXT("claude-opus-5-5");

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ARIA|Config")
	FString AnthropicVersion = TEXT("2026-10-21");

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ARIA|Config", meta = (ClampMin = "256"))
	int32 MaxTokens = 4096;

	/** low | medium | high | xhigh | max. "low" keeps A.R.I.A. fast and terse. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ARIA|Config")
	FString Effort = TEXT("low");

	/** Let the API retry on a fallback model if the primary model declines a request. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ARIA|Config")
	bool bUseServerSideFallbacks = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ARIA|Config", meta = (ClampMin = "5.0"))
	float RequestTimeoutSeconds = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARIA|Config", meta = (MultiLine = "true"))
	FString SystemPrompt;

protected:
	virtual void BeginDestroy() override;

private:
	FString BuildRequestBody(const FString& TelemetryData, const FString& CrewStatement) const;
	void Fail(int32 StatusCode, const FString& Message);
	static FString ExtractApiErrorMessage(const FString& ResponseBody);

	FString ApiKey;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
};
