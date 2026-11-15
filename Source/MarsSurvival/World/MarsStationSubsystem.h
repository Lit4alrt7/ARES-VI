#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Base/MarsModuleTypes.h"
#include "MarsStationSubsystem.generated.h"

class AMarsBaseModule;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStructuralImpact, AMarsBaseModule*, Module, FVector, ImpactLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStationAlert, FName, AlertType, AMarsBaseModule*, Module);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEarthContactEstablished);

/**
 * Ares Station as a whole: module registry, electrical grid (RTG array in the Comm-Relay module),
 * kinetic-event log (gunfire / bulkhead impacts), station alerts and the Earth-contact win state.
 */
UCLASS()
class MARSSURVIVAL_API UMarsStationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ---- Registry ----
	void RegisterModule(AMarsBaseModule* Module);
	void UnregisterModule(AMarsBaseModule* Module);

	UFUNCTION(BlueprintPure, Category = "Station")
	TArray<AMarsBaseModule*> GetModules() const;

	UFUNCTION(BlueprintPure, Category = "Station")
	AMarsBaseModule* FindModuleByType(EMarsModuleType Type) const;

	/** Module whose interior volume contains the point, or null (= outside on the surface). */
	UFUNCTION(BlueprintPure, Category = "Station")
	AMarsBaseModule* FindModuleContainingPoint(const FVector& WorldLocation) const;

	// ---- Power grid ----

	/** 0..1, fraction of requested electrical power the grid can deliver this frame. */
	UFUNCTION(BlueprintPure, Category = "Station|Power")
	float GetPowerAvailability() const { return PowerAvailability; }

	UFUNCTION(BlueprintPure, Category = "Station|Power")
	float GetPowerSupplyW() const { return PowerSupplyW; }

	UFUNCTION(BlueprintPure, Category = "Station|Power")
	float GetPowerDemandW() const { return PowerDemandW; }

	/** Engineering routing profile: which sectors get priority when the RTG cannot cover the load. */
	void CyclePowerRoute();

	/** Electromagnetic storm shield (Engineering terminal, hold E): +4 kW demand, blocks storm damage. */
	void SetEmShield(bool bUp) { bEmShieldUp = bUp; }
	bool IsEmShieldUp() const { return bEmShieldUp; }
	/** Solar-storm electronics failure: multiplies grid output (1 = healthy). */
	void SetStormElectronicsPenalty(float Factor) { StormPenalty = FMath::Clamp(Factor, 0.1f, 1.0f); }
	FString GetPowerRouteName() const;
	FString GetPowerRouteSummary() const;
	int32 GetPowerRoute() const { return PowerRoute; }
	void SetPowerRoute(int32 Route) { PowerRoute = FMath::Clamp(Route, 0, 4); }

	/** Power fraction actually delivered to this module after routing (0..1). */
	float GetModulePowerAvailability(const class AMarsBaseModule* Module) const;

	/** Corridor / non-priority lighting level for the current profile (dimmed lights save power). */
	float GetLightingLevel() const { return PowerRoute == 0 ? 1.0f : 0.45f; }

	/** Used when no Comm-Relay module (which hosts the RTG array) is present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Power")
	float FallbackPowerSupplyW = 16000.0f;

	// ---- Kinetic event log ----

	/** Called by the Glock on every discharge. */
	UFUNCTION(BlueprintCallable, Category = "Station|Events")
	void ReportWeaponDischarge(AActor* Shooter, const FVector& MuzzleLocation, bool bHitStructure);

	/** Called by modules when a projectile penetrates the hull. */
	void ReportStructuralImpact(AMarsBaseModule* Module, const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = "Station|Events")
	void RaiseAlert(FName AlertType, AMarsBaseModule* Module);

	UFUNCTION(BlueprintPure, Category = "Station|Events")
	int32 GetDischargesInLastSeconds(float WindowSeconds) const;

	UFUNCTION(BlueprintPure, Category = "Station|Events")
	int32 GetTotalDischarges() const { return TotalDischarges; }

	UFUNCTION(BlueprintPure, Category = "Station|Events")
	int32 GetTotalStructuralImpacts() const { return TotalStructuralImpacts; }

	UFUNCTION(BlueprintCallable, Category = "Station|Science")
	void RecordSampleAnalysed() { ++SamplesAnalysed; }

	UFUNCTION(BlueprintPure, Category = "Station|Science")
	int32 GetSamplesAnalysed() const { return SamplesAnalysed; }

	/** Set by the Xenolab spectrometer after assaying a bio-agent culture: unlocks targeted countermeasures. */
	UFUNCTION(BlueprintCallable, Category = "Station|Science")
	void SetBioAgentCharacterized() { bBioAgentCharacterized = true; }

	UFUNCTION(BlueprintPure, Category = "Station|Science")
	bool IsBioAgentCharacterized() const { return bBioAgentCharacterized; }

	// ---- Mission ----

	UFUNCTION(BlueprintCallable, Category = "Station|Mission")
	void NotifyEarthContact();

	UFUNCTION(BlueprintPure, Category = "Station|Mission")
	bool HasEarthContact() const { return bEarthContact; }

	/** Concatenated physical-sensor telemetry of every module + grid + kinetic log (for A.R.I.A.). */
	UFUNCTION(BlueprintPure, Category = "Station")
	FString BuildStationTelemetry() const;

	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnStructuralImpact OnStructuralImpact;

	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnStationAlert OnStationAlert;

	UPROPERTY(BlueprintAssignable, Category = "Station|Mission")
	FOnEarthContactEstablished OnEarthContactEstablished;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void UpdatePowerGrid();

	UPROPERTY()
	TArray<TWeakObjectPtr<AMarsBaseModule>> Modules;

	float PowerSupplyW = 0.0f;
	float PowerDemandW = 0.0f;
	float PowerAvailability = 1.0f;
	int32 PowerRoute = 0;
	bool bEmShieldUp = false;
	float StormPenalty = 1.0f;

	TArray<double> DischargeTimestamps;
	int32 TotalDischarges = 0;
	int32 TotalStructuralImpacts = 0;
	int32 SamplesAnalysed = 0;
	bool bEarthContact = false;
	bool bBioAgentCharacterized = false;
};
