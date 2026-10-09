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

/** Where the detail noise is projected. */
UENUM()
enum class EGlazeDetailSpace : uint8
{
	/** Mesh UVs: follows the surface, may show UV seams. */
	UV,
	/** World position: seamless, moving objects slide through the noise. */
	World,
	/** Object position: sticks to moving / scaled static meshes. */
	Local UMETA(DisplayName = "Local (Object)"),
	/** Bind-pose position: sticks to animated skeletal meshes (static meshes behave as Local). */
	PreSkinned UMETA(DisplayName = "Pre-Skinned (Skeletal)")
};

/** How the main textures are mapped onto the mesh. */
UENUM()
enum class EGlazeProjection : uint8
{
	/** Mesh UVs. */
	UV,
	/** Triplanar from world position: no UVs needed, seamless; moving objects slide through the texture. */
	World UMETA(DisplayName = "World (Triplanar)"),
	/** Triplanar from object position: sticks to moving / scaled static meshes. */
	Local UMETA(DisplayName = "Object (Triplanar)")
};

/** Which vertices the wind moves. */
UENUM()
enum class EGlazeWindMask : uint8
{
	/** The whole mesh sways the same. */
	None UMETA(DisplayName = "Whole Mesh"),
	/** More towards the top of the object: fixed base, swaying tips (grass, bushes, banners on a pole). */
	Height UMETA(DisplayName = "By Height"),
	/** Red vertex color: 0 = fixed, 1 = full sway (painted in the mesh). */
	VertexColor UMETA(DisplayName = "By Vertex Color (Red)")
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

	/** Parallax: fakes depth by shifting the textures with the view angle (UV projection only). */
	UPROPERTY(EditAnywhere, Category = "Textures")
	TObjectPtr<UTexture2D> Height;
	UPROPERTY(EditAnywhere, Category = "Textures", meta = (DisplayName = "  Height Channel", EditCondition = "Height != nullptr", EditConditionHides))
	EGlazeChannel HeightChannel = EGlazeChannel::R;

	/** Fix sRGB / compression of the assigned textures so they match their slot (Normal -> Normalmap, grayscale maps -> Masks). Changes the texture assets. */
	UPROPERTY(EditAnywhere, Category = "Textures")
	bool bFixTextureSettings = true;

	// ---- UV ----
	/** Triplanar needs no UVs (scans on unwrapped or stretched meshes), but samples every texture three times. */
	UPROPERTY(EditAnywhere, Category = "UV")
	EGlazeProjection Projection = EGlazeProjection::UV;

	/** Size of one texture tile in centimeters. */
	UPROPERTY(EditAnywhere, Category = "UV", meta = (DisplayName = "Texture Size", EditCondition = "Projection != EGlazeProjection::UV", EditConditionHides, UIMin = "10", UIMax = "1000", ClampMin = "0.01", Units = "Centimeters"))
	float TriplanarSize = 100.f;

	UPROPERTY(EditAnywhere, Category = "UV", meta = (EditCondition = "Projection == EGlazeProjection::UV", EditConditionHides, UIMin = "0.01", UIMax = "16"))
	float Tiling = 1.f;

	UPROPERTY(EditAnywhere, Category = "UV", meta = (DisplayName = "Tiling X/Y", EditCondition = "Projection == EGlazeProjection::UV", EditConditionHides))
	FVector2D TilingXY = FVector2D(1, 1);

