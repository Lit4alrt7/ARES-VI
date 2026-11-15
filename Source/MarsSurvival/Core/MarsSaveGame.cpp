#include "Core/MarsSaveGame.h"

#include "MarsSurvival.h"
#include "Base/CommRelayModule.h"
#include "Base/MarsBaseModule.h"
#include "Core/MarsGameMode.h"
#include "Core/MarsNarrativeDirector.h"
#include "Core/MarsMissionDirector.h"
#include "Vehicles/MarsReconShuttle.h"
#include "Player/PlayerFatigueComponent.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "Items/MarsInventoryComponent.h"
#include "Items/MarsPickup.h"
#include "Player/EVASuitComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Stations/MarsStations.h"
#include "World/MarsStationSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

const FString UMarsSaveGame::CheckpointSlot = TEXT("MarsCheckpoint");
const FString UMarsSaveGame::ManualSlot = TEXT("MarsManual");

namespace
{
	bool IsSaveable(const FProperty* P)
	{
		if (!P || P->ArrayDim != 1 || P->HasAnyPropertyFlags(CPF_Deprecated))
		{
			return false;
		}
		if (P->IsA<FObjectPropertyBase>() || P->IsA<FInterfaceProperty>() || P->IsA<FDelegateProperty>() || P->IsA<FMulticastDelegateProperty>())
		{
			return false;
		}
		if (const FArrayProperty* A = CastField<FArrayProperty>(P))
		{
			return IsSaveable(A->Inner);
		}
		if (const FSetProperty* S = CastField<FSetProperty>(P))
		{
			return IsSaveable(S->ElementProp);
		}
		if (const FMapProperty* M = CastField<FMapProperty>(P))
		{
			return IsSaveable(M->KeyProp) && IsSaveable(M->ValueProp);
		}
		return true;
	}

	void Capture(UObject* Obj, const UClass* StopAt, FMarsSavedObject& Out)
	{
		for (const UClass* C = Obj->GetClass(); C && C != StopAt; C = C->GetSuperClass())
		{
			for (TFieldIterator<FProperty> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				FProperty* P = *It;
				if (!IsSaveable(P))
				{
					continue;
				}
				FString Value;
				P->ExportTextItem_Direct(Value, P->ContainerPtrToValuePtr<void>(Obj), nullptr, Obj, PPF_None);
				Out.Props.Add(P->GetName(), Value);
			}
		}
	}

	void Restore(UObject* Obj, const UClass* StopAt, const FMarsSavedObject& In)
	{
		for (const UClass* C = Obj->GetClass(); C && C != StopAt; C = C->GetSuperClass())
		{
			for (TFieldIterator<FProperty> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				FProperty* P = *It;
				const FString* Value = In.Props.Find(P->GetName());
				if (!Value || !IsSaveable(P))
				{
					continue;
				}
				P->ImportText_Direct(**Value, P->ContainerPtrToValuePtr<void>(Obj), Obj, PPF_None);
			}
		}
	}
}

bool UMarsSaveGame::HasAnySave()
{
	return UGameplayStatics::DoesSaveGameExist(CheckpointSlot, 0) || UGameplayStatics::DoesSaveGameExist(ManualSlot, 0);
}

UMarsSaveGame* UMarsSaveGame::LoadSlot(const FString& Slot)
{
	return Slot.IsEmpty() ? nullptr : Cast<UMarsSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
}

FString UMarsSaveGame::MostRecentSlot()
{
	const UMarsSaveGame* A = LoadSlot(CheckpointSlot);
	const UMarsSaveGame* B = LoadSlot(ManualSlot);
	if (A && B)
	{
		return A->Timestamp > B->Timestamp ? CheckpointSlot : ManualSlot;
	}
	return A ? CheckpointSlot : B ? ManualSlot : FString();
}

