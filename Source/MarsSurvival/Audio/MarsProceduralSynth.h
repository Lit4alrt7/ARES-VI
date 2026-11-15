#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "MarsProceduralSynth.generated.h"

/** One-shot procedural sound events. */
UENUM(BlueprintType)
enum class EMarsSynthEvent : uint8
{
	Gunshot,
	BulletImpactMetal,
	BulletImpactSoft,
	DryFire,
	HullGroan,
	AirlockHiss,
	PhantomFootsteps,
	PhantomKnock,
	PhantomScrape,
	PhantomWhisper,
	PhantomRadioVoice,
	CherenkovZap,
	UIBeep,
	Footstep,
	Ratchet,
	SealantHiss,
	LeakBurst,
	PhantomShriek,
	ShuttleRumble
};

/** One unit of A.R.I.A.'s synthetic speech. */
struct FMarsPhoneme
{
	enum class EKind : uint8 { Silence, Vowel, Nasal, Fricative, Plosive, Breath };
	EKind Kind = EKind::Silence;
	float F1 = 500.0f, F2 = 1500.0f, F3 = 2500.0f; // formants (Hz)
	float Duration = 0.1f;                          // seconds
	float Pitch = 92.0f;                            // glottal f0 (Hz)
	float Gain = 1.0f;
};

/**
 * Real-time stereo synthesiser (audio render thread). No sound assets required.
 *
 *  - Continuous layers: heartbeat, breathing, tinnitus, Geiger clicks, static, hum, leak hiss, wind
 *  - One-shots (EMarsSynthEvent)
 *  - Music: user WAV (slowed + hall reverb) or an original procedural theme, with 8-band EQ meters
 *  - Speech: formant / vocoder-style robotic voice for A.R.I.A. with radio band-pass, bit-crush,
 *    rhythmic cadence and a lo-fi beat bed under the transmission
 */
