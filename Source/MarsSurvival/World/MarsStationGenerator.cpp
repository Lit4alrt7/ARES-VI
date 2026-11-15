#include "World/MarsStationGenerator.h"

#include "MarsSurvival.h"
#include "Base/AirlockModule.h"
#include "Base/BioDomeModule.h"
#include "Base/CommRelayModule.h"
#include "Base/MarsBaseModule.h"
#include "Base/MedicalBayModule.h"
#include "Stations/MarsSaveTerminal.h"
#include "Stations/MarsStoryStations.h"
#include "Vehicles/MarsReconShuttle.h"
#include "Items/MarsItemTypes.h"
#include "Items/MarsPickup.h"
#include "Props/MarsPropBuilder.h"
#include "Stations/MarsStations.h"
#include "World/MarsShuttle.h"
#include "Components/TextRenderComponent.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"

namespace
{
	constexpr float WallT = 20.0f; // module wall thickness

	FVector DirFromYaw(float Yaw)
	{
		return FRotator(0.0f, Yaw, 0.0f).Vector();
	}

	uint8 K(EMarsPropKind Kind) { return static_cast<uint8>(Kind); }
}

AMarsStationGenerator::AMarsStationGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AMarsStationGenerator::Track(AActor* Actor)
{
	if (Actor)
	{
		GeneratedActors.Add(Actor);
#if WITH_EDITOR
		Actor->SetFolderPath(TEXT("AresStation"));
#endif
	}
}

void AMarsStationGenerator::ClearStation()
{
	for (AActor* Actor : GeneratedActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	GeneratedActors.Reset();
}

// ----------------------------------------------------------------------------
// Spawning helpers
// ----------------------------------------------------------------------------

AMarsBaseModule* AMarsStationGenerator::SpawnModule(const FPlan& P)
{
	const FTransform Xf(FRotator(0.0f, P.Yaw, 0.0f), P.Center);
	AMarsBaseModule* Module = GetWorld()->SpawnActorDeferred<AMarsBaseModule>(P.Class, Xf, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Module)
	{
		return nullptr;
	}
	Module->ModuleType = P.Type;
	switch (P.Type)
	{
	case EMarsModuleType::AirlockDecon:     Module->SectorColor = FLinearColor(1.0f, 0.55f, 0.12f); break; // amber hazard
	case EMarsModuleType::EvaPrep:          Module->SectorColor = FLinearColor(1.0f, 0.32f, 0.18f); break; // crimson
	case EMarsModuleType::AeroponicBioDome: Module->SectorColor = FLinearColor(0.15f, 1.0f, 0.7f); break;  // teal bioluminescence
	case EMarsModuleType::XenobiologyLab:   Module->SectorColor = FLinearColor(0.35f, 1.0f, 0.4f); break;  // emerald
	case EMarsModuleType::MedicalBay:       Module->SectorColor = FLinearColor(0.75f, 0.95f, 1.0f); break; // sterile cyan-white
	case EMarsModuleType::StorageCommRelay: Module->SectorColor = FLinearColor(0.9f, 0.25f, 1.0f); break;  // neon magenta
	case EMarsModuleType::CrewQuarters:     Module->SectorColor = FLinearColor(1.0f, 0.72f, 0.45f); break; // warm
	default:                                Module->SectorColor = P.bDuct ? FLinearColor(1.0f, 0.4f, 0.1f) : FLinearColor(0.35f, 0.55f, 1.0f); break;
	}
	Module->RoomSize = P.Size;
	Module->bFrontDoorway = P.bFront;
	Module->bBackDoorway = P.bBack;
	Module->bLeftDoorway = P.bLeft;
	Module->bRightDoorway = P.bRight;
	Module->bLeftWindow = P.bLeftWindow;
	Module->LeftDoorwayOffset = P.LeftOffset;
	Module->RightDoorwayOffset = P.RightOffset;
	Module->DoorwaySize = P.DoorSize;
	Module->SideDoorwaySize = P.SideDoorSize;
	Module->SectorLabel = P.Label;
	Module->SignFrontText = P.FrontSign;
	Module->SignBackText = P.BackSign;
	Module->SignLeftText = P.LeftSign;
	Module->SignRightText = P.RightSign;
	Module->FinishSpawning(Xf);
#if WITH_EDITOR
	Module->SetActorLabel(P.Label.Replace(TEXT(" "), TEXT("_")).Replace(TEXT("["), TEXT("")).Replace(TEXT("]"), TEXT("")).Replace(TEXT("&"), TEXT("And")));
#endif
	Track(Module);
	return Module;
}

AActor* AMarsStationGenerator::SpawnStationAt(UClass* StationClass, AMarsBaseModule* Module, const FVector& LocalPos, float LocalYaw)
{
	if (!StationClass || !Module)
	{
		return nullptr;
	}
	const FTransform World = FTransform(FRotator(0.0f, LocalYaw, 0.0f), LocalPos) * Module->GetActorTransform();
	AMarsStationBase* Station = GetWorld()->SpawnActorDeferred<AMarsStationBase>(StationClass, World, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Station)
	{
		Station->TargetModule = Module;
		Station->FinishSpawning(World);
		Track(Station);
	}
	return Station;
}

AActor* AMarsStationGenerator::SpawnStation(UClass* StationClass, AMarsBaseModule* Module, float LocalX, int32 WallSide, float ExtraYaw)
{
	if (!StationClass || !Module)
	{
		return nullptr;
	}
	const AMarsStationBase* Cdo = GetDefault<AMarsStationBase>(StationClass);
	const float Depth = Cdo ? Cdo->ConsoleSize.X : 60.0f;
	const float Y = WallSide * (Module->RoomSize.Y * 0.5f - Depth * 0.5f - 8.0f);
	return SpawnStationAt(StationClass, Module, FVector(LocalX, Y, 0.0f), (WallSide < 0 ? 90.0f : -90.0f) + ExtraYaw);
}

AActor* AMarsStationGenerator::SpawnPropAt(uint8 PropKind, AMarsBaseModule* Module, const FVector& LocalPos, const FRotator& LocalRot, float Scale, bool bCollision)
{
	if (!Module)
	{
		return nullptr;
	}
	const FTransform World = FTransform(LocalRot, LocalPos) * Module->GetActorTransform();
	AMarsPropBuilder* Prop = GetWorld()->SpawnActorDeferred<AMarsPropBuilder>(AMarsPropBuilder::StaticClass(), World, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Prop)
	{
		Prop->Kind = static_cast<EMarsPropKind>(PropKind);
		Prop->Scale = Scale;
		Prop->bCollision = bCollision;
		Prop->FinishSpawning(World);
		Track(Prop);
	}
	return Prop;
}

AActor* AMarsStationGenerator::SpawnProp(uint8 PropKind, AMarsBaseModule* Module, float LocalX, int32 WallSide, float Inset, float Scale)
{
	if (!Module)
	{
		return nullptr;
	}
	const float Y = WallSide * (Module->RoomSize.Y * 0.5f - Inset);
	return SpawnPropAt(PropKind, Module, FVector(LocalX, Y, 0.0f), FRotator(0.0f, WallSide < 0 ? 90.0f : -90.0f, 0.0f), Scale, true);
}

AActor* AMarsStationGenerator::SpawnPickup(EMarsItemType Item, int32 Count, AMarsBaseModule* Module, const FVector& LocalPos)
{
	if (!Module)
	{
		return nullptr;
	}
	const FTransform World = FTransform(LocalPos) * Module->GetActorTransform();
	AMarsPickup* Pickup = GetWorld()->SpawnActorDeferred<AMarsPickup>(AMarsPickup::StaticClass(), World, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Pickup)
	{
		Pickup->ItemType = Item;
		Pickup->Count = Count;
		Pickup->FinishSpawning(World);
		Track(Pickup);
	}
	return Pickup;
}

// ----------------------------------------------------------------------------
// Detail pass: ribs, ceiling services, valves
// ----------------------------------------------------------------------------

void AMarsStationGenerator::AddDetail(AMarsBaseModule* M, bool bDuct)
{
	if (!M || !bEnvironmentDetail)
	{
		return;
	}
	const float L = M->RoomSize.X, W = M->RoomSize.Y, H = M->RoomSize.Z;

	// Ceiling services: coolant pipe runs along one side, cable tray along the other.
	const int32 Runs = FMath::Max(FMath::FloorToInt(L / 400.0f), 1);
	const float Start = -Runs * 200.0f;
	for (int32 i = 0; i < Runs; ++i)
	{
		SpawnPropAt(K(EMarsPropKind::PipeRun), M, FVector(Start + i * 400.0f, -W * 0.5f + (bDuct ? 25.0f : 16.0f), H - (bDuct ? 14.0f : 82.0f)), FRotator::ZeroRotator, bDuct ? 0.7f : 0.8f, false);
	}
	if (bDuct)
	{
		return;
	}

	// Structural ribs along both walls (skip near doorways in the side walls).
	for (float X = -L * 0.5f + 70.0f; X <= L * 0.5f - 70.0f; X += 260.0f)
	{
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const bool bHasDoor = Side < 0 ? (M->bLeftDoorway || M->bLeftWindow) : M->bRightDoorway;
			const float DoorX = Side < 0 ? M->LeftDoorwayOffset : M->RightDoorwayOffset;
			if (bHasDoor && FMath::Abs(X - DoorX) < 200.0f)
			{
				continue;
			}
			SpawnPropAt(K(EMarsPropKind::StructuralRib), M, FVector(X, Side * (W * 0.5f - 7.0f), 0.0f), FRotator(0.0f, Side < 0 ? 90.0f : -90.0f, 0.0f), H / 300.0f, true);
		}
	}
	DecorateSector(M);
}

