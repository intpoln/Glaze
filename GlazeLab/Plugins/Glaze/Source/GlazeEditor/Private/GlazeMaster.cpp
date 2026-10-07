#include "GlazeMaster.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionLocalPosition.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPreSkinnedNormal.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunctionInterface.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGlazeMaster, Log, All);

namespace Glaze
{
const TCHAR* MasterPackagePath = TEXT("/Game/Glaze/M_Glaze");
const TCHAR* MasterObjectPath = TEXT("/Game/Glaze/M_Glaze.M_Glaze");

namespace Param
{
	static FName Slotted(const TCHAR* Prefix, EGlazeSlot Slot, const TCHAR* Suffix)
	{
		return FName(FString(Prefix) + SlotName(Slot) + Suffix);
	}
	FName UseMap(EGlazeSlot Slot)     { return Slotted(TEXT("Use "), Slot, TEXT(" Map")); }
	FName FromPacked(EGlazeSlot Slot) { return Slotted(TEXT(""), Slot, TEXT(" From Packed")); }
	FName Map(EGlazeSlot Slot)        { return Slotted(TEXT(""), Slot, TEXT(" Map")); }
	FName Channel(EGlazeSlot Slot)    { return Slotted(TEXT(""), Slot, TEXT(" Channel")); }
	FName Value(EGlazeSlot Slot)      { return Slotted(TEXT(""), Slot, TEXT("")); }
	FName Invert(EGlazeSlot Slot)     { return Slotted(TEXT(""), Slot, TEXT(" Invert")); }
	FName Contrast(EGlazeSlot Slot)   { return Slotted(TEXT(""), Slot, TEXT(" Contrast")); }
	FName Bias(EGlazeSlot Slot)       { return Slotted(TEXT(""), Slot, TEXT(" Bias")); }
	FName Min(EGlazeSlot Slot)        { return Slotted(TEXT(""), Slot, TEXT(" Min")); }
	FName Max(EGlazeSlot Slot)        { return Slotted(TEXT(""), Slot, TEXT(" Max")); }

	const FName PackedMap(TEXT("Packed Map"));
	const FName Tiling(TEXT("Tiling")), TilingXY(TEXT("Tiling XY")), Offset(TEXT("UV Offset")), Rotation(TEXT("UV Rotation"));
	const FName Tint(TEXT("Tint")), ColorCorrection(TEXT("Color Correction")), Desaturation(TEXT("Desaturation"));
	const FName ColorContrast(TEXT("Contrast")), Brightness(TEXT("Brightness")), HueShift(TEXT("Hue Shift"));
	const FName OverlayColor(TEXT("Overlay Color")), OverlayAmount(TEXT("Overlay Amount"));
	const FName LerpColor(TEXT("Lerp Color")), LerpAmount(TEXT("Lerp Amount"));
	const FName NormalStrength(TEXT("Normal Strength")), FlipGreen(TEXT("Flip Green"));
	const FName UseOpacity(TEXT("Use Opacity")), OpacityAmount(TEXT("Opacity Amount")), Dithered(TEXT("Dithered Opacity"));
	const FName UseEmissive(TEXT("Use Emissive")), EmissiveFromBaseColor(TEXT("Emissive From Base Color")), EmissiveColor(TEXT("Emissive Color")), EmissiveIntensity(TEXT("Emissive Intensity"));
	const FName UseDetail(TEXT("Use Detail Noise")), DetailFromTexture(TEXT("Detail Noise From Texture")), DetailUV(TEXT("Detail In UV Space"));
	const FName DetailLocal(TEXT("Detail In Local Space")), DetailPreSkinned(TEXT("Detail In Pre-Skinned Space")), DetailMap(TEXT("Detail Noise Map"));
	const FName DetailTiling(TEXT("Detail Tiling")), DetailSize(TEXT("Detail Size")), DetailContrast(TEXT("Detail Contrast")), DetailBias(TEXT("Detail Bias"));
	const FName DetailColor(TEXT("Detail Color")), DetailColorAmount(TEXT("Detail Color Amount")), DetailRoughness(TEXT("Detail Roughness"));
}

namespace
{
	const FName MetaVersionKey(TEXT("GlazeMasterVersion"));

