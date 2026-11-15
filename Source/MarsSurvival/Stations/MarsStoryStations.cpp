#include "Stations/MarsStoryStations.h"

#include "Core/MarsNarrativeDirector.h"
#include "Items/MarsInventoryComponent.h"
#include "Items/MarsItemLibrary.h"
#include "Player/MarsCharacter.h"
#include "Player/EVASuitComponent.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Player/PlayerFatigueComponent.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

// ----------------------------------------------------------------------------
// Storage locker
// ----------------------------------------------------------------------------

AMarsStorageLocker::AMarsStorageLocker()
{
	StationName = TEXT("STORAGE");
	PropKind = EMarsPropKind::WallLocker;
	ConsoleSize = FVector(50.0f, 90.0f, 200.0f);
	ScreenOffset = FVector(27.0f, 0.0f, 182.0f);
	ScreenTextSize = 3.2f;
	ScreenGlow->SetLightColor(FLinearColor(1.0f, 0.7f, 0.3f));
}

void AMarsStorageLocker::OnConstruction(const FTransform& Transform)
{
	// Cabinet footprint depends on the model.
	if (PropKind == EMarsPropKind::MedCabinet)
	{
		ConsoleSize = FVector(40.0f, 90.0f, 170.0f);
		ScreenOffset = FVector(22.0f, 0.0f, 158.0f);
	}
	else if (PropKind == EMarsPropKind::CargoRack)
	{
		ConsoleSize = FVector(70.0f, 140.0f, 190.0f);
		ScreenOffset = FVector(37.0f, 0.0f, 180.0f);
	}
	Super::OnConstruction(Transform);
}

void AMarsStorageLocker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float Target = bOpened ? 105.0f : 0.0f;
	if (!FMath::IsNearlyEqual(DoorAngle, Target, 0.5f))
	{
		DoorAngle = FMath::FInterpTo(DoorAngle, Target, DeltaTime, 4.0f);
		TArray<USceneComponent*> Comps;
		GetComponents<USceneComponent>(Comps);
		for (USceneComponent* S : Comps)
		{
			if (S->ComponentHasTag(TEXT("DoorL")))
			{
				S->SetRelativeRotation(FRotator(0.0f, -DoorAngle, 0.0f));
			}
			else if (S->ComponentHasTag(TEXT("DoorR")))
			{
				S->SetRelativeRotation(FRotator(0.0f, DoorAngle, 0.0f));
			}
		}
	}
}

void AMarsStorageLocker::Interact_Implementation(AMarsCharacter* C)
{
	if (!C || !C->GetInventory())
	{
		return;
	}
	const bool bFirstOpen = !bOpened;
	bOpened = true;

	TArray<FString> Taken;
	for (int32 i = Contents.Num() - 1; i >= 0; --i)
	{
		FMarsInventorySlot& Slot = Contents[i];
		const int32 Got = C->GetInventory()->AddItem(Slot.ItemType, Slot.Count);
		if (Got > 0)
		{
			Taken.Add(FString::Printf(TEXT("+%d %s"), Got, *UMarsItemLibrary::GetItemName(Slot.ItemType).ToString().ToUpper()));
			Slot.Count -= Got;
		}
		if (Slot.Count <= 0)
		{
			Contents.RemoveAt(i);
		}
	}

	if (Taken.Num() > 0)
	{
		C->ShowNotification(FText::FromString(FString::Join(Taken, TEXT("   "))), 4.0f);
	}
	else if (Contents.Num() > 0)
	{
		C->ShowNotification(FText::FromString(TEXT("TOO HEAVY - CARRY LIMIT REACHED")), 2.5f);
	}
	else if (LogText.IsEmpty())
	{
		C->ShowNotification(FText::FromString(TEXT("EMPTY")), 1.5f);
	}

	if (!LogText.IsEmpty())
	{
		C->ShowPanel(FString::Printf(TEXT("%s\n\n%s"), *LogTitle, *LogText), 30.0f);
		if (UMarsNarrativeDirector* D = UMarsNarrativeDirector::Get(this))
		{
			D->AddLog(LogTitle);
		}
	}

	if (bFirstOpen)
	{
		if (BioExposure > 0.0f)
		{
			if (UPlayerBiometricsComponent* Bio = C->GetBiometrics())
			{
				Bio->ExtremophileBioLoad = FMath::Min(Bio->ExtremophileBioLoad + BioExposure, 1.0f);
			}
		}
		for (const FName& Tag : Tags)
		{
			UMarsNarrativeDirector::Report(this, FName(*FString::Printf(TEXT("Locker_%s"), *Tag.ToString())));
		}
		if (StoryEvent != NAME_None)
		{
			UMarsNarrativeDirector::Report(this, StoryEvent);
		}
	}
	Beep(C, Taken.Num() > 0 ? 1.3f : 0.9f);
}