void AMarsStationGenerator::PopulateModule(AMarsBaseModule* M, bool bDuct)
{
	if (!M)
	{
		return;
	}
	AddDetail(M, bDuct);
	if (bDuct)
	{
		return;
	}
	const float HL = M->RoomSize.X * 0.5f;
	const float HW = M->RoomSize.Y * 0.5f;

	switch (M->ModuleType)
	{
	case EMarsModuleType::AirlockDecon:
		SpawnStation(AAirlockPanelStation::StaticClass(), M, 120.0f, +1);
		SpawnStation(ADustScrubberStation::StaticClass(), M, -150.0f, +1);
		break;

	case EMarsModuleType::EvaPrep:
		SpawnStation(ASuitRackStation::StaticClass(), M, -180.0f, -1);
		SpawnStation(AO2RefillStation::StaticClass(), M, 170.0f, -1);
		SpawnStation(AModuleStatusPanel::StaticClass(), M, 0.0f, +1);
		SpawnProp(K(EMarsPropKind::ValveWheel), M, 250.0f, +1, 12.0f);
		SpawnLocker(M, -220.0f, +1, K(EMarsPropKind::WallLocker), TEXT("EVA SUPPLY"), { { EMarsItemType::O2Canister, 2 }, { EMarsItemType::SealantCartridge, 1 } });
		break;

	case EMarsModuleType::AeroponicBioDome:
	{
		SpawnProp(K(EMarsPropKind::PhotobioreactorRack), M, -380.0f, -1, 45.0f);
		SpawnProp(K(EMarsPropKind::PhotobioreactorRack), M, 380.0f, -1, 45.0f);
		SpawnProp(K(EMarsPropKind::PhotobioreactorRack), M, 380.0f, +1, 45.0f);
		if (!M->bRightDoorway || FMath::Abs(M->RightDoorwayOffset + 380.0f) > 260.0f)
		{
			SpawnProp(K(EMarsPropKind::PhotobioreactorRack), M, -380.0f, +1, 45.0f);
		}
		SpawnStation(ACo2ScrubberStation::StaticClass(), M, 0.0f, +1);
		SpawnStation(AWaterDispenserStation::StaticClass(), M, 0.0f, -1);
		SpawnStation(AChlorellaHarvesterStation::StaticClass(), M, HL - 120.0f, -1);
		// Hydroponic bays: two rows of UV-lit racks either side of the central walkway, sealed specimen terrarium.
		for (int32 r = 0; r < 3; ++r)
		{
			const float X = -260.0f + r * 220.0f;
			SpawnPropAt(K(EMarsPropKind::HydroponicRack), M, FVector(X, 150.0f, 0.0f), FRotator::ZeroRotator, 1.0f, true);
			SpawnPropAt(K(EMarsPropKind::HydroponicRack), M, FVector(X, -150.0f, 0.0f), FRotator::ZeroRotator, 1.0f, true);
		}
		SpawnPropAt(K(EMarsPropKind::Terrarium), M, FVector(420.0f, 150.0f, 0.0f), FRotator(0, 90, 0), 1.0f, true);
		break;
	}

	case EMarsModuleType::XenobiologyLab:
		SpawnStation(AXenoAnalyzerStation::StaticClass(), M, -250.0f, -1);
		SpawnStation(AMicroscopeStation::StaticClass(), M, 250.0f, -1);
		SpawnStation(AContainmentBoxStation::StaticClass(), M, -60.0f, +1);
		SpawnStation(AModuleStatusPanel::StaticClass(), M, 330.0f, +1);
		SpawnPropAt(K(EMarsPropKind::Terrarium), M, FVector(150.0f, 130.0f, 0.0f), FRotator(0, 90, 0), 1.0f, true);
		SpawnPropAt(K(EMarsPropKind::GlassPartition), M, FVector(-420.0f, 170.0f, 0.0f), FRotator::ZeroRotator, 1.0f, true);
		SpawnPropAt(K(EMarsPropKind::HydroponicRack), M, FVector(-120.0f, -110.0f, 0.0f), FRotator(0, 90, 0), 0.8f, true); // culture test rack
		SpawnLocker(M, 0.0f, -1, K(EMarsPropKind::WallLocker), TEXT("SAMPLE CABINET"), { { EMarsItemType::RockSample, 1 }, { EMarsItemType::RegolithSample, 1 } });
		break;

	case EMarsModuleType::MedicalBay:
		SpawnStationAt(ADiagnosticScannerBed::StaticClass(), M, FVector(-100.0f, HW - 70.0f, 0.0f), 0.0f);
		SpawnStation(ABioprinterStation::StaticClass(), M, -200.0f, -1);
		SpawnStation(AModuleStatusPanel::StaticClass(), M, 200.0f, -1);
		SpawnStation(AMarsSaveTerminal::StaticClass(), M, 300.0f, +1);
		SpawnPropAt(K(EMarsPropKind::GlassPartition), M, FVector(150.0f, 180.0f, 0.0f), FRotator::ZeroRotator, 1.0f, true);
		SpawnLocker(M, 380.0f, -1, K(EMarsPropKind::MedCabinet), TEXT("MED CABINET"), { { EMarsItemType::IodinePills, 2 }, { EMarsItemType::MethyleneBlueCartridge, 1 } });
		break;

	case EMarsModuleType::StorageCommRelay:
		SpawnStation(ARtgCoreStation::StaticClass(), M, -300.0f, -1);
		SpawnStation(AFoodLockerStation::StaticClass(), M, 260.0f, -1);
		SpawnStation(ACommRelayTerminal::StaticClass(), M, -120.0f, +1);
		SpawnStation(ADishAlignmentTerminal::StaticClass(), M, 150.0f, +1);
		SpawnStation(AMarsSaveTerminal::StaticClass(), M, -440.0f, -1);
		SpawnProp(K(EMarsPropKind::ServerRack), M, -60.0f, -1, 40.0f);
		SpawnProp(K(EMarsPropKind::ServerRack), M, 40.0f, -1, 40.0f);
		// Engineering corner: RTG + grid routing. Supplies on racks and in lockers (story: "Supply" lockers).
		SpawnLocker(M, 380.0f, +1, K(EMarsPropKind::CargoRack), TEXT("CARGO RACK A"), { { EMarsItemType::QuantumTransistor, 1 }, { EMarsItemType::SpareParts, 2 } }, TEXT("Supply"));
		SpawnLocker(M, -180.0f, -1, K(EMarsPropKind::WallLocker), TEXT("RATIONS"), { { EMarsItemType::WaterPouch, 2 }, { EMarsItemType::FoodRation, 2 } }, TEXT("Supply"));
		SpawnLocker(M, 150.0f, -1, K(EMarsPropKind::WallLocker), TEXT("ARMORY"), { { EMarsItemType::Ammo9mm, 33 }, { EMarsItemType::SealantCartridge, 2 } }, TEXT("Supply"));
		{
			// Grid routing terminal next to the RTG, on whichever wall is free of the duct door.
			const bool bDoorClash = M->bRightDoorway && FMath::Abs(M->RightDoorwayOffset + 330.0f) < 220.0f;
			SpawnStation(AMarsPowerRoutingTerminal::StaticClass(), M, bDoorClash ? 260.0f : -330.0f, +1);
		}
		break;

	case EMarsModuleType::RingCorridor:
		SpawnProp(K(EMarsPropKind::ValveWheel), M, FMath::Min(120.0f, HL - 60.0f), -1, 12.0f);
		break;

	case EMarsModuleType::CrewQuarters:
		// The bunk you sleep in, a second (empty) bunk, personal lockers.
		SpawnStationAt(AMarsBunkStation::StaticClass(), M, FVector(-HL + 200.0f, -HW + 58.0f, 0.0f), 0.0f);
		SpawnPropAt(K(EMarsPropKind::Bunk), M, FVector(-HL + 450.0f, -HW + 58.0f, 0.0f), FRotator::ZeroRotator, 1.0f, true);
		SpawnLocker(M, -HL + 160.0f, +1, K(EMarsPropKind::WallLocker), TEXT("PERSONAL LOCKER"), { { EMarsItemType::FoodRation, 1 }, { EMarsItemType::WaterPouch, 1 }, { EMarsItemType::IodinePills, 1 } });
		SpawnLocker(M, -HL + 270.0f, +1, K(EMarsPropKind::WallLocker), TEXT("CREW LOCKER (EVACUATED)"), {});
		SpawnStation(AModuleStatusPanel::StaticClass(), M, 120.0f, +1);
		SpawnStation(AMarsRestTerminal::StaticClass(), M, 300.0f, +1);
		break;

	default:
		break;
	}
}

