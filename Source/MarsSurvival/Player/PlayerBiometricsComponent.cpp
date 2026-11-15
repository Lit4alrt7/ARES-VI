#include "Player/PlayerBiometricsComponent.h"

#include "MarsSurvival.h"
#include "Core/MarsTime.h"

namespace
{
	float DecayToward(float Current, float Target, float HalfLifeHours, float GameHours)
	{
		if (HalfLifeHours <= KINDA_SMALL_NUMBER)
		{
			return Target;
		}
		return Target + (Current - Target) * FMath::Exp(-0.69314718f / HalfLifeHours * GameHours);
	}

	float Saturate01(float X)
	{
		return FMath::Clamp(X, 0.0f, 1.0f);
	}

	int32 StageFromThresholds(float Value, float T1, float T2, float T3, float T4)
	{
		return Value >= T4 ? 4 : Value >= T3 ? 3 : Value >= T2 ? 2 : Value >= T1 ? 1 : 0;
	}
}

UPlayerBiometricsComponent::UPlayerBiometricsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPlayerBiometricsComponent::BeginPlay()
{
	Super::BeginPlay();
	NoiseTime = FMath::FRandRange(0.0f, 1000.0f);
	RadiationInjuryMsv = FMath::Max(RadiationInjuryMsv, 0.0f);
}

void UPlayerBiometricsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bAutoUpdate)
	{
		UpdateBiometrics(DeltaTime, bDustStormActive);
	}
}

void UPlayerBiometricsComponent::UpdateBiometrics(float DeltaTime, bool bIsInDustStorm)
{
	if (DeltaTime <= 0.0f || bIsDead)
	{
		return;
	}
	const float GameHours = MarsTime::ToGameHours(this, DeltaTime);

	UpdateRadiation(DeltaTime, GameHours, bIsInDustStorm);
	UpdateToxicology(GameHours);
	UpdateBioAgent(GameHours);
	UpdateSurvival(DeltaTime, GameHours);
	UpdateRespiration(DeltaTime, GameHours);
	UpdateCardiovascular(DeltaTime);
	UpdateDiseaseStage(DeltaTime);
	UpdateHallucinationCascade(DeltaTime);
	PsychosomaticTrauma = FMath::Max(PsychosomaticTrauma - GameHours * (AntitoxinRemainingHours > 0.0f ? 0.25f : 0.06f), 0.0f);
	TryTriggerPhantom(DeltaTime);
	CheckCriticalThresholds(DeltaTime);
}

// ----------------------------------------------------------------------------
// Radiation: physical dose vs. biological injury
// ----------------------------------------------------------------------------

void UPlayerBiometricsComponent::UpdateRadiation(float RealDt, float GameHours, bool bIsInDustStorm)
{
	float Multiplier = 1.0f;
	if (bIsOnEVA)
	{
		Multiplier *= EvaDoseMultiplier;
		if (bIsInDustStorm)
		{
			Multiplier *= StormDoseMultiplier;
		}
	}
	CurrentDoseRateMsvPerHour = HabitatDoseRateMsvPerHour * Multiplier + AdditionalDoseRateMsvPerHour;
	const float Dose = CurrentDoseRateMsvPerHour * GameHours;
	RadiationDoseMsv += Dose;

	// Amifostine (WR-2721) scavenges radiolytic free radicals: roughly halves effective injury while active.
	const bool bProtected = AmifostineRemainingHours > 0.0f;
	RadiationInjuryMsv += Dose * (bProtected ? 0.5f : 1.0f);
	RadiationInjuryMsv -= RadiationInjuryMsv * RadiationRepairPerHour * (bProtected ? 2.0f : 1.0f) * GameHours;
	RadiationInjuryMsv = FMath::Max(RadiationInjuryMsv, 0.0f);
	AmifostineRemainingHours = FMath::Max(AmifostineRemainingHours - GameHours, 0.0f);

	// Light flashes: Cherenkov light in the vitreous + direct retinal stimulation by heavy ions.
	const float StageBoost = 1.0f + 0.8f * GetStageIndex() + (DiseaseStage == EDiseaseStage::SystemicCollapse ? 6.0f : 0.0f);
	const float RateRatio = CurrentDoseRateMsvPerHour / FMath::Max(HabitatDoseRateMsvPerHour, KINDA_SMALL_NUMBER);
	const float FlashRate = BaselineFlashesPerSecond * RateRatio * StageBoost;
	if (FMath::FRand() < 1.0f - FMath::Exp(-FlashRate * RealDt))
	{
		OnCherenkovFlash.Broadcast(FMath::Clamp(FMath::FRandRange(0.3f, 1.0f) * FMath::Sqrt(RateRatio * StageBoost) / 3.0f, 0.15f, 1.0f));
	}
}

