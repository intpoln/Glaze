#include "Engine/Texture2D.h"
#include "GlazeApply.h"
#include "GlazeMaster.h"
#include "GlazeMatcher.h"
#include "GlazeNoise.h"
#include "GlazePack.h"
#include "GlazeRecipe.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Materials/Material.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialShared.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags GlazeTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	bool HasSlot(const Glaze::FNameMatch& M, EGlazeSlot Slot, EGlazeChannel Channel = EGlazeChannel::R)
	{
		return M.Slots.ContainsByPredicate([&](const Glaze::FSlotMatch& S) { return S.Slot == Slot && S.Channel == Channel; });
	}

	/** Synchronously compiles the material's own shaders; returns the errors, or null when nothing can compile here. */
	const TArray<FString>* CompileErrors(UMaterialInterface* Material, bool* bOutHasShaderMap = nullptr)
	{
		const UMaterialInstance* MI = Cast<UMaterialInstance>(Material);
		FMaterialResource* Resource = !MI || MI->bHasStaticPermutationResource ? Material->GetMaterialResource(GMaxRHIShaderPlatform) : nullptr;
		if (!Resource)
		{
			return nullptr;
		}
		Resource->CacheShaders(EMaterialShaderPrecompileMode::Synchronous);
		Resource->FinishCompilation();
		if (bOutHasShaderMap)
		{
			*bOutHasShaderMap = Resource->GetGameThreadShaderMap() != nullptr;
		}
		return &Resource->GetCompileErrors();
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

	// Review regressions ---------------------------------------------------------------------------------------

	// A lone exact channel name is that channel, not a color map.
	M = MatchTextureName(TEXT("Normal"));
	TestTrue(TEXT("exact Normal"), HasSlot(M, EGlazeSlot::Normal) && M.Slots.Num() == 1);
	TestTrue(TEXT("exact Normal has no set name"), M.SetName.IsEmpty());
	M = MatchTextureName(TEXT("T_Roughness_2K"));
	TestTrue(TEXT("exact Roughness"), HasSlot(M, EGlazeSlot::Roughness));
	M = MatchTextureName(TEXT("AmbientOcclusion"));
	TestTrue(TEXT("exact AmbientOcclusion is not glued"), HasSlot(M, EGlazeSlot::AmbientOcclusion) && M.SetName.IsEmpty());
	{
		UTexture2D* NormalTex = NewTexture(TEXT("Normal"));
		UGlazeRecipe* R = NewObject<UGlazeRecipe>();
		const FTextureSetMatch Match = MatchTextures({ NormalTex }, *R);
		TestTrue(TEXT("single Normal -> Normal slot"), R->Normal == NormalTex && R->BaseColor == nullptr);
		TestEqual(TEXT("single Normal matched"), Match.Unmatched.Num(), 0);
	}

	// Glued suffix with underscores inside the set name.
	M = MatchTextureName(TEXT("Wood_Floor01BaseColor"));
	TestTrue(TEXT("glued multi-token color"), HasSlot(M, EGlazeSlot::BaseColor));
	TestEqual(TEXT("glued multi-token set"), M.SetName, FString(TEXT("Wood_Floor01")));
	M = MatchTextureName(TEXT("Wood_Floor01Normal"));
	TestTrue(TEXT("glued multi-token normal"), HasSlot(M, EGlazeSlot::Normal));
	TestEqual(TEXT("glued multi-token normal set"), M.SetName, FString(TEXT("Wood_Floor01")));
	M = MatchTextureName(TEXT("T_Wood_Floor01NormalGL_2K"));
	TestTrue(TEXT("glued longest suffix is normalgl"), HasSlot(M, EGlazeSlot::Normal) && M.Slots[0].bFlipGreen);
	TestEqual(TEXT("glued normalgl set"), M.SetName, FString(TEXT("Wood_Floor01")));

	// Resolution is reported, not part of the set name.
	M = MatchTextureName(TEXT("T_Wood_BC_4K"));
	TestEqual(TEXT("resolution token"), M.Resolution, FString(TEXT("4k")));
	TestEqual(TEXT("resolution not in set name"), M.SetName, FString(TEXT("Wood")));

	// Already flipped texture is not flipped twice.
	{
		UTexture2D* Gl = NewTexture(TEXT("aerial_rocks_02_nor_gl_4k"));
		UGlazeRecipe* R = NewObject<UGlazeRecipe>();
		TestTrue(TEXT("gl name recommends flip"), MatchTextures({ Gl }, *R).bFlipGreen);
		UTexture2D* GlFlipped = NewTexture(TEXT("aerial_rocks_03_nor_gl_4k"));
		GlFlipped->bFlipGreenChannel = true;
		UGlazeRecipe* R2 = NewObject<UGlazeRecipe>();
		const FTextureSetMatch Match2 = MatchTextures({ GlFlipped }, *R2);
		TestTrue(TEXT("flipped texture still normal"), R2->Normal == GlFlipped);
		TestFalse(TEXT("no second flip"), Match2.bFlipGreen);
	}

	// Same name in different folders = different sets.
	{
		auto NewTextureIn = [](const TCHAR* Folder, const TCHAR* Name)
		{
			UPackage* Package = CreatePackage(*(FString(Folder) / Name));
			return NewObject<UTexture2D>(Package, Name, RF_Transient);
		};
		const FString Root = TEXT("/Temp/GlazeTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		UTexture2D* InColor = NewTextureIn(*(Root / TEXT("Interior")), TEXT("T_Wood_BC"));
		UTexture2D* ExNormal = NewTextureIn(*(Root / TEXT("Exterior")), TEXT("T_Wood_N"));
		UTexture2D* InNormal = NewTextureIn(*(Root / TEXT("Interior")), TEXT("T_Wood_N"));
		const auto Folders = GroupTextureSets({ InColor, ExNormal, InNormal });
		TestEqual(TEXT("folders split sets"), Folders.Num(), 2);
		if (Folders.Num() == 2)
		{
			TestEqual(TEXT("interior set size"), Folders[0].Value.Num(), 2);
			TestTrue(TEXT("interior set content"), Folders[0].Value.Contains(InColor) && Folders[0].Value.Contains(InNormal));
			TestTrue(TEXT("exterior set content"), Folders[1].Value.Num() == 1 && Folders[1].Value[0] == ExNormal);
			TestNotEqual(TEXT("equal names are told apart"), Folders[0].Key, Folders[1].Key);
		}
	}

	// Different resolutions of one name = different sets; a map without resolution joins the first.
	{
		UTexture2D* Bc1 = NewTexture(TEXT("T_ResWood_BC_1K"));
		UTexture2D* N4 = NewTexture(TEXT("T_ResWood_N_4K"));
		UTexture2D* Orm4 = NewTexture(TEXT("T_ResWood_ORM_4K"));
		UTexture2D* Bc4 = NewTexture(TEXT("T_ResWood_BC_4K"));
		const auto Res = GroupTextureSets({ Bc1, N4, Orm4, Bc4 });
		TestEqual(TEXT("two resolutions -> two sets"), Res.Num(), 2);
		if (Res.Num() == 2)
		{
			TestTrue(TEXT("1K set"), Res[0].Value.Num() == 1 && Res[0].Value[0] == Bc1);
			TestEqual(TEXT("4K set size"), Res[1].Value.Num(), 3);
			UGlazeRecipe* R = NewObject<UGlazeRecipe>();
			MatchTextures(Res[1].Value, *R);
			TestTrue(TEXT("4K color is the 4K map"), R->BaseColor == Bc4);
		}
		UTexture2D* Plain = NewTexture(TEXT("T_MixWood_BC"));
		UTexture2D* Mix4 = NewTexture(TEXT("T_MixWood_N_4K"));
		TestEqual(TEXT("missing resolution joins"), GroupTextureSets({ Plain, Mix4 }).Num(), 1);
	}

	// "2K" and "2048" are one resolution; exact two-token channel names need no set name.
	{
		TestEqual(TEXT("2048 key"), MatchTextureName(TEXT("Oak_Normal_2048")).Resolution, FString(TEXT("2k")));
		UTexture2D* A = NewTexture(TEXT("Oak_BaseColor_2K"));
		UTexture2D* B = NewTexture(TEXT("Oak_Normal_2048"));
		UTexture2D* C = NewTexture(TEXT("Oak_ORM_2K"));
		const auto Same = GroupTextureSets({ A, B, C });
		TestEqual(TEXT("2K and 2048 are one set"), Same.Num(), 1);
		M = MatchTextureName(TEXT("Base_Color"));
		TestTrue(TEXT("exact Base_Color"), HasSlot(M, EGlazeSlot::BaseColor) && M.SetName.IsEmpty());
		M = MatchTextureName(TEXT("Normal_GL"));
		TestTrue(TEXT("exact Normal_GL flips green"), HasSlot(M, EGlazeSlot::Normal) && M.Slots[0].bFlipGreen && M.SetName.IsEmpty());
		TestEqual(TEXT("Turkish-safe fold"), GroupTextureSets({ NewTexture(TEXT("T_Iron_BC")), NewTexture(TEXT("T_IRON_N")) }).Num(), 1);
	}

	// Cyrillic names group case-insensitively.
	{
		UTexture2D* A = NewTexture(TEXT("T_Камень_BC"));
		UTexture2D* B = NewTexture(TEXT("T_Камень_ORM"));
		UTexture2D* C = NewTexture(TEXT("T_камень_N"));
		const auto Cyr = GroupTextureSets({ A, B, C });
		TestEqual(TEXT("cyrillic case folding"), Cyr.Num(), 1);
		if (Cyr.Num() == 1)
		{
			TestEqual(TEXT("cyrillic set size"), Cyr[0].Value.Num(), 3);
		}
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

	// An out-of-range enum is rejected and leaves the recipe untouched.
	TSharedPtr<FJsonObject> Object;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
	for (auto& Field : Object->Values)
	{
		if (FString(*Field.Key).Equals(TEXT("RoughnessChannel"), ESearchCase::IgnoreCase))
		{
			Field.Value = MakeShared<FJsonValueNumber>(255);
		}
		if (FString(*Field.Key).Equals(TEXT("LerpAmount"), ESearchCase::IgnoreCase))
		{
			Field.Value = MakeShared<FJsonValueNumber>(0.9);
		}
	}
	FString BadJson;
	FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&BadJson));
	TestFalse(TEXT("bad enum rejected"), B->FromJson(BadJson));
	TestEqual(TEXT("bad json leaves recipe"), B->LerpAmount, 0.4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazeMasterTest, "Glaze.Master", GlazeTestFlags)
bool FGlazeMasterTest::RunTest(const FString&)
{
	// A transient material: the test does not touch /Game. Engine functions must resolve (missing inputs log errors -> fail).
	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_GlazeTest"), RF_Transient);
	Glaze::BuildMasterGraph(Material);
	// A rebuild replaces the graph instead of accumulating old nodes.
	const int32 NodeCount = Material->GetExpressions().Num();
	Glaze::BuildMasterGraph(Material);
	TestEqual(TEXT("rebuild keeps the node count"), Material->GetExpressions().Num(), NodeCount);

	TArray<FMaterialParameterInfo> Infos;
	TArray<FGuid> Ids;
	Material->GetAllScalarParameterInfo(Infos, Ids);
	TSet<FName> Names;
	for (const FMaterialParameterInfo& I : Infos) { Names.Add(I.Name); }
	for (const FName N : TArray<FName>{ Glaze::Param::Tiling, Glaze::Param::Rotation, Glaze::Param::ColorContrast, Glaze::Param::LerpAmount,
		Glaze::Param::Value(EGlazeSlot::Roughness), Glaze::Param::Max(EGlazeSlot::Roughness), Glaze::Param::OpacityAmount,
		Glaze::Param::DetailSize, Glaze::Param::DetailRoughness, Glaze::Param::DetailBump, Glaze::Param::MacroSize, Glaze::Param::MacroAmount,
		Glaze::Param::TriplanarSize, Glaze::Param::HeightRatio, Glaze::Param::ParallaxSteps, Glaze::Param::WindDirection, Glaze::Param::WindStrength, Glaze::Param::WindSpeed,
		Glaze::Param::WindGustSize, Glaze::Param::WindHeight })
	{
		TestTrue(*FString::Printf(TEXT("scalar %s"), *N.ToString()), Names.Contains(N));
	}

	Material->GetAllStaticSwitchParameterInfo(Infos, Ids);
	Names.Reset();
	for (const FMaterialParameterInfo& I : Infos) { Names.Add(I.Name); }
	for (const FName N : TArray<FName>{ Glaze::Param::UseMap(EGlazeSlot::BaseColor), Glaze::Param::ColorCorrection, Glaze::Param::FromPacked(EGlazeSlot::Metallic),
		Glaze::Param::Dithered, Glaze::Param::UseEmissive, Glaze::Param::EmissiveFromBaseColor,
		Glaze::Param::UseDetail, Glaze::Param::DetailFromTexture, Glaze::Param::DetailUV, Glaze::Param::DetailLocal, Glaze::Param::DetailPreSkinned, Glaze::Param::UseMacro,
		Glaze::Param::Triplanar, Glaze::Param::TriplanarLocal, Glaze::Param::UseMap(EGlazeSlot::Height), Glaze::Param::FromPacked(EGlazeSlot::Height),
		Glaze::Param::UseWind, Glaze::Param::WindByHeight, Glaze::Param::WindByVertexColor })
	{
		TestTrue(*FString::Printf(TEXT("switch %s"), *N.ToString()), Names.Contains(N));
	}

	Material->GetAllTextureParameterInfo(Infos, Ids);
	TestEqual(TEXT("texture params: base, normal, packed, 5 grayscale, emissive, detail, height"), Infos.Num(), 11);
	// Masks samplers need linear defaults, or the shader does not compile ("Sampler type is Masks, should be Color").
	for (const FMaterialParameterInfo& I : Infos)
	{
		UTexture* Default = nullptr;
		Material->GetTextureParameterDefaultValue(I, Default);
		if (I.Name == Glaze::Param::DetailMap)
		{
			TestTrue(TEXT("detail default is linear grayscale"), Default && !Default->SRGB && Default->CompressionSettings == TC_Grayscale);
		}
		else if (I.Name != Glaze::Param::Map(EGlazeSlot::BaseColor) && I.Name != Glaze::Param::Map(EGlazeSlot::Emissive) && I.Name != Glaze::Param::Map(EGlazeSlot::Normal))
		{
			TestTrue(*FString::Printf(TEXT("%s default is linear masks"), *I.Name.ToString()),
				Default && !Default->SRGB && Default->CompressionSettings == TC_Masks);
		}
	}
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
	// Emissive from the base color texture: no own emissive map.
	R->bEmissive = true;
	R->bEmissiveFromBaseColor = true;
	R->Emissive = Orm;
	ApplyRecipe(MI, *R, Key);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::EmissiveFromBaseColor), bValue, Guid);
	TestTrue(TEXT("emissive from base color"), bValue);
	TestNull(TEXT("no emissive map override"), TextureParam(Param::Map(EGlazeSlot::Emissive)));
	R->bEmissive = false;
	R->bEmissiveFromBaseColor = false;
	R->Emissive = nullptr;
	ApplyRecipe(MI, *R, Key);
	// Detail noise in pre-skinned space, procedural (no texture) then from a texture.
	R->bDetailNoise = true;
	R->DetailSpace = EGlazeDetailSpace::PreSkinned;
	ApplyRecipe(MI, *R, Key);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::DetailPreSkinned), bValue, Guid);
	TestTrue(TEXT("detail pre-skinned"), bValue);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::DetailFromTexture), bValue, Guid);
	TestFalse(TEXT("procedural detail without texture"), bValue);
	TestNull(TEXT("no detail map override"), TextureParam(Param::DetailMap));
	R->DetailNoise = Orm;
	ApplyRecipe(MI, *R, Key);
	TestTrue(TEXT("detail map"), TextureParam(Param::DetailMap) == Orm);
	R->bDetailNoise = false;
	R->DetailNoise = nullptr;
	ApplyRecipe(MI, *R, Key);
	TestNull(TEXT("detail map removed when off"), TextureParam(Param::DetailMap));

	TestTrue(TEXT("masked override"), MI->BasePropertyOverrides.bOverride_BlendMode && MI->BasePropertyOverrides.BlendMode == BLEND_Masked);

	// Height in the B channel of the packed map (ORDp): parallax reads the packed map, no own height map.
	R->Height = Orm;
	R->HeightChannel = EGlazeChannel::B;
	ApplyRecipe(MI, *R, Key);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::UseMap(EGlazeSlot::Height)), bValue, Guid);
	TestTrue(TEXT("parallax on"), bValue);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::FromPacked(EGlazeSlot::Height)), bValue, Guid);
	TestTrue(TEXT("height from packed"), bValue);
	TestNull(TEXT("no own height map"), TextureParam(Param::Map(EGlazeSlot::Height)));
	// Triplanar has no UVs to shift: parallax off.
	R->Projection = EGlazeProjection::Local;
	ApplyRecipe(MI, *R, Key);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::Triplanar), bValue, Guid);
	TestTrue(TEXT("triplanar on"), bValue);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::TriplanarLocal), bValue, Guid);
	TestTrue(TEXT("triplanar in object space"), bValue);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::UseMap(EGlazeSlot::Height)), bValue, Guid);
	TestFalse(TEXT("no parallax with triplanar"), bValue);
	R->Projection = EGlazeProjection::UV;
	R->Height = nullptr;
	// Wind by vertex color.
	R->bWind = true;
	R->WindMask = EGlazeWindMask::VertexColor;
	ApplyRecipe(MI, *R, Key);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::WindByVertexColor), bValue, Guid);
	TestTrue(TEXT("wind by vertex color"), bValue);
	MI->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(Param::WindByHeight), bValue, Guid);
	TestFalse(TEXT("not by height"), bValue);
	R->bWind = false;
	ApplyRecipe(MI, *R, Key);

	// Same static state: the instance is not touched again.
	const FString Before = Key;
	R->LerpAmount = 0.5f;
	const double Start = FPlatformTime::Seconds();
	ApplyRecipe(MI, *R, Key);
	const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
	AddInfo(FString::Printf(TEXT("ApplyRecipe (values only): %.2f ms"), Ms));
	if (Ms >= 20.0)
	{
		AddWarning(FString::Printf(TEXT("value-only ApplyRecipe is slow: %.2f ms (expected < 20 ms)"), Ms));
	}
	float LerpValue = 0.f;
	TestTrue(TEXT("lerp amount readable"), MI->GetScalarParameterValue(FHashedMaterialParameterInfo(Param::LerpAmount), LerpValue));
	TestEqual(TEXT("value-only change reaches the instance"), LerpValue, 0.5f);
	TestEqual(TEXT("static key unchanged"), Key, Before);

	// Texture fixes plan: ORM -> masks, linear.
	const TArray<FTextureFix> Fixes = PlanTextureFixes(*R);
	TestTrue(TEXT("orm fix planned"), Fixes.ContainsByPredicate([Orm](const FTextureFix& F) { return F.Texture == Orm; }));
	return true;
}

