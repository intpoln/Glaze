// Copyright 2026 Nikita Polushin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "GlazeNoise.generated.h"

class UTexture2D;

UENUM()
enum class EGlazeNoiseResolution : uint8
{
	R2k UMETA(DisplayName = "2K (2048)"),
	R1k UMETA(DisplayName = "1K (1024)"),
	R512 UMETA(DisplayName = "512"),
	R256 UMETA(DisplayName = "256")
};

/** Settings of the noise window: tone adjustments baked into the created texture. */
UCLASS(Transient)
class UGlazeNoiseSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Adjust")
	bool bInvert = false;

	/** Input values at or below this become black. */
	UPROPERTY(EditAnywhere, Category = "Adjust", meta = (DisplayName = "Black Point", ClampMin = "0", ClampMax = "1"))
	float BlackPoint = 0.f;

	/** Input values at or above this become white. */
	UPROPERTY(EditAnywhere, Category = "Adjust", meta = (DisplayName = "White Point", ClampMin = "0", ClampMax = "1"))
	float WhitePoint = 1.f;

	/** Midtones: above 1 brightens them, below 1 darkens. */
	UPROPERTY(EditAnywhere, Category = "Adjust", meta = (ClampMin = "0.1", ClampMax = "10", UIMin = "0.25", UIMax = "4"))
	float Gamma = 1.f;

	/** Pushes values away from (above 0) or toward (below 0) the middle gray. 1 = a hard black and white mask. */
	UPROPERTY(EditAnywhere, Category = "Adjust", meta = (ClampMin = "-1", ClampMax = "1"))
	float Contrast = 0.f;

	UPROPERTY(EditAnywhere, Category = "Adjust", meta = (ClampMin = "-1", ClampMax = "1"))
	float Brightness = 0.f;

	/** Shows the noise repeated N x N to check that it tiles. Not baked. */
	UPROPERTY(EditAnywhere, Category = "Preview", meta = (DisplayName = "Preview Tiling", ClampMin = "1", ClampMax = "4"))
	int32 PreviewTiling = 1;

	UPROPERTY(EditAnywhere, Category = "Output")
	EGlazeNoiseResolution Resolution = EGlazeNoiseResolution::R2k;

	/**
	 * Off (default): linear data, as noise masks for materials (Glaze detail noise) expect.
	 * On: the gray values are treated as sRGB colors. The preview shows how the texture will look either way.
	 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "sRGB"))
	bool bSRGB = false;

	int32 Size() const;

	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Name"))
	FString AssetName;

	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Folder", ContentDir))
	FDirectoryPath Folder;

	bool IsIdentity() const;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChanged, const FPropertyChangedEvent&);
	FOnChanged OnChanged;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
};

namespace Glaze::Noise
{
	/** Value map for every 8-bit input: levels, gamma, contrast, brightness, invert. */
	TArray<uint8> BuildLut(const UGlazeNoiseSettings& Settings);

	/** Decodes a pack noise PNG as linear 8-bit grayscale; Size > 0 resizes it. False when the file cannot be read. */
	bool LoadGray(const FString& File, int32 Size, TArray<uint8>& OutPixels, int32& OutSize);

	/** Pack noise file to read for a texture size (2K, or 1K for 1K and smaller). */
	FString PackFile(const FString& Name, int32 Size);

	/**
	 * Creates and saves a grayscale texture asset (linear, Grayscale compression) at PackagePath from square pixels.
	 * Null + OutError on failure.
	 */
	UTexture2D* CreateTextureAsset(const TArray<uint8>& Pixels, int32 Size, const FString& PackagePath, FString& OutError, bool bSRGB = false);
}
