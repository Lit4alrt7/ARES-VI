#include "UI/IntroBriefingWidget.h"

#include "Audio/MarsAudioManager.h"
#include "Player/MarsCharacter.h"
#include "UI/MarsUIStyle.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr float CharsPerSecond = 55.0f;
}

void UIntroBriefingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);

	Pages =
	{
		{ TEXT("SUBJECT"),
		  TEXT("DR. DAVID MILLER, 27\nLEAD BIO-ENGINEER\nARES-IV FORWARD OUTPOST\nMARTIAN SURFACE, ARABIA TERRA\n\nCREW COMPLEMENT: 1 (OTHER CREW EVACUATED SOL 97)"), 0 },
		{ TEXT("DATE"),
		  TEXT("SOL 142, YEAR 2087\nLOCAL MEAN SOLAR TIME 18:30\n\nEXTERNAL: -61 C, 0.61 kPa, 95% CO2\nSEASON: LATE NORTHERN AUTUMN - DUST STORM SEASON"), 1 },
		{ TEXT("INCIDENT LOG"),
		  TEXT("TELEMETRY LINK WITH EARTH SEVERED FOLLOWING A CORONAL\nMASS EJECTION AND THE ONSET OF A GLOBAL DUST STORM.\n\nDURING DEEP REGOLITH CORE SAMPLING, CONTAINMENT FAILED.\nACUTE EXPOSURE CONFIRMED TO:\n  - PERCHLORATE SALTS (ClO4-)\n  - GALACTIC COSMIC RADIATION\n  - UNCLASSIFIED EXTREMOPHILE, NEURO-ACTIVE BIO-AGENT"), 2 },
		{ TEXT("DIRECTIVE"),
		  TEXT("1. DIAGNOSE PHYSIOLOGICAL DEGRADATION IN THE MED-BAY.\n2. MAINTAIN HABITAT LIFE SUPPORT: PRESSURE, SEALS, CO2.\n3. ASSEMBLE THE QUANTUM TRANSCEIVERS AND ALIGN THE DISH\n   TO RE-ESTABLISH EARTH UPLINK BEFORE SYSTEMIC ORGAN\n   FAILURE. ORBITAL COMM WINDOW: 45 MINUTES.\n\nA.R.I.A. WILL REPORT ONLY WHAT ITS SENSORS MEASURE.\nTRUST THE INSTRUMENTS."), 3 }
	};
}

void UIntroBriefingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!HasKeyboardFocus())
	{
		SetKeyboardFocus();
	}
	PageTime += InDeltaTime;
	TotalTime += InDeltaTime;

	// Teletype tick every few characters.
	if (Pages.IsValidIndex(PageIndex))
	{
		const int32 Chars = FMath::FloorToInt(PageTime * CharsPerSecond);
		if (Chars < Pages[PageIndex].Body.Len() && Chars - LastBeepChar >= 5)
		{
			LastBeepChar = Chars;
			if (const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwningPlayerPawn()))
			{
				if (UMarsAudioManager* Audio = C->GetAudioManager())
				{
					Audio->PlayUIBeep(2.2f);
				}
			}
		}
	}
}

void UIntroBriefingWidget::Advance()
{
	if (bFinished || !Pages.IsValidIndex(PageIndex))
	{
		return;
	}
	const int32 Total = Pages[PageIndex].Body.Len();
	if (PageTime * CharsPerSecond < Total)
	{
		PageTime = Total / CharsPerSecond + 0.01f; // reveal the whole page first
		return;
	}
	if (PageIndex < Pages.Num() - 1)
	{
		++PageIndex;
		PageTime = 0.0f;
		LastBeepChar = 0;
		return;
	}
	bFinished = true;
	OnFinished.Broadcast();
}

FReply UIntroBriefingWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::E)
	{
		Advance();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UIntroBriefingWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Advance();
	return FReply::Handled();
}

