#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarsTerrainGenerator.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FMarsCrater
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crater")
	FVector2D Center = FVector2D::ZeroVector;

	/** Rim radius in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crater")
	float Radius = 5000.0f;

	/** Bowl depth in cm (fresh simple craters: depth/diameter ~ 0.2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crater")
	float Depth = 1500.0f;
};

/**
 * Procedural low-poly Martian surface (PS1-style faceted shading via per-face vertices):
 * fBm rolling terrain + wind-aligned barchan-like dune ridges + simple bowl craters with
 * raised rims, a flattened landing pad for the base, and instanced boulders.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsTerrainGenerator : public AActor
{
	GENERATED_BODY()

public:
	AMarsTerrainGenerator();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Terrain")
	void Regenerate();

	/** Terrain height (world Z) at a world XY. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	float GetHeightAtWorld(float WorldX, float WorldY) const;

	UFUNCTION(BlueprintPure, Category = "Terrain")
	TArray<FMarsCrater> GetCraters() const { return Craters; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Boulders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	int32 Seed = 2087;

	/** Side length of the square terrain in cm (default 4 km). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "10000.0"))
	float SizeCm = 400000.0f;

	/** Grid cell size in cm - large cells give the low-poly look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "500.0"))
	float CellSizeCm = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float HillAmplitudeCm = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float DuneAmplitudeCm = 600.0f;

	/** Prevailing wind direction (degrees) that orients dune crests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float WindDirectionDegrees = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0"))
	int32 NumCraters = 14;

	/** Flat pad around the actor origin where the base sits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float FlatPadRadiusCm = 9000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0"))
	int32 NumBoulders = 450;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	TSoftObjectPtr<UMaterialInterface> TerrainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	TSoftObjectPtr<UMaterialInterface> BoulderMaterial;

	/** Generate in the editor viewport too (turn off while tweaking huge sizes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	bool bGenerateInEditor = true;

protected:
	virtual void BeginPlay() override;

private:
	float SampleHeightLocal(float X, float Y) const;
	void BuildCraters();
	void BuildMesh();
	void ScatterBoulders();

	UPROPERTY()
	TArray<FMarsCrater> Craters;

	bool bGenerated = false;
};
