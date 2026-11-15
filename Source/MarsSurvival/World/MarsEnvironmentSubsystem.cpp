#include "World/MarsEnvironmentSubsystem.h"

#include "MarsSurvival.h"
#include "Core/MarsTime.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"

void UMarsEnvironmentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LocalSolarTimeHours = StartTimeHours;
	RollTonightMinimum();
	UpdateTemperature();
}

void UMarsEnvironmentSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	for (TActorIterator<AExponentialHeightFog> It(&InWorld); It; ++It)
	{
		CachedFog = *It;
		break;
	}
	for (TActorIterator<ADirectionalLight> It(&InWorld); It; ++It)
	{
		CachedSun = *It;
		break;
	}
	ApplyToScene();
}

bool UMarsEnvironmentSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UMarsEnvironmentSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMarsEnvironmentSubsystem, STATGROUP_Tickables);
}

void UMarsEnvironmentSubsystem::Tick(float DeltaTime)
{
	const float GameHours = DeltaTime * TimeScale / MarsTime::SecondsPerHour;
	if (GameHours <= 0.0f)
	{
		return;
	}

	AdvanceClock(GameHours);
	UpdateStorm(GameHours);
	UpdateSolarParticleEvent(GameHours);
	if (ForcedStormIntensity >= 0.0f)
	{
		// Smooth transition into / out of an externally driven storm (no popping fog).
		StormIntensity = FMath::FInterpTo(StormIntensity, FMath::Max(StormIntensity, ForcedStormIntensity), DeltaTime, 0.25f);
	}
	UpdateTemperature();
	ApplyToScene();
}

void UMarsEnvironmentSubsystem::AdvanceClock(float GameHours)
{
	const float Previous = LocalSolarTimeHours;
	LocalSolarTimeHours += GameHours;
	if (LocalSolarTimeHours >= MarsTime::SolLengthHours)
	{
		LocalSolarTimeHours -= MarsTime::SolLengthHours;
		++SolNumber;
	}

	// Decide tonight's minimum each afternoon.
	if (Previous < 15.0f && LocalSolarTimeHours >= 15.0f)
	{
		RollTonightMinimum();
	}
}

void UMarsEnvironmentSubsystem::RollTonightMinimum()
{
	TonightMinTemperatureC = FMath::FRand() < ExtremeNightChance ? ExtremeNightMinTemperatureC : NightMinTemperatureC;
}

void UMarsEnvironmentSubsystem::UpdateTemperature()
{
	// Day: warms from the dawn minimum to a ~14:00 peak. Night (19:00-06:00): falls to TonightMin (-80 C)
	// within ~2 hours of sunset and stays there until dawn.
	const float H = LocalSolarTimeHours;
	const float Night = TonightMinTemperatureC;
	float Temp;
	if (H >= 19.0f || H < 6.0f)
	{
		const float SinceSunset = H >= 19.0f ? H - 19.0f : H + (MarsTime::SolLengthHours - 19.0f);
		const float Dusk = FMath::Lerp(-35.0f, Night, FMath::Clamp(SinceSunset / 2.0f, 0.0f, 1.0f));
		Temp = Dusk;
	}
	else if (H < 14.0f)
	{
		Temp = FMath::Lerp(Night, DayMaxTemperatureC, FMath::SmoothStep(6.0f, 14.0f, H));
	}
	else
	{
		Temp = FMath::Lerp(DayMaxTemperatureC, -35.0f, FMath::SmoothStep(14.0f, 19.0f, H));
	}
	// Dust storms flatten the cycle (up to ~60% reduction in swing).
	const float Mean = 0.5f * (DayMaxTemperatureC + Night);
	AmbientTemperatureC = Mean + (Temp - Mean) * (1.0f - 0.6f * StormIntensity);
}

float UMarsEnvironmentSubsystem::GetSunElevationDegrees() const
{
	// Sunrise ~06:00, sunset ~18:20 (equatorial site).
	const float Phase = 2.0f * PI * (LocalSolarTimeHours - 6.0f) / MarsTime::SolLengthHours;
	return 75.0f * FMath::Sin(Phase);
}

