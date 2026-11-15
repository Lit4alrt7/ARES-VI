#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HallucinationPhantom.generated.h"

class UStaticMeshComponent;

/**
 * A cognitive phantom: the crew member's brain projecting an uncanny biological shadow creature
 * (Stage 2+). Built procedurally (hunched torso, elongated swaying arms, digitigrade legs, glowing eyes).
 *
 * Behaviour: stalks -> freezes and stares back when looked at -> lunges with a shriek -> vanishes.
 * It has NO collision. A bullet that crosses its silhouette triggers the "ballistic awakening":
 * the gunshot shocks the nervous system, the creature collapses and dissolves into black ash/static
 * in under 1.5 s, clarity returns briefly - and the round continues into the bulkhead behind it.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AHallucinationPhantom : public AActor
{
	GENERATED_BODY()

public:
	AHallucinationPhantom();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Phantom")
	void InitPhantom(APawn* InViewer, float InIntensity);

	/** True if the segment passes through the creature's silhouette (capsule test, no physics). */
	bool TestSegment(const FVector& Start, const FVector& End) const;

	/** The bullet awakening: collapse, dissolve to ash, restore clarity. */
	UFUNCTION(BlueprintCallable, Category = "Phantom")
	void Awaken();

	/** Corner / doorway peeker: half-hidden behind a jamb, vanishes the moment it is looked at or lit. */
	void InitPeeker(APawn* InViewer, float InIntensity, const FVector& LeanDirection);

	UFUNCTION(BlueprintPure, Category = "Phantom")
	bool IsDissolving() const { return State == EState::Collapsing; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phantom")
	float StalkSpeed = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phantom")
	float LungeSpeed = 950.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phantom")
	float StareBeforeLungeSeconds = 2.4f;

private:
	enum class EState : uint8 { Stalking, Staring, Lunging, Vanishing, Collapsing, Peeking };

	void BuildCreature();
	void SpawnAsh();
	void SetGlow(float Eyes, float Body);
	bool IsViewerLookingAtMe(float& OutDistance) const;

	UPROPERTY(Transient) TObjectPtr<USceneComponent> BodyPivot;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> HeadPivot;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> ArmL;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> ArmR;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> EyeParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> AshParts;
	TArray<FVector> AshVelocity;

	TWeakObjectPtr<APawn> Viewer;
	EState State = EState::Stalking;
	float StateTime = 0.0f;
	float Age = 0.0f;
	float Lifetime = 10.0f;
	float Intensity = 0.6f;
	FVector LeanDir = FVector::ZeroVector;
	FVector PeekBase = FVector::ZeroVector;
	bool IsViewerLookingDirectly(float CosAngle) const;
	FVector StalkDir = FVector::ForwardVector;
};
