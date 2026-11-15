#include "World/MarsBaseManager.h"

#include "Base/BioDomeModule.h"
#include "Base/MarsBaseModule.h"
#include "Core/MarsTime.h"
#include "Items/MarsItemTypes.h"
#include "Stations/MarsStations.h"
#include "Stations/MarsStoryStations.h"
#include "World/MarsStationSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"

UMarsBaseManager* UMarsBaseManager::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMarsBaseManager>() : nullptr;
}

void UMarsBaseManager::ApplyBaseDamage(float Amount)
{
	const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>();
	if (!Station)
	{
		return;
	}
	for (AMarsBaseModule* M : Station->GetModules())
	{
		if (M)
		{
			M->HullIntegrity = FMath::Clamp(M->HullIntegrity - Amount * FMath::FRandRange(0.5f, 1.5f), 0.15f, 1.0f);
		}
	}
}

void UMarsBaseManager::Refresh()
{
	UWorld* World = GetWorld();
	const UMarsStationSubsystem* Station = World->GetSubsystem<UMarsStationSubsystem>();
	FMarsBaseStatus S;
	if (Station)
	{
		S.PowerOutputW = Station->GetPowerSupplyW();
		S.PowerDemandW = Station->GetPowerDemandW();
		S.PowerAvailability = Station->GetPowerAvailability();
		float O2 = 0.0f, Integrity = 0.0f;
		int32 N = 0;
		for (const AMarsBaseModule* M : Station->GetModules())
		{
			if (!M)
			{
				continue;
			}
			O2 += M->CurrentO2Fraction * 100.0f;
			Integrity += M->HullIntegrity;
			++N;
			if (M->HullIntegrity < S.WorstIntegrity)
			{
				S.WorstIntegrity = M->HullIntegrity;
				S.WorstModule = M->GetModuleName();
			}
			if (M->ModuleType == EMarsModuleType::MedicalBay && M->HullIntegrity < 0.5f)
			{
				S.bCraftingLocked = true;
				S.CraftingLockReason = TEXT("MED-BAY STRUCTURE < 50 %");
			}
		}
		if (N > 0)
		{
			S.AvgO2Percent = O2 / N;
			S.AvgIntegrity = Integrity / N;
		}
	}
	if (S.PowerAvailability < 0.4f && !S.bCraftingLocked)
	{
		S.bCraftingLocked = true;
		S.CraftingLockReason = TEXT("GRID POWER < 40 %");
	}

	for (TActorIterator<AWaterDispenserStation> It(World); It; ++It)
	{
		S.WaterLiters += It->ReserveLiters;
	}
	for (TActorIterator<AMarsStorageLocker> It(World); It; ++It)
	{
		for (const FMarsInventorySlot& Slot : It->Contents)
		{
			switch (Slot.ItemType)
			{
			case EMarsItemType::FoodRation:
			case EMarsItemType::ChlorellaPaste: S.FoodRations += Slot.Count; break;
			case EMarsItemType::O2Canister:     S.O2Canisters += Slot.Count; break;
			case EMarsItemType::SpareParts:     S.SpareParts += Slot.Count; break;
			default: break;
			}
		}
	}
	Status = S;
}

void UMarsBaseManager::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->IsPaused())
	{
		return;
	}
	RefreshTimer -= DeltaTime;
	if (RefreshTimer <= 0.0f)
	{
		RefreshTimer = 1.0f;
		Refresh();
	}

	// Consequence: structurally compromised modules bleed oxygen through micro-fractures.
	const float GameHours = MarsTime::ToGameHours(this, DeltaTime);
	if (const UMarsStationSubsystem* Station = World->GetSubsystem<UMarsStationSubsystem>())
	{
		for (AMarsBaseModule* M : Station->GetModules())
		{
			if (M && M->HullIntegrity < 0.5f)
			{
				const float Bleed = (M->HullIntegrity < 0.3f ? 0.02f : 0.006f) * GameHours;
				M->CurrentO2Fraction = FMath::Max(M->CurrentO2Fraction - Bleed, 0.12f);
			}
			if (ABioDomeModule* Dome = Cast<ABioDomeModule>(M))
			{
				if (Dome->HullIntegrity < 0.4f)
				{
					Dome->ChlorellaCultureHealth = FMath::Max(Dome->ChlorellaCultureHealth - 0.03f * GameHours, 0.0f);
				}
			}
		}
	}
}
