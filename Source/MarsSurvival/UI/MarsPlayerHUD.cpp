#include "UI/MarsPlayerHUD.h"

#include "AI/AriaSubsystem.h"
#include "Core/MarsTutorialManager.h"
#include "World/MarsStationSubsystem.h"
#include "Core/MarsGameMode.h"
#include "Core/MarsUserSettings.h"
#include "Audio/MarsAudioManager.h"
#include "Equipment/FuturisticGlock18.h"
#include "Items/MarsItemLibrary.h"
#include "Equipment/MarsEquipment.h"
#include "Items/MarsInventoryComponent.h"
#include "Player/EVASuitComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Stations/MarsStations.h"
#include "UI/MarsUIStyle.h"
#include "World/SurfaceBeacon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Core/MarsNarrativeDirector.h"
#include "Vehicles/MarsReconShuttle.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsSolarStormManager.h"
#include "Core/MarsMissionDirector.h"
#include "Rendering/DrawElements.h"

void UMarsPlayerHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
}

int32 UMarsPlayerHUD::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwningPlayerPawn());
	if (const AMarsReconShuttle* Shuttle = Cast<AMarsReconShuttle>(GetOwningPlayerPawn()))
	{
		if (!bMinimal)
		{
			PaintCockpit(Out, LayerId, Geo, Shuttle);
		}
		PaintObjective(Out, LayerId, Geo);
		PaintWaypoint(Out, LayerId, Geo);
		PaintOrbitOps(Out, LayerId + 2, Geo, Shuttle);
	}
	if (C)
	{
		PaintWaypoint(Out, LayerId, Geo);
		PaintObjective(Out, LayerId, Geo);
		PaintCritical(Out, LayerId, Geo, C);
		if (!bMinimal)
		{
			PaintSurvival(Out, LayerId, Geo, C);
			PaintBelt(Out, LayerId, Geo, C);
			PaintReadouts(Out, LayerId + 1, Geo, C);
		}
		PaintCenter(Out, LayerId, Geo, C);
		PaintVisor(Out, LayerId, Geo, C);
		if (C->IsInspecting())
		{
			PaintInspect(Out, LayerId + 5, Geo, C);
		}
	}
	PaintClock(Out, LayerId, Geo);
	if (FMarsUserSettings::Get().bSubtitles)
	{
		PaintAria(Out, LayerId, Geo);
	}
	PaintTutorial(Out, LayerId + 4, Geo);
	if (C && C->IsDiagnosticVisible())
	{
		PaintDiagnostic(Out, LayerId + 6, Geo, C);
	}
	PaintSleep(Out, LayerId + 9, Geo, C);
	if (const UMarsMissionDirector* Ops = UMarsMissionDirector::Get(this); Ops && Ops->GetTransitionFade() > 0.0f)
	{
		MarsUI::Box(Out, LayerId + 9, Geo, FVector2D::ZeroVector, Geo.GetLocalSize(), FLinearColor(0, 0, 0, Ops->GetTransitionFade()));
	}
	PaintMissionEnd(Out, LayerId + 10, Geo);
	return LayerId + 20;
}

// ----------------------------------------------------------------------------
// Visual language: status colours, dot-matrix icons, gauges
// ----------------------------------------------------------------------------

FLinearColor UMarsPlayerHUD::StatusColor(int32 Status, float InTime)
{
	if (Status >= 2)
	{
		return FMath::Frac(InTime * 2.0f) < 0.6f ? MarsUI::Alarm : MarsUI::Alarm * 0.35f;
	}
	return Status == 1 ? MarsUI::Amber : MarsUI::Phosphor;
}

namespace
{
	bool IconMask(int32 Icon, float U, float V)
	{
		const float X = U - 0.5f, Y = V - 0.5f;
		const float R = FMath::Sqrt(X * X + Y * Y);
		switch (Icon)
		{
		case 0: // MCP helmet: ring + visor
			return (R > 0.36f && R < 0.47f) || (FMath::Abs(Y + 0.02f) < 0.1f && FMath::Abs(X) < 0.27f);
		case 1: // radiation trefoil
		{
			if (R < 0.09f) { return true; }
			if (R < 0.16f || R > 0.46f) { return false; }
			float A = FMath::RadiansToDegrees(FMath::Atan2(Y, X)) + 90.0f;
			A = FMath::Fmod(A + 360.0f, 120.0f);
			return A < 60.0f;
		}
		case 2: // medical cross
			return (FMath::Abs(X) < 0.13f && FMath::Abs(Y) < 0.42f) || (FMath::Abs(Y) < 0.13f && FMath::Abs(X) < 0.42f);
		default: // wrench: diagonal handle + open jaw
		{
			const float D = FMath::Abs(X + Y) / 1.4142f;
			const bool bHandle = D < 0.08f && X > -0.3f && X < 0.45f && Y < 0.3f;
			const float Hx = X + 0.28f, Hy = Y + 0.28f;
			const float Hr = FMath::Sqrt(Hx * Hx + Hy * Hy);
			const bool bHead = Hr < 0.2f && !(Hx < 0.0f && Hy < 0.0f && Hr > 0.08f);
			return bHandle || bHead;
		}
		}
	}
}

void UMarsPlayerHUD::PaintIcon(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, int32 Icon, const FVector2D& Pos, float Size, const FLinearColor& Color)
{
	const int32 N = 14;
	const float Cell = Size / N;
	for (int32 y = 0; y < N; ++y)
	{
		for (int32 x = 0; x < N; ++x)
		{
			if (IconMask(Icon, (x + 0.5f) / N, (y + 0.5f) / N))
			{
				MarsUI::Box(Out, Layer, Geo, Pos + FVector2D(x * Cell, y * Cell), FVector2D(Cell * 0.85f, Cell * 0.85f), Color);
			}
		}
	}
}

void UMarsPlayerHUD::PaintGauge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, float Width, const FString& Label,
	const FString& Value, float Fill01, int32 Status, float InTime)
{
	const FLinearColor Col = StatusColor(Status, InTime);
	const FSlateFontInfo Font = MarsUI::Mono(10, 120);
	MarsUI::Text(Out, Layer, Geo, Label, Pos, Font, FLinearColor(0.85f, 0.85f, 0.85f));
	MarsUI::Text(Out, Layer, Geo, Value, Pos + FVector2D(Width - MarsUI::Measure(Value, Font).X, 0.0f), Font, Col);
	const int32 Cells = 12;
	const float CellW = Width / Cells;
	for (int32 i = 0; i < Cells; ++i)
	{
		const bool bLit = (i + 0.5f) / Cells <= Fill01;
		MarsUI::Box(Out, Layer, Geo, Pos + FVector2D(i * CellW, 17.0f), FVector2D(CellW - 2.0f, 8.0f), bLit ? Col : FLinearColor(1, 1, 1, 0.07f));
	}
}

// ----------------------------------------------------------------------------
// Tutorial card
// ----------------------------------------------------------------------------

void UMarsPlayerHUD::PaintTutorial(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const AMarsTutorialManager* Tutorial = AMarsTutorialManager::Get(this);
	if (!Tutorial || (!Tutorial->IsActive() && Tutorial->GetCompleteFlash() <= 0.0f))
	{
		return;
	}
	const FMarsTutorialStep Step = Tutorial->GetCurrentStep();
	if (Step.Title.IsEmpty())
	{
		return;
	}

	const FVector2D Pos(34.0f, 64.0f);
	const float W = 400.0f;
	const float H = 96.0f + Step.Tasks.Num() * 22.0f;
	const float Flash = Tutorial->GetCompleteFlash();
	const FLinearColor Border = FLinearColor::LerpUsingHSV(MarsUI::Cyan * 0.6f, MarsUI::Phosphor, Flash);
	MarsUI::Box(Out, Layer, Geo, Pos, FVector2D(W, H), FLinearColor(0.0f, 0.03f, 0.05f, 0.72f));
	MarsUI::Frame(Out, Layer + 1, Geo, Pos, FVector2D(W, H), Border, 1.0f + Flash * 2.0f);

	PaintIcon(Out, Layer + 1, Geo, Step.Icon, Pos + FVector2D(12, 12), 40.0f, MarsUI::Cyan);
	MarsUI::Text(Out, Layer + 1, Geo, FString::Printf(TEXT("SUIT TRAINING  %d / %d"), Tutorial->GetStepIndex() + 1, Tutorial->GetStepCount()), Pos + FVector2D(64, 10), MarsUI::Mono(9, 220), MarsUI::Cyan * 0.8f);
	MarsUI::Text(Out, Layer + 1, Geo, Step.Title, Pos + FVector2D(64, 24), MarsUI::Mono(15, 200), FLinearColor::White);

	// One-line science note, wrapped to two lines.
	FString Sci = Step.Science;
	FString Line1 = Sci, Line2;
	if (Sci.Len() > 52)
	{
		int32 Cut = 52;
		while (Cut > 20 && Sci[Cut] != TEXT(' ')) { --Cut; }
		Line1 = Sci.Left(Cut);
		Line2 = Sci.Mid(Cut + 1);
	}
	MarsUI::Text(Out, Layer + 1, Geo, Line1, Pos + FVector2D(12, 58), MarsUI::Mono(9, 40), FLinearColor(0.75f, 0.85f, 0.9f));
	MarsUI::Text(Out, Layer + 1, Geo, Line2, Pos + FVector2D(12, 72), MarsUI::Mono(9, 40), FLinearColor(0.75f, 0.85f, 0.9f));

	float Y = Pos.Y + 92.0f;
	for (const FMarsTutorialTask& Task : Step.Tasks)
	{
		const FLinearColor Col = Task.bDone ? MarsUI::Phosphor : (Task.bOptional ? MarsUI::AmberDim * 1.5f : FLinearColor::White);
		MarsUI::Frame(Out, Layer + 1, Geo, FVector2D(Pos.X + 12, Y + 2), FVector2D(12, 12), Col);
		if (Task.bDone)
		{
			MarsUI::Box(Out, Layer + 2, Geo, FVector2D(Pos.X + 15, Y + 5), FVector2D(6, 6), Col);
		}
		MarsUI::Text(Out, Layer + 1, Geo, Task.Text + (Task.bOptional ? TEXT("  (optional)") : TEXT("")), FVector2D(Pos.X + 32, Y), MarsUI::Mono(10, 40), Col);
		Y += 22.0f;
	}
}

