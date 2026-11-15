#include "UI/MarsPauseMenuWidget.h"

#include "Audio/MarsAudioManager.h"
#include "Base/CommRelayModule.h"
#include "Base/MarsBaseModule.h"
#include "Core/MarsGameMode.h"
#include "Core/MarsSaveGame.h"
#include "Core/MarsTutorialManager.h"
#include "Core/MarsNarrativeDirector.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsBaseManager.h"
#include "Core/MarsMissionDirector.h"
#include "Player/EVASuitComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "UI/MarsUIStyle.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

namespace
{
	const TCHAR* Label(int32 Item)
	{
		static const TCHAR* Names[] = { TEXT("RESUME"), TEXT("LOAD LAST SAVE"), TEXT("SETTINGS & CONTROLS"), TEXT("RESTART MISSION"), TEXT("QUIT TO MAIN MENU"), TEXT("QUIT TO DESKTOP") };
		return Names[FMath::Clamp(Item, 0, 5)];
	}

	FString Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), S / 60, S % 60);
	}
}

void UMarsPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
}

void UMarsPauseMenuWidget::Setup(bool bInMissionOver)
{
	bMissionOver = bInMissionOver;
	Items.Reset();
	if (!bMissionOver)
	{
		Items.Add(EItem::Resume);
	}
	const FString LastSlot = UMarsSaveGame::MostRecentSlot();
	if (const UMarsSaveGame* Save = UMarsSaveGame::LoadSlot(LastSlot))
	{
		Items.Add(EItem::Load);
		SaveInfo = FString::Printf(TEXT("%s  //  %s  //  %s"), *Save->Label, *Save->LocationName, *Save->Timestamp.ToString(TEXT("%H:%M:%S")));
	}
	Items.Add(EItem::Settings);
	Items.Add(EItem::Restart);
	Items.Add(EItem::QuitMenu);
	Items.Add(EItem::QuitDesktop);
	Selected = 0;
	ArmedIndex = INDEX_NONE;
}

void UMarsPauseMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!HasKeyboardFocus())
	{
		SetKeyboardFocus();
	}
	Time += InDeltaTime;
}

void UMarsPauseMenuWidget::Beep(float Pitch) const
{
	if (const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwningPlayerPawn()))
	{
		if (UMarsAudioManager* Audio = C->GetAudioManager())
		{
			Audio->PlayUIBeep(Pitch);
		}
	}
}

bool UMarsPauseMenuWidget::NeedsConfirm(EItem Item)
{
	return Item == EItem::Load || Item == EItem::Restart || Item == EItem::QuitMenu || Item == EItem::QuitDesktop;
}

void UMarsPauseMenuWidget::Activate(int32 Index)
{
	if (!Items.IsValidIndex(Index))
	{
		return;
	}
	const EItem Item = Items[Index];
	if (NeedsConfirm(Item) && ArmedIndex != Index)
	{
		ArmedIndex = Index;
		Beep(0.9f);
		return;
	}
	ArmedIndex = INDEX_NONE;
	Beep(1.5f);
	switch (Item)
	{
	case EItem::Resume:      OnAction.Broadcast(EMarsPauseAction::Resume); break;
	case EItem::Load:        OnAction.Broadcast(EMarsPauseAction::LoadLastSave); break;
	case EItem::Settings:    Settings.Open(); break;
	case EItem::Restart:     OnAction.Broadcast(EMarsPauseAction::Restart); break;
	case EItem::QuitMenu:    OnAction.Broadcast(EMarsPauseAction::QuitToMenu); break;
	case EItem::QuitDesktop: OnAction.Broadcast(EMarsPauseAction::QuitToDesktop); break;
	}
}

