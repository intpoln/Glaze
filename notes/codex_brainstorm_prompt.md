You are a senior Unreal Engine 5 technical artist + tools designer. Brainstorm (do NOT write code) an editor plugin for UE 5.4–5.8 named "Glaze" (working name).

Goal: create basic-to-medium PBR materials from a set of textures as FAST as possible, with great UX.
Flow idea: select textures in Content Browser -> right click -> "Glaze: Create Material Instance" -> a window opens where
the selected textures are auto-matched to slots (BaseColor, Normal, Roughness, Metallic, AO, Height, Emissive, Opacity, packed ORM/ARM/RMA...) by name suffix,
user can fix mapping, and tick feature modules:
 - Color correction: Desaturation, Contrast, Brightness, Tint, Hue shift, Blend Overlay color, Lerp to Color, Color-to-Lerp (mask by color);
 - Roughness / Specular / AO / Metallic remap: bias, contrast, min/max, invert, curve;
 - Opacity / Dithered Masked (DitherTemporalAA, distance fade, camera fade);
 - WPO (wind/wobble, noise displacement), detail via world-space / local-space noise (texture noise or 3D noise), triplanar/world-aligned, macro variation, detail normal;
 - UV: tiling, offset, rotation, per-instance random.
Users are artists (motion designer) who constantly build such materials by hand.

Answer in RUSSIAN, concise, structured (markdown), ~150-250 lines max:
1. Architecture options and your recommendation:
   (a) Master material(s) with static switches + material instance (MIC) creation — content-only plugin;
   (b) generating a real Material graph via MaterialEditingLibrary (standalone material);
   (c) hybrid. Discuss shader permutation cost, compile times, Substrate vs legacy, Nanite/Lumen, packaging, engine-version compat of .uasset (forward only), maintainability.
2. Can it be done WITHOUT C++ (content-only plugin: Editor Utility Widget + Scripted Asset Action / Asset Action Utility, Blueprints, maybe Python)? Concrete limits: context menu on textures, modal window, auto-detect texture type, setting texture sampler type (Normal/Masks/linear color), sRGB flags, creating MIC/material assets, static switch params from BP, saving. Where C++ would really be needed / make it much better. Name the exact UE classes/functions involved.
3. UX: window layout, auto-matching rules (suffixes _BC/_D/_N/_R/_ORM/_ARM ...), presets (Wood, Metal, Fabric, Foliage, Decal...), live preview, batch mode (many texture sets at once -> many instances), naming/folder conventions, "re-open the window on an existing Glaze instance to edit", undo.
4. Feature list: MVP vs v2 vs later, plus ideas the user did not mention but that save time (e.g. auto texture compression fix, packing check, material function library, quick layer variants).
5. Risks / pitfalls.
Do not edit any files. Just print the answer.
