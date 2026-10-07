#include "GlazeMatcher.h"

#include "Engine/Texture2D.h"

namespace Glaze
{
namespace
{
	struct FSuffixRule
	{
		const TCHAR* Token;
		TArray<FSlotMatch> Slots;
	};

	FSlotMatch S(EGlazeSlot Slot, EGlazeChannel Channel = EGlazeChannel::R, bool bInvert = false, bool bFlip = false)
	{
		FSlotMatch M;
		M.Slot = Slot;
		M.Channel = Channel;
		M.bInvert = bInvert;
		M.bFlipGreen = bFlip;
		return M;
	}

	const TArray<FSuffixRule>& Rules()
	{
		using C = EGlazeChannel;
		using E = EGlazeSlot;
		static const TArray<FSuffixRule> R = {
			// Packed maps first: they are the most specific tokens.
			{ TEXT("orm"),  { S(E::AmbientOcclusion, C::R), S(E::Roughness, C::G), S(E::Metallic, C::B) } },
			{ TEXT("arm"),  { S(E::AmbientOcclusion, C::R), S(E::Roughness, C::G), S(E::Metallic, C::B) } },
			{ TEXT("rma"),  { S(E::Roughness, C::R), S(E::Metallic, C::G), S(E::AmbientOcclusion, C::B) } },
			{ TEXT("mra"),  { S(E::Metallic, C::R), S(E::Roughness, C::G), S(E::AmbientOcclusion, C::B) } },
			{ TEXT("mrao"), { S(E::Metallic, C::R), S(E::Roughness, C::G), S(E::AmbientOcclusion, C::B) } },
			{ TEXT("ordp"), { S(E::AmbientOcclusion, C::R), S(E::Roughness, C::G), S(E::Height, C::B) } },
			{ TEXT("basecolor"), { S(E::BaseColor) } }, { TEXT("albedo"), { S(E::BaseColor) } },
			{ TEXT("albedotransparency"), { S(E::BaseColor) } },
			{ TEXT("diffuse"), { S(E::BaseColor) } }, { TEXT("diff"), { S(E::BaseColor) } },
			{ TEXT("color"), { S(E::BaseColor) } }, { TEXT("colour"), { S(E::BaseColor) } },
			{ TEXT("col"), { S(E::BaseColor) } }, { TEXT("alb"), { S(E::BaseColor) } },
			{ TEXT("bc"), { S(E::BaseColor) } }, { TEXT("d"), { S(E::BaseColor) } },
			{ TEXT("normalgl"), { S(E::Normal, C::R, false, true) } }, { TEXT("norgl"), { S(E::Normal, C::R, false, true) } },
			{ TEXT("nrmgl"), { S(E::Normal, C::R, false, true) } }, { TEXT("ngl"), { S(E::Normal, C::R, false, true) } },
			{ TEXT("normal"), { S(E::Normal) } }, { TEXT("normaldx"), { S(E::Normal) } }, { TEXT("nordx"), { S(E::Normal) } },
			{ TEXT("nrm"), { S(E::Normal) } }, { TEXT("nor"), { S(E::Normal) } }, { TEXT("norm"), { S(E::Normal) } },
			{ TEXT("n"), { S(E::Normal) } },
			{ TEXT("roughness"), { S(E::Roughness) } }, { TEXT("rough"), { S(E::Roughness) } },
			{ TEXT("rgh"), { S(E::Roughness) } }, { TEXT("r"), { S(E::Roughness) } },
			{ TEXT("glossiness"), { S(E::Roughness, C::R, true) } }, { TEXT("gloss"), { S(E::Roughness, C::R, true) } },
			{ TEXT("smoothness"), { S(E::Roughness, C::R, true) } }, { TEXT("smooth"), { S(E::Roughness, C::R, true) } },
			{ TEXT("metallic"), { S(E::Metallic) } }, { TEXT("metalness"), { S(E::Metallic) } },
			{ TEXT("metal"), { S(E::Metallic) } }, { TEXT("met"), { S(E::Metallic) } }, { TEXT("mtl"), { S(E::Metallic) } },
			{ TEXT("m"), { S(E::Metallic) } },
			{ TEXT("ambientocclusion"), { S(E::AmbientOcclusion) } }, { TEXT("occlusion"), { S(E::AmbientOcclusion) } },
			{ TEXT("ao"), { S(E::AmbientOcclusion) } }, { TEXT("occ"), { S(E::AmbientOcclusion) } },
			{ TEXT("specular"), { S(E::Specular) } }, { TEXT("spec"), { S(E::Specular) } }, { TEXT("s"), { S(E::Specular) } },
			{ TEXT("opacity"), { S(E::Opacity) } }, { TEXT("alpha"), { S(E::Opacity) } }, { TEXT("mask"), { S(E::Opacity) } },
			{ TEXT("transparency"), { S(E::Opacity) } },
			{ TEXT("emissive"), { S(E::Emissive) } }, { TEXT("emission"), { S(E::Emissive) } }, { TEXT("emit"), { S(E::Emissive) } },
			{ TEXT("glow"), { S(E::Emissive) } }, { TEXT("e"), { S(E::Emissive) } },
			{ TEXT("height"), { S(E::Height) } }, { TEXT("displacement"), { S(E::Height) } }, { TEXT("disp"), { S(E::Height) } },
			{ TEXT("h"), { S(E::Height) } },
		};
		return R;
	}

	const FSuffixRule* FindRule(const FString& Token)
	{
		for (const FSuffixRule& Rule : Rules())
		{
			if (Token.Equals(Rule.Token, ESearchCase::IgnoreCase))
			{
				return &Rule;
			}
		}
		return nullptr;
	}

