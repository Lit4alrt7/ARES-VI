#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerBiometricsComponent.generated.h"

/** Phantom kinds the hallucination system can request. None of them are real. */
UENUM(BlueprintType)
enum class EPhantomType : uint8
{
	ShadowFigure        UMETA(DisplayName = "Shadow Figure"),
	PeripheralMovement  UMETA(DisplayName = "Peripheral Movement"),
	Footsteps           UMETA(DisplayName = "Footsteps"),
	HullKnocking        UMETA(DisplayName = "Hull Knocking"),
	Whisper             UMETA(DisplayName = "Whisper"),
	RadioVoice          UMETA(DisplayName = "Radio Voice"),
	MetalScraping       UMETA(DisplayName = "Metal Scraping")
};

/**
 * Disease progression stage (combined perchlorate methaemoglobinaemia, radiation injury and
 * extremophile neuro-agent load). The stage is the WORST of the three pathways.
 */
UENUM(BlueprintType)
enum class EDiseaseStage : uint8
{
	Stable              UMETA(DisplayName = "Stable"),
	Prodromal           UMETA(DisplayName = "Stage 1 - Prodromal & mild hypoxia"),
	AcuteMetHb          UMETA(DisplayName = "Stage 2 - Acute methaemoglobinaemia & hypercapnia"),
	NeuroToxic          UMETA(DisplayName = "Stage 3 - Neuro-toxic encephalopathy & radiation sickness"),
	SystemicCollapse    UMETA(DisplayName = "Stage 4 - Critical systemic collapse")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHallucinationChanged, float, NewIntensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCherenkovFlash, float, FlashStrength);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhantomTriggered, EPhantomType, PhantomType, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVitalsCritical, FName, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBiometricDeath, FName, CauseOfDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDiseaseStageChanged, EDiseaseStage, OldStage, EDiseaseStage, NewStage);

/**
 * Internal physiological decay of the protagonist.
 *
 * Pathways
 *  - Perchlorate (ClO4-) body burden -> oxidant stress -> methaemoglobin (Fe2+ -> Fe3+, "chocolate blood").
 *    Also competitively blocks the thyroid sodium-iodide symporter (KI counters this).
 *  - Ionising radiation: physical dose (mSv) and biological radiation injury (amifostine reduces accrual).
 *  - Extremophile neuro-agent: logistic replication; radiation raises endothelial / blood-brain barrier
 *    permeability so the load grows faster. Its neuroactive metabolite drives hallucinations.
 *  - MetHb + low inspired ppO2 -> functional hypoxaemia; inspired CO2 -> hypercapnia, panic.
 *  - Hydration and nutrition drain; dehydration worsens hallucinations and tachycardia.
 *
 * Stage thresholds (any pathway):    MetHb %      Rad injury mSv   Bio-load
 *   1 Prodromal                       3 - 10       50 - 150         0.15
 *   2 Acute MetHb / hypercapnia       10 - 25      150 - 400        0.40
 *   3 Neuro-toxic encephalopathy      25 - 45      400 - 750        0.70
 *   4 Systemic collapse               > 45         > 750            > 0.85   (lethal countdown)
 *
 * All rates are per GAME hour (see MarsTime).
 */