// ----------------------------------------------------------------------------
// Perchlorate / methaemoglobin / thyroid
// ----------------------------------------------------------------------------

void UPlayerBiometricsComponent::UpdateToxicology(float GameHours)
{
	PerchlorateBodyBurdenMg += DustInhalationMgPerHour * AmbientDustLevel * GameHours;
	PerchlorateBodyBurdenMg = DecayToward(PerchlorateBodyBurdenMg, 0.0f, PerchlorateClearanceHalfLifeHours, GameHours);

	MethemoglobinPercentage += MetHbFormationPerMgHour * PerchlorateBodyBurdenMg * GameHours;
	const bool bTreated = MethyleneBlueRemainingHours > 0.0f;
	if (MethemoglobinPercentage > BaselineMetHbPct)
	{
		MethemoglobinPercentage = DecayToward(MethemoglobinPercentage, BaselineMetHbPct,
			bTreated ? MethyleneBlueHalfLifeHours : MetHbReductionHalfLifeHours, GameHours);
	}
	MethemoglobinPercentage = FMath::Clamp(MethemoglobinPercentage, 0.0f, 100.0f);
	MethyleneBlueRemainingHours = FMath::Max(MethyleneBlueRemainingHours - GameHours, 0.0f);

	const float Inhibition = PerchlorateBodyBurdenMg / (PerchlorateBodyBurdenMg + 200.0f);
	const float IodineFactor = IodineRemainingHours > 0.0f ? 0.3f : 1.0f;
	ThyroidFunction = DecayToward(ThyroidFunction, 1.0f - 0.6f * Inhibition * IodineFactor, 6.0f, GameHours);
	IodineRemainingHours = FMath::Max(IodineRemainingHours - GameHours, 0.0f);
}

// ----------------------------------------------------------------------------
// Extremophile neuro-agent
// ----------------------------------------------------------------------------

void UPlayerBiometricsComponent::UpdateBioAgent(float GameHours)
{
	if (AntitoxinRemainingHours > 0.0f)
	{
		// Bound metabolite is cleared renally; replication suppressed.
		ExtremophileBioLoad -= 0.05f * GameHours;
		AntitoxinRemainingHours = FMath::Max(AntitoxinRemainingHours - GameHours, 0.0f);
	}
	else
	{
		// Logistic replication; radiation-induced endothelial permeability lets more agent cross the BBB.
		const float Permeability = Saturate01(RadiationInjuryMsv / 400.0f);
		ExtremophileBioLoad += BioAgentGrowthPerHour * (1.0f + Permeability) * ExtremophileBioLoad * (1.0f - ExtremophileBioLoad) * GameHours;
	}
	ExtremophileBioLoad = Saturate01(ExtremophileBioLoad);
}

// ----------------------------------------------------------------------------
// Hydration / nutrition / stamina
// ----------------------------------------------------------------------------

void UPlayerBiometricsComponent::UpdateSurvival(float RealDt, float GameHours)
{
	const float HeatFactor = AmbientTemperatureC > 28.0f ? 0.5f : 0.0f;
	const float FeverFactor = GetStageIndex() >= 2 ? 0.25f : 0.0f;
	Hydration = Saturate01(Hydration - GameHours / HydrationDrainHours * (1.0f + 0.6f * Exertion + HeatFactor + FeverFactor));
	Nutrition = Saturate01(Nutrition - GameHours / NutritionDrainHours * (1.0f + 0.5f * Exertion));

	if (bSprinting)
	{
		// Stage 1+: impaired O2 delivery makes muscles fatigue faster.
		Stamina -= SprintStaminaDrainPerSecond * (1.0f + 0.25f * GetStageIndex()) * RealDt;
	}
	else
	{
		const float Dehydration = Saturate01((0.25f - Hydration) / 0.25f);
		const float Starvation = Saturate01((0.2f - Nutrition) / 0.2f);
		const float Recovery = 0.12f * (1.0f - 0.6f * Dehydration) * (1.0f - 0.5f * Starvation)
			* (DiseaseStage == EDiseaseStage::SystemicCollapse ? 0.3f : 1.0f) * FMath::Lerp(0.5f, 1.0f, EffectiveO2SaturationPct / 97.0f);
		Stamina += Recovery * RealDt * (1.0f - 0.5f * SleepDeprivation);
	}
	Stamina = Saturate01(Stamina);
}

