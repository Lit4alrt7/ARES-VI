#include "Audio/MarsProceduralSynth.h"

namespace
{
	constexpr float TwoPi = 6.28318530718f;

	FORCEINLINE float OnePole(float& State, float In, float Coef) { State += Coef * (In - State); return State; }
	FORCEINLINE float Env(float T, float Attack, float Decay) { return T < Attack ? T / FMath::Max(Attack, 1e-4f) : FMath::Exp(-(T - Attack) * Decay); }
	FORCEINLINE float SoftClip(float X) { return FMath::Clamp(X * (27.0f + X * X) / (27.0f + 9.0f * X * X), -1.0f, 1.0f); }
	FORCEINLINE float Svf(float& Low, float& Band, float In, float Fc, float Q, float SR)
	{
		const float F = 2.0f * FMath::Sin(PI * FMath::Min(Fc, SR * 0.24f) / SR);
		Low += F * Band;
		const float High = In - Low - Q * Band;
		Band += F * High;
		return Band;
	}
	FORCEINLINE float Saw(float& Phase, float Freq, float Dt) { Phase += Freq * Dt; Phase -= FMath::FloorToFloat(Phase); return Phase * 2.0f - 1.0f; }
	FORCEINLINE float MidiToHz(float Note) { return 440.0f * FMath::Pow(2.0f, (Note - 69.0f) / 12.0f); }
}

// ----------------------------------------------------------------------------
// Reverb building blocks (Freeverb-style)
// ----------------------------------------------------------------------------

float UMarsProceduralSynth::FComb::Process(float In, float Feedback, float Damp)
{
	if (Buffer.Num() == 0)
	{
		return 0.0f;
	}
	const float Out = Buffer[Index];
	Store = Out * (1.0f - Damp) + Store * Damp;
	Buffer[Index] = In + Store * Feedback;
	Index = (Index + 1) % Buffer.Num();
	return Out;
}

float UMarsProceduralSynth::FAllPass::Process(float In)
{
	if (Buffer.Num() == 0)
	{
		return In;
	}
	const float Buffered = Buffer[Index];
	Buffer[Index] = In + Buffered * 0.5f;
	Index = (Index + 1) % Buffer.Num();
	return Buffered - In;
}

UMarsProceduralSynth::UMarsProceduralSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
	bAutoActivate = true;
	bIsUISound = true; // keep playing while the game is paused (menus, briefing)
	for (std::atomic<float>& B : Bands)
	{
		B.store(0.0f);
	}
}

bool UMarsProceduralSynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	SR = static_cast<float>(SampleRate > 0 ? SampleRate : 48000);
	Voices.Reserve(48);

	// Freeverb tunings (samples @44.1 kHz), scaled to the device rate; R channel offset for width.
	const int32 CombTune[4] = { 1557, 1617, 1491, 1422 };
	const int32 AllTune[2] = { 556, 441 };
	const float Scale = SR / 44100.0f * 1.25f; // a bit larger room
	for (int32 i = 0; i < 4; ++i)
	{
		CombsL[i].Buffer.Init(0.0f, FMath::RoundToInt(CombTune[i] * Scale));
		CombsR[i].Buffer.Init(0.0f, FMath::RoundToInt((CombTune[i] + 23) * Scale));
	}
	for (int32 i = 0; i < 2; ++i)
	{
		AllPassL[i].Buffer.Init(0.0f, FMath::RoundToInt(AllTune[i] * Scale));
		AllPassR[i].Buffer.Init(0.0f, FMath::RoundToInt((AllTune[i] + 23) * Scale));
	}
	return true;
}

float UMarsProceduralSynth::NoiseFrom(uint32& State)
{
	State ^= State << 13;
	State ^= State >> 17;
	State ^= State << 5;
	return (State & 0xFFFFFF) / 8388607.5f - 1.0f;
}

float UMarsProceduralSynth::Noise()
{
	return NoiseFrom(RngState);
}

// ----------------------------------------------------------------------------
// Game-thread API
// ----------------------------------------------------------------------------

