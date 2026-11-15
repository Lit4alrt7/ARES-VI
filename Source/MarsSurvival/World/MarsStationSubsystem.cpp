#include "World/MarsStationSubsystem.h"

#include "MarsSurvival.h"
#include "Base/MarsBaseModule.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

bool UMarsStationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UMarsStationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMarsStationSubsystem, STATGROUP_Tickables);
}

void UMarsStationSubsystem::Tick(float DeltaTime)
{
	Modules.RemoveAll([](const TWeakObjectPtr<AMarsBaseModule>& M) { return !M.IsValid(); });
	UpdatePowerGrid();

	// Keep the discharge log bounded (5 minutes of history).
	if (const UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		DischargeTimestamps.RemoveAll([Now](double T) { return Now - T > 300.0; });
	}
}

// ----------------------------------------------------------------------------
// Registry
// ----------------------------------------------------------------------------

void UMarsStationSubsystem::RegisterModule(AMarsBaseModule* Module)
{
	if (Module)
	{
		Modules.AddUnique(Module);
	}
}

void UMarsStationSubsystem::UnregisterModule(AMarsBaseModule* Module)
{
	Modules.Remove(Module);
}

TArray<AMarsBaseModule*> UMarsStationSubsystem::GetModules() const
{
	TArray<AMarsBaseModule*> Result;
	for (const TWeakObjectPtr<AMarsBaseModule>& M : Modules)
	{
		if (AMarsBaseModule* Module = M.Get())
		{
			Result.Add(Module);
		}
	}
	return Result;
}

AMarsBaseModule* UMarsStationSubsystem::FindModuleByType(EMarsModuleType Type) const
{
	for (const TWeakObjectPtr<AMarsBaseModule>& M : Modules)
	{
		if (M.IsValid() && M->ModuleType == Type)
		{
			return M.Get();
		}
	}
	return nullptr;
}

AMarsBaseModule* UMarsStationSubsystem::FindModuleContainingPoint(const FVector& WorldLocation) const
{
	for (const TWeakObjectPtr<AMarsBaseModule>& M : Modules)
	{
		if (M.IsValid() && M->IsPointInside(WorldLocation))
		{
			return M.Get();
		}
	}
	return nullptr;
}

// ----------------------------------------------------------------------------
// Power
// ----------------------------------------------------------------------------

void UMarsStationSubsystem::UpdatePowerGrid()
{
	float Supply = 0.0f;
	float Demand = 0.0f;
	for (const TWeakObjectPtr<AMarsBaseModule>& M : Modules)
	{
		if (const AMarsBaseModule* Module = M.Get())
		{
			Supply += Module->GetPowerSupplyW();
			Demand += Module->GetPowerDemandW();
		}
	}
	if (Supply <= 0.0f)
	{
		Supply = FallbackPowerSupplyW;
	}

	Supply *= StormPenalty;
	if (bEmShieldUp)
	{
		Demand += 4000.0f;
	}
	PowerSupplyW = Supply;
	PowerDemandW = Demand;
	PowerAvailability = Demand > KINDA_SMALL_NUMBER ? FMath::Clamp(Supply / Demand, 0.0f, 1.0f) : 1.0f;
}

// ----------------------------------------------------------------------------
// Events
// ----------------------------------------------------------------------------

void UMarsStationSubsystem::ReportWeaponDischarge(AActor* Shooter, const FVector& MuzzleLocation, bool bHitStructure)
{
	++TotalDischarges;
	if (const UWorld* World = GetWorld())
	{
		DischargeTimestamps.Add(World->GetTimeSeconds());
	}
	UE_LOG(LogMarsSurvival, Verbose, TEXT("Weapon discharge at %s (structure hit: %d)"), *MuzzleLocation.ToString(), bHitStructure);
}

void UMarsStationSubsystem::ReportStructuralImpact(AMarsBaseModule* Module, const FVector& Location)
{
	++TotalStructuralImpacts;
	OnStructuralImpact.Broadcast(Module, Location);
}

void UMarsStationSubsystem::RaiseAlert(FName AlertType, AMarsBaseModule* Module)
{
	UE_LOG(LogMarsSurvival, Warning, TEXT("STATION ALERT: %s (%s)"), *AlertType.ToString(), Module ? *Module->GetModuleName() : TEXT("station"));
	OnStationAlert.Broadcast(AlertType, Module);
}

