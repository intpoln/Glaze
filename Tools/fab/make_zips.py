"""Fab upload zips: packages/Glaze_<v>_source.zip = Glaze/ with .uplugin, Source, Resources (no Binaries/Intermediate).

Run after Tools/package.sh 5.7 and 5.8:  python Tools/fab/make_zips.py
"""
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKIP = {"Binaries", "Intermediate", "UnrealBuildTool"}

for v in ("5.7", "5.8"):
    src = ROOT / "packages" / v / "Glaze"
    out = ROOT / "packages" / f"Glaze_{v}_source.zip"
    if not (src / "Glaze.uplugin").is_file():
        raise SystemExit(f"no package: {src} (run Tools/package.sh {v})")
    n = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(src.rglob("*")):
            r = p.relative_to(src)
            if p.is_file() and r.parts[0] not in SKIP:
                z.write(p, Path("Glaze") / r)
                n += 1
    print(f"{out}  {n} files")