	/** White 4x4 linear texture in the project, next to the master: default of the Masks / grayscale map parameters. */
	UTexture* DefaultLinearTexture(const TCHAR* AssetName, TextureCompressionSettings Compression)
	{
		const FString PackageName = FString(TEXT("/Game/Glaze/")) + AssetName;
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *(PackageName + TEXT(".") + AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackageName);
		UTexture2D* Texture = NewObject<UTexture2D>(Package, AssetName, RF_Public | RF_Standalone);
		TArray<uint8> Pixels;
		Pixels.Init(255, 4 * 4 * 4);
		Texture->Source.Init(4, 4, 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->CompressionSettings = Compression;
		Texture->SRGB = false;
		Texture->PostEditChange();
		FAssetRegistryModule::AssetCreated(Texture);
		Package->MarkPackageDirty();
		UEditorLoadingAndSavingUtils::SavePackages({ Package }, /*bOnlyDirty*/ false);
		return Texture;
	}

	/** One output of an expression. */
	struct FPin
	{
		UMaterialExpression* E = nullptr;
		int32 Out = 0;
	};

	// Texture sample / vector parameter output indices: RGB, R, G, B, A, RGBA.
	constexpr int32 OutRGB = 0, OutA = 4, OutRGBA = 5;

	void Link(FExpressionInput& Input, const FPin& Pin)
	{
		if (Pin.E)
		{
			Input.Connect(Pin.Out, Pin.E);
		}
	}

	/** Small builder so the graph below reads like the math it computes. Layout is done at the end. */
	class FGraph
	{
	public:
		explicit FGraph(UMaterial* InMaterial) : M(InMaterial) {}

		template <class T>
		T* New()
		{
			return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M, T::StaticClass()));
		}

		FPin Const(float V)
		{
			auto* E = New<UMaterialExpressionConstant>();
			E->R = V;
			return { E };
		}
		FPin Const3(const FLinearColor& V)
		{
			auto* E = New<UMaterialExpressionConstant3Vector>();
			E->Constant = V;
			return { E };
		}
		FPin Scalar(FName Name, float Default, const TCHAR* Group, float SliderMin = 0.f, float SliderMax = 0.f)
		{
			auto* E = New<UMaterialExpressionScalarParameter>();
			E->ParameterName = Name;
			E->DefaultValue = Default;
			E->Group = Group;
			E->SortPriority = Sort++;
			E->SliderMin = SliderMin;
			E->SliderMax = SliderMax;
			return { E };
		}
		/** Vector parameter; Out = RGB (0) or RGBA (5). */
		FPin Vector(FName Name, const FLinearColor& Default, const TCHAR* Group, int32 Out = OutRGB)
		{
			auto* E = New<UMaterialExpressionVectorParameter>();
			E->ParameterName = Name;
			E->DefaultValue = Default;
			E->Group = Group;
			E->SortPriority = Sort++;
			return { E, Out };
		}
		UMaterialExpressionTextureSampleParameter2D* Texture(FName Name, const TCHAR* DefaultPath, EMaterialSamplerType Type, const FPin& UV)
		{
			auto* E = New<UMaterialExpressionTextureSampleParameter2D>();
			E->ParameterName = Name;
			// Linear samplers need a linear default: engine white textures are sRGB and fail to compile there.
			switch (Type)
			{
			case SAMPLERTYPE_Masks:           E->Texture = DefaultLinearTexture(TEXT("T_Glaze_DefaultMask"), TC_Masks); break;
			case SAMPLERTYPE_LinearGrayscale: E->Texture = DefaultLinearTexture(TEXT("T_Glaze_DefaultGray"), TC_Grayscale); break;
			default:                          E->Texture = LoadObject<UTexture>(nullptr, DefaultPath); break;
			}
			E->SamplerType = Type;
			E->SamplerSource = SSM_Wrap_WorldGroupSettings; // shared samplers: no 16-sampler limit
			E->Group = TEXT("01 Textures");
			E->SortPriority = Sort++;
			Link(E->Coordinates, UV);
			return E;
		}
		FPin Switch(FName Name, const FPin& True, const FPin& False, const TCHAR* Group)
		{
			auto* E = New<UMaterialExpressionStaticSwitchParameter>();
			E->ParameterName = Name;
			E->DefaultValue = false;
			E->Group = Group;
			E->SortPriority = Sort++;
			Link(E->A, True);
			Link(E->B, False);
			return { E };
		}

