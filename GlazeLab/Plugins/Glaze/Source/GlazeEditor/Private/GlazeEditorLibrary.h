#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GlazeEditorLibrary.generated.h"

class UMaterialInstanceConstant;
class UTexture2D;

/**
 * Editor scripting (Python: unreal.GlazeEditorLibrary): opens and drives the Glaze windows the way a user would,
 * for automation and scripted checks. Property values use the Details text format, e.g. "Masked", "True", "0.5".
 */
UCLASS()
class UGlazeEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Opens the Glaze window for these textures (several sets are queued), with a live preview on the selected meshes. */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static void OpenWindowForTextures(const TArray<UTexture2D*>& Textures, const FString& OutputFolder);

	/** "Edit with Glaze". False when the instance was not made by Glaze. */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static bool OpenWindowForInstance(UMaterialInstanceConstant* Instance);

	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static bool IsWindowOpen();

	/** Sets a recipe property of the open window as the Details panel would (preview and texture fixes follow). */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static bool SetWindowProperty(const FString& Name, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static FString GetWindowProperty(const FString& Name);

	/** Header and info lines of the open window (matches, fixes, warnings). */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static FString GetWindowInfo();

	/** The transient preview instance shown on the selected meshes. */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static UMaterialInstanceConstant* GetWindowPreview();

	/** Create (or Apply / Create & Next). False when it failed and the window stays on the set. */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static bool WindowCreate();

	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static void WindowCreateAll();

	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static void WindowSkip();

	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static void WindowCancel();

	/** Opens "Noise from Pack". */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static void OpenNoiseWindow(const FString& Folder);

	/** Picks a noise by asset name (T_Noise_Clouds). */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static bool SelectNoise(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static bool SetNoiseProperty(const FString& Name, const FString& Value);

	/** Creates the texture; returns its path, empty on failure. */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static FString NoiseCreate();

	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static void CloseNoiseWindow();

	/** Imports a pack material's textures at Size (2048/1024/512/256), like the Content Browser menu. */
	UFUNCTION(BlueprintCallable, Category = "Glaze")
	static TArray<UTexture2D*> ImportPackMaterial(const FString& Name, int32 Size);
};
