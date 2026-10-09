#include "GlazeEditorLibrary.h"

#include "Engine/Texture2D.h"
#include "GlazeApply.h"
#include "GlazeNoise.h"
#include "GlazePack.h"
#include "GlazeRecipe.h"
#include "Materials/MaterialInstanceConstant.h"
#include "SGlazeNoiseWindow.h"
#include "SGlazeWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Sets a property from text and notifies the object like a Details panel edit. */
	bool SetProperty(UObject* Object, const FString& Name, const FString& Value)
	{
		FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(FName(*Name)) : nullptr;
		if (!Property)
		{
			return false;
		}
		Object->PreEditChange(Property);
		if (!Property->ImportText_InContainer(*Value, Object, Object, PPF_None))
		{
			return false;
		}
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Object->PostEditChangeProperty(Event);
		return true;
	}

	FString GetProperty(const UObject* Object, const FString& Name)
	{
		const FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(FName(*Name)) : nullptr;
		FString Out;
		if (Property)
		{
			Property->ExportText_InContainer(0, Out, Object, Object, nullptr, PPF_None);
		}
		return Out;
	}
}

void UGlazeEditorLibrary::OpenWindowForTextures(const TArray<UTexture2D*>& Textures, const FString& OutputFolder)
{
	SGlazeWindow::OpenForTextures(Textures, OutputFolder);
}

bool UGlazeEditorLibrary::OpenWindowForInstance(UMaterialInstanceConstant* Instance)
{
	SGlazeWindow::OpenForInstance(Instance);
	TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get();
	return W && W->EditTarget == Instance;
}

bool UGlazeEditorLibrary::IsWindowOpen()
{
	return SGlazeWindow::Get().IsValid();
}

bool UGlazeEditorLibrary::SetWindowProperty(const FString& Name, const FString& Value)
{
	TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get();
	return W && SetProperty(W->Recipe.Get(), Name, Value);
}

FString UGlazeEditorLibrary::GetWindowProperty(const FString& Name)
{
	TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get();
	return W ? GetProperty(W->Recipe.Get(), Name) : FString();
}

FString UGlazeEditorLibrary::GetWindowInfo()
{
	TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get();
	return W ? W->HeaderText->GetText().ToString() + TEXT("\n") + W->InfoText->GetText().ToString() : FString();
}

UMaterialInstanceConstant* UGlazeEditorLibrary::GetWindowPreview()
{
	TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get();
	return W ? W->PreviewInstance.Get() : nullptr;
}

bool UGlazeEditorLibrary::WindowCreate()
{
	TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get();
	if (!W)
	{
		return false;
	}
	const bool bOk = W->CreateCurrent();
	if (bOk)
	{
		W->NextOrClose();
	}
	return bOk;
}

void UGlazeEditorLibrary::WindowCreateAll()
{
	if (TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get())
	{
		W->OnCreateAll();
	}
}

void UGlazeEditorLibrary::WindowSkip()
{
	if (TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get())
	{
		W->OnSkip();
	}
}

void UGlazeEditorLibrary::WindowCancel()
{
	if (TSharedPtr<SGlazeWindow> W = SGlazeWindow::Get())
	{
		W->OnCancel();
	}
}

void UGlazeEditorLibrary::OpenNoiseWindow(const FString& Folder)
{
	SGlazeNoiseWindow::Open(Folder);
}

bool UGlazeEditorLibrary::SelectNoise(const FString& Name)
{
	TSharedPtr<SGlazeNoiseWindow> W = SGlazeNoiseWindow::Get();
	if (!W)
	{
		return false;
	}
	for (int32 I = 0; I < W->Items.Num(); ++I)
	{
		if (W->Items[I]->Name == Name)
		{
			W->Select(I);
			return true;
		}
	}
	return false;
}

bool UGlazeEditorLibrary::SetNoiseProperty(const FString& Name, const FString& Value)
{
	TSharedPtr<SGlazeNoiseWindow> W = SGlazeNoiseWindow::Get();
	return W && SetProperty(W->Settings.Get(), Name, Value);
}

FString UGlazeEditorLibrary::NoiseCreate()
{
	TSharedPtr<SGlazeNoiseWindow> W = SGlazeNoiseWindow::Get();
	if (!W)
	{
		return FString();
	}
	W->OnCreate();
	return W->LastCreated;
}

void UGlazeEditorLibrary::CloseNoiseWindow()
{
	SGlazeNoiseWindow::CloseAll();
}

TArray<UTexture2D*> UGlazeEditorLibrary::ImportPackMaterial(const FString& Name, int32 Size)
{
	return Glaze::Pack::ImportMaterial(Name, Size);
}
