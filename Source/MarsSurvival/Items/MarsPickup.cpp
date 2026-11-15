#include "Items/MarsPickup.h"

#include "Audio/MarsAudioManager.h"
#include "Items/MarsInventoryComponent.h"
#include "Items/MarsItemLibrary.h"
#include "Player/MarsCharacter.h"
#include "Props/MarsPropBuilder.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AMarsPickup::AMarsPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.35f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetVisibility(false);
	Mesh->SetHiddenInGame(true);

	ModelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ModelPivot"));
	ModelPivot->SetupAttachment(Root);

	MarkerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MarkerLight"));
	MarkerLight->SetupAttachment(Root);
	MarkerLight->SetRelativeLocation(FVector(0, 0, 40.0f));
	MarkerLight->SetIntensityUnits(ELightUnits::Lumens);
	MarkerLight->SetIntensity(35.0f);
	MarkerLight->SetAttenuationRadius(220.0f);
	MarkerLight->SetCastShadows(false);
}

void AMarsPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AMarsPropBuilder::BuildProp(ModelPivot, AMarsPropBuilder::GetPropForItem(ItemType), ModelScale, false);

	FLinearColor Colour(0.9f, 0.9f, 0.9f);
	switch (UMarsItemLibrary::GetItemInfo(ItemType).Category)
	{
	case EMarsItemCategory::Medical:    Colour = FLinearColor(0.2f, 0.5f, 1.0f); break;
	case EMarsItemCategory::Component:  Colour = FLinearColor(0.2f, 1.0f, 0.6f); break;
	case EMarsItemCategory::Sample:     Colour = FLinearColor(1.0f, 0.5f, 0.2f); break;
	case EMarsItemCategory::Consumable: Colour = FLinearColor(1.0f, 1.0f, 0.4f); break;
	default: break;
	}
	MarkerLight->SetLightColor(Colour);
}

void AMarsPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Slow PS1-style turntable spin + bob.
	ModelPivot->AddRelativeRotation(FRotator(0.0f, 40.0f * DeltaTime, 0.0f));
	ModelPivot->SetRelativeLocation(FVector(0, 0, 6.0f + 3.0f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.0f)));
}

void AMarsPickup::Interact_Implementation(AMarsCharacter* InteractingCharacter)
{
	if (!InteractingCharacter || !InteractingCharacter->GetInventory())
	{
		return;
	}
	const int32 Taken = InteractingCharacter->GetInventory()->AddItem(ItemType, Count);
	if (Taken <= 0)
	{
		InteractingCharacter->ShowNotification(FText::FromString(TEXT("TOO HEAVY - CARRY LIMIT REACHED")), 2.5f);
		return;
	}

	if (UMarsAudioManager* Audio = InteractingCharacter->GetAudioManager())
	{
		Audio->PlayUIBeep(1.3f);
	}
	InteractingCharacter->ShowNotification(FText::FromString(FString::Printf(TEXT("+%d %s"), Taken, *UMarsItemLibrary::GetItemName(ItemType).ToString().ToUpper())), 2.0f);
	Count -= Taken;
	if (!InteractingCharacter->HasInspected(ItemType))
	{
		InteractingCharacter->BeginInspect(ItemType);
	}
	if (Count <= 0)
	{
		Destroy();
	}
}

FText AMarsPickup::GetInteractionPrompt_Implementation(AMarsCharacter* InteractingCharacter) const
{
	return FText::FromString(FString::Printf(TEXT("[E] Take %s x%d"), *UMarsItemLibrary::GetItemName(ItemType).ToString(), Count));
}
