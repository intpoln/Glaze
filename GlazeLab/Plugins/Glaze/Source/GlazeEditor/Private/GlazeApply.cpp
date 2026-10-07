#include "GlazeApply.h"

#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "FileHelpers.h"
#include "GlazeMaster.h"
#include "GlazeRecipe.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ScopedTransaction.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "Glaze"

namespace Glaze
{
namespace
{
	const FName MetaRecipeKey(TEXT("GlazeRecipe"));

	const EGlazeSlot GrayscaleSlots[] = { EGlazeSlot::Roughness, EGlazeSlot::Metallic, EGlazeSlot::AmbientOcclusion, EGlazeSlot::Specular, EGlazeSlot::Opacity };

	enum class EUse : uint8 { Color, Normal, Masks, Grayscale };

	EUse UseOf(EGlazeSlot Slot)
	{
		if (Slot == EGlazeSlot::Normal)
		{
			return EUse::Normal;
		}
		return IsScalarSlot(Slot) ? EUse::Masks : EUse::Color;
	}

	const FGlazeRemap& RemapOf(const UGlazeRecipe& R, EGlazeSlot Slot)
	{
		switch (Slot)
		{
		case EGlazeSlot::Roughness:        return R.RoughnessRemap;
		case EGlazeSlot::Metallic:         return R.MetallicRemap;
		case EGlazeSlot::AmbientOcclusion: return R.AmbientOcclusionRemap;
		case EGlazeSlot::Specular:         return R.SpecularRemap;
		default:                           return R.OpacityRemap;
		}
	}

	float ValueOf(const UGlazeRecipe& R, EGlazeSlot Slot)
	{
		switch (Slot)
		{
		case EGlazeSlot::Roughness:        return R.RoughnessValue;
		case EGlazeSlot::Metallic:         return R.MetallicValue;
		case EGlazeSlot::AmbientOcclusion: return R.AmbientOcclusionValue;
		case EGlazeSlot::Specular:         return R.SpecularValue;
		default:                           return 1.f;
		}
	}

	FLinearColor ChannelMask(EGlazeChannel Channel)
	{
		FLinearColor C(0, 0, 0, 0);
		C.Component(static_cast<int32>(Channel)) = 1.f;
		return C;
	}

	/** The texture read by two or more grayscale slots becomes the shared "Packed Map": one sample instead of several. */
	UTexture2D* FindPacked(const UGlazeRecipe& R)
	{
		TMap<UTexture2D*, int32> Uses;
		for (EGlazeSlot Slot : GrayscaleSlots)
		{
			if (UTexture2D* T = R.GetSlot(Slot))
			{
				if (++Uses.FindOrAdd(T) == 2)
				{
					return T;
				}
			}
		}
		return nullptr;
	}

	/** Uses of each assigned texture, to find conflicts (one texture as color and as mask). */
	TMap<UTexture2D*, TSet<EUse>> CollectUses(const UGlazeRecipe& R)
	{
		TMap<UTexture2D*, TSet<EUse>> Uses;
		for (int32 I = 0; I < static_cast<int32>(EGlazeSlot::Num); ++I)
		{
			const EGlazeSlot Slot = static_cast<EGlazeSlot>(I);
			if (Slot == EGlazeSlot::Height)
			{
				continue; // not sampled yet
			}
			if (UTexture2D* T = R.GetSlot(Slot))
			{
				Uses.FindOrAdd(T).Add(UseOf(Slot));
			}
		}
		if (R.bDetailNoise && R.DetailNoise)
		{
			Uses.FindOrAdd(R.DetailNoise).Add(EUse::Grayscale);
		}
		return Uses;
	}

	/** Returns true when an override was removed (the render proxy then needs a full update). */
	bool SetTexture(UMaterialInstanceConstant* MI, FName Name, UTexture* Texture)
	{
		if (Texture)
		{
			MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Name), Texture);
			return false;
		}
		// No override: the instance must not reference textures it does not sample.
		return MI->TextureParameterValues.RemoveAll([Name](const FTextureParameterValue& V) { return V.ParameterInfo.Name == Name; }) > 0;
	}
}

