#pragma once

#include "CoreMinimal.h"
#include "Base/MarsBaseModule.h"
#include "BioDomeModule.generated.h"

/**
 * Aeroponic Bio-Dome with Chlorella vulgaris photobioreactors.
 * Photosynthesis (6CO2 + 6H2O -> C6H12O6 + 6O2) scrubs CO2 and adds O2 across the whole station.
 * The culture needs grow-light power, 15-35 C, cabin pressure, and is poisoned by perchlorate dust.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ABioDomeModule : public AMarsBaseModule
{
	GENERATED_BODY()

public:
	ABioDomeModule();

	virtual float GetPowerDemandW() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> GrowLight;

	/** 0..1 health of the algal culture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bio-Dome", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChlorellaCultureHealth = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bio-Dome")
	bool bGrowLightsOnline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bio-Dome")
	float GrowLightPowerW = 900.0f;

	/** CO2 removed from every module per game hour at full health (station-wide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bio-Dome")
	float StationCo2RemovalPpmPerHour = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bio-Dome")
	float StationO2AdditionPerHour = 0.002f;

	UFUNCTION(BlueprintPure, Category = "Bio-Dome")
	float GetPhotosyntheticOutput() const { return PhotosyntheticOutput; }

protected:
	virtual void TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds) override;
	virtual FString GetModuleSpecificTelemetry() const override;

private:
	float PhotosyntheticOutput = 0.0f;
};
