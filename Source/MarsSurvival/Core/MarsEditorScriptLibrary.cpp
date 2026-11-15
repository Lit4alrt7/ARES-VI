#include "Core/MarsEditorScriptLibrary.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant3Vector.h"

bool UMarsEditorScriptLibrary::ConnectWorldPositionOffset(UMaterial* Material, UMaterialExpression* Expression)
{
#if WITH_EDITORONLY_DATA
	if (!Material || !Expression)
	{
		return false;
	}
	Material->PreEditChange(nullptr);
	// Shadow depth passes render from the light's point of view, where the camera-relative PS1 vertex
	// snap lands elsewhere -> shimmering shadow acne. Route the offset through a "ShadowPassSwitch"
	// (UMaterialExpressionShadowReplace, not exported from Engine -> created and wired by reflection)
	// so shadow casters use the un-jittered surface.
	UMaterialExpression* Source = Expression;
	if (UClass* SwitchClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionShadowReplace")))
	{
		UMaterialExpression* Switch = NewObject<UMaterialExpression>(Material, SwitchClass);
		UMaterialExpressionConstant3Vector* Zero = NewObject<UMaterialExpressionConstant3Vector>(Material);
		Zero->Constant = FLinearColor::Black;
		Switch->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX + 220;
		Switch->MaterialExpressionEditorY = Expression->MaterialExpressionEditorY;
		Zero->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX;
		Zero->MaterialExpressionEditorY = Expression->MaterialExpressionEditorY + 160;
		Material->GetExpressionCollection().AddExpression(Switch);
		Material->GetExpressionCollection().AddExpression(Zero);
		auto Input = [Switch, SwitchClass](const TCHAR* Name) -> FExpressionInput*
		{
			const FStructProperty* Prop = FindFProperty<FStructProperty>(SwitchClass, Name);
			return Prop ? Prop->ContainerPtrToValuePtr<FExpressionInput>(Switch) : nullptr;
		};
		FExpressionInput* Default = Input(TEXT("Default"));
		FExpressionInput* Shadow = Input(TEXT("Shadow"));
		if (Default && Shadow)
		{
			Default->Connect(0, Expression);
			Shadow->Connect(0, Zero);
			Source = Switch;
		}
	}
	Material->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Source);
	Material->PostEditChange();
	return true;
#else
	return false;
#endif
}
