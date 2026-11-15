#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/PlayerBiometricsComponent.h"
#include "MarsPhantomActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * A hallucinated figure. It exists only in the protagonist's visual cortex: it has NO collision,
 * so traces and bullets pass straight through it into whatever is behind (usually the hull).
 * It dissolves when stared at directly, approached, or after its lifetime.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsPhantomActor : public AActor
{
	GENERATED_BODY()

public:
	AMarsPhantomActor();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Phantom")
	void InitPhantom(APawn* InViewer, EPhantomType InType, float InIntensity, float InLifetimeSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phantom")
	TSoftObjectPtr<UMaterialInterface> PhantomMaterial;

	/** Seconds of direct gaze before it vanishes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phantom")
	float StareToVanishSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phantom")
	float VanishDistanceCm = 260.0f;

protected:
	virtual void BeginPlay() override;

private:
	void SetOpacity(float Opacity);

	TWeakObjectPtr<APawn> Viewer;
	EPhantomType PhantomType = EPhantomType::ShadowFigure;
	float Intensity = 0.5f;
	float Lifetime = 4.0f;
	float Age = 0.0f;
	float StareTime = 0.0f;
	float FadeOut = -1.0f;
	FVector DriftDirection = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;
};