// ----------------------------------------------------------------------------
// Layout
// ----------------------------------------------------------------------------

void AMarsStationGenerator::GenerateStation()
{
	ClearStation();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto MakePlan = [](UClass* Class, EMarsModuleType Type, const FVector& Size, const FString& Label)
	{
		FPlan P;
		P.Class = Class;
		P.Type = Type;
		P.Size = Size;
		P.Label = Label;
		return P;
	};
	int32 CorridorIndex = 1;
	auto Corridor = [&](float InteriorLength)
	{
		return MakePlan(AMarsBaseModule::StaticClass(), EMarsModuleType::RingCorridor,
			FVector(FMath::Max(InteriorLength, 120.0f), CorridorSize.Y, CorridorSize.Z), FString::Printf(TEXT("RING CORRIDOR R-%d"), CorridorIndex++));
	};

	const FPlan Eva  = MakePlan(AMarsBaseModule::StaticClass(), EMarsModuleType::EvaPrep, FVector(800, 600, 320), TEXT("[SEC-02] EVA PREP BAY"));
	const FPlan Dome = MakePlan(ABioDomeModule::StaticClass(), EMarsModuleType::AeroponicBioDome, FVector(1400, 900, 450), TEXT("[SEC-03] AEROPONIC BIO-DOME"));
	const FPlan Xeno = MakePlan(AMarsBaseModule::StaticClass(), EMarsModuleType::XenobiologyLab, FVector(1000, 600, 320), TEXT("[SEC-04] XENOBIOLOGY LAB"));
	const FPlan Med  = MakePlan(AMedicalBayModule::StaticClass(), EMarsModuleType::MedicalBay, FVector(900, 600, 320), TEXT("[SEC-05] MED-BAY"));
	const FPlan Comm = MakePlan(ACommRelayModule::StaticClass(), EMarsModuleType::StorageCommRelay, FVector(1000, 700, 340), TEXT("[SEC-06] STORAGE & COMM-RELAY"));
	FPlan Lock       = MakePlan(AAirlockModule::StaticClass(), EMarsModuleType::AirlockDecon, FVector(500, 400, 300), TEXT("[SEC-01] AIRLOCK & DECON"));

	// Sides of the ring (travel yaw): A +X (0), B +Y (90), C -X (180), D -Y (270).
	TArray<FPlan> Sides[4] = { { Eva, Dome }, { Xeno }, { Med, Comm }, {} };
	auto SideLength = [](const TArray<FPlan>& Side)
	{
		float L = 0.0f;
		for (const FPlan& S : Side) { L += S.Size.X + 2.0f * WallT; }
		return L;
	};
	const float LA = SideLength(Sides[0]);
	const float LC = SideLength(Sides[2]);
	if (FMath::Abs(LA - LC) > 1.0f)
	{
		(LA < LC ? Sides[0] : Sides[2]).Add(Corridor(FMath::Abs(LA - LC) - 2.0f * WallT));
	}
	Sides[3].Add(MakePlan(AMarsBaseModule::StaticClass(), EMarsModuleType::CrewQuarters, FVector(SideLength(Sides[1]) - 2.0f * WallT, 600.0f, 320.0f), TEXT("[SEC-07] CREW QUARTERS")));
	const float SideAB = FMath::Max(SideLength(Sides[0]), SideLength(Sides[2]));
	const float SideBD = FMath::Max(SideLength(Sides[1]), SideLength(Sides[3]));

	const float J = JunctionSize;
	const float W = J + 2.0f * WallT + SideAB;
	const float H = J + 2.0f * WallT + SideBD;
	const FVector Origin = GetActorLocation();
	const float Yaws[4] = { 0.0f, 90.0f, 180.0f, 270.0f };

	FVector JC[4];
	JC[0] = Origin + FVector(-W * 0.5f, -H * 0.5f, 0.0f);
	JC[1] = JC[0] + DirFromYaw(Yaws[0]) * W;
	JC[2] = JC[1] + DirFromYaw(Yaws[1]) * H;
	JC[3] = JC[2] + DirFromYaw(Yaws[2]) * W;

	// ---- Pass 1: plan every ring segment ----
	TArray<FPlan> Ring;
	for (int32 s = 0; s < 4; ++s)
	{
		FPlan JP = MakePlan(AMarsBaseModule::StaticClass(), EMarsModuleType::RingCorridor, FVector(J, J, 320), FString::Printf(TEXT("JUNCTION J-%d"), s + 1));
		JP.Center = JC[s];
		JP.Yaw = Yaws[(s + 3) % 4];
		JP.bFront = (s == 0); // airlock spur
		JP.bBack = true;
		JP.bRight = true;
		JP.bJunction = true;
		JP.JunctionIndex = s;
		Ring.Add(JP);

		const FVector Dir = DirFromYaw(Yaws[s]);
		FVector Cursor = JC[s] + Dir * (J * 0.5f + WallT);
		for (FPlan Seg : Sides[s])
		{
			Seg.Center = Cursor + Dir * (Seg.Size.X * 0.5f + WallT);
			Seg.Yaw = Yaws[s];
			Cursor = Seg.Center + Dir * (Seg.Size.X * 0.5f + WallT);
			Ring.Add(Seg);
		}
	}

	// Doorway signage: name of the next / previous sector around the ring.
	for (int32 i = 0; i < Ring.Num(); ++i)
	{
		const FString& Next = Ring[(i + 1) % Ring.Num()].Label;
		const FString& Prev = Ring[(i - 1 + Ring.Num()) % Ring.Num()].Label;
		FPlan& P = Ring[i];
		if (P.bJunction)
		{
			P.BackSign = TEXT("< ") + Prev;
			P.RightSign = Next + TEXT("  >");
			P.LeftSign = P.Label;
			P.FrontSign = P.bFront ? TEXT("SEC-01 AIRLOCK / SURFACE  >") : TEXT("");
		}
		else
		{
			P.FrontSign = Next + TEXT("  >");
			P.BackSign = TEXT("< ") + Prev;
		}
	}

	// ---- Maintenance duct: Bio-Dome <-> Storage & Comm across the courtyard ----
	FPlan* DomePlan = Ring.FindByPredicate([](const FPlan& P) { return P.Type == EMarsModuleType::AeroponicBioDome; });
	FPlan* CommPlan = Ring.FindByPredicate([](const FPlan& P) { return P.Type == EMarsModuleType::StorageCommRelay; });
	FPlan Duct;
	bool bHasDuct = false;
	if (DomePlan && CommPlan)
	{
		const float DomeLo = DomePlan->Center.X - DomePlan->Size.X * 0.5f, DomeHi = DomePlan->Center.X + DomePlan->Size.X * 0.5f;
		const float CommLo = CommPlan->Center.X - CommPlan->Size.X * 0.5f, CommHi = CommPlan->Center.X + CommPlan->Size.X * 0.5f;
		const float Lo = FMath::Max(DomeLo, CommLo) + 100.0f, Hi = FMath::Min(DomeHi, CommHi) - 100.0f;
		if (Hi > Lo)
		{
			const float DuctX = (Lo + Hi) * 0.5f;
			const FVector2D DuctDoor(DuctSize.X - 20.0f, DuctSize.Y - 15.0f);
			DomePlan->bRight = true;                                   // dome yaw 0: local +Y faces the courtyard
			DomePlan->RightOffset = DuctX - DomePlan->Center.X;
			DomePlan->SideDoorSize = DuctDoor;
			DomePlan->RightSign = TEXT("MAINT DUCT M-1  >  SEC-06");
			CommPlan->bRight = true;                                   // comm yaw 180: local +Y faces the courtyard
			CommPlan->RightOffset = -(DuctX - CommPlan->Center.X);
			CommPlan->SideDoorSize = DuctDoor;
			CommPlan->RightSign = TEXT("MAINT DUCT M-1  >  SEC-03");

			const float DomeFace = DomePlan->Center.Y + DomePlan->Size.Y * 0.5f + WallT;
			const float CommFace = CommPlan->Center.Y - CommPlan->Size.Y * 0.5f - WallT;
			Duct = MakePlan(AMarsBaseModule::StaticClass(), EMarsModuleType::RingCorridor,
				FVector(CommFace - DomeFace - 2.0f * WallT, DuctSize.X, DuctSize.Y), TEXT("MAINTENANCE DUCT M-1"));
			Duct.Center = FVector(DuctX, (DomeFace + CommFace) * 0.5f, Origin.Z);
			Duct.Yaw = 90.0f;
			Duct.DoorSize = DuctDoor;
			Duct.FrontSign = TEXT("SEC-06 COMM >");
			Duct.BackSign = TEXT("< SEC-03 DOME");
			Duct.bDuct = true;
			bHasDuct = Duct.Size.X > 100.0f;
		}
	}

	// ---- Pass 2: spawn ----
	AMarsBaseModule* Airlock = nullptr;
	TArray<AMarsBaseModule*> RingModules;
	for (const FPlan& P : Ring)
	{
		AMarsBaseModule* Module = SpawnModule(P);
		RingModules.Add(Module);
		if (P.bJunction)
		{
			static const EStatusPanelMode PanelModes[4] = { EStatusPanelMode::LifeSupport, EStatusPanelMode::GridBreaker, EStatusPanelMode::ThermalControl, EStatusPanelMode::ModuleGauges };
			if (AModuleStatusPanel* Panel = Cast<AModuleStatusPanel>(SpawnStation(AModuleStatusPanel::StaticClass(), Module, 0.0f, -1)))
			{
				Panel->Mode = PanelModes[P.JunctionIndex];
			}
			AddDetail(Module, false);

			if (P.bFront && Module)
			{
				// SEC-01 Airlock spur with a viewport window in its -Y wall.
				const FVector Out = DirFromYaw(P.Yaw);
				Lock.Center = P.Center + Out * (J * 0.5f + WallT + Lock.Size.X * 0.5f + WallT);
				Lock.Yaw = P.Yaw;
				Lock.bLeftWindow = true;
				Lock.FrontSign = TEXT("SURFACE EXIT");
				Lock.BackSign = TEXT("< RING / JUNCTION J-1");
				Lock.RightSign = Lock.Label;
				Airlock = SpawnModule(Lock);
				PopulateModule(Airlock, false);
				AirlockExteriorPoint = Lock.Center + Out * (Lock.Size.X * 0.5f + WallT + 400.0f);
				SpawnReconPad(Airlock);

				if (Airlock)
				{
					// Exterior stencil above the outer hatch.
					const FVector SignPos = Lock.Center + Out * (Lock.Size.X * 0.5f + WallT + 3.0f) + FVector(0, 0, 270.0f);
					if (ATextRenderActor* Sign = World->SpawnActor<ATextRenderActor>(SignPos, FRotator(0.0f, P.Yaw, 0.0f)))
					{
						Sign->GetTextRender()->SetText(FText::FromString(TEXT("ARES-IV  //  SEC-01 AIRLOCK")));
						Sign->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
						Sign->GetTextRender()->SetWorldSize(34.0f);
						Sign->GetTextRender()->SetTextRenderColor(FColor(255, 176, 48));
						Track(Sign);
					}

					// The player wakes up suited in the airlock, facing the viewport.
					if (bSpawnPlayerStart)
					{
						const FTransform StartXf = FTransform(FRotator(0.0f, -90.0f, 0.0f), FVector(-60.0f, 70.0f, 100.0f)) * Airlock->GetActorTransform();
						if (APlayerStart* Start = World->SpawnActor<APlayerStart>(StartXf.GetLocation(), StartXf.Rotator()))
						{
							Track(Start);
						}
					}

					// Shuttle on its pad, ~42 m beyond the viewport.
					const FVector ShuttlePos = (FTransform(FVector(0.0f, -4200.0f, 0.0f)) * Airlock->GetActorTransform()).GetLocation() + FVector(0, 0, 260.0f);
					const FVector Away = (ShuttlePos - Airlock->GetActorLocation()).GetSafeNormal2D();
					const FTransform ShuttleXf(FRotator(0.0f, Away.Rotation().Yaw, 0.0f), ShuttlePos);
					if (AMarsShuttle* Shuttle = World->SpawnActorDeferred<AMarsShuttle>(AMarsShuttle::StaticClass(), ShuttleXf, this))
					{
						Shuttle->FinishSpawning(ShuttleXf);
						Track(Shuttle);
					}
				}
			}
		}
		else
		{
			PopulateModule(Module, false);
		}

		if (Module && P.Type == EMarsModuleType::StorageCommRelay)
		{
			const FVector RoofPos = Module->GetActorLocation() + FVector(0, 0, P.Size.Z + WallT);
			const FTransform DishXf(FRotator(0, P.Yaw + 90.0f, 0), RoofPos);
			if (AMarsPropBuilder* Dish = World->SpawnActorDeferred<AMarsPropBuilder>(AMarsPropBuilder::StaticClass(), DishXf, this))
			{
				Dish->Kind = EMarsPropKind::Dish;
				Dish->Scale = 1.5f;
				Dish->FinishSpawning(DishXf);
				Track(Dish);
			}
		}
	}

	// ---- Wayfinding: glowing wall arrows toward the key sectors on every ring segment ----
	{
		TArray<int32> Idx;
		TArray<FString> Names;
		TArray<FLinearColor> Colors;
		auto AddTarget = [&](TFunctionRef<bool(const FPlan&)> Pred, const TCHAR* Name, const FLinearColor& Color)
		{
			for (int32 i = 0; i < Ring.Num(); ++i)
			{
				if (Pred(Ring[i]))
				{
					Idx.Add(i);
					Names.Add(Name);
					Colors.Add(Color);
					return;
				}
			}
		};
		AddTarget([](const FPlan& P) { return P.bJunction && P.bFront; }, TEXT("AIRLOCK / SHUTTLE PAD"), FLinearColor(1.0f, 0.55f, 0.12f));
		AddTarget([](const FPlan& P) { return P.Type == EMarsModuleType::MedicalBay; }, TEXT("MED-BAY"), FLinearColor(0.75f, 0.95f, 1.0f));
		AddTarget([](const FPlan& P) { return P.Type == EMarsModuleType::CrewQuarters; }, TEXT("CREW QUARTERS"), FLinearColor(1.0f, 0.72f, 0.45f));
		AddTarget([](const FPlan& P) { return P.Type == EMarsModuleType::StorageCommRelay; }, TEXT("ENGINEERING / COMM"), FLinearColor(0.9f, 0.25f, 1.0f));
		PlaceNavigationArrows(RingModules, Idx, Names, Colors);
	}

	if (bHasDuct)
	{
		AMarsBaseModule* DuctModule = SpawnModule(Duct);
		PopulateModule(DuctModule, true);
	}

	UE_LOG(LogMarsSurvival, Log, TEXT("Ares-IV generated: %d actors, ring %.0f x %.0f m, maintenance duct: %s"),
		GeneratedActors.Num(), W / 100.0f, H / 100.0f, bHasDuct ? TEXT("yes") : TEXT("no"));
}

