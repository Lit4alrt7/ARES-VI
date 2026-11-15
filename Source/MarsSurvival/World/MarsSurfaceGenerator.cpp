#include "World/MarsSurfaceGenerator.h"

#include "MarsSurvival.h"
#include "Items/MarsItemTypes.h"
#include "Props/MarsPropBuilder.h"
#include "Stations/MarsStoryStations.h"
#include "World/MarsTerrainGenerator.h"
#include "World/RadiationHazardZone.h"
#include "World/SurfaceBeacon.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"

namespace
{
	const TCHAR* LogRelay =
		TEXT("Relay R-3 flight recorder, final frames, Sol 97:\n")
		TEXT("> CMD 0x4F: DEORBIT BURN - AUTH: MARS DIRECTORATE / OVERSIGHT\n")
		TEXT("> CMD 0x51: PURGE ARES-IV UPLINK KEYS\n")
		TEXT("> NOTE (ops): 'Evacuation shuttle departs with five. Miller stays. Keep him talking to the AI, not to Earth.'\n")
		TEXT("The relay never burned up. Someone aimed it at the valley.");
	const TCHAR* LogDrill =
		TEXT("Ares-III drill log, core 14, depth 140.2 m:\n")
		TEXT("Brine lens breached at 139 m. Core warm to the touch. Fluorescence under UV - the cores glow when the lights are off.\n")
		TEXT("Hasegawa reports headaches. Then the dreams. Then he says the dreams are talking.\n")
		TEXT("Directorate reply: 'Continue sampling. Seal the cores. Do not report outside the program.'");
	const TCHAR* LogOutpost =
		TEXT("Ares-III commander's log, final entry (decrypted):\n")
		TEXT("They are building Ares-IV forty kilometres downwind of our drill site. I asked why. 'Long-term exposure study.'\n")
		TEXT("It is not a station. It is a petri dish with a crew in it.\n")
		TEXT("If you are reading this from Ares-IV: the quarantine is not there to protect Earth from you. It is there to keep you from telling them.");
}

AMarsSurfaceGenerator::AMarsSurfaceGenerator()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

float AMarsSurfaceGenerator::Ground(AMarsTerrainGenerator* Terrain, float X, float Y) const
{
	return Terrain ? Terrain->GetHeightAtWorld(X, Y) : 0.0f;
}

AActor* AMarsSurfaceGenerator::SpawnProp(uint8 Kind, const FVector& Location, float Yaw, bool bCollision, const FLinearColor& Tint, const FVector& Extent)
{
	const FTransform Xf(FRotator(0.0f, Yaw, 0.0f), Location);
	AMarsPropBuilder* Prop = GetWorld()->SpawnActorDeferred<AMarsPropBuilder>(AMarsPropBuilder::StaticClass(), Xf, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Prop)
	{
		Prop->Kind = static_cast<EMarsPropKind>(Kind);
		Prop->bCollision = bCollision;
		Prop->Tint = Tint;
		Prop->Extent = Extent;
		Prop->FinishSpawning(Xf);
		GeneratedActors.Add(Prop);
	}
	return Prop;
}

void AMarsSurfaceGenerator::SpawnPOI(AMarsTerrainGenerator* Terrain, const FVector2D& Where, uint8 Kind, const FString& BeaconLabel, FName Tag, FName Event,
	const FString& LockerName, const TArray<FMarsInventorySlot>& Contents, const FString& LogTitle, const FString& LogText, float HazardMsvH, float BioExposure)
{
	UWorld* World = GetWorld();
	const float Z = Ground(Terrain, Where.X, Where.Y);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Where.Y, -Where.X)); // face the base
	SpawnProp(Kind, FVector(Where.X, Where.Y, Z), Yaw, true);

	// Survey mast + gameplay beacon (shows on the visor compass / shuttle radar).
	const FVector2D Side = FVector2D(-Where.Y, Where.X).GetSafeNormal() * 1200.0f;
	const FVector MastPos(Where.X + Side.X, Where.Y + Side.Y, Ground(Terrain, Where.X + Side.X, Where.Y + Side.Y));
	SpawnProp(static_cast<uint8>(EMarsPropKind::SurveyBeaconMast), MastPos, Yaw, true, FLinearColor(1.0f, 0.3f, 0.1f));
	const FTransform BeaconXf(FRotator::ZeroRotator, MastPos + FVector(150.0f, 0.0f, 0.0f));
	if (ASurfaceBeacon* Beacon = World->SpawnActorDeferred<ASurfaceBeacon>(ASurfaceBeacon::StaticClass(), BeaconXf, this))
	{
		Beacon->BeaconLabel = BeaconLabel;
		Beacon->BatteryCapacityWh = 1000000.0f;
		Beacon->BatteryWh = 1000000.0f;
		Beacon->RadioRangeCm = 400000.0f;
		Beacon->FinishSpawning(BeaconXf);
		GeneratedActors.Add(Beacon);
	}

	// Recovery locker (cargo rack strapped to the wreck), facing the approach.
	const FVector2D Toward = (-Where).GetSafeNormal() * 900.0f;
	const FVector LockerPos(Where.X + Toward.X, Where.Y + Toward.Y, Ground(Terrain, Where.X + Toward.X, Where.Y + Toward.Y));
	const FTransform LockerXf(FRotator(0.0f, Yaw, 0.0f), LockerPos);
	if (AMarsStorageLocker* Locker = World->SpawnActorDeferred<AMarsStorageLocker>(AMarsStorageLocker::StaticClass(), LockerXf, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Locker->PropKind = EMarsPropKind::CargoRack;
		Locker->StationName = LockerName;
		Locker->Contents = Contents;
		Locker->Tags.Add(Tag);
		Locker->StoryEvent = Event;
		Locker->LogTitle = LogTitle;
		Locker->LogText = LogText;
		Locker->BioExposure = BioExposure;
		Locker->FinishSpawning(LockerXf);
		GeneratedActors.Add(Locker);
	}

	if (HazardMsvH > 0.0f)
	{
		const FTransform HazXf(FRotator::ZeroRotator, FVector(Where.X, Where.Y, Z));
		if (ARadiationHazardZone* Hazard = World->SpawnActorDeferred<ARadiationHazardZone>(ARadiationHazardZone::StaticClass(), HazXf, this))
		{
			Hazard->PeakDoseRateMsvPerHour = HazardMsvH;
			Hazard->StormMultiplier = 4.0f;
			Hazard->FinishSpawning(HazXf);
			GeneratedActors.Add(Hazard);
		}
	}
}

