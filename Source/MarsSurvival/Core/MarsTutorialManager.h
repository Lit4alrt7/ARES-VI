#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarsTutorialManager.generated.h"

/** One checklist line inside a tutorial step. */
USTRUCT(BlueprintType)
struct FMarsTutorialTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tutorial") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial") FName Event;
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial") bool bDone = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial") bool bOptional = false;
};

/** A tutorial step: icon + title + science note + checklist. */
USTRUCT(BlueprintType)
struct FMarsTutorialStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tutorial") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial") FString Science;
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial") int32 Icon = 0; // 0 suit, 1 radiation, 2 medical, 3 wrench
	UPROPERTY(BlueprintReadOnly, Category = "Tutorial") TArray<FMarsTutorialTask> Tasks;
};

/**
 * Interactive onboarding for the first minutes of play (diegetic "suit training overlay"):
 *   1. Airlock pressurisation -> stow the MCP suit inside the habitat
 *   2. Geiger dosimeter -> understand radiation hotspots
 *   3. Med-Bay scanner -> print and inject methylene blue
 *   4. Torque wrench on cold seals before -80 C nightfall (+ patch breaches)
 * Gameplay code reports events with AMarsTutorialManager::Report(this, "EventName").
 */
UCLASS()
class MARSSURVIVAL_API AMarsTutorialManager : public AActor
{
	GENERATED_BODY()

public:
	AMarsTutorialManager();

	virtual void Tick(float DeltaTime) override;

	/** Report a gameplay event to the active tutorial (safe to call when no tutorial exists). */
	static void Report(const UObject* WorldContext, FName Event);

	static AMarsTutorialManager* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StartTutorial();

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SkipTutorial();

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	int32 GetStepIndex() const { return StepIndex; }

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	int32 GetStepCount() const { return Steps.Num(); }

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	FMarsTutorialStep GetCurrentStep() const { return Steps.IsValidIndex(StepIndex) ? Steps[StepIndex] : FMarsTutorialStep(); }

	/** 0..1 "step complete" flash for the HUD. */
	UFUNCTION(BlueprintPure, Category = "Tutorial")
	float GetCompleteFlash() const { return CompleteFlash; }

private:
	void HandleEvent(FName Event);
	void BuildSteps();
	void AdvanceIfDone();

	TArray<FMarsTutorialStep> Steps;
	int32 StepIndex = 0;
	bool bActive = false;
	float CompleteFlash = 0.0f;
	float DosimeterHeldSeconds = 0.0f;
	float NextStepDelay = 0.0f;
};
