#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/MarsItemTypes.h"
#include "MarsPropBuilder.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EMarsPropShape : uint8
{
	Box,
	Cylinder,
	Sphere,
	Cone
};

UENUM(BlueprintType)
enum class EMarsPropKind : uint8
{
	None,
	// Hand-held items / pickups (real-world scale, origin at centre)
	GlockModel,
	TorqueWrench,
	WeldGun,
	Dosimeter,
	QuantumTransistor,
	SyringeMethyleneBlue,
	SyringeAntitoxin,
	AutoInjectorAmifostine,
	KIBlister,
	O2Canister,
	SampleContainer,
	RockSample,
	BioCulture,
	WaterPouch,
	FoodRation,
	ChlorellaTube,
	SpareParts,
	BeaconKit,
	AmmoBox,
	SealantCartridge,
	// Station furniture (origin on the floor, front faces +X)
	Terminal,
	PhotobioreactorRack,
	RtgCore,
	Dish,
	ScannerBed,
	Spectrometer,
	Microscope,
	ContainmentBox,
	SuitRack,
	WaterDispenser,
	FoodLocker,
	O2RefillRack,
	Crate,
	AirlockValve,
	ScrubberUnit,
	// Environmental detail (origin on the floor / at the mount point)
	PipeRun,            // 4 m of exposed cooling pipes along +X
	CableTray,          // 4 m overhead cable tray along +X
	ValveWheel,         // wall valve with a rotating hand-wheel
	ServerRack,         // blinking data/telemetry rack
	StructuralRib,      // 3 m wall frame rib
	GlassPartition,     // frosted glass partition, 2 m wide
	// Scavengers-Reign-style xenoflora (bioluminescent, reacts to light and radiation)
	BioFlora,
	SporePods,
	Mycelium,
	Terrarium,
	// Vehicles
	Shuttle,
	ReconShuttle,
	// Storage / living
	WallLocker,
	MedCabinet,
	CargoRack,
	Bunk,
	// Sector dressing (tinted by Tint, sized by Extent)
	SectorTrim,
	HazardStripe,
	HoloScreen,
	SteamVent,
	CableBundle,
	WallPanels,
	Grime,
	// Surface points of interest
	LandingPad,
	RelaySatellite,
	IceDrillRig,
	AbandonedOutpost,
	BasaltColumns,
	SurveyBeaconMast,
	// Wayfinding
	NavArrow,      // glowing wall arrow plate, points along +X (or -X if Extent.X < 0)
	// Orbital operations
	Asteroid,
	ScienceBuoy,
	DerelictStation,
	DebrisContainer,
	StarField,
	// Interior overhaul (reference look: gunmetal, chamfered corners, diamond plate)
	CeilingChamfer,  // angled panel at the wall/ceiling corner along +X (Extent.X long), sector light strip
	CeilingLight,    // octagonal recessed light fixture
	FloorRunner,     // diamond-plate walkway with grating edges, Extent.X x Extent.Y
	CircuitBox,      // wall utility box with LED matrix (Tint)
	PaddedPanels,    // white padded med-bay wall cladding, Extent.X x Extent.Z
	HydroponicRack,  // three-tier hydroponic trays with UV grow bars
	NavSignPanel,    // destination board backing, Extent.X wide x Extent.Z tall
	NavChevronUp,    // "through this door" chevron (Tint)
	SuitMannequin    // full MCP EVA suit on a stand
};

/**
 * Procedural PS1-style prop factory. Assembles low-poly hard-surface models from engine basic
 * shapes (cube / cylinder / sphere / cone) with flat colours and emissive accents via the
 * M_PS1_Surface material (vertex jitter + affine warp), so the game needs no imported art.
 *
 * Place an AMarsPropBuilder in a level and pick a Kind, or call the static BuildProp() from any
 * actor's OnConstruction (pickups, stations, equipment viewmodels all do).
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsPropBuilder : public AActor
{
	GENERATED_BODY()

public:
	AMarsPropBuilder();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

	/** Creates an invisible pivot (for parts that rotate around a shared axis, e.g. valve wheels). */
	static USceneComponent* AddPivot(USceneComponent* Parent, const FVector& Location, const FRotator& Rotation, FName Tag);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	EMarsPropKind Kind = EMarsPropKind::Crate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop", meta = (ClampMin = "0.05"))
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	bool bCollision = true;

	/** Accent colour for tinted kinds (sector trim, holo screens, locker stripes, beacons). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	FLinearColor Tint = FLinearColor(1.0f, 0.62f, 0.1f);

	/** Size for stretchable kinds (trim length, panel wall size, cable span...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	FVector Extent = FVector(400.0f, 100.0f, 250.0f);

	/** Tint / Extent used by the next BuildProp call (set by the actor before building). */
	static FLinearColor BuildTint;
	static FVector BuildExtent;

	/** Builds the prop under Parent. Call from OnConstruction (components are construction-script owned). */
	static TArray<UStaticMeshComponent*> BuildProp(USceneComponent* Parent, EMarsPropKind Kind, float Scale = 1.0f, bool bCollision = false, float SelfIllumination = 0.0f);

	/** One coloured primitive. Size is the full extent in cm (cylinder: diameter x diameter x height). */
	static UStaticMeshComponent* AddPart(USceneComponent* Parent, EMarsPropShape Shape, const FVector& Location, const FVector& Size,
		const FRotator& Rotation, const FLinearColor& Color, float Emissive = 0.0f, bool bCollision = false, FName Tag = NAME_None);

	/** Material response of one part (PS1 master exposes Roughness / Metallic). */
	static void SetSurface(UStaticMeshComponent* Part, float Roughness, float Metallic = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Prop")
	static EMarsPropKind GetPropForItem(EMarsItemType ItemType);

private:
	/** Animated parts discovered after construction: "Spin" pivots, "Blink" LEDs, "Pulse" bioluminescence, "Breathe" pods. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> SpinParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GlowParts;

	TArray<FLinearColor> GlowBase;
	TArray<uint8> GlowMode; // 0 = pulse, 1 = blink
	TArray<FVector> BreatheBase;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BreatheParts;

	float AnimTime = 0.0f;
};