FReply UMarsPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (InKeyEvent.IsRepeat())
	{
		return FReply::Handled();
	}
	if (Settings.IsOpen())
	{
		Settings.OnKey(Key, GetWorld());
		Beep(1.25f);
		return FReply::Handled();
	}
	const int32 N = Items.Num();
	if (Key == EKeys::Escape)
	{
		if (ArmedIndex != INDEX_NONE)
		{
			ArmedIndex = INDEX_NONE;
		}
		else if (!bMissionOver)
		{
			OnAction.Broadcast(EMarsPauseAction::Resume);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::W) { Selected = (Selected + N - 1) % N; ArmedIndex = INDEX_NONE; Beep(1.2f); return FReply::Handled(); }
	if (Key == EKeys::Down || Key == EKeys::S) { Selected = (Selected + 1) % N; ArmedIndex = INDEX_NONE; Beep(1.2f); return FReply::Handled(); }
	if (Key == EKeys::Enter || Key == EKeys::E) { Activate(Selected); return FReply::Handled(); }
	return FReply::Handled();
}

FVector2D UMarsPauseMenuWidget::ItemPos(const FVector2D& Size, int32 Index) const
{
	return FVector2D(Size.X * 0.08f, Size.Y * 0.30f + Index * 46.0f);
}

int32 UMarsPauseMenuWidget::ItemAt(const FGeometry& Geo, const FVector2D& ScreenPos) const
{
	const FVector2D Local = Geo.AbsoluteToLocal(ScreenPos);
	const FVector2D Size = Geo.GetLocalSize();
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FVector2D P = ItemPos(Size, i);
		if (Local.Y >= P.Y - 8.0f && Local.Y <= P.Y + 32.0f && Local.X >= P.X - 20.0f && Local.X <= P.X + 400.0f)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FReply UMarsPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Settings.IsOpen())
	{
		Settings.OnMouseDown(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), InMouseEvent.GetEffectingButton(), GetWorld());
		Beep(1.25f);
		return FReply::Handled();
	}
	const int32 Item = ItemAt(InGeometry, InMouseEvent.GetScreenSpacePosition());
	if (Item != INDEX_NONE && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (Item != Selected)
		{
			ArmedIndex = INDEX_NONE;
		}
		Selected = Item;
		Activate(Item);
	}
	return FReply::Handled();
}

FReply UMarsPauseMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Settings.IsOpen())
	{
		Settings.OnMouseMove(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
		return FReply::Handled();
	}
	const int32 Item = ItemAt(InGeometry, InMouseEvent.GetScreenSpacePosition());
	if (Item != INDEX_NONE && Item != Selected)
	{
		Selected = Item;
		ArmedIndex = INDEX_NONE;
	}
	return FReply::Handled();
}

FReply UMarsPauseMenuWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Settings.OnMouseWheel(InMouseEvent.GetWheelDelta());
	return FReply::Handled();
}

