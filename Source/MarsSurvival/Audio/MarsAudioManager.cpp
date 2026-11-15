#include "Audio/MarsAudioManager.h"

#include "Audio/MarsProceduralSynth.h"
#include "Base/MarsBaseModule.h"
#include "Items/MarsInventoryComponent.h"
#include "Player/EVASuitComponent.h"
#include "Core/MarsUserSettings.h"
#include "Player/MarsCharacter.h"
#include "World/MarsEnvironmentSubsystem.h"
#include "World/MarsStationSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UMarsAudioManager::UMarsAudioManager()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMarsAudioManager::BeginPlay()
{
	Super::BeginPlay();
	Synth = GetOwner() ? GetOwner()->FindComponentByClass<UMarsProceduralSynth>() : nullptr;
	if (Synth)
	{
		Synth->Start();
	}
	const FMarsUserSettings& S = FMarsUserSettings::Get();
	ApplyVolumeSettings(S.MasterVolume, S.SfxVolume, S.AmbientVolume, S.VoiceVolume, S.MusicVolume);
}

float UMarsAudioManager::GetGeigerClickCount() const
{
	return Synth ? static_cast<float>(Synth->GetGeigerClickCount()) : 0.0f;
}

float UMarsAudioManager::PanFor(const FVector& Location) const
{
	const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwner());
	if (!C || !C->GetFirstPersonCamera())
	{
		return 0.0f;
	}
	const UCameraComponent* Cam = C->GetFirstPersonCamera();
	const FVector Dir = (Location - Cam->GetComponentLocation()).GetSafeNormal();
	return FMath::Clamp(FVector::DotProduct(Dir, Cam->GetRightVector()), -1.0f, 1.0f);
}

float UMarsAudioManager::DistanceGain(const FVector& Location, float FalloffCm) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 1.0f;
	}
	const float D = FVector::Dist(Owner->GetActorLocation(), Location);
	return FMath::Clamp(1.0f / (1.0f + FMath::Square(D / FalloffCm)), 0.0f, 1.0f);
}

void UMarsAudioManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AMarsCharacter* C = Cast<AMarsCharacter>(GetOwner());
	if (!Synth || !C)
	{
		return;
	}
	const UPlayerBiometricsComponent* Bio = C->GetBiometrics();
	const UEVASuitComponent* Suit = C->GetSuit();
	const AMarsBaseModule* Module = C->GetCurrentModule();
	const UMarsEnvironmentSubsystem* Env = GetWorld()->GetSubsystem<UMarsEnvironmentSubsystem>();
	const UMarsStationSubsystem* Station = GetWorld()->GetSubsystem<UMarsStationSubsystem>();
	if (!Bio)
	{
		return;
	}

	const int32 Stage = Bio->GetStageIndex();
	const float Hypoxia = Bio->GetHypoxia01();
	const float Exhaustion = 1.0f - Bio->Stamina;
	const float Hypercapnia = FMath::Clamp((Bio->Co2LevelPpm - 5000.0f) / 40000.0f, 0.0f, 1.0f);
	const bool bHelmet = Suit && Suit->IsHelmetSealed();
	TinnitusSpike = FMath::Max(TinnitusSpike - DeltaTime * 0.02f, 0.0f);

	// Heartbeat - muffled in the ears; in Stage 4 it drowns everything else.
	const float HeartGain = Bio->IsDead() ? 0.0f : FMath::Clamp((Stage >= 2 ? 0.08f : 0.0f) + Hypoxia * 0.5f + Bio->PanicLevel * 0.3f
		+ (Stage >= 4 ? 0.6f : 0.0f) + Exhaustion * 0.12f, 0.0f, 1.0f);
	Synth->SetHeartbeat(Bio->HeartRateBpm, HeartGain);

	// Breathing / gasping (louder inside a sealed helmet).
	const float BreathGain = Bio->IsDead() ? 0.0f : FMath::Clamp(0.04f + Exhaustion * 0.25f + (Stage >= 2 ? 0.12f : 0.0f) + Hypercapnia * 0.2f + (bHelmet ? 0.1f : 0.0f), 0.0f, 0.6f);
	const float Gasp = FMath::Clamp((Stage >= 2 ? 0.45f : 0.0f) + Hypercapnia + Exhaustion * 0.5f, 0.0f, 1.0f);
	Synth->SetBreathing(10.0f + Bio->HeartRateBpm / 6.0f + Exhaustion * 10.0f, BreathGain, Gasp);

	// Tinnitus: faint metallic ringing from Stage 1, spikes on hallucinations.
	static const float StageTinnitus[] = { 0.0f, 0.004f, 0.008f, 0.015f, 0.02f };
	Synth->SetTinnitus(StageTinnitus[Stage] + TinnitusSpike);

	// Geiger-Muller: exponential response to dose rate, only if a dosimeter is carried.
	const bool bHasDosimeter = C->GetInventory() && C->GetInventory()->HasItem(EMarsItemType::RadiationDosimeter);
	const float Rate = Bio->CurrentDoseRateMsvPerHour;
	const float Cps = 0.8f + 40.0f * Rate + 20.0f * (FMath::Exp(3.0f * FMath::Min(Rate, 3.0f)) - 1.0f);
	Synth->SetGeiger(bHasDosimeter ? Cps : 0.0f, bDosimeterHeld ? 0.5f : 0.12f);

	// Constant Cherenkov static in Stage 4 / during an SPE outside.
	float Static = Stage >= 4 ? 25.0f : 0.0f;
	if (Env && Env->IsSolarParticleEventActive() && !Module)
	{
		Static += 10.0f;
	}
	Synth->SetStatic(Static);

	// Ambience, ducked under a pounding heart.
	const float Duck = 1.0f - 0.7f * FMath::Clamp(HeartGain - 0.4f, 0.0f, 1.0f);
	const float Power = Station ? Station->GetPowerAvailability() : 1.0f;
	Synth->SetHum(Module ? 0.05f * Duck : 0.0f, Power);

	float Leak = 0.0f;
	if (Module && Module->GetTotalLeakAreaCm2() > 0.05f)
	{
		Leak = FMath::Clamp(Module->GetTotalLeakAreaCm2() / 4.0f, 0.05f, 1.0f) * FMath::Clamp(Module->CurrentPressureKpa / 101.3f, 0.0f, 1.0f) * 0.3f;
	}
	Synth->SetLeak(Leak * Duck);

	const float Storm = Env ? Env->GetStormIntensity() : 0.0f;
	// Mars air is thin (~0.6 kPa): even storm wind is faint; inside the helmet it is a low rumble.
	Synth->SetWind(Module ? 0.0f : (0.03f + Storm * 0.3f) * (bHelmet ? 0.7f : 1.0f) * Duck);

	// Critical state: suit O2 under 15 %, deep hypoxia, or the Stage 4 countdown -> muffle the world.
	float Critical = FMath::Clamp((Hypoxia - 0.45f) / 0.4f, 0.0f, 1.0f);
	if (Suit && Suit->IsSuitDonned())
	{
		Critical = FMath::Max(Critical, FMath::Clamp((0.15f - Suit->GetO2Fraction01()) / 0.15f, 0.0f, 1.0f));
	}
	if (Stage >= 4)
	{
		Critical = FMath::Max(Critical, 0.6f);
	}
	CriticalFactor = Bio->IsDead() ? 0.0f : Critical;
	Synth->SetMuffle(CriticalFactor * 0.9f);
	if (CriticalFactor > 0.0f)
	{
		Synth->SetHeartbeat(Bio->HeartRateBpm, FMath::Max(HeartGain, 0.35f + 0.5f * CriticalFactor));
	}

	Synth->SetMasterGain(MasterVolume);
	Synth->SetBusGains(BusSfx, BusAmbient, BusVoice, BusMusic);
}

void UMarsAudioManager::ApplyVolumeSettings(float Master, float Sfx, float Ambient, float Voice, float Music)
{
	MasterVolume = Master;
	BusSfx = Sfx;
	BusAmbient = Ambient;
	BusVoice = Voice;
	BusMusic = Music;
	if (Synth)
	{
		Synth->SetMasterGain(MasterVolume);
		Synth->SetBusGains(BusSfx, BusAmbient, BusVoice, BusMusic);
	}
}

// ----------------------------------------------------------------------------
// One-shots
// ----------------------------------------------------------------------------