float UIntroBriefingWidget::ImageValue(int32 Image, float U, float V, float Time)
{
	auto Hash = [](float X, float Y) { return FMath::Frac(FMath::Sin(X * 12.9898f + Y * 78.233f) * 43758.5453f); };
	switch (Image)
	{
	case 0: // Mars globe: lit sphere, craters, polar cap, Valles-like scar
	{
		const float X = (U - 0.5f) * 2.2f, Y = (V - 0.5f) * 2.2f;
		const float R2 = X * X + Y * Y;
		if (R2 > 1.0f)
		{
			return Hash(U * 50.0f, V * 50.0f) > 0.995f ? 0.8f : 0.0f; // stars
		}
		const float Z = FMath::Sqrt(1.0f - R2);
		float Light = FMath::Clamp(-0.6f * X - 0.5f * Y + 0.62f * Z, 0.0f, 1.0f);
		Light *= 0.75f + 0.25f * FMath::PerlinNoise2D(FVector2D(X * 4.0f + Time * 0.05f, Y * 4.0f));
		if (Y < -0.8f) { Light = FMath::Max(Light, 0.9f * Z); }            // polar cap
		if (FMath::Abs(Y - 0.1f + 0.15f * X) < 0.04f && X > -0.6f && X < 0.5f) { Light *= 0.35f; } // canyon
		const float Crater = FMath::Abs(FMath::PerlinNoise2D(FVector2D(X * 9.0f, Y * 9.0f)));
		if (Crater < 0.04f) { Light *= 0.5f; }
		return Light;
	}
	case 1: // Station schematic: ring of modules on a grid
	{
		const float Grid = (FMath::Frac(U * 16.0f) < 0.06f || FMath::Frac(V * 12.0f) < 0.06f) ? 0.15f : 0.0f;
		const bool bRing = (U > 0.15f && U < 0.85f && V > 0.2f && V < 0.8f) && !(U > 0.27f && U < 0.73f && V > 0.35f && V < 0.65f);
		const bool bLock = U > 0.12f && U < 0.24f && V > 0.8f && V < 0.95f;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 3.0f);
		return FMath::Max(Grid, bRing ? 0.85f : (bLock ? 0.6f + 0.4f * Pulse : 0.0f));
	}
	case 2: // Perchlorate ion ClO4-: tetrahedral Cl with four O
	{
		auto Ball = [&](float Cx, float Cy, float R) { const float D = FMath::Sqrt(FMath::Square(U - Cx) + FMath::Square(V - Cy)); return D < R ? FMath::Sqrt(1.0f - D * D / (R * R)) : 0.0f; };
		float Val = Ball(0.5f, 0.5f, 0.16f);
		const float Spin = Time * 0.6f;
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = Spin + i * PI * 0.5f;
			const float Ox = 0.5f + FMath::Cos(A) * 0.3f, Oy = 0.5f + FMath::Sin(A) * 0.22f * (i % 2 ? 1.0f : -0.6f);
			Val = FMath::Max(Val, Ball(Ox, Oy, 0.09f) * 0.8f);
			// bond
			const FVector2D P(U, V), A0(0.5f, 0.5f), B0(Ox, Oy);
			const FVector2D AB = B0 - A0;
			const float T = FMath::Clamp(FVector2D::DotProduct(P - A0, AB) / AB.SizeSquared(), 0.0f, 1.0f);
			if (FVector2D::Distance(P, A0 + AB * T) < 0.012f) { Val = FMath::Max(Val, 0.5f); }
		}
		return Val;
	}
	default: // Earth crescent + dish uplink beam
	{
		const float X = (U - 0.72f) * 3.0f, Y = (V - 0.3f) * 3.0f;
		const float R2 = X * X + Y * Y;
		float Val = 0.0f;
		if (R2 < 1.0f)
		{
			Val = FMath::Clamp(X * 0.9f + 0.2f, 0.0f, 1.0f) * (0.7f + 0.3f * FMath::PerlinNoise2D(FVector2D(X * 5.0f, Y * 5.0f)));
		}
		// Dish silhouette bottom-left
		const float Dx = U - 0.25f, Dy = V - 0.78f;
		if (Dy > 0.0f && Dy < 0.12f && FMath::Abs(Dx) < 0.18f - Dy * 0.6f && Dy > Dx * Dx * 3.0f) { Val = 0.9f; }
		// Dashed beam
		const float BeamT = (U - 0.25f) / 0.47f;
		if (BeamT > 0.0f && BeamT < 1.0f && FMath::Abs(V - FMath::Lerp(0.72f, 0.36f, BeamT)) < 0.008f && FMath::Frac(BeamT * 12.0f - Time * 2.0f) < 0.5f) { Val = 1.0f; }
		return Val;
	}
	}
}

void UIntroBriefingWidget::DrawHalftone(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size, int32 Image) const
{
	MarsUI::Frame(Out, Layer, Geo, Pos - FVector2D(6, 6), Size + FVector2D(12, 12), MarsUI::AmberDim, 1.0f);
	const int32 Cols = 72;
	const float Cell = Size.X / Cols;
	const int32 Rows = FMath::FloorToInt(Size.Y / Cell);
	// Ordered-dither threshold matrix gives the halftone its newsprint texture.
	static const float Bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
	const float Flicker = 0.92f + 0.08f * FMath::Sin(TotalTime * 37.0f);
	for (int32 y = 0; y < Rows; ++y)
	{
		const float Scan = (y % 2 == 0) ? 1.0f : 0.78f;
		for (int32 x = 0; x < Cols; ++x)
		{
			float Val = ImageValue(Image, (x + 0.5f) / Cols, (y + 0.5f) / Rows, TotalTime);
			if (Val < 0.04f)
			{
				continue; // keep black areas clean
			}
			Val = FMath::Clamp(Val + ((Bayer[y % 4][x % 4] + 0.5f) / 16.0f - 0.5f) * 0.35f, 0.0f, 1.0f);
			if (Val < 0.08f)
			{
				continue;
			}
			const float Dot = Cell * FMath::Sqrt(Val) * 0.95f;
			const FVector2D C = Pos + FVector2D((x + 0.5f) * Cell, (y + 0.5f) * Cell);
			MarsUI::Box(Out, Layer + 1, Geo, C - FVector2D(Dot * 0.5f, Dot * 0.5f), FVector2D(Dot, Dot), MarsUI::Amber * (Flicker * Scan));
		}
	}
}

