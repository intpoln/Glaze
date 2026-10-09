#pragma once

#include "CoreMinimal.h"
#include "GlazeRecipe.h"

class UTexture2D;

namespace Glaze
{
	/** One slot (and channel) a texture name maps to. A packed map (ORM, RMA...) yields several. */
	struct FSlotMatch
	{
		EGlazeSlot Slot = EGlazeSlot::Num;
		EGlazeChannel Channel = EGlazeChannel::R;
		/** Gloss / smoothness map: roughness must be inverted. */
		bool bInvert = false;
		/** OpenGL normal map: green must be flipped. */
		bool bFlipGreen = false;
	};

	struct FNameMatch
	{
		/** Name with prefix (T_), slot suffix and resolution tokens removed: "T_Wood_01_BaseColor_2K" -> "Wood_01". */
		FString SetName;
		TArray<FSlotMatch> Slots;
		/** The token that decided the match, for the UI ("_N"). */
		FString Reason;
		/** Lower-case resolution token that was removed ("4k", "2048"), empty when the name has none. */
		FString Resolution;
	};

	/** Pure name matching, no asset access. Slots is empty when nothing matched. */
	FNameMatch MatchTextureName(const FString& AssetName);

	struct FTextureSetMatch
	{
		FString SetName;
		/** Textures matched to no slot. */
		TArray<UTexture2D*> Unmatched;
		/** Per slot: why it was chosen ("_ORM.G"). */
		TMap<EGlazeSlot, FString> Reasons;
		bool bInvertRoughness = false;
		bool bFlipGreen = false;
	};

	/** Fills the recipe slots from the textures. The first texture wins a slot; packed maps fill several slots. */
	FTextureSetMatch MatchTextures(const TArray<UTexture2D*>& Textures, UGlazeRecipe& Recipe);

	/**
	 * Splits textures into sets by name ("Wood_01", "Stone_02"); order of first appearance is kept.
	 * Textures only share a set when they live in the same folder and the name matches (Unicode case-insensitive).
	 * Different resolutions of one name ("Wood_4K", "Wood_1K") become separate sets ("Wood (4K)"); maps without a
	 * resolution join the first of them. Equal names from different folders get the folder appended.
	 */
	TArray<TPair<FString, TArray<UTexture2D*>>> GroupTextureSets(const TArray<UTexture2D*>& Textures);
}
