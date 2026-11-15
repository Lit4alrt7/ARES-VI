#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MarsInteractable.h"
#include "SurfaceBeacon.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Battery-powered navigation beacon: xenon strobe + radio ping shown on the helmet HUD
 * (bearing / range). Batteries drain faster in the cold. Press E to pick it back up.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ASurfaceBeacon : public AActor, public IMarsInteractable
{
	GENERATED_BODY()

public:
	ASurfaceBeacon();

	virtual void Tick(float DeltaTime) override;

	virtual void Interact_Implementation(AMarsCharacter* InteractingCharacter) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* InteractingCharacter) const override;

	UFUNCTION(BlueprintPure, Category = "Beacon")
	bool IsActive() const { return BatteryWh > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Beacon")
	float GetBatteryFraction() const { return BatteryCapacityWh > 0.0f ? BatteryWh / BatteryCapacityWh : 0.0f; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mast;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Strobe;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	FString BeaconLabel = TEXT("BCN");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	float BatteryCapacityWh = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	float BatteryWh = 40.0f;

	/** Base draw (strobe + transmitter) in W. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	float PowerDrawW = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	float StrobePeriodSeconds = 1.5f;

	/** Radio ping range in cm (HUD display). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	float RadioRangeCm = 300000.0f;

	/** Register with the nearest player automatically when placed in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beacon")
	bool bAutoRegisterWithPlayer = true;

protected:
	virtual void BeginPlay() override;

private:
	float StrobeTimer = 0.0f;
};
