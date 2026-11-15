#include "UI/SplashIntroWidget.h"

#include "Audio/MarsAudioManager.h"
#include "Core/MarsSaveGame.h"
#include "Player/MarsCharacter.h"
#include "UI/MarsUIStyle.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Math/RandomStream.h"
#include "Rendering/DrawElements.h"
#include "RenderUtils.h"

namespace
{
	constexpr float BootSeconds = 3.6f;
	constexpr float CardSeconds = 3.8f;
	constexpr float MenuInputDelay = 0.6f;   // ignore stray key-repeats from skipping the intro cards
	constexpr float GlobeDiskFraction = 0.9f; // planet disk radius inside the globe texture (rest = atmosphere halo)
	constexpr float AxialTilt = -0.33f;       // pole tipped away: we look down on the equatorial belt
	const FLinearColor Ice(0.35f, 0.75f, 1.0f);
	const FVector SunDir = FVector(-0.80f, 0.32f, 0.50f).GetSafeNormal();

	// Ares-IV landing site (Jezero-like crater rim): lon 77.5 E, lat 18.4 N.
	constexpr float SiteLon = 1.353f;
	constexpr float SiteLat = 0.321f;

	float Hash3(int32 X, int32 Y, int32 Z)
	{
		uint32 H = static_cast<uint32>(X) * 374761393u + static_cast<uint32>(Y) * 668265263u + static_cast<uint32>(Z) * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return static_cast<float>((H ^ (H >> 16)) & 0xffffu) / 65535.0f;
	}

	float ValueNoise(const FVector& P)
	{
		const int32 X = FMath::FloorToInt(P.X), Y = FMath::FloorToInt(P.Y), Z = FMath::FloorToInt(P.Z);
		const float FX = P.X - X, FY = P.Y - Y, FZ = P.Z - Z;
		const float U = FX * FX * (3 - 2 * FX), V = FY * FY * (3 - 2 * FY), W = FZ * FZ * (3 - 2 * FZ);
		auto L = [](float A, float B, float T) { return A + (B - A) * T; };
		return L(
			L(L(Hash3(X, Y, Z), Hash3(X + 1, Y, Z), U), L(Hash3(X, Y + 1, Z), Hash3(X + 1, Y + 1, Z), U), V),
			L(L(Hash3(X, Y, Z + 1), Hash3(X + 1, Y, Z + 1), U), L(Hash3(X, Y + 1, Z + 1), Hash3(X + 1, Y + 1, Z + 1), U), V), W);
	}

	float Fbm(FVector P, int32 Octaves)
	{
		float Sum = 0.0f, Amp = 0.5f, Norm = 0.0f;
		for (int32 i = 0; i < Octaves; ++i)
		{
			Sum += ValueNoise(P) * Amp;
			Norm += Amp;
			P = P * 2.03f + FVector(17.1f, 3.7f, 9.2f);
			Amp *= 0.5f;
		}
		return Sum / Norm;
	}

	FVector SphereDir(float Lon, float Lat)
	{
		return FVector(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat));
	}

	float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	const float Bayer4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

	const TCHAR* ItemLabel(int32 Item)
	{
		switch (Item)
		{
		case 0:  return TEXT("BEGIN NEW MISSION");
		case 1:  return TEXT("CONTINUE");
		case 2:  return TEXT("SETTINGS & CONTROLS");
		default: return TEXT("QUIT TO DESKTOP");
		}
	}
}

void USplashIntroWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	// Intro: PARADOX logo -> PARADOX PRODUCTION TEAM poster -> main menu (no engine boot screen).
	Phase = EPhase::StudioLogo;
	PhaseTime = 0.0f;

	LogoTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/ParadoxLogo.ParadoxLogo"));
	PosterTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/ParadoxTeamPoster.ParadoxTeamPoster"));
	if (LogoTexture)
	{
		LogoBrush.SetResourceObject(LogoTexture);
		LogoBrush.ImageSize = FVector2D(LogoTexture->GetSizeX(), LogoTexture->GetSizeY());
	}
	if (PosterTexture)
	{
		PosterBrush.SetResourceObject(PosterTexture);
		PosterBrush.ImageSize = FVector2D(PosterTexture->GetSizeX(), PosterTexture->GetSizeY());
	}

	Items.Reset();
	Items.Add(EMenuItem::NewMission);
	if (UMarsSaveGame::HasAnySave())
	{
		Items.Add(EMenuItem::Continue);
	}
	Items.Add(EMenuItem::Settings);
	Items.Add(EMenuItem::Quit);

	BuildMarsAlbedo();
	GlobeTexture = UTexture2D::CreateTransient(GlobeRes, GlobeRes, PF_B8G8R8A8);
	if (GlobeTexture)
	{
		GlobeTexture->Filter = TF_Nearest; // chunky PS1 texels when scaled up
		GlobeTexture->SRGB = true;
		GlobeTexture->NeverStream = true;
		GlobeTexture->UpdateResource();
		GlobeBrush.SetResourceObject(GlobeTexture);
		GlobeBrush.ImageSize = FVector2D(GlobeRes, GlobeRes);
		RenderGlobe();
	}

	if (const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwningPlayerPawn()))
	{
		if (UMarsAudioManager* Audio = C->GetAudioManager())
		{
			Audio->PlayIntroMusic(5.0f);
		}
	}
}

void USplashIntroWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!HasKeyboardFocus())
	{
		SetKeyboardFocus();
	}
	PhaseTime += InDeltaTime;
	TotalTime += InDeltaTime;

	if ((Phase == EPhase::Boot && PhaseTime > BootSeconds) || ((Phase == EPhase::StudioLogo || Phase == EPhase::TeamPoster) && PhaseTime > CardSeconds))
	{
		NextPhase();
	}

	if (Phase == EPhase::Menu)
	{
		// Dev capture: "-OpenSettingsAt=3" opens the settings screen N seconds into the menu.
		float OpenSettingsAt = -1.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("OpenSettingsAt="), OpenSettingsAt) && !Settings.IsOpen() && PhaseTime >= OpenSettingsAt && PhaseTime - InDeltaTime < OpenSettingsAt)
		{
			Settings.Open();
		}
		Spin += InDeltaTime * (2.0f * PI / 300.0f); // one Martian "day" every 5 minutes
		GlobeUpdateAccum += InDeltaTime;
		if (GlobeUpdateAccum > 1.0f / 15.0f)
		{
			GlobeUpdateAccum = 0.0f;
			RenderGlobe();
		}
	}

	if (const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwningPlayerPawn()))
	{
		if (const UMarsAudioManager* Audio = C->GetAudioManager())
		{
			const TArray<float> B = Audio->GetMusicBands();
			for (int32 i = 0; i < 8 && i < B.Num(); ++i)
			{
				Bands[i] = FMath::FInterpTo(Bands[i], B[i], InDeltaTime, 14.0f);
			}
		}
	}
}

void USplashIntroWidget::NextPhase()
{
	if (Phase == EPhase::Menu)
	{
		return;
	}
	Phase = static_cast<EPhase>(static_cast<uint8>(Phase) + 1);
	PhaseTime = 0.0f;
}

void USplashIntroWidget::Beep(float Pitch) const
{
	if (const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwningPlayerPawn()))
	{
		if (UMarsAudioManager* Audio = C->GetAudioManager())
		{
			Audio->PlayUIBeep(Pitch);
		}
	}
}

void USplashIntroWidget::Activate(int32 Index)
{
	if (!Items.IsValidIndex(Index) || PhaseTime < MenuInputDelay)
	{
		return;
	}
	Beep(1.6f);
	switch (Items[Index])
	{
	case EMenuItem::NewMission:
		OnStartMission.Broadcast();
		break;
	case EMenuItem::Continue:
		OnContinueMission.Broadcast();
		break;
	case EMenuItem::Settings:
		Settings.Open();
		break;
	case EMenuItem::Quit:
		bConfirmQuit = true;
		break;
	}
}

FReply USplashIntroWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Phase != EPhase::Menu)
	{
		if (Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::Escape)
		{
			NextPhase();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	if (PhaseTime < MenuInputDelay || InKeyEvent.IsRepeat())
	{
		return FReply::Handled();
	}

	if (Settings.IsOpen())
	{
		Settings.OnKey(Key, GetWorld());
		Beep(1.25f);
		return FReply::Handled();
	}

	if (bConfirmQuit)
	{
		if (Key == EKeys::Y || Key == EKeys::Enter)
		{
			UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		}
		else if (Key == EKeys::N || Key == EKeys::Escape || Key == EKeys::BackSpace)
		{
			bConfirmQuit = false;
			Beep(0.9f);
		}
		return FReply::Handled();
	}

	const int32 N = Items.Num();
	if (Key == EKeys::Up || Key == EKeys::W) { Selected = (Selected + N - 1) % N; Beep(1.2f); return FReply::Handled(); }
	if (Key == EKeys::Down || Key == EKeys::S) { Selected = (Selected + 1) % N; Beep(1.2f); return FReply::Handled(); }
	// SPACE is deliberately NOT a confirm key here: it is the "skip intro" key, and mashing it used to
	// trigger whatever item the mouse cursor happened to rest on (including QUIT).
	if (Key == EKeys::Enter || Key == EKeys::E) { Activate(Selected); return FReply::Handled(); }
	return FReply::Handled();
}

FVector2D USplashIntroWidget::ItemPos(const FVector2D& Size, int32 Index) const
{
	return FVector2D(Size.X * 0.075f, Size.Y * 0.44f + Index * 50.0f);
}

int32 USplashIntroWidget::ItemAt(const FGeometry& Geo, const FVector2D& ScreenPos) const
{
	const FVector2D Local = Geo.AbsoluteToLocal(ScreenPos);
	const FVector2D Size = Geo.GetLocalSize();
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FVector2D P = ItemPos(Size, i);
		if (Local.Y >= P.Y - 8.0f && Local.Y <= P.Y + 34.0f && Local.X >= P.X - 20.0f && Local.X <= P.X + 420.0f)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FReply USplashIntroWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Phase != EPhase::Menu)
	{
		NextPhase();
		return FReply::Handled();
	}
	if (PhaseTime < MenuInputDelay)
	{
		return FReply::Handled();
	}
	if (Settings.IsOpen())
	{
		Settings.OnMouseDown(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()), InMouseEvent.GetEffectingButton(), GetWorld());
		Beep(1.25f);
		return FReply::Handled();
	}
	if (bConfirmQuit)
	{
		return FReply::Handled(); // answer with Y / N
	}
	const int32 Item = ItemAt(InGeometry, InMouseEvent.GetScreenSpacePosition());
	if (Item != INDEX_NONE && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Selected = Item;
		Activate(Item);
	}
	return FReply::Handled();
}

FReply USplashIntroWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Phase == EPhase::Menu)
	{
		if (Settings.IsOpen())
		{
			Settings.OnMouseMove(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
		}
		else if (!bConfirmQuit)
		{
			const int32 Item = ItemAt(InGeometry, InMouseEvent.GetScreenSpacePosition());
			if (Item != INDEX_NONE)
			{
				Selected = Item;
			}
		}
	}
	return FReply::Unhandled();
}

FReply USplashIntroWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Settings.OnMouseWheel(InMouseEvent.GetWheelDelta());
	return FReply::Handled();
}

float USplashIntroWidget::CardAlpha() const
{
	const float In = FMath::Clamp(PhaseTime / 0.8f, 0.0f, 1.0f);
	const float Out = FMath::Clamp((CardSeconds - PhaseTime) / 0.7f, 0.0f, 1.0f);
	return FMath::Min(In, Out);
}

// ----------------------------------------------------------------------------
// Procedural Mars
// ----------------------------------------------------------------------------