// ----------------------------------------------------------------------------
// Sector dressing, lockers, recon pad
// ----------------------------------------------------------------------------

AActor* AMarsStationGenerator::SpawnPropTinted(uint8 PropKind, AMarsBaseModule* Module, const FVector& LocalPos, const FRotator& LocalRot, const FLinearColor& Tint, const FVector& Extent, bool bCollision)
{
	if (!Module)
	{
		return nullptr;
	}
	const FTransform World = FTransform(LocalRot, LocalPos) * Module->GetActorTransform();
	AMarsPropBuilder* Prop = GetWorld()->SpawnActorDeferred<AMarsPropBuilder>(AMarsPropBuilder::StaticClass(), World, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Prop)
	{
		Prop->Kind = static_cast<EMarsPropKind>(PropKind);
		Prop->bCollision = bCollision;
		Prop->Tint = Tint;
		Prop->Extent = Extent;
		Prop->FinishSpawning(World);
		Track(Prop);
	}
	return Prop;
}

AActor* AMarsStationGenerator::SpawnLocker(AMarsBaseModule* Module, float LocalX, int32 WallSide, uint8 PropKind, const FString& Name, const TArray<FMarsInventorySlot>& Contents, FName Tag)
{
	if (!Module)
	{
		return nullptr;
	}
	// Dodge side doorways.
	const bool bDoor = WallSide < 0 ? (Module->bLeftDoorway || Module->bLeftWindow) : Module->bRightDoorway;
	const float DoorX = WallSide < 0 ? Module->LeftDoorwayOffset : Module->RightDoorwayOffset;
	if (bDoor && FMath::Abs(LocalX - DoorX) < 190.0f)
	{
		LocalX = DoorX + (LocalX >= DoorX ? 200.0f : -200.0f);
		LocalX = FMath::Clamp(LocalX, -Module->RoomSize.X * 0.5f + 80.0f, Module->RoomSize.X * 0.5f - 80.0f);
	}
	const EMarsPropKind Kind = static_cast<EMarsPropKind>(PropKind);
	const float Depth = Kind == EMarsPropKind::CargoRack ? 70.0f : Kind == EMarsPropKind::MedCabinet ? 40.0f : 50.0f;
	const float Y = WallSide * (Module->RoomSize.Y * 0.5f - Depth * 0.5f - 6.0f);
	const FTransform World = FTransform(FRotator(0.0f, WallSide < 0 ? 90.0f : -90.0f, 0.0f), FVector(LocalX, Y, 0.0f)) * Module->GetActorTransform();
	AMarsStorageLocker* Locker = GetWorld()->SpawnActorDeferred<AMarsStorageLocker>(AMarsStorageLocker::StaticClass(), World, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Locker)
	{
		Locker->TargetModule = Module;
		Locker->PropKind = Kind;
		Locker->StationName = Name;
		Locker->Contents = Contents;
		if (Tag != NAME_None)
		{
			Locker->Tags.Add(Tag);
		}
		Locker->FinishSpawning(World);
		Track(Locker);
	}
	return Locker;
}