void UMarsProceduralSynth::Trigger(EMarsSynthEvent Event, float Gain, float Pan, float Param)
{
	const uint32 Seed = FMath::Rand() | 1u;
	SynthCommand([this, Event, Gain, Pan, Param, Seed]()
	{
		if (Voices.Num() >= 48)
		{
			Voices.RemoveAt(0);
		}
		FVoice V;
		V.Type = Event;
		V.Gain = Gain;
		V.Pan = FMath::Clamp(Pan, -1.0f, 1.0f);
		V.Param = Param;
		V.Seed = Seed;
		switch (Event)
		{
		case EMarsSynthEvent::Gunshot:            V.Duration = 0.9f; break;
		case EMarsSynthEvent::BulletImpactMetal:  V.Duration = 0.9f; break;
		case EMarsSynthEvent::BulletImpactSoft:   V.Duration = 0.25f; break;
		case EMarsSynthEvent::DryFire:            V.Duration = 0.05f; break;
		case EMarsSynthEvent::HullGroan:          V.Duration = 3.2f; break;
		case EMarsSynthEvent::AirlockHiss:        V.Duration = FMath::Max(Param, 1.0f); break;
		case EMarsSynthEvent::PhantomFootsteps:   V.Duration = 3.4f; break;
		case EMarsSynthEvent::PhantomKnock:       V.Duration = 2.2f; break;
		case EMarsSynthEvent::PhantomScrape:      V.Duration = 2.6f; break;
		case EMarsSynthEvent::PhantomWhisper:     V.Duration = 1.8f; break;
		case EMarsSynthEvent::PhantomRadioVoice:  V.Duration = 3.0f; break;
		case EMarsSynthEvent::CherenkovZap:       V.Duration = 0.14f; break;
		case EMarsSynthEvent::UIBeep:             V.Duration = 0.09f; break;
		case EMarsSynthEvent::Footstep:           V.Duration = 0.16f; break;
		case EMarsSynthEvent::Ratchet:            V.Duration = 0.04f; break;
		case EMarsSynthEvent::SealantHiss:        V.Duration = 0.4f; break;
		case EMarsSynthEvent::LeakBurst:          V.Duration = 1.5f; break;
		case EMarsSynthEvent::PhantomShriek:      V.Duration = 1.1f; break;
		case EMarsSynthEvent::ShuttleRumble:      V.Duration = FMath::Max(Param, 2.0f); break;
		}
		Voices.Add(V);
	});
}

void UMarsProceduralSynth::SetMusicPCM(TArray<float>&& InterleavedStereo, int32 SourceSampleRate)
{
	TSharedPtr<TArray<float>> Data = MakeShared<TArray<float>>(MoveTemp(InterleavedStereo));
	SynthCommand([this, Data, SourceSampleRate]()
	{
		MusicPCM = MoveTemp(*Data);
		MusicSourceRate = FMath::Max(SourceSampleRate, 8000);
		MusicPos = 0.0;
	});
}

void UMarsProceduralSynth::SetMusicTarget(float Gain, float FadeSeconds)
{
	MusicTarget.store(FMath::Clamp(Gain, 0.0f, 1.5f));
	MusicFade.store(FMath::Max(FadeSeconds, 0.05f));
}

void UMarsProceduralSynth::GetMusicBands(float OutBands[8]) const
{
	for (int32 i = 0; i < 8; ++i)
	{
		OutBands[i] = Bands[i].load();
	}
}

void UMarsProceduralSynth::QueueSpeech(TArray<FMarsPhoneme>&& Phonemes)
{
	TSharedPtr<TArray<FMarsPhoneme>> Data = MakeShared<TArray<FMarsPhoneme>>(MoveTemp(Phonemes));
	bSpeaking.store(Data->Num() > 0);
	SynthCommand([this, Data]()
	{
		SpeechQueue = MoveTemp(*Data);
		SpeechIndex = 0;
		PhonemeTime = 0.0f;
		BeatTime = 0.0f;
	});
}

// ----------------------------------------------------------------------------
// One-shot voices
// ----------------------------------------------------------------------------

