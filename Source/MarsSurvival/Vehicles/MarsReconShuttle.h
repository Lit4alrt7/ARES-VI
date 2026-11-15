#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/MarsInteractable.h"
#include "MarsReconShuttle.generated.h"

class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UPointLightComponent;
class AMarsCharacter;

/**
 * RS-2 single-seat Martian VTOL recon shuttle. Walk up and press E to board (the character is stowed in
 * the cockpit and the controller possesses the shuttle); E again near the ground to climb out.
 *
 * Flight model (arcade VTOL, Mars gravity 3.71 m/s^2):
 *  - Hover assist holds altitude; SPACE / C climb / descend.
 *  - W/S pitch the craft to accelerate forward / back, A/D roll into a strafe, mouse yaws, SHIFT boosts.
 *  - L toggles the landing gear; touching down needs the gear out and a gentle vertical speed.
 * Systems: battery (drains with thrust, recharges on the base pad), cabin O2 reserve that tops up the
 * pilot's suit tank and suit battery while seated, radiation-storm radar and waypoint compass on the HUD.
 */
UCLASS(Blueprintable)
class MARSSURVIVAL_API AMarsReconShuttle : public APawn, public IMarsInteractable
{
	GENERATED_BODY()

public:
	AMarsReconShuttle();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// IMarsInteractable
	virtual void Interact_Implementation(AMarsCharacter* InteractingCharacter) override;
	virtual void InteractHold_Implementation(AMarsCharacter* InteractingCharacter, float DeltaTime) override {}
	virtual FText GetInteractionPrompt_Implementation(AMarsCharacter* InteractingCharacter) const override;

	void Board(AMarsCharacter* Character);
	bool TryExit();

	AMarsCharacter* GetPilot() const { return Pilot.Get(); }
	float GetAltitudeMeters() const { return AltitudeCm / 100.0f; }
	float GetSpeedMs() const { return Velocity.Size() / 100.0f; }
	float GetVerticalSpeedMs() const { return Velocity.Z / 100.0f; }
	float GetBattery01() const { return BatteryWh / BatteryCapacityWh; }
	float GetO2Reserve01() const { return O2ReserveLiters / O2ReserveCapacity; }
	bool IsGearDown() const { return bGearDown; }
	bool IsLanded() const { return bLanded; }
	bool IsDocked() const { return bDocked; }
	float GetThrottle01() const { return ThrustLevel; }

	// ---- Orbital operations ----
	bool IsInOrbit() const { return bInOrbit; }
	void EnterOrbit(const FVector& Where);
	void LeaveOrbit(const FVector& Where);
	bool IsRepairHeld() const { return bRepairHeld; }
	/** 0..1 progress of the orbit-burn / de-orbit hold (for the cockpit prompt). */
	float GetBurnHold01() const { return BurnHold / 3.0f; }
	static constexpr float OrbitBurnAltitudeM = 1200.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Hull;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Floodlight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shuttle")
	float BatteryCapacityWh = 9000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shuttle")
	float BatteryWh = 9000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shuttle")
	float O2ReserveCapacity = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shuttle")
	float O2ReserveLiters = 4000.0f;

	/** Where the shuttle counts as docked (base pad): recharges and refills here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shuttle")
	FVector DockLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shuttle")
	float MaxSpeedCmS = 6500.0f;

private:
	void MoveForwardInput(float V) { InForward = V; }
	void MoveRightInput(float V) { InRight = V; }
	void TurnInput(float V) { InYaw = V; }
	void LookUpInput(float V) { InLook = V; }
	void ClimbPressed() { InClimb = 1.0f; }
	void ClimbReleased() { InClimb = 0.0f; }
	void DescendPressed() { InDescend = 1.0f; }
	void DescendReleased() { InDescend = 0.0f; }
	void BoostPressed() { bBoost = true; }
	void BoostReleased() { bBoost = false; }
	void ToggleGear();
	void RepairPressed() { bRepairHeld = true; }
	void RepairReleased() { bRepairHeld = false; }
	void UpdateOrbitFlight(float DeltaTime);
	void ExitPressed() { TryExit(); }
	void PausePressed();
	void ToggleHUDPressed();
	void UpdateFlight(float DeltaTime);
	void UpdateSystems(float DeltaTime);
	void UpdateVisuals(float DeltaTime);
	float GroundHeightBelow(const FVector& At) const;

	TWeakObjectPtr<AMarsCharacter> Pilot;

	float InForward = 0.0f, InRight = 0.0f, InYaw = 0.0f, InLook = 0.0f, InClimb = 0.0f, InDescend = 0.0f;
	bool bBoost = false;
	bool bGearDown = true;
	bool bLanded = true;
	bool bDocked = true;
	FVector Velocity = FVector::ZeroVector;
	float Yaw = 0.0f;
	float VisualPitch = 0.0f;
	float VisualRoll = 0.0f;
	float GearBlend = 1.0f;
	float AltitudeCm = 0.0f;
	float ThrustLevel = 0.0f;
	float CameraPitch = -12.0f;
	float CameraYawOffset = 0.0f;
	float YawRate = 0.0f;
	float LookIdle = 0.0f;
	float WarnCooldown = 0.0f;
	float AutoFlyTime = 0.0f;
	bool bInOrbit = false;
	bool bRepairHeld = false;
	float BurnHold = 0.0f;
};
