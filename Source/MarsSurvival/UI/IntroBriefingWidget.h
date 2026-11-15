#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IntroBriefingWidget.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnBriefingFinished);

/**
 * Full-screen black archival briefing shown at game start (game paused).
 * Typewriter mono/amber text beside procedural halftone "archival photographs" rendered as
 * dithered dot grids (Mars globe, station schematic, perchlorate ion, Earth uplink), with
 * scanlines and flicker. SPACE / ENTER / click advances; the last page initialises suit telemetry.
 */
UCLASS()
class MARSSURVIVAL_API UIntroBriefingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnBriefingFinished OnFinished;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	struct FPage
	{
		FString Heading;
		FString Body;
		int32 Image = 0;
	};

	void Advance();
	void DrawHalftone(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size, int32 Image) const;
	static float ImageValue(int32 Image, float U, float V, float Time);

	TArray<FPage> Pages;
	int32 PageIndex = 0;
	float PageTime = 0.0f;
	float TotalTime = 0.0f;
	int32 LastBeepChar = 0;
	bool bFinished = false;
};