float UMarsProceduralSynth::RenderVoice(FVoice& V, float Dt)
{
	const float T = V.Time;
	float Out = 0.0f;

	switch (V.Type)
	{
	case EMarsSynthEvent::Gunshot:
	{
		const float N = NoiseFrom(V.Seed);
		const float Crack = (N - OnePole(V.Lp, N, 0.08f)) * FMath::Exp(-T * 60.0f) * 1.2f;
		V.Phase += TwoPi * (55.0f + 240.0f * FMath::Exp(-T * 45.0f)) * Dt;
		const float Thump = FMath::Sin(V.Phase) * FMath::Exp(-T * 14.0f) * 0.9f;
		V.Phase2 += TwoPi * 2350.0f * Dt;
		const float Ring = FMath::Sin(V.Phase2) * FMath::Exp(-T * 30.0f) * 0.12f;
		const float Tail = OnePole(V.Lp2, N, 0.03f) * FMath::Exp(-T * 5.0f) * 0.6f;
		Out = Crack + Thump + Ring + Tail;
		break;
	}
	case EMarsSynthEvent::BulletImpactMetal:
	{
		const float N = NoiseFrom(V.Seed);
		V.Phase += TwoPi * 523.0f * Dt;
		V.Phase2 += TwoPi * 1347.0f * Dt;
		const float Clang = (FMath::Sin(V.Phase) * 0.6f + FMath::Sin(V.Phase2) * 0.35f + FMath::Sin(V.Phase * 5.63f) * 0.15f) * FMath::Exp(-T * 6.0f);
		Out = Clang * 0.6f + N * FMath::Exp(-T * 50.0f) * 0.5f;
		break;
	}
	case EMarsSynthEvent::BulletImpactSoft:
		Out = OnePole(V.Lp, NoiseFrom(V.Seed), 0.15f) * FMath::Exp(-T * 25.0f) * 1.2f;
		break;
	case EMarsSynthEvent::DryFire:
	case EMarsSynthEvent::Ratchet:
	{
		const float N = NoiseFrom(V.Seed);
		V.Phase += TwoPi * (V.Type == EMarsSynthEvent::DryFire ? 1800.0f : 3200.0f) * Dt;
		Out = (N * 0.5f + FMath::Sin(V.Phase) * 0.6f) * FMath::Exp(-T * 120.0f);
		break;
	}
	case EMarsSynthEvent::HullGroan:
	{
		const float Freq = FMath::Lerp(34.0f, 58.0f, FMath::Sin(T / V.Duration * PI)) * (0.9f + V.Param * 0.2f);
		V.Phase += TwoPi * Freq * Dt;
		const float Tone = FMath::Sin(V.Phase) + 0.5f * FMath::Sin(V.Phase * 2.01f) + 0.3f * FMath::Sin(V.Phase * 3.03f);
		const float Trem = 0.7f + 0.3f * FMath::Sin(TwoPi * 6.5f * T);
		const float Amp = FMath::Sin(FMath::Clamp(T / V.Duration, 0.0f, 1.0f) * PI);
		const float N = NoiseFrom(V.Seed);
		const float Creak = (FMath::Frac(T * 7.0f) < 0.08f ? OnePole(V.Lp, N, 0.3f) : 0.0f) * 0.4f;
		Out = (Tone * 0.35f * Trem + Creak) * Amp;
		break;
	}
	case EMarsSynthEvent::AirlockHiss:
	case EMarsSynthEvent::LeakBurst:
	case EMarsSynthEvent::SealantHiss:
	{
		const float N = NoiseFrom(V.Seed);
		const float HpIn = N - OnePole(V.Lp, N, 0.25f);
		const float Amp = V.Type == EMarsSynthEvent::AirlockHiss
			? FMath::Min(T * 4.0f, 1.0f) * FMath::Min((V.Duration - T) * 2.0f, 1.0f) * (0.6f + 0.4f * FMath::Sin(T * 3.0f))
			: Env(T, 0.02f, V.Type == EMarsSynthEvent::LeakBurst ? 2.0f : 6.0f);
		Out = HpIn * Amp * 0.5f;
		break;
	}
	case EMarsSynthEvent::PhantomFootsteps:
	{
		const float StepT = FMath::Fmod(T, 0.62f);
		const float N = NoiseFrom(V.Seed);
		V.Phase += TwoPi * 70.0f * Dt;
		Out = (OnePole(V.Lp, N, 0.05f) * 2.0f + FMath::Sin(V.Phase) * 0.6f) * FMath::Exp(-StepT * 30.0f) * FMath::Min((V.Duration - T) * 2.0f, 1.0f) * 0.8f;
		break;
	}
	case EMarsSynthEvent::PhantomKnock:
	{
		const float KnockT = FMath::Fmod(T, 0.6f);
		V.Phase += TwoPi * 190.0f * Dt;
		V.Phase2 += TwoPi * 437.0f * Dt;
		Out = T < 1.8f ? (FMath::Sin(V.Phase) * 0.7f + FMath::Sin(V.Phase2) * 0.35f + NoiseFrom(V.Seed) * 0.3f) * FMath::Exp(-KnockT * 22.0f) : 0.0f;
		break;
	}
	case EMarsSynthEvent::PhantomScrape:
	{
		const float N = NoiseFrom(V.Seed);
		const float Band = Svf(V.Low, V.Band, N, FMath::Lerp(700.0f, 1700.0f, 0.5f + 0.5f * FMath::Sin(T * 2.3f)), 0.08f, SR);
		const float Rough = 0.5f + 0.5f * FMath::Abs(FMath::Sin(T * 37.0f + FMath::Sin(T * 11.0f)));
		Out = Band * 0.25f * Rough * FMath::Sin(FMath::Clamp(T / V.Duration, 0.0f, 1.0f) * PI);
		break;
	}
	case EMarsSynthEvent::PhantomWhisper:
	{
		const float N = NoiseFrom(V.Seed);
		const float Hp = N - OnePole(V.Lp, N, 0.35f);
		const float Band = Svf(V.Low, V.Band, Hp, 2400.0f + 600.0f * FMath::Sin(T * 9.0f), 0.3f, SR);
		const float Syllable = FMath::Max(FMath::Sin(T * TwoPi * 4.5f), 0.0f);
		Out = (Band * 0.6f + Hp * 0.15f) * Syllable * FMath::Sin(FMath::Clamp(T / V.Duration, 0.0f, 1.0f) * PI) * 0.6f;
		break;
	}
	case EMarsSynthEvent::PhantomRadioVoice:
	{
		const float N = NoiseFrom(V.Seed);
		const float Band = OnePole(V.Lp, N, 0.3f) - OnePole(V.Lp2, N, 0.02f);
		V.Phase += TwoPi * (120.0f + 25.0f * FMath::Sin(T * 3.0f)) * Dt;
		const float Buzz = FMath::Frac(V.Phase / TwoPi) * 2.0f - 1.0f;
		const float Syllable = FMath::Max(FMath::Sin(T * TwoPi * 3.2f + FMath::Sin(T * 5.0f)), 0.0f);
		Out = (Band * 0.35f + Buzz * Syllable * 0.18f) * (FMath::Frac(T * 1.7f) < 0.85f ? 1.0f : 0.1f);
		break;
	}
	case EMarsSynthEvent::PhantomShriek:
	{
		// Inhuman screech: descending chirp, heavy fold-over distortion, noise breath.
		V.Phase += TwoPi * FMath::Lerp(1900.0f, 380.0f, FMath::Sqrt(T / V.Duration)) * Dt;
		V.Phase2 += TwoPi * 37.0f * Dt;
		const float Tone = FMath::Sin(V.Phase + 3.0f * FMath::Sin(V.Phase2));
		const float N = NoiseFrom(V.Seed);
		Out = SoftClip((Tone * 2.5f + N * 0.8f) * 0.8f) * Env(T, 0.02f, 3.0f) * 0.55f;
		break;
	}
	case EMarsSynthEvent::ShuttleRumble:
	{
		// Rocket engine roar heard through the hull: low noise + sub + crackle, swelling then receding.
		const float N = NoiseFrom(V.Seed);
		const float Low = OnePole(V.Lp2, OnePole(V.Lp, N, 0.02f), 0.04f) * 6.0f;
		V.Phase += TwoPi * 32.0f * Dt;
		const float Crackle = FMath::Abs(N) > 0.97f ? N : 0.0f;
		const float X = FMath::Clamp(T / V.Duration, 0.0f, 1.0f);
		const float Shape = FMath::Sin(X * PI) * (X < 0.15f ? X / 0.15f : 1.0f);
		Out = (Low + FMath::Sin(V.Phase) * 0.4f + Crackle * 0.3f) * Shape * 0.6f;
		break;
	}
	case EMarsSynthEvent::CherenkovZap:
		V.Phase += TwoPi * FMath::Lerp(9000.0f, 5000.0f, T / V.Duration) * Dt;
		Out = (FMath::Sin(V.Phase) * 0.3f + NoiseFrom(V.Seed) * 0.2f) * FMath::Exp(-T * 30.0f);
		break;
	case EMarsSynthEvent::UIBeep:
		V.Phase += TwoPi * 1250.0f * FMath::Max(V.Param, 0.3f) * Dt;
		Out = (FMath::Sin(V.Phase) > 0.0f ? 0.18f : -0.18f) * Env(T, 0.003f, 25.0f);
		break;
	case EMarsSynthEvent::Footstep:
		V.Phase += TwoPi * 90.0f * Dt;
		Out = (OnePole(V.Lp, NoiseFrom(V.Seed), 0.06f) * 1.6f + FMath::Sin(V.Phase) * 0.3f) * FMath::Exp(-T * 35.0f);
		break;
	}

	V.Time += Dt;
	return Out * V.Gain;
}

