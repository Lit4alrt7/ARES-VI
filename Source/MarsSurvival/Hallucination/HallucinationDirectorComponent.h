#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Player/PlayerBiometricsComponent.h"
#include "HallucinationDirectorComponent.generated.h"

class AMarsPhantomActor;

/**
 * Turns the biometric hallucination cascade into perception:
 *  - Stage 2: auditory pareidolia (footsteps in empty corridors, metal scraping behind bulkheads, knocks)
 *  - Stage 3+: collision-less shadow figures darting at the edge of vision (bullets pass through them)
 *  - Vavilov-Cherenkov flashes -> retro post-process + audio zap
 *  - REAL structural groans when seals cold-contract, so the player can never be sure what is real
 * All audio is procedural (UMarsAudioManager). Optional captions for accessibility.
 */
UCLASS(ClassGroup = (MarsSurvival), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UHallucinationDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHallucinationDirectorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Hallucination")
	bool HasRecentPhantom(float WithinSeconds) const;

	/** What the crew member thinks they perceived, in their own words. */
	UFUNCTION(BlueprintPure, Category = "Hallucination")
	FString GetLastPhantomDescription() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hallucination")
	TSubclassOf<AMarsPhantomActor> PhantomClass;

	/** Captions such as "[footsteps behind you]" (accessibility). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hallucination")
	bool bShowCaptions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hallucination")
	float GroanChancePerSecond = 0.06f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandlePhantom(EPhantomType Type, float Intensity);

	UFUNCTION()
	void HandleCherenkovFlash(float Strength);

private:
	void SpawnVisualPhantom(EPhantomType Type, float Intensity);
	void PlayAudioPhantom(EPhantomType Type, float Intensity);
	void UpdateStructuralGroans(float DeltaTime);
	void UpdatePeekers(float DeltaTime);
	bool FindPeekSpot(FVector& OutLocation, FVector& OutLean) const;
	float PeekTimer = 30.0f;
	bool FindPhantomLocation(EPhantomType Type, FVector& OutLocation) const;
	void Caption(const TCHAR* Text);

	EPhantomType LastPhantomType = EPhantomType::Footsteps;
	double LastPhantomTime = -1000.0;
};