void AMarsStationGenerator::DecorateSector(AMarsBaseModule* M)
{
	if (!M)
	{
		return;
	}
	const float L = M->RoomSize.X, W = M->RoomSize.Y, H = M->RoomSize.Z;
	const FLinearColor C = M->SectorColor;
	const EMarsModuleType T = M->ModuleType;
	const bool bIndustrial = T == EMarsModuleType::AirlockDecon || T == EMarsModuleType::EvaPrep || T == EMarsModuleType::StorageCommRelay;
	const bool bMed = T == EMarsModuleType::MedicalBay;
	const bool bJunction = L <= JunctionSize + 1.0f && W <= JunctionSize + 1.0f;
	const float Chamfer = H > 400.0f ? 60.0f : 42.0f;

	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const FRotator Face(0.0f, Side < 0 ? 0.0f : 180.0f, 0.0f); // prop +Y faces the room
		const float WallY = Side * (W * 0.5f - 3.0f);
		const bool bDoor = Side < 0 ? (M->bLeftDoorway || M->bLeftWindow) : M->bRightDoorway;

		// Angled gunmetal panel where wall meets ceiling, carrying the sector light strip (reference profile).
		SpawnPropTinted(K(EMarsPropKind::CeilingChamfer), M, FVector(0.0f, Side * (W * 0.5f), H), Face, C, FVector(L - 4.0f, Chamfer, 10.0f));

		// Wall cladding where the wall is solid: padded white in the Med-Bay, recessed modular panels elsewhere.
		if (!bDoor && !bJunction)
		{
			SpawnPropTinted(K(bMed ? EMarsPropKind::PaddedPanels : EMarsPropKind::WallPanels), M, FVector(0.0f, WallY - Side * 2.0f, 0.0f), Face, C,
				FVector(L - 80.0f, 4.0f, FMath::Min(H - Chamfer - 30.0f, 230.0f)));
		}
		// Utility boxes high on the wall, clear of lockers and screens.
		if (!bMed)
		{
			for (int32 k = -1; k <= 1; k += 2)
			{
				const float X = k * (L * 0.5f - 45.0f);
				const float DoorX = Side < 0 ? M->LeftDoorwayOffset : M->RightDoorwayOffset;
				if (bDoor && FMath::Abs(X - DoorX) < 160.0f)
				{
					continue;
				}
				SpawnPropTinted(K(EMarsPropKind::CircuitBox), M, FVector(X, WallY - Side * 1.0f, H - Chamfer - 60.0f), Face, C, FVector(1.0f));
			}
		}
		if (bIndustrial)
		{
			SpawnPropTinted(K(EMarsPropKind::HazardStripe), M, FVector(0.0f, WallY - Side * 6.0f, 22.0f), Face, C, FVector(L - 60.0f, 2.0f, 22.0f));
		}
	}

	// Ceiling: recessed octagonal lights; cable bundle tucked against the ceiling.
	const int32 Lights = L > 700.0f ? 3 : 1;
	for (int32 i = 0; i < Lights; ++i)
	{
		const float X = Lights == 1 ? 0.0f : (-L * 0.33f + i * L * 0.33f);
		SpawnPropTinted(K(EMarsPropKind::CeilingLight), M, FVector(X, 0.0f, H), FRotator::ZeroRotator, C, FVector(1.0f));
	}
	SpawnPropTinted(K(EMarsPropKind::CableBundle), M, FVector(0.0f, W * 0.22f, H - 12.0f), FRotator::ZeroRotator, C, FVector(L - 140.0f, 30.0f, 30.0f));

	// Floor: diamond-plate walkway down the middle (not in the padded, tiled Med-Bay).
	if (!bMed)
	{
		SpawnPropTinted(K(EMarsPropKind::FloorRunner), M, FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator, C, FVector(L - 20.0f, FMath::Min(W * 0.4f, 140.0f), 2.0f));
	}
	if (bIndustrial || T == EMarsModuleType::RingCorridor)
	{
		SpawnPropTinted(K(EMarsPropKind::SteamVent), M, FVector(L * 0.25f, -W * 0.5f + 4.0f, 60.0f), FRotator(0.0f, 90.0f, 0.0f), C, FVector(100.0f));
	}
}

