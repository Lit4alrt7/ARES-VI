#include "World/SurfaceBeacon.h"

#include "Core/MarsTime.h"
#include "Items/MarsInventoryComponent.h"
#include "Player/MarsCharacter.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ASurfaceBeacon::ASurfaceBeacon()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Mast = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mast"));
	SetRootComponent(Mast);
	if (CylinderMesh.Succeeded())
	{
		Mast->SetStaticMesh(CylinderMesh.Object);
	}
	Mast->SetRelativeScale3D(FVector(0.08f, 0.08f, 1.4f));
	Mast->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	Strobe = CreateDefaultSubobject<UPointLightComponent>(TEXT("Strobe"));
	Strobe->SetupAttachment(Mast);
	Strobe->SetUsingAbsoluteScale(true);
	Strobe->SetRelativeLocation(FVector(0, 0, 55.0f));
	Strobe->SetIntensityUnits(ELightUnits::Lumens);
	Strobe->SetIntensity(0.0f);
	Strobe->SetAttenuationRadius(2500.0f);
	Strobe->SetLightColor(FLinearColor(1.0f, 0.35f, 0.1f));
	Strobe->SetCastShadows(false);
}

void ASurfaceBeacon::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoRegisterWithPlayer)
	{
		if (AMarsCharacter* Player = Cast<AMarsCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Player->RegisterBeacon(this);
		}
	}
}

void ASurfaceBeacon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsActive())
	{
		Strobe->SetIntensity(0.0f);
		return;
	}

	// Li-ion capacity effectively drops in the cold; model as extra drain.
	float ColdFactor = 1.0f;
	if (const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		ColdFactor += FMath::Clamp(-Env->GetAmbientTemperatureC() / 60.0f, 0.0f, 1.5f);
	}
	BatteryWh = FMath::Max(BatteryWh - PowerDrawW * ColdFactor * MarsTime::ToGameHours(this, DeltaTime), 0.0f);

	StrobeTimer += DeltaTime;
	if (StrobeTimer >= StrobePeriodSeconds)
	{
		StrobeTimer = 0.0f;
	}
	Strobe->SetIntensity(StrobeTimer < 0.12f ? 8000.0f : 0.0f);
}

void ASurfaceBeacon::Interact_Implementation(AMarsCharacter* InteractingCharacter)
{
	if (InteractingCharacter && InteractingCharacter->GetInventory() &&
		InteractingCharacter->GetInventory()->AddItem(EMarsItemType::SurfaceBeacon, 1) > 0)
	{
		InteractingCharacter->ShowNotification(FText::FromString(FString::Printf(TEXT("BEACON %s RECOVERED"), *BeaconLabel)), 2.0f);
		Destroy();
	}
}

FText ASurfaceBeacon::GetInteractionPrompt_Implementation(AMarsCharacter* InteractingCharacter) const
{
	return FText::FromString(FString::Printf(TEXT("[E] Recover beacon %s (battery %.0f%%)"), *BeaconLabel, GetBatteryFraction() * 100.0f));
}
