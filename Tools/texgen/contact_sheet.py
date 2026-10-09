"""Contact sheet of the generated pack: one row per material (BC 2x2 tiled, N, ORM), for a quick visual check.
Noises get their own labeled grid below the material rows (they do not widen the sheet).

Run: python Tools/texgen/contact_sheet.py <raw_dir> <out.png> [name filter...]
"""
import os
import sys

from PIL import Image, ImageDraw

RAW = sys.argv[1]
OUT = sys.argv[2]
FILTER = sys.argv[3:]
CELL = 256
LABEL = 18
MAT_COLS = 3
NOISE_COLS = 6
BG = (30, 30, 30)
FG = (230, 230, 230)


def thumb(path, tile=False):
    with Image.open(path) as src:
        if tile:
            im = src.convert("RGB").resize((CELL // 2, CELL // 2), Image.LANCZOS)
            t = Image.new("RGB", (CELL, CELL))
            for x in (0, CELL // 2):
                for y in (0, CELL // 2):
                    t.paste(im, (x, y))
            im.close()
            return t
        return src.convert("RGB").resize((CELL, CELL), Image.LANCZOS)


def paste(sheet, path, pos, tile=False):
    t = thumb(path, tile)
    sheet.paste(t, pos)
    t.close()


names = sorted(n for n in os.listdir(RAW) if os.path.isdir(os.path.join(RAW, n)) and n not in ("test", "Noises"))
if FILTER:
    names = [n for n in names if any(f.lower() in n.lower() for f in FILTER)]
mats = []  # (name, folder, files)
for name in names:
    folder = os.path.join(RAW, name, "1k")
    if os.path.isdir(folder):
        files = sorted(os.listdir(folder))
        mats.append((name, folder, [f for f in files if f.endswith("_BC.png")] + [f for f in files if not f.endswith("_BC.png")]))

noise_dir = os.path.join(RAW, "Noises", "1k")
noises = []
if os.path.isdir(noise_dir) and (not FILTER or any(f.lower() in "noises" for f in FILTER)):
    noises = sorted(f for f in os.listdir(noise_dir) if f.lower().endswith(".png"))

mat_rows = len(mats)
noise_rows = (len(noises) + NOISE_COLS - 1) // NOISE_COLS
width = max(MAT_COLS if mats else 0, NOISE_COLS if noises else 0, 1) * CELL
mat_h = mat_rows * (CELL + LABEL)
height = max(mat_h + noise_rows * (CELL + LABEL) + (LABEL if noises else 0), 1)
sheet = Image.new("RGB", (width, height), BG)
draw = ImageDraw.Draw(sheet)

for r, (name, folder, files) in enumerate(mats):
    y = r * (CELL + LABEL)
    draw.text((4, y + 3), name, fill=FG)
    for c, f in enumerate(files[:MAT_COLS]):
        paste(sheet, os.path.join(folder, f), (c * CELL, y + LABEL), tile=f.endswith("_BC.png"))

if noises:
    top = mat_h
    draw.text((4, top + 3), "Noises", fill=FG)
    top += LABEL
    for i, f in enumerate(noises):
        x, y = (i % NOISE_COLS) * CELL, top + (i // NOISE_COLS) * (CELL + LABEL)
        draw.text((x + 4, y + 3), f[:-4].replace("T_Noise_", ""), fill=FG)
        paste(sheet, os.path.join(noise_dir, f), (x, y + LABEL))

sheet.save(OUT)
print(OUT, sheet.size)