// ----------------------------------------------------------------------------
// Music: user track (slowed) or original procedural theme
// ----------------------------------------------------------------------------

float UMarsProceduralSynth::RenderProceduralTheme(float Dt, float& OutR)
{
	// Original dark-ambient progression (Am - Fmaj7 - Cmaj7 - Em), 5 s per chord.
	static const float Chords[4][3] = { { 57, 60, 64 }, { 53, 57, 64 }, { 48, 55, 59 }, { 52, 55, 59 } };
	const int32 ChordIndex = static_cast<int32>(MusicTime / 5.0f) % 4;
	const float* Chord = Chords[ChordIndex];

	float Pad = 0.0f;
	for (int32 v = 0; v < 3; ++v)
	{
		const float F = MidiToHz(Chord[v]);
		Pad += Saw(ThemePhases[v * 2], F * 0.997f, Dt) + Saw(ThemePhases[v * 2 + 1], F * 1.004f, Dt);
	}
	const float Cut = 0.02f + 0.012f * (0.5f + 0.5f * FMath::Sin(MusicTime * 0.21f));
	const float PadL = OnePole(ThemeLp[0], Pad, Cut) * 0.08f;
	const float PadR = OnePole(ThemeLp[1], Pad, Cut * 0.9f) * 0.08f;

	// Sub bass on the root.
	SubPhase += TwoPi * MidiToHz(Chord[0] - 24.0f) * Dt;
	const float Sub = FMath::Sin(SubPhase) * 0.12f;

	// Slow glassy arpeggio (every 0.75 s).
	const float ArpStep = FMath::Fmod(MusicTime, 0.75f);
	if (ArpStep < Dt)
	{
		const int32 Step = static_cast<int32>(MusicTime / 0.75f);
		ArpFreq = MidiToHz(Chord[Step % 3] + 12.0f * (1 + (Step / 3) % 2));
		ArpEnv = 1.0f;
	}
	ArpPhase += TwoPi * ArpFreq * Dt;
	ArpEnv *= FMath::Exp(-Dt * 2.2f);
	const float Arp = (FMath::Sin(ArpPhase) + 0.3f * FMath::Sin(ArpPhase * 2.0f)) * ArpEnv * 0.07f;

	OutR = PadR + Sub + Arp * 0.8f;
	return PadL + Sub + Arp;
}