void USplashIntroWidget::BuildMarsAlbedo()
{
	Albedo.SetNumUninitialized(MapW * MapH);
	const FLinearColor Bright(0.80f, 0.48f, 0.28f);
	const FLinearColor Ochre(0.58f, 0.27f, 0.14f);
	const FLinearColor Dark(0.26f, 0.13f, 0.09f);
	const FLinearColor Ice2(0.93f, 0.91f, 0.88f);

	struct FCrater { FVector Dir; float Radius; };
	TArray<FCrater> Craters;
	FRandomStream Rng(2087);
	for (int32 i = 0; i < 150; ++i)
	{
		FCrater C;
		C.Dir = SphereDir(Rng.FRandRange(-PI, PI), FMath::Asin(Rng.FRandRange(-0.95f, 0.95f)));
		C.Radius = FMath::Pow(Rng.FRand(), 3.0f) * 0.07f + 0.008f;
		Craters.Add(C);
	}
	const FVector Olympus = SphereDir(-2.34f, 0.32f);
	const FVector Tharsis[] = { SphereDir(-1.97f, 0.21f), SphereDir(-1.85f, 0.03f), SphereDir(-2.10f, -0.15f) };
	const FVector Site = SphereDir(SiteLon, SiteLat);

	for (int32 V = 0; V < MapH; ++V)
	{
		const float Lat = HALF_PI - (V + 0.5f) / MapH * PI;
		for (int32 U = 0; U < MapW; ++U)
		{
			const float Lon = (U + 0.5f) / MapW * 2.0f * PI - PI;
			const FVector D = SphereDir(Lon, Lat);

			const float N1 = Fbm(D * 3.0f, 5);
			FLinearColor C = FMath::Lerp(Bright, Ochre, SmoothStep(0.35f, 0.65f, N1));

			// Dark albedo provinces (Syrtis / Acidalia-like).
			const float Maria = Fbm(D * 2.1f + FVector(7.0f, 1.0f, 3.0f), 4);
			C = FMath::Lerp(C, Dark, SmoothStep(0.54f, 0.68f, Maria) * 0.85f);
			// Southern highlands a touch darker and rougher.
			C *= 1.0f - 0.12f * SmoothStep(0.1f, -0.5f, Lat) * Fbm(D * 9.0f, 3);

			// Valles Marineris: a long canyon scar east of Tharsis.
			if (Lon > -1.62f && Lon < -0.52f)
			{
				const float Centre = -0.15f + 0.035f * FMath::Sin(Lon * 6.0f) + 0.02f * (Fbm(D * 12.0f, 2) - 0.5f);
				const float Width = 0.012f + 0.02f * SmoothStep(-1.6f, -0.9f, Lon) * SmoothStep(-0.5f, -0.9f, Lon);
				const float Canyon = 1.0f - SmoothStep(Width, Width + 0.02f, FMath::Abs(Lat - Centre));
				C = FMath::Lerp(C, Dark * 0.8f, Canyon * 0.8f);
			}

			// Olympus Mons + Tharsis shield volcanoes: pale domes with a dark escarpment ring.
			const float AO = FMath::Acos(FMath::Clamp(FVector::DotProduct(D, Olympus), -1.0f, 1.0f));
			if (AO < 0.12f)
			{
				C = AO < 0.09f ? FMath::Lerp(C, Bright * 1.1f, 0.6f * (1.0f - AO / 0.09f)) : C * 0.6f;
				if (AO < 0.012f) { C *= 0.5f; } // caldera
			}
			for (const FVector& T : Tharsis)
			{
				const float A = FMath::Acos(FMath::Clamp(FVector::DotProduct(D, T), -1.0f, 1.0f));
				if (A < 0.05f) { C = FMath::Lerp(C, Bright * 1.05f, 0.5f * (1.0f - A / 0.05f)); }
			}

			// Impact craters: dark floor, bright rim.
			for (const FCrater& K : Craters)
			{
				const float Dot = FVector::DotProduct(D, K.Dir);
				if (Dot < FMath::Cos(K.Radius * 1.3f))
				{
					continue;
				}
				const float A = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)) / K.Radius;
				if (A < 0.8f) { C *= 0.82f + 0.1f * A; }
				else if (A < 1.15f) { C *= 1.12f; }
			}

			// Polar caps (north larger), ragged edge.
			const float CapNoise = (Fbm(D * 7.0f, 3) - 0.5f) * 0.12f;
			const float Cap = FMath::Max(SmoothStep(1.26f, 1.34f, Lat + CapNoise), SmoothStep(-1.30f, -1.38f, Lat + CapNoise));
			C = FMath::Lerp(C, Ice2, Cap);

			// Our crater: a faint bright ring at the outpost.
			const float AS = FMath::Acos(FMath::Clamp(FVector::DotProduct(D, Site), -1.0f, 1.0f));
			if (AS > 0.022f && AS < 0.03f) { C *= 1.2f; }

			Albedo[V * MapW + U] = C;
		}
	}
}

