#include "Base/CommRelayModule.h"

#include "MarsSurvival.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"

ACommRelayModule::ACommRelayModule()
{
	ModuleType = EMarsModuleType::StorageCommRelay;
	RoomSize = FVector(1000.0f, 700.0f, 340.0f);
	// Dead-end module at the far (-X) end of the station.
	bBackDoorway = false;
	BuildShell();
}

float ACommRelayModule::GetPowerSupplyW() const
{
	// Thermoelectric conversion efficiency drops when the power conditioning bay freezes.
	const float ColdPenalty = CurrentTemperatureC < -20.0f ? 0.7f : 1.0f;
	return RtgArrayOutputW * ColdPenalty;
}

float ACommRelayModule::GetPowerDemandW() const
{
	return Super::GetPowerDemandW() + (bCalibratingThisFrame || CalibrationProgress > 0.0f ? TransceiverPowerW : 0.0f);
}

void ACommRelayModule::TakeBallisticDamageAtLocation(float DamageAmount, FVector ImpactPoint)
{
	Super::TakeBallisticDamageAtLocation(DamageAmount, ImpactPoint);
	TransceiverIntegrity = FMath::Clamp(TransceiverIntegrity - 0.08f, 0.0f, 1.0f);
	CalibrationProgress = FMath::Max(CalibrationProgress - 0.25f, 0.0f);
}

int32 ACommRelayModule::InstallTransistors(int32 Count)
{
	const int32 Needed = FMath::Max(RequiredTransistors - InstalledTransistors, 0);
	const int32 Installed = FMath::Clamp(Count, 0, Needed);
	InstalledTransistors += Installed;
	return Installed;
}

void ACommRelayModule::RepairTransceiver(float Amount)
{
	TransceiverIntegrity = FMath::Clamp(TransceiverIntegrity + Amount, 0.0f, 1.0f);
}

bool ACommRelayModule::IsReadyToCalibrate() const
{
	return GetBlockingReason().IsEmpty();
}

FString ACommRelayModule::GetBlockingReason() const
{
	if (!AreTransistorsInstalled())
	{
		return FString::Printf(TEXT("QUANTUM TRANSISTORS %d/%d"), InstalledTransistors, RequiredTransistors);
	}
	if (TransceiverIntegrity < 0.6f)
	{
		return FString::Printf(TEXT("TRANSCEIVER DAMAGED (%.0f%%) - SPARE PARTS REQUIRED"), TransceiverIntegrity * 100.0f);
	}
	const UMarsStationSubsystem* Station = GetStation();
	if (Station && Station->GetModulePowerAvailability(this) < 0.5f)
	{
		return TEXT("INSUFFICIENT GRID POWER");
	}
	return FString();
}

void ACommRelayModule::Calibrate(float RealDeltaTime)
{
	if (!IsReadyToCalibrate())
	{
		return;
	}
	bCalibratingThisFrame = true;
	CalibrationIdleTime = 0.0f;

	float Rate = 1.0f / FMath::Max(CalibrationSeconds, 1.0f);
	if (const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		// Optical depth from dust: link margin falls sharply, fall back to slow radio-assisted acquisition.
		Rate *= FMath::Lerp(1.0f, 0.15f, Env->GetStormIntensity());
	}
	if (const UMarsStationSubsystem* Station = GetStation())
	{
		Rate *= Station->GetModulePowerAvailability(this);
	}
	CalibrationProgress = FMath::Min(CalibrationProgress + Rate * RealDeltaTime, 1.0f);
}

bool ACommRelayModule::TransmitToEarth()
{
	if (!IsReadyToTransmit())
	{
		return false;
	}
	if (UMarsStationSubsystem* Station = GetStation())
	{
		Station->NotifyEarthContact();
	}
	return true;
}

void ACommRelayModule::TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds)
{
	// Alignment drifts when nobody is holding the terminal (thermal flexure of the optical bench).
	if (!bCalibratingThisFrame && CalibrationProgress > 0.0f && CalibrationProgress < 1.0f)
	{
		CalibrationIdleTime += RealDeltaTime;
		if (CalibrationIdleTime > 2.0f)
		{
			CalibrationProgress = FMath::Max(CalibrationProgress - 0.01f * RealDeltaTime, 0.0f);
		}
	}
	bCalibratingThisFrame = false;
}

FString ACommRelayModule::GetModuleSpecificTelemetry() const
{
	return FString::Printf(TEXT("RTG_OUTPUT_W=%.0f; QUANTUM_TRANSISTORS=%d/%d; TRANSCEIVER_INTEGRITY=%.2f; LINK_CALIBRATION=%.2f"),
		GetPowerSupplyW(), InstalledTransistors, RequiredTransistors, TransceiverIntegrity, CalibrationProgress);
}
