#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/MarsItemTypes.h"
#include "Props/MarsPropBuilder.h"
#include "MarsEquipment.generated.h"

class AMarsCharacter;
class AMarsBaseModule;
class UMarsAudioManager;

/** Base for hand-held gear attached to the first-person camera. Viewmodel built procedurally. */
UCLASS(Abstract, Blueprintable)
class MARSSURVIVAL_API AMarsEquipment : public AActor
{
	GENERATED_BODY()

public:
	AMarsEquipment();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

	virtual void OnEquipped(AMarsCharacter* NewOwner);
	virtual void OnUnequipped();

	virtual void StartPrimary() { bPrimaryHeld = true; }
	virtual void StopPrimary() { bPrimaryHeld = false; }
	virtual void StartSecondary() {}
	virtual void Reload() {}

	/** One-line HUD status (ammo, charge, readings). */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	virtual FText GetStatusText() const { return FText::GetEmpty(); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ViewRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EMarsItemType ItemType = EMarsItemType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EMarsPropKind ViewModelKind = EMarsPropKind::None;

	/** Offset from the camera when held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FVector HeldOffset = FVector(32.0f, 14.0f, -14.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FRotator HeldRotation = FRotator::ZeroRotator;

	/** Suited right hand: grip centre and grip axis (local Z = along the handle) in view-model space. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	bool bShowHand = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FVector GripLocation = FVector(-4.6f, 0.0f, -6.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FRotator GripRotation = FRotator(-18.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	float GripWidth = 3.0f;

protected:
	void BuildSuitHand();
	bool TraceFromView(float Range, FHitResult& OutHit, float SpreadDegrees = 0.0f) const;
	static AMarsBaseModule* GetHitModule(const FHitResult& Hit);
	UMarsAudioManager* GetAudio() const;

	/** Hand tremor from the disease stage (applied to aim precision). */
	float GetTremorDegrees() const;

	/** Viewmodel parts carrying a component tag (e.g. "Slide", "Needle"). */
	TArray<UStaticMeshComponent*> FindTaggedParts(FName Tag) const;

	TWeakObjectPtr<AMarsCharacter> OwnerCharacter;
	bool bPrimaryHeld = false;
};
