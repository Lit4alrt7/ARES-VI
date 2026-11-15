#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MarsInteractable.h"
#include "Items/MarsItemTypes.h"
#include "Props/MarsPropBuilder.h"
#include "MarsStations.generated.h"

class AMarsBaseModule;
class UStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;

/**
 * Base for every interactive console / valve / machine (spec name: AMarsInteractableStation).
 * - Procedural PS1 prop model (PropKind) + invisible collision volume for interaction traces
 * - Live diegetic screen (UTextRenderComponent, mono stencil look) refreshed 4x per second;
 *   text glitches when the crew member is at disease Stage 3+
 * - Binds to the module that contains it if TargetModule is not set
 */
UCLASS(Abstract, Blueprintable)
class MARSSURVIVAL_API AMarsStationBase : public AActor, public IMarsInteractable
{
	GENERATED_BODY()

public:
	AMarsStationBase();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Invisible interaction / collision volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Console;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> Screen;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> ScreenGlow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	TObjectPtr<AMarsBaseModule> TargetModule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FString StationName = TEXT("STATION");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	EMarsPropKind PropKind = EMarsPropKind::Terminal;

	/** Collision volume size (cm), sitting on the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FVector ConsoleSize = FVector(60.0f, 70.0f, 150.0f);

	/** Screen text anchor (local, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	FVector ScreenOffset = FVector(18.0f, 0.0f, 132.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	float ScreenTextSize = 4.0f;

	/** Garble text according to the player's neuro-toxic state (used by HUD panels too). */
	static FString GlitchText(const FString& In, float Amount);

	/** 0..1 glitch amount from the player's disease stage / hallucination. */
	static float GetPlayerGlitchAmount(const UObject* WorldContext);

protected:
	virtual void BeginPlay() override;

	/** Live screen contents. */
	virtual FString GetScreenText() const { return StationName; }

	/** 0 = nominal (green), 1 = caution (amber), 2 = critical (flashing red). */
	virtual int32 GetScreenStatus() const { return 0; }

	AMarsBaseModule* ResolveModule() const;

	template <typename T>
	T* GetModuleAs() const { return Cast<T>(ResolveModule()); }

	void Beep(AMarsCharacter* C, float Pitch = 1.0f) const;
};

// ---------------------------------------------------------------- SEC-01 Airlock

/** Airlock cycle control. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AAirlockPanelStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AAirlockPanelStation();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;
};

/** Electrostatic dust scrubber: hold E to strip perchlorate dust from the suit and the chamber. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ADustScrubberStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	ADustScrubberStation();
	virtual void InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
};

// ---------------------------------------------------------------- SEC-02 EVA Prep

/** MCP suit rack. E = don / doff. Hold E = refill O2 / LiOH / battery. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ASuitRackStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	ASuitRackStation();
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual void InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suit Rack")
	float StationO2ReserveLiters = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suit Rack")
	float RefillLitersPerSecond = 120.0f;
protected:
	virtual FString GetScreenText() const override;
private:
	float HoldTime = 0.0f;
};

/** O2 refill manifold: E dispenses a filled O2 canister. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AO2RefillStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AO2RefillStation();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "O2")
	int32 CanistersAvailable = 4;
protected:
	virtual FString GetScreenText() const override;
};

/** Generic wall panel: module gauges, grid breaker, thermal control, life-support overview. */
UENUM(BlueprintType)
enum class EStatusPanelMode : uint8
{
	ModuleGauges,
	GridBreaker,
	ThermalControl,
	LifeSupport
};

UCLASS(Blueprintable)
class MARSSURVIVAL_API AModuleStatusPanel : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AModuleStatusPanel();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	EStatusPanelMode Mode = EStatusPanelMode::ModuleGauges;
protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;
};

// ---------------------------------------------------------------- SEC-03 Bio-Dome

/** CO2 scrubber console: E toggles grow lights; hold E (2 s) runs an emergency LiOH purge. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ACo2ScrubberStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	ACo2ScrubberStation();
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual void InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;
private:
	float HoldTime = 0.0f;
	float PurgeCooldown = 0.0f;
};

/** Water recovery dispenser (photobioreactor condensate). */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AWaterDispenserStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AWaterDispenserStation();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	float ReserveLiters = 40.0f;
protected:
	virtual FString GetScreenText() const override;
};

/** Chlorella harvester: E harvests edible algal paste (costs culture health). */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AChlorellaHarvesterStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AChlorellaHarvesterStation();
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
private:
	float Cooldown = 0.0f;
};

// ---------------------------------------------------------------- SEC-04 Xenobiology

/** Optical / mass spectrometer: assays bio-agent cultures (unlocks drugs) and rock samples. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AXenoAnalyzerStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AXenoAnalyzerStation();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
private:
	int32 FindingsIndex = 0;
};

/** Electron microscope: examine a smear of your own blood. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMicroscopeStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AMicroscopeStation();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
};

/** BSL-3 sample containment box: extract a bio-agent culture (exposure risk without a suit). */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AContainmentBoxStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AContainmentBoxStation();
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
private:
	float Cooldown = 0.0f;
};

// ---------------------------------------------------------------- SEC-05 Med-Bay

/** Biometric diagnostic scanner bed. E = full scan + staging. Hold E (4 s) = emergency stabilisation. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ADiagnosticScannerBed : public AMarsStationBase
{
	GENERATED_BODY()
public:
	ADiagnosticScannerBed();
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual void InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;
private:
	float HoldTime = 0.0f;
	float IdleTime = 0.0f;
};

/** Pharmaceutical bioprinter. E = select recipe, hold E (1 s) = print / collect. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ABioprinterStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	ABioprinterStation();
	virtual void Tick(float DeltaTime) override;
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual void InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
private:
	int32 SelectedRecipe = 0;
	float HoldTime = 0.0f;
	float IdleTime = 0.0f;
};

// ---------------------------------------------------------------- SEC-06 Storage & Comm-Relay

/** RTG power core readout. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ARtgCoreStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	ARtgCoreStation();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;
};

/** Quantum transistor sockets / transmitter: install, repair, transmit. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ACommRelayTerminal : public AMarsStationBase
{
	GENERATED_BODY()
public:
	ACommRelayTerminal();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;
};

/** Orbital dish alignment terminal: hold E to calibrate the optical link. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ADishAlignmentTerminal : public AMarsStationBase
{
	GENERATED_BODY()
public:
	ADishAlignmentTerminal();
	virtual void InteractHold_Implementation(AMarsCharacter* C, float DeltaSeconds) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;
protected:
	virtual FString GetScreenText() const override;
	virtual int32 GetScreenStatus() const override;
};

/** Food locker: E eats a ration if hungry, otherwise stows one. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AFoodLockerStation : public AMarsStationBase
{
	GENERATED_BODY()
public:
	AFoodLockerStation();
	virtual void Interact_Implementation(AMarsCharacter* C) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* C) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food")
	int32 RationsAvailable = 8;
protected:
	virtual FString GetScreenText() const override;
};
