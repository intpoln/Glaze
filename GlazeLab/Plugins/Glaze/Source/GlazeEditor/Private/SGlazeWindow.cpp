#include "SGlazeWindow.h"

#include "Components/MeshComponent.h"
#include "Editor.h"
#include "EditorUndoClient.h"
#include "Engine/Selection.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "GlazeApply.h"
#include "GlazeMaster.h"
#include "GlazeMatcher.h"
#include "GlazeRecipe.h"
#include "IDetailsView.h"
#include "Interfaces/IMainFrameModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Glaze"

namespace
{
	TWeakPtr<SGlazeWindow> GOpenWindow;

	UGlazeRecipe* NewRecipe(UGlazeRecipe* CopyFrom = nullptr)
	{
		// Transactional: recipe edits are undoable. A new object per texture set, so an undo never brings back
		// the previous set's textures and name.
		UGlazeRecipe* R = CopyFrom ? DuplicateObject(CopyFrom, GetTransientPackage()) : NewObject<UGlazeRecipe>(GetTransientPackage());
		R->SetFlags(RF_Transactional);
		return R;
	}

	bool IsPreviewInstance(const UMaterialInterface* M)
	{
		return M && M->GetOuter() == GetTransientPackage() && M->GetName().StartsWith(TEXT("GlazePreview"));
	}

	/** Puts back Originals in the slots showing Preview; drops slots that only the preview added (empty override list). */
	void RestoreSlots(UMeshComponent* Component, const UMaterialInterface* Preview, const TArray<TObjectPtr<UMaterialInterface>>& Originals)
	{
		for (int32 I = 0; I < Component->OverrideMaterials.Num(); ++I)
		{
			if (Component->OverrideMaterials[I] == Preview)
			{
				Component->SetMaterial(I, Originals.IsValidIndex(I) ? Originals[I].Get() : nullptr);
			}
		}
		int32 Num = Component->OverrideMaterials.Num();
		while (Num > Originals.Num() && !Component->OverrideMaterials[Num - 1])
		{
			--Num;
		}
		if (Num != Component->OverrideMaterials.Num())
		{
			Component->OverrideMaterials.SetNum(Num);
			Component->MarkRenderStateDirty();
		}
	}
}

/**
 * Any transaction on a previewed component (a move, a property edit) records the preview material in its snapshot,
 * so an undo can bring it back after the window is closed. This puts the original materials back after every undo/redo.
 */
class FGlazePreviewGuard : public FEditorUndoClient, public FGCObject
{
public:
	FGlazePreviewGuard()
	{
		if (GEditor)
		{
			GEditor->RegisterForUndo(this);
		}
	}
	virtual ~FGlazePreviewGuard() override
	{
		if (GEditor)
		{
			GEditor->UnregisterForUndo(this);
		}
	}

	void Remember(UMaterialInterface* Preview, UMeshComponent* Component, const TArray<TObjectPtr<UMaterialInterface>>& Originals)
	{
		for (const FRecord& R : Records)
		{
			if (R.Preview == Preview && R.Component == Component)
			{
				return;
			}
		}
		Records.Add({ Preview, Component, Originals });
	}

	virtual void PostUndo(bool bSuccess) override { Fix(); }
	virtual void PostRedo(bool bSuccess) override { Fix(); }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		for (FRecord& R : Records)
		{
			Collector.AddReferencedObjects(R.Originals);
		}
	}
	virtual FString GetReferencerName() const override { return TEXT("FGlazePreviewGuard"); }

