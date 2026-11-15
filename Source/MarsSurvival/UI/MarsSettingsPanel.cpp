#include "UI/MarsSettingsPanel.h"

#include "Core/MarsUserSettings.h"
#include "UI/MarsUIStyle.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/PlatformTime.h"
#include "Layout/Geometry.h"

namespace
{
	const TCHAR* TabNames[] = { TEXT("CONTROLS"), TEXT("CAMERA"), TEXT("AUDIO"), TEXT("DISPLAY"), TEXT("GAMEPLAY") };
	const FIntPoint Resolutions[] = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
	const float FrameLimits[] = { 30.0f, 60.0f, 120.0f, 144.0f, 0.0f };

	FString OnOff(bool b) { return b ? TEXT("ON") : TEXT("OFF"); }
	FString Pct(float V) { return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(V * 100.0f)); }

	int32 ResolutionIndex(const FIntPoint& R)
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(Resolutions); ++i)
		{
			if (Resolutions[i] == R) { return i; }
		}
		return 1;
	}

	int32 FrameLimitIndex(float L)
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(FrameLimits); ++i)
		{
			if (FMath::IsNearlyEqual(FrameLimits[i], L)) { return i; }
		}
		return 4;
	}

	FString WindowModeName(EWindowMode::Type M)
	{
		switch (M)
		{
		case EWindowMode::Fullscreen:         return TEXT("FULLSCREEN");
		case EWindowMode::WindowedFullscreen: return TEXT("BORDERLESS");
		default:                              return TEXT("WINDOWED");
		}
	}

	FString KeyName(const FKey& Key)
	{
		return Key.IsValid() ? Key.GetDisplayName(false).ToString().ToUpper() : TEXT("---");
	}
}

void FMarsSettingsPanel::Open()
{
	bOpen = true;
	bWaitingForKey = false;
	Row = 0;
	Scroll = 0;
}

void FMarsSettingsPanel::Commit(UWorld* World) const
{
	FMarsUserSettings& S = FMarsUserSettings::Get();
	S.Save();
	S.Apply(World);
}

TArray<FMarsSettingsPanel::FRow> FMarsSettingsPanel::BuildRows() const
{
	const FMarsUserSettings& S = FMarsUserSettings::Get();
	TArray<FRow> Rows;
	auto Add = [&Rows](const FString& L, const FString& V, float Slider = -1.0f, bool bAction = false)
	{
		FRow R; R.Label = L; R.Value = V; R.Slider = Slider; R.bAction = bAction; Rows.Add(R);
	};

	switch (Tab)
	{
	case ETab::Controls:
		for (const FMarsBinding& B : FMarsUserSettings::GetBindings())
		{
			Add(B.Label, KeyName(FMarsUserSettings::GetBoundKey(B)));
		}
		Add(TEXT("RESTORE DEFAULT KEYS"), TEXT(""), -1.0f, true);
		break;

	case ETab::Camera:
		Add(TEXT("Mouse sensitivity"), FString::Printf(TEXT("%.1fx"), S.MouseSensitivity), (S.MouseSensitivity - 0.2f) / 2.8f);
		Add(TEXT("Invert mouse Y"), OnOff(S.bInvertY));
		Add(TEXT("Smooth acceleration"), OnOff(S.bMouseSmoothing));
		Add(TEXT("Head-bob"), S.HeadBob <= 0.0f ? TEXT("OFF") : Pct(S.HeadBob), S.HeadBob);
		Add(TEXT("Weapon sway / breathing"), Pct(S.WeaponSway), S.WeaponSway);
		Add(TEXT("Field of view"), FString::Printf(TEXT("%d"), FMath::RoundToInt(S.FieldOfView)), (S.FieldOfView - 65.0f) / 45.0f);
		break;

	case ETab::Audio:
		Add(TEXT("Master volume"), Pct(S.MasterVolume), S.MasterVolume);
		Add(TEXT("Effects (SFX)"), Pct(S.SfxVolume), S.SfxVolume);
		Add(TEXT("Ambience"), Pct(S.AmbientVolume), S.AmbientVolume);
		Add(TEXT("A.R.I.A. voice"), Pct(S.VoiceVolume), S.VoiceVolume);
		Add(TEXT("Music"), Pct(S.MusicVolume), S.MusicVolume);
		Add(TEXT("Subtitles (A.R.I.A. text)"), OnOff(S.bSubtitles));
		break;

	case ETab::Display:
	{
		const UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		if (G)
		{
			const FIntPoint Res = G->GetScreenResolution();
			const float Limit = G->GetFrameRateLimit();
			Add(TEXT("Window mode"), WindowModeName(G->GetFullscreenMode()));
			Add(TEXT("Resolution"), FString::Printf(TEXT("%d x %d"), Res.X, Res.Y));
			Add(TEXT("V-Sync"), OnOff(G->IsVSyncEnabled()));
			Add(TEXT("Frame limit"), Limit <= 0.0f ? TEXT("UNLIMITED") : FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Limit)));
			Add(TEXT("Render quality"), FString::Printf(TEXT("%d / 3"), FMath::Clamp(G->GetOverallScalabilityLevel(), 0, 3)), FMath::Clamp(G->GetOverallScalabilityLevel(), 0, 3) / 3.0f);
		}
		break;
	}

	case ETab::Gameplay:
		Add(TEXT("Inspect new items on pickup"), OnOff(S.bInspectNewItems));
		Add(TEXT("RESET ALL SETTINGS"), TEXT(""), -1.0f, true);
		break;

	default:
		break;
	}
	return Rows;
}

