#include "GlazeMatcher.h"

#include "Engine/Texture2D.h"
#include "Algo/Count.h"
#include "Internationalization/Text.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

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
			{ TEXT("albedotransparency"), { S(E::BaseColor) } }, // alpha as opacity needs a color-sampled alpha in the master (roadmap)
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

	/** Case folding for set keys: ASCII first (locale independent, so Turkish "I" stays "i"), then ICU for the rest (Cyrillic etc.). */
	FString FoldCase(const FString& In)
	{
		const FString Ascii = In.ToLower();
		bool bAscii = true;
		for (const TCHAR C : Ascii)
		{
			if (C > 127)
			{
				bAscii = false;
				break;
			}
		}
		return bAscii ? Ascii : FText::FromString(Ascii).ToLower().ToString();
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

	/** One key per resolution: "2048" and "2K" are the same ("1k", "2k", "4k", "8k", "512"). */
	FString NormalizeResolution(const FString& Token)
	{
		const FString L = Token.ToLower();
		if (L == TEXT("1024")) return TEXT("1k");
		if (L == TEXT("2048")) return TEXT("2k");
		if (L == TEXT("4096")) return TEXT("4k");
		if (L == TEXT("8192")) return TEXT("8k");
		return L;
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
	FNameMatch Result;
	for (const FString& T : Tokens)
	{
		if (IsResolutionToken(T))
		{
			Result.Resolution = NormalizeResolution(T);
			break;
		}
	}
	Tokens.RemoveAll([](const FString& T) { return IsResolutionToken(T); });

	if (Tokens.Num() == 0)
	{
		Result.SetName = AssetName;
		return Result;
	}

	// Two last tokens joined first ("Base_Color", "Normal_GL", "Ambient_Occlusion"), then the last one.
	int32 Consumed = 0;
	const FSuffixRule* Rule = nullptr;
	if (Tokens.Num() >= 2)
	{
		Rule = FindRule(Tokens[Tokens.Num() - 2] + Tokens.Last());
		Consumed = Rule ? 2 : 0;
	}
	if (!Rule && Tokens.Num() >= 2)
	{
		Rule = FindRule(Tokens.Last());
		Consumed = Rule ? 1 : 0;
	}
	if (!Rule)
	{
		// A lone exact channel name ("Normal", "Roughness"): the set has no name of its own. Short ones ("N", "R") stay ambiguous.
		if (Tokens.Num() == 1)
		{
			const FSuffixRule* Exact = FindRule(Tokens[0]);
			if (Exact && FString(Exact->Token).Len() >= 4)
			{
				Result.Slots = Exact->Slots;
				Result.Reason = TEXT("_") + Tokens[0];
				return Result; // SetName stays empty
			}
		}
		// "Wood01BaseColor" / "Wood_Floor01Normal": a long suffix (4+ letters) glued to the last token. Longest suffix wins.
		const FString Last = Tokens.Last();
		const FString Lower = Last.ToLower();
		const FSuffixRule* Best = nullptr;
		int32 BestLen = 0;
		for (const FSuffixRule& Candidate : Rules())
		{
			const FString Token(Candidate.Token);
			if (Token.Len() >= 4 && Token.Len() > BestLen && Lower.Len() > Token.Len() && Lower.EndsWith(Token))
			{
				Best = &Candidate;
				BestLen = Token.Len();
			}
		}
		if (Best)
		{
			Tokens.Last() = Last.LeftChop(BestLen);
			Result.SetName = FString::Join(Tokens, TEXT("_"));
			Result.Slots = Best->Slots;
			Result.Reason = FString::Printf(TEXT("...%s"), *Last.Right(BestLen));
			return Result;
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
			// The texture asset may already flip green on import: flipping in the material too would undo it.
			Out.bFlipGreen |= Slot.bFlipGreen && !Texture->bFlipGreenChannel;
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
	struct FEntry
	{
		UTexture2D* Texture = nullptr;
		FString Resolution;
	};
	struct FGroup
	{
		FString Key;
		FString DisplayName;
		FString Folder;
		TArray<FEntry> Entries;
	};

	// 1. Same folder + same (case-folded) name = same candidate set.
	TArray<FGroup> Groups;
	for (UTexture2D* Texture : Textures)
	{
		if (!Texture)
		{
			continue;
		}
		const FString Folder = FPackageName::GetLongPackagePath(Texture->GetOutermost()->GetName());
		const FNameMatch Match = MatchTextureName(Texture->GetName());
		FString Display = Match.SetName;
		if (Display.IsEmpty())
		{
			// "Normal" alone: the folder names the set.
			Folder.Split(TEXT("/"), nullptr, &Display, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Display.IsEmpty())
			{
				Display = Texture->GetName();
			}
		}
		const FString Key = FoldCase(Folder) + TEXT("|") + FoldCase(Display);
		FGroup* Group = Groups.FindByPredicate([&Key](const FGroup& G) { return G.Key == Key; });
		if (!Group)
		{
			Group = &Groups.AddDefaulted_GetRef();
			Group->Key = Key;
			Group->DisplayName = Display;
			Group->Folder = Folder;
		}
		Group->Entries.Add({ Texture, Match.Resolution });
	}

	// 2. Several resolutions of the same name are separate sets; maps without a resolution join the first one.
	struct FSet
	{
		FString Name;
		FString Folder;
		TArray<UTexture2D*> Textures;
	};
	TArray<FSet> Result;
	for (const FGroup& Group : Groups)
	{
		TArray<FString> Resolutions;
		for (const FEntry& E : Group.Entries)
		{
			if (!E.Resolution.IsEmpty())
			{
				Resolutions.AddUnique(E.Resolution);
			}
		}
		if (Resolutions.Num() <= 1)
		{
			FSet& Set = Result.AddDefaulted_GetRef();
			Set.Name = Group.DisplayName;
			Set.Folder = Group.Folder;
			for (const FEntry& E : Group.Entries)
			{
				Set.Textures.Add(E.Texture);
			}
			continue;
		}
		const int32 First = Result.Num();
		for (const FString& Res : Resolutions)
		{
			FSet& Set = Result.AddDefaulted_GetRef();
			Set.Name = Group.DisplayName + TEXT(" (") + Res.ToUpper() + TEXT(")");
			Set.Folder = Group.Folder;
		}
		for (const FEntry& E : Group.Entries)
		{
			const int32 Index = E.Resolution.IsEmpty() ? 0 : Resolutions.IndexOfByKey(E.Resolution);
			Result[First + Index].Textures.Add(E.Texture);
		}
	}

	// 3. Equal names from different folders: tell them apart by the folder.
	TArray<TPair<FString, TArray<UTexture2D*>>> Sets;
	for (const FSet& Set : Result)
	{
		FString Name = Set.Name;
		const int32 Same = Algo::CountIf(Result, [&Set](const FSet& O) { return FoldCase(O.Name) == FoldCase(Set.Name); });
		if (Same > 1)
		{
			Name += TEXT(" [") + Set.Folder + TEXT("]");
		}
		Sets.Emplace(Name, Set.Textures);
	}
	return Sets;
}
}