void UMarsAudioManager::PlayGunshot() { if (Synth) { Synth->Trigger(EMarsSynthEvent::Gunshot, 0.9f, 0.15f); } }
void UMarsAudioManager::PlayDryFire() { if (Synth) { Synth->Trigger(EMarsSynthEvent::DryFire, 0.6f, 0.2f); } }
void UMarsAudioManager::PlayRatchet() { if (Synth) { Synth->Trigger(EMarsSynthEvent::Ratchet, 0.5f, 0.25f); } }
void UMarsAudioManager::PlaySealantHiss() { if (Synth) { Synth->Trigger(EMarsSynthEvent::SealantHiss, 0.4f, 0.2f); } }
void UMarsAudioManager::PlayUIBeep(float Pitch) { if (Synth) { Synth->Trigger(EMarsSynthEvent::UIBeep, 0.5f, 0.0f, Pitch); } }
void UMarsAudioManager::PlayFootstep(float Gain) { if (Synth) { Synth->Trigger(EMarsSynthEvent::Footstep, 0.25f * Gain, FMath::FRandRange(-0.1f, 0.1f)); } }

void UMarsAudioManager::PlayBulletImpact(FVector Location, bool bMetal)
{
	if (Synth)
	{
		Synth->Trigger(bMetal ? EMarsSynthEvent::BulletImpactMetal : EMarsSynthEvent::BulletImpactSoft, DistanceGain(Location, 1500.0f), PanFor(Location));
		if (bMetal)
		{
			Synth->Trigger(EMarsSynthEvent::LeakBurst, 0.6f * DistanceGain(Location, 1200.0f), PanFor(Location));
		}
	}
}

void UMarsAudioManager::PlayHullGroan(FVector Location, float Intensity)
{
	if (Synth)
	{
		Synth->Trigger(EMarsSynthEvent::HullGroan, FMath::Clamp(Intensity, 0.2f, 1.2f), PanFor(Location) * 0.6f, FMath::FRand());
	}
}

void UMarsAudioManager::PlayAirlockHiss(FVector Location, float DurationSeconds)
{
	if (Synth)
	{
		Synth->Trigger(EMarsSynthEvent::AirlockHiss, 0.7f * DistanceGain(Location, 1500.0f), PanFor(Location), DurationSeconds);
	}
}

void UMarsAudioManager::PlayLeakBurst(FVector Location)
{
	if (Synth)
	{
		Synth->Trigger(EMarsSynthEvent::LeakBurst, DistanceGain(Location, 1500.0f), PanFor(Location));
	}
}

void UMarsAudioManager::PlayPhantom(EPhantomType Type, FVector Location, float Intensity)
{
	if (!Synth)
	{
		return;
	}
	const float Pan = PanFor(Location);
	const float Gain = 0.4f + 0.5f * Intensity;
	switch (Type)
	{
	case EPhantomType::Footsteps:     Synth->Trigger(EMarsSynthEvent::PhantomFootsteps, Gain, Pan); break;
	case EPhantomType::HullKnocking:  Synth->Trigger(EMarsSynthEvent::PhantomKnock, Gain, Pan); break;
	case EPhantomType::MetalScraping: Synth->Trigger(EMarsSynthEvent::PhantomScrape, Gain, Pan); break;
	case EPhantomType::Whisper:       Synth->Trigger(EMarsSynthEvent::PhantomWhisper, Gain, Pan); break;
	case EPhantomType::RadioVoice:    Synth->Trigger(EMarsSynthEvent::PhantomRadioVoice, Gain * 0.8f, 0.0f); break;
	default: break;
	}
	SpikeTinnitus(0.02f * Intensity);
}

void UMarsAudioManager::PlayPhantomShriek(FVector Location)
{
	if (Synth)
	{
		Synth->Trigger(EMarsSynthEvent::PhantomShriek, 0.8f, PanFor(Location));
	}
	SpikeTinnitus(0.05f);
}

void UMarsAudioManager::PlayShuttleRumble(float DurationSeconds)
{
	if (Synth)
	{
		Synth->Trigger(EMarsSynthEvent::ShuttleRumble, 1.0f, -0.3f, DurationSeconds);
	}
}

// ----------------------------------------------------------------------------
// Music
// ----------------------------------------------------------------------------

