#include "Player/MarsSuitViewmodel.h"

#include "Player/EVASuitComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Props/MarsPropBuilder.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// Arm-local frame: +X runs from the elbow to the fist, +Z is the back of the wrist (display side).
	// Raised: the forearm crosses the lower view from the left, display turned toward the eyes.
	// Lowered: the same pose dropped out of frame.
	const FVector RaisedLoc(38.0f, -3.0f, -10.0f);
	const FVector LoweredLoc(26.0f, -26.0f, -42.0f);
	FRotator ArmPose()
	{
		const FVector AlongArm = FVector(0.35f, 1.0f, 0.12f).GetSafeNormal();      // camera space: to the right
		const FVector ToEye = (-RaisedLoc).GetSafeNormal();                         // display faces the camera
		return FRotationMatrix::MakeFromXZ(AlongArm, ToEye).Rotator();
	}
}

UMarsSuitViewmodel::UMarsSuitViewmodel()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMarsSuitViewmodel::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void UMarsSuitViewmodel::Build()
{
	Arm = AMarsPropBuilder::AddPivot(this, LoweredLoc, ArmPose(), TEXT("SuitArm"));
	if (!Arm)
	{
		return;
	}
	using S = EMarsPropShape;
	const FLinearColor Fabric(0.8f, 0.8f, 0.77f);
	const FLinearColor FabricShade(0.6f, 0.6f, 0.58f);
	const FLinearColor Metal(0.52f, 0.54f, 0.57f);
	const FLinearColor Dark(0.08f, 0.085f, 0.09f);
	const FLinearColor Glove(0.2f, 0.21f, 0.23f);
	const FLinearColor Accent(0.9f, 0.38f, 0.08f);
	const FLinearColor Screen(0.03f, 0.2f, 0.1f);
	const FRotator AlongX(90.0f, 0.0f, 0.0f);
	const FLinearColor Rubber(0.05f, 0.05f, 0.055f);
	// Material classes: 0 = fabric (rough), 1 = rubber (very rough, dark), 2 = metal (smooth, metallic), 3 = plastic.
	auto Add = [this](S Shape, const FVector& L, const FVector& Sz, const FRotator& R, const FLinearColor& C, float E = 0.2f, int32 Mat = 0)
	{
		UStaticMeshComponent* Part = AMarsPropBuilder::AddPart(Arm, Shape, L, Sz, R, C, E);
		static const float RoughTab[4] = { 0.92f, 0.98f, 0.28f, 0.55f };
		static const float MetalTab[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
		AMarsPropBuilder::SetSurface(Part, RoughTab[FMath::Clamp(Mat, 0, 3)], MetalTab[FMath::Clamp(Mat, 0, 3)]);
		return Part;
	};

	// Forearm sleeve with layered armour plate (fabric + hard shell).
	Add(S::Cylinder, FVector(-18.0f, 0, 0), FVector(8.6f, 8.6f, 30.0f), AlongX, Fabric);
	Add(S::Box, FVector(-16.0f, 0, 3.9f), FVector(18.0f, 6.0f, 1.4f), FRotator::ZeroRotator, FabricShade, 0.2f, 3);
	Add(S::Box, FVector(-16.0f, 0, 4.7f), FVector(16.0f, 4.2f, 0.4f), FRotator::ZeroRotator, Metal, 0.3f, 2);
	Add(S::Cylinder, FVector(-24.0f, 0, 0), FVector(8.9f, 8.9f, 1.4f), AlongX, Accent, 0.6f, 3);
	// Elbow pressure bellows: ribbed black rubber convolutes between metal retaining rings.
	Add(S::Cylinder, FVector(-33.5f, 0, 0), FVector(9.4f, 9.4f, 0.8f), AlongX, Metal, 0.3f, 2);
	for (int32 i = 0; i < 6; ++i)
	{
		const float D = i % 2 ? 8.4f : 9.2f;
		Add(S::Cylinder, FVector(-35.0f - i * 1.1f, 0, 0), FVector(D, D, 1.0f), AlongX, Rubber, 0.05f, 1);
	}
	// Wrist pressure seal: rubber gasket convolutes, then a machined bearing ring.
	for (int32 i = 0; i < 5; ++i)
	{
		const float D = i % 2 ? 7.4f : 8.2f;
		Add(S::Cylinder, FVector(-3.0f - i * 1.2f, 0, 0), FVector(D, D, 0.9f), AlongX, Rubber, 0.05f, 1);
	}
	Add(S::Cylinder, FVector(-9.2f, 0, 0), FVector(8.7f, 8.7f, 0.7f), AlongX, Metal, 0.3f, 2);              // sleeve-side clamp ring
	// Hydraulic assist strut along the forearm.
	Add(S::Cylinder, FVector(-14.0f, -4.6f, 1.0f), FVector(1.2f, 1.2f, 16.0f), AlongX, Metal, 0.3f, 2);
	Add(S::Cylinder, FVector(-7.0f, -4.6f, 1.0f), FVector(0.7f, 0.7f, 6.0f), AlongX, FLinearColor(0.85f, 0.85f, 0.88f), 0.3f, 2);
	// Cable loom from the wrist computer into the sleeve.
	Add(S::Box, FVector(-9.0f, 3.2f, 3.0f), FVector(14.0f, 0.7f, 0.7f), FRotator(0, -4, 0), Accent, 0.2f, 1);
	Add(S::Box, FVector(-9.0f, 2.3f, 3.6f), FVector(14.0f, 0.5f, 0.5f), FRotator(0, -3, 0), Dark, 0.2f, 1);
	// Metallic wrist bearing with a black rubber O-ring.
	Add(S::Cylinder, FVector(0.5f, 0, 0), FVector(7.4f, 7.4f, 2.2f), AlongX, Metal, 0.3f, 2);
	Add(S::Cylinder, FVector(0.5f, 0, 0), FVector(7.7f, 7.7f, 0.5f), AlongX, Rubber, 0.05f, 1);
	// Glove (loose fist): rubberised palm, fabric back.
	Add(S::Box, FVector(5.5f, 0, 0), FVector(7.0f, 7.4f, 5.0f), FRotator::ZeroRotator, Glove, 0.2f, 1);
	Add(S::Box, FVector(9.4f, 0, -0.8f), FVector(2.2f, 7.2f, 3.6f), FRotator(0, 0, 0), Glove, 0.2f, 1);
	Add(S::Box, FVector(6.0f, 4.2f, -0.4f), FVector(5.0f, 1.6f, 2.0f), FRotator(0, 20, 0), Glove, 0.2f, 1);
	Add(S::Box, FVector(5.5f, 0, 2.6f), FVector(4.0f, 4.0f, 0.4f), FRotator::ZeroRotator, Accent, 0.8f, 3);

	// Wrist telemetry computer: housing, bezel, screen, hard keys, status LEDs.
	Add(S::Box, FVector(-6.0f, 0, 4.9f), FVector(10.0f, 7.0f, 1.8f), FRotator::ZeroRotator, Dark);
	Add(S::Box, FVector(-6.0f, 0, 5.85f), FVector(9.0f, 6.0f, 0.15f), FRotator::ZeroRotator, Metal);
	Add(S::Box, FVector(-6.0f, 0, 5.95f), FVector(8.0f, 5.2f, 0.12f), FRotator::ZeroRotator, Screen, 1.2f);
	for (int32 i = 0; i < 3; ++i)
	{
		Add(S::Box, FVector(-11.4f, -2.0f + i * 2.0f, 5.4f), FVector(0.9f, 1.4f, 0.6f), FRotator::ZeroRotator, Accent, 0.6f);
	}
	Add(S::Sphere, FVector(-1.6f, 2.8f, 5.7f), FVector(0.5f, 0.5f, 0.5f), FRotator::ZeroRotator, FLinearColor(0.2f, 1.0f, 0.3f), 10.0f);
	Add(S::Sphere, FVector(-1.6f, 1.6f, 5.7f), FVector(0.5f, 0.5f, 0.5f), FRotator::ZeroRotator, FLinearColor(1.0f, 0.6f, 0.1f), 10.0f);

	// Helmet collar: metal neck ring + rubber seal, fixed to the helmet (always at the bottom of the faceplate).
	if (USceneComponent* Collar = AMarsPropBuilder::AddPivot(this, FVector(14.0f, 0.0f, -21.5f), FRotator::ZeroRotator, TEXT("Collar")))
	{
		for (int32 i = -7; i <= 7; ++i)
		{
			const float A = FMath::DegreesToRadians(i * 11.0f);
			const FVector Pos(FMath::Cos(A) * 9.0f - 9.0f, FMath::Sin(A) * 26.0f, 0.0f);
			const FRotator Rot(0.0f, i * 11.0f, 0.0f);
			if (UStaticMeshComponent* Ring = AMarsPropBuilder::AddPart(Collar, S::Box, Pos, FVector(2.2f, 3.9f, 2.0f), Rot, Metal, 0.25f))
			{
				AMarsPropBuilder::SetSurface(Ring, 0.25f, 1.0f);
				Ring->SetCastShadow(false);
			}
			if (UStaticMeshComponent* Seal = AMarsPropBuilder::AddPart(Collar, S::Box, Pos + FVector(0.6f, 0.0f, 1.4f), FVector(1.4f, 3.9f, 0.9f), Rot, Rubber, 0.05f))
			{
				AMarsPropBuilder::SetSurface(Seal, 0.98f, 0.0f);
				Seal->SetCastShadow(false);
			}
		}
		CollarRoot = Collar;
	}

	WristText = NewObject<UTextRenderComponent>(GetOwner(), TEXT("WristText"));
	WristText->SetupAttachment(Arm);
	// Text lies flat on the screen (+Z), reading from the elbow toward the fist.
	WristText->SetRelativeLocation(FVector(-6.0f, 0.0f, 6.1f));
	// Text faces the display normal (+Z) with its up toward the far edge of the wrist (-Y), reading toward the fist.
	WristText->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, -1.0f, 0.0f)).Rotator());
	WristText->SetHorizontalAlignment(EHTA_Center);
	WristText->SetVerticalAlignment(EVRTA_TextCenter);
	WristText->SetWorldSize(0.95f);
	WristText->SetTextRenderColor(FColor(90, 255, 140));
	WristText->RegisterComponent();

	TArray<USceneComponent*> Children;
	Arm->GetChildrenComponents(true, Children);
	for (USceneComponent* C : Children)
	{
		if (UStaticMeshComponent* M = Cast<UStaticMeshComponent>(C))
		{
			M->SetCastShadow(false);
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
			{
				MID->SetScalarParameterValue(TEXT("JitterAmount"), 0.0f);
			}
		}
	}
}

