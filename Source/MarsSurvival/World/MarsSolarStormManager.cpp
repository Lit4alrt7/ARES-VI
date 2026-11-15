#include "World/MarsSolarStormManager.h"

#include "MarsSurvival.h"
#include "AI/AriaSubsystem.h"
#include "Base/MarsBaseModule.h"
#include "Core/MarsNarrativeDirector.h"
#include "Player/MarsCharacter.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	constexpr float WarningSeconds = 90.0f;
	constexpr float ActiveSeconds = 240.0f;
	constexpr float SubsideSeconds = 30.0f;
	constexpr float SurfaceDoseMsvH = 4.0f;
}

UMarsSolarStormManager* UMarsSolarStormManager::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMarsSolarStormManager>() : nullptr;
}

bool UMarsSolarStormManager::IsShelter(const AMarsBaseModule* Module)
{
	return Module && Module->ModuleType == EMarsModuleType::MedicalBay;
}

float UMarsSolarStormManager::GetIndoorDoseFactor(const AMarsBaseModule* Module) const
{
	if (!Module)
	{
		return 1.0f;
	}
	if (IsShelter(Module))
	{
		return 0.0f;
	}
	const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>();
	return Station && Station->IsEmShieldUp() ? 0.03f : 0.25f;
}

bool UMarsSolarStormManager::AreElectronicsFailing() const
{
	if (Phase != ESolarStormPhase::Active)
	{
		return false;
	}
	const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>();
	return !(Station && Station->IsEmShieldUp());
}

void UMarsSolarStormManager::SetPhase(ESolarStormPhase NewPhase, float Seconds)
{
	Phase = NewPhase;
	PhaseRemaining = Seconds;
	UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
	UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>();
	UAriaSubsystem* Aria = GetWorld()->GetSubsystem<UAriaSubsystem>();
	AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

	switch (NewPhase)
	{
	case ESolarStormPhase::Warning:
		if (Aria)
		{
			Aria->SayScripted(TEXT("A.R.I.A.: Coronal mass ejection inbound. Ninety seconds. Raise the electromagnetic shield at Engineering, or seal yourself in the Med-Bay storm shelter. Now."));
		}
		if (Player)
		{
			Player->ShowNotification(FText::FromString(TEXT("!! SOLAR STORM IN 90 s - HOLD E AT ENGINEERING (EM SHIELD) OR SHELTER IN MED-BAY !!")), 6.0f);
		}
		break;
	case ESolarStormPhase::Active:
		++StormsSoFar;
		if (Env)
		{
			Env->SetForcedStorm(1.0f);
			Env->SetForcedSolarDose(SurfaceDoseMsvH);
		}
		if (Player)
		{
			Player->ShowNotification(FText::FromString(TEXT("SOLAR STORM - ZERO VISIBILITY, UNSHIELDED ELECTRONICS FAILING")), 5.0f);
		}
		DamageTimer = 10.0f;
		break;
	case ESolarStormPhase::Subsiding:
		if (Env)
		{
			Env->SetForcedSolarDose(0.0f);
			Env->SetForcedStorm(0.35f);
		}
		break;
	case ESolarStormPhase::Idle:
		if (Env)
		{
			Env->SetForcedStorm(-1.0f);
			Env->SetForcedSolarDose(0.0f);
		}
		if (Aria)
		{
			Aria->SayScripted(TEXT("A.R.I.A.: Proton flux back to background. Storm has passed. Check the modules for structural stress."));
		}
		break;
	}
	if (Station)
	{
		Station->SetStormElectronicsPenalty(AreElectronicsFailing() ? 0.55f : 1.0f);
	}
}

void UMarsSolarStormManager::TriggerStorm()
{
	if (Phase == ESolarStormPhase::Idle)
	{
		SetPhase(ESolarStormPhase::Warning, WarningSeconds);
	}
}

void UMarsSolarStormManager::DamageTick()
{
	UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>();
	if (!Station || Station->IsEmShieldUp())
	{
		return;
	}
	TArray<AMarsBaseModule*> Modules = Station->GetModules();
	Modules.RemoveAll([](const AMarsBaseModule* M) { return !M || UMarsSolarStormManager::IsShelter(M); });
	if (Modules.Num() == 0)
	{
		return;
	}
	AMarsBaseModule* Hit = Modules[FMath::RandRange(0, Modules.Num() - 1)];
	Hit->HullIntegrity = FMath::Max(Hit->HullIntegrity - 0.06f, 0.2f);
	if (AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->ShowNotification(FText::FromString(FString::Printf(TEXT("HULL STRESS: %s  %.0f %%"), *Hit->GetModuleName(), Hit->HullIntegrity * 100.0f)), 3.0f);
	}
}

void UMarsSolarStormManager::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->IsPaused())
	{
		return;
	}

	// Dev capture hook.
	if (!bDevChecked)
	{
		bDevChecked = true;
		float At = -1.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("SolarStormAt="), At))
		{
			DevTimer = At;
		}
	}
	if (DevTimer > 0.0f)
	{
		DevTimer -= DeltaTime;
		if (DevTimer <= 0.0f)
		{
			TriggerStorm();
		}
	}

	// Schedule: roll once per Sol from Sol 3; chapter III guarantees one by Sol 6.
	if (const UMarsEnvironmentSubsystem* Env = World->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		const int32 Sol = Env->GetSolNumber();
		if (Sol != LastSolChecked)
		{
			LastSolChecked = Sol;
			ScheduledHour = -1.0f;
			const UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this);
			const bool bForce = StormsSoFar == 0 && Sol >= 6 && Story && Story->GetChapter() == EMarsChapter::Uplink;
			if (Sol >= 3 && (bForce || FMath::FRand() < 0.15f))
			{
				ScheduledHour = FMath::FRandRange(9.0f, 20.0f);
			}
		}
		if (ScheduledHour >= 0.0f && Phase == ESolarStormPhase::Idle && Env->GetLocalSolarTimeHours() >= ScheduledHour)
		{
			ScheduledHour = -1.0f;
			TriggerStorm();
		}
	}

	if (Phase == ESolarStormPhase::Idle)
	{
		return;
	}
	PhaseRemaining -= DeltaTime;
	if (Phase == ESolarStormPhase::Active)
	{
		DamageTimer -= DeltaTime;
		if (DamageTimer <= 0.0f)
		{
			DamageTimer = 15.0f;
			DamageTick();
		}
		if (UMarsStationSubsystem* Station = World->GetSubsystem<UMarsStationSubsystem>())
		{
			Station->SetStormElectronicsPenalty(AreElectronicsFailing() ? 0.55f : 1.0f);
		}
	}
	if (PhaseRemaining <= 0.0f)
	{
		switch (Phase)
		{
		case ESolarStormPhase::Warning:   SetPhase(ESolarStormPhase::Active, ActiveSeconds); break;
		case ESolarStormPhase::Active:    SetPhase(ESolarStormPhase::Subsiding, SubsideSeconds); break;
		case ESolarStormPhase::Subsiding: SetPhase(ESolarStormPhase::Idle, 0.0f); break;
		default: break;
		}
	}
}
