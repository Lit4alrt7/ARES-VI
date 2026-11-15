#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/MarsItemTypes.h"
#include "MarsInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

/** Mass-limited engineer's inventory (one entry per item type). */
UCLASS(ClassGroup = (MarsSurvival), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UMarsInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMarsInventoryComponent();

	/** Returns how many were actually added (mass limit). */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(EMarsItemType ItemType, int32 Count = 1);

	/** Removes exactly Count or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(EMarsItemType ItemType, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(EMarsItemType ItemType) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(EMarsItemType ItemType, int32 Count = 1) const { return GetItemCount(ItemType) >= Count; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetTotalMassKg() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FMarsInventorySlot> GetSlots() const { return Slots; }

	/** Save/load: replace the whole inventory. */
	void SetSlots(const TArray<FMarsInventorySlot>& InSlots) { Slots = InSlots; OnInventoryChanged.Broadcast(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	float MaxCarryMassKg = 45.0f;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

protected:
	/** Also used as the starting loadout (edit in the character defaults). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FMarsInventorySlot> Slots;

	friend class AMarsCharacter;
};