bool UMarsAudioManager::LoadWav(const FString& Path, TArray<float>& OutStereo, int32& OutRate) const
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 44)
	{
		return false;
	}
	auto U16 = [&](int32 At) { return static_cast<uint16>(Bytes[At] | (Bytes[At + 1] << 8)); };
	auto U32 = [&](int32 At) { return static_cast<uint32>(Bytes[At] | (Bytes[At + 1] << 8) | (Bytes[At + 2] << 16) | (Bytes[At + 3] << 24)); };
	if (FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) != 0)
	{
		return false;
	}

	int32 Format = 0, Channels = 0, Bits = 0, DataOffset = -1, DataSize = 0;
	OutRate = 44100;
	int32 Pos = 12;
	while (Pos + 8 <= Bytes.Num())
	{
		const uint32 ChunkSize = U32(Pos + 4);
		if (FMemory::Memcmp(Bytes.GetData() + Pos, "fmt ", 4) == 0)
		{
			Format = U16(Pos + 8);
			Channels = U16(Pos + 10);
			OutRate = static_cast<int32>(U32(Pos + 12));
			Bits = U16(Pos + 22);
			if (Format == 0xFFFE && ChunkSize >= 26)
			{
				Format = U16(Pos + 32); // WAVE_FORMAT_EXTENSIBLE sub-format
			}
		}
		else if (FMemory::Memcmp(Bytes.GetData() + Pos, "data", 4) == 0)
		{
			DataOffset = Pos + 8;
			DataSize = FMath::Min(static_cast<int32>(ChunkSize), Bytes.Num() - DataOffset);
			break;
		}
		Pos += 8 + static_cast<int32>(ChunkSize) + (ChunkSize & 1);
	}
	if (DataOffset < 0 || Channels < 1 || Channels > 8 || !(Bits == 16 || Bits == 24 || (Bits == 32 && Format == 3)))
	{
		return false;
	}

	const int32 BytesPerSample = Bits / 8;
	const int32 Frames = DataSize / (BytesPerSample * Channels);
	OutStereo.SetNumUninitialized(Frames * 2);
	auto Sample = [&](int32 Offset) -> float
	{
		const uint8* P = Bytes.GetData() + Offset;
		if (Bits == 16) { return static_cast<int16>(P[0] | (P[1] << 8)) / 32768.0f; }
		if (Bits == 24) { const int32 V = (P[0] << 8) | (P[1] << 16) | (P[2] << 24); return (V >> 8) / 8388608.0f; }
		float F; FMemory::Memcpy(&F, P, 4); return F;
	};
	for (int32 i = 0; i < Frames; ++i)
	{
		const int32 Base = DataOffset + i * BytesPerSample * Channels;
		const float L = Sample(Base);
		const float R = Channels > 1 ? Sample(Base + BytesPerSample) : L;
		OutStereo[i * 2] = L;
		OutStereo[i * 2 + 1] = R;
	}
	return Frames > 0;
}

void UMarsAudioManager::PlayIntroMusic(float FadeSeconds)
{
	if (!Synth)
	{
		Synth = GetOwner() ? GetOwner()->FindComponentByClass<UMarsProceduralSynth>() : nullptr;
		if (!Synth)
		{
			return;
		}
		Synth->Start();
	}
	if (!bMusicLoaded)
	{
		bMusicLoaded = true;
		TArray<float> Pcm;
		int32 Rate = 44100;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Audio/IntroTheme.wav");
		if (LoadWav(Path, Pcm, Rate))
		{
			bCustomTrackLoaded = true;
			UE_LOG(LogTemp, Log, TEXT("Intro theme loaded: %s (%d Hz, %.1f s)"), *Path, Rate, Pcm.Num() / 2.0f / Rate);
			Synth->SetMusicPCM(MoveTemp(Pcm), Rate);
		}
	}
	Synth->SetMusicSpeed(SlowedSpeed);
	Synth->SetReverbMix(ReverbWet);
	Synth->SetMusicTarget(0.9f, FadeSeconds);
}

void UMarsAudioManager::StopMusic(float FadeSeconds)
{
	if (Synth)
	{
		Synth->SetMusicTarget(0.0f, FadeSeconds);
	}
}

TArray<float> UMarsAudioManager::GetMusicBands() const
{
	float B[8] = { 0 };
	if (Synth)
	{
		Synth->GetMusicBands(B);
	}
	return TArray<float>(B, 8);
}

void UMarsAudioManager::PlayCherenkovZap(float Strength)
{
	if (Synth)
	{
		Synth->Trigger(EMarsSynthEvent::CherenkovZap, 0.25f + 0.4f * Strength, FMath::FRandRange(-0.8f, 0.8f));
	}
	SpikeTinnitus(0.01f * Strength);
}