// ----------------------------------------------------------------------------
// Med-Bay visual diagnostic: anatomical dot-matrix body + gauges + countermeasure chips
// ----------------------------------------------------------------------------

void UMarsPlayerHUD::PaintDiagnostic(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const UPlayerBiometricsComponent* Bio = C->GetBiometrics();
	if (!Bio)
	{
		return;
	}
	const UMarsStationSubsystem* Station = GetWorld() ? GetWorld()->GetSubsystem<UMarsStationSubsystem>() : nullptr;
	const bool bAgentKnown = Station && Station->IsBioAgentCharacterized();
	const float Glitch = AMarsStationBase::GetPlayerGlitchAmount(C);
	auto G = [Glitch](const FString& S) { return AMarsStationBase::GlitchText(S, Glitch * 0.6f); };

	// Status per organ system.
	const float MetHb = Bio->MethemoglobinPercentage;
	const int32 BloodS = MetHb >= 25.0f ? 2 : MetHb >= 3.0f ? 1 : 0;
	const float SpO2 = Bio->EffectiveO2SaturationPct;
	const int32 LungS = (SpO2 < 80.0f || Bio->Co2LevelPpm > 5000.0f) ? 2 : (SpO2 < 92.0f || Bio->Co2LevelPpm > 2000.0f) ? 1 : 0;
	const int32 HeartS = Bio->HeartRateBpm > 140.0f ? 2 : Bio->HeartRateBpm > 105.0f ? 1 : 0;
	const int32 BrainS = Bio->ExtremophileBioLoad >= 0.7f ? 2 : Bio->ExtremophileBioLoad >= 0.15f ? 1 : 0;
	const int32 ThyroidS = Bio->ThyroidFunction < 0.6f ? 2 : Bio->ThyroidFunction < 0.85f ? 1 : 0;
	const int32 RadS = Bio->RadiationInjuryMsv >= 400.0f ? 2 : Bio->RadiationInjuryMsv >= 50.0f ? 1 : 0;
	const int32 Stage = Bio->GetStageIndex();

	const FVector2D Size = Geo.GetLocalSize();
	const float W = 640.0f, H = 470.0f;
	const FVector2D P(Size.X - W - 36.0f, Size.Y * 0.1f);
	MarsUI::Box(Out, Layer, Geo, P, FVector2D(W, H), FLinearColor(0.0f, 0.03f, 0.03f, 0.88f));
	MarsUI::Frame(Out, Layer + 1, Geo, P, FVector2D(W, H), StatusColor(FMath::Min(Stage, 2), Time) * 0.8f, 1.5f);
	MarsUI::Text(Out, Layer + 1, Geo, G(TEXT("MED-BAY  //  BIOMETRIC SCAN")), P + FVector2D(16, 12), MarsUI::Mono(10, 220), MarsUI::Cyan);

	// Stage badge.
	static const TCHAR* StageNames[] = { TEXT("STABLE"), TEXT("PRODROMAL HYPOXIA"), TEXT("ACUTE METHAEMOGLOBINAEMIA"), TEXT("NEURO-TOXIC ENCEPHALOPATHY"), TEXT("SYSTEMIC COLLAPSE") };
	const FLinearColor StageCol = StatusColor(Stage >= 3 ? 2 : Stage >= 1 ? 1 : 0, Time);
	MarsUI::Box(Out, Layer + 1, Geo, P + FVector2D(16, 34), FVector2D(W - 32, 40), StageCol * FLinearColor(1, 1, 1, 0.18f));
	MarsUI::Text(Out, Layer + 2, Geo, G(FString::Printf(TEXT("STAGE %d"), Stage)), P + FVector2D(28, 41), MarsUI::Mono(18, 200), StageCol);
	MarsUI::Text(Out, Layer + 2, Geo, G(StageNames[Stage]), P + FVector2D(160, 46), MarsUI::Mono(11, 120), FLinearColor::White);
	if (Stage >= 4)
	{
		MarsUI::Text(Out, Layer + 2, Geo, FString::Printf(TEXT("%.0f s"), Bio->GetStage4SecondsRemaining()), P + FVector2D(W - 90, 44), MarsUI::Mono(14, 100), MarsUI::Alarm);
	}

	// Anatomical dot-matrix body (24 x 44 cells).
	const FVector2D BodyPos = P + FVector2D(24, 92);
	const float Cell = 7.0f;
	for (int32 y = 0; y < 44; ++y)
	{
		for (int32 x = 0; x < 24; ++x)
		{
			const float U = (x + 0.5f) / 24.0f - 0.5f;
			const float V = (y + 0.5f) / 44.0f;
			int32 Zone = -1; // 0 brain, 1 thyroid, 2 lungs, 3 heart, 4 blood/limbs, 5 torso
			if (FMath::Square(U / 0.17f) + FMath::Square((V - 0.08f) / 0.07f) < 1.0f) { Zone = 0; }
			else if (FMath::Abs(U) < 0.06f && V > 0.15f && V < 0.2f) { Zone = 1; }
			else if (V > 0.2f && V < 0.55f && FMath::Abs(U) < 0.24f)
			{
				const bool bLung = V > 0.24f && V < 0.42f && FMath::Abs(U) > 0.05f && FMath::Abs(U) < 0.2f;
				const bool bHeart = FMath::Square((U + 0.03f) / 0.07f) + FMath::Square((V - 0.36f) / 0.04f) < 1.0f;
				Zone = bHeart ? 3 : bLung ? 2 : 5;
			}
			else if (V > 0.21f && V < 0.55f && FMath::Abs(U) > 0.27f && FMath::Abs(U) < 0.36f) { Zone = 4; }      // arms
			else if (V >= 0.55f && V < 0.98f && FMath::Abs(U) > 0.04f && FMath::Abs(U) < 0.2f) { Zone = 4; }       // legs
			if (Zone < 0)
			{
				continue;
			}
			int32 S = 0;
			switch (Zone)
			{
			case 0: S = BrainS; break;
			case 1: S = ThyroidS; break;
			case 2: S = LungS; break;
			case 3: S = HeartS; break;
			case 4: S = BloodS; break;
			default: S = RadS; break;
			}
			FLinearColor Col = StatusColor(S, Time + Zone * 0.13f);
			if (Zone == 4 && MetHb >= 15.0f)
			{
				Col = FLinearColor(0.45f, 0.22f, 0.08f); // "chocolate blood"
			}
			if (Zone == 3)
			{
				Col *= 0.6f + 0.4f * FMath::Max(FMath::Sin(Time * Bio->HeartRateBpm / 60.0f * 2.0f * PI), 0.0f);
			}
			MarsUI::Box(Out, Layer + 2, Geo, BodyPos + FVector2D(x * Cell, y * Cell), FVector2D(Cell - 1.5f, Cell - 1.5f), Col * FLinearColor(1, 1, 1, Zone == 5 ? 0.45f : 1.0f));
		}
	}
	// Whole-body radiation halo.
	if (RadS > 0)
	{
		MarsUI::Frame(Out, Layer + 1, Geo, BodyPos - FVector2D(6, 6), FVector2D(24 * Cell + 12, 44 * Cell + 12), StatusColor(RadS, Time) * 0.7f);
	}
	MarsUI::Text(Out, Layer + 2, Geo, TEXT("BRAIN / THYROID / LUNGS / HEART / BLOOD"), BodyPos + FVector2D(-6, 44 * Cell + 12), MarsUI::Mono(7, 60), FLinearColor(0.6f, 0.6f, 0.6f));

	// Gauges.
	const FVector2D GPos = P + FVector2D(220, 92);
	const float GW = W - 240.0f;
	float GY = GPos.Y;
	auto Gauge = [&](const TCHAR* Label, const FString& Value, float Fill, int32 Status)
	{
		PaintGauge(Out, Layer + 2, Geo, FVector2D(GPos.X, GY), GW, G(Label), Value, FMath::Clamp(Fill, 0.0f, 1.0f), Status, Time);
		GY += 36.0f;
	};
	Gauge(TEXT("METHAEMOGLOBIN"), FString::Printf(TEXT("%.1f %%"), MetHb), MetHb / 50.0f, BloodS);
	Gauge(TEXT("O2 SATURATION"), FString::Printf(TEXT("%.0f %%"), SpO2), SpO2 / 100.0f, LungS);
	Gauge(TEXT("INSPIRED CO2"), FString::Printf(TEXT("%.0f ppm"), Bio->Co2LevelPpm), Bio->Co2LevelPpm / 10000.0f, Bio->Co2LevelPpm > 5000.0f ? 2 : Bio->Co2LevelPpm > 2000.0f ? 1 : 0);
	Gauge(TEXT("RADIATION INJURY"), FString::Printf(TEXT("%.0f mSv"), Bio->RadiationInjuryMsv), Bio->RadiationInjuryMsv / 800.0f, RadS);
	Gauge(TEXT("BIO-AGENT LOAD"), bAgentKnown ? FString::Printf(TEXT("%.0f %%"), Bio->ExtremophileBioLoad * 100.0f) : FString(TEXT("UNKNOWN")), bAgentKnown ? Bio->ExtremophileBioLoad : 0.5f, bAgentKnown ? BrainS : 1);
	Gauge(TEXT("THYROID (NIS)"), FString::Printf(TEXT("%.0f %%"), Bio->ThyroidFunction * 100.0f), Bio->ThyroidFunction, ThyroidS);
	Gauge(TEXT("HEART RATE"), FString::Printf(TEXT("%.0f bpm"), Bio->HeartRateBpm), Bio->HeartRateBpm / 200.0f, HeartS);
	Gauge(TEXT("HYDRATION"), FString::Printf(TEXT("%.0f %%"), Bio->Hydration * 100.0f), Bio->Hydration, Bio->Hydration < 0.15f ? 2 : Bio->Hydration < 0.3f ? 1 : 0);

	// Countermeasure chips.
	float CX = P.X + 16.0f;
	const float CY = P.Y + H - 40.0f;
	MarsUI::Text(Out, Layer + 2, Geo, TEXT("TREAT:"), FVector2D(CX, CY + 6), MarsUI::Mono(9, 160), MarsUI::Cyan);
	CX += 64.0f;
	for (FString Item : Bio->GetIndicatedCountermeasures())
	{
		int32 Paren;
		if (Item.FindChar(TEXT('('), Paren))
		{
			Item = Item.Left(Paren);
		}
		Item.TrimStartAndEndInline();
		Item = Item.Replace(TEXT("!! "), TEXT("")).Replace(TEXT(" !!"), TEXT(""));
		if (Item.Len() > 22)
		{
			Item = Item.Left(22);
		}
		const FSlateFontInfo ChipFont = MarsUI::Mono(9, 80);
		const float CW = MarsUI::Measure(Item, ChipFont).X + 16.0f;
		if (CX + CW > P.X + W - 12.0f)
		{
			break;
		}
		const bool bUrgent = Item.Contains(TEXT("STABILIS"));
		const FLinearColor ChipCol = bUrgent ? StatusColor(2, Time) : MarsUI::Amber;
		MarsUI::Box(Out, Layer + 2, Geo, FVector2D(CX, CY), FVector2D(CW, 24), ChipCol * FLinearColor(1, 1, 1, 0.2f));
		MarsUI::Frame(Out, Layer + 3, Geo, FVector2D(CX, CY), FVector2D(CW, 24), ChipCol);
		MarsUI::Text(Out, Layer + 3, Geo, G(Item), FVector2D(CX + 8, CY + 5), ChipFont, FLinearColor::White, false);
		CX += CW + 8.0f;
	}
}

