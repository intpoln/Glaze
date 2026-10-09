#include "GlazeRecipe.h"

#include "Engine/Texture2D.h"
#include "JsonObjectConverter.h"

void UGlazeRecipe::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	OnChanged.Broadcast(Event);
}

UTexture2D* UGlazeRecipe::GetSlot(EGlazeSlot Slot) const
{
	switch (Slot)
	{
	case EGlazeSlot::BaseColor:        return BaseColor;
	case EGlazeSlot::Normal:           return Normal;
	case EGlazeSlot::Roughness:        return Roughness;
	case EGlazeSlot::Metallic:         return Metallic;
	case EGlazeSlot::AmbientOcclusion: return AmbientOcclusion;
	case EGlazeSlot::Specular:         return Specular;
	case EGlazeSlot::Opacity:          return Opacity;
	case EGlazeSlot::Emissive:         return Emissive;
	case EGlazeSlot::Height:           return Height;
	default:                           return nullptr;
	}
}

void UGlazeRecipe::SetSlot(EGlazeSlot Slot, UTexture2D* Texture, EGlazeChannel Channel)
{
	switch (Slot)
	{
	case EGlazeSlot::BaseColor:        BaseColor = Texture; break;
	case EGlazeSlot::Normal:           Normal = Texture; break;
	case EGlazeSlot::Roughness:        Roughness = Texture; RoughnessChannel = Channel; break;
	case EGlazeSlot::Metallic:         Metallic = Texture; MetallicChannel = Channel; break;
	case EGlazeSlot::AmbientOcclusion: AmbientOcclusion = Texture; AmbientOcclusionChannel = Channel; break;
	case EGlazeSlot::Specular:         Specular = Texture; SpecularChannel = Channel; break;
	case EGlazeSlot::Opacity:          Opacity = Texture; OpacityChannel = Channel; break;
	case EGlazeSlot::Emissive:         Emissive = Texture; break;
	case EGlazeSlot::Height:           Height = Texture; break;
	default: break;
	}
}

EGlazeChannel UGlazeRecipe::GetChannel(EGlazeSlot Slot) const
{
	switch (Slot)
	{
	case EGlazeSlot::Roughness:        return RoughnessChannel;
	case EGlazeSlot::Metallic:         return MetallicChannel;
	case EGlazeSlot::AmbientOcclusion: return AmbientOcclusionChannel;
	case EGlazeSlot::Specular:         return SpecularChannel;
	case EGlazeSlot::Opacity:          return OpacityChannel;
	default:                           return EGlazeChannel::R;
	}
}

FString UGlazeRecipe::ToJson() const
{
	FString Out;
	FJsonObjectConverter::UStructToJsonObjectString(GetClass(), this, Out, 0, CPF_Transient);
	return Out;
}

bool UGlazeRecipe::FromJson(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		return false;
	}
	// Read into a scratch recipe first: the converter accepts any number for an enum, and a bad file must not
	// leave this recipe half-overwritten.
	UGlazeRecipe* Scratch = NewObject<UGlazeRecipe>(GetTransientPackage());
	if (!FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), GetClass(), Scratch, 0, CPF_Transient))
	{
		return false;
	}
	for (TFieldIterator<FEnumProperty> It(GetClass()); It; ++It)
	{
		const int64 Value = It->GetUnderlyingProperty()->GetSignedIntPropertyValue(It->ContainerPtrToValuePtr<void>(Scratch));
		if (!It->GetEnum()->IsValidEnumValue(Value) || Value == It->GetEnum()->GetMaxEnumValue())
		{
			return false;
		}
	}
	return FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), GetClass(), this, 0, CPF_Transient);
}

namespace Glaze
{
	const TCHAR* SlotName(EGlazeSlot Slot)
	{
		switch (Slot)
		{
		case EGlazeSlot::BaseColor:        return TEXT("Base Color");
		case EGlazeSlot::Normal:           return TEXT("Normal");
		case EGlazeSlot::Roughness:        return TEXT("Roughness");
		case EGlazeSlot::Metallic:         return TEXT("Metallic");
		case EGlazeSlot::AmbientOcclusion: return TEXT("Ambient Occlusion");
		case EGlazeSlot::Specular:         return TEXT("Specular");
		case EGlazeSlot::Opacity:          return TEXT("Opacity");
		case EGlazeSlot::Emissive:         return TEXT("Emissive");
		case EGlazeSlot::Height:           return TEXT("Height");
		default:                           return TEXT("?");
		}
	}

	bool IsScalarSlot(EGlazeSlot Slot)
	{
		return Slot == EGlazeSlot::Roughness || Slot == EGlazeSlot::Metallic || Slot == EGlazeSlot::AmbientOcclusion
			|| Slot == EGlazeSlot::Specular || Slot == EGlazeSlot::Opacity || Slot == EGlazeSlot::Height;
	}
}
