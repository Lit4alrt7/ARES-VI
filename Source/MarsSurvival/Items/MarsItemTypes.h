#pragma once

#include "CoreMinimal.h"
#include "MarsItemTypes.generated.h"

UENUM(BlueprintType)
enum class EMarsItemType : uint8
{
	None                    UMETA(DisplayName = "None"),
	// Tools / weapon (equippable)
	Glock18                 UMETA(DisplayName = "Glock-18"),
	WeldSealantGun          UMETA(DisplayName = "Weld / Sealant Gun"),
	TorqueWrench            UMETA(DisplayName = "Torque Wrench"),
	RadiationDosimeter      UMETA(DisplayName = "Radiation Dosimeter"),
	// Ammunition / tool consumables
	Ammo9mm                 UMETA(DisplayName = "9x19mm Rounds"),
	SealantCartridge        UMETA(DisplayName = "Sealant Cartridge"),
	// Life support / medical
	O2Canister              UMETA(DisplayName = "O2 Canister"),
	MethyleneBlueCartridge  UMETA(DisplayName = "Methylene Blue Cartridge"),
	IodinePills             UMETA(DisplayName = "Potassium Iodide Pills"),
	// Samples / components
	RegolithSample          UMETA(DisplayName = "Regolith Sample"),
	RockSample              UMETA(DisplayName = "Rock Sample"),
	QuantumTransistor       UMETA(DisplayName = "Quantum Transistor"),
	SpareParts              UMETA(DisplayName = "Spare Parts"),
	SurfaceBeacon           UMETA(DisplayName = "Surface Beacon"),
	// Survival / advanced medicine (appended to keep existing values stable)
	WaterPouch              UMETA(DisplayName = "Water Pouch"),
	FoodRation              UMETA(DisplayName = "Food Ration"),
	ChlorellaPaste          UMETA(DisplayName = "Chlorella Paste"),
	NeuroAntitoxin          UMETA(DisplayName = "Neuro-Antitoxin"),
	Amifostine              UMETA(DisplayName = "Amifostine"),
	BioAgentCulture         UMETA(DisplayName = "Bio-Agent Culture")
};

UENUM(BlueprintType)
enum class EMarsItemCategory : uint8
{
	Tool,
	Weapon,
	Ammunition,
	Consumable,
	Medical,
	Sample,
	Component,
	Deployable
};

USTRUCT(BlueprintType)
struct FMarsItemInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EMarsItemCategory Category = EMarsItemCategory::Component;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float MassKg = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;
};

USTRUCT(BlueprintType)
struct FMarsInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EMarsItemType ItemType = EMarsItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0"))
	int32 Count = 0;
};
