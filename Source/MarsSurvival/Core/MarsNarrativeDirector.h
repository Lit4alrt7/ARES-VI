#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MarsNarrativeDirector.generated.h"

UENUM()
enum class EMarsChapter : uint8
{
	Stabilization, // Sol 1-2: learn the base, calibrate the suit, a strange ping from orbit
	Survey,        // Sol 3-4: fly the recon shuttle to the survey sites, recover components and logs
	Uplink,        // Sol 5+:  storms, symptoms, assemble the uplink, tell Earth the truth
	Epilogue
};

/** One linear story objective. Completed by a reported event or by a polled world condition. */
struct FMarsStoryStep
{
	EMarsChapter Chapter = EMarsChapter::Stabilization;
	FString Text;
	FName Event;                                   // completes when Report(Event) arrives
	TFunction<bool(UWorld*)> Poll;                 // ...or when this returns true
	UClass* TargetClass = nullptr;                 // wayfinding target: first actor of this class
	FName TargetTag;                               // ...optionally with this actor tag
	FString AriaOnComplete;                        // A.R.I.A. line when the step completes
};

/**
 * Story / progression director (Subnautica-style: calm start, the world opens up, pressure builds).
 * Tracks the current objective and chapter, drives symptom escalation (NarrativeEscalation on the
 * player's biometrics), storm frequency, and provides the wayfinding target for HUD / floor markers.
 * Gameplay code reports beats with UMarsNarrativeDirector::Report(this, TEXT("Event")).
 */
UCLASS()
class MARSSURVIVAL_API UMarsNarrativeDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMarsNarrativeDirector* Get(const UObject* WorldContext);
	static void Report(const UObject* WorldContext, FName Event);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMarsNarrativeDirector, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bStarted; }

	void StartStory();
	bool IsStarted() const { return bStarted; }

	int32 GetStepIndex() const { return StepIndex; }
	void SetStepIndex(int32 Index); // save / load
	int32 GetStepCount() const { return Steps.Num(); }

	EMarsChapter GetChapter() const;
	FString GetChapterTitle() const;
	FString GetObjectiveText() const;
	AActor* GetObjectiveTarget() const { return CachedTarget.Get(); }
	float GetEscalation() const { return Escalation; }
	float GetObjectiveFlash() const { return ObjectiveFlash; }

	bool AreWaypointsEnabled() const { return bWaypoints; }
	void ToggleWaypoints() { bWaypoints = !bWaypoints; }

	/** Data logs recovered so far (title + text), for the pause menu archive. */
	const TArray<FString>& GetLogTitles() const { return LogTitles; }
	void AddLog(const FString& Title) { LogTitles.AddUnique(Title); }
	void SetLogs(const TArray<FString>& Titles) { LogTitles = Titles; }

private:
	void BuildSteps();
	void Advance();
	void RefreshTarget();

	TArray<FMarsStoryStep> Steps;
	int32 StepIndex = 0;
	bool bStarted = false;
	bool bWaypoints = true;
	float Escalation = 0.0f;
	float ObjectiveFlash = 0.0f;
	float TargetRefresh = 0.0f;
	TWeakObjectPtr<AActor> CachedTarget;
	TArray<FString> LogTitles;
};
