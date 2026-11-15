#include "UI/MarsInventoryWidget.h"

#include "Audio/MarsAudioManager.h"
#include "Items/MarsInventoryComponent.h"
#include "Items/MarsItemLibrary.h"
#include "Player/MarsCharacter.h"
#include "UI/MarsUIStyle.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr float InvRowH = 34.0f;
}

AMarsCharacter* UMarsInventoryWidget::GetPlayer() const
{
	return Cast<AMarsCharacter>(GetOwningPlayerPawn());
}

void UMarsInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
}

void UMarsInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	FlashTime = FMath::Max(FlashTime - InDeltaTime, 0.0f);
	if (!HasKeyboardFocus())
	{
		SetKeyboardFocus();
	}
	const int32 N = GetItems().Num();
	Selected = N > 0 ? FMath::Clamp(Selected, 0, N - 1) : 0;
}

TArray<FMarsInventorySlot> UMarsInventoryWidget::GetItems() const
{
	const AMarsCharacter* C = GetPlayer();
	TArray<FMarsInventorySlot> Items = C && C->GetInventory() ? C->GetInventory()->GetSlots() : TArray<FMarsInventorySlot>();
	Items.RemoveAll([](const FMarsInventorySlot& S) { return S.Count <= 0 || S.ItemType == EMarsItemType::None; });
	return Items;
}

EMarsItemType UMarsInventoryWidget::SelectedType() const
{
	const TArray<FMarsInventorySlot> Items = GetItems();
	return Items.IsValidIndex(Selected) ? Items[Selected].ItemType : EMarsItemType::None;
}

void UMarsInventoryWidget::UseSelected()
{
	AMarsCharacter* C = GetPlayer();
	const EMarsItemType Type = SelectedType();
	if (!C || Type == EMarsItemType::None)
	{
		return;
	}
	const FString Name = UMarsItemLibrary::GetItemName(Type).ToString().ToUpper();
	const EMarsInventoryUse Result = C->UseInventoryItem(Type);
	if (Result == EMarsInventoryUse::Held || Result == EMarsInventoryUse::Equipped)
	{
		OnClosed.Broadcast(); // back to the world with it in hand
		return;
	}
	Flash = Result == EMarsInventoryUse::Consumed ? FString::Printf(TEXT("USED: %s"), *Name) : FString::Printf(TEXT("CANNOT USE %s NOW"), *Name);
	FlashTime = 2.0f;
}

void UMarsInventoryWidget::DropSelected()
{
	AMarsCharacter* C = GetPlayer();
	const EMarsItemType Type = SelectedType();
	if (C && Type != EMarsItemType::None && C->DropInventoryItem(Type))
	{
		Flash = FString::Printf(TEXT("DROPPED: %s"), *UMarsItemLibrary::GetItemName(Type).ToString().ToUpper());
		FlashTime = 2.0f;
	}
}

void UMarsInventoryWidget::AssignSelected(int32 BeltIndex)
{
	AMarsCharacter* C = GetPlayer();
	const EMarsItemType Type = SelectedType();
	if (C && Type != EMarsItemType::None)
	{
		C->AssignBeltSlot(BeltIndex, Type);
		Flash = FString::Printf(TEXT("BELT %d  <-  %s"), BeltIndex + 1, *UMarsItemLibrary::GetItemName(Type).ToString().ToUpper());
		FlashTime = 2.0f;
		if (UMarsAudioManager* Audio = C->GetAudioManager())
		{
			Audio->PlayUIBeep(1.3f);
		}
	}
}

FReply UMarsInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const int32 N = GetItems().Num();
	if (Key == EKeys::Escape || Key == EKeys::I || Key == EKeys::Tab)
	{
		OnClosed.Broadcast();
		return FReply::Handled();
	}
	if (N > 0)
	{
		if (Key == EKeys::Up || Key == EKeys::W) { Selected = (Selected + N - 1) % N; return FReply::Handled(); }
		if (Key == EKeys::Down || Key == EKeys::S) { Selected = (Selected + 1) % N; return FReply::Handled(); }
		if (Key == EKeys::Enter || Key == EKeys::E || Key == EKeys::SpaceBar) { UseSelected(); return FReply::Handled(); }
		if (Key == EKeys::X || Key == EKeys::Delete) { DropSelected(); return FReply::Handled(); }
		const FKey Nums[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
		for (int32 i = 0; i < 6; ++i)
		{
			if (Key == Nums[i])
			{
				AssignSelected(i);
				return FReply::Handled();
			}
		}
	}
	return FReply::Handled();
}