void FMarsSettingsPanel::ChangeTab(int32 Direction)
{
	const int32 N = static_cast<int32>(ETab::Count);
	Tab = static_cast<ETab>((static_cast<int32>(Tab) + Direction + N) % N);
	Row = 0;
	Scroll = 0;
	bWaitingForKey = false;
}

void FMarsSettingsPanel::ClampScroll(int32 NumRows)
{
	Row = FMath::Clamp(Row, 0, FMath::Max(NumRows - 1, 0));
	if (Row < Scroll) { Scroll = Row; }
	if (Row >= Scroll + VisibleRows) { Scroll = Row - VisibleRows + 1; }
	Scroll = FMath::Clamp(Scroll, 0, FMath::Max(NumRows - VisibleRows, 0));
}

void FMarsSettingsPanel::Adjust(int32 RowIndex, int32 Dir, UWorld* World)
{
	FMarsUserSettings& S = FMarsUserSettings::Get();
	auto Step = [Dir](float& V, float StepSize, float Min, float Max) { V = FMath::Clamp(FMath::RoundToFloat((V + Dir * StepSize) / StepSize) * StepSize, Min, Max); };

	switch (Tab)
	{
	case ETab::Camera:
		switch (RowIndex)
		{
		case 0: Step(S.MouseSensitivity, 0.1f, 0.2f, 3.0f); break;
		case 1: S.bInvertY = !S.bInvertY; break;
		case 2: S.bMouseSmoothing = !S.bMouseSmoothing; break;
		case 3: Step(S.HeadBob, 0.1f, 0.0f, 1.0f); break;
		case 4: Step(S.WeaponSway, 0.1f, 0.0f, 1.0f); break;
		case 5: Step(S.FieldOfView, 5.0f, 65.0f, 110.0f); break;
		}
		Commit(World);
		break;

	case ETab::Audio:
		switch (RowIndex)
		{
		case 0: Step(S.MasterVolume, 0.05f, 0.0f, 1.0f); break;
		case 1: Step(S.SfxVolume, 0.05f, 0.0f, 1.0f); break;
		case 2: Step(S.AmbientVolume, 0.05f, 0.0f, 1.0f); break;
		case 3: Step(S.VoiceVolume, 0.05f, 0.0f, 1.0f); break;
		case 4: Step(S.MusicVolume, 0.05f, 0.0f, 1.0f); break;
		case 5: S.bSubtitles = !S.bSubtitles; break;
		}
		Commit(World);
		break;

	case ETab::Display:
		if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			switch (RowIndex)
			{
			case 0:
			{
				static const EWindowMode::Type Modes[] = { EWindowMode::Windowed, EWindowMode::WindowedFullscreen, EWindowMode::Fullscreen };
				int32 Cur = 0;
				for (int32 i = 0; i < 3; ++i) { if (Modes[i] == G->GetFullscreenMode()) { Cur = i; } }
				G->SetFullscreenMode(Modes[(Cur + Dir + 3) % 3]);
				break;
			}
			case 1:
			{
				const int32 N = UE_ARRAY_COUNT(Resolutions);
				G->SetScreenResolution(Resolutions[(ResolutionIndex(G->GetScreenResolution()) + Dir + N) % N]);
				break;
			}
			case 2: G->SetVSyncEnabled(!G->IsVSyncEnabled()); break;
			case 3:
			{
				const int32 N = UE_ARRAY_COUNT(FrameLimits);
				G->SetFrameRateLimit(FrameLimits[(FrameLimitIndex(G->GetFrameRateLimit()) + Dir + N) % N]);
				break;
			}
			case 4: G->SetOverallScalabilityLevel(FMath::Clamp(G->GetOverallScalabilityLevel() + Dir, 0, 3)); break;
			}
			G->ApplySettings(false);
		}
		break;

	case ETab::Gameplay:
		if (RowIndex == 0)
		{
			S.bInspectNewItems = !S.bInspectNewItems;
			Commit(World);
		}
		break;

	default:
		break;
	}
}