	bool IsResolutionToken(const FString& Token)
	{
		const FString L = Token.ToLower();
		if (L.Len() >= 2 && L.EndsWith(TEXT("k")) && L.LeftChop(1).IsNumeric())
		{
			return true; // 1k, 2k, 4k, 8k
		}
		return L == TEXT("512") || L == TEXT("1024") || L == TEXT("2048") || L == TEXT("4096") || L == TEXT("8192");
	}
}

FNameMatch MatchTextureName(const FString& AssetName)
{
	TArray<FString> Tokens;
	static const TCHAR* Delimiters[] = { TEXT("_"), TEXT("-"), TEXT("."), TEXT(" ") };
	AssetName.ParseIntoArray(Tokens, Delimiters, UE_ARRAY_COUNT(Delimiters));

	// Prefix T_ / TX_.
	if (Tokens.Num() > 1 && (Tokens[0].Equals(TEXT("T"), ESearchCase::IgnoreCase) || Tokens[0].Equals(TEXT("TX"), ESearchCase::IgnoreCase)))
	{
		Tokens.RemoveAt(0);
	}
	Tokens.RemoveAll([](const FString& T) { return IsResolutionToken(T); });

	FNameMatch Result;
	if (Tokens.Num() == 0)
	{
		Result.SetName = AssetName;
		return Result;
	}

	// Two last tokens joined first ("Base_Color", "Normal_GL", "Ambient_Occlusion"), then the last one.
	// Single-letter tokens only count when something else names the set.
	int32 Consumed = 0;
	const FSuffixRule* Rule = nullptr;
	if (Tokens.Num() >= 3)
	{
		Rule = FindRule(Tokens[Tokens.Num() - 2] + Tokens.Last());
		Consumed = Rule ? 2 : 0;
	}
	if (!Rule && Tokens.Num() >= 2)
	{
		Rule = FindRule(Tokens.Last());
		Consumed = Rule ? 1 : 0;
	}
	if (!Rule && Tokens.Num() == 1)
	{
		// "Wood01BaseColor" style: a long suffix glued to the name.
		const FString Lower = Tokens[0].ToLower();
		for (const FSuffixRule& Candidate : Rules())
		{
			const FString Token(Candidate.Token);
			if (Token.Len() >= 4 && Lower.EndsWith(Token) && Lower.Len() > Token.Len())
			{
				Result.SetName = Tokens[0].LeftChop(Token.Len());
				Result.Slots = Candidate.Slots;
				Result.Reason = FString::Printf(TEXT("...%s"), *Tokens[0].Right(Token.Len()));
				return Result;
			}
		}
	}

	if (Rule)
	{
		Result.Slots = Rule->Slots;
		Result.Reason = TEXT("_") + FString::Join(TArrayView<const FString>(Tokens).Right(Consumed), TEXT("_"));
		Tokens.SetNum(Tokens.Num() - Consumed);
	}
	Result.SetName = FString::Join(Tokens, TEXT("_"));
	return Result;
}

FTextureSetMatch MatchTextures(const TArray<UTexture2D*>& Textures, UGlazeRecipe& Recipe)
{
	FTextureSetMatch Out;
	static const TCHAR* ChannelNames[] = { TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
	for (UTexture2D* Texture : Textures)
	{
		if (!Texture)
		{
			continue;
		}
		const FNameMatch Match = MatchTextureName(Texture->GetName());
		if (Out.SetName.IsEmpty())
		{
			Out.SetName = Match.SetName;
		}
		bool bUsed = false;
		for (const FSlotMatch& Slot : Match.Slots)
		{
			if (Recipe.GetSlot(Slot.Slot))
			{
				continue;
			}
			Recipe.SetSlot(Slot.Slot, Texture, Slot.Channel);
			bUsed = true;
			FString Reason = Match.Reason;
			if (Match.Slots.Num() > 1)
			{
				Reason += FString(TEXT(".")) + ChannelNames[static_cast<int32>(Slot.Channel)];
			}
			Out.Reasons.Add(Slot.Slot, Reason);
			Out.bInvertRoughness |= Slot.bInvert && Slot.Slot == EGlazeSlot::Roughness;
			Out.bFlipGreen |= Slot.bFlipGreen;
		}
		if (!bUsed)
		{
			Out.Unmatched.Add(Texture);
		}
	}

	// A single unnamed texture is most likely a color map.
	if (Textures.Num() == 1 && Out.Unmatched.Num() == 1 && !Recipe.BaseColor)
	{
		Recipe.BaseColor = Out.Unmatched[0];
		Out.Reasons.Add(EGlazeSlot::BaseColor, TEXT("single texture"));
		Out.Unmatched.Reset();
	}
	return Out;
}

TArray<TPair<FString, TArray<UTexture2D*>>> GroupTextureSets(const TArray<UTexture2D*>& Textures)
{
	TArray<TPair<FString, TArray<UTexture2D*>>> Sets;
	for (UTexture2D* Texture : Textures)
	{
		if (!Texture)
		{
			continue;
		}
		const FString SetName = MatchTextureName(Texture->GetName()).SetName;
		TPair<FString, TArray<UTexture2D*>>* Found = Sets.FindByPredicate(
			[&SetName](const TPair<FString, TArray<UTexture2D*>>& P) { return P.Key.Equals(SetName, ESearchCase::IgnoreCase); });
		if (!Found)
		{
			Found = &Sets.Emplace_GetRef(SetName, TArray<UTexture2D*>());
		}
		Found->Value.Add(Texture);
	}
	return Sets;
}
}
