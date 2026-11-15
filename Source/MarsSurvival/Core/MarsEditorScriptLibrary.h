#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MarsEditorScriptLibrary.generated.h"

class UMaterial;
class UMaterialExpression;

/**
 * Editor scripting helpers used by Content/Python/setup_mars_project.py for things the stock
 * Python API does not expose (MP_WorldPositionOffset is hidden from Python in UE 5.3).
 */
UCLASS()
class MARSSURVIVAL_API UMarsEditorScriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Wires an expression's first output into the material's World Position Offset input. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "Mars|Editor")
	static bool ConnectWorldPositionOffset(UMaterial* Material, UMaterialExpression* Expression);
};