		template <class T>
		FPin Binary(const FPin& A, const FPin& B)
		{
			auto* E = New<T>();
			Link(E->A, A);
			Link(E->B, B);
			return { E };
		}
		FPin Add(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionAdd>(A, B); }
		FPin Sub(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionSubtract>(A, B); }
		FPin Mul(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionMultiply>(A, B); }
		FPin Div(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionDivide>(A, B); }
		FPin Max(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionMax>(A, B); }
		FPin Dot(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionDotProduct>(A, B); }
		FPin Append(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionAppendVector>(A, B); }

		FPin Lerp(const FPin& A, const FPin& B, const FPin& Alpha)
		{
			auto* E = New<UMaterialExpressionLinearInterpolate>();
			Link(E->A, A);
			Link(E->B, B);
			Link(E->Alpha, Alpha);
			return { E };
		}
		FPin OneMinus(const FPin& A)
		{
			auto* E = New<UMaterialExpressionOneMinus>();
			Link(E->Input, A);
			return { E };
		}
		FPin Abs(const FPin& A)
		{
			auto* E = New<UMaterialExpressionAbs>();
			Link(E->Input, A);
			return { E };
		}
		/** Passes a vertex-shader-only value (pre-skinned data) to the pixel shader. */
		FPin Interpolate(const FPin& A)
		{
			auto* E = New<UMaterialExpressionVertexInterpolator>();
			Link(E->Input, A);
			return { E };
		}
		FPin Saturate(const FPin& A)
		{
			auto* E = New<UMaterialExpressionSaturate>();
			Link(E->Input, A);
			return { E };
		}
		FPin Mask(const FPin& A, bool R, bool G, bool B = false, bool Alpha = false)
		{
			auto* E = New<UMaterialExpressionComponentMask>();
			Link(E->Input, A);
			E->R = R;
			E->G = G;
			E->B = B;
			E->A = Alpha;
			return { E };
		}
		/** Sine / cosine of an angle in degrees. */
		FPin SinDeg(const FPin& A)
		{
			auto* E = New<UMaterialExpressionSine>();
			Link(E->Input, A);
			E->Period = 360.f;
			return { E };
		}
		FPin CosDeg(const FPin& A)
		{
			auto* E = New<UMaterialExpressionCosine>();
			Link(E->Input, A);
			E->Period = 360.f;
			return { E };
		}
		FPin Desaturate(const FPin& A, const FPin& Fraction)
		{
			auto* E = New<UMaterialExpressionDesaturation>();
			Link(E->Input, A);
			Link(E->Fraction, Fraction);
			return { E };
		}
		/** Calls an engine material function; inputs are matched by name. Engine content exists in every project. */
		FPin Call(const TCHAR* FunctionPath, const TArray<TPair<FString, FPin>>& Inputs)
		{
			auto* E = New<UMaterialExpressionMaterialFunctionCall>();
			UMaterialFunctionInterface* Function = LoadObject<UMaterialFunctionInterface>(nullptr, FunctionPath);
			if (!Function)
			{
				UE_LOG(LogGlazeMaster, Error, TEXT("Missing engine material function %s"), FunctionPath);
				return {};
			}
			E->SetMaterialFunction(Function);
			for (const TPair<FString, FPin>& In : Inputs)
			{
				FFunctionExpressionInput* Found = E->FunctionInputs.FindByPredicate([&In](const FFunctionExpressionInput& F)
				{
					return F.ExpressionInput && F.ExpressionInput->InputName.ToString().Equals(In.Key, ESearchCase::IgnoreCase);
				});
				if (Found)
				{
					Link(Found->Input, In.Value);
				}
				else
				{
					UE_LOG(LogGlazeMaster, Error, TEXT("%s has no input '%s'"), FunctionPath, *In.Key);
				}
			}
			return { E };
		}

		UMaterial* M;
		int32 Sort = 0;
	};

	const TCHAR* GroupOf(EGlazeSlot Slot)
	{
		switch (Slot)
		{
		case EGlazeSlot::Roughness:        return TEXT("05 Roughness");
		case EGlazeSlot::Metallic:         return TEXT("06 Metallic");
		case EGlazeSlot::Specular:         return TEXT("07 Specular");
		case EGlazeSlot::AmbientOcclusion: return TEXT("08 Ambient Occlusion");
		case EGlazeSlot::Opacity:          return TEXT("09 Opacity");
		default:                           return TEXT("10 Other");
		}
	}