void UMarsProceduralSynth::RenderMusic(float& OutL, float& OutR, float Dt)
{
	const float Target = MusicTarget.load();
	const float FadeRate = Dt / MusicFade.load();
	MusicGainNow += FMath::Clamp(Target - MusicGainNow, -FadeRate, FadeRate);
	if (MusicGainNow <= 0.0005f && Target <= 0.0f)
	{
		OutL = OutR = 0.0f;
		return;
	}
	const float Speed = MusicSpeed.load();
	MusicTime += Dt * Speed;

	float L, R;
	const int32 Frames = MusicPCM.Num() / 2;
	if (Frames > 1)
	{
		// Linear-interpolated resampling; speed < 1 lowers tempo and pitch together ("slowed").
		const int32 I0 = static_cast<int32>(MusicPos) % Frames;
		const int32 I1 = (I0 + 1) % Frames;
		const float Frac = static_cast<float>(MusicPos - FMath::FloorToDouble(MusicPos));
		L = FMath::Lerp(MusicPCM[I0 * 2], MusicPCM[I1 * 2], Frac);
		R = FMath::Lerp(MusicPCM[I0 * 2 + 1], MusicPCM[I1 * 2 + 1], Frac);
		MusicPos += static_cast<double>(MusicSourceRate) / SR * Speed;
		if (MusicPos >= Frames)
		{
			MusicPos -= Frames; // loop
		}
	}
	else
	{
		L = RenderProceduralTheme(Dt, R);
	}

	// Big hall reverb (parallel combs -> series all-passes), damped highs.
	const float Wet = ReverbWet.load();
	const float In = (L + R) * 0.5f * 0.35f;
	float RevL = 0.0f, RevR = 0.0f;
	for (int32 i = 0; i < 4; ++i)
	{
		RevL += CombsL[i].Process(In, 0.88f, 0.35f);
		RevR += CombsR[i].Process(In, 0.88f, 0.35f);
	}
	for (int32 i = 0; i < 2; ++i)
	{
		RevL = AllPassL[i].Process(RevL);
		RevR = AllPassR[i].Process(RevR);
	}
	OutL = (L * (1.0f - Wet * 0.5f) + RevL * Wet) * MusicGainNow;
	OutR = (R * (1.0f - Wet * 0.5f) + RevR * Wet) * MusicGainNow;

	// 8-band analyser for the retro EQ display.
	static const float Centers[8] = { 60, 150, 320, 640, 1250, 2500, 5000, 9000 };
	const float Mono = (OutL + OutR) * 0.5f;
	for (int32 b = 0; b < 8; ++b)
	{
		const float Band = Svf(BandLow[b], BandBand[b], Mono, Centers[b], 1.0f, SR);
		BandEnv[b] = FMath::Max(FMath::Abs(Band), BandEnv[b] * (1.0f - Dt * 6.0f));
	}
}