FVector2D UMarsInventoryWidget::RowPos(const FVector2D& Size, int32 Index) const
{
	return FVector2D(Size.X * 0.08f, Size.Y * 0.2f + Index * InvRowH);
}

int32 UMarsInventoryWidget::RowAt(const FGeometry& Geo, const FVector2D& ScreenPos) const
{
	const FVector2D Local = Geo.AbsoluteToLocal(ScreenPos);
	const FVector2D Size = Geo.GetLocalSize();
	const int32 N = GetItems().Num();
	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D P = RowPos(Size, i);
		if (Local.Y >= P.Y - 6.0f && Local.Y < P.Y + InvRowH - 6.0f && Local.X >= P.X - 12.0f && Local.X <= P.X + Size.X * 0.42f)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FReply UMarsInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const int32 Row = RowAt(InGeometry, InMouseEvent.GetScreenSpacePosition());
	if (Row != INDEX_NONE)
	{
		Selected = Row;
		if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			DropSelected();
		}
		else
		{
			UseSelected();
		}
	}
	return FReply::Handled();
}

FReply UMarsInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const int32 Row = RowAt(InGeometry, InMouseEvent.GetScreenSpacePosition());
	if (Row != INDEX_NONE)
	{
		Selected = Row;
	}
	return FReply::Handled();
}