	UPROPERTY(EditAnywhere, Category = "UV", meta = (EditCondition = "Projection == EGlazeProjection::UV", EditConditionHides))
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "UV", meta = (EditCondition = "Projection == EGlazeProjection::UV", EditConditionHides, UIMin = "-180", UIMax = "180", Units = "Degrees"))
	float Rotation = 0.f;

	/** Parallax depth from the Height map, as a fraction of one texture tile: 0 = flat. White in the map is the surface, black is deepest. */
	UPROPERTY(EditAnywhere, Category = "UV", meta = (DisplayName = "Parallax Depth", EditCondition = "Height != nullptr && Projection == EGlazeProjection::UV", EditConditionHides, UIMin = "0", UIMax = "0.3", ClampMin = "0"))
	float HeightRatio = 0.05f;

	/** Parallax quality: layers the view ray is traced through. More = cleaner edges at grazing angles, but costlier (up to that many Height samples per pixel). */
	UPROPERTY(EditAnywhere, Category = "UV", meta = (DisplayName = "Parallax Steps", EditCondition = "Height != nullptr && Projection == EGlazeProjection::UV", EditConditionHides, UIMin = "1", UIMax = "64", ClampMin = "1", ClampMax = "64"))
	int32 ParallaxSteps = 16;

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

	/** Glow with the Base Color texture (no extra sample). The Emissive Map slot is ignored while this is on. */
	UPROPERTY(EditAnywhere, Category = "Emissive", meta = (DisplayName = "Use Base Color Texture", EditCondition = "bEmissive", EditConditionHides))
	bool bEmissiveFromBaseColor = false;

	UPROPERTY(EditAnywhere, Category = "Emissive", meta = (EditCondition = "bEmissive", EditConditionHides, HideAlphaChannel))
	FLinearColor EmissiveColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Emissive", meta = (EditCondition = "bEmissive", EditConditionHides, UIMin = "0", UIMax = "50", ClampMin = "0"))
	float EmissiveIntensity = 1.f;

	// ---- Noise: detail (grime / bump) and macro variation break up tiling ----
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Noise"))
	bool bDetailNoise = false;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Macro Variation"))
	bool bMacroVariation = false;

	/** Where both noises are projected. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Space", EditCondition = "bDetailNoise || bMacroVariation", EditConditionHides))
	EGlazeDetailSpace DetailSpace = EGlazeDetailSpace::World;

	/** Grayscale noise texture (Add → Glaze: Noise from Pack). Empty = procedural 3D noise, no texture needed. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Texture", EditCondition = "bDetailNoise", EditConditionHides))
	TObjectPtr<UTexture2D> DetailNoise;

	/** Detail repeats per UV tile. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Tiling", EditCondition = "bDetailNoise && DetailSpace == EGlazeDetailSpace::UV", EditConditionHides, UIMin = "0.1", UIMax = "16", ClampMin = "0.001"))
	float DetailTiling = 2.f;

	/** Size of one detail tile in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Size", EditCondition = "bDetailNoise && DetailSpace != EGlazeDetailSpace::UV", EditConditionHides, UIMin = "1", UIMax = "1000", ClampMin = "0.01", Units = "Centimeters"))
	float DetailSize = 200.f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Contrast", EditCondition = "bDetailNoise", EditConditionHides, UIMin = "0", UIMax = "8", ClampMin = "0"))
	float DetailContrast = 1.f;

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Bias", EditCondition = "bDetailNoise", EditConditionHides, UIMin = "-1", UIMax = "1"))
	float DetailBias = 0.f;

	/** Base color is blended towards this color where the detail is bright. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Color", EditCondition = "bDetailNoise", EditConditionHides, HideAlphaChannel))
	FLinearColor DetailColor = FLinearColor(0.05f, 0.04f, 0.03f);

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Color Amount", EditCondition = "bDetailNoise", EditConditionHides, UIMin = "0", UIMax = "1"))
	float DetailColorAmount = 0.3f;

	/** Added to roughness where the detail is bright (negative = shinier). */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Roughness", EditCondition = "bDetailNoise", EditConditionHides, UIMin = "-1", UIMax = "1"))
	float DetailRoughness = 0.15f;

	/** Bumps from the detail: its height in centimeters (negative = dents). 0 = flat. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Detail Bump", EditCondition = "bDetailNoise", EditConditionHides, UIMin = "-5", UIMax = "5", Units = "Centimeters"))
	float DetailBump = 0.f;

	/** Size of one macro patch in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Macro Size", EditCondition = "bMacroVariation && DetailSpace != EGlazeDetailSpace::UV", EditConditionHides, UIMin = "1", UIMax = "5000", ClampMin = "0.01", Units = "Centimeters"))
	float MacroSize = 1000.f;

	/** Macro patches per UV tile. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Macro Tiling", EditCondition = "bMacroVariation && DetailSpace == EGlazeDetailSpace::UV", EditConditionHides, UIMin = "0.01", UIMax = "4", ClampMin = "0.001"))
	float MacroTiling = 0.25f;

	/** Brightness swing of the macro patches: 0.3 = from 70% to 130%. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Macro Amount", EditCondition = "bMacroVariation", EditConditionHides, UIMin = "0", UIMax = "1"))
	float MacroAmount = 0.3f;

	/** Roughness swing of the macro patches. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (DisplayName = "Macro Roughness", EditCondition = "bMacroVariation", EditConditionHides, UIMin = "0", UIMax = "1"))
	float MacroRoughness = 0.1f;

	// ---- Wind: world position offset, sways the vertices ----
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (DisplayName = "Wind"))
	bool bWind = false;

	/** Direction the wind blows to, around the vertical axis: 0 = +X, 90 = +Y. */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (DisplayName = "Direction", EditCondition = "bWind", EditConditionHides, UIMin = "-180", UIMax = "180", Units = "Degrees"))
	float WindDirection = 0.f;

	/** How far the vertices move, in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (DisplayName = "Strength", EditCondition = "bWind", EditConditionHides, UIMin = "0", UIMax = "50", Units = "Centimeters"))
	float WindStrength = 5.f;

	/** Gusts per second. */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (DisplayName = "Speed", EditCondition = "bWind", EditConditionHides, UIMin = "0", UIMax = "4"))
	float WindSpeed = 0.5f;

	/** Distance between gusts in centimeters: neighbours closer than this sway together. */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (DisplayName = "Gust Size", EditCondition = "bWind", EditConditionHides, UIMin = "50", UIMax = "5000", ClampMin = "1", Units = "Centimeters"))
	float WindGustSize = 800.f;

	UPROPERTY(EditAnywhere, Category = "Wind", meta = (DisplayName = "Mask", EditCondition = "bWind", EditConditionHides))
	EGlazeWindMask WindMask = EGlazeWindMask::Height;

	/** Height above the object's pivot where the sway reaches full strength. */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (DisplayName = "Full Sway Height", EditCondition = "bWind && WindMask == EGlazeWindMask::Height", EditConditionHides, UIMin = "10", UIMax = "2000", ClampMin = "0.01", Units = "Centimeters"))
	float WindHeight = 200.f;

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