/** Compiles real shader permutations (needs a GPU RHI: Tools/runtests.sh Glaze.Compile rhi). */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazeCompileTest, "Glaze.Compile", GlazeTestFlags)
bool FGlazeCompileTest::RunTest(const FString&)
{
	using namespace Glaze;
	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_GlazeCompileTest"), RF_Transient);
	Material->SetUsageByFlag(MATUSAGE_SkeletalMesh, true);
	BuildMasterGraph(Material);
	Material->PostEditChange();

	UTexture2D* Noise = Pack::Noises().Num() > 0 ? Pack::ImportNoise(Pack::Noises()[0], 1024) : nullptr;
	TestNotNull(TEXT("pack noise"), Noise);
	const TArray<UTexture2D*> Wood = Pack::ImportMaterial(TEXT("Wood_Planks"), 1024);

	struct FCase { const TCHAR* Name; TFunction<void(UGlazeRecipe&)> Setup; };
	const TArray<FCase> Cases = {
		{ TEXT("defaults"), [](UGlazeRecipe&) {} },
		{ TEXT("wood + cc + masked dither + emissive from base"), [](UGlazeRecipe& R)
			{ R.bColorCorrection = true; R.BlendMode = EGlazeBlend::Masked; R.bDithered = true; R.bEmissive = true; R.bEmissiveFromBaseColor = true; } },
		{ TEXT("detail uv texture"), [Noise](UGlazeRecipe& R) { R.bDetailNoise = true; R.DetailNoise = Noise; R.DetailSpace = EGlazeDetailSpace::UV; } },
		{ TEXT("detail world texture"), [Noise](UGlazeRecipe& R) { R.bDetailNoise = true; R.DetailNoise = Noise; R.DetailSpace = EGlazeDetailSpace::World; } },
		{ TEXT("detail local procedural"), [](UGlazeRecipe& R) { R.bDetailNoise = true; R.DetailSpace = EGlazeDetailSpace::Local; } },
		{ TEXT("detail pre-skinned texture"), [Noise](UGlazeRecipe& R) { R.bDetailNoise = true; R.DetailNoise = Noise; R.DetailSpace = EGlazeDetailSpace::PreSkinned; } },
		{ TEXT("detail pre-skinned procedural"), [](UGlazeRecipe& R) { R.bDetailNoise = true; R.DetailSpace = EGlazeDetailSpace::PreSkinned; } },
		{ TEXT("detail world + bump + macro"), [Noise](UGlazeRecipe& R)
			{ R.bDetailNoise = true; R.DetailNoise = Noise; R.DetailBump = 1.f; R.bMacroVariation = true; } },
		{ TEXT("macro only, uv"), [](UGlazeRecipe& R) { R.bMacroVariation = true; R.DetailSpace = EGlazeDetailSpace::UV; } },
		{ TEXT("macro only, pre-skinned"), [](UGlazeRecipe& R) { R.bMacroVariation = true; R.DetailSpace = EGlazeDetailSpace::PreSkinned; } },
		{ TEXT("triplanar world + masked + emissive map"), [](UGlazeRecipe& R)
			{ R.Projection = EGlazeProjection::World; R.BlendMode = EGlazeBlend::Masked; R.bEmissive = true; R.Emissive = R.BaseColor; } },
		{ TEXT("triplanar object + detail bump"), [Noise](UGlazeRecipe& R)
			{ R.Projection = EGlazeProjection::Local; R.bDetailNoise = true; R.DetailNoise = Noise; R.DetailBump = 1.f; } },
		{ TEXT("parallax from packed"), [](UGlazeRecipe& R) { R.Height = R.Roughness; R.HeightChannel = EGlazeChannel::B; R.Rotation = 30.f; } },
		{ TEXT("parallax own map"), [](UGlazeRecipe& R)
			{ R.Height = R.Roughness; R.Roughness = R.Metallic = R.AmbientOcclusion = nullptr; } },
		{ TEXT("wind by height"), [](UGlazeRecipe& R) { R.bWind = true; R.WindMask = EGlazeWindMask::Height; } },
		{ TEXT("wind by vertex color + pre-skinned detail"), [](UGlazeRecipe& R)
			{ R.bWind = true; R.WindMask = EGlazeWindMask::VertexColor; R.bDetailNoise = true; R.DetailSpace = EGlazeDetailSpace::PreSkinned; } },
		{ TEXT("wind whole mesh"), [](UGlazeRecipe& R) { R.bWind = true; R.WindMask = EGlazeWindMask::None; } },
	};
	for (const FCase& Case : Cases)
	{
		UGlazeRecipe* R = NewObject<UGlazeRecipe>();
		MatchTextures(Wood, *R);
		Case.Setup(*R);
		UMaterialInstanceConstant* MI = NewObject<UMaterialInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transient);
		MI->SetParentEditorOnly(Material);
		FString Key;
		ApplyRecipe(MI, *R, Key);
		bool bHasShaderMap = false;
		const TArray<FString>* Errors = CompileErrors(MI, &bHasShaderMap);
		if (!Errors)
		{
			AddWarning(FString::Printf(TEXT("%s: no own permutation resource (null RHI?)"), Case.Name));
			continue;
		}
		for (const FString& Error : *Errors)
		{
			AddError(FString::Printf(TEXT("%s: %s"), Case.Name, *Error));
		}
		TestTrue(*FString::Printf(TEXT("%s: compiled"), Case.Name), bHasShaderMap);
	}

	// The check itself works: a Masks sampler with an sRGB default (the old v1 bug) must fail.
	UMaterial* Bad = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_GlazeBadTest"), RF_Transient);
	auto* Sample = Cast<UMaterialExpressionTextureSampleParameter2D>(
		UMaterialEditingLibrary::CreateMaterialExpression(Bad, UMaterialExpressionTextureSampleParameter2D::StaticClass()));
	Sample->ParameterName = TEXT("Bad");
	Sample->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	Sample->SamplerType = SAMPLERTYPE_Masks;
	Bad->GetEditorOnlyData()->BaseColor.Connect(0, Sample);
	Bad->PostEditChange();
	const TArray<FString>* BadErrors = CompileErrors(Bad);
	TestTrue(TEXT("wrong sampler type is reported"), BadErrors && BadErrors->Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazePackTest, "Glaze.Pack", GlazeTestFlags)
