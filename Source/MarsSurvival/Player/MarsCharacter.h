#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Items/MarsItemTypes.h"
#include "Player/PlayerBiometricsComponent.h"
#include "MarsCharacter.generated.h"

class UCameraComponent;
class UPlayerBiometricsComponent;
class UEVASuitComponent;
class UMarsInventoryComponent;
class UHallucinationDirectorComponent;
class URetroPostProcessComponent;
class UMarsAudioManager;
class UMarsProceduralSynth;
class UAriaVoiceComponent;
class AMarsEquipment;
class AMarsBaseModule;
class ASurfaceBeacon;

USTRUCT(BlueprintType)
struct FMarsNotification
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	float RemainingSeconds = 0.0f;
};

/** Result of using an item from the inventory screen. */
enum class EMarsInventoryUse : uint8 { Failed, Consumed, Equipped, Held };

/** One engineer-belt quick slot. */
USTRUCT(BlueprintType)
struct FMarsBeltSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Belt")
	EMarsItemType ItemType = EMarsItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Belt")
	FString ShortName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Belt")
	bool bShowCount = false;
};

/**
 * Dr. David Miller, bio-engineer. First-person, Mars gravity (0.38 g).
 * Each frame decides what gas is being breathed (module / sealed MCP helmet / raw Martian air)
 * and feeds it to the biometrics component.
 *
 * Controls: WASD move, Shift sprint, E interact (hold for hold-actions), LMB/RMB use, R reload,
 * 1-4 tools, 5 drink, 6 eat, H med-kit (tap = select, hold = inject), Q O2 canister, B beacon, T A.R.I.A.
 */
