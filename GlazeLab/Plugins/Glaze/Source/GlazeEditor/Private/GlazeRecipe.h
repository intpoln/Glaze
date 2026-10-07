#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GlazeRecipe.generated.h"

class UTexture2D;

UENUM()
enum class EGlazeChannel : uint8
{
	R, G, B, A
};

UENUM()
enum class EGlazeBlend : uint8
{
	Opaque,
	Masked
};

/** Texture slots Glaze can fill. Order matters: it is the display order of the slot list. */
enum class EGlazeSlot : uint8
{
	BaseColor,
	Normal,
	Roughness,
	Metallic,
	AmbientOcclusion,
	Specular,
	Opacity,
	Emissive,
	Height,
	Num
};

/** Remap of a grayscale map: invert -> contrast around 0.5 + bias -> fit into [Min, Max]. Defaults leave the map unchanged. */
USTRUCT()
struct FGlazeRemap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Remap")
	bool bInvert = false;

	UPROPERTY(EditAnywhere, Category = "Remap", meta = (UIMin = "0", UIMax = "4", ClampMin = "0"))
	float Contrast = 1.f;

	UPROPERTY(EditAnywhere, Category = "Remap", meta = (UIMin = "-1", UIMax = "1"))
	float Bias = 0.f;

	UPROPERTY(EditAnywhere, Category = "Remap", meta = (UIMin = "0", UIMax = "1"))
	float Min = 0.f;

	UPROPERTY(EditAnywhere, Category = "Remap", meta = (UIMin = "0", UIMax = "1"))
	float Max = 1.f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGlazeRecipeChanged, const FPropertyChangedEvent&);

/**
 * Everything the Glaze window edits. Serialized to JSON into the instance's package metadata,
 * so "Edit with Glaze" can reopen it. Nothing of this class ends up referenced by the created assets.
 */
UCLASS(Transient)
class UGlazeRecipe : public UObject
{
	GENERATED_BODY()

public:
	// ---- Output ----
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Name"))
	FString AssetName;

	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Folder", ContentDir))
	FDirectoryPath Folder;

	// ---- Textures ----
	UPROPERTY(EditAnywhere, Category = "Textures")
	TObjectPtr<UTexture2D> BaseColor;

	UPROPERTY(EditAnywhere, Category = "Textures")
	TObjectPtr<UTexture2D> Normal;

	UPROPERTY(EditAnywhere, Category = "Textures")
	TObjectPtr<UTexture2D> Roughness;
	UPROPERTY(EditAnywhere, Category = "Textures", meta = (DisplayName = "  Roughness Channel", EditCondition = "Roughness != nullptr", EditConditionHides))
	EGlazeChannel RoughnessChannel = EGlazeChannel::R;

	UPROPERTY(EditAnywhere, Category = "Textures")
	TObjectPtr<UTexture2D> Metallic;
	UPROPERTY(EditAnywhere, Category = "Textures", meta = (DisplayName = "  Metallic Channel", EditCondition = "Metallic != nullptr", EditConditionHides))
	EGlazeChannel MetallicChannel = EGlazeChannel::R;

	UPROPERTY(EditAnywhere, Category = "Textures", meta = (DisplayName = "Ambient Occlusion"))
	TObjectPtr<UTexture2D> AmbientOcclusion;
	UPROPERTY(EditAnywhere, Category = "Textures", meta = (DisplayName = "  AO Channel", EditCondition = "AmbientOcclusion != nullptr", EditConditionHides))
	EGlazeChannel AmbientOcclusionChannel = EGlazeChannel::R;

	UPROPERTY(EditAnywhere, Category = "Textures")
	TObjectPtr<UTexture2D> Specular;
	UPROPERTY(EditAnywhere, Category = "Textures", meta = (DisplayName = "  Specular Channel", EditCondition = "Specular != nullptr", EditConditionHides))
	EGlazeChannel SpecularChannel = EGlazeChannel::R;

	/** Empty = alpha of Base Color (used only when Blend Mode is Masked). */
	UPROPERTY(EditAnywhere, Category = "Textures")
	TObjectPtr<UTexture2D> Opacity;
	UPROPERTY(EditAnywhere, Category = "Textures", meta = (DisplayName = "  Opacity Channel", EditCondition = "Opacity != nullptr", EditConditionHides))
	EGlazeChannel OpacityChannel = EGlazeChannel::R;

	UPROPERTY(EditAnywhere, Category = "Textures")
	TObjectPtr<UTexture2D> Emissive;

	/** Height is recognized but not used by the material yet. */
	UPROPERTY(EditAnywhere, Category = "Textures", AdvancedDisplay)
	TObjectPtr<UTexture2D> Height;

	/** Fix sRGB / compression of the assigned textures so they match their slot (Normal -> Normalmap, grayscale maps -> Masks). Changes the texture assets. */
	UPROPERTY(EditAnywhere, Category = "Textures")
	bool bFixTextureSettings = true;

	// ---- UV ----
	UPROPERTY(EditAnywhere, Category = "UV", meta = (UIMin = "0.01", UIMax = "16"))
	float Tiling = 1.f;

	UPROPERTY(EditAnywhere, Category = "UV", meta = (DisplayName = "Tiling X/Y"))
	FVector2D TilingXY = FVector2D(1, 1);