// ----------------------------------------------------------------------------
// Speech: formant synthesis + radio processing + beat bed
// ----------------------------------------------------------------------------

float UMarsProceduralSynth::RenderSpeech(float Dt)
{
	float Target = 0.0f, NoiseAmt = 0.0f;
	float TargetF1 = CurF1, TargetF2 = CurF2, TargetF3 = CurF3, Pitch = 92.0f;
	bool bBurst = false;

	if (SpeechQueue.IsValidIndex(SpeechIndex))
	{
		const FMarsPhoneme& P = SpeechQueue[SpeechIndex];
		Pitch = P.Pitch;
		TargetF1 = P.F1; TargetF2 = P.F2; TargetF3 = P.F3;
		switch (P.Kind)
		{
		case FMarsPhoneme::EKind::Vowel:     Target = P.Gain; break;
		case FMarsPhoneme::EKind::Nasal:     Target = P.Gain * 0.45f; break;
		case FMarsPhoneme::EKind::Fricative: NoiseAmt = P.Gain * 0.6f; break;
		case FMarsPhoneme::EKind::Breath:    NoiseAmt = P.Gain * 0.25f; break;
		case FMarsPhoneme::EKind::Plosive:   bBurst = PhonemeTime < 0.025f; NoiseAmt = bBurst ? P.Gain : 0.0f; break;
		default: break;
		}
		PhonemeTime += Dt;
		if (PhonemeTime >= P.Duration)
		{
			PhonemeTime = 0.0f;
			++SpeechIndex;
		}
		SpeechTail = 1.2f;
	}
	else if (SpeechTail > 0.0f)
	{
		SpeechTail -= Dt;
		if (SpeechTail <= 0.0f)
		{
			bSpeaking.store(false);
		}
	}

	// Smooth articulation.
	const float Glide = 1.0f - FMath::Exp(-Dt * 60.0f);
	CurF1 += (TargetF1 - CurF1) * Glide;
	CurF2 += (TargetF2 - CurF2) * Glide;
	CurF3 += (TargetF3 - CurF3) * Glide;
	CurAmp += (Target - CurAmp) * (1.0f - FMath::Exp(-Dt * 120.0f));
	CurNoise += (NoiseAmt - CurNoise) * (1.0f - FMath::Exp(-Dt * 200.0f));

	// Glottal source: band-limited-ish pulse train (deep monotone).
	GlottalPhase += Pitch * Dt;
	GlottalPhase -= FMath::FloorToFloat(GlottalPhase);
	const float Pulse = FMath::Exp(-GlottalPhase * 9.0f) * 2.0f - 0.22f;
	const float Source = Pulse * CurAmp + Noise() * CurNoise;

	// Three parallel formant resonators.
	float Voice = 0.0f;
	Voice += Svf(FormLow[0], FormBand[0], Source, CurF1, 0.18f, SR) * 1.0f;
	Voice += Svf(FormLow[1], FormBand[1], Source, CurF2, 0.14f, SR) * 0.7f;
	Voice += Svf(FormLow[2], FormBand[2], Source, CurF3, 0.12f, SR) * 0.35f;
	Voice += Noise() * CurNoise * 0.3f; // sibilance

	// Radio transmitter: 300 Hz - 3.4 kHz band, sample-rate reduction + 6-bit crush, ring-mod shimmer.
	RadioHp = 0.97f * (RadioHp + Voice - RadioHpPrev);
	RadioHpPrev = Voice;
	float Radio = OnePole(RadioLp, RadioHp, 0.35f);
	if (++CrushCounter >= 3)
	{
		CrushCounter = 0;
		CrushHold = FMath::RoundToFloat(Radio * 32.0f) / 32.0f;
	}
	Radio = CrushHold * (0.85f + 0.15f * FMath::Sin(TwoPi * 30.0f * BeatTime));

	const bool bActive = SpeechTail > 0.0f;
	const float Static = bActive ? Noise() * 0.012f : 0.0f;
	SpeechEnvFollow = FMath::Max(FMath::Abs(Radio), SpeechEnvFollow * (1.0f - Dt * 8.0f));
	SpeechLevel.store(SpeechEnvFollow);
	return (Radio * 0.9f + Static) * (bActive ? 1.0f : 0.0f);
}

