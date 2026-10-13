// Copyright 2026 Nikita Polushin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * The texture pack shipped with the plugin (Resources/Pack, PNG + pack.json).
 * Textures are imported into the PROJECT (/Game/Glaze/Pack/...) on first use, so materials keep working without Glaze.
 */
namespace Glaze::Pack
{
	extern const TCHAR* ContentRoot; // /Game/Glaze/Pack

	/** Plugin folder with the PNGs and pack.json. */
	FString SourceDir();

	const TArray<FString>& Materials();
	const TArray<FString>& Noises();

	/** Texture sizes offered for the pack: 2048 and 1024 are shipped, 512 and 256 are resized from 1024 on import. */
	inline const int32 Sizes[] = { 2048, 1024, 512, 256 };

	/** Content path of a pack texture folder, e.g. /Game/Glaze/Pack/Wood_Planks/2k (or /512). */
	FString ContentFolder(const FString& Name, int32 Size);

	/**
	 * Imports (or reuses already imported) BC/N/ORM textures of a pack material. Settings are fixed on import.
	 * A map that could not be imported or saved is left out of the result; its file name is added to OutFailed
	 * (Result.Num() < expected also means a partial pack). Failures are logged as errors and shown as a notification.
	 */
	TArray<UTexture2D*> ImportMaterial(const FString& Name, int32 Size, TArray<FString>* OutFailed = nullptr);

	/** Imports (or reuses) one noise texture: linear grayscale. Null when the import or the save failed (logged as an error). */
	UTexture2D* ImportNoise(const FString& Name, int32 Size);
}
