#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Base/MarsModuleTypes.h"
#include "MarsBaseModule.generated.h"

class AMarsBaseModule;
class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UTextRenderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHullPunctured, AMarsBaseModule*, Module, FVector, ImpactPoint, float, NewHullIntegrity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModuleEvent, AMarsBaseModule*, Module);

/**
 * One pressurised compartment of Ares Station.
 *
 * Builds its own walkable greybox shell (floor, ceiling, walls, doorways) from cube meshes so the
 * game is playable without art. Every shell piece belongs to this actor, so a bullet hitting a
 * wall routes into TakeDamage -> TakeBallisticDamage.
 *
 * Simulated (in game time, see MarsTime):
 *  - Thermal balance: heater (limited by grid power) vs. conduction to the Martian night.
 *  - Seal cold-contraction: elastomer seals stiffen below their critical temperature and leak.
 *  - Pressure: choked-flow blowdown through punctures / breach / seals vs. life-support make-up.
 *  - Atmosphere: CO2 and O2 from occupants and scrubbers; dust ingress through breaches.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsBaseModule : public AActor
{
	GENERATED_BODY()

public:
	AMarsBaseModule();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ------------------------------------------------------------------
	// Core simulation API
	// ------------------------------------------------------------------

	/** Accidental bullet impact (from hallucination-driven gunfire): punctures, may breach catastrophically. */
	UFUNCTION(BlueprintCallable, Category = "Mars Module|Damage")
	void TakeBallisticDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Mars Module|Damage")
	virtual void TakeBallisticDamageAtLocation(float DamageAmount, FVector ImpactPoint);

	/** Thermal model + seal contraction ("SpaceX effect": cold-embrittled O-rings leak). */
	UFUNCTION(BlueprintCallable, Category = "Mars Module|Simulation")
	void UpdateThermalContraction(float DeltaTime, float ExternalAmbientTemp);

	UFUNCTION(BlueprintCallable, Category = "Mars Module|Simulation")
	void UpdatePressure(float DeltaTime);

	// ------------------------------------------------------------------
	// Repair / maintenance
	// ------------------------------------------------------------------

	/** Sealant patches. Returns number of punctures actually sealed. */
	UFUNCTION(BlueprintCallable, Category = "Mars Module|Repair")
	int32 PatchPunctures(int32 NumPatches = 1);

	/** Structural weld repair; clears a catastrophic breach once integrity recovers. */
	UFUNCTION(BlueprintCallable, Category = "Mars Module|Repair")
	void RepairHull(float IntegrityRestored);

	/** Torque wrench: re-compress seals (adds SealIntegrity). */
	UFUNCTION(BlueprintCallable, Category = "Mars Module|Repair")
	void ApplySealTorque(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Mars Module|Repair")
	void ServiceSeals();

	// ------------------------------------------------------------------
	// Atmosphere exchange (used by Bio-Dome, airlock decon, suits tracking dust in)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Mars Module|Atmosphere")
	void RemoveCo2Ppm(float Ppm);

	UFUNCTION(BlueprintCallable, Category = "Mars Module|Atmosphere")
	void AddO2Fraction(float Fraction);

	UFUNCTION(BlueprintCallable, Category = "Mars Module|Atmosphere")
	void AddDustContamination(float Amount);

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Mars Module|State")
	float GetTotalLeakAreaCm2() const;

	UFUNCTION(BlueprintPure, Category = "Mars Module|State")
	bool IsDecompressing() const { return bIsDecompressing; }

	UFUNCTION(BlueprintPure, Category = "Mars Module|State")
	bool IsBreached() const { return bCatastrophicBreach; }

	UFUNCTION(BlueprintPure, Category = "Mars Module|State")
	bool IsPointInside(const FVector& WorldLocation, float MarginCm = 0.0f) const;

	UFUNCTION(BlueprintPure, Category = "Mars Module|State")
	FString GetModuleName() const;

	UFUNCTION(BlueprintPure, Category = "Mars Module|Power")
	virtual float GetPowerDemandW() const;

	/** Only the Comm-Relay module (RTG array) supplies power. */
	UFUNCTION(BlueprintPure, Category = "Mars Module|Power")
	virtual float GetPowerSupplyW() const { return 0.0f; }

	/** Physical-sensor readout (fed to A.R.I.A.). */
	UFUNCTION(BlueprintPure, Category = "Mars Module|Telemetry")
	FString GetTelemetryReport() const;

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteriorVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> CeilingLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> Ceiling;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> WallLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> WallRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> FrontWallA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> FrontWallB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> FrontLintel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> BackWallA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> BackWallB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> BackLintel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> WallLeftB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> WallLeftLintel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> WallRightB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> WallRightLintel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> WallLeftSill;

	/** Laminated sapphire/polycarbonate viewport pane. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Shell")
	TObjectPtr<UStaticMeshComponent> WindowGlass;

	/** Diegetic stencil signage mounted above each doorway (interior side). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Signage")
	TObjectPtr<UTextRenderComponent> SignFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Signage")
	TObjectPtr<UTextRenderComponent> SignBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Signage")
	TObjectPtr<UTextRenderComponent> SignLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Signage")
	TObjectPtr<UTextRenderComponent> SignRight;

	// ------------------------------------------------------------------
	// Layout
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	EMarsModuleType ModuleType = EMarsModuleType::EvaPrep;

	/** Interior size in cm (X length, Y width, Z height). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	FVector RoomSize = FVector(1000.0f, 600.0f, 320.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	float WallThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	FVector2D DoorwaySize = FVector2D(160.0f, 230.0f);

	/** Doorway in the +X end wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	bool bFrontDoorway = true;

	/** Doorway in the -X end wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	bool bBackDoorway = true;

	/** Doorway in the -Y side wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	bool bLeftDoorway = false;

	/** Doorway in the +Y side wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	bool bRightDoorway = false;

	/** Side-wall doorway size (cm) - smaller for maintenance crawl ducts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	FVector2D SideDoorwaySize = FVector2D(160.0f, 230.0f);

	/** Local X offset of the side doorways along the wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	float LeftDoorwayOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	float RightDoorwayOffset = 0.0f;

	/** Reinforced viewport window in the -Y wall (ignored if that wall has a doorway). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	bool bLeftWindow = false;

	/** Window width x height (cm) and sill height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	FVector WindowSize = FVector(260.0f, 120.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|Layout")
	TSoftObjectPtr<UMaterialInterface> ShellMaterial;

	/** e.g. "[SEC-05] MED-BAY". Used as the room title and in telemetry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Signage")
	FString SectorLabel;

	/** Text above each doorway (where it leads). Empty = sign hidden; side signs fall back to the room title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Signage")
	FString SignFrontText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Signage")
	FString SignBackText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Signage")
	FString SignLeftText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Signage")
	FString SignRightText;

	// ------------------------------------------------------------------
	// State
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|State", meta = (ClampMin = "0.0"))
	float CurrentPressureKpa = 101.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|State")
	float CurrentTemperatureC = 21.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HullIntegrity = 1.0f;

	/** Colour-coded sector lighting / trim (set by the station generator). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module|Look")
	FLinearColor SectorColor = FLinearColor(1.0f, 0.93f, 0.82f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SealIntegrity = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars Module|State")
	float SealTemperatureC = 21.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|State", meta = (ClampMin = "0.0"))
	float CurrentCo2Ppm = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CurrentO2Fraction = 0.21f;

	/** Regolith dust (perchlorate-laden) suspended / deposited inside, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars Module|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DustContamination = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars Module|State")
	int32 PunctureCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars Module|State")
	int32 TotalBallisticImpacts = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Systems")
	bool bHeatingOnline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Systems")
	bool bSealHeatersOnline = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Systems")
	bool bLifeSupportOnline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Systems")
	bool bCo2ScrubberOnline = true;

	/** Outside temperature (pulled from UMarsEnvironmentSubsystem each tick when available). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Environment")
	float ExternalAmbientTemperatureC = -80.0f;

	// ------------------------------------------------------------------
	// Tuning
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Pressure", meta = (ClampMin = "1.0"))
	float InternalVolumeM3 = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Pressure")
	float NominalPressureKpa = 101.3f;

	/** Mean Martian surface pressure (~6 mbar = 0.6 kPa). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Pressure")
	float MarsAmbientPressureKpa = 0.6f;

	/** Effective choked-flow discharge velocity (Cd x critical-flow factor), m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Pressure")
	float EffectiveDischargeVelocity = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Pressure")
	float RepressurizationRateKpaPerHour = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Pressure")
	float CriticalPressureKpa = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Damage")
	float IntegrityLossPerDamagePoint = 0.004f;

	/** 9 mm round ~ 0.64 cm^2 hole (petalled exit enlarges it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Damage")
	float BulletHoleAreaCm2 = 0.64f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BreachIntegrityThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Damage")
	float CatastrophicBreachAreaCm2 = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Thermal")
	float HeatingSetpointC = 21.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Thermal")
	float MaxHeaterPowerW = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Thermal")
	float ThermostatGainWPerK = 250.0f;

	/** Overall hull conductance U*A (W/K): multilayer insulation + regolith berm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Thermal")
	float HullThermalConductanceWPerK = 8.0f;

	/** Lumped thermal mass (J/K). Time constant = mass / conductance (~2 game hours). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Thermal")
	float ThermalMassJPerK = 57600.0f;

	/** 0 = seal sits at cabin temperature, 1 = at exterior temperature. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Seals", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SealExposureFactor = 0.5f;

	/** Below this, elastomer seals approach their glass transition and lose contact pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Seals")
	float SealCriticalTemperatureC = -25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Seals")
	float SealContractionRatePerKelvinHour = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Seals", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SealHeaterProtection = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Seals")
	float MaxSealLeakAreaCm2 = 3.0f;

	/** ~1 kg CO2 per crew-day into 60 m^3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Atmosphere")
	float Co2PerOccupantPpmPerHour = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Atmosphere")
	float ScrubberRemovalPpmPerHour = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Atmosphere")
	float ScrubberFloorPpm = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Atmosphere")
	float O2ConsumptionPerOccupantPerHour = 0.0005f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Atmosphere")
	float DustFiltrationPerHour = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Power")
	float LifeSupportPowerW = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Module|Tuning|Power")
	float BaseLoadPowerW = 60.0f;

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Mars Module|Events")
	FOnHullPunctured OnHullPunctured;

	UPROPERTY(BlueprintAssignable, Category = "Mars Module|Events")
	FOnModuleEvent OnHullBreached;

	UPROPERTY(BlueprintAssignable, Category = "Mars Module|Events")
	FOnModuleEvent OnDecompressionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Mars Module|Events")
	FOnModuleEvent OnDecompressionStopped;

	UPROPERTY(BlueprintAssignable, Category = "Mars Module|Events")
	FOnModuleEvent OnPressureCritical;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Subclass hook: runs after the shared simulation each tick, with the game-time delta in seconds. */
	virtual void TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds) {}

	/** Subclass hook: extra telemetry fields appended to GetTelemetryReport(). */
	virtual FString GetModuleSpecificTelemetry() const { return FString(); }

	/** Rebuilds wall/floor transforms from RoomSize. */
	void BuildShell();

	void UpdateAtmosphere(float GameHours);
	void UpdateLighting(float DeltaTime);
	float GetSealLeakAreaCm2() const;
	float GetGridPowerAvailability() const;
	class UMarsStationSubsystem* GetStation() const;

	UStaticMeshComponent* CreateShellPiece(const FName& Name);
	UTextRenderComponent* CreateSign(const FName& Name);
	void UpdateSignage();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars Module|State")
	float PunctureLeakAreaCm2 = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars Module|State")
	bool bCatastrophicBreach = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars Module|State")
	int32 OccupantCount = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ShellPieces;

	float LastHeaterPowerW = 0.0f;
	bool bIsDecompressing = false;
	bool bPressureCriticalLatched = false;
	float LightFlickerTimer = 0.0f;
};
