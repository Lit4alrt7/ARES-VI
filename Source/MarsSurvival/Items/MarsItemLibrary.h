#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/MarsItemTypes.h"
#include "MarsItemLibrary.generated.h"

/** Static item database (works without any data assets). */
UCLASS()
class MARSSURVIVAL_API UMarsItemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Items")
	static FMarsItemInfo GetItemInfo(EMarsItemType ItemType);

	UFUNCTION(BlueprintPure, Category = "Items")
	static bool IsEquippable(EMarsItemType ItemType);

	UFUNCTION(BlueprintPure, Category = "Items")
	static FText GetItemName(EMarsItemType ItemType);
};
