#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AriaVoiceComponent.generated.h"

class UMarsProceduralSynth;

/**
 * A.R.I.A.'s voice: a rule-based text-to-phoneme front end feeding the formant synthesiser in
 * UMarsProceduralSynth. Delivery is a deep, flat, metallic monotone; words land on a 16th-note grid
 * (88 BPM) for a rhythmic, deadpan, sampled-radio cadence, with a small pitch drop at sentence ends,
 * band-limited and bit-crushed like a lo-fi transmission over a quiet boom-bap bed.
 * It is a generic robotic voice - no real person's voice is sampled or cloned.
 */
UCLASS(ClassGroup = (MarsSurvival), meta = (BlueprintSpawnableComponent))
class MARSSURVIVAL_API UAriaVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAriaVoiceComponent();

	/** Speak a line (prefix "A.R.I.A.:" is stripped; long text is truncated at a sentence boundary). */
	UFUNCTION(BlueprintCallable, Category = "ARIA Voice")
	void Speak(const FString& Text);

	UFUNCTION(BlueprintPure, Category = "ARIA Voice")
	bool IsSpeaking() const;

	/** Base pitch of the voice (Hz). Low and flat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARIA Voice")
	float BasePitchHz = 88.0f;

	/** Words per beat grid (BPM of the cadence). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARIA Voice")
	float CadenceBpm = 88.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARIA Voice")
	int32 MaxCharacters = 240;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARIA Voice")
	bool bVoiceEnabled = true;

private:
	UMarsProceduralSynth* GetSynth() const;
};
