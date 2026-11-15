#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Math/Box2D.h"

class FSlateWindowElementList;
struct FGeometry;
class UWorld;

/**
 * Terminal-style settings screen shared by the main menu and the pause menu.
 * Tabs: CONTROLS (rebind every key), CAMERA, AUDIO, DISPLAY, GAMEPLAY.
 *
 * Keyboard: W/S or arrows = row, A/D or left/right = adjust, Q/E or TAB = tab, ENTER = rebind / toggle,
 * ESC = back. Mouse: click a row (left/right half of a slider = -/+), wheel scrolls.
 */
class MARSSURVIVAL_API FMarsSettingsPanel
{
public:
	void Open();
	void Close() { bOpen = false; bWaitingForKey = false; }
	bool IsOpen() const { return bOpen; }
	bool IsWaitingForKey() const { return bWaitingForKey; }

	void Paint(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Pos, const FVector2D& Size, float Time) const;

	/** Returns true when consumed. ESC (when not rebinding) closes the panel. */
	bool OnKey(const FKey& Key, UWorld* World);
	bool OnMouseDown(const FVector2D& Local, const FKey& Button, UWorld* World);
	void OnMouseMove(const FVector2D& Local);
	void OnMouseWheel(float Delta);

private:
	enum class ETab : uint8 { Controls, Camera, Audio, Display, Gameplay, Count };

	struct FRow
	{
		FString Label;
		FString Value;
		float Slider = -1.0f; // 0..1 when the row is a slider
		bool bAction = false; // a button row (reset, apply)
	};

	TArray<FRow> BuildRows() const;
	void Adjust(int32 RowIndex, int32 Direction, UWorld* World);
	void Activate(int32 RowIndex, UWorld* World);
	void ChangeTab(int32 Direction);
	void ClampScroll(int32 NumRows);
	void Commit(UWorld* World) const;

	bool bOpen = false;
	bool bWaitingForKey = false;
	ETab Tab = ETab::Controls;
	int32 Row = 0;
	int32 Scroll = 0;
	FString Flash;
	double FlashUntil = 0.0;

	// Hit areas recorded during Paint (local widget space).
	mutable TArray<FBox2D> RowRects;
	mutable TArray<int32> RowIndices;
	mutable TArray<FBox2D> TabRects;
	mutable FBox2D BackRect = FBox2D(ForceInit);
	mutable int32 VisibleRows = 12;
};
