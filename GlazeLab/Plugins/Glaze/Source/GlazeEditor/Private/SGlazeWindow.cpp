#include "SGlazeWindow.h"

#include "Components/MeshComponent.h"
#include "Editor.h"
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
	Recipe.Reset(NewObject<UGlazeRecipe>());

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bHideSelectionTip = true;
	Args.bShowOptions = false;
	Args.bAllowSearch = true;
	Details = PropertyEditor.CreateDetailView(Args);

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
				.IsChecked_Lambda([this] { return bAssignToSelection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bAssignToSelection = S == ECheckBoxState::Checked; })
				.ToolTipText(LOCTEXT("AssignTip", "After Create, the new material stays on the meshes selected in the level (undoable). Otherwise their materials are restored."))
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
	RestoreTargets();
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
			W->RestoreTargets();
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
		Sets = { TPair<FString, TArray<UTexture2D*>>(Sets[0].Key, Textures) };
	}

	TSharedRef<SGlazeWindow> W = Open(LOCTEXT("TitleCreate", "Glaze: Create Material"));
	W->Sets = MoveTemp(Sets);
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
	if (!bKeepSettings)
	{
		Recipe->OnChanged.RemoveAll(this);
		Recipe.Reset(NewObject<UGlazeRecipe>());
	}
	for (int32 I = 0; I < static_cast<int32>(EGlazeSlot::Num); ++I)
	{
		Recipe->SetSlot(static_cast<EGlazeSlot>(I), nullptr);
	}

	const TArray<UTexture2D*>& Textures = Sets[Index].Value;
	const Glaze::FTextureSetMatch Match = Glaze::MatchTextures(Textures, *Recipe);
	if (!bKeepSettings)
	{
		Recipe->RoughnessRemap.bInvert = Match.bInvertRoughness;
		Recipe->bFlipGreen = Match.bFlipGreen;
		Recipe->bEmissive = Recipe->Emissive != nullptr;
		if (Recipe->Opacity)
		{
			Recipe->BlendMode = EGlazeBlend::Masked;
		}
	}
	Recipe->AssetName = TEXT("MI_") + (Match.SetName.IsEmpty() ? Sets[Index].Key : Match.SetName);
	if (!OutputFolder.IsEmpty())
	{
		Recipe->Folder.Path = OutputFolder;
	}
	else if (Textures.Num() > 0 && Textures[0])
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
	if (Recipe->Height)
	{
		MatchNotes.Add(TEXT("Height map found: not used by the material yet."));
	}

	if (Recipe->bFixTextureSettings)
	{
		Glaze::FixTextureSettings(*Recipe);
	}
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
	if (IsTextureSlotProperty(Name))
	{
		if (Recipe->bFixTextureSettings)
		{
			Glaze::FixTextureSettings(*Recipe);
		}
		if (Name == GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Emissive) && Recipe->Emissive)
		{
			Recipe->bEmissive = true;
		}
		if (Name == GET_MEMBER_NAME_CHECKED(UGlazeRecipe, Opacity) && Recipe->Opacity)
		{
			Recipe->BlendMode = EGlazeBlend::Masked;
		}
	}
	RefreshPreview();
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
		UMaterialInstanceConstant* MI = NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transient);
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
				if (Component->GetMaterial(I) != PreviewInstance.Get())
				{
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
		if (UMeshComponent* Component = Target.Component.Get())
		{
			Component->OverrideMaterials = Target.OriginalOverrides;
			Component->MarkRenderStateDirty();
		}
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
	UMaterialInstanceConstant* MI = Glaze::WriteInstanceAsset(*Recipe, EditTarget.Get(), Error);
	if (!MI)
	{
		Notify(FText::FromString(Error), false);
		return false;
	}
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
	while (CreateCurrent() && HasMoreSets())
	{
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

void SGlazeWindow::Close()
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;
	RestoreTargets();
	if (TSharedPtr<SWindow> W = Window.Pin())
	{
		W->RequestDestroyWindow();
	}
}

FText SGlazeWindow::GetCreateLabel() const
{
	if (EditTarget.IsValid())
	{
		return LOCTEXT("Apply", "Apply");
	}
	return HasMoreSets() ? LOCTEXT("CreateNext", "Create & Next") : LOCTEXT("Create", "Create");
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