void FMarsSettingsPanel::Activate(int32 RowIndex, UWorld* World)
{
	const int32 NumRows = BuildRows().Num();
	if (Tab == ETab::Controls)
	{
		if (RowIndex == NumRows - 1)
		{
			FMarsUserSettings::ResetBindingsToDefaults(World);
			Flash = TEXT("DEFAULT KEYS RESTORED");
			FlashUntil = FPlatformTime::Seconds() + 2.0;
		}
		else
		{
			bWaitingForKey = true;
		}
		return;
	}
	if (Tab == ETab::Gameplay && RowIndex == 1)
	{
		FMarsUserSettings& S = FMarsUserSettings::Get();
		S = FMarsUserSettings();
		S.Save();
		S.Load();
		S.Apply(World);
		FMarsUserSettings::ResetBindingsToDefaults(World);
		Flash = TEXT("ALL SETTINGS RESET");
		FlashUntil = FPlatformTime::Seconds() + 2.0;
		return;
	}
	Adjust(RowIndex, 1, World);
}

bool FMarsSettingsPanel::OnKey(const FKey& Key, UWorld* World)
{
	if (!bOpen)
	{
		return false;
	}
	if (bWaitingForKey)
	{
		if (Key == EKeys::Escape)
		{
			bWaitingForKey = false;
			return true;
		}
		const TArray<FMarsBinding>& Bindings = FMarsUserSettings::GetBindings();
		if (Bindings.IsValidIndex(Row))
		{
			// A key can only drive one control: swap it away from whatever used it before.
			const FMarsBinding& Target = Bindings[Row];
			const FKey Previous = FMarsUserSettings::GetBoundKey(Target);
			for (const FMarsBinding& Other : Bindings)
			{
				if (&Other != &Target && FMarsUserSettings::GetBoundKey(Other) == Key && Previous.IsValid())
				{
					FMarsUserSettings::Rebind(World, Other, Previous);
				}
			}
			FMarsUserSettings::Rebind(World, Target, Key);
			Flash = FString::Printf(TEXT("%s  ->  %s"), *Target.Label.ToUpper(), *KeyName(Key));
			FlashUntil = FPlatformTime::Seconds() + 2.0;
		}
		bWaitingForKey = false;
		return true;
	}

	const int32 NumRows = BuildRows().Num();
	if (Key == EKeys::Escape || Key == EKeys::BackSpace)
	{
		Close();
		return true;
	}
	if (Key == EKeys::Up || Key == EKeys::W) { Row = (Row + NumRows - 1) % FMath::Max(NumRows, 1); ClampScroll(NumRows); return true; }
	if (Key == EKeys::Down || Key == EKeys::S) { Row = (Row + 1) % FMath::Max(NumRows, 1); ClampScroll(NumRows); return true; }
	if (Key == EKeys::Tab || Key == EKeys::E || Key == EKeys::PageDown) { ChangeTab(1); return true; }
	if (Key == EKeys::Q || Key == EKeys::PageUp) { ChangeTab(-1); return true; }
	if (Key == EKeys::Left || Key == EKeys::A) { if (Tab != ETab::Controls) { Adjust(Row, -1, World); } return true; }
	if (Key == EKeys::Right || Key == EKeys::D) { if (Tab != ETab::Controls) { Adjust(Row, 1, World); } return true; }
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar) { Activate(Row, World); return true; }
	return true; // modal: swallow everything else
}

