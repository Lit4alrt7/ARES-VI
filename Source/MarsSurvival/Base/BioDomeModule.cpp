#include "Base/BioDomeModule.h"

#include "Core/MarsTime.h"
#include "World/MarsStationSubsystem.h"
#include "Components/PointLightComponent.h"

ABioDomeModule::ABioDomeModule()
{
	ModuleType = EMarsModuleType::AeroponicBioDome;
	RoomSize = FVector(1400.0f, 900.0f, 450.0f);
	BuildShell();

	GrowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GrowLight"));
	GrowLight->SetupAttachment(SceneRoot);
	GrowLight->SetRelativeLocation(FVector(0, 0, 380.0f));
	GrowLight->SetIntensityUnits(ELightUnits::Lumens);
	GrowLight->SetIntensity(1500.0f);
	GrowLight->SetAttenuationRadius(1200.0f);
	// Red + blue LED mix used for photosynthetic organisms reads as magenta.
	GrowLight->SetLightColor(FLinearColor(0.85f, 0.25f, 0.95f));
	GrowLight->SetCastShadows(false);
}

float ABioDomeModule::GetPowerDemandW() const
{
	return Super::GetPowerDemandW() + (bGrowLightsOnline ? GrowLightPowerW : 0.0f);
}

void ABioDomeModule::TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds)
{
	const float GameHours = GameDeltaSeconds / MarsTime::SecondsPerHour;
	const UMarsStationSubsystem* Station = GetStation();
	const float Power = Station ? Station->GetModulePowerAvailability(this) : 1.0f;
	const float Light = bGrowLightsOnline ? Power : 0.0f;

	// Culture health: thermal tolerance, pressure, perchlorate toxicity, light starvation.
	float HealthDelta = 0.02f; // slow regrowth under good conditions
	if (CurrentTemperatureC < 15.0f || CurrentTemperatureC > 35.0f) { HealthDelta -= 0.08f; }
	if (CurrentTemperatureC < 0.0f)                                  { HealthDelta -= 0.25f; } // freezing lyses cells
	if (CurrentPressureKpa < 40.0f)                                  { HealthDelta -= 0.35f; }
	if (DustContamination > 0.25f)                                   { HealthDelta -= 0.15f * DustContamination; }
	if (Light < 0.2f)                                                { HealthDelta -= 0.03f; }
	ChlorellaCultureHealth = FMath::Clamp(ChlorellaCultureHealth + HealthDelta * GameHours, 0.0f, 1.0f);

	PhotosyntheticOutput = ChlorellaCultureHealth * Light;

	if (Station && PhotosyntheticOutput > 0.0f)
	{
		for (AMarsBaseModule* Module : Station->GetModules())
		{
			Module->RemoveCo2Ppm(StationCo2RemovalPpmPerHour * PhotosyntheticOutput * GameHours);
			Module->AddO2Fraction(StationO2AdditionPerHour * PhotosyntheticOutput * GameHours);
		}
	}

	if (GrowLight)
	{
		GrowLight->SetIntensity(1500.0f * Light);
	}
}

FString ABioDomeModule::GetModuleSpecificTelemetry() const
{
	return FString::Printf(TEXT("CHLORELLA_HEALTH=%.2f; PHOTOSYNTHETIC_OUTPUT=%.2f; GROW_LIGHTS=%s"),
		ChlorellaCultureHealth, PhotosyntheticOutput, bGrowLightsOnline ? TEXT("ON") : TEXT("OFF"));
}