int32 UIntroBriefingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = Geo.GetLocalSize();
	MarsUI::Box(Out, LayerId, Geo, FVector2D::ZeroVector, Size, FLinearColor::Black);
	if (!Pages.IsValidIndex(PageIndex))
	{
		return LayerId + 1;
	}
	const FPage& Page = Pages[PageIndex];
	const float Margin = Size.X * 0.06f;

	const FSlateFontInfo Small = MarsUI::Mono(11, 160);
	const FSlateFontInfo Head = MarsUI::Mono(24, 260);
	const FSlateFontInfo Body = MarsUI::Mono(14, 70);

	MarsUI::Text(Out, LayerId + 2, Geo, TEXT("ARES-IV FORWARD OUTPOST  //  ARCHIVAL BRIEFING  //  RECORD 0142-B"), FVector2D(Margin, Margin * 0.6f), Small, MarsUI::AmberDim);
	MarsUI::Text(Out, LayerId + 2, Geo, FString::Printf(TEXT("%d / %d"), PageIndex + 1, Pages.Num()), FVector2D(Size.X - Margin - 60.0f, Margin * 0.6f), Small, MarsUI::AmberDim);
	MarsUI::Box(Out, LayerId + 2, Geo, FVector2D(Margin, Margin * 0.6f + 22.0f), FVector2D(Size.X - Margin * 2.0f, 1.0f), MarsUI::AmberDim);

	// Archival image on the left.
	const FVector2D ImagePos(Margin, Size.Y * 0.2f);
	const FVector2D ImageSize(Size.X * 0.36f, Size.X * 0.36f * 0.75f);
	DrawHalftone(Out, LayerId + 2, Geo, ImagePos, ImageSize, Page.Image);
	MarsUI::Text(Out, LayerId + 4, Geo, FString::Printf(TEXT("FIG. %d  -  ARCHIVE PLATE / DITHERED TRANSMISSION"), PageIndex + 1),
		ImagePos + FVector2D(0, ImageSize.Y + 14.0f), Small, MarsUI::AmberDim);

	// Typewriter text on the right.
	const float TextX = ImagePos.X + ImageSize.X + Margin * 0.8f;
	float Y = Size.Y * 0.2f;
	MarsUI::Text(Out, LayerId + 4, Geo, Page.Heading, FVector2D(TextX, Y), Head, MarsUI::Amber);
	Y += 52.0f;

	const int32 Visible = FMath::Clamp(FMath::FloorToInt(PageTime * CharsPerSecond), 0, Page.Body.Len());
	TArray<FString> Lines;
	Page.Body.Left(Visible).ParseIntoArray(Lines, TEXT("\n"), false);
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		FString Line = Lines[i];
		if (i == Lines.Num() - 1 && Visible < Page.Body.Len() && FMath::Frac(TotalTime * 3.0f) < 0.5f)
		{
			Line += TEXT("_");
		}
		MarsUI::Text(Out, LayerId + 4, Geo, Line, FVector2D(TextX, Y), Body, MarsUI::Amber);
		Y += 24.0f;
	}

	// Continue prompt.
	if (Visible >= Page.Body.Len() && FMath::Frac(TotalTime * 1.2f) < 0.7f)
	{
		const bool bLast = PageIndex == Pages.Num() - 1;
		MarsUI::TextCentered(Out, LayerId + 4, Geo, bLast ? TEXT("[ PRESS SPACE / CLICK TO INITIALIZE SUIT TELEMETRY ]") : TEXT("[ PRESS SPACE / CLICK TO CONTINUE ]"),
			FVector2D(Size.X * 0.5f, Size.Y - Margin * 1.2f), MarsUI::Mono(13, 200), bLast ? MarsUI::Phosphor : MarsUI::Amber);
	}

	// CRT scanlines over everything.
	for (float SY = 0.0f; SY < Size.Y; SY += 3.0f)
	{
		MarsUI::Box(Out, LayerId + 6, Geo, FVector2D(0, SY), FVector2D(Size.X, 1.0f), FLinearColor(0, 0, 0, 0.28f));
	}
	return LayerId + 7;
}
