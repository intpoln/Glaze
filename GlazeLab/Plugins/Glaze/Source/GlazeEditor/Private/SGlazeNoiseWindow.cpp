#include "SGlazeNoiseWindow.h"

#include "AssetToolsModule.h"
#include "Async/Async.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GlazeNoise.h"
#include "GlazePack.h"
#include "IDetailsView.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/PackageName.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GlazeNoise"

DEFINE_LOG_CATEGORY_STATIC(LogGlazeNoise, Log, All);

namespace
{
	TWeakPtr<SGlazeNoiseWindow> GOpenNoiseWindow;

	constexpr int32 ThumbnailSize = 96;
	constexpr int32 PreviewSize = 512;
	constexpr float PreviewDisplaySize = 440.f;

	/** Gray through the value map, as opaque BGRA for a transient texture. */
	TArray<uint8> ToBGRA(const TArray<uint8>& Gray, const TArray<uint8>* Lut)
	{
		TArray<uint8> Out;
		Out.SetNumUninitialized(Gray.Num() * 4);
		for (int32 I = 0; I < Gray.Num(); ++I)
		{
			const uint8 V = Lut ? (*Lut)[Gray[I]] : Gray[I];
			Out[I * 4 + 0] = V;
			Out[I * 4 + 1] = V;
			Out[I * 4 + 2] = V;
			Out[I * 4 + 3] = 255;
		}
		return Out;
	}

	FString DisplayName(const FString& Name)
	{
		return Name.StartsWith(TEXT("T_Noise_")) ? Name.RightChop(8) : Name;
	}

	void Notify(const FText& Text, bool bSuccess)
	{
		UE_LOG(LogGlazeNoise, Display, TEXT("%s: %s"), bSuccess ? TEXT("OK") : TEXT("FAILED"), *Text.ToString());
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 4.f;
		Info.bUseSuccessFailIcons = true;
		FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

void SGlazeNoiseWindow::Construct(const FArguments& InArgs)
{
	Settings.Reset(NewObject<UGlazeNoiseSettings>(GetTransientPackage()));
	Settings->OnChanged.AddSP(this, &SGlazeNoiseWindow::OnSettingsChanged);

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.bHideSelectionTip = true;
	Args.bShowOptions = false;
	Args.bAllowSearch = false;
	Details = PropertyEditor.CreateDetailView(Args);
	Details->SetObject(Settings.Get());

	PreviewBrush.DrawAs = ESlateBrushDrawType::Image;
	PreviewBrush.ImageSize = FVector2D(PreviewDisplaySize);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			// Gallery
			+ SHorizontalBox::Slot().AutoWidth().Padding(6)
			[
				SNew(SBox).WidthOverride(3 * (ThumbnailSize + 24) + 20)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(Gallery, SWrapBox).UseAllottedSize(true)
					]
				]
			]
			// Preview
			+ SHorizontalBox::Slot().AutoWidth().Padding(6).VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(PreviewDisplaySize).HeightOverride(PreviewDisplaySize)
					[
						SNew(SImage).Image(&PreviewBrush)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						return Items.IsValidIndex(Selected) ? FText::FromString(DisplayName(Items[Selected]->Name)) : LOCTEXT("PickNoise", "Pick a noise on the left.");
					})
					.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
				]
			]
			// Settings
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6)
			[
				Details.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8).HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
			[
				SNew(SButton).Text(LOCTEXT("Cancel", "Cancel")).OnClicked_Lambda([this] { Close(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)
			[
				SNew(SButton).ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.Text(LOCTEXT("Create", "Create Texture"))
				.ToolTipText(LOCTEXT("CreateTip", "Bakes the adjustments into a new grayscale texture (Ctrl+Enter)."))
				.IsEnabled_Lambda([this] { return Items.IsValidIndex(Selected); })
				.OnClicked(this, &SGlazeNoiseWindow::OnCreate)
			]
		]
	];
}

SGlazeNoiseWindow::~SGlazeNoiseWindow()
{
	if (Settings)
	{
		Settings->OnChanged.RemoveAll(this);
	}
}

void SGlazeNoiseWindow::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TSharedPtr<FItem>& Item : Items)
	{
		Collector.AddReferencedObject(Item->Thumbnail);
	}
	Collector.AddReferencedObject(PreviewTexture);
}

