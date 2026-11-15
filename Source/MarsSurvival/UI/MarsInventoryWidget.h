#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/MarsItemTypes.h"
#include "MarsInventoryWidget.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnMarsInventoryClosed);

/**
 * Inventory screen (I / Tab). Lists everything the player carries.
 *   ENTER / E / left click : use it (equip tools and the Glock, drink, eat, inject, attach an O2 canister;
 *                            anything else is taken in hand to look at)
 *   1 - 6                  : put it on that belt slot (then the number key uses it in the field)
 *   X / DEL / right click  : drop one on the floor
 *   I / TAB / ESC          : close
 */
UCLASS()
class MARSSURVIVAL_API UMarsInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnMarsInventoryClosed OnClosed;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	TArray<FMarsInventorySlot> GetItems() const;
	EMarsItemType SelectedType() const;
	void UseSelected();
	void DropSelected();
	void AssignSelected(int32 BeltIndex);
	int32 RowAt(const FGeometry& Geo, const FVector2D& ScreenPos) const;
	FVector2D RowPos(const FVector2D& Size, int32 Index) const;
	class AMarsCharacter* GetPlayer() const;

	int32 Selected = 0;
	float Time = 0.0f;
	FString Flash;
	float FlashTime = 0.0f;
};