private:
	void Fix()
	{
		TSharedPtr<SGlazeWindow> Open = GOpenWindow.Pin();
		const UMaterialInterface* Live = Open && !Open->bClosing ? Open->PreviewInstance.Get() : nullptr;
		// The preview is kept alive only by the undo buffer: once it is gone, nothing can bring it back.
		Records.RemoveAll([](const FRecord& R) { return !R.Preview.IsValid() || !R.Component.IsValid(); });
		for (const FRecord& R : Records)
		{
			// The open window shows its own preview and restores it when it closes.
			if (R.Preview.Get() != Live)
			{
				RestoreSlots(R.Component.Get(), R.Preview.Get(), R.Originals);
			}
		}
		if (Open && !Open->bClosing)
		{
			Open->OnUndoRedo();
		}
	}

	struct FRecord
	{
		TWeakObjectPtr<UMaterialInterface> Preview;
		TWeakObjectPtr<UMeshComponent> Component;
		TArray<TObjectPtr<UMaterialInterface>> Originals;
	};
	TArray<FRecord> Records;
};

namespace
{
	TUniquePtr<FGlazePreviewGuard> GPreviewGuard;

	void Notify(const FText& Text, bool bSuccess)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 4.f;
		Info.bUseSuccessFailIcons = true;
		FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}

	bool IsTextureSlotProperty(FName Name)
	{
		static const TSet<FName> Slots = {
			GET_MEMBER_NAME_CHECKED(UGlazeRecipe, BaseColor), GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Normal),
			GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Roughness), GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Metallic),
			GET_MEMBER_NAME_CHECKED(UGlazeRecipe, AmbientOcclusion), GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Specular),
			GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Opacity), GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Emissive),
			GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Height), GET_MEMBER_NAME_CHECKED(UGlazeRecipe, bFixTextureSettings),
		};
		return Slots.Contains(Name);
	}
}

