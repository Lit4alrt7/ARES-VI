#include "Audio/AriaVoiceComponent.h"

#include "Audio/MarsProceduralSynth.h"
#include "GameFramework/Actor.h"

namespace
{
	using EKind = FMarsPhoneme::EKind;

	FMarsPhoneme Make(EKind Kind, float F1, float F2, float F3, float Dur, float Pitch, float Gain = 1.0f)
	{
		FMarsPhoneme P;
		P.Kind = Kind; P.F1 = F1; P.F2 = F2; P.F3 = F3; P.Duration = Dur; P.Pitch = Pitch; P.Gain = Gain;
		return P;
	}

	FString SpellNumber(TCHAR C)
	{
		static const TCHAR* Words[] = { TEXT("ZERO"), TEXT("WUN"), TEXT("TOO"), TEXT("THREE"), TEXT("FOR"), TEXT("FIVE"), TEXT("SIKS"), TEXT("SEVEN"), TEXT("AIT"), TEXT("NINER") };
		return Words[C - TEXT('0')];
	}

	/** Uppercase, numbers spelled out, symbols to words, punctuation kept as pause markers. */
	FString Normalise(FString Text, int32 MaxChars)
	{
		Text.ReplaceInline(TEXT("A.R.I.A.:"), TEXT(""));
		Text.ReplaceInline(TEXT("A.R.I.A."), TEXT("ARIA"));
		Text.ReplaceInline(TEXT("%"), TEXT(" PERCENT"));
		Text.ReplaceInline(TEXT("mSv"), TEXT(" MILLISIEVERT"));
		Text.ReplaceInline(TEXT("kPa"), TEXT(" KILOPASCAL"));
		Text.ReplaceInline(TEXT("ppm"), TEXT(" P P M"));
		Text.ReplaceInline(TEXT("CO2"), TEXT("C O TWO"));
		Text.ReplaceInline(TEXT("MetHb"), TEXT("MET HEMOGLOBIN"));
		Text = Text.ToUpper();
		if (Text.Len() > MaxChars)
		{
			int32 Cut = MaxChars;
			for (int32 i = MaxChars; i > MaxChars / 2; --i)
			{
				if (Text[i] == TEXT('.')) { Cut = i + 1; break; }
			}
			Text = Text.Left(Cut);
		}

		FString Out;
		for (int32 i = 0; i < Text.Len(); ++i)
		{
			const TCHAR C = Text[i];
			if (FChar::IsDigit(C))
			{
				Out += TEXT(" ") + SpellNumber(C) + TEXT(" ");
			}
			else if (C == TEXT('.') && i + 1 < Text.Len() && FChar::IsDigit(Text[i + 1]))
			{
				Out += TEXT(" POINT ");
			}
			else
			{
				Out.AppendChar(C);
			}
		}
		return Out;
	}
}

UAriaVoiceComponent::UAriaVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UMarsProceduralSynth* UAriaVoiceComponent::GetSynth() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMarsProceduralSynth>() : nullptr;
}

bool UAriaVoiceComponent::IsSpeaking() const
{
	const UMarsProceduralSynth* Synth = GetSynth();
	return Synth && Synth->IsSpeaking();
}