void UMarsPauseMenuWidget::PaintStatus(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& PanelSize) const
{
	using namespace MarsUI;
	Box(Out, Layer, Geo, Pos, PanelSize, FLinearColor(0.01f, 0.012f, 0.015f, 0.85f));
	Frame(Out, Layer + 1, Geo, Pos, PanelSize, AmberDim * 1.3f);
	Text(Out, Layer + 2, Geo, TEXT("MISSION STATUS"), Pos + FVector2D(22, 18), Mono(15, 260), Amber);

	const UWorld* World = GetWorld();
	const AMarsGameMode* GM = World ? World->GetAuthGameMode<AMarsGameMode>() : nullptr;
	const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwningPlayerPawn());
	const ACommRelayModule* Comm = nullptr;
	if (World)
	{
		for (TActorIterator<ACommRelayModule> It(const_cast<UWorld*>(World)); It; ++It)
		{
			Comm = *It;
			break;
		}
	}

	const FSlateFontInfo LF = Mono(10, 140);
	const FSlateFontInfo VF = Mono(12, 120);
	float Y = Pos.Y + 62.0f;
	const float X = Pos.X + 22.0f;
	const float VX = Pos.X + PanelSize.X * 0.48f;
	auto Row = [&](const FString& L, const FString& V, const FLinearColor& Col)
	{
		Text(Out, Layer + 2, Geo, L, FVector2D(X, Y + 2.0f), LF, AmberDim * 1.6f);
		Text(Out, Layer + 2, Geo, V, FVector2D(VX, Y), VF, Col);
		Y += 28.0f;
	};

	const UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this);
	if (const UMarsEnvironmentSubsystem* Env = World ? World->GetSubsystem<UMarsEnvironmentSubsystem>() : nullptr)
	{
		const float H = Env->GetLocalSolarTimeHours();
		Row(TEXT("SOL / LOCAL TIME"), FString::Printf(TEXT("SOL %d  %02d:%02d  %s  %.0f C"), Env->GetSolNumber(), FMath::FloorToInt(H), FMath::FloorToInt(FMath::Frac(H) * 60.0f),
			Env->IsNight() ? TEXT("NIGHT") : TEXT("DAY"), Env->GetAmbientTemperatureC()), Cyan);
	}
	if (const UMarsBaseManager* BaseMgr = UMarsBaseManager::Get(this))
	{
		const FMarsBaseStatus& B = BaseMgr->GetStatus();
		Row(TEXT("BASE POWER"), FString::Printf(TEXT("%.1f / %.1f kW  (%.0f %%)"), B.PowerOutputW / 1000.0f, B.PowerDemandW / 1000.0f, B.PowerAvailability * 100.0f),
			B.PowerAvailability < 0.5f ? Alarm : B.PowerAvailability < 0.9f ? Amber : Phosphor);
		Row(TEXT("BASE STRUCTURE / O2"), FString::Printf(TEXT("%.0f %%  /  %.1f %%  O2"), B.AvgIntegrity * 100.0f, B.AvgO2Percent),
			B.AvgIntegrity < 0.5f ? Alarm : B.AvgIntegrity < 0.8f ? Amber : Phosphor);
		Row(TEXT("STOCKS"), FString::Printf(TEXT("H2O %.0f L  FOOD %d  O2 %d  PARTS %d%s"), B.WaterLiters, B.FoodRations, B.O2Canisters, B.SpareParts,
			B.bCraftingLocked ? TEXT("  [BIOPRINTER LOCKED]") : TEXT("")), B.bCraftingLocked ? Amber : Phosphor);
	}
	if (const UMarsMissionDirector* Ops = UMarsMissionDirector::Get(this))
	{
		Row(TEXT("ORBITAL OPS"), Ops->GetStatusLine(), Cyan);
	}
	if (Story)
	{
		Row(TEXT("CHAPTER"), Story->GetChapterTitle(), Amber);
		Row(TEXT("DATA LOGS"), FString::Printf(TEXT("%d / 3 RECOVERED"), Story->GetLogTitles().Num()), Phosphor);
	}
	if (GM && GM->CommWindowMinutes > 0.0f)
	{
		const float Rem = GM->GetRemainingSeconds();
		Row(TEXT("EARTH COMM WINDOW"), Clock(Rem), Rem < 300.0f ? Alarm : Phosphor);
	}
	if (Comm)
	{
		Row(TEXT("QUANTUM TRANSISTORS"), FString::Printf(TEXT("%d / %d"), Comm->InstalledTransistors, Comm->RequiredTransistors),
			Comm->AreTransistorsInstalled() ? Phosphor : Amber);
		Row(TEXT("TRANSCEIVER"), FString::Printf(TEXT("%.0f %%"), Comm->TransceiverIntegrity * 100.0f), Comm->TransceiverIntegrity < 0.6f ? Amber : Phosphor);
	}
	if (C)
	{
		const AMarsBaseModule* M = C->GetCurrentModule();
		Row(TEXT("LOCATION"), M ? M->GetModuleName() : TEXT("MARTIAN SURFACE (EVA)"), Cyan);
		if (M)
		{
			const bool bLeak = M->GetTotalLeakAreaCm2() > 0.05f;
			Row(TEXT("HULL / PRESSURE"), FString::Printf(TEXT("%.0f %%  //  %.1f kPa%s"), M->HullIntegrity * 100.0f, M->CurrentPressureKpa, bLeak ? TEXT("  LEAK") : TEXT("")),
				bLeak || M->HullIntegrity < 0.6f ? Alarm : Phosphor);
		}
		if (const UEVASuitComponent* Suit = C->GetSuit())
		{
			Row(TEXT("SUIT O2"), Suit->IsSuitDonned() ? FString::Printf(TEXT("%.0f %%"), Suit->GetO2Fraction01() * 100.0f) : TEXT("SUIT STOWED"),
				Suit->IsSuitDonned() && Suit->GetO2Fraction01() < 0.2f ? Alarm : Phosphor);
		}
		if (const UPlayerBiometricsComponent* Bio = C->GetBiometrics())
		{
			Row(TEXT("WATER / FOOD"), FString::Printf(TEXT("%.0f %%  /  %.0f %%"), Bio->Hydration * 100.0f, Bio->Nutrition * 100.0f), Phosphor);
			Row(TEXT("CONDITION"), TEXT("SCAN AT MED-BAY BED"), AmberDim * 1.8f);
		}
	}

	// Objective
	Y += 10.0f;
	Box(Out, Layer + 1, Geo, FVector2D(X, Y), FVector2D(PanelSize.X - 44.0f, 1.0f), AmberDim);
	Y += 14.0f;
	Text(Out, Layer + 2, Geo, TEXT("CURRENT OBJECTIVE"), FVector2D(X, Y), Mono(10, 200), Amber);
	Y += 26.0f;
	FString Objective;
	if (Story && Story->IsStarted() && !(AMarsTutorialManager::Get(this) && AMarsTutorialManager::Get(this)->IsActive()))
	{
		Objective = Story->GetObjectiveText();
	}
	else if (const AMarsTutorialManager* Tut = AMarsTutorialManager::Get(this); Tut && Tut->IsActive())
	{
		Objective = FString::Printf(TEXT("SUIT TRAINING %d/%d: %s"), Tut->GetStepIndex() + 1, Tut->GetStepCount(), *Tut->GetCurrentStep().Title);
	}
	else if (Comm && !Comm->AreTransistorsInstalled())
	{
		Objective = FString::Printf(TEXT("Recover quantum transistors (%d/%d): one in the Storage crates, two at the crashed lander ~650 m out (follow beacon LANDER - its RTG leaks)."),
			Comm->InstalledTransistors, Comm->RequiredTransistors);
	}
	else if (Comm && Comm->TransceiverIntegrity < 0.6f)
	{
		Objective = TEXT("Repair the transceiver with spare parts at the Comm-Relay terminal.");
	}
	else if (Comm && !Comm->IsReadyToTransmit())
	{
		const FString Block = Comm->GetBlockingReason();
		Objective = Block.IsEmpty() ? TEXT("Hold E at the dish alignment terminal until calibration reaches 100 %.") : Block;
	}
	else
	{
		Objective = TEXT("Transmit to Earth from the Comm-Relay terminal.");
	}
	// Simple word wrap.
	const float WrapW = PanelSize.X - 44.0f;
	const FSlateFontInfo OF = Mono(11, 60);
	TArray<FString> Words;
	Objective.ParseIntoArray(Words, TEXT(" "));
	FString Line;
	for (const FString& W : Words)
	{
		const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
		if (Measure(Try, OF).X > WrapW && !Line.IsEmpty())
		{
			Text(Out, Layer + 2, Geo, Line, FVector2D(X, Y), OF, Phosphor);
			Y += 20.0f;
			Line = W;
		}
		else
		{
			Line = Try;
		}
	}
	if (!Line.IsEmpty())
	{
		Text(Out, Layer + 2, Geo, Line, FVector2D(X, Y), OF, Phosphor);
	}

	// Save info
	Text(Out, Layer + 2, Geo, SaveInfo.IsEmpty() ? TEXT("NO SAVE ON FILE  -  USE A MISSION LOG TERMINAL (STORAGE / MED-BAY)") : FString::Printf(TEXT("LAST SAVE: %s"), *SaveInfo),
		FVector2D(X, Pos.Y + PanelSize.Y - 34.0f), Mono(8, 120), AmberDim * 1.6f);
}