void USplashIntroWidget::RenderGlobe()
{
	if (!GlobeTexture || Albedo.Num() != MapW * MapH)
	{
		return;
	}
	const int32 N = GlobeRes;
	uint8* Data = static_cast<uint8*>(FMemory::Malloc(N * N * 4));
	const float CT = FMath::Cos(AxialTilt), ST = FMath::Sin(AxialTilt);
	const FLinearColor HazeColor(1.0f, 0.55f, 0.33f);

	auto Quantize = [](float V01, float Dither) -> uint8
	{
		const float Q = FMath::Clamp(FMath::FloorToFloat(V01 * 31.0f + Dither), 0.0f, 31.0f);
		return static_cast<uint8>(Q * 255.0f / 31.0f);
	};

	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			uint8* Px = Data + (Y * N + X) * 4;
			const float PX = ((X + 0.5f) / N * 2.0f - 1.0f) / GlobeDiskFraction;
			const float PY = -((Y + 0.5f) / N * 2.0f - 1.0f) / GlobeDiskFraction;
			const float R2 = PX * PX + PY * PY;
			const float Dither = Bayer4[(Y & 3) * 4 + (X & 3)] / 16.0f;
			FLinearColor Col = FLinearColor::Black;
			float Alpha = 1.0f;

			if (R2 <= 1.0f)
			{
				const float PZ = FMath::Sqrt(1.0f - R2);
				const FVector Nrm(PX, PY, PZ);
				// View -> planet frame: undo axial tilt (about view X), then spin about the polar axis.
				const float Yp = PY * CT + PZ * ST;
				const float Zp = -PY * ST + PZ * CT;
				const float Lat = FMath::Asin(FMath::Clamp(Yp, -1.0f, 1.0f));
				float Lon = FMath::Atan2(PX, Zp) + Spin;
				Lon = FMath::Fmod(Lon + PI, 2.0f * PI);
				if (Lon < 0.0f) { Lon += 2.0f * PI; }
				const int32 U = FMath::Clamp(static_cast<int32>(Lon / (2.0f * PI) * MapW), 0, MapW - 1);
				const int32 V = FMath::Clamp(static_cast<int32>((HALF_PI - Lat) / PI * MapH), 0, MapH - 1);
				const FLinearColor A = Albedo[V * MapW + U];

				const float NdL = FVector::DotProduct(Nrm, SunDir);
				const float Light = FMath::Pow(FMath::Clamp(NdL * 1.05f + 0.04f, 0.0f, 1.0f), 0.85f);
				const float Rim = FMath::Pow(1.0f - PZ, 2.4f);
				Col = A * (Light + 0.012f) + HazeColor * Rim * 0.55f * FMath::Clamp(NdL + 0.35f, 0.0f, 1.0f);
				// Dust haze softens the terminator.
				Col += HazeColor * 0.05f * SmoothStep(-0.15f, 0.1f, NdL) * (1.0f - SmoothStep(0.1f, 0.4f, NdL));
			}
			else
			{
				// Thin atmosphere halo, brightest on the sunward limb.
				const float R = FMath::Sqrt(R2);
				const float Halo = FMath::Square(FMath::Clamp(1.0f - (R - 1.0f) / 0.1f, 0.0f, 1.0f));
				const float Sunward = FMath::Clamp((PX * SunDir.X + PY * SunDir.Y) / R * 0.8f + 0.35f, 0.0f, 1.0f);
				Col = HazeColor * 0.9f;
				Alpha = Halo * Sunward * 0.8f;
			}

			const FColor S = Col.GetClamped().ToFColor(true);
			Px[0] = Quantize(S.B / 255.0f, Dither);
			Px[1] = Quantize(S.G / 255.0f, Dither);
			Px[2] = Quantize(S.R / 255.0f, Dither);
			Px[3] = static_cast<uint8>(FMath::Clamp(Alpha, 0.0f, 1.0f) * 255.0f);
		}
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, N, N);
	GlobeTexture->UpdateTextureRegions(0, 1, Region, N * 4, 4, Data, [](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
	{
		FMemory::Free(SrcData);
		delete Regions;
	});
}

FVector2D USplashIntroWidget::GlobeCenter(const FVector2D& Size) const
{
	// The ship drifts very slightly in its orbit.
	return FVector2D(Size.X * 0.70f + FMath::Sin(TotalTime * 0.11f) * 8.0f, Size.Y * 1.02f + FMath::Sin(TotalTime * 0.07f) * 6.0f);
}

float USplashIntroWidget::GlobeRadius(const FVector2D& Size) const
{
	return Size.Y * 0.80f;
}

// ----------------------------------------------------------------------------
// Painting
// ----------------------------------------------------------------------------

void USplashIntroWidget::PaintBoot(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const FVector2D Size = Geo.GetLocalSize();
	static const TCHAR* Lines[] =
	{
		TEXT("UNREAL ENGINE 5.3  //  RUNTIME BOOT SEQUENCE"),
		TEXT(""),
		TEXT("RHI ......................... D3D12   [ OK ]"),
		TEXT("SHADER MODEL ................ SM5     [ OK ]"),
		TEXT("PROCEDURAL MESH FACTORY ............. [ OK ]"),
		TEXT("REAL-TIME AUDIO SYNTHESIS ........... [ OK ]"),
		TEXT("PS1 RASTER EMULATION (DITHER/AFFINE)  [ OK ]"),
		TEXT("A.R.I.A. TELEMETRY CORE ............. [ OK ]"),
		TEXT(""),
		TEXT("LOADING ARES-IV FORWARD OUTPOST ..."),
	};
	const int32 Visible = FMath::Clamp(FMath::FloorToInt(PhaseTime / 0.28f), 0, UE_ARRAY_COUNT(Lines));
	const FSlateFontInfo Font = MarsUI::Mono(13, 60);
	float Y = Size.Y * 0.28f;
	const float X = Size.X * 0.22f;
	for (int32 i = 0; i < Visible; ++i)
	{
		MarsUI::Text(Out, Layer, Geo, Lines[i], FVector2D(X, Y), Font, i == 0 ? FLinearColor::White : MarsUI::Phosphor);
		Y += 24.0f;
	}
	const FVector2D BadgePos(X, Size.Y * 0.18f);
	MarsUI::Frame(Out, Layer, Geo, BadgePos - FVector2D(10, 8), FVector2D(330, 40), FLinearColor(1, 1, 1, 0.7f));
	MarsUI::Text(Out, Layer, Geo, TEXT("POWERED BY UNREAL ENGINE 5"), BadgePos, MarsUI::Mono(14, 160), FLinearColor::White);
	const float P = FMath::Clamp(PhaseTime / BootSeconds, 0.0f, 1.0f);
	MarsUI::Frame(Out, Layer, Geo, FVector2D(X, Size.Y * 0.72f), FVector2D(Size.X * 0.56f, 14.0f), MarsUI::Phosphor * 0.7f);
	MarsUI::Box(Out, Layer, Geo, FVector2D(X + 3, Size.Y * 0.72f + 3), FVector2D((Size.X * 0.56f - 6) * P, 8.0f), MarsUI::Phosphor);
	if (FMath::Frac(TotalTime * 2.0f) < 0.5f)
	{
		MarsUI::Text(Out, Layer, Geo, TEXT("_"), FVector2D(X, Y), Font, MarsUI::Phosphor);
	}
}

