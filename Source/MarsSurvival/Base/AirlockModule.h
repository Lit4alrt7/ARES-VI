#pragma once

#include "CoreMinimal.h"
#include "Base/MarsBaseModule.h"
#include "AirlockModule.generated.h"

UENUM(BlueprintType)
enum class EAirlockState : uint8
{
	PressurizedInnerOpen    UMETA(DisplayName = "Pressurized (inner hatch open)"),
	Depressurizing          UMETA(DisplayName = "Depressurizing"),
	DepressurizedOuterOpen  UMETA(DisplayName = "Depressurized (outer hatch open)"),
	DeconAndRepressurizing  UMETA(DisplayName = "Electrostatic decon + repressurizing")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAirlockStateChanged, EAirlockState, NewState);

/**
 * Airlock & Decon. Pumps between Mars ambient (~0.6 kPa / 6 mbar) and 101.3 kPa.
 * On the way in, an electrostatic curtain strips perchlorate-laden dust from suits; skipping it
 * tracks regolith into the habitat.
 *
 * Layout: +X end = outer hatch (surface), -X end = inner hatch (station).
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AAirlockModule : public AMarsBaseModule
{
	GENERATED_BODY()

public:
	AAirlockModule();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Toggle: inside -> go out, outside -> come in. Ignored while a cycle runs. */
	UFUNCTION(BlueprintCallable, Category = "Airlock")
	bool RequestCycle();

	UFUNCTION(BlueprintPure, Category = "Airlock")
	EAirlockState GetAirlockState() const { return AirlockState; }

	UFUNCTION(BlueprintPure, Category = "Airlock")
	bool IsCycling() const;

	UFUNCTION(BlueprintPure, Category = "Airlock")
	float GetCycleProgress() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> InnerHatch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OuterHatch;

	/** Pump speed in kPa per real second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	float PumpRateKpaPerSecond = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	float DeconDurationSeconds = 6.0f;

	UPROPERTY(BlueprintAssignable, Category = "Airlock")
	FOnAirlockStateChanged OnAirlockStateChanged;

protected:
	virtual void TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds) override;
	virtual FString GetModuleSpecificTelemetry() const override;

private:
	void SetState(EAirlockState NewState);
	void SetHatchOpen(UStaticMeshComponent* Hatch, bool bOpen);
	void PlaceHatches();
	void DecontaminateOccupants(float RealDeltaTime);

	UPROPERTY(VisibleAnywhere, Category = "Airlock")
	EAirlockState AirlockState = EAirlockState::PressurizedInnerOpen;

	float DeconElapsed = 0.0f;
};
