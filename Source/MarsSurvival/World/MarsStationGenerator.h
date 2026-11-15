#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Base/MarsModuleTypes.h"
#include "Items/MarsItemTypes.h"
#include "MarsStationGenerator.generated.h"

class AMarsBaseModule;
class AMarsStationBase;

/**
 * Builds Ares-IV as a closed ring-and-hub with a maintenance shortcut:
 *
 *        J-4 ---- [SEC-05 MED-BAY] ---- [SEC-06 STORAGE & COMM] ---- J-3
 *         |                                     ||                      |
 *     RING CORRIDOR R-1               MAINTENANCE DUCT M-1      [SEC-04 XENOLAB]
 *         |                          (crouch, 1.2 m high)               |
 *        J-1 ---- [SEC-02 EVA PREP] ---- [SEC-03 BIO-DOME] ----------- J-2
 *         |
 *   [SEC-01 AIRLOCK] (viewport window -> shuttle pad) -> surface
 *
 * The player starts suited inside the airlock, watching the shuttle depart through the viewport.
 * Every room gets functional stations plus environmental detail (ribs, pipes, cable trays, valves,
 * server racks, glass partitions) and the Bio-Dome / Xenolab get bioluminescent xenoflora.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsStationGenerator : public AActor
{
	GENERATED_BODY()

public:
	AMarsStationGenerator();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Station Generator")
	void GenerateStation();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Station Generator")
	void ClearStation();

	UFUNCTION(BlueprintPure, Category = "Station Generator")
	FVector GetAirlockExteriorPoint() const { return AirlockExteriorPoint; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Generator")
	float JunctionSize = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Generator")
	FVector CorridorSize = FVector(1000.0f, 320.0f, 290.0f);

	/** Maintenance duct cross-section (width, height) - forces a crouch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Generator")
	FVector2D DuctSize = FVector2D(130.0f, 135.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Generator")
	bool bSpawnPlayerStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Generator")
	bool bEnvironmentDetail = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station Generator")
	TArray<TObjectPtr<AActor>> GeneratedActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station Generator")
	FVector AirlockExteriorPoint = FVector::ZeroVector;

	/** Where the recon shuttle docks (centre of landing pad 2). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station Generator")
	FVector ReconPadLocation = FVector::ZeroVector;

	UFUNCTION(BlueprintPure, Category = "Station Generator")
	FVector GetReconPadLocation() const { return ReconPadLocation; }

private:
	AActor* SpawnPropTinted(uint8 PropKind, AMarsBaseModule* Module, const FVector& LocalPos, const FRotator& LocalRot, const FLinearColor& Tint, const FVector& Extent, bool bCollision = false);
	AActor* SpawnLocker(AMarsBaseModule* Module, float LocalX, int32 WallSide, uint8 PropKind, const FString& Name, const TArray<struct FMarsInventorySlot>& Contents, FName Tag = NAME_None);
	void DecorateSector(AMarsBaseModule* M);
	void SpawnReconPad(AMarsBaseModule* Airlock);
	void PlaceNavigationArrows(const TArray<AMarsBaseModule*>& RingModules, const TArray<int32>& TargetIdx, const TArray<FString>& TargetNames, const TArray<FLinearColor>& TargetColors);

	struct FPlan
	{
		UClass* Class = nullptr;
		EMarsModuleType Type = EMarsModuleType::RingCorridor;
		FVector Size = FVector::ZeroVector;
		FString Label;
		FVector Center = FVector::ZeroVector;
		float Yaw = 0.0f;
		bool bFront = true, bBack = true, bLeft = false, bRight = false, bLeftWindow = false;
		float LeftOffset = 0.0f, RightOffset = 0.0f;
		FVector2D DoorSize = FVector2D(160.0f, 230.0f);
		FVector2D SideDoorSize = FVector2D(160.0f, 230.0f);
		FString FrontSign, BackSign, LeftSign, RightSign;
		bool bDuct = false;
		bool bJunction = false;
		int32 JunctionIndex = -1;
	};

	AMarsBaseModule* SpawnModule(const FPlan& Plan);
	AActor* SpawnStation(UClass* StationClass, AMarsBaseModule* Module, float LocalX, int32 WallSide, float ExtraYaw = 0.0f);
	AActor* SpawnStationAt(UClass* StationClass, AMarsBaseModule* Module, const FVector& LocalPos, float LocalYaw);
	AActor* SpawnProp(uint8 PropKind, AMarsBaseModule* Module, float LocalX, int32 WallSide, float Inset, float Scale = 1.0f);
	AActor* SpawnPropAt(uint8 PropKind, AMarsBaseModule* Module, const FVector& LocalPos, const FRotator& LocalRot, float Scale, bool bCollision);
	AActor* SpawnPickup(EMarsItemType Item, int32 Count, AMarsBaseModule* Module, const FVector& LocalPos);
	void PopulateModule(AMarsBaseModule* Module, bool bDuct);
	void AddDetail(AMarsBaseModule* Module, bool bDuct);
	void Track(AActor* Actor);
};