UCLASS(ClassGroup = (MarsSurvival), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UMarsProceduralSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	UMarsProceduralSynth(const FObjectInitializer& ObjectInitializer);

	// ---- Continuous layers (game thread) ----
	void SetHeartbeat(float Bpm, float Gain)      { HeartBpm.store(Bpm); HeartGain.store(Gain); }
	void SetBreathing(float PerMinute, float Gain, float Gasp) { BreathRate.store(PerMinute); BreathGain.store(Gain); BreathGasp.store(Gasp); }
	void SetTinnitus(float Gain)                  { TinnitusGain.store(Gain); }
	void SetGeiger(float ClicksPerSecond, float Gain) { GeigerRate.store(ClicksPerSecond); GeigerGain.store(Gain); }
	void SetStatic(float CracklesPerSecond)       { StaticRate.store(CracklesPerSecond); }
	void SetHum(float Gain, float PowerFraction)  { HumGain.store(Gain); HumPower.store(PowerFraction); }
	void SetLeak(float Gain)                      { LeakGain.store(Gain); }
	void SetWind(float Gain)                      { WindGain.store(Gain); }
	void SetMasterGain(float Gain)                { MasterGain.store(Gain); }
	/** Player volume buses (settings menu), 0..1. */
	void SetBusGains(float Sfx, float Ambient, float Voice, float Music) { SfxBus.store(Sfx); AmbientBus.store(Ambient); VoiceBus.store(Voice); MusicBus.store(Music); }
	/** 0..1: low-pass the world (ambience + one-shots) when the player is near death; heartbeat stays dry. */
	void SetMuffle(float Amount)                  { Muffle.store(Amount); }

	/** Queue a one-shot. Pan -1 (left) .. +1 (right). */
	void Trigger(EMarsSynthEvent Event, float Gain = 1.0f, float Pan = 0.0f, float Param = 0.0f);

	uint32 GetGeigerClickCount() const { return GeigerClicks.load(); }

	// ---- Music ----

	/** Hand over decoded PCM (interleaved stereo float, -1..1). Empty = procedural theme. */
	void SetMusicPCM(TArray<float>&& InterleavedStereo, int32 SourceSampleRate);

	/** Fade music in/out (seconds). */
	void SetMusicTarget(float Gain, float FadeSeconds);

	/** 0.82 = classic "slowed" (tempo and pitch together). */
	void SetMusicSpeed(float Speed) { MusicSpeed.store(Speed); }

	void SetReverbMix(float Wet) { ReverbWet.store(Wet); }

	/** 8 EQ band levels (0..1) for the menu visualiser. */
	void GetMusicBands(float OutBands[8]) const;

	// ---- Speech ----
	void QueueSpeech(TArray<FMarsPhoneme>&& Phonemes);
	bool IsSpeaking() const { return bSpeaking.load(); }
	float GetSpeechLevel() const { return SpeechLevel.load(); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	struct FVoice
	{
		EMarsSynthEvent Type = EMarsSynthEvent::UIBeep;
		float Time = 0.0f;
		float Duration = 0.1f;
		float Gain = 1.0f;
		float Pan = 0.0f;
		float Param = 0.0f;
		float Phase = 0.0f;
		float Phase2 = 0.0f;
		float Lp = 0.0f, Lp2 = 0.0f, Hp = 0.0f, Band = 0.0f, Low = 0.0f;
		uint32 Seed = 1;
	};

	struct FComb
	{
		TArray<float> Buffer;
		int32 Index = 0;
		float Store = 0.0f;
		float Process(float In, float Feedback, float Damp);
	};

	struct FAllPass
	{
		TArray<float> Buffer;
		int32 Index = 0;
		float Process(float In);
	};

	float RenderVoice(FVoice& V, float Dt);
	float Noise();
	static float NoiseFrom(uint32& State);
	void RenderMusic(float& OutL, float& OutR, float Dt);
	float RenderProceduralTheme(float Dt, float& OutR);
	float RenderSpeech(float Dt);
	float RenderBeat(float Dt);

	// Audio-thread state
	float SR = 48000.0f;
	TArray<FVoice> Voices;
	uint32 RngState = 22695477u;
	float HeartPhase = 0.0f, BreathPhase = 0.0f, TinPhaseA = 0.0f, TinPhaseB = 0.0f, HumPhase = 0.0f;
	float WindLp = 0.0f, WindLp2 = 0.0f, WindLfo = 0.0f;
	float LeakHpPrev = 0.0f, LeakHpOut = 0.0f, LeakLp = 0.0f;
	float BreathLp = 0.0f, BreathBand = 0.0f, BreathLow = 0.0f;
	float FanLp = 0.0f, ClickEnv = 0.0f, ClickPhase = 0.0f, StaticEnv = 0.0f;
	float SmoothHeart = 0.0f, SmoothBreath = 0.0f, SmoothTin = 0.0f, SmoothHum = 0.0f, SmoothLeak = 0.0f, SmoothWind = 0.0f;

	// Music
	TArray<float> MusicPCM;
	int32 MusicSourceRate = 44100;
	double MusicPos = 0.0;
	float MusicGainNow = 0.0f;
	float MusicTime = 0.0f;
	float ThemePhases[8] = { 0 };
	float ThemeLp[2] = { 0 };
	float ArpPhase = 0.0f, ArpEnv = 0.0f, ArpFreq = 440.0f;
	float SubPhase = 0.0f;
	FComb CombsL[4], CombsR[4];
	FAllPass AllPassL[2], AllPassR[2];
	float BandLow[8] = { 0 }, BandBand[8] = { 0 }, BandEnv[8] = { 0 };

	// Speech
	TArray<FMarsPhoneme> SpeechQueue;
	int32 SpeechIndex = 0;
	float PhonemeTime = 0.0f;
	float GlottalPhase = 0.0f;
	float CurF1 = 500.0f, CurF2 = 1500.0f, CurF3 = 2500.0f, CurAmp = 0.0f, CurNoise = 0.0f;
	float FormLow[3] = { 0 }, FormBand[3] = { 0 };
	float RadioHp = 0.0f, RadioHpPrev = 0.0f, RadioLp = 0.0f;
	float CrushHold = 0.0f;
	int32 CrushCounter = 0;
	float SpeechTail = 0.0f;
	float BeatTime = 0.0f;
	float KickPhase = 0.0f;
	float SpeechEnvFollow = 0.0f;

	// Game -> audio parameters
	std::atomic<float> HeartBpm{ 75.0f };
	std::atomic<float> HeartGain{ 0.0f };
	std::atomic<float> BreathRate{ 14.0f };
	std::atomic<float> BreathGain{ 0.0f };
	std::atomic<float> BreathGasp{ 0.0f };
	std::atomic<float> TinnitusGain{ 0.0f };
	std::atomic<float> GeigerRate{ 0.0f };
	std::atomic<float> GeigerGain{ 0.0f };
	std::atomic<float> StaticRate{ 0.0f };
	std::atomic<float> HumGain{ 0.0f };
	std::atomic<float> HumPower{ 1.0f };
	std::atomic<float> LeakGain{ 0.0f };
	std::atomic<float> WindGain{ 0.0f };
	std::atomic<float> MasterGain{ 0.8f };
	std::atomic<float> SfxBus{ 1.0f };
	std::atomic<float> AmbientBus{ 0.8f };
	std::atomic<float> VoiceBus{ 1.0f };
	std::atomic<float> MusicBus{ 0.8f };
	std::atomic<float> Muffle{ 0.0f };
	float SmoothMuffle = 0.0f;
	float MuffleL = 0.0f;
	float MuffleR = 0.0f;
	std::atomic<float> MusicTarget{ 0.0f };
	std::atomic<float> MusicFade{ 2.0f };
	std::atomic<float> MusicSpeed{ 0.82f };
	std::atomic<float> ReverbWet{ 0.45f };
	std::atomic<uint32> GeigerClicks{ 0 };
	std::atomic<bool> bSpeaking{ false };
	std::atomic<float> SpeechLevel{ 0.0f };
	std::atomic<float> Bands[8];
};