void SGlazeWindow::Construct(const FArguments& InArgs)
{
	Recipe.Reset(NewRecipe());
	if (!GPreviewGuard)
	{
		GPreviewGuard = MakeUnique<FGlazePreviewGuard>();
	}

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bHideSelectionTip = true;
	Args.bShowOptions = false;
	Args.bAllowSearch = true;
	Details = PropertyEditor.CreateDetailView(Args);
	// Apply updates the existing asset in place: it does not rename or move it.
	Details->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([this](const FPropertyAndParent& P)
	{
		const FName Name = P.Property.GetFName();
		return !EditTarget.IsValid()
			|| (Name != GET_MEMBER_NAME_CHECKED(UGlazeRecipe, AssetName) && Name != GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Folder));
	}));
	BindEditorEvents();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(10, 8)
		[
			SAssignNew(HeaderText, STextBlock).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			Details.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().MaxHeight(140.f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(8)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(InfoText, STextBlock).AutoWrapText(true)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsEnabled_Lambda([this] { return Sets.Num() <= 1 || EditTarget.IsValid(); })
				.IsChecked_Lambda([this] { return bAssignToSelection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bAssignToSelection = S == ECheckBoxState::Checked; })
				.ToolTipText(LOCTEXT("AssignTip", "After Create, the new material stays on the meshes selected in the level (undoable). Otherwise their materials are restored.\nNot available for several texture sets: the meshes only preview them."))
				[
					SNew(STextBlock).Text(LOCTEXT("Assign", "Keep on selected meshes"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
			[
				SNew(SButton).Text(LOCTEXT("Skip", "Skip")).OnClicked(this, &SGlazeWindow::OnSkip)
				.Visibility_Lambda([this] { return HasMoreSets() ? EVisibility::Visible : EVisibility::Collapsed; })
				.ToolTipText(LOCTEXT("SkipTip", "Skip this texture set and go to the next one."))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
			[
				SNew(SButton).Text(LOCTEXT("CreateAll", "Create All")).OnClicked(this, &SGlazeWindow::OnCreateAll)
				.Visibility_Lambda([this] { return HasMoreSets() ? EVisibility::Visible : EVisibility::Collapsed; })
				.ToolTipText(LOCTEXT("CreateAllTip", "Create this and all remaining sets with the current settings."))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
			[
				SNew(SButton).Text(LOCTEXT("Cancel", "Cancel")).OnClicked(this, &SGlazeWindow::OnCancel)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
			[
				SNew(SButton).ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.Text(this, &SGlazeWindow::GetCreateLabel)
				.ToolTipText(LOCTEXT("CreateTip", "Ctrl+Enter"))
				.OnClicked(this, &SGlazeWindow::OnCreate)
			]
		]
	];
}

SGlazeWindow::~SGlazeWindow()
{
	UnbindEditorEvents();
	RestoreTargets();
	RevertTextureFixes();
	if (Recipe)
	{
		Recipe->OnChanged.RemoveAll(this);
	}
}

void SGlazeWindow::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FTarget& Target : Targets)
	{
		Collector.AddReferencedObjects(Target.OriginalOverrides);
	}
	// A deleted texture is nulled here instead of leaving a dangling pointer in the queue.
	for (TPair<FString, TArray<TObjectPtr<UTexture2D>>>& Set : Sets)
	{
		Collector.AddReferencedObjects(Set.Value);
	}
}

void SGlazeWindow::BindEditorEvents()
{
	UPackage::PreSavePackageWithContextEvent.AddSPLambda(this, [this](UPackage* Package, FObjectPreSaveContext)
	{
		for (const FTarget& Target : Targets)
		{
			if (Target.Component.IsValid() && Target.Component->GetPackage() == Package)
			{
				RestoreTargets();
				return;
			}
		}
	});
	UPackage::PackageSavedWithContextEvent.AddSPLambda(this, [this](const FString&, UPackage*, FObjectPostSaveContext)
	{
		ApplyPreviewToTargets();
	});
	FCoreUObjectDelegates::OnObjectModified.AddSPLambda(this, [this](UObject* Object)
	{
		if (!bFixingTextures)
		{
			if (FTextureState* State = FixedTextures.Find(Cast<UTexture2D>(Object)))
			{
				State->bEditedElsewhere = true;
			}
		}
	});
	for (FSimpleMulticastDelegate* Begin : { &FEditorDelegates::OnDeleteActorsBegin, &FEditorDelegates::OnEditCutActorsBegin,
		&FEditorDelegates::OnEditCopyActorsBegin, &FEditorDelegates::OnDuplicateActorsBegin })
	{
		Begin->AddSP(this, &SGlazeWindow::RestoreTargets);
	}
	for (FSimpleMulticastDelegate* End : { &FEditorDelegates::OnDeleteActorsEnd, &FEditorDelegates::OnEditCutActorsEnd,
		&FEditorDelegates::OnEditCopyActorsEnd, &FEditorDelegates::OnDuplicateActorsEnd })
	{
		End->AddSP(this, &SGlazeWindow::ApplyPreviewToTargets);
	}
}

void SGlazeWindow::UnbindEditorEvents()
{
	UPackage::PreSavePackageWithContextEvent.RemoveAll(this);
	UPackage::PackageSavedWithContextEvent.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectModified.RemoveAll(this);
	for (FSimpleMulticastDelegate* Event : { &FEditorDelegates::OnDeleteActorsBegin, &FEditorDelegates::OnEditCutActorsBegin,
		&FEditorDelegates::OnEditCopyActorsBegin, &FEditorDelegates::OnDuplicateActorsBegin, &FEditorDelegates::OnDeleteActorsEnd,
		&FEditorDelegates::OnEditCutActorsEnd, &FEditorDelegates::OnEditCopyActorsEnd, &FEditorDelegates::OnDuplicateActorsEnd })
	{
		Event->RemoveAll(this);
	}
}

TSharedRef<SGlazeWindow> SGlazeWindow::Open(const FText& Title)
{
	if (TSharedPtr<SGlazeWindow> Previous = GOpenWindow.Pin())
	{
		Previous->Close();
	}

	TSharedRef<SGlazeWindow> Widget = SNew(SGlazeWindow);
	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(520, 860))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			Widget
		];
	Widget->Window = NewWindow;
	NewWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([WeakWidget = TWeakPtr<SGlazeWindow>(Widget)](const TSharedRef<SWindow>&)
	{
		if (TSharedPtr<SGlazeWindow> W = WeakWidget.Pin())
		{
			W->UnbindEditorEvents();
			W->RestoreTargets();
			W->RevertTextureFixes();
		}
	}));

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	if (TSharedPtr<SWindow> Parent = MainFrame.GetParentWindow())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(NewWindow, Parent.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(NewWindow);
	}
	FSlateApplication::Get().SetKeyboardFocus(Widget);
	GOpenWindow = Widget;
	return Widget;
}