bool FMarsSettingsPanel::OnMouseDown(const FVector2D& Local, const FKey& Button, UWorld* World)
{
	if (!bOpen)
	{
		return false;
	}
	if (bWaitingForKey)
	{
		return OnKey(Button, World); // mouse buttons can be bound too
	}
	if (BackRect.bIsValid && BackRect.IsInside(Local))
	{
		Close();
		return true;
	}
	for (int32 i = 0; i < TabRects.Num(); ++i)
	{
		if (TabRects[i].IsInside(Local))
		{
			ChangeTab(i - static_cast<int32>(Tab));
			return true;
		}
	}
	for (int32 i = 0; i < RowRects.Num(); ++i)
	{
		if (RowRects[i].IsInside(Local))
		{
			Row = RowIndices[i];
			const TArray<FRow> Rows = BuildRows();
			const bool bLeftHalf = Local.X < RowRects[i].GetCenter().X + RowRects[i].GetExtent().X * 0.45f;
			if (Rows.IsValidIndex(Row) && Rows[Row].Slider >= 0.0f)
			{
				Adjust(Row, bLeftHalf ? -1 : 1, World);
			}
			else if (Button == EKeys::RightMouseButton && Tab != ETab::Controls)
			{
				Adjust(Row, -1, World);
			}
			else
			{
				Activate(Row, World);
			}
			return true;
		}
	}
	return true;
}

void FMarsSettingsPanel::OnMouseMove(const FVector2D& Local)
{
	if (!bOpen || bWaitingForKey)
	{
		return;
	}
	for (int32 i = 0; i < RowRects.Num(); ++i)
	{
		if (RowRects[i].IsInside(Local))
		{
			Row = RowIndices[i];
			return;
		}
	}
}

void FMarsSettingsPanel::OnMouseWheel(float Delta)
{
	if (!bOpen)
	{
		return;
	}
	const int32 NumRows = BuildRows().Num();
	Scroll = FMath::Clamp(Scroll - FMath::RoundToInt(Delta), 0, FMath::Max(NumRows - VisibleRows, 0));
	Row = FMath::Clamp(Row, Scroll, FMath::Min(Scroll + VisibleRows - 1, NumRows - 1));
}

