// Copyright 2026 Nikita Polushin. All Rights Reserved.

#include "GlazeNoise.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "GlazePack.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

bool UGlazeNoiseSettings::IsIdentity() const
{
	return !bInvert && BlackPoint == 0.f && WhitePoint == 1.f && Gamma == 1.f && Contrast == 0.f && Brightness == 0.f;
}

int32 UGlazeNoiseSettings::Size() const
{
	switch (Resolution)
	{
	case EGlazeNoiseResolution::R1k:  return 1024;
	case EGlazeNoiseResolution::R512: return 512;
	case EGlazeNoiseResolution::R256: return 256;
	default:                          return 2048;
	}
}

void UGlazeNoiseSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	OnChanged.Broadcast(Event);
}

void UGlazeNoiseSettings::PostEditUndo()
{
	Super::PostEditUndo();
	FPropertyChangedEvent Event(nullptr);
	OnChanged.Broadcast(Event);
}

namespace Glaze::Noise
{
TArray<uint8> BuildLut(const UGlazeNoiseSettings& S)
{
	TArray<uint8> Lut;
	Lut.SetNumUninitialized(256);
	const float Range = FMath::Max(S.WhitePoint - S.BlackPoint, 1e-4f);
	const float Gamma = FMath::Max(S.Gamma, 0.01f);
	// 1 (max) would be an infinite slope: a step at the middle gray.
	const float Contrast = FMath::Clamp(S.Contrast, -1.f, 1.f);
	const float Slope = Contrast >= 0.f ? 1.f / FMath::Max(1.f - Contrast, 1e-3f) : 1.f + Contrast;
	for (int32 I = 0; I < 256; ++I)
	{
		float V = FMath::Clamp((I / 255.f - S.BlackPoint) / Range, 0.f, 1.f);
		V = FMath::Pow(V, 1.f / Gamma);
		V = (V - 0.5f) * Slope + 0.5f + S.Brightness;
		V = FMath::Clamp(V, 0.f, 1.f);
		if (S.bInvert)
		{
			V = 1.f - V;
		}
		Lut[I] = static_cast<uint8>(FMath::RoundToInt(V * 255.f));
	}
	return Lut;
}

FString PackFile(const FString& Name, int32 Size)
{
	return Glaze::Pack::SourceDir() / TEXT("Noises") / (Size >= 2048 ? TEXT("2k") : TEXT("1k")) / Name + TEXT(".png");
}

bool LoadGray(const FString& File, int32 Size, TArray<uint8>& OutPixels, int32& OutSize)
{
	FImage Image;
	if (!FImageUtils::LoadImage(*File, Image) || Image.SizeX <= 0 || Image.SizeX != Image.SizeY)
	{
		return false;
	}
	// The pack noises are linear masks: an 8-bit PNG is labelled sRGB on load, relabel it without touching the bytes.
	Image.GammaSpace = EGammaSpace::Linear;
	FImage Gray;
	if (Size > 0 && Size != Image.SizeX)
	{
		Image.ResizeTo(Gray, Size, Size, ERawImageFormat::G8, EGammaSpace::Linear);
	}
	else
	{
		Image.CopyTo(Gray, ERawImageFormat::G8, EGammaSpace::Linear);
	}
	OutSize = Gray.SizeX;
	OutPixels = MoveTemp(Gray.RawData);
	return OutPixels.Num() == OutSize * OutSize;
}

UTexture2D* CreateTextureAsset(const TArray<uint8>& Pixels, int32 Size, const FString& PackagePath, FString& OutError, bool bSRGB)
{
	if (Pixels.Num() != Size * Size || !FPackageName::IsValidTextForLongPackageName(PackagePath))
	{
		OutError = FString::Printf(TEXT("Invalid noise texture path or data: %s"), *PackagePath);
		return nullptr;
	}
	if (FindPackage(nullptr, *PackagePath) || FPackageName::DoesPackageExist(PackagePath))
	{
		OutError = FString::Printf(TEXT("%s already exists: pick another name."), *PackagePath);
		return nullptr;
	}
	UPackage* Package = CreatePackage(*PackagePath);
	UTexture2D* Texture = NewObject<UTexture2D>(Package, *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone | RF_Transactional);
	Texture->Source.Init(Size, Size, 1, 1, TSF_G8, Pixels.GetData());
	Texture->CompressionSettings = TC_Grayscale;
	Texture->SRGB = bSRGB;
	Texture->PostEditChange();
	Texture->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Texture);
	if (!UEditorLoadingAndSavingUtils::SavePackages({ Package }, /*bOnlyDirty*/ false))
	{
		OutError = FString::Printf(TEXT("Created %s but could not save it (is the Content folder writable?)"), *PackagePath);
	}
	return Texture;
}
}
