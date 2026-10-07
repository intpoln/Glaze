"""Copies the rendered pack (TexturePack/raw) into the plugin: Plugins/Glaze/Resources/Pack.

- Materials: <Name>/<2k|1k>/T_<Name>_<BC|N|ORM>.png (BC stays RGB, N and ORM are data).
- Noises: Noises/<2k|1k>/T_Noise_<Name>.png, stored as 8-bit grayscale.
- Graph JSONs go to TexturePack/graphs (editable sources, not shipped).
- Writes Pack/pack.json (manifest read by the plugin).

Run: python Tools/texgen/package_pack.py
"""
import json
import os
import shutil

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RAW = os.path.join(ROOT, "TexturePack", "raw")
GRAPHS = os.path.join(ROOT, "TexturePack", "graphs")
DEST = os.path.join(ROOT, "GlazeLab", "Plugins", "Glaze", "Resources", "Pack")


def save(src, dst, mode):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    Image.open(src).convert(mode).save(dst, optimize=True)


def main():
    if os.path.isdir(DEST):
        shutil.rmtree(DEST)
    os.makedirs(GRAPHS, exist_ok=True)
    manifest = {"materials": [], "noises": []}
    for name in sorted(os.listdir(RAW)):
        folder = os.path.join(RAW, name)
        if not os.path.isdir(folder) or name == "test":
            continue
        if name == "Noises":
            for res in ("2k", "1k"):
                for f in sorted(os.listdir(os.path.join(folder, res))):
                    save(os.path.join(folder, res, f), os.path.join(DEST, "Noises", res, f), "L")
            manifest["noises"] = [f[:-4] for f in sorted(os.listdir(os.path.join(folder, "1k")))]
            continue
        for res in ("2k", "1k"):
            for f in sorted(os.listdir(os.path.join(folder, res))):
                save(os.path.join(folder, res, f), os.path.join(DEST, name, res, f), "RGB")
        graph = os.path.join(folder, f"{name}.graph.json")
        if os.path.isfile(graph):
            shutil.copy2(graph, os.path.join(GRAPHS, f"{name}.graph.json"))
        manifest["materials"].append(name)
    with open(os.path.join(DEST, "pack.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    total = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(DEST) for f in fs)
    print(f"{len(manifest['materials'])} materials, {len(manifest['noises'])} noises, {total / 1e6:.1f} MB -> {DEST}")


if __name__ == "__main__":
    main()