int32 UMarsPauseMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace MarsUI;
	const FVector2D Size = Geo.GetLocalSize();
	Box(Out, LayerId, Geo, FVector2D::ZeroVector, Size, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));

	const AMarsGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMarsGameMode>() : nullptr;
	FString Title = TEXT("PAUSED");
	FLinearColor TitleCol = Amber;
	if (bMissionOver && GM)
	{
		Title = GM->IsVictory() ? TEXT("MISSION COMPLETE") : TEXT("MISSION FAILED");
		TitleCol = GM->IsVictory() ? Phosphor : Alarm;
	}
	Text(Out, LayerId + 2, Geo, Title, FVector2D(Size.X * 0.08f, Size.Y * 0.14f), Mono(34, 400), TitleCol);
	if (bMissionOver && GM)
	{
		Text(Out, LayerId + 2, Geo, GM->GetEndReason().ToUpper(), FVector2D(Size.X * 0.08f + 4.0f, Size.Y * 0.14f + 54.0f), Mono(10, 160), AmberDim * 1.6f);
	}
	else
	{
		Text(Out, LayerId + 2, Geo, TEXT("ARES-IV  //  SV-4 SUIT INTERFACE HOLD"), FVector2D(Size.X * 0.08f + 4.0f, Size.Y * 0.14f + 54.0f), Mono(10, 160), AmberDim * 1.6f);
	}

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const bool bSel = i == Selected && !Settings.IsOpen();
		const FVector2D P = ItemPos(Size, i);
		if (bSel)
		{
			Box(Out, LayerId + 1, Geo, P - FVector2D(14.0f, 6.0f), FVector2D(380.0f, 36.0f), FLinearColor(Amber.R, Amber.G, Amber.B, 0.12f + 0.05f * FMath::Sin(Time * 4.0f)));
			Box(Out, LayerId + 2, Geo, P - FVector2D(14.0f, 6.0f), FVector2D(3.0f, 36.0f), Amber);
		}
		FString L = Label(static_cast<int32>(Items[i]));
		FLinearColor Col = bSel ? Amber : AmberDim * 1.5f;
		if (i == ArmedIndex)
		{
			L = FString::Printf(TEXT("%s ?  ENTER / CLICK AGAIN"), *L);
			Col = FMath::Frac(Time * 2.0f) < 0.5f ? Alarm : Amber;
		}
		Text(Out, LayerId + 2, Geo, FString::Printf(TEXT("%s%s"), bSel ? TEXT("> ") : TEXT("  "), *L), P, Mono(15, 220), Col);
	}

	const FVector2D PanelSize(FMath::Min(Size.X * 0.42f, 620.0f), Size.Y * 0.66f);
	PaintStatus(Out, LayerId + 3, Geo, FVector2D(Size.X * 0.92f - PanelSize.X, Size.Y * 0.14f), PanelSize);

	Text(Out, LayerId + 2, Geo, bMissionOver ? TEXT("W/S SELECT   ENTER CONFIRM") : TEXT("W/S SELECT   ENTER CONFIRM   ESC RESUME"),
		FVector2D(Size.X * 0.08f, Size.Y * 0.90f), Mono(9, 200), AmberDim * 1.4f);

	if (Settings.IsOpen())
	{
		Box(Out, LayerId + 10, Geo, FVector2D::ZeroVector, Size, FLinearColor(0, 0, 0, 0.5f));
		const FVector2D PS(FMath::Min(Size.X * 0.62f, 980.0f), Size.Y * 0.80f);
		Settings.Paint(Out, LayerId + 11, Geo, (Size - PS) * 0.5f, PS, Time);
	}
	return LayerId + 40;
}