void UMarsPlayerHUD::PaintSurvival(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const UPlayerBiometricsComponent* Bio = C->GetBiometrics();
	if (!Bio)
	{
		return;
	}
	const FVector2D Size = Geo.GetLocalSize();
	const FSlateFontInfo Label = MarsUI::Mono(10, 200);
	const float BarW = 170.0f, BarH = 7.0f;
	float Y = Size.Y - 78.0f;
	const float X = 34.0f;

	auto Bar = [&](const TCHAR* Name, float Value, const FLinearColor& Color)
	{
		const bool bLow = Value < 0.25f;
		const float Blink = bLow && FMath::Frac(Time * 1.5f) < 0.5f ? 0.35f : 1.0f;
		const FLinearColor Col = (bLow ? MarsUI::Alarm : Color) * Blink;
		MarsUI::Text(Out, Layer, Geo, FString::Printf(TEXT("%-5s %3.0f"), Name, Value * 100.0f), FVector2D(X, Y), Label, Col);
		Y += 16.0f;
		MarsUI::Box(Out, Layer, Geo, FVector2D(X, Y), FVector2D(BarW, BarH), FLinearColor(0, 0, 0, 0.55f));
		// Segmented fill (10 cells), DSKY style.
		const int32 Cells = 10;
		const float CellW = BarW / Cells;
		for (int32 i = 0; i < Cells; ++i)
		{
			if ((i + 0.5f) / Cells <= Value)
			{
				MarsUI::Box(Out, Layer + 1, Geo, FVector2D(X + i * CellW + 1.0f, Y + 1.0f), FVector2D(CellW - 2.0f, BarH - 2.0f), Col);
			}
		}
		Y += 18.0f;
	};
	Bar(TEXT("H2O"), Bio->Hydration, MarsUI::Cyan);
	Bar(TEXT("FOOD"), Bio->Nutrition, MarsUI::Amber);
}

void UMarsPlayerHUD::PaintBelt(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const UMarsInventoryComponent* Inv = C->GetInventory();
	if (!Inv)
	{
		return;
	}
	const FVector2D Size = Geo.GetLocalSize();
	const TArray<FMarsBeltSlot> Belt = C->GetBeltSlots();
	const float SlotW = 86.0f, SlotH = 48.0f, Gap = 6.0f;
	const float TotalW = Belt.Num() * SlotW + (Belt.Num() - 1) * Gap;
	const float X0 = Size.X * 0.5f - TotalW * 0.5f;
	const float Y0 = Size.Y - SlotH - 26.0f;
	const FSlateFontInfo Key = MarsUI::Mono(9, 100);
	const FSlateFontInfo Name = MarsUI::Mono(11, 140);

	const AMarsEquipment* Equipped = C->GetEquippedItem();
	for (int32 i = 0; i < Belt.Num(); ++i)
	{
		const FMarsBeltSlot& BeltSlot = Belt[i];
		const FVector2D P(X0 + i * (SlotW + Gap), Y0);
		const int32 Count = Inv->GetItemCount(BeltSlot.ItemType);
		const bool bActive = Equipped && Equipped->ItemType == BeltSlot.ItemType;
		const FLinearColor Col = Count > 0 ? (bActive ? MarsUI::Amber : MarsUI::Phosphor) : MarsUI::AmberDim * 0.6f;

		MarsUI::Box(Out, Layer, Geo, P, FVector2D(SlotW, SlotH), FLinearColor(0, 0, 0, bActive ? 0.65f : 0.4f));
		MarsUI::Frame(Out, Layer + 1, Geo, P, FVector2D(SlotW, SlotH), Col, bActive ? 2.0f : 1.0f);
		MarsUI::Text(Out, Layer + 2, Geo, FString::Printf(TEXT("%d"), i + 1), P + FVector2D(5, 3), Key, Col);
		MarsUI::TextCentered(Out, Layer + 2, Geo, BeltSlot.ShortName, P + FVector2D(SlotW * 0.5f, 15.0f), Name, Col);
		if (BeltSlot.bShowCount)
		{
			MarsUI::TextCentered(Out, Layer + 2, Geo, FString::Printf(TEXT("x%d"), Count), P + FVector2D(SlotW * 0.5f, 31.0f), Key, Col);
		}
	}

	// Med-kit / mission items line under the belt.
	const FString Med = C->GetSelectedMedicationLabel();
	const int32 Transistors = Inv->GetItemCount(EMarsItemType::QuantumTransistor);
	FString Line = FString::Printf(TEXT("MED [H]: %s"), *Med);
	if (Transistors > 0)
	{
		Line += FString::Printf(TEXT("    QT x%d"), Transistors);
	}
	if (Inv->HasItem(EMarsItemType::BioAgentCulture))
	{
		Line += TEXT("    CULTURE x1");
	}
	MarsUI::TextCentered(Out, Layer + 2, Geo, Line, FVector2D(Size.X * 0.5f, Y0 + SlotH + 6.0f), Key, MarsUI::AmberDim * 1.6f);

	// Status of the tool in hand sits right above the belt.
	if (Equipped)
	{
		MarsUI::TextCentered(Out, Layer + 2, Geo, Equipped->GetStatusText().ToString(), FVector2D(Size.X * 0.5f, Y0 - 22.0f), MarsUI::Mono(11, 120), MarsUI::Phosphor);
	}
}