bool UMarsEnvironmentSubsystem::IsNight() const
{
	return LocalSolarTimeHours >= 19.0f || LocalSolarTimeHours < 6.0f;
}

float UMarsEnvironmentSubsystem::GetVisibilityMeters() const
{
	return FMath::Lerp(20000.0f, 40.0f, FMath::Pow(StormIntensity, 0.6f));
}

void UMarsEnvironmentSubsystem::SetStormPhase(EDustStormPhase NewPhase, float PhaseDurationHours)
{
	StormPhase = NewPhase;
	StormPhaseRemainingHours = PhaseDurationHours;
	UE_LOG(LogMarsSurvival, Log, TEXT("Dust storm phase -> %d (%.1f h)"), static_cast<int32>(NewPhase), PhaseDurationHours);
	OnDustStormPhaseChanged.Broadcast(NewPhase);
}

void UMarsEnvironmentSubsystem::UpdateStorm(float GameHours)
{
	switch (StormPhase)
	{
	case EDustStormPhase::Clear:
		StormIntensity = FMath::Max(StormIntensity - GameHours * 0.5f, 0.0f);
		if (FMath::FRand() < 1.0f - FMath::Exp(-StormChancePerHour * GameHours))
		{
			SetStormPhase(EDustStormPhase::Building, FMath::FRandRange(1.5f, 3.0f));
		}
		break;

	case EDustStormPhase::Building:
		StormIntensity = FMath::Min(StormIntensity + GameHours * 0.4f, 1.0f);
		StormPhaseRemainingHours -= GameHours;
		if (StormPhaseRemainingHours <= 0.0f)
		{
			SetStormPhase(EDustStormPhase::Storm, FMath::FRandRange(6.0f, 16.0f));
		}
		break;

	case EDustStormPhase::Storm:
		StormIntensity = FMath::Min(StormIntensity + GameHours * 0.5f, 1.0f);
		StormPhaseRemainingHours -= GameHours;
		if (!bSolarParticleEvent && FMath::FRand() < 1.0f - FMath::Exp(-SpeChancePerStormHour * GameHours))
		{
			ForceSolarParticleEvent(FMath::FRandRange(1.5f, 4.0f));
		}
		if (StormPhaseRemainingHours <= 0.0f)
		{
			SetStormPhase(EDustStormPhase::Subsiding, FMath::FRandRange(2.0f, 4.0f));
		}
		break;

	case EDustStormPhase::Subsiding:
		StormIntensity = FMath::Max(StormIntensity - GameHours * 0.3f, 0.0f);
		StormPhaseRemainingHours -= GameHours;
		if (StormPhaseRemainingHours <= 0.0f || StormIntensity <= 0.0f)
		{
			SetStormPhase(EDustStormPhase::Clear, 0.0f);
		}
		break;
	}
}

void UMarsEnvironmentSubsystem::UpdateSolarParticleEvent(float GameHours)
{
	if (!bSolarParticleEvent)
	{
		return;
	}
	SpeRemainingHours -= GameHours;
	if (SpeRemainingHours <= 0.0f)
	{
		bSolarParticleEvent = false;
		OnSolarParticleEventChanged.Broadcast(false);
	}
}

void UMarsEnvironmentSubsystem::ForceDustStorm(float DurationGameHours)
{
	StormIntensity = FMath::Max(StormIntensity, 0.3f);
	SetStormPhase(EDustStormPhase::Storm, DurationGameHours);
}

void UMarsEnvironmentSubsystem::ForceSolarParticleEvent(float DurationGameHours)
{
	const bool bWasActive = bSolarParticleEvent;
	bSolarParticleEvent = true;
	SpeRemainingHours = DurationGameHours;
	if (!bWasActive)
	{
		UE_LOG(LogMarsSurvival, Warning, TEXT("Solar particle event started (%.1f h)"), DurationGameHours);
		OnSolarParticleEventChanged.Broadcast(true);
	}
}

