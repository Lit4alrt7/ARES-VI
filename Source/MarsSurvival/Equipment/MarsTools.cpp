#include "Equipment/MarsTools.h"

#include "Audio/MarsAudioManager.h"
#include "Base/MarsBaseModule.h"
#include "Core/MarsTutorialManager.h"
#include "Items/MarsInventoryComponent.h"
#include "Player/MarsCharacter.h"
#include "Player/PlayerBiometricsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// ============================================================================
// Weld / sealant gun
// ============================================================================

AWeldSealantTool::AWeldSealantTool()
{
	ItemType = EMarsItemType::WeldSealantGun;
	ViewModelKind = EMarsPropKind::WeldGun;
	HeldOffset = FVector(44.0f, 15.0f, -17.0f);
	GripLocation = FVector(-4.0f, 0.0f, -8.0f);
	GripRotation = FRotator(15.0f, 0.0f, 0.0f);
	GripWidth = 5.0f;
}

void AWeldSealantTool::Reload()
{
	AMarsCharacter* Character = OwnerCharacter.Get();
	if (Character && Character->GetInventory() && LoadedCharge <= 0 &&
		Character->GetInventory()->RemoveItem(EMarsItemType::SealantCartridge, 1))
	{
		LoadedCharge = PatchesPerCartridge;
		if (UMarsAudioManager* Audio = GetAudio())
		{
			Audio->PlayRatchet();
		}
	}
}

void AWeldSealantTool::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	LastTargetStatus.Reset();
	if (!bPrimaryHeld)
	{
		PatchProgress = 0.0f;
		return;
	}

	FHitResult Hit;
	AMarsBaseModule* Module = TraceFromView(RangeCm, Hit) ? GetHitModule(Hit) : nullptr;
	if (!Module)
	{
		LastTargetStatus = TEXT("NO HULL SURFACE");
		return;
	}
	if (LoadedCharge <= 0)
	{
		Reload();
		if (LoadedCharge <= 0)
		{
			LastTargetStatus = TEXT("NO CARTRIDGES");
			return;
		}
	}

	bool bWorking = false;
	if (Module->PunctureCount > 0)
	{
		bWorking = true;
		PatchProgress += DeltaTime;
		LastTargetStatus = FString::Printf(TEXT("CURING %.0f%% (%d holes)"), 100.0f * PatchProgress / PatchSeconds, Module->PunctureCount);
		if (PatchProgress >= PatchSeconds)
		{
			PatchProgress = 0.0f;
			Module->PatchPunctures(1);
			AMarsTutorialManager::Report(this, TEXT("HullPatched"));
			--LoadedCharge;
		}
	}
	else if (Module->HullIntegrity < 1.0f)
	{
		bWorking = true;
		const float Amount = WeldIntegrityPerSecond * DeltaTime;
		Module->RepairHull(Amount);
		WeldAccumulated += Amount;
		if (WeldAccumulated >= 0.2f)
		{
			WeldAccumulated = 0.0f;
			--LoadedCharge;
		}
		LastTargetStatus = FString::Printf(TEXT("ARC WELD %.0f%%"), Module->HullIntegrity * 100.0f);
	}
	else
	{
		LastTargetStatus = TEXT("HULL NOMINAL");
	}

	HissTimer -= DeltaTime;
	if (bWorking && HissTimer <= 0.0f)
	{
		HissTimer = 0.35f;
		if (UMarsAudioManager* Audio = GetAudio())
		{
			Audio->PlaySealantHiss();
		}
	}
}

FText AWeldSealantTool::GetStatusText() const
{
	const AMarsCharacter* Character = OwnerCharacter.Get();
	const int32 Spare = Character && Character->GetInventory() ? Character->GetInventory()->GetItemCount(EMarsItemType::SealantCartridge) : 0;
	return FText::FromString(FString::Printf(TEXT("SEALANT %d (+%d)  %s"), LoadedCharge, Spare, *LastTargetStatus));
}

// ============================================================================
// Torque wrench
// ============================================================================

ATorqueWrenchTool::ATorqueWrenchTool()
{
	ItemType = EMarsItemType::TorqueWrench;
	ViewModelKind = EMarsPropKind::TorqueWrench;
	HeldOffset = FVector(46.0f, 15.0f, -16.0f);
	GripLocation = FVector(-10.0f, 0.0f, 0.0f);
	GripRotation = FRotator(-80.0f, 0.0f, 0.0f);
	GripWidth = 3.3f;
	HeldRotation = FRotator(0.0f, -12.0f, 0.0f);
}

