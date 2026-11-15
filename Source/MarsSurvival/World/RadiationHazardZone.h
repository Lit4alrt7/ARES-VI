#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RadiationHazardZone.generated.h"

class USphereComponent;
class AMarsCharacter;

/**
 * Localised radiation hotspot on the surface (e.g. a thorium-rich exposed outcrop, an old
 * lander's damaged RTG, or a crater that focuses secondary neutrons). Intensifies during
 * dust storms / SPEs. Adds dose rate to characters inside, falling off with distance.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ARadiationHazardZone : public AActor
{
	GENERATED_BODY()

public:
	ARadiationHazardZone();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Zone;

	/** Dose rate at the centre in clear weather (mSv per game hour). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard")
	float PeakDoseRateMsvPerHour = 0.5f;

	/** Multiplier at full storm intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard")
	float StormMultiplier = 4.0f;

	UFUNCTION(BlueprintPure, Category = "Hazard")
	float GetDoseRateAt(const FVector& WorldLocation) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Dose rate this zone currently contributes to each overlapping character. */
	TMap<TWeakObjectPtr<AMarsCharacter>, float> AppliedRates;
};
