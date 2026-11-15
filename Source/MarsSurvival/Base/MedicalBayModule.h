#pragma once

#include "CoreMinimal.h"
#include "Base/MarsBaseModule.h"
#include "Items/MarsItemTypes.h"
#include "MedicalBayModule.generated.h"

class UPlayerBiometricsComponent;
class UMarsInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisComplete, EMarsItemType, Product);

/**
 * Medical Bay: CO-oximetry blood analyser and a pharmaceutical bioprinter that synthesises
 * Methylene Blue (methaemoglobinaemia antidote) and Potassium Iodide (thyroid NIS protection)
 * from feedstock cartridges.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMedicalBayModule : public AMarsBaseModule
{
	GENERATED_BODY()

public:
	AMedicalBayModule();

	virtual float GetPowerDemandW() const override;

	/** Full blood panel as multi-line text (shown on the analyser screen). */
	UFUNCTION(BlueprintCallable, Category = "Medical Bay")
	FString RunBloodPanel(const UPlayerBiometricsComponent* Biometrics) const;

	/** Recipes the bioprinter can currently produce (targeted drugs unlock after the Xenolab assay). */
	UFUNCTION(BlueprintPure, Category = "Medical Bay")
	TArray<EMarsItemType> GetAvailableRecipes() const;

	/** Queue a synthesis of one of GetAvailableRecipes(). */
	UFUNCTION(BlueprintCallable, Category = "Medical Bay")
	bool StartSynthesis(EMarsItemType Product);

	/** Moves finished products into the inventory. Returns number collected. */
	UFUNCTION(BlueprintCallable, Category = "Medical Bay")
	int32 CollectOutput(UMarsInventoryComponent* Inventory);

	UFUNCTION(BlueprintPure, Category = "Medical Bay")
	bool IsSynthesizing() const { return ActiveRecipe != EMarsItemType::None; }

	UFUNCTION(BlueprintPure, Category = "Medical Bay")
	float GetSynthesisProgress() const;

	UFUNCTION(BlueprintPure, Category = "Medical Bay")
	int32 GetPendingOutputCount() const { return OutputTray.Num(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medical Bay")
	int32 BioprinterFeedstock = 10;

	/** Game hours per synthesis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medical Bay")
	float SynthesisDurationHours = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medical Bay")
	float BioprinterPowerW = 400.0f;

	UPROPERTY(BlueprintAssignable, Category = "Medical Bay")
	FOnSynthesisComplete OnSynthesisComplete;

protected:
	virtual void TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds) override;
	virtual FString GetModuleSpecificTelemetry() const override;

private:
	EMarsItemType ActiveRecipe = EMarsItemType::None;
	float SynthesisRemainingHours = 0.0f;

	UPROPERTY()
	TArray<EMarsItemType> OutputTray;
};