FText AMarsStorageLocker::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	if (!bOpened)
	{
		return FText::FromString(FString::Printf(TEXT("[E] Open %s"), *StationName.ToLower()));
	}
	if (Contents.Num() > 0)
	{
		return FText::FromString(TEXT("[E] Take remaining items"));
	}
	return FText::FromString(LogText.IsEmpty() ? TEXT("Empty") : TEXT("[E] Re-read data log"));
}

FString AMarsStorageLocker::GetScreenText() const
{
	int32 Items = 0;
	for (const FMarsInventorySlot& S : Contents)
	{
		Items += S.Count;
	}
	FString Line2 = Items > 0 ? FString::Printf(TEXT("%d ITEMS"), Items) : TEXT("EMPTY");
	if (!LogText.IsEmpty())
	{
		Line2 += TEXT(" + LOG");
	}
	return FString::Printf(TEXT("%s\n%s"), *StationName, *Line2);
}

int32 AMarsStorageLocker::GetScreenStatus() const
{
	return !bOpened && (!LogText.IsEmpty() || StoryEvent != NAME_None) ? 1 : 0;
}

// ----------------------------------------------------------------------------
// Bunk
// ----------------------------------------------------------------------------

AMarsBunkStation::AMarsBunkStation()
{
	StationName = TEXT("CREW BUNK");
	PropKind = EMarsPropKind::Bunk;
	ConsoleSize = FVector(210.0f, 100.0f, 180.0f);
	ScreenOffset = FVector(0.0f, 52.0f, 120.0f);
	ScreenTextSize = 3.0f;
	ScreenGlow->SetLightColor(FLinearColor(0.4f, 0.5f, 1.0f));
}

void AMarsBunkStation::Interact_Implementation(AMarsCharacter* C)
{
	UPlayerFatigueComponent* Fatigue = C ? C->GetFatigue() : nullptr;
	if (!Fatigue)
	{
		return;
	}
	if (C->GetSuit() && C->GetSuit()->IsSuitDonned())
	{
		C->ShowNotification(FText::FromString(TEXT("STOW THE EVA SUIT FIRST (EVA PREP RACK)")), 3.0f);
		Beep(C, 0.6f);
		return;
	}
	if (!Fatigue->CanSleep())
	{
		C->ShowNotification(FText::FromString(TEXT("NOT TIRED YET - COME BACK IN THE EVENING")), 3.0f);
		Beep(C, 0.7f);
		return;
	}
	C->BeginSleep();
}

FText AMarsBunkStation::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const UPlayerFatigueComponent* F = C ? C->GetFatigue() : nullptr;
	return FText::FromString(F && F->CanSleep() ? TEXT("[E] Sleep until 07:00") : TEXT("[E] Sleep (not tired yet - use the REST terminal to pass time)"));
}

FString AMarsBunkStation::GetScreenText() const
{
	const UMarsEnvironmentSubsystem* Env = GetWorld() ? GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>() : nullptr;
	if (!Env)
	{
		return StationName;
	}
	const float H = Env->GetLocalSolarTimeHours();
	return FString::Printf(TEXT("SOL %d\n%02d:%02d"), Env->GetSolNumber(), FMath::FloorToInt(H), FMath::FloorToInt(FMath::Frac(H) * 60.0f));
}

// ----------------------------------------------------------------------------
// Power routing (Engineering)
// ----------------------------------------------------------------------------

AMarsPowerRoutingTerminal::AMarsPowerRoutingTerminal()
{
	StationName = TEXT("ENGINEERING - GRID ROUTING");
	PropKind = EMarsPropKind::Terminal;
	ScreenTextSize = 3.0f;
	ScreenGlow->SetLightColor(FLinearColor(1.0f, 0.35f, 0.1f));
}

void AMarsPowerRoutingTerminal::Interact_Implementation(AMarsCharacter* C)
{
	if (UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>())
	{
		Station->CyclePowerRoute();
		if (C)
		{
			C->ShowNotification(FText::FromString(FString::Printf(TEXT("GRID ROUTING: %s"), *Station->GetPowerRouteName())), 3.0f);
		}
		Beep(C, 1.2f);
	}
}

