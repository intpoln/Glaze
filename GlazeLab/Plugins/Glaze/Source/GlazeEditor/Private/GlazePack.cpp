#include "GlazePack.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogGlazePack, Log, All);

namespace Glaze::Pack
{
const TCHAR* ContentRoot = TEXT("/Game/Glaze/Pack");

namespace
{
	struct FManifest
	{
		TArray<FString> Materials;
		TArray<FString> Noises;
	};

	const FManifest& Manifest()
	{
		static FManifest Cached;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			FString Text;
			TSharedPtr<FJsonObject> Json;
			if (FFileHelper::LoadFileToString(Text, *(SourceDir() / TEXT("pack.json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) && Json)
			{
				Json->TryGetStringArrayField(TEXT("materials"), Cached.Materials);
				Json->TryGetStringArrayField(TEXT("noises"), Cached.Noises);
				bLoaded = true;
			}
			else
			{
				UE_LOG(LogGlazePack, Warning, TEXT("Texture pack manifest not found in %s"), *SourceDir());
			}
		}
		return Cached;
	}

	/** Content subfolder for a size: 2k, 1k, 512, 256. */
	FString Res(int32 Size) { return Size >= 2048 ? TEXT("2k") : Size == 1024 ? TEXT("1k") : FString::FromInt(Size); }
	/** The pack ships 2K and 1K files; smaller sizes are made from the 1K ones. */
	const TCHAR* SourceRes(int32 Size) { return Size >= 2048 ? TEXT("2k") : TEXT("1k"); }

	/** A copy of File resized to Size (under Saved/Glaze/Resized) for import; File itself when it is not larger. */
	FString ResizedFile(const FString& File, int32 Size)
	{
		FImage Image;
		if (Size <= 0 || !FImageUtils::LoadImage(*File, Image) || Image.SizeX <= Size)
		{
			return File;
		}
		FImage Resized;
		Image.ResizeTo(Resized, Size, FMath::Max(1, Image.SizeY * Size / Image.SizeX), Image.Format, Image.GammaSpace);
		const FString Out = FPaths::ProjectSavedDir() / TEXT("Glaze/Resized") / FString::FromInt(Size) / FPaths::GetCleanFilename(File);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Out), true);
		if (!FImageUtils::SaveImageByExtension(*Out, Resized))
		{
			UE_LOG(LogGlazePack, Error, TEXT("Could not write the resized copy %s"), *Out);
			return FString();
		}
		return Out;
	}

	/** Texture settings by file suffix: normals, packed data and noises are linear. */
	void FixSettings(UTexture2D* Texture)
	{
		const FString Name = Texture->GetName();
		TextureCompressionSettings Compression = TC_Default;
		bool bSRGB = true;
		if (Name.EndsWith(TEXT("_N")))
		{
			Compression = TC_Normalmap;
			bSRGB = false;
		}
		else if (Name.EndsWith(TEXT("_ORM")))
		{
			Compression = TC_Masks;
			bSRGB = false;
		}
		else if (Name.StartsWith(TEXT("T_Noise_")))
		{
			Compression = TC_Grayscale;
			bSRGB = false;
		}
		if (Texture->CompressionSettings != Compression || Texture->SRGB != bSRGB)
		{
			Texture->PreEditChange(nullptr);
			Texture->CompressionSettings = Compression;
			Texture->SRGB = bSRGB;
			Texture->PostEditChange();
			Texture->MarkPackageDirty();
		}
	}

	void Notify(const FString& Text)
	{
		if (FSlateApplication::IsInitialized() && !IsRunningCommandlet() && !GIsAutomationTesting)
		{
			FNotificationInfo Info(FText::FromString(Text));
			Info.ExpireDuration = 8.f;
			Info.bUseLargeFont = false;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	/** Saves the texture package. False (and an Error in the log) when the file could not be written. */
	bool SaveTexture(UTexture2D* Texture)
	{
		if (!UEditorLoadingAndSavingUtils::SavePackages({ Texture->GetOutermost() }, /*bOnlyDirty*/ false))
		{
			UE_LOG(LogGlazePack, Error, TEXT("Could not save %s (is the Content folder writable?)"), *Texture->GetOutermost()->GetName());
			return false;
		}
		return true;
	}

	/** Loads Folder/AssetName or imports File (resized to Size when larger) there. Returns null on failure (import or save). */
	UTexture2D* ImportOne(const FString& File, const FString& Folder, int32 Size)
	{
		const FString AssetName = FPaths::GetBaseFilename(File);
		const FString ObjectPath = Folder / AssetName + TEXT(".") + AssetName;
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			// Imported earlier but never written (the save failed): try again, the .uasset is still missing.
			if (!FPackageName::DoesPackageExist(Existing->GetOutermost()->GetName()) && !SaveTexture(Existing))
			{
				return nullptr;
			}
			return Existing;
		}

		const FString ImportFile = ResizedFile(File, Size);
		if (ImportFile.IsEmpty())
		{
			return nullptr;
		}
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = ImportFile;
		Task->DestinationPath = Folder;
		Task->DestinationName = AssetName;
		Task->bAutomated = true;
		Task->bReplaceExisting = false;
		Task->bSave = false;
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks({ Task });

		UTexture2D* Texture = nullptr;
		for (UObject* Object : Task->GetObjects())
		{
			Texture = Cast<UTexture2D>(Object);
			if (Texture)
			{
				break;
			}
		}
		if (!Texture)
		{
			UE_LOG(LogGlazePack, Error, TEXT("Could not import %s"), *File);
			return nullptr;
		}
		FixSettings(Texture);
		return SaveTexture(Texture) ? Texture : nullptr;
	}
}

FString SourceDir()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Glaze"));
	return Plugin ? Plugin->GetBaseDir() / TEXT("Resources/Pack") : FString();
}

const TArray<FString>& Materials() { return Manifest().Materials; }
const TArray<FString>& Noises() { return Manifest().Noises; }

FString ContentFolder(const FString& Name, int32 Size)
{
	return FString(ContentRoot) / Name / Res(Size);
}

TArray<UTexture2D*> ImportMaterial(const FString& Name, int32 Size, TArray<FString>* OutFailed)
{
	TArray<UTexture2D*> Result;
	TArray<FString> Files;
	const FString Dir = SourceDir() / Name / SourceRes(Size);
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.png")), true, false);
	Files.Sort();
	TArray<FString> Failed;
	if (Files.IsEmpty())
	{
		UE_LOG(LogGlazePack, Error, TEXT("No textures found for pack material %s in %s"), *Name, *Dir);
		Failed.Add(FString(SourceRes(Size)) + TEXT("/*.png"));
	}
	else
	{
		// A complete pack material has BC, N and ORM; a trimmed copy must not pass as a full set.
		bool bBC = false, bN = false, bORM = false;
		for (const FString& File : Files)
		{
			const FString Base = FPaths::GetBaseFilename(File);
			bN |= Base.EndsWith(TEXT("_N"));
			bORM |= Base.EndsWith(TEXT("_ORM"));
			bBC |= Base.EndsWith(TEXT("_BC"));
		}
		if (!bBC) { Failed.Add(TEXT("*_BC.png (missing)")); }
		if (!bN) { Failed.Add(TEXT("*_N.png (missing)")); }
		if (!bORM) { Failed.Add(TEXT("*_ORM.png (missing)")); }
	}
	for (const FString& File : Files)
	{
		if (UTexture2D* Texture = ImportOne(Dir / File, ContentFolder(Name, Size), Size))
		{
			Result.Add(Texture);
		}
		else
		{
			Failed.Add(File);
		}
	}
	if (!Failed.IsEmpty())
	{
		const FString List = FString::Join(Failed, TEXT(", "));
		UE_LOG(LogGlazePack, Error, TEXT("Pack material %s (%s) is incomplete, failed: %s"), *Name, *Res(Size), *List);
		Notify(FString::Printf(TEXT("Glaze: pack material %s is incomplete. Not imported: %s"), *Name, *List));
	}
	if (OutFailed)
	{
		*OutFailed = Failed;
	}
	return Result;
}

UTexture2D* ImportNoise(const FString& Name, int32 Size)
{
	const FString File = SourceDir() / TEXT("Noises") / SourceRes(Size) / Name + TEXT(".png");
	if (!FPaths::FileExists(File))
	{
		UE_LOG(LogGlazePack, Error, TEXT("Pack noise file not found: %s"), *File);
		return nullptr;
	}
	UTexture2D* Noise = ImportOne(File, FString(ContentRoot) / TEXT("Noises") / Res(Size), Size);
	if (!Noise)
	{
		Notify(FString::Printf(TEXT("Glaze: could not import noise %s"), *Name));
	}
	return Noise;
}
}