void AMarsStationGenerator::SpawnReconPad(AMarsBaseModule* Airlock)
{
	if (!Airlock)
	{
		return;
	}
	UWorld* World = GetWorld();
	// Pad 2 sits beside the airlock exit, away from the intro shuttle's pad (which is on the viewport side).
	const FTransform PadXf = FTransform(FVector(Airlock->RoomSize.X * 0.5f + 1900.0f, 2400.0f, 0.0f)) * Airlock->GetActorTransform();
	FVector PadPos = PadXf.GetLocation();
	PadPos.Z = 0.0f;
	ReconPadLocation = PadPos;
	const FTransform PadWorld(FRotator(0.0f, Airlock->GetActorRotation().Yaw, 0.0f), PadPos);
	if (AMarsPropBuilder* Pad = World->SpawnActorDeferred<AMarsPropBuilder>(AMarsPropBuilder::StaticClass(), PadWorld, this))
	{
		Pad->Kind = EMarsPropKind::LandingPad;
		Pad->bCollision = true;
		Pad->FinishSpawning(PadWorld);
		Track(Pad);
	}
	const FTransform ShipXf(FRotator(0.0f, Airlock->GetActorRotation().Yaw + 90.0f, 0.0f), PadPos + FVector(0.0f, 0.0f, 320.0f));
	if (AMarsReconShuttle* Ship = World->SpawnActorDeferred<AMarsReconShuttle>(AMarsReconShuttle::StaticClass(), ShipXf, this))
	{
		Ship->DockLocation = PadPos;
		Ship->FinishSpawning(ShipXf);
		Track(Ship);
	}
	// Exterior stencil.
	if (ATextRenderActor* Sign = World->SpawnActor<ATextRenderActor>(PadPos + FVector(0.0f, 0.0f, 40.0f) + PadWorld.GetRotation().GetRightVector() * 900.0f, FRotator(0.0f, PadWorld.Rotator().Yaw - 90.0f, 0.0f)))
	{
		Sign->GetTextRender()->SetText(FText::FromString(TEXT("PAD 2  //  RS-2 RECON")));
		Sign->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
		Sign->GetTextRender()->SetWorldSize(60.0f);
		Sign->GetTextRender()->SetTextRenderColor(FColor(255, 176, 48));
		Track(Sign);
	}
}