void UMarsPlayerHUD::PaintCenter(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const FVector2D Mid = Size * 0.5f;
	MarsUI::Box(Out, Layer, Geo, Mid - FVector2D(1.5f, 1.5f), FVector2D(3.0f, 3.0f), FLinearColor(1, 1, 1, 0.7f));

	const FSlateFontInfo PromptFont = MarsUI::Mono(12, 90);
	float Y = Mid.Y + 22.0f;
	const FText Prompt = C->GetFocusPrompt();
	if (!Prompt.IsEmpty())
	{
		MarsUI::TextCentered(Out, Layer, Geo, Prompt.ToString(), FVector2D(Mid.X, Y), PromptFont, FLinearColor::White);
		Y += 22.0f;
	}

	Y += 14.0f;
	for (const FMarsNotification& N : C->GetNotifications())
	{
		const float Alpha = FMath::Clamp(N.RemainingSeconds, 0.0f, 1.0f);
		MarsUI::TextCentered(Out, Layer, Geo, N.Text.ToString(), FVector2D(Mid.X, Y), MarsUI::Mono(11, 90), FLinearColor(1.0f, 0.88f, 0.65f, Alpha));
		Y += 18.0f;
	}

	// Terminal read-out panel (scanner, spectrometer...). Text glitches at Stage 3+.
	const FString Panel = C->GetPanelText();
	if (!Panel.IsEmpty())
	{
		const FString Shown = AMarsStationBase::GlitchText(Panel, AMarsStationBase::GetPlayerGlitchAmount(C));
		TArray<FString> Lines;
		Shown.ParseIntoArray(Lines, TEXT("\n"), false);
		const FSlateFontInfo PanelFont = MarsUI::Mono(11, 40);
		const float W = 520.0f;
		const float H = Lines.Num() * 17.0f + 24.0f;
		const FVector2D P(Size.X - W - 40.0f, Size.Y * 0.12f);
		MarsUI::Box(Out, Layer + 2, Geo, P, FVector2D(W, H), FLinearColor(0.0f, 0.04f, 0.02f, 0.86f));
		MarsUI::Frame(Out, Layer + 3, Geo, P, FVector2D(W, H), MarsUI::Phosphor * 0.6f);
		float LY = P.Y + 12.0f;
		for (const FString& L : Lines)
		{
			MarsUI::Text(Out, Layer + 4, Geo, L, FVector2D(P.X + 14.0f, LY), PanelFont, MarsUI::Phosphor, false);
			LY += 17.0f;
		}
	}
}

void UMarsPlayerHUD::PaintVisor(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const UEVASuitComponent* Suit = C->GetSuit();
	const UPlayerBiometricsComponent* Bio = C->GetBiometrics();
	const FVector2D Size = Geo.GetLocalSize();
	const FSlateFontInfo Visor = MarsUI::Mono(10, 160);

	// Suit biomonitor alarm (Stage 4) - an alarm, not a number.
	if (Bio && Bio->GetStageIndex() >= 4 && FMath::Frac(Time * 2.0f) < 0.6f)
	{
		MarsUI::TextCentered(Out, Layer, Geo, TEXT("!! BIOMONITOR ALARM - SEEK MED-BAY STABILISATION !!"), FVector2D(Size.X * 0.5f, 26.0f), MarsUI::Mono(12, 200), MarsUI::Alarm);
	}

	if (!Suit || !Suit->IsHelmetSealed())
	{
		return;
	}
	// Helmet visor frame: dark rim closing the corners + thin HUD bezel lines.
	{
		const FLinearColor Rim(0.0f, 0.0f, 0.0f, 0.55f);
		const float Ch = Size.Y * 0.16f;
		const FVector2D Corners[4] = { FVector2D(0, 0), FVector2D(Size.X, 0), FVector2D(Size.X, Size.Y), FVector2D(0, Size.Y) };
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector2D A = Corners[k] + FVector2D(k == 1 || k == 2 ? -Ch * 1.6f : Ch * 1.6f, 0.0f);
			const FVector2D B = Corners[k] + FVector2D(0.0f, k >= 2 ? -Ch : Ch);
			const FVector2D Out2 = (Corners[k] - (A + B) * 0.5f).GetSafeNormal();
			TArray<FVector2f> Pts = { FVector2f(A + Out2 * Ch * 0.35f), FVector2f(B + Out2 * Ch * 0.35f) };
			FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Rim, false, Ch * 0.7f);
			Pts = { FVector2f(A), FVector2f(B) };
			FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, MarsUI::Cyan * FLinearColor(1, 1, 1, 0.18f), true, 1.5f);
		}
		MarsUI::Box(Out, Layer, Geo, FVector2D(Size.X * 0.5f - 60.0f, Size.Y - 6.0f), FVector2D(120.0f, 2.0f), MarsUI::Cyan * FLinearColor(1, 1, 1, 0.25f));
		MarsUI::Text(Out, Layer, Geo, TEXT("MCP-4  //  G: WRIST"), FVector2D(Size.X - 190.0f, Size.Y - 24.0f), MarsUI::Mono(8, 160), MarsUI::Cyan * FLinearColor(1, 1, 1, 0.45f));
	}

	// Helmet visor ring: only what the MCP helmet itself displays.
	const float O2 = Suit->GetO2Fraction01();
	const FLinearColor O2Col = O2 < 0.1f ? MarsUI::Alarm : O2 < 0.25f ? MarsUI::Amber : MarsUI::Cyan;
	if (bMinimal && O2 >= 0.25f)
	{
		return;
	}
	MarsUI::Text(Out, Layer, Geo, FString::Printf(TEXT("VISOR  O2 %3.0f %%  //  %3.0f L  /  %.1f MIN LEFT"), O2 * 100.0f, Suit->O2RemainingLiters, Suit->GetO2MinutesRemainingReal()),
		FVector2D(34.0f, 26.0f), Visor, O2Col);
	// Segmented suit O2 bar under the readout.
	for (int32 i = 0; i < 20; ++i)
	{
		const bool bLit = i < FMath::CeilToInt(O2 * 20.0f);
		MarsUI::Box(Out, Layer, Geo, FVector2D(34.0f + i * 9.0f, 42.0f + (Suit->DustLoad > 0.05f ? 16.0f : 0.0f)), FVector2D(7.0f, 4.0f), bLit ? O2Col : O2Col * FLinearColor(1, 1, 1, 0.15f));
	}
	if (Suit->DustLoad > 0.05f)
	{
		MarsUI::Text(Out, Layer, Geo, FString::Printf(TEXT("SUIT DUST %2.0f%%  DECON REQUIRED"), Suit->DustLoad * 100.0f), FVector2D(34.0f, 42.0f), Visor, MarsUI::Amber);
	}

	// Beacon compass strip.
	if (!C->GetCurrentModule())
	{
		const float ViewYaw = C->GetControlRotation().Yaw;
		const float StripW = 420.0f;
		const float SX = Size.X * 0.5f - StripW * 0.5f;
		const float SY = 54.0f;
		MarsUI::Box(Out, Layer, Geo, FVector2D(SX, SY), FVector2D(StripW, 1.0f), MarsUI::Cyan * 0.5f);
		for (const ASurfaceBeacon* Beacon : C->GetKnownBeacons())
		{
			if (!Beacon || !Beacon->IsActive())
			{
				continue;
			}
			const FVector Delta = Beacon->GetActorLocation() - C->GetActorLocation();
			if (Delta.Size() > Beacon->RadioRangeCm)
			{
				continue;
			}
			const float Bearing = FRotator::NormalizeAxis(Delta.Rotation().Yaw - ViewYaw);
			if (FMath::Abs(Bearing) > 90.0f)
			{
				continue;
			}
			const float BX = SX + StripW * 0.5f + Bearing / 90.0f * StripW * 0.5f;
			MarsUI::Box(Out, Layer + 1, Geo, FVector2D(BX - 1.0f, SY - 6.0f), FVector2D(2.0f, 12.0f), MarsUI::Cyan);
			MarsUI::TextCentered(Out, Layer + 1, Geo, FString::Printf(TEXT("%s %.0fm"), *Beacon->BeaconLabel, Delta.Size() / 100.0f), FVector2D(BX, SY + 8.0f), Visor, MarsUI::Cyan);
		}
	}
}

