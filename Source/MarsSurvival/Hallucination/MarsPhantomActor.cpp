#include "Hallucination/MarsPhantomActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AMarsPhantomActor::AMarsPhantomActor()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	if (CylinderMesh.Succeeded())
	{
		Body->SetStaticMesh(CylinderMesh.Object);
	}
	// Tall, narrow, slightly hunched silhouette (~1.85 m).
	Body->SetRelativeScale3D(FVector(0.45f, 0.35f, 1.85f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCastShadow(false);
	Body->SetCanEverAffectNavigation(false);

	PhantomMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MarsSurvival/Materials/M_Phantom.M_Phantom")));
}

void AMarsPhantomActor::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Mat = PhantomMaterial.LoadSynchronous())
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(Mat, this);
		Body->SetMaterial(0, MaterialInstance);
	}
	SetOpacity(0.0f);
}

void AMarsPhantomActor::InitPhantom(APawn* InViewer, EPhantomType InType, float InIntensity, float InLifetimeSeconds)
{
	Viewer = InViewer;
	PhantomType = InType;
	Intensity = FMath::Clamp(InIntensity, 0.0f, 1.0f);
	Lifetime = FMath::Max(InLifetimeSeconds, 0.5f);

	if (PhantomType == EPhantomType::PeripheralMovement)
	{
		// A quick dart across the edge of vision.
		DriftDirection = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal() * 260.0f;
		Lifetime = FMath::Min(Lifetime, 1.2f);
	}
	else
	{
		DriftDirection = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal() * 15.0f;
	}
}

void AMarsPhantomActor::SetOpacity(float Opacity)
{
	if (MaterialInstance)
	{
		MaterialInstance->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}
	Body->SetVisibility(Opacity > 0.02f);
}

void AMarsPhantomActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;

	const APawn* ViewerPawn = Viewer.Get();
	if (!ViewerPawn)
	{
		Destroy();
		return;
	}

	AddActorWorldOffset(DriftDirection * DeltaTime);

	FVector ViewLocation;
	FRotator ViewRotation;
	ViewerPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	// Always face the viewer.
	const FVector ToViewer = ViewLocation - GetActorLocation();
	SetActorRotation(FRotator(0.0f, ToViewer.Rotation().Yaw, 0.0f));

	// Direct gaze or approach dissolves it.
	const FVector ToPhantom = (GetActorLocation() + FVector(0, 0, 90.0f) - ViewLocation);
	const float Dot = FVector::DotProduct(ViewRotation.Vector(), ToPhantom.GetSafeNormal());
	StareTime = Dot > 0.97f ? StareTime + DeltaTime : FMath::Max(StareTime - DeltaTime, 0.0f);

	if (FadeOut < 0.0f && (StareTime >= StareToVanishSeconds || ToPhantom.Size() < VanishDistanceCm || Age >= Lifetime))
	{
		FadeOut = 0.0f;
	}

	float Opacity = FMath::Clamp(Age / 0.4f, 0.0f, 1.0f) * FMath::Lerp(0.35f, 0.9f, Intensity);
	if (FadeOut >= 0.0f)
	{
		FadeOut += DeltaTime;
		Opacity *= FMath::Clamp(1.0f - FadeOut / 0.25f, 0.0f, 1.0f);
		if (FadeOut >= 0.25f)
		{
			Destroy();
			return;
		}
	}
	// Flicker like a bad signal.
	Opacity *= FMath::FRand() < 0.08f ? 0.2f : 1.0f;
	SetOpacity(Opacity);
}
