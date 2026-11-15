#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarsImpactFX.generated.h"

class UStaticMeshComponent;

UENUM()
enum class EMarsImpactType : uint8
{
	Metal,      // hull plate: hot orange sparks + scorched bullet hole
	Regolith,   // rock / soil / furniture: dust chips + dark pock mark
	PhantomAsh, // a hallucination disintegrating: grey ash flakes drifting up
	ThrusterDust // regolith blasted sideways by engine exhaust (no mark left)
};

/**
 * Self-contained PS1-style impact effect: a burst of tiny procedural chips with Mars gravity,
 * plus (for real surfaces) a persistent bullet-hole mark oriented to the surface normal.
 */
UCLASS()
class MARSSURVIVAL_API AMarsImpactFX : public AActor
{
	GENERATED_BODY()

public:
	AMarsImpactFX();

	static void Spawn(UWorld* World, const FVector& Location, const FVector& Normal, EMarsImpactType Type);

	virtual void Tick(float DeltaTime) override;

private:
	struct FChip
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		FVector Velocity;
		float Life;
		float MaxLife;
		FVector BaseScale;
	};

	void Build(const FVector& Normal, EMarsImpactType Type);

	TArray<FChip> Chips;
	float Gravity = -371.0f;
	EMarsImpactType ImpactType = EMarsImpactType::Metal;
};

/** Persistent bullet hole; the oldest ones are recycled so the level never fills up. */
UCLASS()
class MARSSURVIVAL_API AMarsBulletHole : public AActor
{
	GENERATED_BODY()

public:
	AMarsBulletHole();
	void Init(const FVector& Normal, bool bMetal);
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Glow;
	float Age = 0.0f;
	bool bMetalHole = false;
};