int32 UMarsStationSubsystem::GetDischargesInLastSeconds(float WindowSeconds) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	const double Now = World->GetTimeSeconds();
	int32 Count = 0;
	for (const double T : DischargeTimestamps)
	{
		if (Now - T <= WindowSeconds)
		{
			++Count;
		}
	}
	return Count;
}

void UMarsStationSubsystem::NotifyEarthContact()
{
	if (!bEarthContact)
	{
		bEarthContact = true;
		UE_LOG(LogMarsSurvival, Log, TEXT("QUANTUM LINK ESTABLISHED WITH EARTH"));
		OnEarthContactEstablished.Broadcast();
	}
}

FString UMarsStationSubsystem::BuildStationTelemetry() const
{
	FString Report = FString::Printf(
		TEXT("GRID: SUPPLY_W=%.0f; DEMAND_W=%.0f; AVAILABILITY=%.2f\n")
		TEXT("KINETIC_LOG: WEAPON_DISCHARGES_60S=%d; WEAPON_DISCHARGES_TOTAL=%d; HULL_PENETRATIONS_TOTAL=%d; SAMPLES_ANALYSED=%d; EARTH_LINK=%s\n"),
		PowerSupplyW, PowerDemandW, PowerAvailability,
		GetDischargesInLastSeconds(60.0f), TotalDischarges, TotalStructuralImpacts, SamplesAnalysed,
		bEarthContact ? TEXT("ESTABLISHED") : TEXT("DOWN"));

	for (const TWeakObjectPtr<AMarsBaseModule>& M : Modules)
	{
		if (const AMarsBaseModule* Module = M.Get())
		{
			// Ring corridors only report when something is wrong (keeps telemetry compact).
			if (Module->ModuleType == EMarsModuleType::RingCorridor && Module->PunctureCount == 0 && !Module->IsBreached() && Module->SealIntegrity > 0.8f)
			{
				continue;
			}
			Report += Module->GetTelemetryReport();
			Report += TEXT("\n");
		}
	}
	return Report;
}

// ----------------------------------------------------------------------------
// Power routing (Engineering terminal)
// ----------------------------------------------------------------------------

namespace
{
	const TCHAR* RouteNames[] = { TEXT("BALANCED"), TEXT("LIFE SUPPORT"), TEXT("MED-BAY SYNTH"), TEXT("BIO-DOME HEATERS"), TEXT("UPLINK") };
}

void UMarsStationSubsystem::CyclePowerRoute()
{
	PowerRoute = (PowerRoute + 1) % 5;
}

FString UMarsStationSubsystem::GetPowerRouteName() const
{
	return RouteNames[FMath::Clamp(PowerRoute, 0, 4)];
}

FString UMarsStationSubsystem::GetPowerRouteSummary() const
{
	switch (PowerRoute)
	{
	case 1:  return TEXT("AIRLOCK/EVA/QTRS +");
	case 2:  return TEXT("MED-BAY +  LIGHTS -");
	case 3:  return TEXT("BIO-DOME +  LIGHTS -");
	case 4:  return TEXT("COMM-RELAY +  LIGHTS -");
	default: return TEXT("EVEN SPLIT");
	}
}

float UMarsStationSubsystem::GetModulePowerAvailability(const AMarsBaseModule* Module) const
{
	if (!Module || PowerRoute == 0)
	{
		return PowerAvailability;
	}
	bool bPriority = false;
	switch (PowerRoute)
	{
	case 1: bPriority = Module->ModuleType == EMarsModuleType::AirlockDecon || Module->ModuleType == EMarsModuleType::EvaPrep || Module->ModuleType == EMarsModuleType::CrewQuarters; break;
	case 2: bPriority = Module->ModuleType == EMarsModuleType::MedicalBay; break;
	case 3: bPriority = Module->ModuleType == EMarsModuleType::AeroponicBioDome; break;
	case 4: bPriority = Module->ModuleType == EMarsModuleType::StorageCommRelay; break;
	default: break;
	}
	// Priority sectors are fed first; the rest share what is left.
	return FMath::Clamp(PowerAvailability * (bPriority ? 1.6f : 0.75f), 0.0f, 1.0f);
}