bool UPlayerBiometricsComponent::SetSprinting(bool bWantsSprint)
{
	if (bWantsSprint && Stamina < 0.03f)
	{
		bSprinting = false;
		return false;
	}
	if (bWantsSprint && !bSprinting && Stamina < 0.2f)
	{
		return false; // must recover a little before sprinting again
	}
	bSprinting = bWantsSprint;
	return bSprinting;
}

// ----------------------------------------------------------------------------
// Respiration / cardiovascular
// ----------------------------------------------------------------------------

float UPlayerBiometricsComponent::AmbientSaturationFromPpO2(float PpO2Kpa)
{
	return 97.0f * FMath::Pow(FMath::Clamp(PpO2Kpa / 16.0f, 0.0f, 1.0f), 0.4f);
}

void UPlayerBiometricsComponent::UpdateRespiration(float RealDt, float GameHours)
{
	Co2LevelPpm = FMath::FInterpTo(Co2LevelPpm, AmbientCo2Ppm, RealDt, 0.5f);

	float FiO2 = AmbientO2Fraction;
	if (SupplementalO2RemainingHours > 0.0f && AmbientPressureKpa > 50.0f)
	{
		FiO2 = FMath::Max(FiO2, 0.6f); // non-rebreather mask from an O2 canister
		SupplementalO2RemainingHours = FMath::Max(SupplementalO2RemainingHours - GameHours, 0.0f);
	}
	EffectiveO2SaturationPct = AmbientSaturationFromPpO2(AmbientPressureKpa * FiO2) * (1.0f - MethemoglobinPercentage / 100.0f);
}

float UPlayerBiometricsComponent::GetHypoxia01() const
{
	return Saturate01((92.0f - EffectiveO2SaturationPct) / 40.0f);
}

void UPlayerBiometricsComponent::UpdateCardiovascular(float RealDt)
{
	const float HypoxiaRaw = Saturate01((95.0f - EffectiveO2SaturationPct) / 45.0f);
	const float HypercapniaRaw = Saturate01((Co2LevelPpm - 5000.0f) / 45000.0f);
	const float Dehydration = Saturate01((0.25f - Hydration) / 0.25f);

	float TargetHr = 68.0f + 60.0f * PanicLevel + 45.0f * HypoxiaRaw + 20.0f * HypercapniaRaw + 35.0f * Exertion + 15.0f * Dehydration;
	if (DiseaseStage >= EDiseaseStage::NeuroToxic)
	{
		// Autonomic instability: erratic spikes.
		TargetHr += FMath::Max(FMath::PerlinNoise1D(NoiseTime * 3.0f), 0.0f) * 60.0f;
	}
	HeartRateBpm = FMath::Clamp(FMath::FInterpTo(HeartRateBpm, TargetHr, RealDt, 0.8f), 40.0f, 200.0f);

	const float TargetCortisol = 350.0f + 650.0f * PanicLevel + 250.0f * HallucinationIntensity;
	CortisolNmolL = FMath::FInterpTo(CortisolNmolL, TargetCortisol, RealDt, 0.05f);
}

// ----------------------------------------------------------------------------
// Disease staging
// ----------------------------------------------------------------------------

