#include "Engine/Texture2D.h"
#include "GlazeApply.h"
#include "GlazeMaster.h"
#include "GlazeMatcher.h"
#include "GlazePack.h"
#include "GlazeRecipe.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags GlazeTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	bool HasSlot(const Glaze::FNameMatch& M, EGlazeSlot Slot, EGlazeChannel Channel = EGlazeChannel::R)
	{
		return M.Slots.ContainsByPredicate([&](const Glaze::FSlotMatch& S) { return S.Slot == Slot && S.Channel == Channel; });
	}

	UTexture2D* NewTexture(const TCHAR* Name)
	{
		return NewObject<UTexture2D>(GetTransientPackage(), Name, RF_Transient);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazeMatcherTest, "Glaze.Matcher", GlazeTestFlags)
bool FGlazeMatcherTest::RunTest(const FString&)
{
	using namespace Glaze;
	FNameMatch M = MatchTextureName(TEXT("T_Wood_01_BaseColor_2K"));
	TestEqual(TEXT("set name"), M.SetName, FString(TEXT("Wood_01")));
	TestTrue(TEXT("base color"), HasSlot(M, EGlazeSlot::BaseColor));

	M = MatchTextureName(TEXT("Rock_Base_Color"));
	TestEqual(TEXT("two-token suffix set"), M.SetName, FString(TEXT("Rock")));
	TestTrue(TEXT("two-token suffix"), HasSlot(M, EGlazeSlot::BaseColor));

	M = MatchTextureName(TEXT("T_Metal_ORM"));
	TestEqual(TEXT("orm slots"), M.Slots.Num(), 3);
	TestTrue(TEXT("orm ao"), HasSlot(M, EGlazeSlot::AmbientOcclusion, EGlazeChannel::R));
	TestTrue(TEXT("orm rough"), HasSlot(M, EGlazeSlot::Roughness, EGlazeChannel::G));
	TestTrue(TEXT("orm metal"), HasSlot(M, EGlazeSlot::Metallic, EGlazeChannel::B));

	M = MatchTextureName(TEXT("brick_wall_nor_gl_4k"));
	TestEqual(TEXT("polyhaven set"), M.SetName, FString(TEXT("brick_wall")));
	TestTrue(TEXT("gl normal"), HasSlot(M, EGlazeSlot::Normal) && M.Slots[0].bFlipGreen);

	M = MatchTextureName(TEXT("Fabric_Gloss"));
	TestTrue(TEXT("gloss -> inverted roughness"), HasSlot(M, EGlazeSlot::Roughness) && M.Slots[0].bInvert);

	M = MatchTextureName(TEXT("T_Stone_N"));
	TestTrue(TEXT("single letter suffix"), HasSlot(M, EGlazeSlot::Normal));

	M = MatchTextureName(TEXT("Leaves"));
	TestEqual(TEXT("single token: no slot"), M.Slots.Num(), 0);

	M = MatchTextureName(TEXT("Wood01BaseColor"));
	TestTrue(TEXT("glued suffix"), HasSlot(M, EGlazeSlot::BaseColor));
	TestEqual(TEXT("glued suffix set"), M.SetName, FString(TEXT("Wood01")));

	// Sets and recipe filling
	TArray<UTexture2D*> Textures = { NewTexture(TEXT("T_Wood_BC")), NewTexture(TEXT("T_Wood_N")), NewTexture(TEXT("T_Wood_ORM")),
		NewTexture(TEXT("T_Stone_BaseColor")), NewTexture(TEXT("T_Stone_Roughness")) };
	const auto Sets = GroupTextureSets(Textures);
	TestEqual(TEXT("two sets"), Sets.Num(), 2);
	if (Sets.Num() == 2)
	{
		TestEqual(TEXT("wood set size"), Sets[0].Value.Num(), 3);
		UGlazeRecipe* Recipe = NewObject<UGlazeRecipe>();
		const FTextureSetMatch Match = MatchTextures(Sets[0].Value, *Recipe);
		TestTrue(TEXT("bc"), Recipe->BaseColor == Textures[0]);
		TestTrue(TEXT("normal"), Recipe->Normal == Textures[1]);
		TestTrue(TEXT("rough from orm"), Recipe->Roughness == Textures[2] && Recipe->RoughnessChannel == EGlazeChannel::G);
		TestTrue(TEXT("ao from orm"), Recipe->AmbientOcclusion == Textures[2] && Recipe->AmbientOcclusionChannel == EGlazeChannel::R);
		TestEqual(TEXT("no unmatched"), Match.Unmatched.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazeRecipeJsonTest, "Glaze.RecipeJson", GlazeTestFlags)
bool FGlazeRecipeJsonTest::RunTest(const FString&)
{
	UGlazeRecipe* A = NewObject<UGlazeRecipe>();
	A->bColorCorrection = true;
	A->LerpAmount = 0.4f;
	A->LerpColor = FLinearColor(1, 0, 0);
	A->RoughnessRemap.Max = 0.7f;
	A->BlendMode = EGlazeBlend::Masked;
	A->BaseColor = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	const FString Json = A->ToJson();

	UGlazeRecipe* B = NewObject<UGlazeRecipe>();
	TestTrue(TEXT("parse"), B->FromJson(Json));
	TestTrue(TEXT("cc"), B->bColorCorrection);
	TestEqual(TEXT("lerp amount"), B->LerpAmount, 0.4f);
	TestEqual(TEXT("lerp color"), B->LerpColor, FLinearColor(1, 0, 0));
	TestEqual(TEXT("remap"), B->RoughnessRemap.Max, 0.7f);
	TestTrue(TEXT("blend"), B->BlendMode == EGlazeBlend::Masked);
	TestTrue(TEXT("texture"), B->BaseColor == A->BaseColor && B->BaseColor != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazeMasterTest, "Glaze.Master", GlazeTestFlags)
bool FGlazeMasterTest::RunTest(const FString&)
{
	// A transient material: the test does not touch /Game. Engine functions must resolve (missing inputs log errors -> fail).
	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_GlazeTest"), RF_Transient);
	Glaze::BuildMasterGraph(Material);

	TArray<FMaterialParameterInfo> Infos;
	TArray<FGuid> Ids;
	Material->GetAllScalarParameterInfo(Infos, Ids);
	TSet<FName> Names;
	for (const FMaterialParameterInfo& I : Infos) { Names.Add(I.Name); }
	for (const FName N : TArray<FName>{ Glaze::Param::Tiling, Glaze::Param::Rotation, Glaze::Param::ColorContrast, Glaze::Param::LerpAmount,
		Glaze::Param::Value(EGlazeSlot::Roughness), Glaze::Param::Max(EGlazeSlot::Roughness), Glaze::Param::OpacityAmount })
	{
		TestTrue(*FString::Printf(TEXT("scalar %s"), *N.ToString()), Names.Contains(N));
	}

	Material->GetAllStaticSwitchParameterInfo(Infos, Ids);
	Names.Reset();
	for (const FMaterialParameterInfo& I : Infos) { Names.Add(I.Name); }
	for (const FName N : TArray<FName>{ Glaze::Param::UseMap(EGlazeSlot::BaseColor), Glaze::Param::ColorCorrection, Glaze::Param::FromPacked(EGlazeSlot::Metallic),
		Glaze::Param::Dithered, Glaze::Param::UseEmissive })
	{
		TestTrue(*FString::Printf(TEXT("switch %s"), *N.ToString()), Names.Contains(N));
	}

	Material->GetAllTextureParameterInfo(Infos, Ids);
	TestEqual(TEXT("texture params: base, normal, packed, 5 grayscale, emissive"), Infos.Num(), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazeInstanceTest, "Glaze.Instance", GlazeTestFlags)
bool FGlazeInstanceTest::RunTest(const FString&)
{
	using namespace Glaze;
	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_GlazeTest2"), RF_Transient);
	BuildMasterGraph(Material);
	UMaterialInstanceConstant* MI = NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transient);
	MI->SetParentEditorOnly(Material);

	UTexture2D* Color = NewTexture(TEXT("T_Test_BC"));
	UTexture2D* Orm = NewTexture(TEXT("T_Test_ORM"));
	UGlazeRecipe* R = NewObject<UGlazeRecipe>();
	MatchTextures({ Color, Orm }, *R);
	R->bColorCorrection = true;
	R->LerpAmount = 0.25f;
	R->BlendMode = EGlazeBlend::Masked;

	FString Key;
	ApplyRecipe(MI, *R, Key);
	TestFalse(TEXT("static key"), Key.IsEmpty());

	auto TextureParam = [MI](FName Name) -> UTexture*
	{
		const FTextureParameterValue* V = MI->TextureParameterValues.FindByPredicate([Name](const FTextureParameterValue& P) { return P.ParameterInfo.Name == Name; });
		return V ? V->ParameterValue.Get() : nullptr;
	};
	TestTrue(TEXT("base color map"), TextureParam(Param::Map(EGlazeSlot::BaseColor)) == Color);
	TestTrue(TEXT("orm is the packed map"), TextureParam(Param::PackedMap) == Orm);
	TestNull(TEXT("no separate roughness map"), TextureParam(Param::Map(EGlazeSlot::Roughness)));
	TestNull(TEXT("no normal map"), TextureParam(Param::Map(EGlazeSlot::Normal)));

	bool bValue = false;
	FGuid Guid;
	TestTrue(TEXT("roughness switch exists"), MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::FromPacked(EGlazeSlot::Roughness)), bValue, Guid));
	TestTrue(TEXT("roughness from packed"), bValue);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::UseMap(EGlazeSlot::Normal)), bValue, Guid);
	TestFalse(TEXT("normal off"), bValue);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::ColorCorrection), bValue, Guid);
	TestTrue(TEXT("color correction on"), bValue);

	float Lerp = 0.f;
	MI->GetScalarParameterValue(FHashedMaterialParameterInfo(Param::LerpAmount), Lerp);
	TestEqual(TEXT("lerp amount"), Lerp, 0.25f);
	FLinearColor Channel;
	MI->GetVectorParameterValue(FHashedMaterialParameterInfo(Param::Channel(EGlazeSlot::Metallic)), Channel);
	TestEqual(TEXT("metallic reads B"), Channel, FLinearColor(0, 0, 1, 0));
	TestTrue(TEXT("masked override"), MI->BasePropertyOverrides.bOverride_BlendMode && MI->BasePropertyOverrides.BlendMode == BLEND_Masked);

	// Same static state: the instance is not touched again.
	const FString Before = Key;
	R->LerpAmount = 0.5f;
	ApplyRecipe(MI, *R, Key);
	TestEqual(TEXT("static key unchanged"), Key, Before);

	// Texture fixes plan: ORM -> masks, linear.
	const TArray<FTextureFix> Fixes = PlanTextureFixes(*R);
	TestTrue(TEXT("orm fix planned"), Fixes.ContainsByPredicate([Orm](const FTextureFix& F) { return F.Texture == Orm; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazePackTest, "Glaze.Pack", GlazeTestFlags)
bool FGlazePackTest::RunTest(const FString&)
{
	using namespace Glaze;
	TestTrue(TEXT("pack materials"), Pack::Materials().Num() >= 10);
	TestTrue(TEXT("pack noises"), Pack::Noises().Num() >= 10);
	TestTrue(TEXT("wood planks listed"), Pack::Materials().Contains(TEXT("Wood_Planks")));

	// Imports into the lab project's /Game/Glaze/Pack (reused on later runs).
	const TArray<UTexture2D*> Textures = Pack::ImportMaterial(TEXT("Wood_Planks"), false);
	TestEqual(TEXT("three maps"), Textures.Num(), 3);
	for (UTexture2D* T : Textures)
	{
		if (T->GetName().EndsWith(TEXT("_N")))
		{
			TestTrue(TEXT("normal settings"), T->CompressionSettings == TC_Normalmap && !T->SRGB);
		}
		else if (T->GetName().EndsWith(TEXT("_ORM")))
		{
			TestTrue(TEXT("orm settings"), T->CompressionSettings == TC_Masks && !T->SRGB);
		}
		else
		{
			TestTrue(TEXT("base color srgb"), T->SRGB);
		}
		TestTrue(TEXT("in project content"), T->GetPathName().StartsWith(TEXT("/Game/Glaze/Pack/Wood_Planks/1k/")));
	}
	UGlazeRecipe* R = NewObject<UGlazeRecipe>();
	MatchTextures(Textures, *R);
	TestNotNull(TEXT("bc matched"), R->BaseColor.Get());
	TestNotNull(TEXT("normal matched"), R->Normal.Get());
	TestTrue(TEXT("orm -> roughness G"), R->Roughness != nullptr && R->RoughnessChannel == EGlazeChannel::G);

	UTexture2D* Noise = Pack::ImportNoise(Pack::Noises()[0], false);
	TestTrue(TEXT("noise grayscale"), Noise && Noise->CompressionSettings == TC_Grayscale && !Noise->SRGB);
	return true;
}

#endif