void USplashIntroWidget::PaintEmblem(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Center, float Scale, float Alpha) const
{
	for (int32 i = -40; i <= 40; ++i)
	{
		const float T = i / 40.0f;
		const FVector2D P = Center + FVector2D(T * 170.0f, 90.0f * T * T - 30.0f) * Scale;
		MarsUI::Box(Out, Layer, Geo, P - FVector2D(3, 3) * Scale, FVector2D(6, 6) * Scale, FLinearColor(0.85f, 0.95f, 1.0f, Alpha));
	}
	for (int32 k = -1; k <= 1; ++k)
	{
		const float H = (k == 0 ? 200.0f : 120.0f) * Scale;
		MarsUI::Box(Out, Layer, Geo, Center + FVector2D(k * 22.0f * Scale - 3.0f, -H * 0.6f), FVector2D(6.0f, H), FLinearColor(Ice.R, Ice.G, Ice.B, Alpha * (k == 0 ? 0.9f : 0.5f)));
	}
	MarsUI::Box(Out, Layer + 1, Geo, Center + FVector2D(-10, -70) * Scale, FVector2D(20, 70) * Scale, FLinearColor(1, 1, 1, Alpha));
	MarsUI::TextCentered(Out, Layer + 1, Geo, TEXT("P A R A D O X"), Center + FVector2D(0, 110) * Scale, MarsUI::Mono(28, 500), FLinearColor(1, 1, 1, Alpha));
}

void USplashIntroWidget::PaintImageCard(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FSlateBrush& Brush, bool bHasTexture, bool bLogo) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const float Alpha = CardAlpha();
	if (bHasTexture)
	{
		const FVector2D Img = Brush.ImageSize;
		const float Fit = FMath::Min(Size.Y * 0.8f / FMath::Max(Img.Y, 1.0), Size.X * 0.8f / FMath::Max(Img.X, 1.0));
		const FVector2D DrawSize = Img * Fit * (1.0f + 0.03f * PhaseTime / CardSeconds);
		const FVector2D Pos = (Size - DrawSize) * 0.5f;
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(DrawSize), FSlateLayoutTransform(FVector2f(Pos))), &Brush, ESlateDrawEffect::None, FLinearColor(1, 1, 1, Alpha));
	}
	else if (bLogo)
	{
		PaintEmblem(Out, Layer, Geo, Size * 0.5f, Size.Y / 900.0f, Alpha);
	}
	else
	{
		MarsUI::TextCentered(Out, Layer, Geo, TEXT("PARADOX PRODUCTION TEAM"), FVector2D(Size.X * 0.5f, Size.Y * 0.46f), MarsUI::Mono(26, 260), FLinearColor(0.85f, 0.2f, 0.2f, Alpha));
	}
	if (!bLogo)
	{
		MarsUI::TextCentered(Out, Layer + 2, Geo, TEXT("A  PARADOX  PRODUCTION  TEAM  GAME"), FVector2D(Size.X * 0.5f, Size.Y * 0.93f), MarsUI::Mono(11, 300), FLinearColor(0.8f, 0.8f, 0.8f, Alpha * 0.8f));
	}
}

void USplashIntroWidget::PaintEqualizer(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size) const
{
	const int32 Segments = 12;
	const float BarW = Size.X / 8.0f;
	const float SegH = Size.Y / Segments;
	for (int32 b = 0; b < 8; ++b)
	{
		const float Level = FMath::Clamp(Bands[b] * (0.7f + 0.3f * FMath::Sin(TotalTime * 1.7f + b)), 0.0f, 1.0f);
		const int32 Lit = FMath::RoundToInt(Level * Segments);
		for (int32 s = 0; s < Segments; ++s)
		{
			const float Frac = (s + 0.5f) / Segments;
			const FLinearColor Col = Frac > 0.85f ? MarsUI::Alarm : Frac > 0.6f ? MarsUI::Amber : MarsUI::Phosphor;
			const FVector2D P(Pos.X + b * BarW + 2.0f, Pos.Y + Size.Y - (s + 1) * SegH + 1.0f);
			MarsUI::Box(Out, Layer, Geo, P, FVector2D(BarW - 4.0f, SegH - 2.0f), s < Lit ? Col : Col * FLinearColor(1, 1, 1, 0.08f));
		}
	}
}