void UPlayerBiometricsComponent::UpdateDiseaseStage(float RealDt)
{
	const int32 MetHbStage = StageFromThresholds(MethemoglobinPercentage, 3.0f, 10.0f, 25.0f, 45.0f);
	const int32 RadStage = StageFromThresholds(RadiationInjuryMsv, 50.0f, 150.0f, 400.0f, 750.0f);
	const int32 BioStage = StageFromThresholds(ExtremophileBioLoad, 0.15f, 0.40f, 0.70f, 0.85f);
	const EDiseaseStage NewStage = static_cast<EDiseaseStage>(FMath::Max3(MetHbStage, RadStage, BioStage));

	if (NewStage != DiseaseStage)
	{
		const EDiseaseStage Old = DiseaseStage;
		DiseaseStage = NewStage;
		UE_LOG(LogMarsSurvival, Warning, TEXT("Disease stage %d -> %d"), static_cast<int32>(Old), static_cast<int32>(NewStage));
		if (NewStage == EDiseaseStage::SystemicCollapse)
		{
			Stage4CountdownSeconds = Stage4LethalSeconds;
		}
		OnDiseaseStageChanged.Broadcast(Old, NewStage);
		if (NewStage > Old)
		{
			RaiseCritical(FName(*FString::Printf(TEXT("DiseaseStage%d"), static_cast<int32>(NewStage))));
		}
	}

	if (DiseaseStage == EDiseaseStage::SystemicCollapse)
	{
		Stage4CountdownSeconds -= RealDt;
		if (Stage4CountdownSeconds <= 0.0f)
		{
			Die(TEXT("SystemicCollapse"));
		}
	}
	else
	{
		Stage4CountdownSeconds = 0.0f;
	}
	StabilizeCooldownRemaining = FMath::Max(StabilizeCooldownRemaining - RealDt, 0.0f);
}

// ----------------------------------------------------------------------------
// Hallucination cascade
// ----------------------------------------------------------------------------

void UPlayerBiometricsComponent::UpdateHallucinationCascade(float RealDt)
{
	static const float StageBase[] = { 0.0f, 0.05f, 0.15f, 0.40f, 0.70f };
	const float StageTerm = StageBase[GetStageIndex()];
	const float BioTerm = Saturate01((ExtremophileBioLoad - 0.15f) / 0.6f) * 0.75f;
	const float RadTerm = Saturate01((RadiationInjuryMsv - 100.0f) / 600.0f) * 0.4f;
	const float AcuteTerm = Saturate01(CurrentDoseRateMsvPerHour / 1.0f) * 0.3f;
	const float HypoxiaTerm = Saturate01((90.0f - EffectiveO2SaturationPct) / 40.0f) * 0.7f;
	const float HypercapniaTerm = Saturate01((Co2LevelPpm - 5000.0f) / 35000.0f) * 0.6f;
	const float DehydrationTerm = Saturate01((0.25f - Hydration) / 0.25f) * 0.25f;
	const float FatigueTerm = Saturate01((SleepDeprivation - 0.55f) / 0.45f) * 0.45f;

	const float PanicTarget = FMath::Max3(Saturate01((Co2LevelPpm - 5000.0f) / 45000.0f), HallucinationIntensity * 0.8f, HypoxiaTerm * 0.6f);
	PanicLevel = FMath::FInterpTo(PanicLevel, PanicTarget, RealDt, PanicTarget > PanicLevel ? 0.6f : 0.05f);

	const float Combined = 1.0f
		- (1.0f - StageTerm) * (1.0f - BioTerm) * (1.0f - RadTerm) * (1.0f - AcuteTerm)
		* (1.0f - HypoxiaTerm) * (1.0f - HypercapniaTerm) * (1.0f - DehydrationTerm) * (1.0f - PanicLevel * 0.25f)
		* (1.0f - FatigueTerm);

	NoiseTime += RealDt * 0.35f;
	ClarityRemaining = FMath::Max(ClarityRemaining - RealDt, 0.0f);
	const float ClarityFactor = ClarityRemaining > 0.0f ? 0.3f : 1.0f;
	// Story gate: neuro-symptoms are muted early on (acute hypoxia / CO2 still bite through at 50 %).
	const float StoryGate = FMath::Max(FMath::Lerp(0.25f, 1.0f, NarrativeEscalation), FMath::Max(HypoxiaTerm, HypercapniaTerm) > 0.2f ? 0.5f : 0.0f);
	const float Target = Saturate01((Combined + FMath::PerlinNoise1D(NoiseTime) * 0.15f * Combined) * ClarityFactor * StoryGate);
	HallucinationIntensity = Saturate01(FMath::FInterpTo(HallucinationIntensity, Target, RealDt,
		Target > HallucinationIntensity ? HallucinationRiseSpeed : HallucinationDecaySpeed));

	if (FMath::Abs(HallucinationIntensity - LastBroadcastHallucination) >= 0.01f)
	{
		LastBroadcastHallucination = HallucinationIntensity;
		OnHallucinationChanged.Broadcast(HallucinationIntensity);
	}
}