void SGlazeWindow::OpenForTextures(const TArray<UTexture2D*>& Textures, const FString& OutputFolder)
{
	if (Textures.Num() == 0)
	{
		return;
	}
	TArray<TPair<FString, TArray<UTexture2D*>>> Sets = Glaze::GroupTextureSets(Textures);
	// Names without a recognizable slot give one "set" per texture: then it is one material, not a batch.
	if (Sets.Num() > 1 && Sets.Num() == Textures.Num())
	{
		bool bAnyMatched = false;
		for (const TPair<FString, TArray<UTexture2D*>>& Set : Sets)
		{
			UGlazeRecipe* Scratch = NewObject<UGlazeRecipe>();
			bAnyMatched |= Glaze::MatchTextures(Set.Value, *Scratch).Unmatched.Num() < Set.Value.Num();
		}
		if (!bAnyMatched)
		{
			Sets = { TPair<FString, TArray<UTexture2D*>>(Sets[0].Key, Textures) };
		}
	}

	TSharedRef<SGlazeWindow> W = Open(LOCTEXT("TitleCreate", "Glaze: Create Material"));
	for (const TPair<FString, TArray<UTexture2D*>>& Set : Sets)
	{
		W->Sets.Emplace(Set.Key, TArray<TObjectPtr<UTexture2D>>(Set.Value));
	}
	W->OutputFolder = OutputFolder;
	W->CapturePreviewTargets();
	W->StartSet(0, false);
}

void SGlazeWindow::OpenForInstance(UMaterialInstanceConstant* Instance)
{
	if (!Instance)
	{
		return;
	}
	// The menu only checks instances that are already loaded.
	if (!Glaze::IsGlazeInstance(Instance))
	{
		Notify(FText::Format(LOCTEXT("NotGlaze", "{0} was not made by Glaze (its parent is not M_Glaze)."), FText::FromString(Instance->GetName())), false);
		return;
	}
	if (TSharedPtr<SGlazeWindow> Existing = GOpenWindow.Pin())
	{
		if (Existing->EditTarget == Instance && Existing->Window.IsValid())
		{
			Existing->Window.Pin()->BringToFront();
			return;
		}
	}
	TSharedRef<SGlazeWindow> W = Open(FText::Format(LOCTEXT("TitleEdit", "Glaze: {0}"), FText::FromString(Instance->GetName())));
	W->EditTarget = Instance;
	if (!Glaze::ReadRecipe(Instance, *W->Recipe))
	{
		W->MatchNotes.Add(TEXT("No saved Glaze recipe: settings start from defaults."));
	}
	W->Recipe->AssetName = Instance->GetName();
	W->Recipe->Folder.Path = FPackageName::GetLongPackagePath(Instance->GetPackage()->GetName());
	W->Recipe->OnChanged.AddSP(W, &SGlazeWindow::OnRecipeChanged);
	W->Details->SetObject(W->Recipe.Get());
	W->CapturePreviewTargets();
	W->RefreshPreview();
	W->RefreshInfo();
}