void FMarsSettingsPanel::Paint(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size, float Time) const
{
	using namespace MarsUI;
	RowRects.Reset();
	RowIndices.Reset();
	TabRects.Reset();

	Box(Out, Layer, Geo, Pos, Size, FLinearColor(0.01f, 0.012f, 0.015f, 0.94f));
	Frame(Out, Layer + 1, Geo, Pos, Size, AmberDim * 1.4f, 2.0f);
	Frame(Out, Layer + 1, Geo, Pos + FVector2D(6, 6), Size - FVector2D(12, 12), AmberDim * 0.7f);

	Text(Out, Layer + 2, Geo, TEXT("SYSTEM CONFIGURATION"), Pos + FVector2D(28, 22), Mono(18, 300), Amber);
	Text(Out, Layer + 2, Geo, TEXT("SV-4 SUIT INTERFACE  //  OPERATOR PREFERENCES"), Pos + FVector2D(28, 52), Mono(9, 200), AmberDim * 1.5f);

	// Tabs
	float TX = Pos.X + 28.0f;
	const float TY = Pos.Y + 82.0f;
	for (int32 i = 0; i < static_cast<int32>(ETab::Count); ++i)
	{
		const bool bSel = i == static_cast<int32>(Tab);
		const FSlateFontInfo F = Mono(11, 160);
		const FVector2D TS = Measure(TabNames[i], F) + FVector2D(24, 12);
		if (bSel)
		{
			Box(Out, Layer + 1, Geo, FVector2D(TX, TY), TS, Amber * FLinearColor(1, 1, 1, 0.9f));
		}
		else
		{
			Frame(Out, Layer + 1, Geo, FVector2D(TX, TY), TS, AmberDim * 1.3f, 2.0f);
		}
		Text(Out, Layer + 2, Geo, TabNames[i], FVector2D(TX + 12, TY + 6), F, bSel ? FLinearColor::Black : AmberDim * 1.6f, false);
		TabRects.Add(FBox2D(FVector2D(TX, TY), FVector2D(TX, TY) + TS));
		TX += TS.X + 8.0f;
	}

	// Rows
	const TArray<FRow> Rows = BuildRows();
	const float RowH = 30.0f;
	const float ListTop = TY + 48.0f;
	const float ListBottom = Pos.Y + Size.Y - 64.0f;
	VisibleRows = FMath::Max(FMath::FloorToInt((ListBottom - ListTop) / RowH), 3);
	const int32 First = FMath::Clamp(Scroll, 0, FMath::Max(Rows.Num() - VisibleRows, 0));
	const float RX = Pos.X + 28.0f;
	const float RW = Size.X - 56.0f;
	const FSlateFontInfo RowFont = Mono(12, 80);

	for (int32 i = First; i < Rows.Num() && i < First + VisibleRows; ++i)
	{
		const FRow& R = Rows[i];
		const float Y = ListTop + (i - First) * RowH;
		const bool bSel = i == Row;
		const FBox2D Rect(FVector2D(RX, Y), FVector2D(RX + RW, Y + RowH - 4.0f));
		RowRects.Add(Rect);
		RowIndices.Add(i);
		if (bSel)
		{
			Box(Out, Layer + 1, Geo, Rect.Min, Rect.GetSize(), FLinearColor(Amber.R, Amber.G, Amber.B, 0.10f + 0.04f * FMath::Sin(Time * 5.0f)));
			Box(Out, Layer + 2, Geo, Rect.Min, FVector2D(3.0f, Rect.GetSize().Y), Amber);
		}
		const FLinearColor LabelCol = R.bAction ? Cyan : (bSel ? Amber : AmberDim * 1.6f);
		Text(Out, Layer + 2, Geo, R.bAction ? FString::Printf(TEXT("[ %s ]"), *R.Label) : R.Label, FVector2D(RX + 14.0f, Y + 6.0f), RowFont, LabelCol);

		FString Value = R.Value;
		if (bSel && bWaitingForKey)
		{
			Value = FMath::Frac(Time * 2.0f) < 0.5f ? TEXT("PRESS A KEY  (ESC CANCEL)") : TEXT("");
		}
		if (R.Slider >= 0.0f)
		{
			// Segmented slider bar + value.
			const int32 Segs = 20;
			const float BarX = RX + RW * 0.52f;
			const float BarW = RW * 0.30f;
			const int32 Lit = FMath::RoundToInt(FMath::Clamp(R.Slider, 0.0f, 1.0f) * Segs);
			for (int32 s = 0; s < Segs; ++s)
			{
				Box(Out, Layer + 2, Geo, FVector2D(BarX + s * BarW / Segs, Y + 7.0f), FVector2D(BarW / Segs - 2.0f, 12.0f),
					s < Lit ? (bSel ? Amber : AmberDim * 1.5f) : AmberDim * FLinearColor(1, 1, 1, 0.25f));
			}
			Text(Out, Layer + 2, Geo, Value, FVector2D(BarX + BarW + 16.0f, Y + 6.0f), RowFont, bSel ? Phosphor : Phosphor * 0.75f);
			if (bSel)
			{
				Text(Out, Layer + 2, Geo, TEXT("<"), FVector2D(BarX - 18.0f, Y + 6.0f), RowFont, Amber);
			}
		}
		else if (!Value.IsEmpty())
		{
			const FSlateFontInfo VF = Tab == ETab::Controls ? Mono(12, 120) : RowFont;
			const FVector2D VS = Measure(Value, VF);
			const float VX = RX + RW - VS.X - 18.0f;
			if (Tab == ETab::Controls && !(bSel && bWaitingForKey))
			{
				// Keycap: filled plate + darker bottom lip (1-px outlines vanish under DPI scaling).
				Box(Out, Layer + 2, Geo, FVector2D(VX - 10.0f, Y + 1.0f), FVector2D(VS.X + 20.0f, RowH - 5.0f), (bSel ? Amber : AmberDim) * FLinearColor(1, 1, 1, bSel ? 0.35f : 0.22f));
				Box(Out, Layer + 2, Geo, FVector2D(VX - 10.0f, Y + RowH - 5.0f), FVector2D(VS.X + 20.0f, 2.0f), (bSel ? Amber : AmberDim) * FLinearColor(1, 1, 1, 0.8f));
			}
			Text(Out, Layer + 3, Geo, Value, FVector2D(VX, Y + 6.0f), VF, bSel ? Phosphor : Phosphor * 0.8f);
		}
	}

	// Scroll indicator
	if (Rows.Num() > VisibleRows)
	{
		const float TrackH = VisibleRows * RowH;
		const float ThumbH = TrackH * VisibleRows / Rows.Num();
		const float ThumbY = ListTop + (TrackH - ThumbH) * First / FMath::Max(Rows.Num() - VisibleRows, 1);
		Box(Out, Layer + 1, Geo, FVector2D(Pos.X + Size.X - 18.0f, ListTop), FVector2D(3.0f, TrackH), AmberDim * FLinearColor(1, 1, 1, 0.4f));
		Box(Out, Layer + 2, Geo, FVector2D(Pos.X + Size.X - 19.0f, ThumbY), FVector2D(5.0f, ThumbH), Amber);
	}

	// Footer
	const float FY = Pos.Y + Size.Y - 44.0f;
	if (!Flash.IsEmpty() && FPlatformTime::Seconds() < FlashUntil)
	{
		Text(Out, Layer + 2, Geo, Flash, FVector2D(RX, FY - 22.0f), Mono(10, 160), Phosphor);
	}
	const FString Hint = Tab == ETab::Controls
		? TEXT("W/S SELECT   ENTER REBIND   Q/E TAB   ESC BACK")
		: TEXT("W/S SELECT   A/D ADJUST   ENTER TOGGLE   Q/E TAB   ESC BACK");
	Text(Out, Layer + 2, Geo, Hint, FVector2D(RX, FY), Mono(9, 160), AmberDim * 1.4f);

	const FString Back = TEXT("[ BACK ]");
	const FSlateFontInfo BF = Mono(11, 200);
	const FVector2D BS = Measure(Back, BF);
	const FVector2D BP(Pos.X + Size.X - BS.X - 34.0f, FY - 4.0f);
	Text(Out, Layer + 2, Geo, Back, BP, BF, Amber);
	BackRect = FBox2D(BP - FVector2D(6, 4), BP + BS + FVector2D(6, 4));
}