	const TCHAR* DefaultColorTexture = TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");
	const TCHAR* DefaultNormalTexture = TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal");
	const TCHAR* DefaultMasksTexture = TEXT("/Game/Glaze/T_Glaze_DefaultMask.T_Glaze_DefaultMask");
}

void BuildMasterGraph(UMaterial* Material)
{
	UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
	FGraph G(Material);
	UMaterialEditorOnlyData* Out = Material->GetEditorOnlyData();

	// ---- UV: rotate around the tile center, then tile and offset ----
	const TCHAR* GroupUV = TEXT("02 UV");
	auto* TexCoord = G.New<UMaterialExpressionTextureCoordinate>();
	const FPin Angle = G.Scalar(Param::Rotation, 0.f, GroupUV, -180.f, 180.f);
	const FPin Sin = G.SinDeg(Angle);
	const FPin Cos = G.CosDeg(Angle);
	const FPin Centered = G.Sub({ TexCoord }, G.Const(0.5f));
	const FPin CX = G.Mask(Centered, true, false);
	const FPin CY = G.Mask(Centered, false, true);
	const FPin RX = G.Sub(G.Mul(CX, Cos), G.Mul(CY, Sin));
	const FPin RY = G.Add(G.Mul(CX, Sin), G.Mul(CY, Cos));
	const FPin Rotated = G.Add(G.Append(RX, RY), G.Const(0.5f));
	const FPin TilingXY = G.Mask(G.Vector(Param::TilingXY, FLinearColor(1, 1, 0, 0), GroupUV), true, true);
	const FPin Tiling = G.Mul(TilingXY, G.Scalar(Param::Tiling, 1.f, GroupUV, 0.01f, 16.f));
	const FPin OffsetXY = G.Mask(G.Vector(Param::Offset, FLinearColor(0, 0, 0, 0), GroupUV), true, true);
	const FPin UV = G.Add(G.Mul(Rotated, Tiling), OffsetXY);

	// ---- Detail noise: a grayscale mask in UV / world / object / bind-pose space, applied to color and roughness below ----
	FPin Detail;
	{
		const TCHAR* Group = TEXT("12 Detail Noise");
		auto* WorldPos = G.New<UMaterialExpressionWorldPosition>();
		WorldPos->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets; // no swimming once WPO is used
		auto* LocalPos = G.New<UMaterialExpressionLocalPosition>();
		LocalPos->IncludedOffsets = EPositionIncludedOffsets::ExcludeOffsets;
		auto* NormalWS = G.New<UMaterialExpressionVertexNormalWS>();
		auto* NormalLocal = G.New<UMaterialExpressionTransform>();
		NormalLocal->TransformSourceType = TRANSFORMSOURCE_World;
		NormalLocal->TransformType = TRANSFORM_Local;
		Link(NormalLocal->Input, { NormalWS });
		// Pre-skinned data exists only in the vertex shader.
		const FPin PreSkinnedPos = G.Interpolate({ G.New<UMaterialExpressionPreSkinnedPosition>() });
		const FPin PreSkinnedNormal = G.Interpolate({ G.New<UMaterialExpressionPreSkinnedNormal>() });

		const FPin Position = G.Switch(Param::DetailPreSkinned, PreSkinnedPos, G.Switch(Param::DetailLocal, { LocalPos }, { WorldPos }, Group), Group);
		const FPin Normal = G.Switch(Param::DetailPreSkinned, PreSkinnedNormal, G.Switch(Param::DetailLocal, { NormalLocal }, { NormalWS }, Group), Group);
		const FPin P = G.Div(Position, G.Scalar(Param::DetailSize, 200.f, Group, 1.f, 1000.f));
		const FPin DetailUV = G.Mul({ TexCoord }, G.Scalar(Param::DetailTiling, 2.f, Group, 0.1f, 16.f));

		// Texture: one sample in UV space, or triplanar (three samples blended by the normal) in 3D spaces.
		TArray<UMaterialExpressionTextureSampleParameter2D*> Samples;
		auto Sample = [&](const FPin& Coords) -> FPin
		{
			UMaterialExpressionTextureSampleParameter2D* E = G.Texture(Param::DetailMap, nullptr, SAMPLERTYPE_LinearGrayscale, Coords);
			if (Samples.Num() > 0)
			{
				E->SortPriority = Samples[0]->SortPriority; // one parameter, four expressions: keep their metadata identical
			}
			Samples.Add(E);
			return { E, 1 }; // R
		};
		const FPin UVSample = Sample(DetailUV);
		FPin W = G.Abs(Normal);
		W = G.Mul(W, W);
		W = G.Mul(W, W); // sharper blend between the projections
		W = G.Div(W, G.Dot(W, G.Const3(FLinearColor(1, 1, 1))));
		const FPin X = Sample(G.Mask(P, false, true, true));
		const FPin Y = Sample(G.Mask(P, true, false, true));
		const FPin Z = Sample(G.Mask(P, true, true));
		const FPin Triplanar = G.Dot(G.Append(G.Append(X, Y), Z), W);
		const FPin FromTexture = G.Switch(Param::DetailUV, UVSample, Triplanar, Group);

		// Procedural: no texture, works the same in every space.
		auto* Noise = G.New<UMaterialExpressionNoise>();
		Noise->Scale = 1.f;
		Noise->NoiseFunction = NOISEFUNCTION_GradientTex3D;
		Noise->Levels = 4;
		Noise->OutputMin = 0.f;
		Noise->OutputMax = 1.f;
		Link(Noise->Position, G.Switch(Param::DetailUV, G.Append(DetailUV, G.Const(0.f)), P, Group));

		Detail = G.Switch(Param::DetailFromTexture, FromTexture, { Noise }, Group);
		const FPin Half = G.Const(0.5f);
		Detail = G.Saturate(G.Add(G.Add(G.Mul(G.Sub(Detail, Half), G.Scalar(Param::DetailContrast, 1.f, Group, 0.f, 8.f)), Half),
			G.Scalar(Param::DetailBias, 0.f, Group, -1.f, 1.f)));
	}

	// ---- Base color ----
	const TCHAR* GroupColor = TEXT("03 Base Color");
	const TCHAR* GroupCC = TEXT("04 Color Correction");
	UMaterialExpressionTextureSampleParameter2D* BaseTex = G.Texture(Param::Map(EGlazeSlot::BaseColor), DefaultColorTexture, SAMPLERTYPE_Color, UV);
	FPin Color = G.Switch(Param::UseMap(EGlazeSlot::BaseColor), { BaseTex, OutRGB }, G.Const3(FLinearColor::White), GroupColor);
	{
		const FPin Desat = G.Desaturate(Color, G.Scalar(Param::Desaturation, 0.f, GroupCC, 0.f, 1.f));
		// Contrast around linear mid grey (0.18 = sRGB 0.5), so a contrast change keeps mid tones in place.
		const FPin Pivot = G.Const(0.18f);
		const FPin Contrasted = G.Max(G.Add(G.Mul(G.Sub(Desat, Pivot), G.Scalar(Param::ColorContrast, 1.f, GroupCC, 0.f, 3.f)), Pivot), G.Const(0.f));
		const FPin Bright = G.Mul(Contrasted, G.Scalar(Param::Brightness, 1.f, GroupCC, 0.f, 3.f));
		const FPin Hue = G.Call(TEXT("/Engine/Functions/Engine_MaterialFunctions02/HueShift.HueShift"),
			{ { TEXT("Hue Shift Percentage"), G.Scalar(Param::HueShift, 0.f, GroupCC, 0.f, 1.f) }, { TEXT("Texture"), Bright } });
		const FPin Overlay = G.Call(TEXT("/Engine/Functions/Engine_MaterialFunctions03/Blends/Blend_Overlay.Blend_Overlay"),
			{ { TEXT("Base"), Hue }, { TEXT("Blend"), G.Vector(Param::OverlayColor, FLinearColor(0.5f, 0.5f, 0.5f), GroupCC) } });
		const FPin Overlaid = G.Lerp(Hue, Overlay, G.Scalar(Param::OverlayAmount, 0.f, GroupCC, 0.f, 1.f));
		const FPin Lerped = G.Lerp(Overlaid, G.Vector(Param::LerpColor, FLinearColor::White, GroupCC), G.Scalar(Param::LerpAmount, 0.f, GroupCC, 0.f, 1.f));
		Color = G.Switch(Param::ColorCorrection, Lerped, Color, GroupCC);
	}
	{
		const TCHAR* Group = TEXT("12 Detail Noise");
		const FPin Dirty = G.Lerp(Color, G.Vector(Param::DetailColor, FLinearColor(0.05f, 0.04f, 0.03f), Group),
			G.Mul(Detail, G.Scalar(Param::DetailColorAmount, 0.3f, Group, 0.f, 1.f)));
		Color = G.Switch(Param::UseDetail, Dirty, Color, Group);
	}
	Link(Out->BaseColor, G.Mul(Color, G.Vector(Param::Tint, FLinearColor::White, GroupColor)));

	// ---- Normal: strength, green flip, rotation compensation (vectors rotate with the UVs) ----
	{
		const TCHAR* GroupNormal = TEXT("10 Normal");
		UMaterialExpressionTextureSampleParameter2D* NormalTex = G.Texture(Param::Map(EGlazeSlot::Normal), DefaultNormalTexture, SAMPLERTYPE_Normal, UV);
		const FPin N = { NormalTex, OutRGB };
		const FPin NX = G.Mask(N, true, false);
		const FPin Flip = G.Sub(G.Const(1.f), G.Mul(G.Scalar(Param::FlipGreen, 0.f, GroupNormal, 0.f, 1.f), G.Const(2.f)));
		const FPin NY = G.Mul(G.Mask(N, false, true), Flip);
		// Inverse rotation of the UV rotation above.
		const FPin RNX = G.Add(G.Mul(NX, Cos), G.Mul(NY, Sin));
		const FPin RNY = G.Sub(G.Mul(NY, Cos), G.Mul(NX, Sin));
		const FPin XY = G.Mul(G.Append(RNX, RNY), G.Scalar(Param::NormalStrength, 1.f, GroupNormal, 0.f, 3.f));
		const FPin Normal = G.Append(XY, G.Mask(N, false, false, true));
		Link(Out->Normal, G.Switch(Param::UseMap(EGlazeSlot::Normal), Normal, G.Const3(FLinearColor(0, 0, 1)), GroupNormal));
	}

	// ---- Grayscale channels: own map or a channel of the shared packed map, remapped; or a constant ----
	UMaterialExpressionTextureSampleParameter2D* PackedTex = G.Texture(Param::PackedMap, DefaultMasksTexture, SAMPLERTYPE_Masks, UV);
	auto Grayscale = [&](EGlazeSlot Slot, const FPin& NoMap) -> FPin
	{
		const TCHAR* Group = GroupOf(Slot);
		UMaterialExpressionTextureSampleParameter2D* Tex = G.Texture(Param::Map(Slot), DefaultMasksTexture, SAMPLERTYPE_Masks, UV);
		const FPin Channel = G.Vector(Param::Channel(Slot), FLinearColor(1, 0, 0, 0), Group, OutRGBA);
		const FPin Own = G.Dot({ Tex, OutRGBA }, Channel);
		const FPin Packed = G.Dot({ PackedTex, OutRGBA }, Channel);
		FPin V = G.Switch(Param::FromPacked(Slot), Packed, Own, Group);
		V = G.Lerp(V, G.OneMinus(V), G.Scalar(Param::Invert(Slot), 0.f, Group, 0.f, 1.f));
		const FPin Half = G.Const(0.5f);
		V = G.Saturate(G.Add(G.Add(G.Mul(G.Sub(V, Half), G.Scalar(Param::Contrast(Slot), 1.f, Group, 0.f, 4.f)), Half),
			G.Scalar(Param::Bias(Slot), 0.f, Group, -1.f, 1.f)));
		V = G.Lerp(G.Scalar(Param::Min(Slot), 0.f, Group, 0.f, 1.f), G.Scalar(Param::Max(Slot), 1.f, Group, 0.f, 1.f), V);
		return G.Switch(Param::UseMap(Slot), V, NoMap, Group);
	};
	{
		const TCHAR* Group = TEXT("12 Detail Noise");
		const FPin Roughness = Grayscale(EGlazeSlot::Roughness, G.Scalar(Param::Value(EGlazeSlot::Roughness), 0.5f, GroupOf(EGlazeSlot::Roughness), 0.f, 1.f));
		const FPin Varied = G.Saturate(G.Add(Roughness, G.Mul(Detail, G.Scalar(Param::DetailRoughness, 0.15f, Group, -1.f, 1.f))));
		Link(Out->Roughness, G.Switch(Param::UseDetail, Varied, Roughness, Group));
	}
	Link(Out->Metallic, Grayscale(EGlazeSlot::Metallic, G.Scalar(Param::Value(EGlazeSlot::Metallic), 0.f, GroupOf(EGlazeSlot::Metallic), 0.f, 1.f)));
	Link(Out->Specular, Grayscale(EGlazeSlot::Specular, G.Scalar(Param::Value(EGlazeSlot::Specular), 0.5f, GroupOf(EGlazeSlot::Specular), 0.f, 1.f)));
	Link(Out->AmbientOcclusion, Grayscale(EGlazeSlot::AmbientOcclusion,
		G.Scalar(Param::Value(EGlazeSlot::AmbientOcclusion), 1.f, GroupOf(EGlazeSlot::AmbientOcclusion), 0.f, 1.f)));

	// ---- Opacity (used when the instance overrides Blend Mode to Masked) ----
	{
		const TCHAR* Group = GroupOf(EGlazeSlot::Opacity);
		FPin Opacity = Grayscale(EGlazeSlot::Opacity, { BaseTex, OutA });
		Opacity = G.Mul(Opacity, G.Scalar(Param::OpacityAmount, 1.f, Group, 0.f, 1.f));
		const FPin Dithered = G.Call(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA"),
			{ { TEXT("Alpha Threshold"), Opacity } });
		Opacity = G.Switch(Param::Dithered, Dithered, Opacity, Group);
		Link(Out->OpacityMask, G.Switch(Param::UseOpacity, Opacity, G.Const(1.f), Group));
	}

	// ---- Emissive ----
	{
		const TCHAR* Group = TEXT("11 Emissive");
		UMaterialExpressionTextureSampleParameter2D* EmissiveTex = G.Texture(Param::Map(EGlazeSlot::Emissive), DefaultColorTexture, SAMPLERTYPE_Color, UV);
		FPin Map = G.Switch(Param::UseMap(EGlazeSlot::Emissive), { EmissiveTex, OutRGB }, G.Const3(FLinearColor::White), Group);
		Map = G.Switch(Param::EmissiveFromBaseColor, { BaseTex, OutRGB }, Map, Group);
		const FPin Emissive = G.Mul(G.Mul(Map, G.Vector(Param::EmissiveColor, FLinearColor::White, Group)),
			G.Scalar(Param::EmissiveIntensity, 1.f, Group, 0.f, 50.f));
		Link(Out->EmissiveColor, G.Switch(Param::UseEmissive, Emissive, G.Const3(FLinearColor::Black), Group));
	}

	UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	// Parameter queries read the cached data; PostEditChange would refresh it too, but callers (tests) may skip that.
	Material->UpdateCachedExpressionData();
}