	UPROPERTY(EditAnywhere, Category = "UV")
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "UV", meta = (UIMin = "-180", UIMax = "180", Units = "Degrees"))
	float Rotation = 0.f;

	// ---- Base Color ----
	/** Multiplies the base color. Without a Base Color texture this is the color. */
	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (HideAlphaChannel))
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (DisplayName = "Color Correction"))
	bool bColorCorrection = false;

	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (EditCondition = "bColorCorrection", EditConditionHides, UIMin = "0", UIMax = "1"))
	float Desaturation = 0.f;

	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (EditCondition = "bColorCorrection", EditConditionHides, UIMin = "0", UIMax = "3", ClampMin = "0"))
	float Contrast = 1.f;

	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (EditCondition = "bColorCorrection", EditConditionHides, UIMin = "0", UIMax = "3", ClampMin = "0"))
	float Brightness = 1.f;

	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (EditCondition = "bColorCorrection", EditConditionHides, UIMin = "0", UIMax = "1"))
	float HueShift = 0.f;

	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (EditCondition = "bColorCorrection", EditConditionHides, HideAlphaChannel))
	FLinearColor OverlayColor = FLinearColor(0.5f, 0.5f, 0.5f);

	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (EditCondition = "bColorCorrection", EditConditionHides, UIMin = "0", UIMax = "1"))
	float OverlayAmount = 0.f;

	/** Lerp to Color: the color the base color is blended towards. */
	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (EditCondition = "bColorCorrection", EditConditionHides, HideAlphaChannel))
	FLinearColor LerpColor = FLinearColor::White;

	/** Lerp to Color: 0 = original color, 1 = Lerp Color. */
	UPROPERTY(EditAnywhere, Category = "Base Color", meta = (EditCondition = "bColorCorrection", EditConditionHides, UIMin = "0", UIMax = "1"))
	float LerpAmount = 0.f;

	// ---- Normal ----
	UPROPERTY(EditAnywhere, Category = "Normal", meta = (UIMin = "0", UIMax = "3"))
	float NormalStrength = 1.f;

	/** For OpenGL-style normal maps (green up). */
	UPROPERTY(EditAnywhere, Category = "Normal")
	bool bFlipGreen = false;

	// ---- Grayscale channels: value without a map, remap with a map ----
	UPROPERTY(EditAnywhere, Category = "Roughness", meta = (DisplayName = "Value (no map)", UIMin = "0", UIMax = "1"))
	float RoughnessValue = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Roughness", meta = (DisplayName = "Remap (map)"))
	FGlazeRemap RoughnessRemap;

	UPROPERTY(EditAnywhere, Category = "Metallic", meta = (DisplayName = "Value (no map)", UIMin = "0", UIMax = "1"))
	float MetallicValue = 0.f;
	UPROPERTY(EditAnywhere, Category = "Metallic", meta = (DisplayName = "Remap (map)"))
	FGlazeRemap MetallicRemap;

	UPROPERTY(EditAnywhere, Category = "Specular", meta = (DisplayName = "Value (no map)", UIMin = "0", UIMax = "1"))
	float SpecularValue = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Specular", meta = (DisplayName = "Remap (map)"))
	FGlazeRemap SpecularRemap;

	UPROPERTY(EditAnywhere, Category = "Ambient Occlusion", meta = (DisplayName = "Value (no map)", UIMin = "0", UIMax = "1"))
	float AmbientOcclusionValue = 1.f;
	UPROPERTY(EditAnywhere, Category = "Ambient Occlusion", meta = (DisplayName = "Remap (map)"))
	FGlazeRemap AmbientOcclusionRemap;

	// ---- Opacity ----
	UPROPERTY(EditAnywhere, Category = "Opacity")
	EGlazeBlend BlendMode = EGlazeBlend::Opaque;

	UPROPERTY(EditAnywhere, Category = "Opacity")
	bool bTwoSided = false;

	UPROPERTY(EditAnywhere, Category = "Opacity", meta = (EditCondition = "BlendMode == EGlazeBlend::Masked", EditConditionHides, UIMin = "0", UIMax = "1"))
	float OpacityClip = 0.3333f;

	/** Multiplies opacity: animate it for a dissolve / fade. */
	UPROPERTY(EditAnywhere, Category = "Opacity", meta = (EditCondition = "BlendMode == EGlazeBlend::Masked", EditConditionHides, UIMin = "0", UIMax = "1"))
	float OpacityAmount = 1.f;

	/** Dithered (stippled) opacity: soft fades with a Masked material, smoothed by TAA/TSR. */
	UPROPERTY(EditAnywhere, Category = "Opacity", meta = (EditCondition = "BlendMode == EGlazeBlend::Masked", EditConditionHides))
	bool bDithered = false;

	UPROPERTY(EditAnywhere, Category = "Opacity", meta = (DisplayName = "Remap (map)", EditCondition = "BlendMode == EGlazeBlend::Masked", EditConditionHides))
	FGlazeRemap OpacityRemap;

	// ---- Emissive ----
	UPROPERTY(EditAnywhere, Category = "Emissive", meta = (DisplayName = "Emissive"))
	bool bEmissive = false;

	UPROPERTY(EditAnywhere, Category = "Emissive", meta = (EditCondition = "bEmissive", EditConditionHides, HideAlphaChannel))
	FLinearColor EmissiveColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Emissive", meta = (EditCondition = "bEmissive", EditConditionHides, UIMin = "0", UIMax = "50", ClampMin = "0"))
	float EmissiveIntensity = 1.f;

	FOnGlazeRecipeChanged OnChanged;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;

	UTexture2D* GetSlot(EGlazeSlot Slot) const;
	void SetSlot(EGlazeSlot Slot, UTexture2D* Texture, EGlazeChannel Channel = EGlazeChannel::R);
	EGlazeChannel GetChannel(EGlazeSlot Slot) const;

	FString ToJson() const;
	bool FromJson(const FString& Json);
};

namespace Glaze
{
	const TCHAR* SlotName(EGlazeSlot Slot);
	/** Grayscale slots are sampled with the Masks sampler and read one channel. */
	bool IsScalarSlot(EGlazeSlot Slot);
}
