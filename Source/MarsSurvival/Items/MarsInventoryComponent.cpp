#include "Items/MarsInventoryComponent.h"

#include "Items/MarsItemLibrary.h"

UMarsInventoryComponent::UMarsInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UMarsInventoryComponent::AddItem(EMarsItemType ItemType, int32 Count)
{
	if (ItemType == EMarsItemType::None || Count <= 0)
	{
		return 0;
	}

	const float UnitMass = UMarsItemLibrary::GetItemInfo(ItemType).MassKg;
	int32 Accepted = Count;
	if (UnitMass > 0.0f)
	{
		const float FreeMass = MaxCarryMassKg - GetTotalMassKg();
		Accepted = FMath::Clamp(FMath::FloorToInt(FreeMass / UnitMass), 0, Count);
	}
	if (Accepted <= 0)
	{
		return 0;
	}

	FMarsInventorySlot* Slot = Slots.FindByPredicate([ItemType](const FMarsInventorySlot& S) { return S.ItemType == ItemType; });
	if (Slot)
	{
		Slot->Count += Accepted;
	}
	else
	{
		FMarsInventorySlot NewSlot;
		NewSlot.ItemType = ItemType;
		NewSlot.Count = Accepted;
		Slots.Add(NewSlot);
	}
	OnInventoryChanged.Broadcast();
	return Accepted;
}

bool UMarsInventoryComponent::RemoveItem(EMarsItemType ItemType, int32 Count)
{
	const int32 Index = Slots.IndexOfByPredicate([ItemType](const FMarsInventorySlot& S) { return S.ItemType == ItemType; });
	if (Index == INDEX_NONE || Count <= 0 || Slots[Index].Count < Count)
	{
		return false;
	}
	Slots[Index].Count -= Count;
	if (Slots[Index].Count <= 0)
	{
		Slots.RemoveAt(Index);
	}
	OnInventoryChanged.Broadcast();
	return true;
}

int32 UMarsInventoryComponent::GetItemCount(EMarsItemType ItemType) const
{
	const FMarsInventorySlot* Slot = Slots.FindByPredicate([ItemType](const FMarsInventorySlot& S) { return S.ItemType == ItemType; });
	return Slot ? Slot->Count : 0;
}

float UMarsInventoryComponent::GetTotalMassKg() const
{
	float Total = 0.0f;
	for (const FMarsInventorySlot& Slot : Slots)
	{
		Total += UMarsItemLibrary::GetItemInfo(Slot.ItemType).MassKg * Slot.Count;
	}
	return Total;
}