void SGlazeWindow::StartSet(int32 Index, bool bKeepSettings)
{
	SetIndex = Index;
	MatchNotes.Reset();
	Recipe->OnChanged.RemoveAll(this);
	Recipe.Reset(NewRecipe(bKeepSettings ? Recipe.Get() : nullptr));
	for (int32 I = 0; I < static_cast<int32>(EGlazeSlot::Num); ++I)
	{
		Recipe->SetSlot(static_cast<EGlazeSlot>(I), nullptr);
	}

	UnsavedInstance.Reset();
	TArray<UTexture2D*> Textures;
	int32 Missing = 0;
	for (UTexture2D* T : Sets[Index].Value)
	{
		if (IsValid(T))
		{
			Textures.Add(T);
		}
		else
		{
			++Missing;
		}
	}
	bSetHasMissing = Missing > 0;
	if (Missing > 0)
	{
		MatchNotes.Add(FString::Printf(TEXT("Warning: %d texture(s) of this set were deleted."), Missing));
	}
	const Glaze::FTextureSetMatch Match = Glaze::MatchTextures(Textures, *Recipe);
	// These describe the maps of this set, so every set gets its own; the other settings carry over in a batch.
	Recipe->RoughnessRemap.bInvert = Match.bInvertRoughness;
	Recipe->bFlipGreen = Match.bFlipGreen;
	// Switched on for the previous set's maps: off again when this set has no such map. A user's choice carries over.
	if (Recipe->Emissive)
	{
		bAutoEmissive = !Recipe->bEmissive || bAutoEmissive || !bKeepSettings;
		Recipe->bEmissive = true;
	}
	else if (!bKeepSettings || bAutoEmissive)
	{
		Recipe->bEmissive = false;
		bAutoEmissive = false;
	}
	if (Recipe->Opacity)
	{
		bAutoMasked = Recipe->BlendMode != EGlazeBlend::Masked || bAutoMasked || !bKeepSettings;
		Recipe->BlendMode = EGlazeBlend::Masked;
	}
	else if (bAutoMasked)
	{
		Recipe->BlendMode = EGlazeBlend::Opaque;
		bAutoMasked = false;
	}
	Recipe->AssetName = TEXT("MI_") + (Match.SetName.IsEmpty() ? Sets[Index].Key : Match.SetName);
	if (bFolderEdited)
	{
		// keep the folder the user picked
	}
	else if (!OutputFolder.IsEmpty())
	{
		Recipe->Folder.Path = OutputFolder;
	}
	else if (Textures.Num() > 0)
	{
		Recipe->Folder.Path = FPackageName::GetLongPackagePath(Textures[0]->GetPackage()->GetName());
	}

	for (int32 I = 0; I < static_cast<int32>(EGlazeSlot::Num); ++I)
	{
		const EGlazeSlot Slot = static_cast<EGlazeSlot>(I);
		if (UTexture2D* T = Recipe->GetSlot(Slot))
		{
			const FString* Reason = Match.Reasons.Find(Slot);
			MatchNotes.Add(FString::Printf(TEXT("%s  <-  %s  (%s)"), Glaze::SlotName(Slot), *T->GetName(), Reason ? **Reason : TEXT("")));
		}
	}
	for (UTexture2D* T : Match.Unmatched)
	{
		MatchNotes.Add(FString::Printf(TEXT("Not matched: %s (drag it into a slot)"), *T->GetName()));
	}

	FixTextures();
	if (!Recipe->OnChanged.IsBoundToObject(this))
	{
		Recipe->OnChanged.AddSP(this, &SGlazeWindow::OnRecipeChanged);
	}
	Details->SetObject(Recipe.Get(), /*bForceRefresh*/ true);
	RefreshPreview();
	RefreshInfo();
}

void SGlazeWindow::OnRecipeChanged(const FPropertyChangedEvent& Event)
{
	const FName Name = Event.GetMemberPropertyName();
	if (Name == GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Folder))
	{
		bFolderEdited = true;
	}
	if (Name == GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Emissive) && Recipe->Emissive && !Recipe->bEmissive)
	{
		Recipe->bEmissive = true;
		bAutoEmissive = true;
	}
	if (Name == GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Opacity) && Recipe->Opacity && Recipe->BlendMode != EGlazeBlend::Masked)
	{
		Recipe->BlendMode = EGlazeBlend::Masked;
		bAutoMasked = true;
	}
	if (Name == GET_MEMBER_NAME_CHECKED(UGlazeRecipe, bEmissive))
	{
		bAutoEmissive = false;
	}
	if (Name == GET_MEMBER_NAME_CHECKED(UGlazeRecipe, BlendMode))
	{
		bAutoMasked = false;
	}
	// After the switches above: a map is fixed only once the material samples it (blend mode, emissive, projection...).
	if (Event.ChangeType != EPropertyChangeType::Interactive)
	{
		FixTextures();
	}
	// The clip value is compiled into the shader: recompile once on release, not on every step of a drag.
	if (Event.ChangeType != EPropertyChangeType::Interactive || Name != GET_MEMBER_NAME_CHECKED(UGlazeRecipe, OpacityClip))
	{
		RefreshPreview();
	}
	if (Event.ChangeType != EPropertyChangeType::Interactive)
	{
		RefreshInfo();
	}
}

