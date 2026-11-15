#include "Player/EVASuitComponent.h"

#include "MarsSurvival.h"
#include "Core/MarsTime.h"

UEVASuitComponent::UEVASuitComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEVASuitComponent::DonSuit()
{
	bSuitDonned = true;
	HelmetPressureKpa = O2RemainingLiters > 0.0f ? HelmetSetpointKpa : 0.0f;
	HelmetCo2Ppm = 600.0f;
}

void UEVASuitComponent::DoffSuit()
{
	bSuitDonned = false;
}

void UEVASuitComponent::UpdateSuit(float DeltaTime, float HeartRateBpm, float ExternalTemperatureC, float StormIntensity, bool bOutside)
{
	if (!bSuitDonned || DeltaTime <= 0.0f)
	{
		return;
	}
	const float GameMinutes = DeltaTime * MarsTime::GetTimeScale(this) / 60.0f;
	const float GameHours = GameMinutes / 60.0f;

	// Metabolic O2: ~0.25 L/min at rest, rising with heart rate; plus leak through damaged seals.
	CurrentO2ConsumptionLpm = 0.25f + 0.02f * FMath::Max(HeartRateBpm - 70.0f, 0.0f) + (1.0f - SuitIntegrity) * 3.0f;
	if (bOutside && ExternalTemperatureC < -60.0f)
	{
		CurrentO2ConsumptionLpm *= 2.0f; // shivering + thermal regulation at -80 C
	}
	O2RemainingLiters = FMath::Max(O2RemainingLiters - CurrentO2ConsumptionLpm * GameMinutes, 0.0f);

	// Helmet pressure regulated from the tank; decays toward outside once empty.
	if (O2RemainingLiters > 0.0f)
	{
		HelmetPressureKpa = FMath::FInterpTo(HelmetPressureKpa, HelmetSetpointKpa, DeltaTime, 2.0f);
	}
	else
	{
		HelmetPressureKpa = FMath::Max(HelmetPressureKpa * FMath::Exp(-GameMinutes / 2.0f), bOutside ? 0.6f : HelmetPressureKpa);
	}

	// CO2: LiOH absorbs (2LiOH + CO2 -> Li2CO3 + H2O) until exhausted.
	LiOHRemainingHours = FMath::Max(LiOHRemainingHours - GameHours * (HeartRateBpm / 80.0f), 0.0f);
	const float Co2Target = LiOHRemainingHours > 0.0f ? 600.0f : 60000.0f;
	HelmetCo2Ppm = FMath::FInterpTo(HelmetCo2Ppm, Co2Target, DeltaTime, LiOHRemainingHours > 0.0f ? 0.5f : 0.02f);

	// Battery: avionics + heater. Outside, heater load grows with cold and doubles in the -80 C night.
	// Inside a pressurised module the suit is on station power: it recharges (docking port).
	if (bOutside)
	{
		const float NightFactor = ExternalTemperatureC < -60.0f ? 2.0f : 1.0f;
		const float HeaterW = (25.0f + FMath::Max(10.0f - ExternalTemperatureC, 0.0f) * 3.0f) * NightFactor;
		BatteryWh = FMath::Max(BatteryWh - HeaterW * GameHours, 0.0f);
	}
	else
	{
		BatteryWh = FMath::Min(BatteryWh + 600.0f * GameHours, BatteryCapacityWh);
	}

	if (bOutside)
	{
		DustLoad = FMath::Clamp(DustLoad + (0.3f + 1.2f * StormIntensity) * GameHours, 0.0f, 1.0f);
	}

	const float O2Frac = GetO2Fraction01();
	if (O2Frac < 0.25f) { Warn(TEXT("O2Low")); }
	if (O2Frac < 0.10f) { Warn(TEXT("O2Critical")); }
	if (O2RemainingLiters <= 0.0f) { Warn(TEXT("O2Depleted")); }
	if (LiOHRemainingHours <= 0.0f) { Warn(TEXT("ScrubberExhausted")); }
	if (BatteryWh <= 0.0f) { Warn(TEXT("BatteryDepleted")); }
}

void UEVASuitComponent::Warn(FName Warning)
{
	if (!ActiveWarnings.Contains(Warning))
	{
		ActiveWarnings.Add(Warning);
		OnSuitWarning.Broadcast(Warning);
	}
}

void UEVASuitComponent::GetBreathingEnvironment(float& OutPressureKpa, float& OutCo2Ppm, float& OutO2Fraction) const
{
	OutPressureKpa = HelmetPressureKpa;
	OutCo2Ppm = HelmetCo2Ppm;
	OutO2Fraction = 1.0f; // pure O2 helmet atmosphere
}

float UEVASuitComponent::RefillO2(float Liters)
{
	const float Added = FMath::Clamp(Liters, 0.0f, O2TankCapacity - O2RemainingLiters);
	O2RemainingLiters += Added;
	if (Added > 0.0f)
	{
		ActiveWarnings.Remove(TEXT("O2Low"));
		ActiveWarnings.Remove(TEXT("O2Critical"));
		ActiveWarnings.Remove(TEXT("O2Depleted"));
	}
	return Added;
}

void UEVASuitComponent::ServiceConsumables()
{
	LiOHRemainingHours = LiOHCapacityHours;
	BatteryWh = BatteryCapacityWh;
	SuitIntegrity = 1.0f;
	ActiveWarnings.Empty();
}

float UEVASuitComponent::GetO2MinutesRemainingReal() const
{
	const float GameMinutesLeft = O2RemainingLiters / FMath::Max(CurrentO2ConsumptionLpm, 0.01f);
	return GameMinutesLeft * 60.0f / FMath::Max(MarsTime::GetTimeScale(this), 1.0f) / 60.0f;
}

FString UEVASuitComponent::GetTelemetryReport() const
{
	return FString::Printf(
		TEXT("SUIT: DONNED=%s; O2_L=%.0f/%.0f; O2_LPM=%.2f; HELMET_P_KPA=%.1f; HELMET_CO2_PPM=%.0f; LIOH_H=%.1f; BATTERY_WH=%.0f; INTEGRITY=%.2f; DUST_LOAD=%.2f"),
		bSuitDonned ? TEXT("YES") : TEXT("NO"), O2RemainingLiters, O2TankCapacity, CurrentO2ConsumptionLpm,
		HelmetPressureKpa, HelmetCo2Ppm, LiOHRemainingHours, BatteryWh, SuitIntegrity, DustLoad);
}
