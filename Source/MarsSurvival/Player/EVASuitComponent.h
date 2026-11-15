#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EVASuitComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuitWarning, FName, Warning);

/**
 * Mechanical Counter Pressure (MCP) EVA suit: elastic skin-tight garment applies pressure to the
 * body mechanically; only the helmet holds gas (pure O2 at ~30 kPa). MCP suits tolerate small
 * punctures far better than gas-pressurised suits (local edema, no loss of total pressure).
 *
 * Consumables: O2 tank (litres STP), LiOH CO2 scrubber, battery for heaters.
 * Dust: regolith accumulates on the suit during EVA and must be stripped in the airlock decon.
 */
UCLASS(ClassGroup = (MarsSurvival), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UEVASuitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEVASuitComponent();

	/** Advance consumables. Called by the character each frame. */
	void UpdateSuit(float DeltaTime, float HeartRateBpm, float ExternalTemperatureC, float StormIntensity, bool bOutside);

	UFUNCTION(BlueprintCallable, Category = "EVA Suit")
	void DonSuit();

	UFUNCTION(BlueprintCallable, Category = "EVA Suit")
	void DoffSuit();

	UFUNCTION(BlueprintPure, Category = "EVA Suit")
	bool IsSuitDonned() const { return bSuitDonned; }

	/** Helmet sealed = breathing from the suit loop. */
	UFUNCTION(BlueprintPure, Category = "EVA Suit")
	bool IsHelmetSealed() const { return bSuitDonned; }

	/** Breathing gas inside the helmet. */
	UFUNCTION(BlueprintPure, Category = "EVA Suit")
	void GetBreathingEnvironment(float& OutPressureKpa, float& OutCo2Ppm, float& OutO2Fraction) const;

	/** Returns litres actually added. */
	UFUNCTION(BlueprintCallable, Category = "EVA Suit")
	float RefillO2(float Liters);

	/** Full LiOH + battery service (suit rack). */
	UFUNCTION(BlueprintCallable, Category = "EVA Suit")
	void ServiceConsumables();

	UFUNCTION(BlueprintCallable, Category = "EVA Suit")
	void RemoveDust(float Amount) { DustLoad = FMath::Max(DustLoad - Amount, 0.0f); }

	UFUNCTION(BlueprintPure, Category = "EVA Suit")
	float GetO2Fraction01() const { return O2TankCapacity > 0.0f ? O2RemainingLiters / O2TankCapacity : 0.0f; }

	/** Real-time minutes of O2 left at current consumption. */
	UFUNCTION(BlueprintPure, Category = "EVA Suit")
	float GetO2MinutesRemainingReal() const;

	UFUNCTION(BlueprintPure, Category = "EVA Suit")
	FString GetTelemetryReport() const;

	// ---- State ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EVA Suit")
	bool bSuitDonned = false;

	/** Primary O2 tank capacity in litres at STP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EVA Suit|O2")
	float O2TankCapacity = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EVA Suit|O2")
	float O2RemainingLiters = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EVA Suit|O2")
	float HelmetSetpointKpa = 30.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EVA Suit|O2")
	float HelmetPressureKpa = 30.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EVA Suit|O2")
	float HelmetCo2Ppm = 600.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EVA Suit|O2")
	float CurrentO2ConsumptionLpm = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EVA Suit|Scrubber")
	float LiOHCapacityHours = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EVA Suit|Scrubber")
	float LiOHRemainingHours = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EVA Suit|Power")
	float BatteryCapacityWh = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EVA Suit|Power")
	float BatteryWh = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EVA Suit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SuitIntegrity = 1.0f;

	/** Regolith dust on the suit, 0..1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EVA Suit")
	float DustLoad = 0.0f;

	UPROPERTY(BlueprintAssignable, Category = "EVA Suit")
	FOnSuitWarning OnSuitWarning;

private:
	void Warn(FName Warning);

	TSet<FName> ActiveWarnings;
};