UCLASS()
class MARSSURVIVAL_API AMarsCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMarsCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	// ---- Feel: recoil inertia, item inspection ----
	/** Weapon kick: snaps the view up now and recovers part of it over the next ~0.3 s. Also kicks the viewmodel back. */
	void AddRecoil(float PitchDegrees, float YawDegrees);
	/** Show a newly picked-up item in front of the camera; mouse rotates it, E / click stores it. */
	void BeginInspect(EMarsItemType ItemType, bool bForce = false);
	void EndInspect();
	bool IsInspecting() const { return InspectActor != nullptr; }
	bool IsLampOn() const;

	// ---- Inventory screen ----
	/** Equip / consume / take in hand (anything not usable is held up to look at). */
	EMarsInventoryUse UseInventoryItem(EMarsItemType ItemType);
	FString GetInventoryUseLabel(EMarsItemType ItemType) const;
	bool DropInventoryItem(EMarsItemType ItemType);
	void AssignBeltSlot(int32 Index, EMarsItemType ItemType);

	// ---- Sleep ----
	class UPlayerFatigueComponent* GetFatigue() const { return Fatigue; }
	void BeginSleep();
	/** Fade out, skip to TargetHour (local), fade in. bSleep = real sleep (fatigue reset) vs. waiting. */
	void BeginRest(float TargetHour, bool bSleep);
	bool IsSleeping() const { return SleepTime >= 0.0f; }
	/** 0..1 black-out of the screen while sleeping. */
	float GetSleepFade() const;
	class UMarsSuitViewmodel* GetSuitViewmodel() const { return SuitViewmodel; }
	EMarsItemType GetInspectItem() const { return InspectItem; }
	/** First time this item type is taken this run (save-game tracks it). */
	bool HasInspected(EMarsItemType ItemType) const { return InspectedTypes.Contains(ItemType); }
	TArray<EMarsItemType> GetInspectedTypes() const { return InspectedTypes.Array(); }
	void SetInspectedTypes(const TArray<EMarsItemType>& Types) { InspectedTypes = TSet<EMarsItemType>(Types); }
	/** Save/load. */
	void SetDeadForLoad(bool bDead) { bIsDead = bDead; }
	UPlayerBiometricsComponent* GetBiometrics() const { return Biometrics; }
	UEVASuitComponent* GetSuit() const { return Suit; }
	UMarsInventoryComponent* GetInventory() const { return Inventory; }
	UHallucinationDirectorComponent* GetHallucinationDirector() const { return HallucinationDirector; }
	UMarsAudioManager* GetAudioManager() const { return AudioManager; }
	UAriaVoiceComponent* GetAriaVoice() const { return AriaVoice; }

	/** Visual (icon / body-diagram) diagnostic panel from the Med-Bay scanner. */
	UFUNCTION(BlueprintCallable, Category = "Mars Character")
	void ShowDiagnostic(float DurationSeconds = 18.0f) { DiagnosticRemaining = DurationSeconds; PanelRemaining = 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	bool IsDiagnosticVisible() const { return DiagnosticRemaining > 0.0f; }

	/** Begin the game sealed in the MCP suit (the opening takes place in the airlock). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Character")
	bool bStartInSuit = true;

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	AMarsBaseModule* GetCurrentModule() const { return CurrentModule.Get(); }

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	AMarsEquipment* GetEquippedItem() const { return EquippedItem; }

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	FText GetFocusPrompt() const { return FocusPrompt; }

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	TArray<FMarsNotification> GetNotifications() const { return Notifications; }

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	TArray<ASurfaceBeacon*> GetKnownBeacons() const;

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	TArray<FMarsBeltSlot> GetBeltSlots() const { return BeltSlots; }

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	FString GetSelectedMedicationLabel() const;

	UFUNCTION(BlueprintCallable, Category = "Mars Character")
	void ShowNotification(const FText& Text, float DurationSeconds = 3.0f);

	UFUNCTION(BlueprintCallable, Category = "Mars Character")
	void ShowPanel(const FString& Text, float DurationSeconds = 12.0f);

	UFUNCTION(BlueprintPure, Category = "Mars Character")
	FString GetPanelText() const { return PanelRemaining > 0.0f ? PanelText : FString(); }

	UFUNCTION(BlueprintCallable, Category = "Mars Character")
	void EquipItem(EMarsItemType ItemType);

	UFUNCTION(BlueprintCallable, Category = "Mars Character")
	void Holster();

	UFUNCTION(BlueprintCallable, Category = "Mars Character")
	bool UseConsumable(EMarsItemType ItemType);

	UFUNCTION(BlueprintCallable, Category = "Mars Character")
	void RegisterBeacon(ASurfaceBeacon* Beacon);

	// ---- Dev / demo console commands (` console) ----

	/** MarsEquip 1..6 - use a belt slot. */
	UFUNCTION(Exec)
	void MarsEquip(int32 Slot);

	/** MarsSetStage 0..4 - jump the disease to the middle of a stage (demo the hallucinations). */
	UFUNCTION(Exec)
	void MarsSetStage(int32 Stage);

	/** MarsTurn <yaw degrees> - set view yaw. */
	UFUNCTION(Exec)
	void MarsTurn(float Yaw);

	/** MarsSpawnPhantom - project a cognitive phantom 7 m ahead (demo the ballistic awakening). */
	UFUNCTION(Exec)
	void MarsSpawnPhantom();

	/** MarsShowScan - open the visual Med-Bay diagnostic. */
	UFUNCTION(Exec)
	void MarsShowScan() { ShowDiagnostic(30.0f); }

	UFUNCTION(BlueprintCallable, Category = "Mars Character")
	void AddHazardDoseRate(float DeltaMsvPerHour) { HazardDoseRateMsvPerHour = FMath::Max(HazardDoseRateMsvPerHour + DeltaMsvPerHour, 0.0f); }

	// ---- Components ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UMarsSuitViewmodel> SuitViewmodel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UPlayerFatigueComponent> Fatigue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USpotLightComponent> HelmetLamp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPlayerBiometricsComponent> Biometrics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UEVASuitComponent> Suit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMarsInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHallucinationDirectorComponent> HallucinationDirector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<URetroPostProcessComponent> RetroPostProcess;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMarsAudioManager> AudioManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMarsProceduralSynth> Synth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAriaVoiceComponent> AriaVoice;

	// ---- Tuning ----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mars Character")
	TMap<EMarsItemType, TSubclassOf<AMarsEquipment>> EquipmentClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mars Character")
	TArray<FMarsBeltSlot> BeltSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Character")
	float InteractionRangeCm = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Character")
	float BaseWalkSpeed = 330.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Character")
	float SuitedWalkSpeed = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Character")
	float SprintMultiplier = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Character")
	float O2CanisterLiters = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Character")
	TSubclassOf<ASurfaceBeacon> BeaconClass;

protected:
	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void OnInteractPressed();
	void OnInteractReleased();
	void OnPrimaryPressed();
	void OnPrimaryReleased();
	void OnSecondaryPressed();
	void OnReloadPressed();
	void OnSprintPressed() { bWantsSprint = true; }
	void OnSprintReleased() { bWantsSprint = false; }
	void OnSlot1() { UseBeltSlot(0); }
	void OnSlot2() { UseBeltSlot(1); }
	void OnSlot3() { UseBeltSlot(2); }
	void OnSlot4() { UseBeltSlot(3); }
	void OnSlot5() { UseBeltSlot(4); }
	void OnSlot6() { UseBeltSlot(5); }
	void OnMedPressed();
	void OnMedReleased();
	void OnUseO2();
	void OnDeployBeacon();
	void OnTalkToAria();
	void OnCrouchToggle();
	void OnTurn(float Value);
	void OnLookUp(float Value);
	void OnToggleHUD();
	void OnWristDisplay();
	void OnToggleWaypoints();
	void OnToggleLamp();
	void OnInventory();
	void OnPause();

	UFUNCTION()
	void HandleDeath(FName Cause);

	UFUNCTION()
	void HandleStageChanged(EDiseaseStage OldStage, EDiseaseStage NewStage);

private:
	void UseBeltSlot(int32 Index);
	void UpdateCurrentModule();
	void UpdateBreathingEnvironment(float DeltaTime);
	void UpdateInteractionFocus();
	void UpdateMovement(float DeltaTime);
	void UpdateTremor(float DeltaTime);
	void UpdateNotifications(float DeltaTime);
	void UpdateViewFeel(float DeltaTime);
	void GrantStartingLoadout();
	TArray<EMarsItemType> GetCarriedMedications() const;

	TWeakObjectPtr<AMarsBaseModule> CurrentModule;
	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusPrompt;
	bool bInteractHeld = false;
	bool bIsDead = false;
	bool bWantsSprint = false;
	float HazardDoseRateMsvPerHour = 0.0f;
	float StepDistance = 0.0f;
	float TremorTime = 0.0f;
	FVector2D LastTremor = FVector2D::ZeroVector;
	FVector2D SmoothedLook = FVector2D::ZeroVector;
	FVector2D LookThisFrame = FVector2D::ZeroVector;
	FVector2D RecoilDebt = FVector2D::ZeroVector;
	FVector SwayLocation = FVector::ZeroVector;
	FRotator SwayRotation = FRotator::ZeroRotator;
	float ViewKick = 0.0f;
	float BobTime = 0.0f;
	float BobBlend = 0.0f;
	float EyeHeight = 68.0f;
	float ColdExposureSeconds = 0.0f;
	float SleepTime = -1.0f;
	bool bSleepApplied = false;
	float RestTargetHour = 7.0f;
	bool bRestIsSleep = true;
	bool bWasOutside = false;
	void UpdateSleep(float DeltaTime);
	float BreathTime = 0.0f;
	EMarsItemType InspectItem = EMarsItemType::None;
	TSet<EMarsItemType> InspectedTypes;

	UPROPERTY(Transient)
	TObjectPtr<AActor> InspectActor;
	double MedPressTime = 0.0;
	EMarsItemType SelectedMedication = EMarsItemType::MethyleneBlueCartridge;

	UPROPERTY(Transient)
	TObjectPtr<AMarsEquipment> EquippedItem;

	UPROPERTY(Transient)
	TMap<EMarsItemType, TObjectPtr<AMarsEquipment>> SpawnedEquipment;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ASurfaceBeacon>> KnownBeacons;

	TArray<FMarsNotification> Notifications;
	FString PanelText;
	float PanelRemaining = 0.0f;
	float DiagnosticRemaining = 0.0f;
};
