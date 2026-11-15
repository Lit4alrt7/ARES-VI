#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class FSlateWindowElementList;
struct FGeometry;

/**
 * Retro-aerospace UI kit: monospaced terminal typography (Slate "Mono" face = Roboto Mono / DroidSansMono)
 * with wide letter-spacing and drop shadows, in the spirit of Alien's MU/TH/UR and the Apollo DSKY.
 */
namespace MarsUI
{
	const FLinearColor Amber(1.0f, 0.66f, 0.18f);
	const FLinearColor AmberDim(0.55f, 0.34f, 0.08f);
	const FLinearColor Phosphor(0.42f, 1.0f, 0.62f);
	const FLinearColor Cyan(0.45f, 0.85f, 1.0f);
	const FLinearColor Alarm(1.0f, 0.18f, 0.12f);
	const FLinearColor Ink(0.0f, 0.0f, 0.0f, 0.78f);

	MARSSURVIVAL_API FSlateFontInfo Mono(int32 Size, int32 LetterSpacing = 80);

	MARSSURVIVAL_API FVector2D Measure(const FString& Text, const FSlateFontInfo& Font);

	MARSSURVIVAL_API void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, const FVector2D& Pos,
		const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = true);

	MARSSURVIVAL_API void TextCentered(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& Text, const FVector2D& CenterTop,
		const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = true);

	MARSSURVIVAL_API void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color);

	/** 1-px outline rectangle. */
	MARSSURVIVAL_API void Frame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color, float Thickness = 1.0f);
}
