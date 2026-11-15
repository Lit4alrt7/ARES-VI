#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarsSurfaceGenerator.generated.h"

class AMarsTerrainGenerator;

/**
 * Open-world dressing of the crater valley around Ares-IV (run once from the map build script, after
 * the terrain and the station): story points of interest with their survey beacons, lockers and data
 * logs (crashed relay satellite R-3 = ALPHA, deep ice-core drill rig = BETA, abandoned ARES-III outpost),
 * radiation hazards, and basalt column formations scattered across the plain.
 */
UCLASS()
class MARSSURVIVAL_API AMarsSurfaceGenerator : public AActor
{
	GENERATED_BODY()

public:
	AMarsSurfaceGenerator();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Surface Generator")
	void GenerateSurface(AMarsTerrainGenerator* Terrain);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Generator")
	int32 Seed = 2087;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Generator")
	int32 NumBasaltFormations = 22;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Generator")
	TArray<TObjectPtr<AActor>> GeneratedActors;

private:
	AActor* SpawnProp(uint8 Kind, const FVector& Location, float Yaw, bool bCollision, const FLinearColor& Tint = FLinearColor(1.0f, 0.62f, 0.1f), const FVector& Extent = FVector(400.0f));
	void SpawnPOI(AMarsTerrainGenerator* Terrain, const FVector2D& Where, uint8 Kind, const FString& BeaconLabel, FName Tag, FName Event,
		const FString& LockerName, const TArray<struct FMarsInventorySlot>& Contents, const FString& LogTitle, const FString& LogText, float HazardMsvH, float BioExposure);
	float Ground(AMarsTerrainGenerator* Terrain, float X, float Y) const;
};