void UAriaVoiceComponent::Speak(const FString& Text)
{
	UMarsProceduralSynth* Synth = GetSynth();
	if (!Synth || !bVoiceEnabled)
	{
		return;
	}

	const FString Clean = Normalise(Text, MaxCharacters);
	const float Sixteenth = 60.0f / FMath::Max(CadenceBpm, 40.0f) / 4.0f;

	TArray<FMarsPhoneme> Out;
	float Clock = 0.0f;
	auto Push = [&](const FMarsPhoneme& P) { Out.Add(P); Clock += P.Duration; };
	auto PadToGrid = [&]()
	{
		// Rhythmic cadence: every word starts on the next 16th-note boundary.
		const float Next = FMath::CeilToFloat(Clock / Sixteenth - 0.001f) * Sixteenth;
		if (Next - Clock > 0.001f)
		{
			Push(Make(EKind::Silence, 500, 1500, 2500, Next - Clock, BasePitchHz));
		}
	};

	TArray<FString> Words;
	Clean.ParseIntoArrayWS(Words);
	for (int32 w = 0; w < Words.Num(); ++w)
	{
		FString Word = Words[w];
		const bool bSentenceEnd = Word.EndsWith(TEXT(".")) || Word.EndsWith(TEXT("!")) || Word.EndsWith(TEXT("?"));
		const bool bComma = Word.EndsWith(TEXT(",")) || Word.EndsWith(TEXT(";")) || Word.EndsWith(TEXT(":"));
		// Deadpan: dead flat, then a villainous drop on the last word of a sentence.
		const float Pitch = BasePitchHz * (bSentenceEnd ? 0.86f : 1.0f) * (w % 4 == 3 ? 0.97f : 1.0f);

		PadToGrid();
		for (int32 i = 0; i < Word.Len(); ++i)
		{
			const TCHAR C = Word[i];
			const TCHAR Next = i + 1 < Word.Len() ? Word[i + 1] : TEXT(' ');
			switch (C)
			{
			case 'A': Push(Make(EKind::Vowel, 760, 1250, 2550, 0.12f, Pitch)); break;
			case 'E': Push(Make(EKind::Vowel, 520, 1850, 2500, 0.10f, Pitch)); break;
			case 'I': Push(Make(EKind::Vowel, 320, 2250, 2950, 0.10f, Pitch)); break;
			case 'O': Push(Make(EKind::Vowel, 520, 900, 2400, 0.13f, Pitch)); break;
			case 'U': Push(Make(EKind::Vowel, 340, 820, 2300, 0.11f, Pitch)); break;
			case 'Y': Push(Make(EKind::Vowel, 300, 2200, 2900, 0.08f, Pitch)); break;
			case 'L': Push(Make(EKind::Vowel, 360, 1150, 2600, 0.06f, Pitch, 0.7f)); break;
			case 'R': Push(Make(EKind::Vowel, 430, 1250, 1650, 0.06f, Pitch, 0.7f)); break;
			case 'W': Push(Make(EKind::Vowel, 300, 700, 2200, 0.06f, Pitch, 0.7f)); break;
			case 'M': Push(Make(EKind::Nasal, 260, 1100, 2400, 0.07f, Pitch)); break;
			case 'N': Push(Make(EKind::Nasal, 280, 1600, 2600, 0.06f, Pitch)); break;
			case 'S': case 'Z': case 'C': case 'X':
				if (C == 'S' && Next == 'H') { Push(Make(EKind::Fricative, 2200, 3000, 4000, 0.09f, Pitch, 0.8f)); ++i; }
				else { Push(Make(EKind::Fricative, 3800, 5200, 6500, 0.08f, Pitch, 0.7f)); }
				break;
			case 'F': case 'V': Push(Make(EKind::Fricative, 1600, 3200, 5000, 0.07f, Pitch, 0.5f)); break;
			case 'T':
				if (Next == 'H') { Push(Make(EKind::Fricative, 1800, 3400, 5200, 0.07f, Pitch, 0.4f)); ++i; }
				else { Push(Make(EKind::Plosive, 400, 1800, 3500, 0.05f, Pitch, 0.9f)); }
				break;
			case 'P': case 'B': case 'D': case 'K': case 'G': case 'Q': case 'J':
				Push(Make(EKind::Plosive, 400, 1500, 3000, 0.05f, Pitch, 0.8f)); break;
			case 'H': Push(Make(EKind::Breath, 600, 1400, 2500, 0.05f, Pitch)); break;
			default: break;
			}
		}
		if (bSentenceEnd)
		{
			Push(Make(EKind::Silence, 500, 1500, 2500, Sixteenth * 3.0f, BasePitchHz));
		}
		else if (bComma)
		{
			Push(Make(EKind::Silence, 500, 1500, 2500, Sixteenth * 1.5f, BasePitchHz));
		}
	}

	Synth->QueueSpeech(MoveTemp(Out));
}