int32 UMarsInventoryWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace MarsUI;
	const FVector2D Size = Geo.GetLocalSize();
	const AMarsCharacter* C = GetPlayer();
	const UMarsInventoryComponent* Inv = C ? C->GetInventory() : nullptr;
	Box(Out, LayerId, Geo, FVector2D::ZeroVector, Size, FLinearColor(0.0f, 0.0f, 0.0f, 0.78f));

	Text(Out, LayerId + 1, Geo, TEXT("INVENTORY"), FVector2D(Size.X * 0.08f, Size.Y * 0.08f), Mono(26, 400), Amber);
	if (Inv)
	{
		const float Mass = Inv->GetTotalMassKg();
		const float Max = Inv->MaxCarryMassKg;
		Text(Out, LayerId + 1, Geo, FString::Printf(TEXT("CARRIED MASS %.1f / %.0f KG  (MARS: %.1f KG-F)"), Mass, Max, Mass * 0.38f),
			FVector2D(Size.X * 0.08f + 4.0f, Size.Y * 0.08f + 44.0f), Mono(9, 160), Mass > Max * 0.9f ? Alarm : AmberDim * 1.6f);
	}

	// Item list.
	const TArray<FMarsInventorySlot> Items = GetItems();
	if (Items.Num() == 0)
	{
		Text(Out, LayerId + 1, Geo, TEXT("NOTHING CARRIED"), RowPos(Size, 0), Mono(13, 160), AmberDim * 1.5f);
	}
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const bool bSel = i == Selected;
		const FVector2D P = RowPos(Size, i);
		if (bSel)
		{
			Box(Out, LayerId + 1, Geo, P - FVector2D(12.0f, 6.0f), FVector2D(Size.X * 0.42f, InvRowH - 4.0f), FLinearColor(Amber.R, Amber.G, Amber.B, 0.14f + 0.05f * FMath::Sin(Time * 4.0f)));
			Box(Out, LayerId + 2, Geo, P - FVector2D(12.0f, 6.0f), FVector2D(3.0f, InvRowH - 4.0f), Amber);
		}
		const FMarsItemInfo Info = UMarsItemLibrary::GetItemInfo(Items[i].ItemType);
		Text(Out, LayerId + 2, Geo, Info.DisplayName.ToString().ToUpper(), P, Mono(13, 120), bSel ? Amber : AmberDim * 1.6f);
		Text(Out, LayerId + 2, Geo, FString::Printf(TEXT("x%d"), Items[i].Count), P + FVector2D(Size.X * 0.3f, 0.0f), Mono(13, 120), Phosphor);
		Text(Out, LayerId + 2, Geo, FString::Printf(TEXT("%.1f kg"), Info.MassKg * Items[i].Count), P + FVector2D(Size.X * 0.36f, 2.0f), Mono(9, 100), AmberDim * 1.4f);
	}

	// Detail panel.
	const FVector2D DP(Size.X * 0.56f, Size.Y * 0.2f);
	const FVector2D DS(Size.X * 0.36f, Size.Y * 0.5f);
	Box(Out, LayerId + 1, Geo, DP, DS, FLinearColor(0.01f, 0.012f, 0.015f, 0.85f));
	Frame(Out, LayerId + 2, Geo, DP, DS, AmberDim * 1.3f, 2.0f);
	if (Items.IsValidIndex(Selected))
	{
		const EMarsItemType Type = Items[Selected].ItemType;
		const FMarsItemInfo Info = UMarsItemLibrary::GetItemInfo(Type);
		Text(Out, LayerId + 3, Geo, Info.DisplayName.ToString().ToUpper(), DP + FVector2D(20, 18), Mono(15, 160), Amber);
		// Wrapped description.
		TArray<FString> Words;
		Info.Description.ToString().ParseIntoArray(Words, TEXT(" "));
		FString Line;
		float Y = DP.Y + 56.0f;
		const FSlateFontInfo F = Mono(10, 40);
		for (const FString& W : Words)
		{
			const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
			if (Measure(Try, F).X > DS.X - 40.0f && !Line.IsEmpty())
			{
				Text(Out, LayerId + 3, Geo, Line, FVector2D(DP.X + 20.0f, Y), F, Phosphor);
				Y += 17.0f;
				Line = W;
			}
			else
			{
				Line = Try;
			}
		}
		if (!Line.IsEmpty())
		{
			Text(Out, LayerId + 3, Geo, Line, FVector2D(DP.X + 20.0f, Y), F, Phosphor);
		}
		const FString UseLabel = UMarsItemLibrary::IsEquippable(Type) ? TEXT("EQUIP / HOLD") : (C ? C->GetInventoryUseLabel(Type) : TEXT("USE"));
		Text(Out, LayerId + 3, Geo, FString::Printf(TEXT("[ENTER / CLICK]  %s"), *UseLabel), FVector2D(DP.X + 20.0f, DP.Y + DS.Y - 74.0f), Mono(10, 140), Cyan);
		Text(Out, LayerId + 3, Geo, TEXT("[1-6]  PUT ON BELT SLOT      [X / RIGHT CLICK]  DROP ONE"), FVector2D(DP.X + 20.0f, DP.Y + DS.Y - 50.0f), Mono(9, 120), AmberDim * 1.6f);
	}

	// Belt preview.
	if (C)
	{
		const TArray<FMarsBeltSlot> Belt = C->GetBeltSlots();
		const float BX = Size.X * 0.56f;
		const float BY = Size.Y * 0.74f;
		Text(Out, LayerId + 1, Geo, TEXT("BELT"), FVector2D(BX, BY), Mono(9, 200), AmberDim * 1.6f);
		for (int32 i = 0; i < Belt.Num(); ++i)
		{
			const FVector2D P(BX + i * 92.0f, BY + 18.0f);
			Frame(Out, LayerId + 2, Geo, P, FVector2D(86.0f, 40.0f), AmberDim * 1.4f, 2.0f);
			Text(Out, LayerId + 3, Geo, FString::Printf(TEXT("%d"), i + 1), P + FVector2D(5, 3), Mono(8, 100), AmberDim * 1.6f);
			Text(Out, LayerId + 3, Geo, Belt[i].ShortName, P + FVector2D(8, 18), Mono(9, 80), Phosphor);
		}
	}

	if (FlashTime > 0.0f)
	{
		TextCentered(Out, LayerId + 4, Geo, Flash, FVector2D(Size.X * 0.5f, Size.Y * 0.87f), Mono(12, 200), Phosphor * FLinearColor(1, 1, 1, FMath::Min(FlashTime, 1.0f)));
	}
	Text(Out, LayerId + 1, Geo, TEXT("W/S SELECT   ENTER USE   1-6 BELT   X DROP   I / TAB / ESC CLOSE"), FVector2D(Size.X * 0.08f, Size.Y * 0.93f), Mono(9, 160), AmberDim * 1.4f);
	return LayerId + 6;
}