void SGlazeWindow::RefreshPreview()
{
	if (!PreviewInstance)
	{
		UMaterial* Master = Glaze::EnsureMaster();
		if (!Master)
		{
			return;
		}
		UMaterialInstanceConstant* MI = NewObject<UMaterialInstanceConstant>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UMaterialInstanceConstant::StaticClass(), TEXT("GlazePreview")), RF_Transient);
		MI->SetParentEditorOnly(Master);
		PreviewInstance.Reset(MI);
		PreviewStaticKey.Reset();
	}
	Glaze::ApplyRecipe(PreviewInstance.Get(), *Recipe, PreviewStaticKey);
	ApplyPreviewToTargets();
}

void SGlazeWindow::RefreshInfo()
{
	if (EditTarget.IsValid())
	{
		HeaderText->SetText(FText::Format(LOCTEXT("HeaderEdit", "Editing {0}"), FText::FromString(EditTarget->GetName())));
	}
	else if (Sets.Num() > 1)
	{
		HeaderText->SetText(FText::Format(LOCTEXT("HeaderSets", "{0}   (set {1} of {2})"), FText::FromString(Sets[SetIndex].Key),
			FText::AsNumber(SetIndex + 1), FText::AsNumber(Sets.Num())));
	}
	else
	{
		HeaderText->SetText(FText::FromString(Recipe->AssetName));
	}

	TArray<FString> Lines = MatchNotes;
	TArray<FString> Warnings;
	const TArray<Glaze::FTextureFix> Fixes = Glaze::PlanTextureFixes(*Recipe, &Warnings);
	for (const Glaze::FTextureFix& Fix : Fixes)
	{
		Lines.Add(TEXT("Texture settings to fix: ") + Fix.Description);
	}
	for (const FString& W : Warnings)
	{
		Lines.Add(TEXT("Warning: ") + W);
	}
	int32 Meshes = 0;
	for (const FTarget& T : Targets)
	{
		Meshes += T.Component.IsValid() ? 1 : 0;
	}
	Lines.Add(Meshes > 0
		? FString::Printf(TEXT("Live preview on %d selected mesh component(s)."), Meshes)
		: TEXT("Tip: select meshes in the level before opening Glaze to preview the material on them."));
	InfoText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void SGlazeWindow::CapturePreviewTargets()
{
	RestoreTargets();
	Targets.Reset();
	if (!GEditor)
	{
		return;
	}
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			TInlineComponentArray<UMeshComponent*> Components(Actor);
			for (UMeshComponent* Component : Components)
			{
				if (Component && Component->GetNumMaterials() > 0)
				{
					Targets.Add({ Component, Component->OverrideMaterials });
				}
			}
		}
	}
}

void SGlazeWindow::ApplyPreviewToTargets()
{
	if (!PreviewInstance)
	{
		return;
	}
	for (FTarget& Target : Targets)
	{
		if (UMeshComponent* Component = Target.Component.Get())
		{
			for (int32 I = 0; I < Component->GetNumMaterials(); ++I)
			{
				// A slot the user changed while the window is open is theirs: leave it.
				UMaterialInterface* Current = Component->OverrideMaterials.IsValidIndex(I) ? Component->OverrideMaterials[I].Get() : nullptr;
				if (Current != PreviewInstance.Get() && Current == Target.Original(I))
				{
					if (GPreviewGuard)
					{
						GPreviewGuard->Remember(PreviewInstance.Get(), Component, Target.OriginalOverrides);
					}
					Component->SetMaterial(I, PreviewInstance.Get());
				}
			}
		}
	}
}