float UMarsProceduralSynth::RenderBeat(float Dt)
{
	// Lo-fi boom-bap bed under transmissions: 88 BPM, kick 1 & 3, snare 2 & 4, swung hats, vinyl crackle.
	if (SpeechTail <= 0.0f)
	{
		return 0.0f;
	}
	BeatTime += Dt;
	const float Beat = 60.0f / 88.0f;
	const float InBar = FMath::Fmod(BeatTime, Beat * 4.0f);
	const float BeatPos = FMath::Fmod(BeatTime, Beat);
	const int32 BeatIdx = static_cast<int32>(InBar / Beat);

	float Out = 0.0f;
	if (BeatIdx == 0 || BeatIdx == 2)
	{
		KickPhase += TwoPi * (48.0f + 90.0f * FMath::Exp(-BeatPos * 30.0f)) * Dt;
		Out += FMath::Sin(KickPhase) * FMath::Exp(-BeatPos * 9.0f) * 0.35f;
	}
	else
	{
		Out += Noise() * FMath::Exp(-BeatPos * 22.0f) * 0.12f;
	}
	const float Half = FMath::Fmod(BeatTime + Beat * 0.08f, Beat * 0.5f);
	Out += Noise() * FMath::Exp(-Half * 90.0f) * 0.03f;
	if (Noise() > 0.9993f)
	{
		Out += Noise() * 0.15f;
	}
	return Out * FMath::Clamp(SpeechTail, 0.0f, 1.0f);
}

// ----------------------------------------------------------------------------
// Mixer
// ----------------------------------------------------------------------------

