#include "GlazePack.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
			bLoaded = true;
			FString Text;
			TSharedPtr<FJsonObject> Json;
			if (FFileHelper::LoadFileToString(Text, *(SourceDir() / TEXT("pack.json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) && Json)
			{
				Json->TryGetStringArrayField(TEXT("materials"), Cached.Materials);
				Json->TryGetStringArrayField(TEXT("noises"), Cached.Noises);
			}
			else
			{
				UE_LOG(LogGlazePack, Warning, TEXT("Texture pack manifest not found in %s"), *SourceDir());
			}
		}
		return Cached;
	}

	const TCHAR* Res(bool b2k) { return b2k ? TEXT("2k") : TEXT("1k"); }

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

	/** Loads Folder/AssetName or imports File there. Returns null on failure. */
	UTexture2D* ImportOne(const FString& File, const FString& Folder)
	{
		const FString AssetName = FPaths::GetBaseFilename(File);
		const FString ObjectPath = Folder / AssetName + TEXT(".") + AssetName;
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Existing;
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = File;
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
		UEditorLoadingAndSavingUtils::SavePackages({ Texture->GetPackage() }, /*bOnlyDirty*/ false);
		return Texture;
	}
}

FString SourceDir()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Glaze"));
	return Plugin ? Plugin->GetBaseDir() / TEXT("Resources/Pack") : FString();
}

const TArray<FString>& Materials() { return Manifest().Materials; }
const TArray<FString>& Noises() { return Manifest().Noises; }

FString ContentFolder(const FString& Name, bool b2k)
{
	return FString(ContentRoot) / Name / Res(b2k);
}

TArray<UTexture2D*> ImportMaterial(const FString& Name, bool b2k)
{
	TArray<UTexture2D*> Result;
	TArray<FString> Files;
	const FString Dir = SourceDir() / Name / Res(b2k);
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.png")), true, false);
	Files.Sort();
	for (const FString& File : Files)
	{
		if (UTexture2D* Texture = ImportOne(Dir / File, ContentFolder(Name, b2k)))
		{
			Result.Add(Texture);
		}
	}
	return Result;
}

UTexture2D* ImportNoise(const FString& Name, bool b2k)
{
	const FString File = SourceDir() / TEXT("Noises") / Res(b2k) / Name + TEXT(".png");
	return FPaths::FileExists(File) ? ImportOne(File, FString(ContentRoot) / TEXT("Noises") / Res(b2k)) : nullptr;
}
}
