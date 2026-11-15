#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AriaSubsystem.generated.h"

class UClaudeAPIClient;
class AMarsBaseModule;

USTRUCT(BlueprintType)
struct FAriaMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ARIA")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "ARIA")
	float WorldTime = 0.0f;

	/** True when generated locally (no uplink). */
	UPROPERTY(BlueprintReadOnly, Category = "ARIA")
	bool bOffline = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAriaMessage, const FAriaMessage&, Message);

/**
 * Owns A.R.I.A.: gathers physical telemetry (station, environment, crew sensors, suit) and asks the
 * Claude client for a report periodically and on events (breach, decompression, gunfire bursts,
 * critical vitals, crew queries). Falls back to a deterministic local report if offline.
 */
UCLASS()
class MARSSURVIVAL_API UAriaSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Ask for a report now. Reason is a short event tag; CrewStatement is what the player said. */
	UFUNCTION(BlueprintCallable, Category = "ARIA")
	void RequestReport(FName Reason, const FString& CrewStatement = TEXT(""));

	/** Scripted line (story beats): logged, shown and spoken. */
	UFUNCTION(BlueprintCallable, Category = "ARIA")
	void SayScripted(const FString& Text);

	UFUNCTION(BlueprintPure, Category = "ARIA")
	FString BuildTelemetry(FName Reason) const;

	UFUNCTION(BlueprintPure, Category = "ARIA")
	TArray<FAriaMessage> GetMessageLog() const { return MessageLog; }

	UFUNCTION(BlueprintPure, Category = "ARIA")
	bool IsAwaitingResponse() const;

	UFUNCTION(BlueprintPure, Category = "ARIA")
	UClaudeAPIClient* GetClient() const { return Client; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARIA")
	float PeriodicReportSeconds = 120.0f;

	/** Minimum gap between automatic event-driven reports. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARIA")
	float EventCooldownSeconds = 20.0f;

	UPROPERTY(BlueprintAssignable, Category = "ARIA")
	FOnAriaMessage OnAriaMessage;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UFUNCTION()
	void HandleResponse(const FString& ResponseText);

	UFUNCTION()
	void HandleFailure(int32 HttpStatusCode, const FString& ErrorMessage);

	UFUNCTION()
	void HandleStationAlert(FName AlertType, AMarsBaseModule* Module);

	UFUNCTION()
	void HandleVitalsCritical(FName Reason);

private:
	void PushMessage(const FString& Text, bool bOffline);
	FString BuildOfflineReport(FName Reason, const FString& CrewStatement) const;

	UPROPERTY(Transient)
	TObjectPtr<UClaudeAPIClient> Client;

	TArray<FAriaMessage> MessageLog;
	FName PendingReason = NAME_None;
	FString PendingStatement;
	float TimeSincePeriodic = 0.0f;
	float TimeSinceEvent = 1000.0f;
	int32 LastSeenDischarges = 0;
	bool bBoundToPlayer = false;
};
