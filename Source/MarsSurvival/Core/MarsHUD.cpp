#include "Core/MarsHUD.h"

#include "UI/MarsPlayerHUD.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

void AMarsHUD::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = GetOwningPlayerController())
	{
		HUDWidget = CreateWidget<UMarsPlayerHUD>(PC, UMarsPlayerHUD::StaticClass());
		if (HUDWidget)
		{
			HUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			HUDWidget->AddToViewport(0);
		}
	}
}

void AMarsHUD::SetHUDVisible(bool bVisible)
{
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void AMarsHUD::ToggleMinimalHUD()
{
	if (HUDWidget)
	{
		HUDWidget->SetMinimal(!HUDWidget->IsMinimal());
	}
}
