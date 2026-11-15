#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UI/MarsPauseMenuWidget.h"
#include "MarsGameMode.generated.h"

class UIntroBriefingWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMissionEnded, bool, bVictory, const FString&, Reason);

/**
 * Mission rules. At BeginPlay: pause and show the archival intro briefing; on dismissal, resume
 * and start the 45-minute Earth comm window. Win = quantum uplink; lose = death or window closure.
 */
UCLASS()
class MARSSURVIVAL_API AMarsGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMarsGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Mission")
	void NotifyPlayerDied(FName Cause);

	UFUNCTION(BlueprintPure, Category = "Mission")
	float GetRemainingSeconds() const { return RemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsMissionOver() const { return bMissionOver; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsVictory() const { return bVictory; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	FString GetEndReason() const { return EndReason; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	float CommWindowMinutes = 0.0f; // 0 = no hard time limit (the story is Sol-based)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	bool bShowIntroBriefing = true;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FOnMissionEnded OnMissionEnded;

	/** ESC: open / close the pause menu (ignored during the intro screens). */
	void TogglePauseMenu();
	void ToggleInventory();

	/** Autosave now (sleeping, story beats). */
	void RequestCheckpoint(const FString& Label) { SaveCheckpoint(Label); }
	bool IsPauseMenuOpen() const { return PauseMenu != nullptr; }

	/** Automatic checkpoint interval (real seconds, only while inside the station). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	float CheckpointIntervalSeconds = 240.0f;

protected:
	UFUNCTION()
	void HandleEarthContact();

	void ShowSplash();
	void ShowBriefing();
	void FinishBriefing();
	void StartOpeningSequence();
	void HandleShuttleDeparted();
	void EndMission(bool bInVictory, const FString& Reason);
	void ShowPauseMenu(bool bMissionOverMode);
	void HidePauseMenu();
	void HandlePauseAction(EMarsPauseAction Action);
	void ApplySaveSlot(const FString& Slot);
	void SaveCheckpoint(const FString& Label);
	void ReloadLevel(const FString& Options);

private:
	UPROPERTY(Transient)
	TObjectPtr<UIntroBriefingWidget> Briefing;

	UPROPERTY(Transient)
	TObjectPtr<class USplashIntroWidget> Splash;

	UPROPERTY(Transient)
	TObjectPtr<class AMarsTutorialManager> Tutorial;

	UPROPERTY(Transient)
	TObjectPtr<UMarsPauseMenuWidget> PauseMenu;

	UPROPERTY(Transient)
	TObjectPtr<class UMarsInventoryWidget> InventoryScreen;

	void CloseInventory();

	float CheckpointTimer = 0.0f;
	float EndMenuDelay = -1.0f;
	float MissionElapsed = 0.0f;

	float RemainingSeconds = 0.0f;
	bool bMissionOver = false;
	bool bVictory = false;
	FString EndReason;
};
