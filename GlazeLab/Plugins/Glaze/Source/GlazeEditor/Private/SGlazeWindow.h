#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureDefines.h"
#include "UObject/GCObject.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class UGlazeRecipe;
class UMaterialInstanceConstant;
class UMeshComponent;
class UMaterialInterface;
class UTexture2D;
class STextBlock;
class SWindow;

/** The Glaze window: texture slots + modules (a details view over UGlazeRecipe), live preview on selected meshes. */
class SGlazeWindow : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SGlazeWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGlazeWindow() override;

	/** Selected textures; several sets (by name) are created one after another with the same settings. */
	/** OutputFolder: where instances go (empty = next to the first texture). */
	static void OpenForTextures(const TArray<UTexture2D*>& Textures, const FString& OutputFolder = FString());
	/** Reopens a Glaze instance with its saved recipe. */
	static void OpenForInstance(UMaterialInstanceConstant* Instance);
	/** Closes the window right away (module shutdown): restores meshes and textures. */
	static void CloseAll();

	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	// Keeps the original materials of preview targets and the queued texture sets alive.
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SGlazeWindow"); }

private:
	static TSharedRef<SGlazeWindow> Open(const FText& Title);

	void StartSet(int32 Index, bool bKeepSettings);
	void OnRecipeChanged(const struct FPropertyChangedEvent& Event);
	void RefreshPreview();
	void RefreshInfo();

	void CapturePreviewTargets();
	void ApplyPreviewToTargets();
	void RestoreTargets();
	void AssignToTargets(UMaterialInterface* Material);
	/** The preview instance must not be saved with a level or recorded by delete/copy/duplicate transactions. */
	void BindEditorEvents();
	void UnbindEditorEvents();

	/** Fixes texture settings for the current recipe, remembering the old ones so Cancel can put them back. */
	void FixTextures();
	/** Puts back texture settings changed by Glaze that were not saved with a created instance. */
	void RevertTextureFixes();
	/** Undo/redo can change the recipe: show it and preview it again. */
	void OnUndoRedo();
	friend class FGlazePreviewGuard;
	void ForgetSavedTextureFixes();

	FReply OnCreate();
	FReply OnCreateAll();
	FReply OnSkip();
	FReply OnCancel();
	bool CreateCurrent();
	void NextOrClose();
	void Close(bool bImmediate = false);

	FText GetCreateLabel() const;
	bool HasMoreSets() const { return SetIndex + 1 < Sets.Num(); }

	TStrongObjectPtr<UGlazeRecipe> Recipe;
	TStrongObjectPtr<UMaterialInstanceConstant> PreviewInstance;
	FString PreviewStaticKey;

	/** Set when editing an existing instance ("Edit with Glaze"). */
	TWeakObjectPtr<UMaterialInstanceConstant> EditTarget;
	/** Created but not saved (save failed): the next Create retries on it instead of making another asset. */
	TWeakObjectPtr<UMaterialInstanceConstant> UnsavedInstance;
	/** The user picked the output folder: batch sets keep it instead of using their texture folders. */
	bool bFolderEdited = false;
	/** Masked / Emissive were switched on by Glaze for a set's maps, not by the user: the next set without them turns them off. */
	bool bAutoMasked = false;
	bool bAutoEmissive = false;
	/** Some textures of the current set were deleted: Create All stops here. */
	bool bSetHasMissing = false;

	struct FTextureState
	{
		TextureCompressionSettings Compression;
		bool bSRGB;
		bool bVirtual;
		bool bWasDirty;
		/** The settings Glaze set: Cancel reverts only while the texture still has them. */
		TextureCompressionSettings FixedCompression = TC_Default;
		bool bFixedSRGB = false;
		bool bFixedVirtual = false;
		/** Changed outside Glaze after the fix (e.g. in the Texture Editor): Cancel keeps it marked unsaved. */
		bool bEditedElsewhere = false;
	};
	bool bFixingTextures = false;
	TMap<TWeakObjectPtr<UTexture2D>, FTextureState> FixedTextures;

	TArray<TPair<FString, TArray<TObjectPtr<UTexture2D>>>> Sets;
	FString OutputFolder;
	int32 SetIndex = 0;
	TArray<FString> MatchNotes;

	struct FTarget
	{
		TWeakObjectPtr<UMeshComponent> Component;
		TArray<TObjectPtr<UMaterialInterface>> OriginalOverrides;
		UMaterialInterface* Original(int32 Index) const { return OriginalOverrides.IsValidIndex(Index) ? OriginalOverrides[Index].Get() : nullptr; }
	};
	TArray<FTarget> Targets;
	bool bAssignToSelection = true;

	TSharedPtr<IDetailsView> Details;
	TSharedPtr<STextBlock> HeaderText;
	TSharedPtr<STextBlock> InfoText;
	TWeakPtr<SWindow> Window;
	bool bClosing = false;
};
