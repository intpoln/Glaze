// Copyright 2026 Nikita Polushin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/GCObject.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SWindow;
class SWrapBox;
class UGlazeNoiseSettings;
class UTexture2D;

/** "Glaze: Noise from Pack": pick a pack noise from a gallery, adjust it with a live preview, create a texture asset. */
class SGlazeNoiseWindow : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SGlazeNoiseWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGlazeNoiseWindow() override;

	/** Folder: where the texture goes by default (the Content Browser folder). */
	static void Open(const FString& Folder);
	/** Module shutdown. */
	static void CloseAll();
	static TSharedPtr<SGlazeNoiseWindow> Get();

	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SGlazeNoiseWindow"); }

private:
	friend class UGlazeEditorLibrary;
	struct FItem
	{
		FString Name;
		TObjectPtr<UTexture2D> Thumbnail;
		FSlateBrush Brush;
	};

	void BuildGallery();
	void LoadThumbnails();
	void Select(int32 Index);
	void OnSettingsChanged(const struct FPropertyChangedEvent& Event);
	void UpdatePreview();
	FReply OnCreate();
	void Close();

	TStrongObjectPtr<UGlazeNoiseSettings> Settings;
	TArray<TSharedPtr<FItem>> Items;
	int32 Selected = INDEX_NONE;
	bool bNameEdited = false;
	/** Package path of the last created texture (scripting). */
	FString LastCreated;

	/** The selected noise at preview size, before adjustments. */
	TArray<uint8> SourcePixels;
	int32 SourceSize = 0;
	TObjectPtr<UTexture2D> PreviewTexture;
	FSlateBrush PreviewBrush;

	TSharedPtr<IDetailsView> Details;
	TSharedPtr<SWrapBox> Gallery;
	TWeakPtr<SWindow> Window;
};