void UMarsEnvironmentSubsystem::ApplyToScene()
{
	if (!bDriveSceneLighting)
	{
		return;
	}

	if (AExponentialHeightFog* Fog = CachedFog.Get())
	{
		if (UExponentialHeightFogComponent* FogComp = Fog->GetComponent())
		{
			FogComp->SetFogDensity(bOrbitLighting ? 0.0f : FMath::Lerp(ClearFogDensity, StormFogDensity, StormIntensity));
			const FLinearColor ClearTint(0.55f, 0.38f, 0.25f);
			const FLinearColor StormTint(0.42f, 0.22f, 0.10f);
			FogComp->SetFogInscatteringColor(FLinearColor::LerpUsingHSV(ClearTint, StormTint, StormIntensity));
		}
	}

	if (ADirectionalLight* Sun = CachedSun.Get())
	{
		const float Elevation = GetSunElevationDegrees();
		const float DayFactor = FMath::Clamp(FMath::Sin(FMath::DegreesToRadians(FMath::Max(Elevation, 0.0f))), 0.0f, 1.0f);
		if (ULightComponent* Light = Sun->GetLightComponent())
		{
			// Dust optical depth during a global storm can exceed tau ~ 5 (Opportunity, 2018).
			const float Transmission = FMath::Lerp(1.0f, 0.12f, StormIntensity);
			Light->SetIntensity(bOrbitLighting ? NoonSunIntensityLux * 1.3f : FMath::Max(NoonSunIntensityLux * DayFactor * Transmission, 0.02f));
		}
		if (Sun->GetRootComponent() && Sun->GetRootComponent()->Mobility == EComponentMobility::Movable)
		{
			Sun->SetActorRotation(bOrbitLighting ? FRotator(-25.0f, 110.0f, 0.0f) : FRotator(-FMath::Max(Elevation, -5.0f), 110.0f, 0.0f));
		}
	}
}

FString UMarsEnvironmentSubsystem::GetTelemetryReport() const
{
	static const TCHAR* PhaseNames[] = { TEXT("CLEAR"), TEXT("BUILDING"), TEXT("STORM"), TEXT("SUBSIDING") };
	return FString::Printf(
		TEXT("SOL=%d; LMST_H=%.2f; EXT_TEMP_C=%.1f; DUST_STORM=%s; STORM_TAU_INDEX=%.2f; VISIBILITY_M=%.0f; SPE_ACTIVE=%s"),
		SolNumber, LocalSolarTimeHours, AmbientTemperatureC, PhaseNames[static_cast<int32>(StormPhase)],
		StormIntensity, GetVisibilityMeters(), bSolarParticleEvent ? TEXT("YES") : TEXT("NO"));
}

void UMarsEnvironmentSubsystem::SkipHours(float GameHours)
{
	while (GameHours > 0.0f)
	{
		const float Step = FMath::Min(GameHours, 0.5f);
		AdvanceClock(Step);
		GameHours -= Step;
	}
	UpdateTemperature();
	ApplyToScene();
}

void UMarsEnvironmentSubsystem::SetClock(int32 InSol, float InHours)
{
	SolNumber = FMath::Max(InSol, 1);
	LocalSolarTimeHours = FMath::Clamp(InHours, 0.0f, MarsTime::SolLengthHours - 0.01f);
	UpdateTemperature();
	ApplyToScene();
}

int32 UMarsEnvironmentSubsystem::GetWeatherLevel() const
{
	if (ForcedSolarDoseMsvH > 0.0f)
	{
		return 3;
	}
	return StormIntensity > 0.45f ? 2 : StormIntensity > 0.08f ? 1 : 0;
}

FString UMarsEnvironmentSubsystem::GetWeatherName() const
{
	static const TCHAR* Names[] = { TEXT("CLEAR"), TEXT("DUST HAZE"), TEXT("DUST STORM"), TEXT("SOLAR STORM") };
	return Names[GetWeatherLevel()];
}
