#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/MarsSettingsPanel.h"
#include "SplashIntroWidget.generated.h"

class UTexture2D;

DECLARE_MULTICAST_DELEGATE(FOnSplashStartMission);
DECLARE_MULTICAST_DELEGATE(FOnSplashContinueMission);

/**
 * Startup sequence (game paused, music fading in):
 *   1. Retro "Unreal Engine 5" technical boot badge (POST-style log)
 *   2. Team PARADOX logo (Content/UI/ParadoxLogo texture, procedural CRT emblem fallback)
 *   3. PARADOX PRODUCTION TEAM poster card (Content/UI/ParadoxTeamPoster)
 *   4. Main menu: the view from the transfer ship's viewport onto Mars in orbit (procedural, rotating,
 *      PS1-dithered globe rendered on the CPU into a transient texture), retro EQ, settings screen.
 * SPACE / ENTER / click skips a card. Menu: W/S or arrows + ENTER, or mouse. Quitting asks for confirmation.
 */
UCLASS()
class MARSSURVIVAL_API USplashIntroWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnSplashStartMission OnStartMission;
	FOnSplashContinueMission OnContinueMission;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EPhase : uint8 { Boot, StudioLogo, TeamPoster, Menu };
	enum class EMenuItem : uint8 { NewMission, Continue, Settings, Quit };

	void NextPhase();
	void Activate(int32 Index);
	void Beep(float Pitch) const;
	void PaintBoot(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintImageCard(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FSlateBrush& Brush, bool bHasTexture, bool bLogo) const;
	void PaintEmblem(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Center, float Scale, float Alpha) const;
	void PaintMenu(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintOrbitView(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintViewportFrame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo) const;
	void PaintEqualizer(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size) const;
	float CardAlpha() const;
	int32 ItemAt(const FGeometry& Geo, const FVector2D& ScreenPos) const;
	FVector2D ItemPos(const FVector2D& Size, int32 Index) const;

	// ---- Procedural Mars ----
	void BuildMarsAlbedo();
	void RenderGlobe();
	FVector2D GlobeCenter(const FVector2D& Size) const;
	float GlobeRadius(const FVector2D& Size) const;

	EPhase Phase = EPhase::Boot;
	float PhaseTime = 0.0f;
	float TotalTime = 0.0f;
	int32 Selected = 0;
	bool bConfirmQuit = false;
	float Bands[8] = { 0 };
	TArray<EMenuItem> Items;
	FMarsSettingsPanel Settings;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LogoTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PosterTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GlobeTexture;

	FSlateBrush LogoBrush;
	FSlateBrush PosterBrush;
	FSlateBrush GlobeBrush;

	static constexpr int32 GlobeRes = 288;
	static constexpr int32 MapW = 512;
	static constexpr int32 MapH = 256;
	TArray<FLinearColor> Albedo;
	float Spin = 0.0f;
	float GlobeUpdateAccum = 0.0f;
};