UCLASS(ClassGroup = (MarsSurvival), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UPlayerBiometricsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerBiometricsComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Biometrics")
	void UpdateBiometrics(float DeltaTime, bool bIsInDustStorm);

	// ---- Environment inputs ----

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Environment")
	void SetEnvironmentalConditions(float PressureKpa, float Co2Ppm, float TemperatureC, float O2Fraction = 0.21f, float DustLevel = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Environment")
	void SetOnEVA(bool bNewOnEVA) { bIsOnEVA = bNewOnEVA; }

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Environment")
	void SetDustStormActive(bool bActive) { bDustStormActive = bActive; }

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Environment")
	void SetAdditionalDoseRate(float DoseRateMsvPerHour) { AdditionalDoseRateMsvPerHour = FMath::Max(DoseRateMsvPerHour, 0.0f); }

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Environment")
	void SetExertion(float Exertion01) { Exertion = FMath::Clamp(Exertion01, 0.0f, 1.0f); }

	/** Sprinting drains stamina; returns false when exhausted (caller should stop sprinting). */
	UFUNCTION(BlueprintCallable, Category = "Biometrics|Environment")
	bool SetSprinting(bool bWantsSprint);

	// ---- Exposure ----

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Exposure")
	void ExposeToPerchlorate(float Milligrams);

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Exposure")
	void AddRadiationDose(float DoseMsv);

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Exposure")
	void ExposeToBioAgent(float LoadIncrease);

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Exposure")
	void NotifyStartle(float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Exposure")
	void NotifyWeaponDischarged() { NotifyStartle(0.12f); }

	// ---- Countermeasures ----

	/** Stage 2: 1% IV methylene blue - accelerates NADPH-MetHb reductase. */
	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	void AdministerMethyleneBlue();

	/** Stage 1: oral KI - saturates the thyroid NIS against perchlorate. */
	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	void AdministerIodine();

	/** Stage 3: targeted neuro-antitoxin - binds the bio-agent metabolite, suppresses replication. */
	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	void AdministerNeuroAntitoxin();

	/** Stage 3: amifostine radioprotectant - halves radiation injury accrual, aids repair. */
	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	void AdministerAmifostine();

	/** Stage 1: high-FiO2 supplemental oxygen (mask from an O2 canister). */
	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	void AdministerSupplementalO2(float GameHours = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	void AdministerPerchlorateClearance(float FractionRemoved = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	void AdministerSedative(float PanicReduction = 0.5f);

	/** Med-Bay scanner bed emergency stabilisation (resets the Stage 4 countdown). Returns false on cooldown. */
	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	bool EmergencyStabilize();

	/**
	 * Ballistic awakening: the gunshot's acoustic shock and muzzle flash spike adrenaline and break the
	 * delusion - hallucination intensity collapses and stays suppressed for ClaritySeconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Biometrics|Medical")
	void ApplyAwakeningClarity();

	UFUNCTION(BlueprintPure, Category = "Biometrics")
	bool IsInClarity() const { return ClarityRemaining > 0.0f; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Hallucination")
	float ClaritySeconds = 20.0f;

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Survival")
	void Drink(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Biometrics|Survival")
	void Eat(float Amount);

	// ---- Queries ----

	UFUNCTION(BlueprintPure, Category = "Biometrics")
	EDiseaseStage GetDiseaseStage() const { return DiseaseStage; }

	UFUNCTION(BlueprintPure, Category = "Biometrics")
	int32 GetStageIndex() const { return static_cast<int32>(DiseaseStage); }

	UFUNCTION(BlueprintPure, Category = "Biometrics")
	float GetStage4SecondsRemaining() const { return Stage4CountdownSeconds; }

	UFUNCTION(BlueprintPure, Category = "Biometrics")
	bool IsMethyleneBlueActive() const { return MethyleneBlueRemainingHours > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Biometrics")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Biometrics")
	FName GetCauseOfDeath() const { return CauseOfDeath; }

	UFUNCTION(BlueprintPure, Category = "Biometrics")
	float GetHypoxia01() const;

	/** Environmental death (hypothermia when the suit heater dies, etc.). */
	void KillFromExposure(FName Cause) { Die(Cause); }

	/**
	 * A hallucinated attack the body believes in: adrenaline dump, stamina crash, panic, and accumulated
	 * psychosomatic trauma. Trauma 1.0 = cardiac arrest. Decays with time, sleep and treatment.
	 */
	void ApplyPsychosomaticShock(float Amount);

	/** 0..1: how badly untreated conditions (disease stage, sleep deprivation, perchlorate load) are driving the psyche. */
	float GetUntreatedSeverity() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float PsychosomaticTrauma = 0.0f;

	/** Med-Bay diagnostic report: stage, scientific basis, observed symptoms, indicated countermeasures. */
	UFUNCTION(BlueprintPure, Category = "Biometrics|Medical")
	FString BuildDiagnosisReport(bool bBioAgentCharacterized) const;

	/** Recommended countermeasure item for the current stage (None if stable). */
	UFUNCTION(BlueprintPure, Category = "Biometrics|Medical")
	TArray<FString> GetIndicatedCountermeasures() const;

	/** Sensor readout for A.R.I.A. Excludes hallucination intensity (no sensor measures it). */
	UFUNCTION(BlueprintPure, Category = "Biometrics|Telemetry")
	FString GetTelemetryReport() const;

	// ------------------------------------------------------------------
	// State
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	EDiseaseStage DiseaseStage = EDiseaseStage::Stable;

	/** Physical absorbed/effective dose (what the dosimeter integrates). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|State", meta = (ClampMin = "0.0"))
	float RadiationDoseMsv = 14.0f;

	/** Biological radiation injury (dose weighted by radioprotection, minus repair). Drives staging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|State", meta = (ClampMin = "0.0"))
	float RadiationInjuryMsv = 12.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float CurrentDoseRateMsvPerHour = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|State", meta = (ClampMin = "0.0"))
	float Co2LevelPpm = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|State", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float MethemoglobinPercentage = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|State", meta = (ClampMin = "0.0"))
	float PerchlorateBodyBurdenMg = 8.0f;

	/** Extremophile neuro-agent load, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExtremophileBioLoad = 0.02f;

	/** 0..1, from UPlayerFatigueComponent (hours awake). Feeds hallucinations and stamina recovery. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float SleepDeprivation = 0.0f;

	/** 0..1, from UMarsNarrativeDirector: how far the story lets the neuro-symptoms escalate (0 on Sol 1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float NarrativeEscalation = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float EffectiveO2SaturationPct = 97.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float HeartRateBpm = 78.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float CortisolNmolL = 380.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float ThyroidFunction = 0.8f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float PanicLevel = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|State")
	float HallucinationIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|Survival", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Hydration = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biometrics|Survival", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Nutrition = 0.6f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|Survival")
	float Stamina = 1.0f;

	// ---- Environment (pushed by the character) ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|Environment")
	float AmbientPressureKpa = 101.3f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|Environment")
	float AmbientCo2Ppm = 800.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|Environment")
	float AmbientTemperatureC = 21.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|Environment")
	float AmbientO2Fraction = 0.21f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|Environment")
	float AmbientDustLevel = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|Environment")
	bool bIsOnEVA = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Biometrics|Environment")
	bool bDustStormActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning")
	bool bAutoUpdate = true;

	// ------------------------------------------------------------------
	// Tuning
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Radiation")
	float HabitatDoseRateMsvPerHour = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Radiation")
	float EvaDoseMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Radiation")
	float StormDoseMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Radiation")
	float BaselineFlashesPerSecond = 0.012f;

	/** Fraction of radiation injury repaired per game hour (DNA repair, cell turnover). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Radiation")
	float RadiationRepairPerHour = 0.004f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Toxicology")
	float DustInhalationMgPerHour = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Toxicology")
	float MetHbFormationPerMgHour = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Toxicology")
	float PerchlorateClearanceHalfLifeHours = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Toxicology")
	float MetHbReductionHalfLifeHours = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Toxicology")
	float MethyleneBlueHalfLifeHours = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Toxicology")
	float MethyleneBlueDurationHours = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Toxicology")
	float IodineDurationHours = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Toxicology")
	float BaselineMetHbPct = 1.0f;

	/** Logistic replication rate of the bio-agent per game hour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|BioAgent")
	float BioAgentGrowthPerHour = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|BioAgent")
	float AntitoxinDurationHours = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|BioAgent")
	float AmifostineDurationHours = 6.0f;

	/** Full -> empty in this many game hours at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Survival")
	float HydrationDrainHours = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Survival")
	float NutritionDrainHours = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Survival")
	float SprintStaminaDrainPerSecond = 0.16f;

	/** Real seconds you survive in Stage 4 without stabilisation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Stages")
	float Stage4LethalSeconds = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Stages")
	float StabilizeCooldownSeconds = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Hallucination")
	float HallucinationRiseSpeed = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Hallucination")
	float HallucinationDecaySpeed = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Hallucination")
	float MaxPhantomsPerSecond = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biometrics|Tuning|Hallucination")
	float PhantomCooldownSeconds = 5.0f;

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Biometrics|Events")
	FOnHallucinationChanged OnHallucinationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Biometrics|Events")
	FOnCherenkovFlash OnCherenkovFlash;

	UPROPERTY(BlueprintAssignable, Category = "Biometrics|Events")
	FOnPhantomTriggered OnPhantomTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Biometrics|Events")
	FOnVitalsCritical OnVitalsCritical;

	UPROPERTY(BlueprintAssignable, Category = "Biometrics|Events")
	FOnBiometricDeath OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Biometrics|Events")
	FOnDiseaseStageChanged OnDiseaseStageChanged;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateRadiation(float RealDt, float GameHours, bool bIsInDustStorm);
	void UpdateToxicology(float GameHours);
	void UpdateBioAgent(float GameHours);
	void UpdateSurvival(float RealDt, float GameHours);
	void UpdateRespiration(float RealDt, float GameHours);
	void UpdateCardiovascular(float RealDt);
	void UpdateDiseaseStage(float RealDt);
	void UpdateHallucinationCascade(float RealDt);
	void TryTriggerPhantom(float RealDt);
	void CheckCriticalThresholds(float RealDt);
	void RaiseCritical(FName Reason);
	void Die(FName Cause);

	static float AmbientSaturationFromPpO2(float PpO2Kpa);

	float MethyleneBlueRemainingHours = 0.0f;
	float IodineRemainingHours = 0.0f;
	float AntitoxinRemainingHours = 0.0f;
	float AmifostineRemainingHours = 0.0f;
	float SupplementalO2RemainingHours = 0.0f;
	float AdditionalDoseRateMsvPerHour = 0.0f;
	float Exertion = 0.0f;
	bool bSprinting = false;
	float PhantomCooldownRemaining = 0.0f;
	float LastBroadcastHallucination = -1.0f;
	float NoiseTime = 0.0f;
	float Stage4CountdownSeconds = 0.0f;
	float StabilizeCooldownRemaining = 0.0f;
	float ClarityRemaining = 0.0f;

	float SevereHypoxiaSeconds = 0.0f;
	float EbullismSeconds = 0.0f;
	float HypercapniaSeconds = 0.0f;
	float DehydrationSeconds = 0.0f;
	float StarvationSeconds = 0.0f;

	bool bIsDead = false;
	FName CauseOfDeath = NAME_None;

	TSet<FName> RaisedCriticals;
};
