#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/MarsSettingsPanel.h"
#include "MarsPauseMenuWidget.generated.h"

UENUM()
enum class EMarsPauseAction : uint8
{
	Resume,
	LoadLastSave,
	Restart,
	QuitToMenu,
	QuitToDesktop
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMarsPauseAction, EMarsPauseAction);

/**
 * ESC menu: mission status (comm window, transistors, current objective, location, suit O2, module hull),
 * resume / load last save / settings & controls / restart / quit. Also shown, without RESUME, when the
 * mission ends (death or success). Destructive actions need a second confirm press.
 */
UCLASS()
class MARSSURVIVAL_API UMarsPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(bool bInMissionOver);

	FOnMarsPauseAction OnAction;

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
	enum class EItem : uint8 { Resume, Load, Settings, Restart, QuitMenu, QuitDesktop };

	void Activate(int32 Index);
	void Beep(float Pitch) const;
	int32 ItemAt(const FGeometry& Geo, const FVector2D& ScreenPos) const;
	FVector2D ItemPos(const FVector2D& Size, int32 Index) const;
	static bool NeedsConfirm(EItem Item);
	void PaintStatus(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& PanelSize) const;

	TArray<EItem> Items;
	int32 Selected = 0;
	int32 ArmedIndex = INDEX_NONE;
	bool bMissionOver = false;
	float Time = 0.0f;
	FString SaveInfo;
	FMarsSettingsPanel Settings;
};
