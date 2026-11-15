#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MarsInteractable.h"
#include "Items/MarsItemTypes.h"
#include "MarsPickup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/** A physical item lying in the world, shown with its procedural PS1 model. Press E to pick up. */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsPickup : public AActor, public IMarsInteractable
{
	GENERATED_BODY()

public:
	AMarsPickup();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

	virtual void Interact_Implementation(AMarsCharacter* InteractingCharacter) override;
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* InteractingCharacter) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Invisible interaction volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ModelPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MarkerLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	EMarsItemType ItemType = EMarsItemType::RegolithSample;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Display scale of the model (real-size items are tiny on the floor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float ModelScale = 1.6f;
};
