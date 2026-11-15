#pragma once

#include "CoreMinimal.h"
#include "Equipment/MarsEquipment.h"
#include "MarsTools.generated.h"

/**
 * Weld / sealant gun. Hold LMB on a punctured hull panel: cures one sealant patch per
 * PatchSeconds. With no punctures left, arc-weld mode restores structural integrity
 * (and closes a catastrophic breach). Consumes Sealant Cartridges (5 patches each).
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AWeldSealantTool : public AMarsEquipment
{
	GENERATED_BODY()

public:
	AWeldSealantTool();

	virtual void Tick(float DeltaTime) override;
	virtual void Reload() override;
	virtual FText GetStatusText() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealant")
	float RangeCm = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealant")
	float PatchSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealant")
	int32 PatchesPerCartridge = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealant")
	int32 LoadedCharge = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sealant")
	float WeldIntegrityPerSecond = 0.04f;

private:
	float PatchProgress = 0.0f;
	float WeldAccumulated = 0.0f;
	float HissTimer = 0.0f;
	FString LastTargetStatus;
};

/** Torque wrench. Hold LMB on a bulkhead to re-torque flange bolts and re-compress cold-contracted seals. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ATorqueWrenchTool : public AMarsEquipment
{
	GENERATED_BODY()

public:
	ATorqueWrenchTool();

	virtual void Tick(float DeltaTime) override;
	virtual FText GetStatusText() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torque Wrench")
	float RangeCm = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torque Wrench")
	float SealRestorePerSecond = 0.08f;

private:
	FString LastTargetStatus;
	float RatchetTimer = 0.0f;
	float TorqueSeconds = 0.0f;
};

/**
 * Geiger-Muller dosimeter. While held: louder clicks, animated needle (log scale) and blinking LED.
 * The integrated dose and rate are shown on its own display (not on the HUD).
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ARadiationDosimeter : public AMarsEquipment
{
	GENERATED_BODY()

public:
	ARadiationDosimeter();

	virtual void Tick(float DeltaTime) override;
	virtual void OnEquipped(AMarsCharacter* NewOwner) override;
	virtual void OnUnequipped() override;
	virtual FText GetStatusText() const override;

private:
	float LedFlash = 0.0f;
	float LastClickCount = 0.0f;
};