void UPlayerBiometricsComponent::TryTriggerPhantom(float RealDt)
{
	PhantomCooldownRemaining = FMath::Max(PhantomCooldownRemaining - RealDt, 0.0f);
	const int32 Stage = GetStageIndex();
	if (PhantomCooldownRemaining > 0.0f || Stage < 1 || ClarityRemaining > 0.0f)
	{
		return;
	}

	// Stage 1: almost nothing. Stage 2: auditory pareidolia. Stage 3+: visual phantoms.
	const float StageFactor = FMath::Pow(Stage / 4.0f, 1.5f);
	// No phantoms until the story has escalated (Sol 2+), unless sleep deprivation is severe.
	const float Gate = FMath::Max(NarrativeEscalation, Saturate01((SleepDeprivation - 0.8f) / 0.2f));
	const float Rate = MaxPhantomsPerSecond * StageFactor * (0.4f + HallucinationIntensity) * (Stage == 1 ? 0.15f : 1.0f) * Gate;
	if (FMath::FRand() >= 1.0f - FMath::Exp(-Rate * RealDt))
	{
		return;
	}

	EPhantomType Type = EPhantomType::Footsteps;
	const float Roll = FMath::FRand();
	if (Stage <= 1)
	{
		Type = EPhantomType::HullKnocking;
	}
	else if (Stage == 2)
	{
		// Stage 2: auditory pareidolia, and the first stalking shapes at the end of corridors.
		Type = Roll < 0.3f ? EPhantomType::Footsteps : Roll < 0.5f ? EPhantomType::MetalScraping : Roll < 0.65f ? EPhantomType::HullKnocking
			: Roll < 0.75f ? EPhantomType::Whisper : EPhantomType::ShadowFigure;
	}
	else
	{
		Type = Roll < 0.35f ? EPhantomType::ShadowFigure : Roll < 0.55f ? EPhantomType::PeripheralMovement : Roll < 0.7f ? EPhantomType::Whisper
			: Roll < 0.82f ? EPhantomType::RadioVoice : Roll < 0.92f ? EPhantomType::Footsteps : EPhantomType::MetalScraping;
	}

	PhantomCooldownRemaining = PhantomCooldownSeconds * FMath::FRandRange(0.6f, 1.4f) / FMath::Max(1.0f, Stage - 1.0f);
	OnPhantomTriggered.Broadcast(Type, HallucinationIntensity);
}

// ----------------------------------------------------------------------------
// Lethal thresholds
// ----------------------------------------------------------------------------

void UPlayerBiometricsComponent::CheckCriticalThresholds(float RealDt)
{
	auto Track = [&](bool bCondition, float& Timer, float LethalAfterSeconds, FName WarnReason, FName DeathCause)
	{
		if (bCondition)
		{
			RaiseCritical(WarnReason);
			Timer += RealDt;
			if (Timer >= LethalAfterSeconds)
			{
				Die(DeathCause);
			}
		}
		else
		{
			Timer = FMath::Max(Timer - RealDt * 2.0f, 0.0f);
			RaisedCriticals.Remove(WarnReason);
		}
	};

	Track(EffectiveO2SaturationPct <= 50.0f, SevereHypoxiaSeconds, 25.0f, TEXT("SevereHypoxia"), TEXT("CerebralHypoxia"));
	Track(AmbientPressureKpa <= 6.3f, EbullismSeconds, 8.0f, TEXT("BelowArmstrongLimit"), TEXT("Ebullism"));
	Track(Co2LevelPpm >= 90000.0f, HypercapniaSeconds, 20.0f, TEXT("Hypercapnia"), TEXT("HypercapnicRespiratoryFailure"));
	Track(Hydration <= 0.0f, DehydrationSeconds, 180.0f, TEXT("SevereDehydration"), TEXT("HypovolaemicShock"));
	Track(Nutrition <= 0.0f, StarvationSeconds, 600.0f, TEXT("Starvation"), TEXT("Starvation"));
	if (MethemoglobinPercentage >= 75.0f)
	{
		Die(TEXT("Methemoglobinemia"));
	}
}