void AMarsStationGenerator::PlaceNavigationArrows(const TArray<AMarsBaseModule*>& RingModules, const TArray<int32>& TargetIdx, const TArray<FString>& TargetNames, const TArray<FLinearColor>& TargetColors)
{
	// Airport-style destination boards mounted flat on the end wall ABOVE each ring doorway:
	// "^ MED-BAY" means "through this door". Nothing on the side walls (pipes, lockers, screens live there).
	UWorld* World = GetWorld();
	const int32 N = RingModules.Num();
	for (int32 i = 0; i < N; ++i)
	{
		AMarsBaseModule* M = RingModules[i];
		if (!M || M->RoomSize.X < 300.0f)
		{
			continue;
		}
		const float L = M->RoomSize.X, H = M->RoomSize.Z;
		const float DoorTop = M->DoorwaySize.Y;
		const float Space = H - DoorTop;
		if (Space < 40.0f)
		{
			continue;
		}
		for (int32 End = -1; End <= 1; End += 2)
		{
			if ((End > 0 && !M->bFrontDoorway) || (End < 0 && !M->bBackDoorway))
			{
				continue;
			}
			TArray<int32> Rows;
			for (int32 t = 0; t < TargetIdx.Num(); ++t)
			{
				const int32 Goal = TargetIdx[t];
				if (Goal == i)
				{
					continue;
				}
				const int32 Fwd = (Goal - i + N) % N;
				const int32 Back = (i - Goal + N) % N;
				if ((Fwd <= Back) == (End > 0))
				{
					Rows.Add(t);
				}
			}
			if (Rows.Num() == 0)
			{
				continue;
			}
			// Board frame: wall plane at X = End * (L/2 - 2), facing back into the room.
			const float RowH = FMath::Min(14.0f, (Space - 14.0f) / Rows.Num());
			const float BoardH = RowH * Rows.Num() + 8.0f;
			const float BoardW = 210.0f;
			const float CenterZ = DoorTop + 6.0f + BoardH * 0.5f;
			const float WallX = End * (L * 0.5f - 2.0f);
			const float FaceYaw = End > 0 ? 90.0f : -90.0f;    // prop +Y -> into the room
			const FVector Right = End > 0 ? FVector(0, 1, 0) : FVector(0, -1, 0); // viewer's right on this wall
			SpawnPropTinted(K(EMarsPropKind::NavSignPanel), M, FVector(WallX, 0.0f, CenterZ), FRotator(0.0f, FaceYaw, 0.0f), M->SectorColor, FVector(BoardW, 2.0f, BoardH));
			for (int32 r = 0; r < Rows.Num(); ++r)
			{
				const int32 t = Rows[r];
				const float Z = CenterZ + BoardH * 0.5f - 4.0f - RowH * (r + 0.5f);
				const FVector ChevronPos = FVector(WallX - End * 2.5f, 0.0f, Z) + Right * (-BoardW * 0.5f + 14.0f);
				SpawnPropTinted(K(EMarsPropKind::NavChevronUp), M, ChevronPos, FRotator(0.0f, FaceYaw, 0.0f), TargetColors[t], FVector(1.0f));
				const FVector LabelLocal = FVector(WallX - End * 4.0f, 0.0f, Z - RowH * 0.32f) + Right * (-BoardW * 0.5f + 28.0f);
				const FTransform LabelXf = FTransform(FRotator(0.0f, End > 0 ? 180.0f : 0.0f, 0.0f), LabelLocal) * M->GetActorTransform();
				if (ATextRenderActor* Label = World->SpawnActor<ATextRenderActor>(LabelXf.GetLocation(), LabelXf.Rotator()))
				{
					Label->GetTextRender()->SetText(FText::FromString(TargetNames[t]));
					Label->GetTextRender()->SetHorizontalAlignment(EHTA_Left);
					Label->GetTextRender()->SetVerticalAlignment(EVRTA_TextBottom);
					Label->GetTextRender()->SetWorldSize(RowH * 0.75f);
					Label->GetTextRender()->SetTextRenderColor(TargetColors[t].ToFColor(true));
					Track(Label);
				}
			}
		}
	}
}
