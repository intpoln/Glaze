"""Contact sheet of the generated pack: one row per material (BC 2x2 tiled, N, ORM), for a quick visual check.

Run: python Tools/texgen/contact_sheet.py <raw_dir> <out.png> [name filter...]
"""
import os
import sys

from PIL import Image, ImageDraw

RAW = sys.argv[1]
OUT = sys.argv[2]
FILTER = sys.argv[3:]
CELL = 256


def tiled(path):
    im = Image.open(path).convert("RGB").resize((CELL // 2, CELL // 2), Image.LANCZOS)
    t = Image.new("RGB", (CELL, CELL))
    for x in (0, CELL // 2):
        for y in (0, CELL // 2):
            t.paste(im, (x, y))
    return t


names = sorted(n for n in os.listdir(RAW) if os.path.isdir(os.path.join(RAW, n)) and n != "test")
if FILTER:
    names = [n for n in names if any(f.lower() in n.lower() for f in FILTER)]
rows = []
for name in names:
    folder = os.path.join(RAW, name, "1k")
    if not os.path.isdir(folder):
        continue
    files = sorted(os.listdir(folder))
    bc = [f for f in files if f.endswith("_BC.png")]
    others = [f for f in files if not f.endswith("_BC.png")]
    cells = [tiled(os.path.join(folder, f)) for f in bc] + [
        Image.open(os.path.join(folder, f)).convert("RGB").resize((CELL, CELL), Image.LANCZOS) for f in others]
    rows.append((name, cells))

cols = max((len(c) for _, c in rows), default=1)
sheet = Image.new("RGB", (cols * CELL, len(rows) * (CELL + 18)), (30, 30, 30))
draw = ImageDraw.Draw(sheet)
for r, (name, cells) in enumerate(rows):
    y = r * (CELL + 18)
    draw.text((4, y + 3), name, fill=(230, 230, 230))
    for c, cell in enumerate(cells):
        sheet.paste(cell, (c * CELL, y + 18))
sheet.save(OUT)
print(OUT, sheet.size)
