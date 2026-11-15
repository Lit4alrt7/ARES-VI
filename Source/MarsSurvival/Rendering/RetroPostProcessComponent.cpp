#include "Rendering/RetroPostProcessComponent.h"

#include "MarsSurvival.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

URetroPostProcessComponent::URetroPostProcessComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PostProcessMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MarsSurvival/Materials/M_PS1_PostProcess.M_PS1_PostProcess")));
}

void URetroPostProcessComponent::BeginPlay()
{
	Super::BeginPlay();

	TargetCamera = GetOwner() ? GetOwner()->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!TargetCamera)
	{
		return;
	}

	// Late-90s look: no motion blur, minimal bloom, no lens effects.
	FPostProcessSettings& PP = TargetCamera->PostProcessSettings;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.0f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.15f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.0f; // the material draws its own vignette
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.0f;

	if (UMaterialInterface* Material = PostProcessMaterial.LoadSynchronous())
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(Material, this);
		PP.AddBlendable(MaterialInstance, 1.0f);
	}
	else
	{
		UE_LOG(LogMarsSurvival, Warning, TEXT("Retro post-process material missing (%s). Run Content/Python/setup_mars_project.py in the editor."),
			*PostProcessMaterial.ToString());
	}
}

void URetroPostProcessComponent::TriggerCherenkovFlash(float Strength)
{
	FlashValue = FMath::Max(FlashValue, FMath::Clamp(Strength, 0.0f, 1.0f));
	// Flashes are reported mostly in peripheral vision, often as streaks or "starbursts".
	const float Side = FMath::RandBool() ? FMath::FRandRange(0.05f, 0.3f) : FMath::FRandRange(0.7f, 0.95f);
	FlashPosition = FVector2D(Side, FMath::FRandRange(0.15f, 0.85f));
}

void URetroPostProcessComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!MaterialInstance)
	{
		return;
	}

	FlashValue = FMath::Max(FlashValue - DeltaTime * 5.0f, 0.0f);

	float Hallucination = 0.0f;
	float Panic = 0.0f;
	float Hypoxia = 0.0f;
	float Cyanosis = 0.0f, ColdGrade = 0.0f, Nausea = 0.0f, Distortion = 0.0f, Tunnel = 0.0f;
	if (const AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner()))
	{
		if (const UPlayerBiometricsComponent* Bio = Character->GetBiometrics())
		{
			Hallucination = Bio->HallucinationIntensity;
			Panic = Bio->PanicLevel;
			Hypoxia = Bio->GetHypoxia01();
			const int32 Stage = Bio->GetStageIndex();
			// Stage 1: peripheral cyanosis tint. Stage 2: desaturated cold grade. Stage 3: tunnel vision, nausea
			// waves. Stage 4: violent tearing distortion.
			Cyanosis = FMath::Clamp((Bio->MethemoglobinPercentage - 3.0f) / 25.0f, 0.0f, 1.0f);
			ColdGrade = Stage >= 2 ? FMath::Min(0.45f + 0.15f * (Stage - 2), 1.0f) : 0.0f;
			Nausea = Stage >= 3 ? 0.4f + 0.4f * (Stage - 3) : 0.0f;
			Tunnel = Stage >= 3 ? 0.5f + 0.5f * (Stage - 3) : 0.0f;
			Distortion = Stage >= 4 ? 1.0f : (Stage == 3 ? 0.12f : 0.0f);
			if (Bio->IsDead())
			{
				Hypoxia = 1.0f;
			}
		}
	}
	MaterialInstance->SetScalarParameterValue(TEXT("Cyanosis"), Cyanosis);
	MaterialInstance->SetScalarParameterValue(TEXT("ColdGrade"), ColdGrade);
	MaterialInstance->SetScalarParameterValue(TEXT("Nausea"), Nausea);
	MaterialInstance->SetScalarParameterValue(TEXT("TunnelVision"), Tunnel);
	MaterialInstance->SetScalarParameterValue(TEXT("Distortion"), Distortion);
	SmoothedHallucination = FMath::FInterpTo(SmoothedHallucination, Hallucination, DeltaTime, 2.0f);

	float Dust = 0.0f;
	if (const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>())
	{
		const AMarsCharacter* Character = Cast<AMarsCharacter>(GetOwner());
		const bool bOutside = Character && Character->GetCurrentModule() == nullptr;
		Dust = bOutside ? Env->GetStormIntensity() : 0.0f;
	}

	const float Enabled = bEnableRetroEffect ? 1.0f : 0.0f;
	MaterialInstance->SetScalarParameterValue(TEXT("RetroEnabled"), Enabled);
	MaterialInstance->SetScalarParameterValue(TEXT("PixelSize"), PixelSize);
	MaterialInstance->SetScalarParameterValue(TEXT("ColorLevels"), ColorLevels);
	MaterialInstance->SetScalarParameterValue(TEXT("DitherStrength"), DitherStrength);
	MaterialInstance->SetScalarParameterValue(TEXT("ScanlineIntensity"), ScanlineIntensity);
	MaterialInstance->SetScalarParameterValue(TEXT("Hallucination"), SmoothedHallucination);
	MaterialInstance->SetScalarParameterValue(TEXT("Panic"), Panic);
	MaterialInstance->SetScalarParameterValue(TEXT("Hypoxia"), Hypoxia);
	MaterialInstance->SetScalarParameterValue(TEXT("DustStorm"), Dust);
	MaterialInstance->SetScalarParameterValue(TEXT("CherenkovFlash"), FlashValue);
	MaterialInstance->SetVectorParameterValue(TEXT("FlashPosition"), FLinearColor(FlashPosition.X, FlashPosition.Y, 0.0f, 0.0f));
}