void UPlayerBiometricsComponent::RaiseCritical(FName Reason)
{
	if (!RaisedCriticals.Contains(Reason))
	{
		RaisedCriticals.Add(Reason);
		OnVitalsCritical.Broadcast(Reason);
	}
}

void UPlayerBiometricsComponent::Die(FName Cause)
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;
	CauseOfDeath = Cause;
	UE_LOG(LogMarsSurvival, Error, TEXT("Crew member deceased: %s"), *Cause.ToString());
	OnDeath.Broadcast(Cause);
}

// ----------------------------------------------------------------------------
// Inputs / treatments
// ----------------------------------------------------------------------------

void UPlayerBiometricsComponent::SetEnvironmentalConditions(float PressureKpa, float Co2Ppm, float TemperatureC, float O2Fraction, float DustLevel)
{
	AmbientPressureKpa = FMath::Max(PressureKpa, 0.0f);
	AmbientCo2Ppm = FMath::Max(Co2Ppm, 0.0f);
	AmbientTemperatureC = TemperatureC;
	AmbientO2Fraction = Saturate01(O2Fraction);
	AmbientDustLevel = Saturate01(DustLevel);
}

void UPlayerBiometricsComponent::ExposeToPerchlorate(float Milligrams) { PerchlorateBodyBurdenMg += FMath::Max(Milligrams, 0.0f); }
void UPlayerBiometricsComponent::AddRadiationDose(float DoseMsv) { RadiationDoseMsv += FMath::Max(DoseMsv, 0.0f); RadiationInjuryMsv += FMath::Max(DoseMsv, 0.0f); }
void UPlayerBiometricsComponent::ExposeToBioAgent(float LoadIncrease) { ExtremophileBioLoad = Saturate01(ExtremophileBioLoad + LoadIncrease); }

void UPlayerBiometricsComponent::NotifyStartle(float Magnitude)
{
	PanicLevel = Saturate01(PanicLevel + Magnitude * 0.35f);
	HeartRateBpm = FMath::Min(HeartRateBpm + Magnitude * 25.0f, 190.0f);
}

void UPlayerBiometricsComponent::AdministerMethyleneBlue() { MethyleneBlueRemainingHours = MethyleneBlueDurationHours; }
void UPlayerBiometricsComponent::AdministerIodine() { IodineRemainingHours = IodineDurationHours; }

void UPlayerBiometricsComponent::AdministerNeuroAntitoxin()
{
	ExtremophileBioLoad = Saturate01(ExtremophileBioLoad - 0.2f);
	AntitoxinRemainingHours = AntitoxinDurationHours;
}

void UPlayerBiometricsComponent::AdministerAmifostine()
{
	AmifostineRemainingHours = AmifostineDurationHours;
	RadiationInjuryMsv *= 0.9f;
}

void UPlayerBiometricsComponent::AdministerSupplementalO2(float GameHours) { SupplementalO2RemainingHours = FMath::Max(SupplementalO2RemainingHours, GameHours); }
void UPlayerBiometricsComponent::AdministerPerchlorateClearance(float FractionRemoved) { PerchlorateBodyBurdenMg *= 1.0f - Saturate01(FractionRemoved); }
void UPlayerBiometricsComponent::AdministerSedative(float PanicReduction) { PanicLevel = Saturate01(PanicLevel - PanicReduction); }

bool UPlayerBiometricsComponent::EmergencyStabilize()
{
	if (StabilizeCooldownRemaining > 0.0f)
	{
		return false;
	}
	StabilizeCooldownRemaining = StabilizeCooldownSeconds;
	MethemoglobinPercentage = FMath::Max(MethemoglobinPercentage - 15.0f, 4.0f);
	ExtremophileBioLoad = Saturate01(ExtremophileBioLoad - 0.12f);
	RadiationInjuryMsv *= 0.85f;
	PanicLevel = Saturate01(PanicLevel - 0.4f);
	SupplementalO2RemainingHours = FMath::Max(SupplementalO2RemainingHours, 1.0f);
	if (DiseaseStage == EDiseaseStage::SystemicCollapse)
	{
		Stage4CountdownSeconds = Stage4LethalSeconds;
	}
	return true;
}