void UMarsPlayerHUD::PaintAria(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const UWorld* World = GetWorld();
	const UAriaSubsystem* Aria = World ? World->GetSubsystem<UAriaSubsystem>() : nullptr;
	if (!Aria)
	{
		return;
	}
	const TArray<FAriaMessage> Log = Aria->GetMessageLog();
	if (Log.Num() == 0)
	{
		return;
	}
	const FAriaMessage& Last = Log.Last();
	const float Age = World->GetTimeSeconds() - Last.WorldTime;
	const float Alpha = FMath::Clamp(1.0f - (Age - 14.0f) / 3.0f, 0.0f, 1.0f);
	if (Alpha <= 0.0f && !Aria->IsAwaitingResponse())
	{
		return;
	}

	const FVector2D Size = Geo.GetLocalSize();
	const FSlateFontInfo Font = MarsUI::Mono(11, 60);
	const int32 Visible = FMath::Clamp(FMath::FloorToInt(Age * 45.0f), 0, Last.Text.Len());
	TArray<FString> Words;
	Last.Text.Left(Visible).ParseIntoArray(Words, TEXT(" "));
	TArray<FString> Lines;
	FString Line;
	for (const FString& W : Words)
	{
		if (Line.Len() + W.Len() + 1 > 88)
		{
			Lines.Add(Line);
			Line.Reset();
		}
		Line += Line.IsEmpty() ? W : TEXT(" ") + W;
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}

	float Y = 70.0f;
	const FLinearColor Col = MarsUI::Cyan * FLinearColor(1, 1, 1, Aria->IsAwaitingResponse() ? 1.0f : Alpha);
	MarsUI::TextCentered(Out, Layer, Geo, Aria->IsAwaitingResponse() ? TEXT("A.R.I.A.  [ UPLINK - PROCESSING TELEMETRY ]") : (Last.bOffline ? TEXT("A.R.I.A.  [ LOCAL ]") : TEXT("A.R.I.A.  [ UPLINK ]")),
		FVector2D(Size.X * 0.5f, Y), MarsUI::Mono(9, 220), Col);
	Y += 18.0f;
	for (const FString& L : Lines)
	{
		MarsUI::TextCentered(Out, Layer, Geo, L, FVector2D(Size.X * 0.5f, Y), Font, Col);
		Y += 17.0f;
	}
}

void UMarsPlayerHUD::PaintMissionEnd(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const AMarsGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMarsGameMode>() : nullptr;
	if (!GM || !GM->IsMissionOver())
	{
		return;
	}
	const FVector2D Size = Geo.GetLocalSize();
	MarsUI::Box(Out, Layer, Geo, FVector2D(0, Size.Y * 0.38f), FVector2D(Size.X, 120.0f), FLinearColor(0, 0, 0, 0.85f));
	MarsUI::TextCentered(Out, Layer + 1, Geo, GM->IsVictory() ? TEXT("UPLINK ESTABLISHED") : TEXT("SIGNAL LOST"),
		FVector2D(Size.X * 0.5f, Size.Y * 0.38f + 20.0f), MarsUI::Mono(26, 300), GM->IsVictory() ? MarsUI::Phosphor : MarsUI::Alarm);
	MarsUI::TextCentered(Out, Layer + 1, Geo, GM->GetEndReason(), FVector2D(Size.X * 0.5f, Size.Y * 0.38f + 72.0f), MarsUI::Mono(12, 100), FLinearColor::White);
}

// ----------------------------------------------------------------------------
// Critical state vignette, weapon / radiation readouts, item inspection card
// ----------------------------------------------------------------------------

void UMarsPlayerHUD::PaintCritical(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const UMarsAudioManager* Audio = C->GetAudioManager();
	const float K = Audio ? Audio->GetCriticalFactor() : 0.0f;
	if (K <= 0.01f || C->IsDead())
	{
		return;
	}
	const FVector2D Size = Geo.GetLocalSize();
	const float Bpm = C->GetBiometrics() ? C->GetBiometrics()->HeartRateBpm : 90.0f;
	const float Beat = FMath::Pow(FMath::Max(FMath::Sin(Time * Bpm / 60.0f * PI), 0.0f), 6.0f);
	const float Strength = K * (0.55f + 0.45f * Beat);
	// Layered edge bands = soft red vignette closing in (tunnel vision).
	const int32 Bands = 14;
	const float MaxDepth = FMath::Min(Size.X, Size.Y) * (0.12f + 0.22f * K);
	for (int32 i = 0; i < Bands; ++i)
	{
		const float D = MaxDepth * (1.0f - static_cast<float>(i) / Bands);
		const FLinearColor Col(0.35f, 0.0f, 0.0f, 0.09f * Strength);
		MarsUI::Box(Out, Layer, Geo, FVector2D(0, 0), FVector2D(Size.X, D * 0.6f), Col);
		MarsUI::Box(Out, Layer, Geo, FVector2D(0, Size.Y - D * 0.6f), FVector2D(Size.X, D * 0.6f), Col);
		MarsUI::Box(Out, Layer, Geo, FVector2D(0, 0), FVector2D(D, Size.Y), Col);
		MarsUI::Box(Out, Layer, Geo, FVector2D(Size.X - D, 0), FVector2D(D, Size.Y), Col);
	}
	if (K > 0.5f && FMath::Frac(Time * 1.5f) < 0.6f)
	{
		const UEVASuitComponent* Suit = C->GetSuit();
		const bool bO2 = Suit && Suit->IsSuitDonned() && Suit->GetO2Fraction01() < 0.15f;
		MarsUI::TextCentered(Out, Layer + 1, Geo, bO2 ? TEXT("!! SUIT O2 CRITICAL - Q: O2 CANISTER / RETURN TO AIRLOCK !!") : TEXT("!! HYPOXIA - FIND OXYGEN !!"),
			FVector2D(Size.X * 0.5f, Size.Y * 0.18f), MarsUI::Mono(12, 200), MarsUI::Alarm);
	}
}

void UMarsPlayerHUD::PaintReadouts(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const FVector2D Size = Geo.GetLocalSize();

	// Ammunition (Glock-18): big digits + magazine column, bottom right.
	if (const AFuturisticGlock18* Glock = Cast<AFuturisticGlock18>(C->GetEquippedItem()))
	{
		const int32 Reserve = C->GetInventory() ? C->GetInventory()->GetItemCount(EMarsItemType::Ammo9mm) : 0;
		const float Frac = Glock->MagazineCapacity > 0 ? static_cast<float>(Glock->RoundsInMagazine) / Glock->MagazineCapacity : 0.0f;
		const FLinearColor Col = Glock->RoundsInMagazine == 0 ? StatusColor(2, Time) : Frac < 0.25f ? MarsUI::Amber : MarsUI::Phosphor;
		const FVector2D P(Size.X - 250.0f, Size.Y - 120.0f);
		MarsUI::Text(Out, Layer, Geo, FString::Printf(TEXT("%02d"), Glock->RoundsInMagazine), P, MarsUI::Mono(30, 120), Col);
		MarsUI::Text(Out, Layer, Geo, FString::Printf(TEXT("/ %d"), Reserve), P + FVector2D(74.0f, 18.0f), MarsUI::Mono(12, 120), MarsUI::AmberDim * 1.6f);
		MarsUI::Text(Out, Layer, Geo, Glock->bFullAuto ? TEXT("9MM  //  AUTO") : TEXT("9MM  //  SEMI"), P + FVector2D(0.0f, 48.0f), MarsUI::Mono(8, 200), MarsUI::AmberDim * 1.6f);
		for (int32 i = 0; i < Glock->MagazineCapacity; ++i)
		{
			const bool bLit = i < Glock->RoundsInMagazine;
			MarsUI::Box(Out, Layer, Geo, FVector2D(P.X + 150.0f + (i % 3) * 7.0f, P.Y + 56.0f - (i / 3) * 6.0f), FVector2D(5.0f, 4.0f),
				bLit ? Col : Col * FLinearColor(1, 1, 1, 0.12f));
		}
		if (Glock->RoundsInMagazine == 0)
		{
			MarsUI::Text(Out, Layer, Geo, Reserve > 0 ? TEXT("[R] RELOAD") : TEXT("NO AMMUNITION"), P + FVector2D(0.0f, 64.0f), MarsUI::Mono(9, 200), StatusColor(2, Time));
		}
	}

	// Radiation warning ticks (needs the dosimeter on you), top right.
	const UPlayerBiometricsComponent* Bio = C->GetBiometrics();
	const bool bDosimeter = C->GetInventory() && C->GetInventory()->HasItem(EMarsItemType::RadiationDosimeter);
	if (Bio && bDosimeter)
	{
		const float Rate = Bio->CurrentDoseRateMsvPerHour;
		// 10 ticks on a log scale: 0.01 mSv/h .. 10 mSv/h.
		const int32 Lit = FMath::Clamp(FMath::CeilToInt((FMath::LogX(10.0f, FMath::Max(Rate, 0.001f)) + 2.0f) / 3.0f * 10.0f), 0, 10);
		if (Lit >= 3)
		{
			const int32 Status = Lit >= 8 ? 2 : Lit >= 5 ? 1 : 0;
			const FLinearColor Col = StatusColor(Status, Time);
			const FVector2D P(Size.X - 250.0f, 26.0f);
			MarsUI::Text(Out, Layer, Geo, FString::Printf(TEXT("RAD %.2f mSv/h"), Rate), P, MarsUI::Mono(10, 160), Col);
			for (int32 i = 0; i < 10; ++i)
			{
				const float H = 4.0f + i * 1.2f;
				MarsUI::Box(Out, Layer, Geo, FVector2D(P.X + i * 9.0f, P.Y + 34.0f - H), FVector2D(6.0f, H), i < Lit ? (i >= 7 ? MarsUI::Alarm : i >= 4 ? MarsUI::Amber : MarsUI::Phosphor) : Col * FLinearColor(1, 1, 1, 0.12f));
			}
		}
	}
}