void UMarsSuitViewmodel::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwner());
	if (!Arm || !C)
	{
		return;
	}
	const UEVASuitComponent* Suit = C->GetSuit();
	const bool bShowArm = Suit && Suit->IsSuitDonned() && !C->IsDead() && !C->IsInspecting();
	Arm->SetVisibility(bShowArm, true);
	if (CollarRoot)
	{
		CollarRoot->SetVisibility(bShowArm, true);
	}
	if (!bShowArm)
	{
		return;
	}

	Blend = FMath::FInterpTo(Blend, bRaised ? 1.0f : 0.0f, DeltaTime, 7.0f);
	const float Breath = FMath::Sin(GetWorld()->GetTimeSeconds() * 1.6f) * 0.25f;
	Arm->SetRelativeLocation(FMath::Lerp(LoweredLoc, RaisedLoc, Blend) + FVector(0, 0, Breath));
	Arm->SetRelativeRotation(ArmPose());
	Arm->SetVisibility(Blend > 0.02f, true);

	TextTimer -= DeltaTime;
	if (TextTimer <= 0.0f && WristText)
	{
		TextTimer = 0.25f;
		const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
		const UPlayerBiometricsComponent* Bio = C->GetBiometrics();
		const float Hours = Env ? Env->GetLocalSolarTimeHours() : 0.0f;
		const int32 HH = FMath::FloorToInt(Hours);
		const int32 MM = FMath::FloorToInt((Hours - HH) * 60.0f);
		const float Batt = Suit->BatteryCapacityWh > 0.0f ? Suit->BatteryWh / Suit->BatteryCapacityWh : 0.0f;
		WristText->SetText(FText::FromString(FString::Printf(
			TEXT("SOL %d  %02d:%02d\nO2   %3.0f%%\nBATT %3.0f%%\nEXT  %4.0fC\nRAD  %.2f"),
			Env ? Env->GetSolNumber() : 1, HH, MM, Suit->GetO2Fraction01() * 100.0f, Batt * 100.0f,
			Env ? Env->GetAmbientTemperatureC() : -60.0f, Bio ? Bio->CurrentDoseRateMsvPerHour : 0.0f)));
		const bool bWarn = Suit->GetO2Fraction01() < 0.25f || Batt < 0.2f;
		WristText->SetTextRenderColor(bWarn ? FColor(255, 120, 40) : FColor(90, 255, 140));
	}
}