void UPlayerBiometricsComponent::ApplyAwakeningClarity()
{
	ClarityRemaining = ClaritySeconds;
	HallucinationIntensity *= 0.35f;
	PanicLevel = Saturate01(PanicLevel + 0.15f); // adrenaline spike
	PhantomCooldownRemaining = FMath::Max(PhantomCooldownRemaining, ClaritySeconds);
	OnHallucinationChanged.Broadcast(HallucinationIntensity);
}

void UPlayerBiometricsComponent::Drink(float Amount) { Hydration = Saturate01(Hydration + Amount); }
void UPlayerBiometricsComponent::Eat(float Amount) { Nutrition = Saturate01(Nutrition + Amount); }

// ----------------------------------------------------------------------------
// Diagnostics
// ----------------------------------------------------------------------------

TArray<FString> UPlayerBiometricsComponent::GetIndicatedCountermeasures() const
{
	TArray<FString> Out;
	if (MethemoglobinPercentage >= 10.0f) { Out.Add(TEXT("METHYLENE BLUE 1% IV (bioprinter)")); }
	if (ThyroidFunction < 0.85f || PerchlorateBodyBurdenMg > 120.0f) { Out.Add(TEXT("POTASSIUM IODIDE (oral)")); }
	if (EffectiveO2SaturationPct < 90.0f) { Out.Add(TEXT("SUPPLEMENTAL O2 (canister mask)")); }
	if (Co2LevelPpm > 5000.0f) { Out.Add(TEXT("SCRUB CO2 - BIO-DOME / SCRUBBER CONSOLE")); }
	if (ExtremophileBioLoad >= 0.40f) { Out.Add(TEXT("NEURO-ANTITOXIN (requires Xenolab characterisation)")); }
	if (RadiationInjuryMsv >= 150.0f || CurrentDoseRateMsvPerHour > 0.2f) { Out.Add(TEXT("AMIFOSTINE radioprotectant")); }
	if (Hydration < 0.3f) { Out.Add(TEXT("ORAL REHYDRATION")); }
	if (Nutrition < 0.25f) { Out.Add(TEXT("CALORIC INTAKE")); }
	if (DiseaseStage == EDiseaseStage::SystemicCollapse) { Out.Insert(TEXT("!! EMERGENCY STABILISATION ON SCANNER BED NOW !!"), 0); }
	return Out;
}

FString UPlayerBiometricsComponent::BuildDiagnosisReport(bool bBioAgentCharacterized) const
{
	static const TCHAR* StageTitle[] =
	{
		TEXT("STABLE"),
		TEXT("STAGE 1 - PRODROMAL / MILD HYPOXIA"),
		TEXT("STAGE 2 - ACUTE METHAEMOGLOBINAEMIA + HYPERCAPNIA"),
		TEXT("STAGE 3 - NEURO-TOXIC ENCEPHALOPATHY + RADIATION SICKNESS"),
		TEXT("STAGE 4 - CRITICAL SYSTEMIC COLLAPSE")
	};
	static const TCHAR* StageBasis[] =
	{
		TEXT("No pathway above threshold."),
		TEXT("ClO4- oxidises Hb Fe2+ -> Fe3+; O2 delivery mildly impaired. GCR retinal strikes."),
		TEXT("Arterial blood chocolate-brown. CO2 retention -> respiratory acidosis, panic response."),
		TEXT("Bio-agent crossing BBB via radiation-induced endothelial permeability."),
		TEXT("Severe tissue hypoxia, ARS, neuro-synaptic overload. MULTI-ORGAN FAILURE IMMINENT.")
	};

	const int32 S = GetStageIndex();
	const float MetHb = MethemoglobinPercentage;
	FString Report = FString::Printf(
		TEXT("ARES-IV MED-BAY  //  DIAGNOSTIC SCAN\n")
		TEXT("--------------------------------------------\n")
		TEXT("DIAGNOSIS : %s\n")
		TEXT("BASIS     : %s\n")
		TEXT("--------------------------------------------\n")
		TEXT("MetHb (CO-oximetry) %5.1f %%   blood: %s\n")
		TEXT("Functional SaO2     %5.1f %%\n")
		TEXT("Serum ClO4-         %5.0f mg   thyroid %3.0f %%\n")
		TEXT("Radiation injury    %5.0f mSv  (dose %.0f)\n")
		TEXT("Bio-agent load      %s\n")
		TEXT("Inspired CO2        %6.0f ppm\n")
		TEXT("HR %3.0f bpm   cortisol %3.0f nmol/L\n")
		TEXT("Hydration %3.0f %%   nutrition %3.0f %%\n"),
		StageTitle[S], StageBasis[S],
		MetHb, MetHb >= 15.0f ? TEXT("CHOCOLATE-BROWN") : MetHb >= 8.0f ? TEXT("DARK") : TEXT("RED"),
		EffectiveO2SaturationPct, PerchlorateBodyBurdenMg, ThyroidFunction * 100.0f,
		RadiationInjuryMsv, RadiationDoseMsv,
		bBioAgentCharacterized ? *FString::Printf(TEXT("%5.0f %% (qPCR)"), ExtremophileBioLoad * 100.0f) : TEXT("UNCLASSIFIED - RUN XENOLAB ASSAY"),
		Co2LevelPpm, HeartRateBpm, CortisolNmolL, Hydration * 100.0f, Nutrition * 100.0f);

	if (DiseaseStage == EDiseaseStage::SystemicCollapse)
	{
		Report += FString::Printf(TEXT("!! TIME TO IRREVERSIBLE COLLAPSE: %.0f s\n"), Stage4CountdownSeconds);
	}
	Report += TEXT("--------------------------------------------\nINDICATED:\n");
	const TArray<FString> Indicated = GetIndicatedCountermeasures();
	if (Indicated.Num() == 0)
	{
		Report += TEXT("  continue monitoring\n");
	}
	for (const FString& Line : Indicated)
	{
		Report += TEXT("  > ") + Line + TEXT("\n");
	}
	return Report;
}

