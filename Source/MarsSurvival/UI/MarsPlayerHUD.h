#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MarsPlayerHUD.generated.h"

class AMarsCharacter;

/**
 * Minimalist diegetic HUD. On screen ONLY:
 *   1. Hydration bar   2. Nutrition bar   3. Engineer belt (quick-slots 1-6 + med/transistor counts)
 * plus contextual essentials: crosshair, interaction prompt, terminal read-outs while using a console,
 * A.R.I.A. comm line (fades), and the helmet visor ring (O2 / beacons) only while the MCP helmet is sealed.
 * No health / radiation / blood stats - use the dosimeter and the Med-Bay scanner like a scientist.
 */
UCLASS()
class MARSSURVIVAL_API UMarsPlayerHUD : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void PaintSurvival(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintBelt(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintCenter(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintVisor(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintAria(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintMissionEnd(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintTutorial(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintDiagnostic(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintCritical(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintReadouts(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintInspect(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintObjective(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintClock(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintOrbitOps(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const class AMarsReconShuttle* Shuttle) const;
	void PaintWaypoint(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintSleep(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const AMarsCharacter* C) const;
	void PaintCockpit(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const class AMarsReconShuttle* Shuttle) const;

public:
	/** Minimal UI (toggle key): hides bars, belt and readouts; keeps prompts, alarms and subtitles. */
	void SetMinimal(bool bInMinimal) { bMinimal = bInMinimal; }
	bool IsMinimal() const { return bMinimal; }

private:
	bool bMinimal = false;

public:
	/** Dot-matrix icon (0 suit, 1 radiation, 2 medical, 3 wrench). */
	static void PaintIcon(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, int32 Icon, const FVector2D& Pos, float Size, const FLinearColor& Color);
	/** Segmented status gauge: green / amber / flashing red. */
	static void PaintGauge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, float Width, const FString& Label,
		const FString& Value, float Fill01, int32 Status, float Time);
	static FLinearColor StatusColor(int32 Status, float Time);

	float Time = 0.0f;
};