void USplashIntroWidget::PaintOrbitView(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const FVector2D C = GlobeCenter(Size);
	const float R = GlobeRadius(Size);

	// Deep space + stars (occluded by the planet).
	MarsUI::Box(Out, Layer, Geo, FVector2D::ZeroVector, Size, FLinearColor(0.004f, 0.005f, 0.012f));
	FRandomStream Rng(42);
	for (int32 i = 0; i < 320; ++i)
	{
		FVector2D P(Rng.FRand() * Size.X, Rng.FRand() * Size.Y);
		P.X = FMath::Fmod(P.X - TotalTime * 1.5f + Size.X, Size.X);
		if (FVector2D::Distance(P, C) < R * 1.04f)
		{
			continue;
		}
		const float Mag = Rng.FRand();
		const float Tw = 0.6f + 0.4f * FMath::Sin(TotalTime * (1.0f + Rng.FRand() * 3.0f) + i);
		const float S = Mag > 0.97f ? 3.0f : Mag > 0.8f ? 2.0f : 1.0f;
		const FLinearColor Tint = Mag > 0.9f ? FLinearColor(1.0f, 0.85f, 0.7f) : FLinearColor(0.75f, 0.85f, 1.0f);
		MarsUI::Box(Out, Layer + 1, Geo, P, FVector2D(S, S), Tint * FLinearColor(1, 1, 1, (0.25f + 0.75f * Mag) * Tw));
	}

	// Phobos orbit parameters (also used to sort in front of / behind the planet).
	const float PhA = TotalTime * 0.045f + 2.2f;
	const FVector2D PhPos = C + FVector2D(FMath::Cos(PhA) * R * 1.45f, -R * 0.62f + FMath::Sin(PhA) * R * 0.16f);
	const bool bPhobosFront = FMath::Sin(PhA) > 0.0f;
	auto PaintPhobos = [&](int32 L)
	{
		for (int32 y = -3; y <= 3; ++y)
		{
			for (int32 x = -4; x <= 4; ++x)
			{
				if (x * x * 0.55f + y * y > 10.0f) { continue; }
				const float Shade = FMath::Clamp(0.55f - x * 0.07f + y * 0.03f, 0.08f, 0.8f);
				MarsUI::Box(Out, L, Geo, PhPos + FVector2D(x, y) * 3.0f, FVector2D(3.0f, 3.0f), FLinearColor(Shade * 0.55f, Shade * 0.47f, Shade * 0.42f));
			}
		}
	};
	if (!bPhobosFront)
	{
		PaintPhobos(Layer + 1);
	}

	// The planet.
	if (GlobeTexture)
	{
		const float D = 2.0f * R / GlobeDiskFraction;
		FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2f(D, D), FSlateLayoutTransform(FVector2f(C - FVector2D(D, D) * 0.5f))),
			&GlobeBrush, ESlateDrawEffect::None, FLinearColor::White);
	}

	if (bPhobosFront)
	{
		PaintPhobos(Layer + 3);
	}

	// HUD projected on the glass: our parking orbit (dashed) and the outpost marker.
	{
		TArray<FVector2f> Dash;
		for (int32 i = 0; i < 160; ++i)
		{
			const float A0 = i / 160.0f * 2.0f * PI + TotalTime * 0.02f;
			const float A1 = A0 + 2.0f * PI / 320.0f;
			const FVector2D P0 = C + FVector2D(FMath::Cos(A0) * R * 1.16f, -R * 0.5f + FMath::Sin(A0) * R * 0.22f);
			const FVector2D P1 = C + FVector2D(FMath::Cos(A1) * R * 1.16f, -R * 0.5f + FMath::Sin(A1) * R * 0.22f);
			const bool bBehind = FMath::Sin(A0) < 0.0f && FVector2D::Distance(P0, C) < R;
			if (bBehind) { continue; }
			Dash.Reset();
			Dash.Add(FVector2f(P0));
			Dash.Add(FVector2f(P1));
			FSlateDrawElement::MakeLines(Out, Layer + 4, Geo.ToPaintGeometry(), Dash, ESlateDrawEffect::None, MarsUI::Amber * FLinearColor(1, 1, 1, 0.35f), true, 1.0f);
		}

		// Outpost marker: same projection as RenderGlobe, inverted.
		const float LonView = SiteLon - Spin;
		const float Xp = FMath::Cos(SiteLat) * FMath::Sin(LonView);
		const float Zp = FMath::Cos(SiteLat) * FMath::Cos(LonView);
		const float Yp = FMath::Sin(SiteLat);
		const float CT = FMath::Cos(AxialTilt), ST = FMath::Sin(AxialTilt);
		const float Yv = Yp * CT - Zp * ST;
		const float Zv = Yp * ST + Zp * CT;
		if (Zv > 0.05f)
		{
			const FVector2D M = C + FVector2D(Xp, -Yv) * R;
			const float Blink = FMath::Frac(TotalTime * 1.2f) < 0.6f ? 1.0f : 0.35f;
			const FLinearColor MC = MarsUI::Amber * FLinearColor(1, 1, 1, Blink);
			const float B = 10.0f;
			MarsUI::Frame(Out, Layer + 5, Geo, M - FVector2D(B, B), FVector2D(B, B) * 2.0f, MC);
			MarsUI::Box(Out, Layer + 5, Geo, M - FVector2D(1, 1), FVector2D(3, 3), MC);
			TArray<FVector2f> Leader = { FVector2f(M + FVector2D(B, -B)), FVector2f(M + FVector2D(60, -60)), FVector2f(M + FVector2D(190, -60)) };
			FSlateDrawElement::MakeLines(Out, Layer + 5, Geo.ToPaintGeometry(), Leader, ESlateDrawEffect::None, MC * FLinearColor(1, 1, 1, 0.8f), true, 1.0f);
			MarsUI::Text(Out, Layer + 5, Geo, TEXT("ARES-IV  OUTPOST"), M + FVector2D(66, -80), MarsUI::Mono(10, 200), MC);
			MarsUI::Text(Out, Layer + 5, Geo, TEXT("18.4N  77.5E  //  1 CREW"), M + FVector2D(66, -56), MarsUI::Mono(8, 150), MarsUI::AmberDim * 1.6f);
		}
	}
}