TArray<FTextureFix> PlanTextureFixes(const UGlazeRecipe& Recipe, TArray<FString>* OutWarnings)
{
	TArray<FTextureFix> Fixes;
	for (const TPair<UTexture2D*, TSet<EUse>>& Pair : CollectUses(Recipe))
	{
		UTexture2D* T = Pair.Key;
		if (Pair.Value.Num() > 1)
		{
			if (OutWarnings)
			{
				OutWarnings->Add(FString::Printf(TEXT("%s is used both as color/normal and as a mask: the material may not compile."), *T->GetName()));
			}
			continue;
		}
		const EUse Use = *Pair.Value.CreateConstIterator();
		TArray<FString> Changes;
		switch (Use)
		{
		case EUse::Normal:
			if (T->CompressionSettings != TC_Normalmap) { Changes.Add(TEXT("Normalmap")); }
			if (T->SRGB) { Changes.Add(TEXT("sRGB off")); }
			break;
		case EUse::Masks:
			if (T->CompressionSettings != TC_Masks) { Changes.Add(TEXT("Masks")); }
			if (T->SRGB) { Changes.Add(TEXT("sRGB off")); }
			break;
		case EUse::Grayscale:
			if (T->CompressionSettings != TC_Grayscale) { Changes.Add(TEXT("Grayscale")); }
			if (T->SRGB) { Changes.Add(TEXT("sRGB off")); }
			break;
		case EUse::Color:
			if (T->CompressionSettings == TC_Normalmap || T->CompressionSettings == TC_Masks || T->CompressionSettings == TC_Grayscale
				|| T->CompressionSettings == TC_Alpha)
			{
				Changes.Add(TEXT("Default compression"));
			}
			if (!T->SRGB) { Changes.Add(TEXT("sRGB on")); }
			break;
		}
		if (T->VirtualTextureStreaming)
		{
			Changes.Add(TEXT("virtual texturing off"));
		}
		if (Changes.Num() > 0)
		{
			Fixes.Add({ T, FString::Printf(TEXT("%s: %s"), *T->GetName(), *FString::Join(Changes, TEXT(", "))) });
		}
	}
	return Fixes;
}

TArray<UTexture2D*> FixTextureSettings(const UGlazeRecipe& Recipe)
{
	const TArray<FTextureFix> Fixes = PlanTextureFixes(Recipe);
	TArray<UTexture2D*> Changed;
	if (Fixes.Num() == 0)
	{
		return Changed;
	}
	const TMap<UTexture2D*, TSet<EUse>> Uses = CollectUses(Recipe);
	FScopedTransaction Transaction(LOCTEXT("FixTextures", "Glaze: Fix Texture Settings"));
	for (const FTextureFix& Fix : Fixes)
	{
		UTexture2D* T = Fix.Texture;
		const EUse Use = *Uses.FindChecked(T).CreateConstIterator();
		T->PreEditChange(nullptr);
		T->Modify();
		switch (Use)
		{
		case EUse::Normal:
			T->CompressionSettings = TC_Normalmap;
			T->SRGB = false;
			break;
		case EUse::Masks:
			T->CompressionSettings = TC_Masks;
			T->SRGB = false;
			break;
		case EUse::Grayscale:
			T->CompressionSettings = TC_Grayscale;
			T->SRGB = false;
			break;
		case EUse::Color:
			if (T->CompressionSettings == TC_Normalmap || T->CompressionSettings == TC_Masks || T->CompressionSettings == TC_Grayscale
				|| T->CompressionSettings == TC_Alpha)
			{
				T->CompressionSettings = TC_Default;
			}
			T->SRGB = true;
			break;
		}
		T->VirtualTextureStreaming = false;
		T->PostEditChange();
		Changed.Add(T);
	}
	return Changed;
}