FString UPlayerBiometricsComponent::GetTelemetryReport() const
{
	return FString::Printf(
		TEXT("CREW: DIAGNOSTIC_STAGE=%d; DOSE_MSV=%.1f; RAD_INJURY_MSV=%.0f; DOSE_RATE_MSV_H=%.3f; METHB_PCT=%.1f; FUNCTIONAL_SAO2_PCT=%.1f; ")
		TEXT("INSPIRED_CO2_PPM=%.0f; HEART_RATE_BPM=%.0f; CORTISOL_NMOL_L=%.0f; SERUM_CLO4_MG=%.0f; BIOAGENT_LOAD=%.2f; ")
		TEXT("HYDRATION=%.2f; NUTRITION=%.2f; EVA=%s; STAGE4_COUNTDOWN_S=%.0f; VITAL_STATUS=%s"),
		GetStageIndex(), RadiationDoseMsv, RadiationInjuryMsv, CurrentDoseRateMsvPerHour, MethemoglobinPercentage, EffectiveO2SaturationPct,
		Co2LevelPpm, HeartRateBpm, CortisolNmolL, PerchlorateBodyBurdenMg, ExtremophileBioLoad,
		Hydration, Nutrition, bIsOnEVA ? TEXT("YES") : TEXT("NO"), Stage4CountdownSeconds, bIsDead ? TEXT("NO_SIGNS_OF_LIFE") : TEXT("ALIVE"));
}

void UPlayerBiometricsComponent::ApplyPsychosomaticShock(float Amount)
{
	if (bIsDead || Amount <= 0.0f)
	{
		return;
	}
	Stamina = Saturate01(Stamina - Amount * 0.9f);
	PanicLevel = Saturate01(PanicLevel + Amount);
	HeartRateBpm = FMath::Min(HeartRateBpm + 45.0f * Amount, 195.0f);
	PsychosomaticTrauma = Saturate01(PsychosomaticTrauma + Amount * 0.35f);
	NotifyStartle(1.0f);
	if (PsychosomaticTrauma >= 1.0f)
	{
		Die(TEXT("Cardiac arrest - psychosomatic shock (untreated neuro-toxicosis)"));
	}
}

float UPlayerBiometricsComponent::GetUntreatedSeverity() const
{
	const float FromStage = GetStageIndex() / 4.0f;
	const float FromSleep = Saturate01((SleepDeprivation - 0.6f) / 0.4f) * 0.8f;
	const float FromToxin = Saturate01((MethemoglobinPercentage - 8.0f) / 25.0f) * 0.7f;
	const float FromRadiation = Saturate01((RadiationInjuryMsv - 150.0f) / 500.0f) * 0.8f;
	return FMath::Max(FMath::Max(FromStage, FromSleep), FMath::Max(FromToxin, FromRadiation));
}
