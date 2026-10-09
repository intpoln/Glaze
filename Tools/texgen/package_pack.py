"""Copies the rendered pack (TexturePack/raw) into the plugin: Plugins/Glaze/Resources/Pack.

- Materials: <Name>/<2k|1k>/T_<Name>_<BC|N|ORM>.png (BC stays RGB, N and ORM are data).
- Noises: Noises/<2k|1k>/T_Noise_<Name>.png, stored as 8-bit grayscale.
- Graph JSONs go to TexturePack/graphs (editable sources, not shipped).
- Writes Pack/pack.json (manifest read by the plugin).
- Inputs are validated first (files, sizes, matching noise sets); the pack is built next to the old one and swapped in
  only on success, so a failed run leaves the existing pack untouched.

Run: python Tools/texgen/package_pack.py [--check]   (--check only validates, writes nothing)
"""
import json
import os
import shutil
import sys

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RAW = os.path.join(ROOT, "TexturePack", "raw")
GRAPHS = os.path.join(ROOT, "TexturePack", "graphs")
DEST = os.path.join(ROOT, "GlazeLab", "Plugins", "Glaze", "Resources", "Pack")
SIZES = {"2k": 2048, "1k": 1024}
MAPS = ("BC", "N", "ORM")


def check_png(path, res, problems):
    rel = os.path.relpath(path, RAW)
    if not os.path.isfile(path):
        problems.append(f"missing: {rel}")
        return
    try:
        with Image.open(path) as im:
            im.load()  # catches truncated/corrupt files
            size = im.size
    except Exception as e:
        problems.append(f"unreadable: {rel} ({e})")
        return
    if size != (SIZES[res], SIZES[res]):
        problems.append(f"wrong size {size[0]}x{size[1]} (want {SIZES[res]}): {rel}")


def validate():
    """Returns (materials, noises, problems)."""
    problems = []
    if not os.path.isdir(RAW):
        return [], [], [f"raw directory not found: {RAW}"]
    materials, noises = [], []
    for name in sorted(os.listdir(RAW)):
        folder = os.path.join(RAW, name)
        if not os.path.isdir(folder) or name == "test":
            continue
        if name == "Noises":
            sets = {}
            for res in SIZES:
                d = os.path.join(folder, res)
                sets[res] = sorted(f for f in os.listdir(d) if f.lower().endswith(".png")) if os.path.isdir(d) else []
                if not sets[res]:
                    problems.append(f"Noises/{res}: no PNGs")
                for f in sets[res]:
                    check_png(os.path.join(d, f), res, problems)
            if sets["1k"] != sets["2k"]:
                diff = sorted(set(sets["1k"]) ^ set(sets["2k"]))
                problems.append(f"Noises: 1k and 2k sets differ: {', '.join(diff)}")
            noises = [f[:-4] for f in sets["1k"]]
            continue
        for res in SIZES:
            for m in MAPS:
                check_png(os.path.join(folder, res, f"T_{name}_{m}.png"), res, problems)
        materials.append(name)
    if not materials:
        problems.append("no materials found in raw")
    return materials, noises, problems


def save(src, dst, mode):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with Image.open(src) as im:
        im.convert(mode).save(dst, optimize=True)


def build(tmp, materials, noises):
    for res in SIZES:
        for f in noises:
            save(os.path.join(RAW, "Noises", res, f + ".png"), os.path.join(tmp, "Noises", res, f + ".png"), "L")
    for name in materials:
        for res in SIZES:
            for m in MAPS:
                f = f"T_{name}_{m}.png"
                save(os.path.join(RAW, name, res, f), os.path.join(tmp, name, res, f), "RGB")
    with open(os.path.join(tmp, "pack.json"), "w", encoding="utf-8") as f:
        json.dump({"materials": materials, "noises": noises}, f, indent=2)


def main():
    # A previous run was interrupted between the two renames: bring the old pack back first.
    backup = DEST + ".old"
    if not os.path.isdir(DEST) and os.path.isdir(backup):
        os.rename(backup, DEST)
        print(f"restored the previous pack from {backup}")
    materials, noises, problems = validate()
    if problems:
        print(f"pack NOT built, {len(problems)} problem(s); existing pack left untouched:")
        for p in problems:
            print("  - " + p)
        sys.exit(1)
    if "--check" in sys.argv[1:]:
        print(f"check OK: {len(materials)} materials, {len(noises)} noises")
        return
    tmp = DEST + ".new"
    if os.path.isdir(tmp):
        shutil.rmtree(tmp)
    # DEST exists here (restored above or never missing), so a stale .old is safe to drop.
    if os.path.isdir(DEST) and os.path.isdir(backup):
        shutil.rmtree(backup)
    try:
        build(tmp, materials, noises)
    except Exception:
        shutil.rmtree(tmp, ignore_errors=True)
        raise
    if os.path.isdir(DEST):
        os.rename(DEST, backup)
    try:
        os.rename(tmp, DEST)
    except Exception:
        if os.path.isdir(backup):
            os.rename(backup, DEST)
        raise
    shutil.rmtree(backup, ignore_errors=True)
    os.makedirs(GRAPHS, exist_ok=True)
    for name in materials:
        graph = os.path.join(RAW, name, f"{name}.graph.json")
        if os.path.isfile(graph):
            shutil.copy2(graph, os.path.join(GRAPHS, f"{name}.graph.json"))
    total = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(DEST) for f in fs)
    print(f"{len(materials)} materials, {len(noises)} noises, {total / 1e6:.1f} MB -> {DEST}")


if __name__ == "__main__":
    main()
