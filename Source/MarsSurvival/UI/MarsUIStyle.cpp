#include "UI/MarsUIStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace MarsUI
{
	FSlateFontInfo Mono(int32 Size, int32 LetterSpacing)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Mono", Size);
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	FVector2D Measure(const FString& InText, const FSlateFontInfo& Font)
	{
		if (FSlateApplication::IsInitialized())
		{
			const TSharedRef<FSlateFontMeasure> Measurer = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			return Measurer->Measure(InText, Font);
		}
		return FVector2D(InText.Len() * Font.Size * 0.62f, Font.Size * 1.3f);
	}

	static FPaintGeometry At(const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size)
	{
		return Geo.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Pos)));
	}

	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& InText, const FVector2D& Pos,
		const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow)
	{
		const FVector2D Size = Measure(InText, Font);
		if (bShadow)
		{
			FSlateDrawElement::MakeText(Out, Layer, At(Geo, Pos + FVector2D(1.5f, 1.5f), Size), InText, Font, ESlateDrawEffect::None, Ink);
		}
		FSlateDrawElement::MakeText(Out, Layer + 1, At(Geo, Pos, Size), InText, Font, ESlateDrawEffect::None, Color);
	}

	void TextCentered(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FString& InText, const FVector2D& CenterTop,
		const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow)
	{
		const FVector2D Size = Measure(InText, Font);
		Text(Out, Layer, Geo, InText, FVector2D(CenterTop.X - Size.X * 0.5f, CenterTop.Y), Font, Color, bShadow);
	}

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(Out, Layer, At(Geo, Pos, Size), FCoreStyle::Get().GetBrush("GenericWhiteBox"), ESlateDrawEffect::None, Color);
	}

	void Frame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color, float Th)
	{
		Box(Out, Layer, Geo, Pos, FVector2D(Size.X, Th), Color);
		Box(Out, Layer, Geo, Pos + FVector2D(0, Size.Y - Th), FVector2D(Size.X, Th), Color);
		Box(Out, Layer, Geo, Pos, FVector2D(Th, Size.Y), Color);
		Box(Out, Layer, Geo, Pos + FVector2D(Size.X - Th, 0), FVector2D(Th, Size.Y), Color);
	}
}
