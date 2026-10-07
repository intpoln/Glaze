#pragma once

#include "CoreMinimal.h"
#include "GlazeRecipe.h"

class UMaterial;

namespace Glaze
{
	/** Bump when the master graph changes: older masters in projects are rebuilt in place (instances keep their values). */
	constexpr int32 MasterVersion = 4;

	/** The master lives in the PROJECT (not in the plugin), so instances keep working when Glaze is not installed. */
	extern const TCHAR* MasterPackagePath; // /Game/Glaze/M_Glaze
	extern const TCHAR* MasterObjectPath;  // /Game/Glaze/M_Glaze.M_Glaze

	/** Loads the master, creating or rebuilding it when missing or older than MasterVersion. Saves it when changed. */
	UMaterial* EnsureMaster(bool bSave = true);

	/** Rebuilds the expression graph of Material (clears existing expressions). Exposed for tests. */
	void BuildMasterGraph(UMaterial* Material);

	bool IsGlazeMaster(const UObject* Material);

	/** Parameter names shared by the master builder and the instance writer. */
	namespace Param
	{
		FName UseMap(EGlazeSlot Slot);      // "Use Roughness Map"
		FName FromPacked(EGlazeSlot Slot);  // "Roughness From Packed"
		FName Map(EGlazeSlot Slot);         // "Roughness Map"
		FName Channel(EGlazeSlot Slot);     // "Roughness Channel"
		FName Value(EGlazeSlot Slot);       // "Roughness"
		FName Invert(EGlazeSlot Slot);
		FName Contrast(EGlazeSlot Slot);
		FName Bias(EGlazeSlot Slot);
		FName Min(EGlazeSlot Slot);
		FName Max(EGlazeSlot Slot);

		extern const FName PackedMap;
		extern const FName Tiling, TilingXY, Offset, Rotation;
		extern const FName Tint, ColorCorrection, Desaturation, ColorContrast, Brightness, HueShift;
		extern const FName OverlayColor, OverlayAmount, LerpColor, LerpAmount;
		extern const FName NormalStrength, FlipGreen;
		extern const FName UseOpacity, OpacityAmount, Dithered;
		extern const FName UseEmissive, EmissiveFromBaseColor, EmissiveColor, EmissiveIntensity;
		extern const FName UseDetail, DetailFromTexture, DetailUV, DetailLocal, DetailPreSkinned, DetailMap;
		extern const FName DetailTiling, DetailSize, DetailContrast, DetailBias, DetailColor, DetailColorAmount, DetailRoughness;
	}
}