bool UMarsSaveGame::SaveWorld(UWorld* World, const FString& Slot, const FString& InLabel)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AMarsCharacter* Player = PC ? Cast<AMarsCharacter>(PC->GetPawn()) : nullptr;
	AMarsReconShuttle* FlyingShuttle = PC ? Cast<AMarsReconShuttle>(PC->GetPawn()) : nullptr;
	if (!Player && FlyingShuttle)
	{
		Player = FlyingShuttle->GetPilot(); // mission return checkpoint from the cockpit
	}
	if (!Player || Player->IsDead())
	{
		return false;
	}

	UMarsSaveGame* Save = Cast<UMarsSaveGame>(UGameplayStatics::CreateSaveGameObject(UMarsSaveGame::StaticClass()));
	Save->Label = InLabel;
	Save->Timestamp = FDateTime::Now();
	Save->PlayerTransform = Player->GetActorTransform();
	if (FlyingShuttle)
	{
		// Restore on the pad next to the docked shuttle rather than mid-air.
		Save->PlayerTransform = FTransform(FRotator::ZeroRotator, FlyingShuttle->DockLocation + FVector(0.0, 600.0, 120.0));
	}
	Save->ControlRotation = PC->GetControlRotation();
	Save->LocationName = Player->GetCurrentModule() ? Player->GetCurrentModule()->GetModuleName() : TEXT("SURFACE");
	Save->Inventory = Player->GetInventory()->GetSlots();
	Save->InspectedTypes = Player->GetInspectedTypes();
	Save->SuitO2Percent = Player->GetSuit() ? Player->GetSuit()->GetO2Fraction01() * 100.0f : 0.0f;

	if (const AMarsGameMode* GM = World->GetAuthGameMode<AMarsGameMode>())
	{
		Save->RemainingSeconds = GM->GetRemainingSeconds();
	}
	if (const UMarsStationSubsystem* Station = World->GetSubsystem<UMarsStationSubsystem>())
	{
		Save->bBioAgentCharacterized = Station->IsBioAgentCharacterized();
		Save->PowerRoute = Station->GetPowerRoute();
	}
	if (const UMarsEnvironmentSubsystem* Env = World->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		Save->Sol = Env->GetSolNumber();
		Save->SolHours = Env->GetLocalSolarTimeHours();
	}
	if (const UMarsMissionDirector* Ops = UMarsMissionDirector::Get(World))
	{
		Save->MissionsCompleted = Ops->GetCompletedMask();
	}
	if (const UMarsStationSubsystem* Grid = World->GetSubsystem<UMarsStationSubsystem>())
	{
		Save->bEmShield = Grid->IsEmShieldUp();
	}
	for (TActorIterator<AMarsReconShuttle> It(World); It; ++It)
	{
		Save->bHasShuttle = true;
		Save->ShuttleTransform = FlyingShuttle == *It ? FTransform(It->GetActorRotation(), It->DockLocation + FVector(0.0, 0.0, 320.0)) : It->GetActorTransform();
		Save->ShuttleBatteryWh = It->BatteryWh;
		Save->ShuttleO2Liters = It->O2ReserveLiters;
		break;
	}
	if (const UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(World))
	{
		Save->StoryStep = Story->GetStepIndex();
		Save->StoryLogs = Story->GetLogTitles();
	}

	auto Add = [Save](UObject* Obj, const FString& Key, const UClass* StopAt)
	{
		FMarsSavedObject O;
		O.Key = Key;
		Capture(Obj, StopAt, O);
		Save->Objects.Add(MoveTemp(O));
	};
	Add(Player->GetBiometrics(), TEXT("Player.Biometrics"), UActorComponent::StaticClass());
	Add(Player->GetSuit(), TEXT("Player.Suit"), UActorComponent::StaticClass());
	if (Player->GetFatigue())
	{
		Add(Player->GetFatigue(), TEXT("Player.Fatigue"), UActorComponent::StaticClass());
	}
	for (TActorIterator<AMarsBaseModule> It(World); It; ++It)
	{
		Add(*It, It->GetName(), AActor::StaticClass());
		if (const ACommRelayModule* Comm = Cast<ACommRelayModule>(*It))
		{
			Save->TransistorsInstalled = Comm->InstalledTransistors;
		}
	}
	for (TActorIterator<AMarsStationBase> It(World); It; ++It)
	{
		Add(*It, It->GetName(), AActor::StaticClass());
	}
	for (TActorIterator<AMarsPickup> It(World); It; ++It)
	{
		Save->RemainingPickups.Add(It->GetName());
	}

	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	UE_LOG(LogMarsSurvival, Log, TEXT("Save '%s' (%s) -> %s"), *InLabel, *Slot, bOk ? TEXT("ok") : TEXT("FAILED"));
	return bOk;
}