void UMarsPlayerHUD::PaintInspect(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const FMarsItemInfo Info = UMarsItemLibrary::GetItemInfo(C->GetInspectItem());
	const FVector2D PanelSize(380.0f, 200.0f);
	const FVector2D P(Size.X * 0.66f, Size.Y * 0.5f - PanelSize.Y * 0.5f);
	MarsUI::Box(Out, Layer, Geo, P, PanelSize, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	MarsUI::Frame(Out, Layer + 1, Geo, P, PanelSize, MarsUI::Amber * FLinearColor(1, 1, 1, 0.7f));
	MarsUI::Text(Out, Layer + 1, Geo, TEXT("NEW ITEM  //  INSPECT"), P + FVector2D(16, 14), MarsUI::Mono(9, 200), MarsUI::AmberDim * 1.6f);
	MarsUI::Text(Out, Layer + 1, Geo, Info.DisplayName.ToString().ToUpper(), P + FVector2D(16, 34), MarsUI::Mono(15, 160), MarsUI::Amber);

	// Wrap the description.
	TArray<FString> Words;
	Info.Description.ToString().ParseIntoArray(Words, TEXT(" "));
	const FSlateFontInfo F = MarsUI::Mono(10, 40);
	FString Line;
	float Y = P.Y + 66.0f;
	for (const FString& W : Words)
	{
		const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
		if (MarsUI::Measure(Try, F).X > PanelSize.X - 32.0f && !Line.IsEmpty())
		{
			MarsUI::Text(Out, Layer + 1, Geo, Line, FVector2D(P.X + 16.0f, Y), F, MarsUI::Phosphor);
			Y += 16.0f;
			Line = W;
		}
		else
		{
			Line = Try;
		}
	}
	if (!Line.IsEmpty())
	{
		MarsUI::Text(Out, Layer + 1, Geo, Line, FVector2D(P.X + 16.0f, Y), F, MarsUI::Phosphor);
	}
	MarsUI::Text(Out, Layer + 1, Geo, FString::Printf(TEXT("MASS %.2f KG"), Info.MassKg), P + FVector2D(16, PanelSize.Y - 46.0f), MarsUI::Mono(9, 160), MarsUI::AmberDim * 1.6f);
	MarsUI::Text(Out, Layer + 1, Geo, TEXT("MOUSE  ROTATE     E / CLICK  STORE"), P + FVector2D(16, PanelSize.Y - 26.0f), MarsUI::Mono(9, 160), MarsUI::Amber);
}

// ----------------------------------------------------------------------------
// Story objective tracker, waypoint marker, sleep fade, recon-shuttle cockpit
// ----------------------------------------------------------------------------

namespace
{
	void WrapText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, FVector2D Pos, float Width, const FSlateFontInfo& Font,
		const FLinearColor& Color, float LineH)
	{
		TArray<FString> Words;
		Text.ParseIntoArray(Words, TEXT(" "));
		FString Line;
		for (const FString& W : Words)
		{
			const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
			if (MarsUI::Measure(Try, Font).X > Width && !Line.IsEmpty())
			{
				MarsUI::Text(Out, Layer, Geo, Line, Pos, Font, Color);
				Pos.Y += LineH;
				Line = W;
			}
			else
			{
				Line = Try;
			}
		}
		if (!Line.IsEmpty())
		{
			MarsUI::Text(Out, Layer, Geo, Line, Pos, Font, Color);
		}
	}
}

void UMarsPlayerHUD::PaintObjective(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this);
	if (!Story || !Story->IsStarted())
	{
		return;
	}
	const AMarsTutorialManager* Tut = AMarsTutorialManager::Get(this);
	if (Tut && Tut->IsActive())
	{
		return; // the training card is the objective for now
	}
	const float Flash = Story->GetObjectiveFlash();
	if (bMinimal && Flash <= 0.0f)
	{
		return;
	}
	const FVector2D Size = Geo.GetLocalSize();
	const FVector2D Pos(Size.X - 400.0f, 92.0f);
	const FLinearColor Col = FMath::Lerp(MarsUI::Amber, FLinearColor::White, Flash);
	MarsUI::Box(Out, Layer, Geo, Pos - FVector2D(10, 6), FVector2D(392, 3), Col * FLinearColor(1, 1, 1, 0.6f));
	MarsUI::Text(Out, Layer, Geo, Story->GetChapterTitle(), Pos, MarsUI::Mono(8, 200), MarsUI::AmberDim * 1.6f);
	WrapText(Out, Layer, Geo, Story->GetObjectiveText(), Pos + FVector2D(0, 16), 380.0f, MarsUI::Mono(10, 40), Col, 15.0f);
	if (!Story->AreWaypointsEnabled())
	{
		MarsUI::Text(Out, Layer, Geo, TEXT("WAYPOINTS OFF  (N)"), Pos + FVector2D(0, 64), MarsUI::Mono(7, 160), MarsUI::AmberDim);
	}
}

void UMarsPlayerHUD::PaintWaypoint(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this);
	APlayerController* PC = GetOwningPlayer();
	const UMarsMissionDirector* Ops = UMarsMissionDirector::Get(this);
	const bool bOrbit = Ops && Ops->IsInOrbit();
	const AActor* Target = bOrbit ? Ops->GetObjectiveTarget() : (Story ? Story->GetObjectiveTarget() : nullptr);
	if (!Story || (!Story->AreWaypointsEnabled() && !bOrbit) || !Target || !PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Size = FVector(Geo.GetLocalSize().X, Geo.GetLocalSize().Y, 0.0f);
	const FVector Goal = Target->GetActorLocation() + FVector(0.0f, 0.0f, 140.0f);
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const float Dist = FVector::Dist(CamLoc, Goal) / 100.0f;
	const bool bFront = FVector::DotProduct((Goal - CamLoc).GetSafeNormal(), CamFwd) > 0.05f;

	FVector2D P;
	const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Goal, P, false);
	const FVector2D Center(Size.X * 0.5f, Size.Y * 0.5f);
	const float Margin = 60.0f;
	bool bEdge = !bFront || !bProjected || P.X < Margin || P.Y < Margin || P.X > Size.X - Margin || P.Y > Size.Y - Margin;
	if (bEdge)
	{
		// Clamp to the screen border along the direction to the target (flip when behind).
		FVector2D D = bProjected ? P - Center : FVector2D(0.0f, 1.0f);
		if (!bFront)
		{
			D = -D;
		}
		if (D.IsNearlyZero())
		{
			D = FVector2D(0.0f, 1.0f);
		}
		D.Normalize();
		const float Sx = (Size.X * 0.5f - Margin) / FMath::Max(FMath::Abs(D.X), 0.001f);
		const float Sy = (Size.Y * 0.5f - Margin) / FMath::Max(FMath::Abs(D.Y), 0.001f);
		P = Center + D * FMath::Min(Sx, Sy);
	}

	const float Pulse = 0.7f + 0.3f * FMath::Sin(Time * 4.0f);
	const FLinearColor Col = MarsUI::Cyan * FLinearColor(1, 1, 1, 0.9f * Pulse);
	// Diamond.
	TArray<FVector2f> Pts = { FVector2f(P + FVector2D(0, -11)), FVector2f(P + FVector2D(11, 0)), FVector2f(P + FVector2D(0, 11)), FVector2f(P + FVector2D(-11, 0)), FVector2f(P + FVector2D(0, -11)) };
	FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, 2.0f);
	MarsUI::Box(Out, Layer, Geo, P - FVector2D(2, 2), FVector2D(4, 4), Col);
	if (bEdge)
	{
		const FVector2D D = (P - Center).GetSafeNormal();
		const FVector2D Tip = P + D * 20.0f;
		const FVector2D N(-D.Y, D.X);
		TArray<FVector2f> Arrow = { FVector2f(Tip - D * 10.0f + N * 7.0f), FVector2f(Tip), FVector2f(Tip - D * 10.0f - N * 7.0f) };
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, Col, true, 2.0f);
	}
	MarsUI::TextCentered(Out, Layer, Geo, Dist >= 1000.0f ? FString::Printf(TEXT("%.1f km"), Dist / 1000.0f) : FString::Printf(TEXT("%.0f m"), Dist), P + FVector2D(0, 14), MarsUI::Mono(8, 120), Col);
}

