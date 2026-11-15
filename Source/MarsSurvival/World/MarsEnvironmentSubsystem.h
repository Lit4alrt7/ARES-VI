#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MarsEnvironmentSubsystem.generated.h"

class AExponentialHeightFog;
class ADirectionalLight;

UENUM(BlueprintType)
enum class EDustStormPhase : uint8
{
	Clear,
	Building,
	Storm,
	Subsiding
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDustStormPhaseChanged, EDustStormPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSolarParticleEventChanged, bool, bActive);

/**
 * Martian surface environment: sol clock, diurnal temperature, regional dust storms and
 * solar particle events (SPE). Drives fog density and sunlight if those actors exist in the level.
 *
 * Temperatures follow a cosine diurnal cycle (max ~-10 C mid-afternoon, min -80 C before dawn,
 * occasionally -100 C). Dust storms damp the diurnal swing (dust absorbs sunlight by day and
 * re-radiates at night), as observed by Viking / Curiosity REMS.
 */
UCLASS()
class MARSSURVIVAL_API UMarsEnvironmentSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	float GetTimeScale() const { return TimeScale; }

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	float GetLocalSolarTimeHours() const { return LocalSolarTimeHours; }

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	int32 GetSolNumber() const { return SolNumber; }

	/** Fast-forward the clock (sleeping). Rolls Sols, re-rolls the night minimum, updates sky + temperature. */
	void SkipHours(float GameHours);

	/** Save / load. */
	void SetClock(int32 InSol, float InHours);

	/** Weather engine overrides (Solar Storm manager): <0 = no override. */
	void SetForcedStorm(float Intensity) { ForcedStormIntensity = Intensity; }
	void SetForcedSolarDose(float MsvPerHour) { ForcedSolarDoseMsvH = MsvPerHour; }
	/** Above the atmosphere: unfiltered sunlight, no dust fog. */
	void SetOrbitLighting(bool bOrbit) { bOrbitLighting = bOrbit; }

	/** CLEAR / DUST HAZE / DUST STORM / SOLAR STORM. */
	FString GetWeatherName() const;
	/** 0..3 matching GetWeatherName. */
	int32 GetWeatherLevel() const;

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	float GetAmbientTemperatureC() const { return AmbientTemperatureC; }

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	float GetStormIntensity() const { return StormIntensity; }

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	EDustStormPhase GetStormPhase() const { return StormPhase; }

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	bool IsSolarParticleEventActive() const { return bSolarParticleEvent || ForcedSolarDoseMsvH > 0.0f; }

	/** Extra dose rate from an active SPE, unshielded (mSv per game hour). */
	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	float GetSolarParticleDoseRateMsvPerHour() const { return FMath::Max(bSolarParticleEvent ? SpeDoseRateMsvPerHour : 0.0f, ForcedSolarDoseMsvH); }

	/** Horizontal visibility in metres (drops to tens of metres in a storm). */
	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	float GetVisibilityMeters() const;

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	bool IsNight() const;

	/** Sun elevation in degrees (negative below horizon). */
	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	float GetSunElevationDegrees() const;

	UFUNCTION(BlueprintCallable, Category = "Mars Environment")
	void ForceDustStorm(float DurationGameHours = 8.0f);

	UFUNCTION(BlueprintCallable, Category = "Mars Environment")
	void ForceSolarParticleEvent(float DurationGameHours = 3.0f);

	UFUNCTION(BlueprintPure, Category = "Mars Environment")
	FString GetTelemetryReport() const;

	// ---- Tuning ----

	/** Game seconds per real second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float TimeScale = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float StartTimeHours = 7.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float DayMaxTemperatureC = -10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float NightMinTemperatureC = -80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float ExtremeNightMinTemperatureC = -100.0f;

	/** Chance per sol that the coming night drops to the extreme minimum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float ExtremeNightChance = 0.0f; // every night bottoms out at -80 C

	/** Chance per game hour (while clear) that a regional storm starts building. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float StormChancePerHour = 0.03f;

	/** Chance per game hour during a storm that a solar particle event begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float SpeChancePerStormHour = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float SpeDoseRateMsvPerHour = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float ClearFogDensity = 0.0025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float StormFogDensity = 0.09f;

	/** Noon sun illuminance in lux (Mars gets ~43% of Earth's solar flux). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	float NoonSunIntensityLux = 4.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Environment|Tuning")
	bool bDriveSceneLighting = true;

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Mars Environment")
	FOnDustStormPhaseChanged OnDustStormPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Mars Environment")
	FOnSolarParticleEventChanged OnSolarParticleEventChanged;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void AdvanceClock(float GameHours);
	void UpdateTemperature();
	void UpdateStorm(float GameHours);
	void UpdateSolarParticleEvent(float GameHours);
	void ApplyToScene();
	void SetStormPhase(EDustStormPhase NewPhase, float PhaseDurationHours);
	void RollTonightMinimum();

	float LocalSolarTimeHours = 18.5f;
	int32 SolNumber = 1;
	float AmbientTemperatureC = -40.0f;
	float TonightMinTemperatureC = -80.0f;

	EDustStormPhase StormPhase = EDustStormPhase::Clear;
	float StormPhaseRemainingHours = 0.0f;
	float StormIntensity = 0.0f;
	float ForcedStormIntensity = -1.0f;
	float ForcedSolarDoseMsvH = 0.0f;
	bool bOrbitLighting = false;

	bool bSolarParticleEvent = false;
	float SpeRemainingHours = 0.0f;

	TWeakObjectPtr<AExponentialHeightFog> CachedFog;
	TWeakObjectPtr<ADirectionalLight> CachedSun;
};
