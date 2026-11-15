#include "Core/MarsMissionDirector.h"

#include "MarsSurvival.h"
#include "AI/AriaSubsystem.h"
#include "Audio/MarsAudioManager.h"
#include "Core/MarsGameMode.h"
#include "Core/MarsNarrativeDirector.h"
#include "Items/MarsInventoryComponent.h"
#include "Items/MarsItemLibrary.h"
#include "Player/MarsCharacter.h"
#include "Props/MarsPropBuilder.h"
#include "Vehicles/MarsReconShuttle.h"
#include "World/MarsBaseManager.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

const FVector UMarsMissionDirector::OrbitOrigin(0.0, 0.0, 12000000.0); // 120 km up: above the atmosphere

UMarsMissionDirector* UMarsMissionDirector::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMarsMissionDirector>() : nullptr;
}

void UMarsMissionDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildMissions();
}

void UMarsMissionDirector::BuildMissions()
{
	using K = EMarsObjectiveKind;
	auto Obj = [](K Kind, const TCHAR* Text, const FVector& Offset, EMarsPropKind Prop, float Radius, float Duration, float Scale = 1.0f)
	{
		FMarsMissionObjective O;
		O.Kind = Kind;
		O.Text = Text;
		O.Offset = Offset;
		O.PropKind = static_cast<uint8>(Prop);
		O.Radius = Radius;
		O.Duration = Duration;
		O.Scale = Scale;
		return O;
	};
	Missions.Reset();
	{
		FMarsMission M;
		M.Code = TEXT("OP-1"); M.Name = TEXT("FRAGMENT SURVEY"); M.Sol = 2;
		M.Briefing = TEXT("Three Phobos-ejecta fragments crossed our orbit overnight. Scan each one - keep it in the nose cone.");
		M.Objectives = { Obj(K::Scan, TEXT("Scan fragment A"), FVector(400000, 120000, 30000), EMarsPropKind::Asteroid, 90000, 3.0f, 3.0f),
						 Obj(K::Scan, TEXT("Scan fragment B"), FVector(520000, -260000, -60000), EMarsPropKind::Asteroid, 90000, 3.0f, 4.5f),
						 Obj(K::Scan, TEXT("Scan fragment C"), FVector(250000, -480000, 80000), EMarsPropKind::Asteroid, 90000, 3.0f, 2.5f) };
		M.Reward = { { EMarsItemType::SpareParts, 1 } };
		M.AriaDone = TEXT("A.R.I.A.: Fragment spectra logged. Hydrated minerals in B. Someone will want those numbers. Bring us home.");
		Missions.Add(M);
	}
	{
		FMarsMission M;
		M.Code = TEXT("OP-2"); M.Name = TEXT("TELEMETRY HARVEST"); M.Sol = 4;
		M.Briefing = TEXT("Three science buoys from the old relay network still hold deep-space data. Fly within 150 m of each to pull it.");
		M.Objectives = { Obj(K::Reach, TEXT("Download buoy SB-1"), FVector(300000, 300000, 0), EMarsPropKind::ScienceBuoy, 15000, 0.0f),
						 Obj(K::Reach, TEXT("Download buoy SB-2"), FVector(-200000, 450000, 40000), EMarsPropKind::ScienceBuoy, 15000, 0.0f),
						 Obj(K::Reach, TEXT("Download buoy SB-3"), FVector(-420000, 100000, -30000), EMarsPropKind::ScienceBuoy, 15000, 0.0f) };
		M.Reward = { { EMarsItemType::O2Canister, 2 } };
		M.AriaDone = TEXT("A.R.I.A.: Buoy archives recovered. Most of it is radiation telemetry. One file is encrypted with Directorate keys.");
		Missions.Add(M);
	}
	{
		FMarsMission M;
		M.Code = TEXT("OP-3"); M.Name = TEXT("SIGNAL INTERCEPT"); M.Sol = 6;
		M.Briefing = TEXT("Relay R-7 is still forwarding traffic we are not meant to see. Hold position inside its beam for 20 seconds.");
		M.Objectives = { Obj(K::Hold, TEXT("Hold inside relay R-7's beam"), FVector(600000, 0, 50000), EMarsPropKind::RelaySatellite, 45000, 20.0f, 0.5f) };
		M.Reward = { { EMarsItemType::NeuroAntitoxin, 1 } };
		M.LogTitle = TEXT("INTERCEPT // R-7 FORWARD QUEUE");
		M.LogText = TEXT("DIRECTORATE > ARES-IV OVERSIGHT: 'Subject cognitive decline within projections. Continue passive observation. Do NOT authorise rescue before Sol 30.'");
		M.AriaDone = TEXT("A.R.I.A.: I recorded everything. They are not coming for you. They are timing you.");
		Missions.Add(M);
	}
	{
		FMarsMission M;
		M.Code = TEXT("OP-4"); M.Name = TEXT("DERELICT KESTREL"); M.Sol = 8;
		M.Briefing = TEXT("The supply tug Kestrel went silent in orbit on Sol 97. Approach slowly and hold at the docking ring, then scan the reactor.");
		M.Objectives = { Obj(K::Dock, TEXT("Dock with Kestrel (approach < 8 m/s, hold 3 s)"), FVector(-500000, -350000, 0), EMarsPropKind::DerelictStation, 20000, 3.0f),
						 Obj(K::Scan, TEXT("Scan Kestrel's reactor section"), FVector(-500000, -350000, 0), EMarsPropKind::None, 120000, 4.0f) };
		M.Reward = { { EMarsItemType::SpareParts, 2 } };
		M.LogTitle = TEXT("DATA LOG // KESTREL MASTER LOG");
		M.LogText = TEXT("Kestrel was ordered to hold orbit as a relay of last resort - with no crew aboard and its uplink fused. A one-way mirror over the outpost.");
		M.AriaDone = TEXT("A.R.I.A.: Kestrel's reactor is cold, its uplink fused on purpose. Parts are stowed.");
		Missions.Add(M);
	}
	{
		FMarsMission M;
		M.Code = TEXT("OP-5"); M.Name = TEXT("DEBRIS RECOVERY"); M.Sol = 10;
		M.Briefing = TEXT("Two medical resupply canisters from the evacuation flight are tumbling in a debris field. Recover them.");
		M.Objectives = { Obj(K::Reach, TEXT("Recover canister MED-1"), FVector(350000, -500000, -20000), EMarsPropKind::DebrisContainer, 12000, 0.0f),
						 Obj(K::Reach, TEXT("Recover canister MED-2"), FVector(480000, -620000, 60000), EMarsPropKind::DebrisContainer, 12000, 0.0f) };
		M.Reward = { { EMarsItemType::MethyleneBlueCartridge, 2 }, { EMarsItemType::Amifostine, 1 } };
		M.AriaDone = TEXT("A.R.I.A.: Medical canisters recovered intact. Methylene blue and amifostine. You will need them.");
		Missions.Add(M);
	}
	{
		FMarsMission M;
		M.Code = TEXT("OP-6"); M.Name = TEXT("LAST LIGHT"); M.Sol = 12;
		M.Briefing = TEXT("The Directorate's observation satellite passes overhead tonight. Scan it, then sit in its downlink and copy everything.");
		M.Objectives = { Obj(K::Scan, TEXT("Scan the observation satellite"), FVector(0, 650000, 100000), EMarsPropKind::RelaySatellite, 120000, 4.0f, 0.6f),
						 Obj(K::Hold, TEXT("Copy the downlink (hold position 15 s)"), FVector(0, 650000, 100000), EMarsPropKind::None, 50000, 15.0f) };
		M.Reward = { { EMarsItemType::SpareParts, 1 } };
		M.LogTitle = TEXT("DOWNLINK // OBSERVATION ARCHIVE");
		M.LogText = TEXT("Every Sol of yours, logged: vitals, sleep, hallucination frequency. Annotated 'ARES-IV EXPOSURE TRIAL - SUBJECT 1'. The uplink is the only thing they fear.");
		M.AriaDone = TEXT("A.R.I.A.: We have the whole archive. If the uplink works, the world gets it too.");
		Missions.Add(M);
	}
	for (FMarsMission& M : Missions)
	{
		for (FMarsMissionObjective& O : M.Objectives)
		{
			O.Tint = FLinearColor(1.0f, 0.35f, 0.1f);
		}
	}
}