void UMarsPlayerHUD::PaintSleep(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const
{
	const float Fade = C ? C->GetSleepFade() : 0.0f;
	if (Fade <= 0.0f)
	{
		return;
	}
	const FVector2D Size = Geo.GetLocalSize();
	MarsUI::Box(Out, Layer, Geo, FVector2D::ZeroVector, Size, FLinearColor(0, 0, 0, Fade));
	if (Fade > 0.9f)
	{
		const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
		MarsUI::TextCentered(Out, Layer + 1, Geo, FString::Printf(TEXT("SOL %d"), Env ? Env->GetSolNumber() : 1), FVector2D(Size.X * 0.5f, Size.Y * 0.44f), MarsUI::Mono(30, 600), MarsUI::Amber * FLinearColor(1, 1, 1, Fade));
		const float H = Env ? Env->GetLocalSolarTimeHours() : 7.0f;
		MarsUI::TextCentered(Out, Layer + 1, Geo, FString::Printf(TEXT("%02d:%02d  //  ARES-IV"), FMath::FloorToInt(H), FMath::FloorToInt(FMath::Frac(H) * 60.0f)), FVector2D(Size.X * 0.5f, Size.Y * 0.44f + 52.0f), MarsUI::Mono(10, 300), MarsUI::AmberDim * 1.6f * FLinearColor(1, 1, 1, Fade));
	}
}

void UMarsPlayerHUD::PaintCockpit(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsReconShuttle* S) const
{
	using namespace MarsUI;
	const FVector2D Size = Geo.GetLocalSize();
	const float Yaw = FRotator::NormalizeAxis(S->GetActorRotation().Yaw);

	// ---- Compass strip with objective + beacon bearings ----
	{
		const float W = 560.0f, X0 = Size.X * 0.5f - W * 0.5f, Y0 = 30.0f;
		Box(Out, Layer, Geo, FVector2D(X0, Y0), FVector2D(W, 30.0f), FLinearColor(0, 0, 0, 0.45f));
		Frame(Out, Layer + 1, Geo, FVector2D(X0, Y0), FVector2D(W, 30.0f), Cyan * FLinearColor(1, 1, 1, 0.5f), 1.5f);
		for (int32 Deg = -180; Deg < 540; Deg += 15)
		{
			const float Rel = FRotator::NormalizeAxis(Deg - Yaw);
			if (FMath::Abs(Rel) > 60.0f)
			{
				continue;
			}
			const float X = Size.X * 0.5f + Rel / 60.0f * W * 0.5f;
			const int32 D360 = ((Deg % 360) + 360) % 360;
			const bool bMajor = D360 % 45 == 0;
			Box(Out, Layer + 1, Geo, FVector2D(X - 0.5f, Y0 + (bMajor ? 4.0f : 14.0f)), FVector2D(1.5f, bMajor ? 10.0f : 6.0f), Cyan);
			if (bMajor)
			{
				static const TCHAR* Names[] = { TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW"), TEXT("N"), TEXT("NE") };
				TextCentered(Out, Layer + 1, Geo, Names[D360 / 45], FVector2D(X, Y0 + 15.0f), Mono(7, 120), Cyan);
			}
		}
		auto Mark = [&](const FVector& Where, const FString& Label, const FLinearColor& Col)
		{
			const FVector D = Where - S->GetActorLocation();
			const float Rel = FRotator::NormalizeAxis(D.Rotation().Yaw - Yaw);
			const float X = Size.X * 0.5f + FMath::Clamp(Rel, -60.0f, 60.0f) / 60.0f * W * 0.5f;
			Box(Out, Layer + 2, Geo, FVector2D(X - 4.0f, Y0 - 8.0f), FVector2D(8.0f, 8.0f), Col);
			TextCentered(Out, Layer + 2, Geo, FString::Printf(TEXT("%s %.1fkm"), *Label, D.Size2D() / 100000.0f), FVector2D(X, Y0 + 34.0f), Mono(7, 100), Col);
		};
		for (TActorIterator<ASurfaceBeacon> It(GetWorld()); It; ++It)
		{
			if (It->IsActive() && FVector::Dist2D(It->GetActorLocation(), S->GetActorLocation()) < It->RadioRangeCm)
			{
				Mark(It->GetActorLocation(), It->BeaconLabel, AmberDim * 1.6f);
			}
		}
		if (const UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this))
		{
			if (const AActor* T = Story->GetObjectiveTarget())
			{
				Mark(T->GetActorLocation(), TEXT("OBJ"), Cyan);
			}
		}
		Box(Out, Layer + 2, Geo, FVector2D(Size.X * 0.5f - 1.0f, Y0 - 2.0f), FVector2D(2.0f, 34.0f), Amber);
	}

	// ---- Instrument cluster ----
	const FVector2D PanelSize(700.0f, 132.0f);
	const FVector2D P0(Size.X * 0.5f - PanelSize.X * 0.5f, Size.Y - PanelSize.Y - 34.0f);
	Box(Out, Layer, Geo, P0, PanelSize, FLinearColor(0.01f, 0.015f, 0.02f, 0.78f));
	Frame(Out, Layer + 1, Geo, P0, PanelSize, Cyan * FLinearColor(1, 1, 1, 0.6f), 2.0f);
	Text(Out, Layer + 1, Geo, TEXT("RS-2 RECON  //  VTOL"), P0 + FVector2D(14, 8), Mono(8, 200), Cyan * 0.8f);

	// Altimeter + vertical speed.
	const float Alt = S->GetAltitudeMeters();
	const float Vs = S->GetVerticalSpeedMs();
	Text(Out, Layer + 1, Geo, TEXT("ALT"), P0 + FVector2D(18, 30), Mono(8, 200), AmberDim * 1.6f);
	Text(Out, Layer + 1, Geo, FString::Printf(TEXT("%5.0f m"), Alt), P0 + FVector2D(18, 44), Mono(20, 60), Alt < 15.0f && !S->IsLanded() ? Amber : Phosphor);
	Text(Out, Layer + 1, Geo, FString::Printf(TEXT("V/S %+5.1f m/s %s"), Vs, Vs > 0.3f ? TEXT("^") : Vs < -0.3f ? TEXT("v") : TEXT("-")), P0 + FVector2D(18, 78),
		Mono(9, 100), Vs < -6.0f ? Alarm : Phosphor);
	// Speed + throttle.
	Text(Out, Layer + 1, Geo, TEXT("SPEED"), P0 + FVector2D(210, 30), Mono(8, 200), AmberDim * 1.6f);
	Text(Out, Layer + 1, Geo, FString::Printf(TEXT("%4.0f m/s"), S->GetSpeedMs()), P0 + FVector2D(210, 44), Mono(20, 60), Phosphor);
	Text(Out, Layer + 1, Geo, TEXT("THRUST"), P0 + FVector2D(210, 82), Mono(7, 160), AmberDim * 1.6f);
	for (int32 i = 0; i < 16; ++i)
	{
		const bool bLit = i < FMath::RoundToInt(S->GetThrottle01() * 16.0f);
		Box(Out, Layer + 1, Geo, P0 + FVector2D(270.0f + i * 8.0f, 82.0f), FVector2D(6.0f, 10.0f), bLit ? (i > 12 ? Amber : Cyan) : Cyan * FLinearColor(1, 1, 1, 0.12f));
	}
	// Power / O2 / gear.
	auto Bar = [&](const FString& L, float V, float Y)
	{
		const int32 Status = V < 0.15f ? 2 : V < 0.3f ? 1 : 0;
		Text(Out, Layer + 1, Geo, L, P0 + FVector2D(430, Y), Mono(8, 160), AmberDim * 1.6f);
		for (int32 i = 0; i < 18; ++i)
		{
			Box(Out, Layer + 1, Geo, P0 + FVector2D(510.0f + i * 9.0f, Y + 1.0f), FVector2D(7.0f, 9.0f), i < FMath::CeilToInt(V * 18.0f) ? StatusColor(Status, Time) : Phosphor * FLinearColor(1, 1, 1, 0.1f));
		}
	};
	Bar(TEXT("BATTERY"), S->GetBattery01(), 30.0f);
	Bar(TEXT("O2 RES"), S->GetO2Reserve01(), 48.0f);
	if (const AMarsCharacter* Pilot = S->GetPilot())
	{
		if (const UEVASuitComponent* Suit = Pilot->GetSuit())
		{
			Bar(TEXT("SUIT O2"), Suit->GetO2Fraction01(), 66.0f);
		}
	}
	Text(Out, Layer + 1, Geo, S->IsGearDown() ? TEXT("GEAR  DOWN") : TEXT("GEAR  UP"), P0 + FVector2D(430, 88), Mono(10, 160), S->IsGearDown() ? Phosphor : Amber);
	Text(Out, Layer + 1, Geo, S->IsDocked() ? TEXT("DOCKED / CHARGING") : S->IsLanded() ? TEXT("LANDED") : TEXT("AIRBORNE"), P0 + FVector2D(560, 88), Mono(8, 160), Cyan);
	Text(Out, Layer + 1, Geo, TEXT("W/S THRUST  A/D TURN  MOUSE FREE-LOOK 360  SPACE/C UP/DOWN  SHIFT BOOST  L GEAR  E EXIT"), P0 + FVector2D(14, PanelSize.Y - 18.0f), Mono(7, 100), AmberDim * 1.5f);

	// ---- Radiation / storm radar ----
	{
		const float R = 78.0f;
		const FVector2D C(Size.X - R - 40.0f, Size.Y - R - 50.0f);
		const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
		const float Storm = Env ? Env->GetStormIntensity() : 0.0f;
		const AMarsCharacter* Pilot = S->GetPilot();
		const float Dose = Pilot && Pilot->GetBiometrics() ? Pilot->GetBiometrics()->CurrentDoseRateMsvPerHour : 0.0f;
		for (int32 Ring = 1; Ring <= 3; ++Ring)
		{
			TArray<FVector2f> Circle;
			for (int32 i = 0; i <= 48; ++i)
			{
				const float A = i / 48.0f * 2.0f * PI;
				Circle.Add(FVector2f(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R * Ring / 3.0f));
			}
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Circle, ESlateDrawEffect::None, Cyan * FLinearColor(1, 1, 1, 0.35f), true, 1.0f);
		}
		// Storm haze ring (thickens with storm intensity).
		if (Storm > 0.05f)
		{
			TArray<FVector2f> Haze;
			for (int32 i = 0; i <= 48; ++i)
			{
				const float A = i / 48.0f * 2.0f * PI;
				Haze.Add(FVector2f(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R * (1.0f - 0.3f * Storm)));
			}
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Haze, ESlateDrawEffect::None, FLinearColor(1.0f, 0.45f, 0.1f, 0.6f), true, 4.0f + 14.0f * Storm);
		}
		const float Sweep = Time * 2.0f;
		TArray<FVector2f> Line = { FVector2f(C), FVector2f(C + FVector2D(FMath::Cos(Sweep), FMath::Sin(Sweep)) * R) };
		FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), Line, ESlateDrawEffect::None, Phosphor, true, 1.5f);
		// Heading-up blips: beacons and the objective within 2 km.
		auto Blip = [&](const FVector& Where, const FLinearColor& Col)
		{
			const FVector D = Where - S->GetActorLocation();
			const float Rel = FMath::DegreesToRadians(FRotator::NormalizeAxis(D.Rotation().Yaw - Yaw));
			const float Rad = FMath::Min(D.Size2D() / 200000.0f, 1.0f) * R;
			const FVector2D Pt = C + FVector2D(FMath::Sin(Rel), -FMath::Cos(Rel)) * Rad;
			Box(Out, Layer + 2, Geo, Pt - FVector2D(3, 3), FVector2D(6, 6), Col);
		};
		for (TActorIterator<ASurfaceBeacon> It(GetWorld()); It; ++It)
		{
			Blip(It->GetActorLocation(), AmberDim * 1.8f);
		}
		if (const UMarsNarrativeDirector* Story = UMarsNarrativeDirector::Get(this))
		{
			if (const AActor* T = Story->GetObjectiveTarget())
			{
				Blip(T->GetActorLocation(), Cyan);
			}
		}
		Box(Out, Layer + 2, Geo, C - FVector2D(2, 2), FVector2D(4, 4), FLinearColor::White);
		TextCentered(Out, Layer + 2, Geo, TEXT("RAD / STORM RADAR  2 km"), C + FVector2D(0, R + 6.0f), Mono(7, 140), Cyan);
		TextCentered(Out, Layer + 2, Geo, FString::Printf(TEXT("%.2f mSv/h   STORM %.0f%%"), Dose, Storm * 100.0f), C - FVector2D(0, R + 20.0f), Mono(8, 120),
			Dose > 0.5f || Storm > 0.5f ? StatusColor(2, Time) : Dose > 0.1f || Storm > 0.2f ? Amber : Phosphor);
	}
}