bool IsGlazeMaster(const UObject* Material)
{
	return Material && Material->GetPathName() == MasterObjectPath;
}

UMaterial* EnsureMaster(bool bSave)
{
	UMaterial* Material = LoadObject<UMaterial>(nullptr, MasterObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Material)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		Material = Cast<UMaterial>(AssetTools.CreateAsset(TEXT("M_Glaze"), FPackageName::GetLongPackagePath(MasterPackagePath),
			UMaterial::StaticClass(), Factory));
		if (!Material)
		{
			UE_LOG(LogGlazeMaster, Error, TEXT("Could not create %s"), MasterPackagePath);
			return nullptr;
		}
	}

	UPackage* Package = Material->GetPackage();
	FMetaData& Meta = Package->GetMetaData();
	const int32 Version = FCString::Atoi(*Meta.GetValue(Material, MetaVersionKey));
	if (Version == MasterVersion)
	{
		return Material;
	}

	UE_LOG(LogGlazeMaster, Display, TEXT("Building %s (version %d -> %d)"), MasterPackagePath, Version, MasterVersion);
	Material->PreEditChange(nullptr);
	Material->BlendMode = BLEND_Opaque;
	for (EMaterialUsage Usage : { MATUSAGE_SkeletalMesh, MATUSAGE_InstancedStaticMeshes, MATUSAGE_SplineMesh, MATUSAGE_GeometryCache, MATUSAGE_NiagaraMeshParticles })
	{
		Material->SetUsageByFlag(Usage, true);
	}
	BuildMasterGraph(Material);
	Material->PostEditChange();
	Meta.SetValue(Material, MetaVersionKey, *LexToString(MasterVersion));
	Material->MarkPackageDirty();

	if (bSave)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ Package }, /*bOnlyDirty*/ false);
	}
	return Material;
}
}