void AMarsSurfaceGenerator::GenerateSurface(AMarsTerrainGenerator* Terrain)
{
	for (AActor* A : GeneratedActors)
	{
		if (A)
		{
			A->Destroy();
		}
	}
	GeneratedActors.Reset();

	// ---- Story points of interest (distances from the base: ~0.8, 1.1 and 1.4 km) ----
	SpawnPOI(Terrain, FVector2D(-62000.0f, 47000.0f), static_cast<uint8>(EMarsPropKind::RelaySatellite), TEXT("ALPHA / R-3"), TEXT("POI_Relay"), TEXT("Log_Relay"),
		TEXT("R-3 BLACK BOX"), { { EMarsItemType::QuantumTransistor, 1 }, { EMarsItemType::SpareParts, 1 } },
		TEXT("DATA LOG // RELAY R-3 FLIGHT RECORDER"), LogRelay, 0.18f, 0.0f);
	SpawnPOI(Terrain, FVector2D(88000.0f, -64000.0f), static_cast<uint8>(EMarsPropKind::IceDrillRig), TEXT("BETA / DRILL"), TEXT("POI_Drill"), TEXT("Log_Drill"),
		TEXT("CORE STORAGE"), { { EMarsItemType::QuantumTransistor, 1 }, { EMarsItemType::BioAgentCulture, 1 }, { EMarsItemType::O2Canister, 1 } },
		TEXT("DATA LOG // ARES-III DRILL CORE 14"), LogDrill, 0.08f, 0.12f);
	SpawnPOI(Terrain, FVector2D(-105000.0f, -86000.0f), static_cast<uint8>(EMarsPropKind::AbandonedOutpost), TEXT("ARES-III"), TEXT("POI_Outpost"), TEXT("Log_Outpost"),
		TEXT("ARES-III STORES"), { { EMarsItemType::SpareParts, 2 }, { EMarsItemType::FoodRation, 2 }, { EMarsItemType::NeuroAntitoxin, 1 } },
		TEXT("DATA LOG // ARES-III COMMANDER"), LogOutpost, 0.0f, 0.0f);

	// ---- Basalt column formations ----
	FRandomStream Rng(Seed);
	const FVector2D Avoid[] = { FVector2D::ZeroVector, FVector2D(-62000.0f, 47000.0f), FVector2D(88000.0f, -64000.0f), FVector2D(-105000.0f, -86000.0f) };
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < 400 && Placed < NumBasaltFormations; ++Attempt)
	{
		const FVector2D P(Rng.FRandRange(-170000.0f, 170000.0f), Rng.FRandRange(-170000.0f, 170000.0f));
		bool bOk = P.Size() > 22000.0f;
		for (int32 i = 1; i < UE_ARRAY_COUNT(Avoid) && bOk; ++i)
		{
			bOk = FVector2D::Distance(P, Avoid[i]) > 9000.0f;
		}
		if (!bOk)
		{
			continue;
		}
		const FVector Pos(P.X, P.Y, Ground(Terrain, P.X, P.Y));
		SpawnProp(static_cast<uint8>(EMarsPropKind::BasaltColumns), Pos, Rng.FRandRange(0.0f, 360.0f), true, FLinearColor::White, FVector(P.X * 0.01f, P.Y * 0.01f, 0.0f));
		++Placed;
	}
	UE_LOG(LogMarsSurvival, Log, TEXT("Surface generated: 3 POIs, %d basalt formations, %d actors"), Placed, GeneratedActors.Num());
}
