#pragma once

#include "CoreMinimal.h"
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
	static void OpenForTextures(const TArray<UTexture2D*>& Textures);
	/** Reopens a Glaze instance with its saved recipe. */
	static void OpenForInstance(UMaterialInstanceConstant* Instance);

	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	// Keeps the original materials of preview targets alive while the preview replaces them.
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

	FReply OnCreate();
	FReply OnCreateAll();
	FReply OnSkip();
	FReply OnCancel();
	bool CreateCurrent();
	void NextOrClose();
	void Close();

	FText GetCreateLabel() const;
	bool HasMoreSets() const { return SetIndex + 1 < Sets.Num(); }

	TStrongObjectPtr<UGlazeRecipe> Recipe;
	TStrongObjectPtr<UMaterialInstanceConstant> PreviewInstance;
	FString PreviewStaticKey;

	/** Set when editing an existing instance ("Edit with Glaze"). */
	TWeakObjectPtr<UMaterialInstanceConstant> EditTarget;

	TArray<TPair<FString, TArray<UTexture2D*>>> Sets;
	int32 SetIndex = 0;
	TArray<FString> MatchNotes;

	struct FTarget
	{
		TWeakObjectPtr<UMeshComponent> Component;
		TArray<TObjectPtr<UMaterialInterface>> OriginalOverrides;
	};
	TArray<FTarget> Targets;
	bool bAssignToSelection = true;

	TSharedPtr<IDetailsView> Details;
	TSharedPtr<STextBlock> HeaderText;
	TSharedPtr<STextBlock> InfoText;
	TWeakPtr<SWindow> Window;
	bool bClosing = false;
};