void UMarsSaveGame::ApplyToWorld(UWorld* World) const
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AMarsCharacter* Player = PC ? Cast<AMarsCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		return;
	}

	TMap<FString, const FMarsSavedObject*> ByKey;
	for (const FMarsSavedObject& O : Objects)
	{
		ByKey.Add(O.Key, &O);
	}

	for (TActorIterator<AMarsBaseModule> It(World); It; ++It)
	{
		if (const FMarsSavedObject* const* O = ByKey.Find(It->GetName()))
		{
			Restore(*It, AActor::StaticClass(), **O);
		}
	}
	for (TActorIterator<AMarsStationBase> It(World); It; ++It)
	{
		if (const FMarsSavedObject* const* O = ByKey.Find(It->GetName()))
		{
			Restore(*It, AActor::StaticClass(), **O);
		}
	}
	TArray<AMarsPickup*> Taken;
	for (TActorIterator<AMarsPickup> It(World); It; ++It)
	{
		if (!RemainingPickups.Contains(It->GetName()))
		{
			Taken.Add(*It);
		}
	}
	for (AMarsPickup* P : Taken)
	{
		P->Destroy();
	}

	if (const FMarsSavedObject* const* O = ByKey.Find(TEXT("Player.Biometrics")))
	{
		Restore(Player->GetBiometrics(), UActorComponent::StaticClass(), **O);
	}
	if (const FMarsSavedObject* const* O = ByKey.Find(TEXT("Player.Suit")))
	{
		Restore(Player->GetSuit(), UActorComponent::StaticClass(), **O);
	}
	if (const FMarsSavedObject* const* O = ByKey.Find(TEXT("Player.Fatigue")))
	{
		if (Player->GetFatigue())
		{
			Restore(Player->GetFatigue(), UActorComponent::StaticClass(), **O);
		}
	}
	Player->GetInventory()->SetSlots(Inventory);
	if (UMarsEnvironmentSubsystem* Env = World->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		Env->SetClock(Sol, SolHours);
	}
	if (UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(World))
	{
		Story->SetStepIndex(StoryStep);
		Story->SetLogs(StoryLogs);
	}
	if (UMarsStationSubsystem* Grid = World->GetSubsystem<UMarsStationSubsystem>())
	{
		Grid->SetPowerRoute(PowerRoute);
		Grid->SetEmShield(bEmShield);
	}
	if (UMarsMissionDirector* Ops = UMarsMissionDirector::Get(World))
	{
		Ops->SetCompletedMask(MissionsCompleted);
	}
	if (bHasShuttle)
	{
		for (TActorIterator<AMarsReconShuttle> It(World); It; ++It)
		{
			It->SetActorTransform(ShuttleTransform, false, nullptr, ETeleportType::TeleportPhysics);
			It->BatteryWh = ShuttleBatteryWh;
			It->O2ReserveLiters = ShuttleO2Liters;
			break;
		}
	}
	Player->SetInspectedTypes(InspectedTypes);
	Player->SetActorTransform(PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(ControlRotation);

	if (bBioAgentCharacterized)
	{
		if (UMarsStationSubsystem* Station = World->GetSubsystem<UMarsStationSubsystem>())
		{
			Station->SetBioAgentCharacterized();
		}
	}
	UE_LOG(LogMarsSurvival, Log, TEXT("Loaded save '%s' (%d objects)"), *Label, Objects.Num());
}