void SGlazeWindow::RestoreTargets()
{
	for (FTarget& Target : Targets)
	{
		UMeshComponent* Component = Target.Component.Get();
		if (!Component || !PreviewInstance)
		{
			continue;
		}
		// Only slots still showing the preview: an undo or a material assigned meanwhile is kept.
		RestoreSlots(Component, PreviewInstance.Get(), Target.OriginalOverrides);
	}
}

void SGlazeWindow::AssignToTargets(UMaterialInterface* Material)
{
	RestoreTargets();
	FScopedTransaction Transaction(LOCTEXT("Assign", "Glaze: Assign Material"));
	for (FTarget& Target : Targets)
	{
		if (UMeshComponent* Component = Target.Component.Get())
		{
			Component->Modify();
			for (int32 I = 0; I < Component->GetNumMaterials(); ++I)
			{
				Component->SetMaterial(I, Material);
			}
			Target.OriginalOverrides = Component->OverrideMaterials; // nothing to restore any more
		}
	}
}

bool SGlazeWindow::CreateCurrent()
{
	FString Error;
	UMaterialInstanceConstant* Target = EditTarget.IsValid() ? EditTarget.Get() : UnsavedInstance.Get();
	TArray<UTexture2D*> FixedEarlier;
	for (const TPair<TWeakObjectPtr<UTexture2D>, FTextureState>& Pair : FixedTextures)
	{
		if (UTexture2D* T = Pair.Key.Get())
		{
			FixedEarlier.Add(T);
		}
	}
	UMaterialInstanceConstant* MI = Glaze::WriteInstanceAsset(*Recipe, Target, Error, FixedEarlier);
	if (!MI || !Error.IsEmpty())
	{
		// Created but not saved: keep the window, the next Create retries the save on the same asset.
		UnsavedInstance = MI;
		Notify(FText::FromString(Error), false);
		return false;
	}
	UnsavedInstance.Reset();
	ForgetSavedTextureFixes();
	if (bAssignToSelection && Targets.Num() > 0 && (Sets.Num() <= 1 || EditTarget.IsValid()))
	{
		AssignToTargets(MI);
	}
	if (GEditor)
	{
		TArray<UObject*> Objects = { MI };
		GEditor->SyncBrowserToObjects(Objects);
	}
	Notify(FText::Format(EditTarget.IsValid() ? LOCTEXT("Updated", "Updated {0}") : LOCTEXT("Created", "Created {0}"),
		FText::FromString(MI->GetName())), true);
	return true;
}

FReply SGlazeWindow::OnCreate()
{
	if (CreateCurrent())
	{
		NextOrClose();
	}
	return FReply::Handled();
}

FReply SGlazeWindow::OnCreateAll()
{
	while (true)
	{
		if (bSetHasMissing)
		{
			Notify(LOCTEXT("SetMissing", "Some textures of this set were deleted: check it, then Create or Skip."), false);
			return FReply::Handled();
		}
		if (!CreateCurrent())
		{
			return FReply::Handled(); // stay on the failed set
		}
		if (!HasMoreSets())
		{
			break;
		}
		StartSet(SetIndex + 1, true);
	}
	Close();
	return FReply::Handled();
}

FReply SGlazeWindow::OnSkip()
{
	NextOrClose();
	return FReply::Handled();
}

FReply SGlazeWindow::OnCancel()
{
	Close();
	return FReply::Handled();
}

void SGlazeWindow::NextOrClose()
{
	if (!EditTarget.IsValid() && HasMoreSets())
	{
		StartSet(SetIndex + 1, true);
	}
	else
	{
		Close();
	}
}

