#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Player/PlayerBiometricsComponent.h"
#include "MarsAudioManager.generated.h"

class UMarsProceduralSynth;
class AMarsBaseModule;

/**
 * Drives the procedural soundscape from biometrics, suit, module and environment telemetry:
 *  - Geiger-Muller clicks scaling exponentially with local dose rate (when a dosimeter is carried)
 *  - heartbeat (hypoxia / stage 4 drowns out ambience), breathing / gasping, tinnitus spikes
 *  - life-support hum (sags with grid power), air-leak hiss near punctures, storm wind outside
 *  - one-shots: Glock report, bullet impacts, hull groans, airlock hiss, phantom audio, UI beeps
 */
UCLASS(ClassGroup = (MarsSurvival), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UMarsAudioManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UMarsAudioManager();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayGunshot();
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayBulletImpact(FVector Location, bool bMetal);
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayDryFire();
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayRatchet();
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlaySealantHiss();
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayUIBeep(float Pitch = 1.0f);
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayHullGroan(FVector Location, float Intensity = 1.0f);
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayAirlockHiss(FVector Location, float DurationSeconds);
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayLeakBurst(FVector Location);
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayPhantom(EPhantomType Type, FVector Location, float Intensity);
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayCherenkovZap(float Strength);
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayFootstep(float Gain = 1.0f);

	/** Clicks are louder while the dosimeter is in hand. */
	UFUNCTION(BlueprintCallable, Category = "Mars Audio")
	void SetDosimeterHeld(bool bHeld) { bDosimeterHeld = bHeld; }

	UFUNCTION(BlueprintPure, Category = "Mars Audio")
	float GetGeigerClickCount() const;

	/** Tinnitus spike (hallucination / flash). */
	UFUNCTION(BlueprintCallable, Category = "Mars Audio")
	void SpikeTinnitus(float Amount) { TinnitusSpike = FMath::Min(TinnitusSpike + Amount, 0.08f); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterVolume = 0.9f;

	// ---- Music (menu / briefing) ----

	/**
	 * Plays Content/Audio/IntroTheme.wav if present (your own licensed track; 16/24-bit PCM or float WAV),
	 * otherwise the original procedural theme. Always "slowed + reverb": speed 0.82, large hall.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mars Audio|Music")
	void PlayIntroMusic(float FadeSeconds = 3.0f);

	UFUNCTION(BlueprintCallable, Category = "Mars Audio|Music")
	void StopMusic(float FadeSeconds = 3.0f);

	UFUNCTION(BlueprintPure, Category = "Mars Audio|Music")
	TArray<float> GetMusicBands() const;

	UFUNCTION(BlueprintPure, Category = "Mars Audio|Music")
	bool IsUsingCustomTrack() const { return bCustomTrackLoaded; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Audio|Music")
	float SlowedSpeed = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mars Audio|Music")
	float ReverbWet = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayPhantomShriek(FVector Location);
	UFUNCTION(BlueprintCallable, Category = "Mars Audio") void PlayShuttleRumble(float DurationSeconds);

	UMarsProceduralSynth* GetSynth() const { return Synth; }

	/** Settings-menu volumes (0..1). */
	void ApplyVolumeSettings(float Master, float Sfx, float Ambient, float Voice, float Music);

	/** Critical-state factor (0..1): drives the audio muffle and the HUD vignette. */
	float GetCriticalFactor() const { return CriticalFactor; }

protected:
	virtual void BeginPlay() override;

private:
	float PanFor(const FVector& Location) const;
	float DistanceGain(const FVector& Location, float FalloffCm) const;

	UPROPERTY(Transient)
	TObjectPtr<UMarsProceduralSynth> Synth;

	bool LoadWav(const FString& Path, TArray<float>& OutStereo, int32& OutRate) const;

	bool bDosimeterHeld = false;
	bool bCustomTrackLoaded = false;
	bool bMusicLoaded = false;
	float TinnitusSpike = 0.0f;
	float CriticalFactor = 0.0f;
	float BusSfx = 1.0f, BusAmbient = 0.8f, BusVoice = 1.0f, BusMusic = 0.8f;
};