void ApplyRecipe(UMaterialInstanceConstant* MI, const UGlazeRecipe& R, FString& InOutStaticKey)
{
	using namespace Param;
	TArray<TPair<FName, bool>> Switches;
	// The *EditorOnly setters update the render proxy directly. UMaterialEditingLibrary setters run a full
	// UpdateMaterialInstance per call (seconds for ~60 parameters), so that is done once below, only for static changes.
	auto Scalar = [MI](FName Name, float V) { MI->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(Name), V); };
	auto Vector = [MI](FName Name, const FLinearColor& V) { MI->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(Name), V); };
	bool bRemovedTexture = false;
	auto SetTexture = [MI, &bRemovedTexture](FName Name, UTexture* Texture) { bRemovedTexture |= Glaze::SetTexture(MI, Name, Texture); };

	// UV
	Scalar(Tiling, R.Tiling);
	Vector(TilingXY, FLinearColor(R.TilingXY.X, R.TilingXY.Y, 0, 0));
	Vector(Offset, FLinearColor(R.Offset.X, R.Offset.Y, 0, 0));
	Scalar(Rotation, R.Rotation);

	// Base color
	Switches.Add({ UseMap(EGlazeSlot::BaseColor), R.BaseColor != nullptr });
	SetTexture(Map(EGlazeSlot::BaseColor), R.BaseColor);
	Vector(Tint, R.Tint);
	Switches.Add({ ColorCorrection, R.bColorCorrection });
	Scalar(Desaturation, R.Desaturation);
	Scalar(ColorContrast, R.Contrast);
	Scalar(Brightness, R.Brightness);
	Scalar(HueShift, R.HueShift);
	Vector(OverlayColor, R.OverlayColor);
	Scalar(OverlayAmount, R.OverlayAmount);
	Vector(LerpColor, R.LerpColor);
	Scalar(LerpAmount, R.LerpAmount);

	// Normal
	Switches.Add({ UseMap(EGlazeSlot::Normal), R.Normal != nullptr });
	SetTexture(Map(EGlazeSlot::Normal), R.Normal);
	Scalar(NormalStrength, R.NormalStrength);
	Scalar(FlipGreen, R.bFlipGreen ? 1.f : 0.f);

	// Grayscale channels
	UTexture2D* Packed = FindPacked(R);
	SetTexture(PackedMap, Packed);
	for (EGlazeSlot Slot : GrayscaleSlots)
	{
		UTexture2D* T = R.GetSlot(Slot);
		const bool bFromPacked = T && T == Packed;
		Switches.Add({ UseMap(Slot), T != nullptr });
		Switches.Add({ FromPacked(Slot), bFromPacked });
		SetTexture(Map(Slot), bFromPacked ? nullptr : T);
		Vector(Channel(Slot), ChannelMask(R.GetChannel(Slot)));
		const FGlazeRemap& Remap = RemapOf(R, Slot);
		Scalar(Invert(Slot), Remap.bInvert ? 1.f : 0.f);
		Scalar(Contrast(Slot), Remap.Contrast);
		Scalar(Bias(Slot), Remap.Bias);
		Scalar(Min(Slot), Remap.Min);
		Scalar(Max(Slot), Remap.Max);
		if (Slot != EGlazeSlot::Opacity)
		{
			Scalar(Value(Slot), ValueOf(R, Slot));
		}
	}

	// Opacity
	const bool bMasked = R.BlendMode == EGlazeBlend::Masked;
	Switches.Add({ UseOpacity, bMasked });
	Switches.Add({ Dithered, bMasked && R.bDithered });
	Scalar(OpacityAmount, R.OpacityAmount);

	// Emissive
	Switches.Add({ UseEmissive, R.bEmissive });
	const bool bEmissiveFromBase = R.bEmissive && R.bEmissiveFromBaseColor && R.BaseColor != nullptr;
	const bool bEmissiveMap = R.bEmissive && !bEmissiveFromBase && R.Emissive != nullptr;
	Switches.Add({ EmissiveFromBaseColor, bEmissiveFromBase });
	Switches.Add({ UseMap(EGlazeSlot::Emissive), bEmissiveMap });
	SetTexture(Map(EGlazeSlot::Emissive), bEmissiveMap ? R.Emissive.Get() : nullptr);
	Vector(EmissiveColor, R.EmissiveColor);
	Scalar(EmissiveIntensity, R.EmissiveIntensity);

	// Detail noise
	const bool bDetail = R.bDetailNoise;
	Switches.Add({ UseDetail, bDetail });
	Switches.Add({ DetailFromTexture, bDetail && R.DetailNoise != nullptr });
	Switches.Add({ DetailUV, bDetail && R.DetailSpace == EGlazeDetailSpace::UV });
	Switches.Add({ DetailLocal, bDetail && R.DetailSpace == EGlazeDetailSpace::Local });
	Switches.Add({ DetailPreSkinned, bDetail && R.DetailSpace == EGlazeDetailSpace::PreSkinned });
	SetTexture(DetailMap, bDetail ? R.DetailNoise.Get() : nullptr);
	Scalar(DetailTiling, R.DetailTiling);
	Scalar(DetailSize, R.DetailSize);
	Scalar(DetailContrast, R.DetailContrast);
	Scalar(DetailBias, R.DetailBias);
	Vector(DetailColor, R.DetailColor);
	Scalar(DetailColorAmount, R.DetailColorAmount);
	Scalar(DetailRoughness, R.DetailRoughness);

	// Static state: switches + base property overrides. Only a change here needs a recompile.
	FString Key;
	for (const TPair<FName, bool>& S : Switches)
	{
		Key += S.Value ? TEXT("1") : TEXT("0");
	}
	Key += FString::Printf(TEXT("|%d|%d|%.4f"), bMasked ? 1 : 0, R.bTwoSided ? 1 : 0, R.OpacityClip);
	if (Key == InOutStaticKey)
	{
		if (bRemovedTexture)
		{
			UMaterialEditingLibrary::UpdateMaterialInstance(MI);
		}
		return;
	}
	InOutStaticKey = Key;

	for (const TPair<FName, bool>& S : Switches)
	{
		UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MI, S.Key, S.Value, EMaterialParameterAssociation::GlobalParameter, false);
	}
	FMaterialInstanceBasePropertyOverrides& O = MI->BasePropertyOverrides;
	O.bOverride_BlendMode = bMasked;
	O.BlendMode = bMasked ? BLEND_Masked : BLEND_Opaque;
	O.bOverride_OpacityMaskClipValue = bMasked;
	O.OpacityMaskClipValue = R.OpacityClip;
	O.bOverride_TwoSided = R.bTwoSided;
	O.TwoSided = R.bTwoSided;
	UMaterialEditingLibrary::UpdateMaterialInstance(MI);
}