void SGlazeWindow::Close(bool bImmediate)
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;
	UnbindEditorEvents();
	RestoreTargets();
	RevertTextureFixes();
	if (TSharedPtr<SWindow> W = Window.Pin())
	{
		if (bImmediate)
		{
			FSlateApplication::Get().DestroyWindowImmediately(W.ToSharedRef());
		}
		else
		{
			W->RequestDestroyWindow();
		}
	}
}

void SGlazeWindow::CloseAll()
{
	if (TSharedPtr<SGlazeWindow> W = GOpenWindow.Pin())
	{
		W->Close(/*bImmediate*/ true);
	}
	GOpenWindow.Reset();
	GPreviewGuard.Reset();
}

void SGlazeWindow::FixTextures()
{
	if (!Recipe->bFixTextureSettings)
	{
		return;
	}
	for (const Glaze::FTextureFix& Fix : Glaze::PlanTextureFixes(*Recipe))
	{
		UTexture2D* T = Fix.Texture;
		if (!FixedTextures.Contains(T))
		{
			FixedTextures.Add(T, { T->CompressionSettings, T->SRGB != 0, T->VirtualTextureStreaming != 0, T->GetPackage()->IsDirty() });
		}
	}
	TGuardValue<bool> Fixing(bFixingTextures, true);
	for (UTexture2D* T : Glaze::FixTextureSettings(*Recipe))
	{
		if (FTextureState* S = FixedTextures.Find(T))
		{
			S->FixedCompression = T->CompressionSettings;
			S->bFixedSRGB = T->SRGB != 0;
			S->bFixedVirtual = T->VirtualTextureStreaming != 0;
		}
	}
}

void SGlazeWindow::OnUndoRedo()
{
	Details->ForceRefresh();
	RefreshPreview();
	RefreshInfo();
}

void SGlazeWindow::ForgetSavedTextureFixes()
{
	for (auto It = FixedTextures.CreateIterator(); It; ++It)
	{
		UTexture2D* T = It.Key().Get();
		if (!T || !T->GetPackage()->IsDirty())
		{
			It.RemoveCurrent();
		}
	}
}

void SGlazeWindow::RevertTextureFixes()
{
	ForgetSavedTextureFixes();
	TGuardValue<bool> Fixing(bFixingTextures, true);
	for (const TPair<TWeakObjectPtr<UTexture2D>, FTextureState>& Pair : FixedTextures)
	{
		UTexture2D* T = Pair.Key.Get();
		const FTextureState& S = Pair.Value;
		// Changed again since (by the user or an undo): those settings are theirs.
		if (T->CompressionSettings != S.FixedCompression || (T->SRGB != 0) != S.bFixedSRGB || (T->VirtualTextureStreaming != 0) != S.bFixedVirtual)
		{
			continue;
		}
		T->PreEditChange(nullptr);
		T->CompressionSettings = S.Compression;
		T->SRGB = S.bSRGB;
		T->VirtualTextureStreaming = S.bVirtual;
		T->PostEditChange();
		if (!S.bWasDirty && !S.bEditedElsewhere)
		{
			T->GetPackage()->SetDirtyFlag(false);
		}
	}
	FixedTextures.Reset();
}

FText SGlazeWindow::GetCreateLabel() const
{
	if (EditTarget.IsValid())
	{
		return LOCTEXT("Apply", "Apply");
	}
	return HasMoreSets() ? LOCTEXT("CreateNext", "Create & Next") : LOCTEXT("Create", "Create");
}

FReply SGlazeWindow::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	// Before the focused text field eats Enter; taking the focus first commits its value.
	if (KeyEvent.GetKey() == EKeys::Enter && KeyEvent.IsControlDown())
	{
		FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
		return OnCreate();
	}
	return SCompoundWidget::OnPreviewKeyDown(Geometry, KeyEvent);
}

FReply SGlazeWindow::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Enter && KeyEvent.IsControlDown())
	{
		return OnCreate();
	}
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCancel();
	}
	return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
}

#undef LOCTEXT_NAMESPACE
