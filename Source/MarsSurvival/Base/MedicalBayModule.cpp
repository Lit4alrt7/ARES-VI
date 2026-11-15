#include "Base/MedicalBayModule.h"

#include "MarsSurvival.h"
#include "Core/MarsTime.h"
#include "Items/MarsInventoryComponent.h"
#include "Items/MarsItemLibrary.h"
#include "Player/PlayerBiometricsComponent.h"
#include "World/MarsStationSubsystem.h"

AMedicalBayModule::AMedicalBayModule()
{
	ModuleType = EMarsModuleType::MedicalBay;
	RoomSize = FVector(900.0f, 600.0f, 320.0f);
	BuildShell();
}

float AMedicalBayModule::GetPowerDemandW() const
{
	return Super::GetPowerDemandW() + (IsSynthesizing() ? BioprinterPowerW : 0.0f);
}

FString AMedicalBayModule::RunBloodPanel(const UPlayerBiometricsComponent* Bio) const
{
	if (!Bio)
	{
		return TEXT("SCANNER: NO PATIENT");
	}
	const UMarsStationSubsystem* Station = GetStation();
	return Bio->BuildDiagnosisReport(Station && Station->IsBioAgentCharacterized());
}

TArray<EMarsItemType> AMedicalBayModule::GetAvailableRecipes() const
{
	TArray<EMarsItemType> Recipes = { EMarsItemType::MethyleneBlueCartridge, EMarsItemType::IodinePills };
	const UMarsStationSubsystem* Station = GetStation();
	if (Station && Station->IsBioAgentCharacterized())
	{
		Recipes.Add(EMarsItemType::NeuroAntitoxin);
		Recipes.Add(EMarsItemType::Amifostine);
	}
	return Recipes;
}

bool AMedicalBayModule::StartSynthesis(EMarsItemType Product)
{
	const bool bValid = GetAvailableRecipes().Contains(Product);
	if (!bValid || IsSynthesizing() || BioprinterFeedstock <= 0)
	{
		return false;
	}
	--BioprinterFeedstock;
	ActiveRecipe = Product;
	SynthesisRemainingHours = SynthesisDurationHours;
	return true;
}

float AMedicalBayModule::GetSynthesisProgress() const
{
	return IsSynthesizing() && SynthesisDurationHours > 0.0f ? 1.0f - SynthesisRemainingHours / SynthesisDurationHours : 0.0f;
}

int32 AMedicalBayModule::CollectOutput(UMarsInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return 0;
	}
	int32 Collected = 0;
	for (int32 i = OutputTray.Num() - 1; i >= 0; --i)
	{
		if (Inventory->AddItem(OutputTray[i], 1) > 0)
		{
			OutputTray.RemoveAt(i);
			++Collected;
		}
	}
	return Collected;
}

void AMedicalBayModule::TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds)
{
	if (!IsSynthesizing())
	{
		return;
	}
	const UMarsStationSubsystem* Station = GetStation();
	const float Power = Station ? Station->GetModulePowerAvailability(this) : 1.0f;

	SynthesisRemainingHours -= GameDeltaSeconds / MarsTime::SecondsPerHour * Power;
	if (SynthesisRemainingHours <= 0.0f)
	{
		const EMarsItemType Product = ActiveRecipe;
		OutputTray.Add(Product);
		ActiveRecipe = EMarsItemType::None;
		UE_LOG(LogMarsSurvival, Log, TEXT("Bioprinter finished: %s"), *UMarsItemLibrary::GetItemName(Product).ToString());
		OnSynthesisComplete.Broadcast(Product);
	}
}

FString AMedicalBayModule::GetModuleSpecificTelemetry() const
{
	return FString::Printf(TEXT("BIOPRINTER=%s; BIOPRINTER_PROGRESS=%.2f; FEEDSTOCK=%d; OUTPUT_TRAY=%d"),
		IsSynthesizing() ? TEXT("RUNNING") : TEXT("IDLE"), GetSynthesisProgress(), BioprinterFeedstock, OutputTray.Num());
}