void USplashIntroWidget::PaintViewportFrame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const float M = FMath::Max(Size.Y * 0.035f, 22.0f); // bezel width
	const float Ch = Size.Y * 0.11f;                   // corner chamfer
	const FLinearColor Hull(0.045f, 0.047f, 0.052f);
	const FLinearColor HullLight(0.11f, 0.11f, 0.12f);

	// Bezel bars.
	MarsUI::Box(Out, Layer, Geo, FVector2D(0, 0), FVector2D(Size.X, M), Hull);
	MarsUI::Box(Out, Layer, Geo, FVector2D(0, Size.Y - M), FVector2D(Size.X, M), Hull);
	MarsUI::Box(Out, Layer, Geo, FVector2D(0, 0), FVector2D(M, Size.Y), Hull);
	MarsUI::Box(Out, Layer, Geo, FVector2D(Size.X - M, 0), FVector2D(M, Size.Y), Hull);

	// Chamfered corners (thick lines offset toward each corner).
	const FVector2D Corners[4] = { FVector2D(M, M), FVector2D(Size.X - M, M), FVector2D(Size.X - M, Size.Y - M), FVector2D(M, Size.Y - M) };
	const FVector2D Dirs[4][2] = { { FVector2D(1, 0), FVector2D(0, 1) }, { FVector2D(-1, 0), FVector2D(0, 1) }, { FVector2D(-1, 0), FVector2D(0, -1) }, { FVector2D(1, 0), FVector2D(0, -1) } };
	TArray<FVector2f> Pts;
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector2D A = Corners[k] + Dirs[k][0] * Ch;
		const FVector2D B = Corners[k] + Dirs[k][1] * Ch;
		const FVector2D Out2 = -(Dirs[k][0] + Dirs[k][1]).GetSafeNormal();
		const float T = Ch * 0.75f;
		Pts = { FVector2f(A + Out2 * T * 0.5f), FVector2f(B + Out2 * T * 0.5f) };
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Hull, false, T);
		// Bevel highlight along the inner chamfer edge.
		Pts = { FVector2f(A), FVector2f(B) };
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, HullLight, true, 2.0f);
	}

	// Inner gasket + bevel highlight along the straight edges.
	auto Edge = [&](const FVector2D& A, const FVector2D& B)
	{
		Pts = { FVector2f(A), FVector2f(B) };
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, HullLight, true, 2.0f);
	};
	Edge(FVector2D(M + Ch, M), FVector2D(Size.X - M - Ch, M));
	Edge(FVector2D(M + Ch, Size.Y - M), FVector2D(Size.X - M - Ch, Size.Y - M));
	Edge(FVector2D(M, M + Ch), FVector2D(M, Size.Y - M - Ch));
	Edge(FVector2D(Size.X - M, M + Ch), FVector2D(Size.X - M, Size.Y - M - Ch));

	// Rivets.
	for (float X = M + Ch + 20.0f; X < Size.X - M - Ch - 10.0f; X += 64.0f)
	{
		MarsUI::Box(Out, Layer + 1, Geo, FVector2D(X, M * 0.4f), FVector2D(3, 3), HullLight * 1.6f);
		MarsUI::Box(Out, Layer + 1, Geo, FVector2D(X, Size.Y - M * 0.6f), FVector2D(3, 3), HullLight * 1.6f);
	}
	for (float Y = M + Ch + 20.0f; Y < Size.Y - M - Ch - 10.0f; Y += 64.0f)
	{
		MarsUI::Box(Out, Layer + 1, Geo, FVector2D(M * 0.4f, Y), FVector2D(3, 3), HullLight * 1.6f);
		MarsUI::Box(Out, Layer + 1, Geo, FVector2D(Size.X - M * 0.6f, Y), FVector2D(3, 3), HullLight * 1.6f);
	}

	// Stencils on the frame.
	MarsUI::Text(Out, Layer + 2, Geo, TEXT("VIEWPORT 2  //  MTV ODYSSEUS  //  DO NOT LEAN ON GLASS"), FVector2D(M + Ch + 10.0f, Size.Y - M + 4.0f), MarsUI::Mono(8, 200), FLinearColor(0.32f, 0.3f, 0.26f));

	// Glass reflections.
	Pts = { FVector2f(Size.X * 0.30f, M), FVector2f(Size.X * 0.12f, Size.Y - M) };
	FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.7f, 0.8f, 1.0f, 0.018f), false, 70.0f);
	Pts = { FVector2f(Size.X * 0.37f, M), FVector2f(Size.X * 0.22f, Size.Y - M) };
	FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.7f, 0.8f, 1.0f, 0.012f), false, 18.0f);
}

