#pragma once

#include "CoreMinimal.h"
#include "Base/MarsBaseModule.h"
#include "CommRelayModule.generated.h"

/**
 * Storage & Comm-Relay. Hosts the station's RTG array (the electrical grid supply) and the
 * laser/radio quantum transceiver. Endgame: install the quantum transistors, repair the
 * transceiver, hold calibration until the optical link locks, then transmit to Earth.
 *
 * Laser links are attenuated by suspended dust, so calibration slows during storms.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API ACommRelayModule : public AMarsBaseModule
{
	GENERATED_BODY()

public:
	ACommRelayModule();

	virtual float GetPowerSupplyW() const override;
	virtual float GetPowerDemandW() const override;
	virtual void TakeBallisticDamageAtLocation(float DamageAmount, FVector ImpactPoint) override;

	UFUNCTION(BlueprintCallable, Category = "Comm-Relay")
	int32 InstallTransistors(int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Comm-Relay")
	void RepairTransceiver(float Amount);

	/** Advance calibration (called every frame while the player holds the terminal). */
	UFUNCTION(BlueprintCallable, Category = "Comm-Relay")
	void Calibrate(float RealDeltaTime);

	/** Fires the quantum link when ready. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "Comm-Relay")
	bool TransmitToEarth();

	UFUNCTION(BlueprintPure, Category = "Comm-Relay")
	bool AreTransistorsInstalled() const { return InstalledTransistors >= RequiredTransistors; }

	UFUNCTION(BlueprintPure, Category = "Comm-Relay")
	bool IsReadyToCalibrate() const;

	UFUNCTION(BlueprintPure, Category = "Comm-Relay")
	bool IsReadyToTransmit() const { return IsReadyToCalibrate() && CalibrationProgress >= 1.0f; }

	/** Human-readable blocking reason for the terminal ("" when ready). */
	UFUNCTION(BlueprintPure, Category = "Comm-Relay")
	FString GetBlockingReason() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comm-Relay")
	int32 RequiredTransistors = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Comm-Relay")
	int32 InstalledTransistors = 0;

	/** Transceiver hardware integrity, 0..1. Bullets in this module damage it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Comm-Relay", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TransceiverIntegrity = 0.4f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Comm-Relay")
	float CalibrationProgress = 0.0f;

	/** Seconds of uninterrupted calibration needed in clear weather. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comm-Relay")
	float CalibrationSeconds = 45.0f;

	/** RTG array electrical output (We). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comm-Relay|Power")
	float RtgArrayOutputW = 16000.0f; // MMRTG array + Kilopower unit: covers the whole ring incl. -80 C nights

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comm-Relay|Power")
	float TransceiverPowerW = 1500.0f;

protected:
	virtual void TickModuleSystems(float RealDeltaTime, float GameDeltaSeconds) override;
	virtual FString GetModuleSpecificTelemetry() const override;

private:
	bool bCalibratingThisFrame = false;
	float CalibrationIdleTime = 0.0f;
};