bool FGlazePackTest::RunTest(const FString&)
{
	using namespace Glaze;
	TestTrue(TEXT("pack materials"), Pack::Materials().Num() >= 10);
	TestTrue(TEXT("pack noises"), Pack::Noises().Num() >= 10);
	if (Pack::Noises().Num() == 0)
	{
		return false;
	}
	TestTrue(TEXT("wood planks listed"), Pack::Materials().Contains(TEXT("Wood_Planks")));

	// Imports into the lab project's /Game/Glaze/Pack (reused on later runs).
	const TArray<UTexture2D*> Textures = Pack::ImportMaterial(TEXT("Wood_Planks"), 1024);
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

	UTexture2D* Noise = Pack::ImportNoise(Pack::Noises()[0], 1024);
	TestTrue(TEXT("noise grayscale"), Noise && Noise->CompressionSettings == TC_Grayscale && !Noise->SRGB);

	// A complete import reports no failures and every map is written to disk.
	TArray<FString> Failed = { TEXT("stale") };
	const TArray<UTexture2D*> Again = Pack::ImportMaterial(TEXT("Wood_Planks"), 1024, &Failed);
	TestEqual(TEXT("reimport count"), Again.Num(), 3);
	TestEqual(TEXT("no failed files"), Failed.Num(), 0);
	for (UTexture2D* T : Again)
	{
		TestTrue(TEXT("saved to disk"), FPackageName::DoesPackageExist(T->GetOutermost()->GetName()));
	}

	// Failures are reported to the caller (and logged as errors).
	AddExpectedError(TEXT("No textures found for pack material"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("is incomplete"), EAutomationExpectedErrorFlags::Contains, 1);
	Failed.Reset();
	const TArray<UTexture2D*> None = Pack::ImportMaterial(TEXT("Glaze_No_Such_Material"), 1024, &Failed);
	TestEqual(TEXT("missing material imports nothing"), None.Num(), 0);
	TestTrue(TEXT("missing material is reported"), Failed.Num() > 0);

	AddExpectedError(TEXT("Pack noise file not found"), EAutomationExpectedErrorFlags::Contains, 1);
	TestNull(TEXT("missing noise"), Pack::ImportNoise(TEXT("T_Noise_Glaze_No_Such"), 1024));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlazeNoiseTest, "Glaze.Noise", GlazeTestFlags)
bool FGlazeNoiseTest::RunTest(const FString&)
{
	using namespace Glaze;
	UGlazeNoiseSettings* S = NewObject<UGlazeNoiseSettings>();
	TestTrue(TEXT("defaults are identity"), S->IsIdentity());
	TArray<uint8> Lut = Noise::BuildLut(*S);
	TestTrue(TEXT("identity lut"), Lut[0] == 0 && Lut[128] == 128 && Lut[255] == 255);
	S->bInvert = true;
	Lut = Noise::BuildLut(*S);
	TestTrue(TEXT("invert"), Lut[0] == 255 && Lut[255] == 0);
	S->bInvert = false;
	S->BlackPoint = 0.25f;
	S->WhitePoint = 0.75f;
	Lut = Noise::BuildLut(*S);
	TestTrue(TEXT("levels clip"), Lut[50] == 0 && Lut[200] == 255 && FMath::Abs(Lut[128] - 128) <= 2);
	S->BlackPoint = 0.f;
	S->WhitePoint = 1.f;
	S->Contrast = 1.f;
	Lut = Noise::BuildLut(*S);
	TestTrue(TEXT("full contrast is a mask"), Lut[100] == 0 && Lut[156] == 255);
	S->Contrast = 0.f;
	S->Gamma = 2.f;
	Lut = Noise::BuildLut(*S);
	TestTrue(TEXT("gamma above 1 brightens"), Lut[64] > 100);

	// Pack noise decoding: resized grayscale.
	TArray<uint8> Pixels;
	int32 Size = 0;
	TestTrue(TEXT("decode thumbnail"), Noise::LoadGray(Noise::PackFile(Pack::Noises()[0], 1024), 96, Pixels, Size));
	TestEqual(TEXT("thumbnail size"), Size, 96);
	TestEqual(TEXT("one byte per pixel"), Pixels.Num(), 96 * 96);
	TestTrue(TEXT("full size"), Noise::LoadGray(Noise::PackFile(Pack::Noises()[0], 1024), 0, Pixels, Size) && Size == 1024);
	TestFalse(TEXT("missing file"), Noise::LoadGray(Noise::PackFile(TEXT("T_Noise_Glaze_No_Such"), 1024), 0, Pixels, Size));

	// Baked asset: linear grayscale, refuses to overwrite.
	TArray<uint8> Small;
	Small.Init(77, 64 * 64);
	const FString Path = FString::Printf(TEXT("/Temp/GlazeTest_%s/T_Noise_Test"), *FGuid::NewGuid().ToString());
	FString Error;
	UTexture2D* T = Noise::CreateTextureAsset(Small, 64, Path, Error);
	TestNotNull(*FString::Printf(TEXT("texture created (%s)"), *Error), T);
	if (T)
	{
		TestTrue(TEXT("grayscale linear"), T->CompressionSettings == TC_Grayscale && !T->SRGB);
		TestTrue(TEXT("G8 source"), T->Source.GetFormat() == TSF_G8 && T->Source.GetSizeX() == 64);
	}
	Error.Reset();
	TestNull(TEXT("no overwrite"), Noise::CreateTextureAsset(Small, 64, Path, Error));
	TestFalse(TEXT("overwrite reported"), Error.IsEmpty());
	return true;
}

#endif
