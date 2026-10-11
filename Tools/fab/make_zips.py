"""Fab upload zips: packages/Glaze_<v>_source.zip = Glaze/ with .uplugin, Source, Resources (no Binaries/Intermediate).

Run after Tools/package.sh 5.7 and 5.8:  python Tools/fab/make_zips.py
RunUAT BuildPlugin drops FabURL from the packaged .uplugin, so it is restored here from the source .uplugin.
"""
import json
import re
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKIP = {"Binaries", "Intermediate", "UnrealBuildTool"}
FAB_URL = json.loads((ROOT / "GlazeLab/Plugins/Glaze/Glaze.uplugin").read_text(encoding="utf-8")).get("FabURL")


def restore_fab_url(uplugin: Path):
    with open(uplugin, encoding="utf-8", newline="") as f:
        text = f.read()
    if not FAB_URL or '"FabURL"' in text:
        return
    eol = "\r\n" if "\r\n" in text else "\n"
    text, n = re.subn(r'^([ \t]*)("DocsURL": [^\r\n]*,)', lambda m: f'{m[1]}{m[2]}{eol}{m[1]}"FabURL": "{FAB_URL}",', text, count=1, flags=re.M)
    if not n:
        raise SystemExit(f"no DocsURL line to insert FabURL after: {uplugin}")
    with open(uplugin, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    json.loads(text)
    print(f"{uplugin}  FabURL restored")


for v in ("5.7", "5.8"):
    src = ROOT / "packages" / v / "Glaze"
    out = ROOT / "packages" / f"Glaze_{v}_source.zip"
    if not (src / "Glaze.uplugin").is_file():
        raise SystemExit(f"no package: {src} (run Tools/package.sh {v})")
    restore_fab_url(src / "Glaze.uplugin")
    n = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(src.rglob("*")):
            r = p.relative_to(src)
            if p.is_file() and r.parts[0] not in SKIP:
                z.write(p, Path("Glaze") / r)
                n += 1
    print(f"{out}  {n} files")