void USplashIntroWidget::PaintMenu(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const float Fade = FMath::Clamp(PhaseTime / 1.5f, 0.0f, 1.0f);

	PaintOrbitView(Out, Layer, Geo);

	// Dark gradient behind the menu column so text stays readable over the planet.
	for (int32 i = 0; i < 24; ++i)
	{
		const float W = Size.X * 0.48f * (1.0f - i / 24.0f);
		MarsUI::Box(Out, Layer + 6, Geo, FVector2D(0, 0), FVector2D(W, Size.Y), FLinearColor(0, 0, 0, 0.035f));
	}

	PaintViewportFrame(Out, Layer + 7, Geo);

	const float Lx = Size.X * 0.075f;
	const FLinearColor FadeTint(1, 1, 1, Fade);
	MarsUI::Text(Out, Layer + 10, Geo, TEXT("ARES-VI"), FVector2D(Lx, Size.Y * 0.18f), MarsUI::Mono(48, 700), MarsUI::Amber * FadeTint);
	MarsUI::Text(Out, Layer + 10, Geo, TEXT("A MARTIAN SURVIVAL STORY  //  ARES-IV OUTPOST  //  2087"), FVector2D(Lx + 4.0f, Size.Y * 0.18f + 66.0f), MarsUI::Mono(11, 260), MarsUI::AmberDim * 1.5f * FadeTint);
	MarsUI::Box(Out, Layer + 10, Geo, FVector2D(Lx + 4.0f, Size.Y * 0.18f + 94.0f), FVector2D(380.0f, 1.0f), MarsUI::AmberDim * FadeTint);

	// Orbital telemetry (top right, on the glass).
	{
		const float Tx = Size.X * 0.66f;
		const float Ty = Size.Y * 0.09f;
		const int32 Secs = FMath::Max(0, 14 * 60 + 22 - FMath::FloorToInt(TotalTime));
		const FSlateFontInfo F = MarsUI::Mono(9, 160);
		const FLinearColor TC = MarsUI::Phosphor * FLinearColor(1, 1, 1, 0.8f * Fade);
		MarsUI::Text(Out, Layer + 10, Geo, TEXT("MTV ODYSSEUS  //  AREOCENTRIC ORBIT"), FVector2D(Tx, Ty), F, TC);
		MarsUI::Text(Out, Layer + 10, Geo, FString::Printf(TEXT("ALT 412.%d KM   INC 25.2   V 3.34 KM/S"), FMath::FloorToInt(TotalTime * 3.0f) % 10), FVector2D(Tx, Ty + 18.0f), F, TC);
		MarsUI::Text(Out, Layer + 10, Geo, FString::Printf(TEXT("DESCENT WINDOW  T-%02d:%02d"), Secs / 60, Secs % 60), FVector2D(Tx, Ty + 36.0f), F, MarsUI::Amber * FLinearColor(1, 1, 1, Fade));
		MarsUI::Text(Out, Layer + 10, Geo, TEXT("SURFACE: DUST STORM ADVISORY  [!]"), FVector2D(Tx, Ty + 54.0f), F, (FMath::Frac(TotalTime) < 0.5f ? MarsUI::Alarm : MarsUI::AmberDim) * FLinearColor(1, 1, 1, Fade));
	}

	// Menu items.
	const bool bModal = Settings.IsOpen() || bConfirmQuit;
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const bool bSel = i == Selected && !bModal;
		const FVector2D P = ItemPos(Size, i);
		if (bSel)
		{
			MarsUI::Box(Out, Layer + 9, Geo, P - FVector2D(14.0f, 7.0f), FVector2D(400.0f, 38.0f), FLinearColor(MarsUI::Amber.R, MarsUI::Amber.G, MarsUI::Amber.B, 0.12f + 0.05f * FMath::Sin(TotalTime * 4.0f)));
			MarsUI::Box(Out, Layer + 10, Geo, P - FVector2D(14.0f, 7.0f), FVector2D(3.0f, 38.0f), MarsUI::Amber);
		}
		const int32 LabelIndex = static_cast<int32>(Items[i]);
		MarsUI::Text(Out, Layer + 10, Geo, FString::Printf(TEXT("%s%s"), bSel ? TEXT("> ") : TEXT("  "), ItemLabel(LabelIndex)), P, MarsUI::Mono(17, 260),
			(bSel ? MarsUI::Amber : MarsUI::AmberDim * 1.5f) * FadeTint);
	}

	// Music + EQ (bottom left).
	const FVector2D EqPos(Lx, Size.Y * 0.80f);
	PaintEqualizer(Out, Layer + 10, Geo, EqPos, FVector2D(200.0f, 48.0f));
	const AMarsCharacter* Ch = Cast<AMarsCharacter>(GetOwningPlayerPawn());
	const bool bCustom = Ch && Ch->GetAudioManager() && Ch->GetAudioManager()->IsUsingCustomTrack();
	MarsUI::Text(Out, Layer + 10, Geo, bCustom ? TEXT("NOW PLAYING: INTRO THEME  (SLOWED + REVERB)") : TEXT("NOW PLAYING: ARES-IV  (ORIGINAL SCORE)"),
		EqPos + FVector2D(214.0f, 18.0f), MarsUI::Mono(8, 180), MarsUI::Phosphor * 0.8f);
	MarsUI::Text(Out, Layer + 10, Geo, TEXT("W/S  SELECT     ENTER  CONFIRM     MOUSE  CLICK"), FVector2D(Lx, Size.Y * 0.90f), MarsUI::Mono(8, 200), MarsUI::AmberDim * 1.3f);

	// Settings screen.
	if (Settings.IsOpen())
	{
		MarsUI::Box(Out, Layer + 20, Geo, FVector2D::ZeroVector, Size, FLinearColor(0, 0, 0, 0.55f));
		const FVector2D PS(FMath::Min(Size.X * 0.62f, 980.0f), Size.Y * 0.80f);
		Settings.Paint(Out, Layer + 21, Geo, (Size - PS) * 0.5f, PS, TotalTime);
	}

	// Quit confirmation.
	if (bConfirmQuit)
	{
		MarsUI::Box(Out, Layer + 20, Geo, FVector2D::ZeroVector, Size, FLinearColor(0, 0, 0, 0.6f));
		const FVector2D BS(560.0f, 150.0f);
		const FVector2D BP = (Size - BS) * 0.5f;
		MarsUI::Box(Out, Layer + 21, Geo, BP, BS, FLinearColor(0.02f, 0.01f, 0.01f, 0.95f));
		MarsUI::Frame(Out, Layer + 22, Geo, BP, BS, MarsUI::Alarm, 2.0f);
		MarsUI::TextCentered(Out, Layer + 22, Geo, TEXT("ABORT MISSION AND QUIT TO DESKTOP?"), BP + FVector2D(BS.X * 0.5f, 34.0f), MarsUI::Mono(14, 200), MarsUI::Alarm);
		MarsUI::TextCentered(Out, Layer + 22, Geo, TEXT("[ Y ]  QUIT          [ N / ESC ]  STAY"), BP + FVector2D(BS.X * 0.5f, 90.0f), MarsUI::Mono(12, 200), MarsUI::Amber);
	}
}

int32 USplashIntroWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = Geo.GetLocalSize();
	MarsUI::Box(Out, LayerId, Geo, FVector2D::ZeroVector, Size, FLinearColor::Black);

	switch (Phase)
	{
	case EPhase::Boot:       PaintBoot(Out, LayerId + 1, Geo); break;
	case EPhase::StudioLogo: PaintImageCard(Out, LayerId + 1, Geo, LogoBrush, LogoTexture != nullptr, true); break;
	case EPhase::TeamPoster: PaintImageCard(Out, LayerId + 1, Geo, PosterBrush, PosterTexture != nullptr, false); break;
	case EPhase::Menu:       PaintMenu(Out, LayerId + 1, Geo); break;
	}

	if (Phase != EPhase::Menu)
	{
		MarsUI::Text(Out, LayerId + 40, Geo, TEXT("SPACE / CLICK TO SKIP"), FVector2D(Size.X - 230.0f, Size.Y - 34.0f), MarsUI::Mono(9, 200), FLinearColor(1, 1, 1, 0.35f));
	}

	// CRT scanlines + slow roll bar.
	for (float SY = 0.0f; SY < Size.Y; SY += 3.0f)
	{
		MarsUI::Box(Out, LayerId + 45, Geo, FVector2D(0, SY), FVector2D(Size.X, 1.0f), FLinearColor(0, 0, 0, 0.22f));
	}
	const float Roll = FMath::Fmod(TotalTime * 60.0f, Size.Y + 120.0f) - 120.0f;
	MarsUI::Box(Out, LayerId + 45, Geo, FVector2D(0, Roll), FVector2D(Size.X, 120.0f), FLinearColor(1, 1, 1, 0.02f));
	return LayerId + 46;
}