int32 UMarsMissionDirector::GetAvailableMission() const
{
	const UMarsEnvironmentSubsystem* Env = GetWorld() ? GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>() : nullptr;
	const int32 Sol = Env ? Env->GetSolNumber() : 1;
	for (int32 i = 0; i < Missions.Num(); ++i)
	{
		if (!(CompletedMask & (1 << i)) && Missions[i].Sol <= Sol)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString UMarsMissionDirector::GetStatusLine() const
{
	if (const FMarsMission* M = GetActiveMission())
	{
		return FString::Printf(TEXT("%s %s - %s"), *M->Code, *M->Name, IsActiveMissionComplete() ? TEXT("COMPLETE, RETURN (HOLD C)") : *GetObjectiveText());
	}
	const int32 Next = GetAvailableMission();
	if (Next != INDEX_NONE)
	{
		return FString::Printf(TEXT("%s %s READY - fly RS-2 above 1.2 km, hold SPACE"), *Missions[Next].Code, *Missions[Next].Name);
	}
	int32 Done = 0;
	for (int32 i = 0; i < Missions.Num(); ++i)
	{
		Done += (CompletedMask >> i) & 1;
	}
	for (int32 i = 0; i < Missions.Num(); ++i)
	{
		if (!(CompletedMask & (1 << i)))
		{
			return FString::Printf(TEXT("%d/%d DONE - next: %s on Sol %d"), Done, Missions.Num(), *Missions[i].Code, Missions[i].Sol);
		}
	}
	return FString::Printf(TEXT("ALL %d OPERATIONS COMPLETE"), Missions.Num());
}

bool UMarsMissionDirector::IsActiveMissionComplete() const
{
	const FMarsMission* M = GetActiveMission();
	return M && ObjectiveIndex >= M->Objectives.Num();
}

FString UMarsMissionDirector::GetObjectiveText() const
{
	const FMarsMission* M = GetActiveMission();
	if (!M)
	{
		return FString();
	}
	if (ObjectiveIndex >= M->Objectives.Num())
	{
		return TEXT("All objectives complete - hold C to de-orbit");
	}
	return FString::Printf(TEXT("%d/%d  %s"), ObjectiveIndex + 1, M->Objectives.Num(), *M->Objectives[ObjectiveIndex].Text);
}

float UMarsMissionDirector::GetObjectiveProgress01() const
{
	const FMarsMission* M = GetActiveMission();
	if (!M || ObjectiveIndex >= M->Objectives.Num())
	{
		return 1.0f;
	}
	const float D = M->Objectives[ObjectiveIndex].Duration;
	return D > 0.0f ? FMath::Clamp(ObjectiveProgress / D, 0.0f, 1.0f) : 0.0f;
}

AActor* UMarsMissionDirector::GetObjectiveTarget() const
{
	if (!bInOrbit || Malfunction == EMarsMalfunction::NavFailure)
	{
		return nullptr;
	}
	return Targets.IsValidIndex(ObjectiveIndex) ? Targets[ObjectiveIndex].Get() : nullptr;
}

FString UMarsMissionDirector::GetMalfunctionText() const
{
	switch (Malfunction)
	{
	case EMarsMalfunction::ThrusterStuck: return TEXT("RCS VALVE STUCK OPEN - CRAFT YAWING");
	case EMarsMalfunction::NavFailure:    return TEXT("STAR TRACKER FAILURE - NAV OFFLINE");
	case EMarsMalfunction::PowerFault:    return TEXT("POWER BUS SHORT - BATTERY DRAIN x3");
	case EMarsMalfunction::CoolantLeak:   return TEXT("CABIN O2 LINE VENTING");
	default:                              return FString();
	}
}

// ----------------------------------------------------------------------------
// Orbit transitions
// ----------------------------------------------------------------------------

void UMarsMissionDirector::RequestOrbitInsertion(AMarsReconShuttle* Shuttle)
{
	if (!Shuttle || bInOrbit || Transition != ETransition::None)
	{
		return;
	}
	const int32 Mission = GetAvailableMission();
	if (Mission == INDEX_NONE)
	{
		if (DenyCooldown <= 0.0f)
		{
			DenyCooldown = 6.0f;
			if (AMarsCharacter* P = Shuttle->GetPilot())
			{
				P->ShowNotification(FText::FromString(TEXT("NO ORBITAL TASKING TODAY - ") + GetStatusLine()), 4.0f);
			}
		}
		return;
	}
	ActiveMission = Mission;
	ObjectiveIndex = 0;
	ObjectiveProgress = 0.0f;
	TransitShuttle = Shuttle;
	Transition = ETransition::ToOrbit;
	TransitionTime = 0.0f;
	if (UAriaSubsystem* Aria = GetWorld()->GetSubsystem<UAriaSubsystem>())
	{
		Aria->SayScripted(FString::Printf(TEXT("A.R.I.A.: Orbit burn. %s, %s. %s"), *Missions[Mission].Code, *Missions[Mission].Name, *Missions[Mission].Briefing));
	}
}

void UMarsMissionDirector::RequestDeorbit(AMarsReconShuttle* Shuttle, bool bEmergency)
{
	if (!Shuttle || !bInOrbit || Transition != ETransition::None)
	{
		return;
	}
	bEmergencyReturn = bEmergency;
	TransitShuttle = Shuttle;
	Transition = ETransition::ToSurface;
	TransitionTime = 0.0f;
}

void UMarsMissionDirector::FinishTransition()
{
	AMarsReconShuttle* Shuttle = TransitShuttle.Get();
	UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
	if (!Shuttle)
	{
		return;
	}
	if (Transition == ETransition::ToOrbit)
	{
		bInOrbit = true;
		Shuttle->EnterOrbit(OrbitOrigin + FVector(-60000.0, 0.0, 0.0));
		if (Env)
		{
			Env->SetOrbitLighting(true);
		}
		SpawnTargets();
		MalfunctionTimer = FMath::FRandRange(25.0f, 50.0f);
		return;
	}

	// Back to the surface, above Pad 2.
	bInOrbit = false;
	ClearTargets();
	if (Env)
	{
		Env->SetOrbitLighting(false);
	}
	Shuttle->LeaveOrbit(Shuttle->DockLocation + FVector(0.0, 0.0, 120000.0));
	PostReturnHazardTime = 60.0f;
	AMarsCharacter* Pilot = Shuttle->GetPilot();
	const FMarsMission* M = GetActiveMission();
	if (M && IsActiveMissionComplete() && !bEmergencyReturn)
	{
		CompletedMask |= 1 << ActiveMission;
		TArray<FString> Got;
		for (const FMarsInventorySlot& R : M->Reward)
		{
			if (Pilot && Pilot->GetInventory() && Pilot->GetInventory()->AddItem(R.ItemType, R.Count) > 0)
			{
				Got.Add(FString::Printf(TEXT("+%d %s"), R.Count, *UMarsItemLibrary::GetItemName(R.ItemType).ToString().ToUpper()));
			}
		}
		if (Pilot)
		{
			Pilot->ShowNotification(FText::FromString(FString::Printf(TEXT("%s COMPLETE   %s"), *M->Code, *FString::Join(Got, TEXT("  ")))), 5.0f);
			if (!M->LogText.IsEmpty())
			{
				Pilot->ShowPanel(FString::Printf(TEXT("%s\n\n%s"), *M->LogTitle, *M->LogText), 25.0f);
				if (UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this))
				{
					Story->AddLog(M->LogTitle);
				}
			}
		}
		if (UAriaSubsystem* Aria = GetWorld()->GetSubsystem<UAriaSubsystem>())
		{
			Aria->SayScripted(M->AriaDone);
		}
		if (AMarsGameMode* GM = GetWorld()->GetAuthGameMode<AMarsGameMode>())
		{
			GM->RequestCheckpoint(FString::Printf(TEXT("%s RETURN"), *M->Code));
		}
	}
	else if (M)
	{
		// Failed or aborted op: the hard return and lost hardware cost the base.
		if (bEmergencyReturn)
		{
			if (UMarsBaseManager* Base = UMarsBaseManager::Get(this))
			{
				Base->ApplyBaseDamage(0.05f);
			}
		}
		if (Pilot)
		{
			Pilot->ShowNotification(FText::FromString(FString::Printf(TEXT("%s %s"), *M->Code, bEmergencyReturn ? TEXT("FAILED - EMERGENCY RETURN, BASE SYSTEMS STRAINED") : TEXT("ABORTED - TRY AGAIN"))), 5.0f);
		}
	}
	ActiveMission = INDEX_NONE;
	Malfunction = EMarsMalfunction::None;
	bEmergencyReturn = false;
}

void UMarsMissionDirector::SpawnTargets()
{
	ClearTargets();
	const FMarsMission* M = GetActiveMission();
	UWorld* World = GetWorld();
	auto Spawn = [World](EMarsPropKind Kind, const FVector& Where, float Scale, const FLinearColor& Tint, const FVector& Extent) -> AActor*
	{
		const FTransform Xf(FRotator(FMath::FRandRange(-20.0f, 20.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(-20.0f, 20.0f)), Where);
		AMarsPropBuilder* Prop = World->SpawnActorDeferred<AMarsPropBuilder>(AMarsPropBuilder::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Prop)
		{
			Prop->Kind = Kind;
			Prop->Scale = Scale;
			Prop->bCollision = Kind != EMarsPropKind::StarField;
			Prop->Tint = Tint;
			Prop->Extent = Extent;
			Prop->FinishSpawning(Xf);
		}
		return Prop;
	};

	// Star field shell + loose rubble for parallax.
	Scenery.Add(Spawn(EMarsPropKind::StarField, OrbitOrigin, 1.0f, FLinearColor::White, FVector(1500000.0f)));
	for (int32 i = 0; i < 14; ++i)
	{
		const FVector P = OrbitOrigin + FMath::VRand() * FMath::FRandRange(150000.0f, 700000.0f);
		Scenery.Add(Spawn(EMarsPropKind::Asteroid, P, FMath::FRandRange(0.5f, 2.0f), FLinearColor::White, FVector(P.X * 0.001f, P.Y * 0.001f, 1.0f)));
	}
	if (!M)
	{
		return;
	}
	AActor* Last = nullptr;
	for (const FMarsMissionObjective& O : M->Objectives)
	{
		AActor* T = nullptr;
		if (O.PropKind == static_cast<uint8>(EMarsPropKind::None))
		{
			T = Last; // same object as the previous objective (e.g. scan the station we docked with)
		}
		else
		{
			T = Spawn(static_cast<EMarsPropKind>(O.PropKind), OrbitOrigin + O.Offset, O.Scale, O.Tint, FVector(O.Offset.X * 0.001f, O.Offset.Y * 0.001f, 1.0f));
		}
		Targets.Add(T);
		Last = T;
	}
}

void UMarsMissionDirector::ClearTargets()
{
	TSet<AActor*> Unique;
	for (AActor* A : Targets) { if (A) { Unique.Add(A); } }
	for (AActor* A : Scenery) { if (A) { Unique.Add(A); } }
	for (AActor* A : Unique)
	{
		A->Destroy();
	}
	Targets.Reset();
	Scenery.Reset();
}

// ----------------------------------------------------------------------------
// Tick
// ----------------------------------------------------------------------------

void UMarsMissionDirector::UpdateObjectives(float DeltaTime)
{
	const FMarsMission* M = GetActiveMission();
	AMarsReconShuttle* Shuttle = TransitShuttle.Get();
	if (!M || !Shuttle || ObjectiveIndex >= M->Objectives.Num() || !Targets.IsValidIndex(ObjectiveIndex) || !Targets[ObjectiveIndex])
	{
		return;
	}
	const FMarsMissionObjective& O = M->Objectives[ObjectiveIndex];
	const FVector To = Targets[ObjectiveIndex]->GetActorLocation() - Shuttle->GetActorLocation();
	const float Dist = To.Size();
	bool bDone = false;
	switch (O.Kind)
	{
	case EMarsObjectiveKind::Reach:
		bDone = Dist < O.Radius;
		break;
	case EMarsObjectiveKind::Scan:
	{
		const FVector Nose = FRotator(0.0f, Shuttle->GetActorRotation().Yaw, 0.0f).Vector();
		const bool bInCone = Dist < O.Radius && FVector::DotProduct(Nose, To.GetSafeNormal()) > 0.94f;
		ObjectiveProgress = bInCone ? ObjectiveProgress + DeltaTime : FMath::Max(ObjectiveProgress - DeltaTime * 0.5f, 0.0f);
		bDone = ObjectiveProgress >= O.Duration;
		break;
	}
	case EMarsObjectiveKind::Hold:
		ObjectiveProgress = Dist < O.Radius ? ObjectiveProgress + DeltaTime : FMath::Max(ObjectiveProgress - DeltaTime, 0.0f);
		bDone = ObjectiveProgress >= O.Duration;
		break;
	case EMarsObjectiveKind::Dock:
		ObjectiveProgress = (Dist < O.Radius && Shuttle->GetSpeedMs() < 8.0f) ? ObjectiveProgress + DeltaTime : 0.0f;
		bDone = ObjectiveProgress >= O.Duration;
		break;
	}
	if (bDone)
	{
		++ObjectiveIndex;
		ObjectiveProgress = 0.0f;
		if (AMarsCharacter* P = Shuttle->GetPilot())
		{
			P->ShowNotification(FText::FromString(IsActiveMissionComplete() ? TEXT("ALL OBJECTIVES COMPLETE - HOLD C TO DE-ORBIT") : FString::Printf(TEXT("OBJECTIVE COMPLETE - %s"), *GetObjectiveText())), 4.0f);
			if (UMarsAudioManager* Audio = P->GetAudioManager())
			{
				Audio->PlayUIBeep(1.6f);
			}
		}
	}
}

void UMarsMissionDirector::UpdateMalfunctions(float DeltaTime)
{
	AMarsReconShuttle* Shuttle = TransitShuttle.Get();
	const bool bExposed = bInOrbit || PostReturnHazardTime > 0.0f;
	PostReturnHazardTime = FMath::Max(PostReturnHazardTime - DeltaTime, 0.0f);
	if (!Shuttle || !Shuttle->GetPilot())
	{
		return;
	}
	if (Malfunction == EMarsMalfunction::None)
	{
		if (!bExposed)
		{
			return;
		}
		MalfunctionTimer -= DeltaTime;
		if (MalfunctionTimer <= 0.0f)
		{
			MalfunctionTimer = FMath::FRandRange(45.0f, 90.0f);
			Malfunction = static_cast<EMarsMalfunction>(FMath::RandRange(1, 4));
			RepairProgress = 0.0f;
			Shuttle->GetPilot()->ShowNotification(FText::FromString(FString::Printf(TEXT("!! MALFUNCTION: %s - HOLD R FOR MANUAL OVERRIDE !!"), *GetMalfunctionText())), 5.0f);
			if (UMarsAudioManager* Audio = Shuttle->GetPilot()->GetAudioManager())
			{
				Audio->PlayUIBeep(0.5f);
			}
		}
		return;
	}
	// Effects are applied by the shuttle (it queries GetMalfunction()); repair here.
	RepairProgress = Shuttle->IsRepairHeld() ? RepairProgress + DeltaTime : FMath::Max(RepairProgress - DeltaTime, 0.0f);
	if (RepairProgress >= 3.0f)
	{
		Malfunction = EMarsMalfunction::None;
		RepairProgress = 0.0f;
		Shuttle->GetPilot()->ShowNotification(FText::FromString(TEXT("MANUAL OVERRIDE COMPLETE - SYSTEM RESTORED")), 3.0f);
	}
}

void UMarsMissionDirector::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->IsPaused())
	{
		return;
	}
	DenyCooldown = FMath::Max(DenyCooldown - DeltaTime, 0.0f);

	if (Transition != ETransition::None)
	{
		TransitionTime += DeltaTime;
		Fade = TransitionTime < 1.5f ? TransitionTime / 1.5f : FMath::Clamp(1.0f - (TransitionTime - 2.5f) / 1.5f, 0.0f, 1.0f);
		if (TransitionTime >= 1.5f && TransitionTime - DeltaTime < 1.5f)
		{
			FinishTransition();
		}
		if (TransitionTime >= 4.0f)
		{
			Transition = ETransition::None;
			Fade = 0.0f;
		}
		return;
	}

	if (bInOrbit)
	{
		UpdateObjectives(DeltaTime);
		// Dead battery in orbit: emergency return after 30 s of life-support reserve.
		if (AMarsReconShuttle* Shuttle = TransitShuttle.Get())
		{
			DeadBatteryTime = Shuttle->GetBattery01() <= 0.0f ? DeadBatteryTime + DeltaTime : 0.0f;
			if (DeadBatteryTime > 30.0f)
			{
				DeadBatteryTime = 0.0f;
				RequestDeorbit(Shuttle, true);
			}
		}
	}
	UpdateMalfunctions(DeltaTime);
}

void UMarsMissionDirector::DevForceOrbit(AMarsReconShuttle* Shuttle)
{
	if (!Shuttle || bInOrbit || Transition != ETransition::None)
	{
		return;
	}
	ActiveMission = 0;
	ObjectiveIndex = 0;
	ObjectiveProgress = 0.0f;
	TransitShuttle = Shuttle;
	Transition = ETransition::ToOrbit;
	TransitionTime = 0.0f;
}