int32 UMarsProceduralSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const int32 Frames = NumSamples / FMath::Max(NumChannels, 1);
	const float Dt = 1.0f / SR;
	const float Smooth = 1.0f - FMath::Exp(-Dt * 8.0f);

	const float HeartBpmV = HeartBpm.load();
	const float HeartGainV = HeartGain.load();
	const float BreathRateV = BreathRate.load();
	const float BreathGainV = BreathGain.load();
	const float BreathGaspV = BreathGasp.load();
	const float TinV = TinnitusGain.load();
	const float GeigerRateV = GeigerRate.load();
	const float GeigerGainV = GeigerGain.load();
	const float StaticRateV = StaticRate.load();
	const float HumGainV = HumGain.load();
	const float HumPowerV = HumPower.load();
	const float LeakV = LeakGain.load();
	const float WindV = WindGain.load();
	const float Master = MasterGain.load();
	const float SfxV = SfxBus.load();
	const float AmbV = AmbientBus.load();
	const float VoiceV = VoiceBus.load();
	const float MusicV = MusicBus.load();
	const float MuffleV = Muffle.load();

	for (int32 f = 0; f < Frames; ++f)
	{
		SmoothHeart += (HeartGainV - SmoothHeart) * Smooth;
		SmoothBreath += (BreathGainV - SmoothBreath) * Smooth;
		SmoothTin += (TinV - SmoothTin) * Smooth;
		SmoothHum += (HumGainV - SmoothHum) * Smooth;
		SmoothLeak += (LeakV - SmoothLeak) * Smooth;
		SmoothWind += (WindV - SmoothWind) * Smooth;

		float L = 0.0f, R = 0.0f;

		// Heartbeat
		HeartPhase += Dt * HeartBpmV / 60.0f;
		if (HeartPhase >= 1.0f) { HeartPhase -= 1.0f; }
		const float BeatT = HeartPhase * 60.0f / FMath::Max(HeartBpmV, 30.0f);
		const float DubT = BeatT - 0.28f;
		float Heart = FMath::Sin(TwoPi * 52.0f * BeatT) * FMath::Exp(-BeatT * 16.0f);
		if (DubT > 0.0f) { Heart += 0.7f * FMath::Sin(TwoPi * 44.0f * DubT) * FMath::Exp(-DubT * 20.0f); }
		Heart *= SmoothHeart * 0.9f;

		// Breathing
		BreathPhase += Dt * BreathRateV / 60.0f;
		if (BreathPhase >= 1.0f) { BreathPhase -= 1.0f; }
		const float Inhale = BreathPhase < 0.42f ? FMath::Sin(BreathPhase / 0.42f * PI) : 0.0f;
		const float Exhale = BreathPhase >= 0.42f ? FMath::Sin((BreathPhase - 0.42f) / 0.58f * PI) * 0.7f : 0.0f;
		const float BN = Noise();
		const float BLp = OnePole(BreathLp, BN, 0.08f + 0.12f * BreathGaspV);
		const float BBand = Svf(BreathLow, BreathBand, BN, 380.0f + 220.0f * BreathGaspV, 0.4f, SR);
		const float Breath = (BLp * 0.6f + BBand * 0.25f * BreathGaspV) * (Inhale + Exhale) * SmoothBreath;

		// Tinnitus
		TinPhaseA += TwoPi * 7800.0f * Dt;
		TinPhaseB += TwoPi * 7806.0f * Dt;
		if (TinPhaseA > TwoPi) { TinPhaseA -= TwoPi; }
		if (TinPhaseB > TwoPi) { TinPhaseB -= TwoPi; }
		const float Tin = (FMath::Sin(TinPhaseA) + FMath::Sin(TinPhaseB)) * 0.5f * SmoothTin;

		// Life-support hum
		HumPhase += TwoPi * (50.0f * FMath::Lerp(0.85f, 1.0f, HumPowerV)) * Dt;
		if (HumPhase > TwoPi) { HumPhase -= TwoPi; }
		const float Fan = OnePole(FanLp, Noise(), 0.02f) * 0.8f;
		const float Hum = (FMath::Sin(HumPhase) * 0.5f + FMath::Sin(HumPhase * 2.0f) * 0.25f + FMath::Sin(HumPhase * 3.0f) * 0.12f + Fan) * SmoothHum * HumPowerV;

		// Leak hiss
		const float LN = Noise();
		LeakHpOut = 0.98f * (LeakHpOut + LN - LeakHpPrev);
		LeakHpPrev = LN;
		const float Leak = OnePole(LeakLp, LeakHpOut, 0.5f) * SmoothLeak * (0.8f + 0.2f * FMath::Sin(HumPhase * 0.37f));

		// Wind
		WindLfo += Dt * 0.23f;
		const float Gust = 0.55f + 0.45f * FMath::Sin(TwoPi * WindLfo) * FMath::Sin(TwoPi * WindLfo * 0.37f);
		const float Wind = OnePole(WindLp2, OnePole(WindLp, Noise(), 0.03f), 0.05f) * 3.0f * SmoothWind * Gust;

		// Geiger + static
		if (Noise() * 0.5f + 0.5f < GeigerRateV * Dt)
		{
			ClickEnv = 1.0f;
			GeigerClicks.fetch_add(1);
		}
		ClickPhase += TwoPi * 3100.0f * Dt;
		const float Click = (Noise() * 0.6f + FMath::Sin(ClickPhase) * 0.5f) * ClickEnv * GeigerGainV;
		ClickEnv *= 0.992f;
		if (Noise() * 0.5f + 0.5f < StaticRateV * Dt)
		{
			StaticEnv = 1.0f;
		}
		const float Static = Noise() * StaticEnv * 0.25f;
		StaticEnv *= 0.97f;

		// A.R.I.A. speech + beat bed (centre)
		const float Speech = RenderSpeech(Dt) + RenderBeat(Dt);

		// Body sounds stay dry; the world (ambience + one-shots) goes through the muffle filter.
		const float Body = (Heart + Breath + Tin) * (0.5f + 0.5f * SfxV);
		const float WorldMix = (Hum + Leak + Static) * AmbV + Click * SfxV;
		float WL = WorldMix + Wind * 0.9f * AmbV;
		float WR = WorldMix + Wind * 1.1f * AmbV;

		// One-shots
		for (int32 v = Voices.Num() - 1; v >= 0; --v)
		{
			FVoice& V = Voices[v];
			const float S = RenderVoice(V, Dt) * SfxV;
			WL += S * FMath::Sqrt(0.5f * (1.0f - V.Pan)) * 1.41f;
			WR += S * FMath::Sqrt(0.5f * (1.0f + V.Pan)) * 1.41f;
			if (V.Time >= V.Duration)
			{
				Voices.RemoveAtSwap(v);
			}
		}

		SmoothMuffle += (MuffleV - SmoothMuffle) * Smooth * 0.25f;
		if (SmoothMuffle > 0.01f)
		{
			const float Coef = FMath::Lerp(1.0f, 0.035f, SmoothMuffle);
			MuffleL += (WL - MuffleL) * Coef;
			MuffleR += (WR - MuffleR) * Coef;
			WL = MuffleL * (1.0f - 0.35f * SmoothMuffle);
			WR = MuffleR * (1.0f - 0.35f * SmoothMuffle);
		}
		else
		{
			MuffleL = WL;
			MuffleR = WR;
		}

		L += Body + WL + Speech * VoiceV;
		R += Body + WR + Speech * VoiceV;

		// Music
		float ML = 0.0f, MR = 0.0f;
		RenderMusic(ML, MR, Dt);
		L += ML * MusicV;
		R += MR * MusicV;

		OutAudio[f * NumChannels] = SoftClip(L * Master);
		if (NumChannels > 1)
		{
			OutAudio[f * NumChannels + 1] = SoftClip(R * Master);
		}
	}

	for (int32 b = 0; b < 8; ++b)
	{
		Bands[b].store(FMath::Clamp(BandEnv[b] * 6.0f, 0.0f, 1.0f));
	}
	return NumSamples;
}
