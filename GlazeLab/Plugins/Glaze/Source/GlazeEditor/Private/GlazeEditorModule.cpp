#include "ContentBrowserMenuContexts.h"
#include "Engine/Texture2D.h"
#include "GlazeApply.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Modules/ModuleManager.h"
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

	TSharedPtr<FSlateStyleSet> Style;
};

IMPLEMENT_MODULE(FGlazeEditorModule, GlazeEditor)

#undef LOCTEXT_NAMESPACE