void AMarsPowerRoutingTerminal::InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHold > 0.25)
	{
		HoldTime = 0.0f;
		bHoldConsumed = false;
	}
	LastHold = Now;
	HoldTime += DeltaSeconds;
	if (!bHoldConsumed && HoldTime >= 1.2f)
	{
		bHoldConsumed = true;
		if (UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>())
		{
			Station->SetEmShield(!Station->IsEmShieldUp());
			// The press that started this hold also cycled the routing profile once: step back (5 profiles).
			for (int32 i = 0; i < 4; ++i)
			{
				Station->CyclePowerRoute();
			}
			if (C)
			{
				C->ShowNotification(FText::FromString(Station->IsEmShieldUp() ? TEXT("EM STORM SHIELD UP (+4 kW)") : TEXT("EM STORM SHIELD DOWN")), 3.0f);
			}
			Beep(C, Station->IsEmShieldUp() ? 1.6f : 0.8f);
		}
	}
}

FText AMarsPowerRoutingTerminal::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	const UMarsStationSubsystem* Station = GetWorld() ? GetWorld()->GetSubsystem<UMarsStationSubsystem>() : nullptr;
	return FText::FromString(FString::Printf(TEXT("[E] Reroute RTG power   [Hold E] EM shield %s"), Station && Station->IsEmShieldUp() ? TEXT("DOWN") : TEXT("UP")));
}

FString AMarsPowerRoutingTerminal::GetScreenText() const
{
	const UMarsStationSubsystem* Station = GetWorld() ? GetWorld()->GetSubsystem<UMarsStationSubsystem>() : nullptr;
	if (!Station)
	{
		return StationName;
	}
	return FString::Printf(TEXT("GRID %s\n%.1f/%.1f kW  %3.0f%%\n%s\nEM SHIELD %s"), *Station->GetPowerRouteName(),
		Station->GetPowerSupplyW() / 1000.0f, Station->GetPowerDemandW() / 1000.0f, Station->GetPowerAvailability() * 100.0f,
		*Station->GetPowerRouteSummary(), Station->IsEmShieldUp() ? TEXT("UP") : TEXT("DOWN"));
}

int32 AMarsPowerRoutingTerminal::GetScreenStatus() const
{
	const UMarsStationSubsystem* Station = GetWorld() ? GetWorld()->GetSubsystem<UMarsStationSubsystem>() : nullptr;
	const float P = Station ? Station->GetPowerAvailability() : 1.0f;
	return P < 0.5f ? 2 : P < 0.9f ? 1 : 0;
}

// ----------------------------------------------------------------------------
// Rest / chrono terminal
// ----------------------------------------------------------------------------

namespace
{
	const float RestTargets[] = { 6.0f, 12.0f, 18.0f, 22.0f };
}

AMarsRestTerminal::AMarsRestTerminal()
{
	StationName = TEXT("REST / CHRONO");
	PropKind = EMarsPropKind::Terminal;
	ScreenTextSize = 3.2f;
	ScreenGlow->SetLightColor(FLinearColor(0.4f, 0.6f, 1.0f));
}

void AMarsRestTerminal::Interact_Implementation(AMarsCharacter* C)
{
	TargetIndex = (TargetIndex + 1) % UE_ARRAY_COUNT(RestTargets);
	HoldTime = 0.0f;
	Beep(C, 1.1f);
}

void AMarsRestTerminal::InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds)
{
	if (!C || C->IsSleeping())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHoldTime > 0.25)
	{
		HoldTime = 0.0f;
	}
	LastHoldTime = Now;
	HoldTime += DeltaSeconds;
	if (HoldTime >= 1.2f)
	{
		HoldTime = 0.0f;
		C->BeginRest(RestTargets[TargetIndex], false);
	}
}

FText AMarsRestTerminal::GetInteractionPrompt_Implementation(AMarsCharacter* C) const
{
	return FText::FromString(FString::Printf(TEXT("[E] Change time (%02d:00)   [Hold E] Rest until %02d:00"),
		FMath::RoundToInt(RestTargets[TargetIndex]), FMath::RoundToInt(RestTargets[TargetIndex])));
}

FString AMarsRestTerminal::GetScreenText() const
{
	const UMarsEnvironmentSubsystem* Env = GetWorld() ? GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>() : nullptr;
	const float H = Env ? Env->GetLocalSolarTimeHours() : 0.0f;
	return FString::Printf(TEXT("NOW %02d:%02d\nREST TO %02d:00"), FMath::FloorToInt(H), FMath::FloorToInt(FMath::Frac(H) * 60.0f), FMath::RoundToInt(RestTargets[TargetIndex]));
}