void ATorqueWrenchTool::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	LastTargetStatus.Reset();
	if (!bPrimaryHeld)
	{
		return;
	}

	FHitResult Hit;
	AMarsBaseModule* Module = TraceFromView(RangeCm, Hit) ? GetHitModule(Hit) : nullptr;
	if (!Module)
	{
		LastTargetStatus = TEXT("NO FLANGE IN REACH");
		return;
	}

	Module->ApplySealTorque(SealRestorePerSecond * DeltaTime);
	TorqueSeconds += DeltaTime;
	if (TorqueSeconds > 1.5f)
	{
		AMarsTutorialManager::Report(this, TEXT("SealsTorqued"));
	}
	LastTargetStatus = FString::Printf(TEXT("SEALS %.0f%% (%.0f C)"), Module->SealIntegrity * 100.0f, Module->SealTemperatureC);

	RatchetTimer -= DeltaTime;
	if (RatchetTimer <= 0.0f)
	{
		RatchetTimer = 0.22f;
		if (UMarsAudioManager* Audio = GetAudio())
		{
			Audio->PlayRatchet();
		}
		// Little ratchet throw on the viewmodel.
		AddActorLocalRotation(FRotator(0.0f, 0.0f, 6.0f));
	}
	SetActorRelativeRotation(FMath::RInterpTo(GetRootComponent()->GetRelativeRotation(), HeldRotation, DeltaTime, 8.0f));
}

FText ATorqueWrenchTool::GetStatusText() const
{
	return FText::FromString(FString::Printf(TEXT("TORQUE WRENCH  %s"), LastTargetStatus.IsEmpty() ? TEXT("[hold LMB on bulkhead]") : *LastTargetStatus));
}

// ============================================================================
// Dosimeter
// ============================================================================

ARadiationDosimeter::ARadiationDosimeter()
{
	ItemType = EMarsItemType::RadiationDosimeter;
	ViewModelKind = EMarsPropKind::Dosimeter;
	HeldOffset = FVector(34.0f, 9.0f, -11.0f);
	GripLocation = FVector(0.0f, 0.0f, -4.0f);
	GripWidth = 7.0f;
	GripRotation = FRotator::ZeroRotator;
	HeldRotation = FRotator(0.0f, 180.0f, 0.0f); // display faces the player
}

void ARadiationDosimeter::OnEquipped(AMarsCharacter* NewOwner)
{
	Super::OnEquipped(NewOwner);
	if (UMarsAudioManager* Audio = GetAudio())
	{
		Audio->SetDosimeterHeld(true);
	}
}

void ARadiationDosimeter::OnUnequipped()
{
	if (UMarsAudioManager* Audio = GetAudio())
	{
		Audio->SetDosimeterHeld(false);
	}
	Super::OnUnequipped();
}

void ARadiationDosimeter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const AMarsCharacter* Character = OwnerCharacter.Get();
	const UPlayerBiometricsComponent* Bio = Character ? Character->GetBiometrics() : nullptr;
	if (!Bio)
	{
		return;
	}

	// Log-scale needle: 0.01 mSv/h -> left stop, 10 mSv/h -> right stop.
	const float Rate = FMath::Max(Bio->CurrentDoseRateMsvPerHour, 0.001f);
	const float Norm = FMath::Clamp((FMath::LogX(10.0f, Rate) + 2.0f) / 3.0f, 0.0f, 1.0f);
	for (UStaticMeshComponent* Needle : FindTaggedParts(TEXT("Needle")))
	{
		Needle->SetRelativeRotation(FRotator(0.0f, 0.0f, FMath::Lerp(55.0f, -55.0f, Norm) + FMath::FRandRange(-2.0f, 2.0f) * Norm));
	}

	// LED blinks with the click train.
	const UMarsAudioManager* Audio = GetAudio();
	const float Clicks = Audio ? Audio->GetGeigerClickCount() : 0.0f;
	if (Clicks != LastClickCount)
	{
		LastClickCount = Clicks;
		LedFlash = 1.0f;
	}
	LedFlash = FMath::Max(LedFlash - DeltaTime * 15.0f, 0.0f);
	for (UStaticMeshComponent* Led : FindTaggedParts(TEXT("Led")))
	{
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Led->GetMaterial(0)))
		{
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(1.0f, 0.08f, 0.05f) * (0.5f + 12.0f * LedFlash));
		}
	}
}

FText ARadiationDosimeter::GetStatusText() const
{
	const AMarsCharacter* Character = OwnerCharacter.Get();
	const UPlayerBiometricsComponent* Bio = Character ? Character->GetBiometrics() : nullptr;
	if (!Bio)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("DOSE %.1f mSv  |  %.3f mSv/h"), Bio->RadiationDoseMsv, Bio->CurrentDoseRateMsvPerHour));
}
