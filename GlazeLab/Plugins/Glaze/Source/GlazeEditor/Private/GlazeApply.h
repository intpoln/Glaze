// Copyright 2026 Nikita Polushin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UGlazeRecipe;
class UMaterialInstanceConstant;
class UTexture2D;

namespace Glaze
{
	struct FTextureFix
	{
		UTexture2D* Texture = nullptr;
		FString Description; // "T_Wood_N: Normalmap, sRGB off"
	};

	/** What FixTextureSettings would change (nothing is modified). Warnings: a texture in conflicting slots. */
	TArray<FTextureFix> PlanTextureFixes(const UGlazeRecipe& Recipe, TArray<FString>* OutWarnings = nullptr);

	/** Applies the planned fixes (undoable, marks textures dirty). Returns the changed textures. */
	TArray<UTexture2D*> FixTextureSettings(const UGlazeRecipe& Recipe);

	/**
	 * Writes the recipe into an instance of the Glaze master.
	 * Static switches and blend overrides recompile the instance, so it is only updated when they changed:
	 * InOutStaticKey holds the previous static state (empty = always update).
	 */
	void ApplyRecipe(UMaterialInstanceConstant* Instance, const UGlazeRecipe& Recipe, FString& InOutStaticKey);

	/**
	 * Creates (or updates, when Existing is set) the instance asset, stores the recipe in its metadata and saves it
	 * together with the textures whose settings Glaze fixed: now, or earlier (FixedEarlier, e.g. by the preview).
	 * Other unsaved texture edits are not saved. nullptr + OutError: nothing was created.
	 * Instance + OutError: created/updated in memory but not saved.
	 */
	UMaterialInstanceConstant* WriteInstanceAsset(const UGlazeRecipe& Recipe, UMaterialInstanceConstant* Existing, FString& OutError,
		const TArray<UTexture2D*>& FixedEarlier = {});

	/** Recipe saved by WriteInstanceAsset; false when the instance was not made by Glaze. */
	bool ReadRecipe(const UMaterialInstanceConstant* Instance, UGlazeRecipe& OutRecipe);

	bool IsGlazeInstance(const UMaterialInstanceConstant* Instance);
}