UMaterialInstanceConstant* WriteInstanceAsset(const UGlazeRecipe& Recipe, UMaterialInstanceConstant* Existing, FString& OutError)
{
	UMaterial* Master = EnsureMaster();
	if (!Master)
	{
		OutError = TEXT("Could not create the Glaze master material.");
		return nullptr;
	}

	TArray<UPackage*> ToSave;
	if (Recipe.bFixTextureSettings)
	{
		for (UTexture2D* T : FixTextureSettings(Recipe))
		{
			ToSave.Add(T->GetPackage());
		}
	}

	FScopedTransaction Transaction(LOCTEXT("WriteInstance", "Glaze: Material Instance"));
	UMaterialInstanceConstant* MI = Existing;
	if (!MI)
	{
		FString Folder = Recipe.Folder.Path.IsEmpty() ? TEXT("/Game") : Recipe.Folder.Path;
		const FString BaseName = Recipe.AssetName.IsEmpty() ? TEXT("MI_Glaze") : Recipe.AssetName;
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		FString PackageName, AssetName;
		AssetTools.CreateUniqueAssetName(Folder / BaseName, TEXT(""), PackageName, AssetName);
		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = Master;
		MI = Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(AssetName, Folder, UMaterialInstanceConstant::StaticClass(), Factory));
		if (!MI)
		{
			OutError = FString::Printf(TEXT("Could not create %s/%s"), *Folder, *AssetName);
			return nullptr;
		}
	}
	MI->Modify();
	if (MI->Parent != Master)
	{
		UMaterialEditingLibrary::SetMaterialInstanceParent(MI, Master);
	}
	FString Key;
	ApplyRecipe(MI, Recipe, Key);
	MI->GetPackage()->GetMetaData().SetValue(MI, MetaRecipeKey, *Recipe.ToJson());
	MI->MarkPackageDirty();
	ToSave.Add(MI->GetPackage());
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, /*bOnlyDirty*/ true);
	return MI;
}

bool ReadRecipe(const UMaterialInstanceConstant* Instance, UGlazeRecipe& OutRecipe)
{
	if (!IsGlazeInstance(Instance))
	{
		return false;
	}
	const FString& Json = Instance->GetPackage()->GetMetaData().GetValue(Instance, MetaRecipeKey);
	return !Json.IsEmpty() && OutRecipe.FromJson(Json);
}

bool IsGlazeInstance(const UMaterialInstanceConstant* Instance)
{
	return Instance && IsGlazeMaster(Instance->Parent);
}
}

#undef LOCTEXT_NAMESPACE
