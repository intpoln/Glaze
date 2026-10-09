#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "GlazePack.h"
#include "IContentBrowserSingleton.h"
#include "Engine/Texture2D.h"
#include "GlazeApply.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Modules/ModuleManager.h"
#include "SGlazeNoiseWindow.h"
#include "SGlazeWindow.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "Glaze"

class FGlazeEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Style = MakeShared<FSlateStyleSet>("GlazeStyle");
		if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Glaze")))
		{
			Style->Set("Glaze.Icon", new FSlateImageBrush(Plugin->GetBaseDir() / TEXT("Resources/Icon128.png"), FVector2D(16, 16)));
		}
		FSlateStyleRegistry::RegisterSlateStyle(*Style);
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGlazeEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		SGlazeWindow::CloseAll(); // its widgets and GC hooks run code from this module
		SGlazeNoiseWindow::CloseAll();
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (Style)
		{
			FSlateStyleRegistry::UnRegisterSlateStyle(*Style);
			Style.Reset();
		}
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		const FSlateIcon Icon("GlazeStyle", "Glaze.Icon");
		RegisterPackMenu(Icon);

		// Textures: open the window with all selected textures.
		{
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UTexture2D::StaticClass());
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry("Glaze", FNewToolMenuSectionDelegate::CreateLambda([Icon](FToolMenuSection& InSection)
			{
				const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context)
				{
					return;
				}
				FToolUIAction Action;
				Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& MenuContext)
				{
					if (const UContentBrowserAssetContextMenuContext* C = MenuContext.FindContext<UContentBrowserAssetContextMenuContext>())
					{
						SGlazeWindow::OpenForTextures(C->LoadSelectedObjects<UTexture2D>());
					}
				});
				InSection.AddMenuEntry("GlazeCreateMaterial",
					LOCTEXT("CreateMaterial", "Glaze: Create Material"),
					LOCTEXT("CreateMaterialTip", "Build a material instance from the selected textures: slots are matched by name, then tweak color, roughness and more."),
					Icon, Action);
			}));
		}

		// Material instances made by Glaze: reopen them.
		{
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMaterialInstanceConstant::StaticClass());
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry("GlazeEdit", FNewToolMenuSectionDelegate::CreateLambda([Icon](FToolMenuSection& InSection)
			{
				const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context || Context->SelectedAssets.Num() != 1)
				{
					return;
				}
				FToolUIAction Action;
				Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& MenuContext)
				{
					if (const UContentBrowserAssetContextMenuContext* C = MenuContext.FindContext<UContentBrowserAssetContextMenuContext>())
					{
						const TArray<UMaterialInstanceConstant*> Instances = C->LoadSelectedObjects<UMaterialInstanceConstant>();
						if (Instances.Num() == 1)
						{
							SGlazeWindow::OpenForInstance(Instances[0]);
						}
					}
				});
				Action.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda([](const FToolMenuContext& MenuContext)
				{
					const UContentBrowserAssetContextMenuContext* C = MenuContext.FindContext<UContentBrowserAssetContextMenuContext>();
					// Only loaded instances are checked here: right click must not load assets.
					const UMaterialInstanceConstant* MI = C && C->SelectedAssets.Num() == 1
						? Cast<UMaterialInstanceConstant>(C->SelectedAssets[0].FastGetAsset(false)) : nullptr;
					return !MI || Glaze::IsGlazeInstance(MI);
				});
				InSection.AddMenuEntry("GlazeEditInstance",
					LOCTEXT("EditInstance", "Edit with Glaze"),
					LOCTEXT("EditInstanceTip", "Reopen this Glaze material instance in the Glaze window."),
					Icon, Action);
			}));
		}
	}

	/** "Add" menu of the Content Browser: materials and noises from the bundled pack. */
	static void RegisterPackMenu(const FSlateIcon& Icon)
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AddNewContextMenu");
		FToolMenuSection& Section = Menu->FindOrAddSection("Glaze", LOCTEXT("GlazeSection", "Glaze"));

		auto ResolutionMenu = [](TFunction<void(bool)> Action)
		{
			return FNewToolMenuDelegate::CreateLambda([Action](UToolMenu* Sub)
			{
				FToolMenuSection& S = Sub->AddSection("Resolution");
				for (const bool b2k : { true, false })
				{
					S.AddMenuEntry(NAME_None, b2k ? LOCTEXT("Res2k", "2K") : LOCTEXT("Res1k", "1K"), FText::GetEmpty(), FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([Action, b2k] { Action(b2k); })));
				}
			});
		};

		Section.AddSubMenu("GlazePackMaterials", LOCTEXT("PackMaterials", "Glaze: Material from Pack"),
			LOCTEXT("PackMaterialsTip", "Seamless textures bundled with Glaze: imports them into /Game/Glaze/Pack and opens the Glaze window."),
			FNewToolMenuDelegate::CreateLambda([ResolutionMenu](UToolMenu* Sub)
			{
				FToolMenuSection& S = Sub->AddSection("Materials");
				for (const FString& Name : Glaze::Pack::Materials())
				{
					S.AddSubMenu(FName(Name), FText::FromString(Name.Replace(TEXT("_"), TEXT(" "))), FText::GetEmpty(),
						ResolutionMenu([Name](bool b2k)
						{
							const TArray<UTexture2D*> Textures = Glaze::Pack::ImportMaterial(Name, b2k);
							const FContentBrowserItemPath Current = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetCurrentPath();
							SGlazeWindow::OpenForTextures(Textures, Current.HasInternalPath() ? Current.GetInternalPathString() : FString(TEXT("/Game")));
						}));
				}
			}), false, Icon);

		Section.AddMenuEntry("GlazePackNoises", LOCTEXT("PackNoises", "Glaze: Noise from Pack"),
			LOCTEXT("PackNoisesTip", "Seamless grayscale noises bundled with Glaze: pick one, adjust it with a live preview and create a texture."),
			Icon, FUIAction(FExecuteAction::CreateLambda([]
			{
				const FContentBrowserItemPath Current = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetCurrentPath();
				SGlazeNoiseWindow::Open(Current.HasInternalPath() ? Current.GetInternalPathString() : FString(TEXT("/Game")));
			})));
	}

	TSharedPtr<FSlateStyleSet> Style;
};

IMPLEMENT_MODULE(FGlazeEditorModule, GlazeEditor)

#undef LOCTEXT_NAMESPACE