void SGlazeNoiseWindow::Open(const FString& Folder)
{
	if (TSharedPtr<SGlazeNoiseWindow> Existing = GOpenNoiseWindow.Pin())
	{
		if (TSharedPtr<SWindow> W = Existing->Window.Pin())
		{
			W->BringToFront();
			return;
		}
	}
	TSharedRef<SGlazeNoiseWindow> Widget = SNew(SGlazeNoiseWindow);
	Widget->Settings->Folder.Path = Folder.IsEmpty() ? FString(TEXT("/Game")) : Folder;
	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(LOCTEXT("Title", "Glaze: Noise from Pack"))
		.ClientSize(FVector2D(1260, 600))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			Widget
		];
	Widget->Window = NewWindow;
	Widget->BuildGallery();
	Widget->LoadThumbnails();
	Widget->Select(0);

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
	GOpenNoiseWindow = Widget;
}

TSharedPtr<SGlazeNoiseWindow> SGlazeNoiseWindow::Get()
{
	return GOpenNoiseWindow.Pin();
}

void SGlazeNoiseWindow::CloseAll()
{
	if (TSharedPtr<SGlazeNoiseWindow> W = GOpenNoiseWindow.Pin())
	{
		if (TSharedPtr<SWindow> Win = W->Window.Pin())
		{
			FSlateApplication::Get().DestroyWindowImmediately(Win.ToSharedRef());
		}
	}
	GOpenNoiseWindow.Reset();
}

void SGlazeNoiseWindow::BuildGallery()
{
	for (const FString& Name : Glaze::Pack::Noises())
	{
		TSharedRef<FItem> Item = MakeShared<FItem>();
		Item->Name = Name;
		Item->Brush.DrawAs = ESlateBrushDrawType::Image;
		Item->Brush.ImageSize = FVector2D(ThumbnailSize);
		const int32 Index = Items.Add(Item);
		Gallery->AddSlot().Padding(3)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(FText::FromString(Name))
			.OnClicked_Lambda([this, Index] { Select(Index); return FReply::Handled(); })
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor_Lambda([this, Index]
				{
					return Index == Selected ? FAppStyle::GetSlateColor("SelectionColor") : FSlateColor(FLinearColor::Transparent);
				})
				.Padding(3)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).WidthOverride(ThumbnailSize).HeightOverride(ThumbnailSize)
						[
							SNew(SImage).Image(&Item->Brush)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2)
					[
						SNew(SBox).WidthOverride(ThumbnailSize)
						[
							SNew(STextBlock).Text(FText::FromString(DisplayName(Name))).Justification(ETextJustify::Center)
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						]
					]
				]
			]
		];
	}
}

void SGlazeNoiseWindow::LoadThumbnails()
{
	// PNG decoding off the game thread: the window opens at once and thumbnails appear as they are ready.
	TWeakPtr<SGlazeNoiseWindow> Weak = SharedThis(this);
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FString File = Glaze::Noise::PackFile(Items[Index]->Name, 1024);
		Async(EAsyncExecution::ThreadPool, [Weak, Index, File]
		{
			TArray<uint8> Pixels;
			int32 Size = 0;
			if (!Glaze::Noise::LoadGray(File, ThumbnailSize, Pixels, Size))
			{
				return;
			}
			AsyncTask(ENamedThreads::GameThread, [Weak, Index, Pixels = MoveTemp(Pixels), Size]
			{
				TSharedPtr<SGlazeNoiseWindow> This = Weak.Pin();
				if (!This || !This->Items.IsValidIndex(Index))
				{
					return;
				}
				FItem& Item = *This->Items[Index];
				Item.Thumbnail = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, NAME_None, ToBGRA(Pixels, nullptr));
				Item.Brush.SetResourceObject(Item.Thumbnail);
			});
		});
	}
}

void SGlazeNoiseWindow::Select(int32 Index)
{
	if (!Items.IsValidIndex(Index))
	{
		return;
	}
	Selected = Index;
	const FString& Name = Items[Index]->Name;
	if (!bNameEdited)
	{
		Settings->AssetName = Name;
		Details->ForceRefresh();
	}
	if (!Glaze::Noise::LoadGray(Glaze::Noise::PackFile(Name, 1024), PreviewSize, SourcePixels, SourceSize))
	{
		SourcePixels.Reset();
		SourceSize = 0;
		Notify(FText::Format(LOCTEXT("LoadFailed", "Could not read the pack noise {0}."), FText::FromString(Name)), false);
	}
	UpdatePreview();
}

void SGlazeNoiseWindow::OnSettingsChanged(const FPropertyChangedEvent& Event)
{
	if (Event.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UGlazeNoiseSettings, AssetName))
	{
		bNameEdited = true;
	}
	UpdatePreview();
}