void UMarsPlayerHUD::PaintClock(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const UMarsEnvironmentSubsystem* Env = GetWorld() ? GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>() : nullptr;
	if (!Env)
	{
		return;
	}
	const FVector2D Size = Geo.GetLocalSize();
	const float H = Env->GetLocalSolarTimeHours();
	const bool bNight = Env->IsNight();
	const FString Clock = FString::Printf(TEXT("SOL %d   %02d:%02d   %s   %.0f C   %s"), Env->GetSolNumber(), FMath::FloorToInt(H), FMath::FloorToInt(FMath::Frac(H) * 60.0f),
		bNight ? TEXT("NIGHT") : TEXT("DAY"), Env->GetAmbientTemperatureC(), *Env->GetWeatherName());

	// Solar storm banner + electronics failure (unshielded): visor static and tearing.
	if (const UMarsSolarStormManager* Storm = UMarsSolarStormManager::Get(this))
	{
		if (Storm->GetPhase() == ESolarStormPhase::Warning)
		{
			const FLinearColor C = FMath::Frac(Time * 2.0f) < 0.6f ? MarsUI::Alarm : MarsUI::Amber;
			MarsUI::TextCentered(Out, Layer + 3, Geo, FString::Printf(TEXT("!! SOLAR STORM IN %.0f s  -  EM SHIELD (ENGINEERING, HOLD E) OR MED-BAY SHELTER !!"), Storm->GetPhaseSecondsRemaining()),
				FVector2D(Size.X * 0.5f, 132.0f), MarsUI::Mono(11, 160), C);
		}
		else if (Storm->IsActive())
		{
			MarsUI::TextCentered(Out, Layer + 3, Geo, FString::Printf(TEXT("SOLAR STORM  %.0f s"), Storm->GetPhaseSecondsRemaining()), FVector2D(Size.X * 0.5f, 132.0f), MarsUI::Mono(10, 200), MarsUI::Alarm);
			if (Storm->AreElectronicsFailing())
			{
				for (int32 i = 0; i < 6; ++i)
				{
					const float Y = FMath::Frac(Time * (0.7f + i * 0.31f) + i * 0.17f) * Size.Y;
					MarsUI::Box(Out, Layer + 2, Geo, FVector2D(0.0f, Y), FVector2D(Size.X, 2.0f + (i % 3) * 3.0f), FLinearColor(1, 1, 1, 0.05f + 0.05f * (i % 2)));
				}
				if (FMath::Frac(Time * 0.53f) < 0.06f)
				{
					MarsUI::Box(Out, Layer + 2, Geo, FVector2D::ZeroVector, Size, FLinearColor(0.8f, 0.85f, 1.0f, 0.12f));
				}
			}
		}
	}
	MarsUI::TextCentered(Out, Layer, Geo, Clock, FVector2D(Size.X * 0.5f, 8.0f), MarsUI::Mono(9, 200), bNight ? MarsUI::Cyan * 0.9f : MarsUI::Amber * 0.9f);
}

void UMarsPlayerHUD::PaintOrbitOps(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsReconShuttle* S) const
{
	using namespace MarsUI;
	const UMarsMissionDirector* Ops = UMarsMissionDirector::Get(this);
	if (!Ops || !S)
	{
		return;
	}
	const FVector2D Size = Geo.GetLocalSize();
	const FVector2D P(Size.X * 0.5f - 350.0f, Size.Y - 236.0f);

	// Mission line + objective progress.
	FString Line = Ops->GetStatusLine();
	Text(Out, Layer, Geo, FString::Printf(TEXT("ORBITAL OPS // %s"), *Line), P, Mono(9, 80), Ops->IsInOrbit() ? Cyan : AmberDim * 1.6f);
	if (Ops->IsInOrbit() && !Ops->IsActiveMissionComplete())
	{
		const float Prog = Ops->GetObjectiveProgress01();
		if (Prog > 0.0f)
		{
			Box(Out, Layer, Geo, P + FVector2D(0, 18), FVector2D(700.0f, 4.0f), Cyan * FLinearColor(1, 1, 1, 0.15f));
			Box(Out, Layer, Geo, P + FVector2D(0, 18), FVector2D(700.0f * Prog, 4.0f), Cyan);
		}
	}

	// Burn prompts.
	if (!Ops->IsInOrbit() && S->GetAltitudeMeters() > AMarsReconShuttle::OrbitBurnAltitudeM)
	{
		TextCentered(Out, Layer, Geo, Ops->GetAvailableMission() != INDEX_NONE ? TEXT("ORBIT WINDOW OPEN - HOLD SPACE TO BURN FOR ORBIT") : TEXT("NO ORBITAL TASKING TODAY"),
			FVector2D(Size.X * 0.5f, Size.Y * 0.3f), Mono(12, 200), Ops->GetAvailableMission() != INDEX_NONE ? Cyan : AmberDim * 1.6f);
	}
	else if (Ops->IsInOrbit())
	{
		TextCentered(Out, Layer, Geo, TEXT("ORBIT 120 KM  //  HOLD C TO DE-ORBIT"), FVector2D(Size.X * 0.5f, 70.0f), Mono(9, 200), Cyan * 0.8f);
	}
	if (S->GetBurnHold01() > 0.0f)
	{
		Box(Out, Layer, Geo, FVector2D(Size.X * 0.5f - 150.0f, Size.Y * 0.3f + 26.0f), FVector2D(300.0f * S->GetBurnHold01(), 6.0f), Amber);
	}

	// Malfunction.
	if (Ops->GetMalfunction() != EMarsMalfunction::None)
	{
		const FLinearColor C = FMath::Frac(Time * 2.0f) < 0.6f ? Alarm : Amber;
		TextCentered(Out, Layer, Geo, FString::Printf(TEXT("!! %s  -  HOLD R: MANUAL OVERRIDE !!"), *Ops->GetMalfunctionText()), FVector2D(Size.X * 0.5f, Size.Y * 0.36f), Mono(12, 180), C);
		Box(Out, Layer, Geo, FVector2D(Size.X * 0.5f - 150.0f, Size.Y * 0.36f + 26.0f), FVector2D(300.0f, 6.0f), Alarm * FLinearColor(1, 1, 1, 0.2f));
		Box(Out, Layer, Geo, FVector2D(Size.X * 0.5f - 150.0f, Size.Y * 0.36f + 26.0f), FVector2D(300.0f * Ops->GetRepairProgress01(), 6.0f), Phosphor);
	}
}