void SGlazeNoiseWindow::UpdatePreview()
{
	if (SourceSize <= 0)
	{
		PreviewBrush.SetResourceObject(nullptr);
		return;
	}
	TArray<uint8> Lut = Glaze::Noise::BuildLut(*Settings);
	if (!Settings->bSRGB)
	{
		// Linear data reaches the screen sRGB-encoded: show it as a material would.
		for (uint8& V : Lut)
		{
			V = FLinearColor(V / 255.f, 0.f, 0.f).ToFColor(/*bSRGB*/ true).R;
		}
	}
	// Tiling is drawn into the preview image: N x N copies, each scaled down by N.
	const int32 Tiling = FMath::Clamp(Settings->PreviewTiling, 1, 4);
	TArray<uint8> Tiled;
	const TArray<uint8>* Gray = &SourcePixels;
	if (Tiling > 1)
	{
		Tiled.SetNumUninitialized(SourcePixels.Num());
		for (int32 Y = 0; Y < SourceSize; ++Y)
		{
			const int32 SY = (Y * Tiling) % SourceSize;
			for (int32 X = 0; X < SourceSize; ++X)
			{
				Tiled[Y * SourceSize + X] = SourcePixels[SY * SourceSize + (X * Tiling) % SourceSize];
			}
		}
		Gray = &Tiled;
	}
	TArray<uint8> BGRA = ToBGRA(*Gray, &Lut);
	if (!PreviewTexture || PreviewTexture->GetSizeX() != SourceSize)
	{
		PreviewTexture = UTexture2D::CreateTransient(SourceSize, SourceSize, PF_B8G8R8A8, NAME_None, BGRA);
		PreviewBrush.SetResourceObject(PreviewTexture);
		return;
	}
	// Same texture, new pixels: no new resource on every step of a slider drag.
	uint8* Data = static_cast<uint8*>(FMemory::Malloc(BGRA.Num()));
	FMemory::Memcpy(Data, BGRA.GetData(), BGRA.Num());
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, SourceSize, SourceSize);
	PreviewTexture->UpdateTextureRegions(0, 1, Region, SourceSize * 4, 4, Data, [](uint8* InData, const FUpdateTextureRegion2D* InRegion)
	{
		FMemory::Free(InData);
		delete InRegion;
	});
}

FReply SGlazeNoiseWindow::OnCreate()
{
	LastCreated.Reset();
	if (!Items.IsValidIndex(Selected))
	{
		return FReply::Handled();
	}
	const int32 TargetSize = Settings->Size();
	TArray<uint8> Pixels;
	int32 Size = 0;
	if (!Glaze::Noise::LoadGray(Glaze::Noise::PackFile(Items[Selected]->Name, TargetSize), TargetSize, Pixels, Size))
	{
		Notify(LOCTEXT("ReadFailed", "Could not read the pack noise file."), false);
		return FReply::Handled();
	}
	const TArray<uint8> Lut = Glaze::Noise::BuildLut(*Settings);
	for (uint8& V : Pixels)
	{
		V = Lut[V];
	}

	FString Folder = Settings->Folder.Path;
	Folder.RemoveFromEnd(TEXT("/"));
	const FString Name = FPaths::MakeValidFileName(Settings->AssetName.TrimStartAndEnd(), TEXT('_'));
	if (Name.IsEmpty() || !FPackageName::IsValidLongPackageName(Folder / Name))
	{
		Notify(LOCTEXT("BadName", "Pick a name and a folder under /Game."), false);
		return FReply::Handled();
	}
	FString Path = Folder / Name;
	if (!bNameEdited)
	{
		// The suggested name: a second copy of the same noise gets a number instead of an error.
		FString Package, Asset;
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().CreateUniqueAssetName(Path, TEXT(""), Package, Asset);
		Path = Package;
	}
	FString Error;
	UTexture2D* Texture = Glaze::Noise::CreateTextureAsset(Pixels, Size, Path, Error, Settings->bSRGB);
	if (!Texture || !Error.IsEmpty())
	{
		Notify(FText::FromString(Error), false);
		if (!Texture)
		{
			return FReply::Handled();
		}
	}
	else
	{
		Notify(FText::Format(LOCTEXT("Created", "Created {0}"), FText::FromString(FPackageName::GetShortName(Path))), true);
	}
	LastCreated = Texture->GetPathName();
	if (GEditor)
	{
		TArray<UObject*> Objects = { Texture };
		GEditor->SyncBrowserToObjects(Objects);
	}
	Close();
	return FReply::Handled();
}

void SGlazeNoiseWindow::Close()
{
	if (TSharedPtr<SWindow> W = Window.Pin())
	{
		W->RequestDestroyWindow();
	}
}

FReply SGlazeNoiseWindow::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	// Before a focused text field eats Enter; taking the focus first commits its value.
	if (KeyEvent.GetKey() == EKeys::Enter && KeyEvent.IsControlDown())
	{
		FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
		return OnCreate();
	}
	return SCompoundWidget::OnPreviewKeyDown(Geometry, KeyEvent);
}

FReply SGlazeNoiseWindow::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
}

#undef LOCTEXT_NAMESPACE
