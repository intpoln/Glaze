"""Builds the Fab listing images from the raw editor captures in fab/raw (see fab/FAB_LISTING.md)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
RAW, OUT = ROOT / 'fab/raw', ROOT / 'fab/images'
PACK = ROOT / 'GlazeLab/Plugins/Glaze/Resources/Pack'
FONTS = Path('C:/Windows/Fonts')
BG, PANEL, TEXT, DIM, ACCENT = (24, 26, 30), (36, 39, 45), (240, 240, 240), (160, 165, 175), (88, 166, 255)


def font(size, bold=False):
    return ImageFont.truetype(str(FONTS / ('seguisb.ttf' if bold else 'segoeui.ttf')), size)


def fit(im, w, h):
    """Scale and center-crop to w x h."""
    s = max(w / im.width, h / im.height)
    im = im.resize((round(im.width * s), round(im.height * s)), Image.LANCZOS)
    x, y = (im.width - w) // 2, (im.height - h) // 2
    return im.crop((x, y, x + w, y + h))


def rounded(im, r=18):
    mask = Image.new('L', im.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, im.width - 1, im.height - 1), r, fill=255)
    out = Image.new('RGBA', im.size)
    out.paste(im.convert('RGB'), (0, 0), mask)
    return out


def paste_card(canvas, im, x, y):
    shadow = Image.new('RGBA', (im.width + 40, im.height + 40), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle((20, 24, im.width + 20, im.height + 24), 18, fill=(0, 0, 0, 140))
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(12)), (x - 20, y - 20))
    canvas.alpha_composite(rounded(im), (x, y))


def arrow(d, x, y):
    d.polygon([(x, y - 22), (x + 34, y), (x, y + 22)], fill=ACCENT)


def caption(d, x, y, step, title, sub, w):
    d.text((x, y), step, font=font(26, True), fill=ACCENT)
    d.text((x + 44, y), title, font=font(26, True), fill=TEXT)
    d.multiline_text((x, y + 40), sub, font=font(20), fill=DIM, spacing=6)


def header(d, title, sub):
    d.text((80, 50), title, font=font(54, True), fill=TEXT)
    d.text((82, 122), sub, font=font(26), fill=DIM)


def workflow():
    c = Image.new('RGBA', (1920, 1080), BG)
    d = ImageDraw.Draw(c)
    header(d, 'Texture set  →  material in one window',
           'Glaze builds a Material Instance from your textures: no node graphs, no master material to write.')
    top, h = 210, 640
    # 1. Textures
    x1, w1 = 80, 380
    card = Image.new('RGB', (w1, h), PANEL)
    cd = ImageDraw.Draw(card)
    for i, (suffix, label) in enumerate((('BC', 'T_Rock_Cliff_BC'), ('N', 'T_Rock_Cliff_N'), ('ORM', 'T_Rock_Cliff_ORM'))):
        t = Image.open(PACK / 'Rock_Cliff/1k' / f'T_Rock_Cliff_{suffix}.png').convert('RGB').resize((170, 170), Image.LANCZOS)
        y = 30 + i * 205
        card.paste(t, (24, y))
        cd.text((210, y + 60), label.replace('T_Rock_Cliff_', ''), font=font(30, True), fill=TEXT)
        cd.text((210, y + 100), label, font=font(17), fill=DIM)
    paste_card(c, card, x1, top)
    # 2. Window
    win = Image.open(RAW / 'glaze_window.png').convert('RGB')
    w2 = round(win.width * h / win.height)
    x2 = x1 + w1 + 90
    paste_card(c, win.resize((w2, h), Image.LANCZOS), x2, top)
    # 3. Result on the mesh
    x3 = x2 + w2 + 90
    w3 = 1920 - 80 - x3
    paste_card(c, fit(Image.open(RAW / 'live_preview.png').convert('RGB'), w3, h), x3, top)
    for xa in (x1 + w1 + 28, x2 + w2 + 28):
        arrow(d, xa, top + h // 2)
    cy = top + h + 34
    caption(d, x1, cy, '1', 'Select textures', 'Right-click → Glaze: Create Material.\nMaps are matched by name.', w1)
    caption(d, x2, cy, '2', 'Tune', 'Color, roughness, detail noise,\ntriplanar, parallax, wind.', w2)
    caption(d, x3, cy, '3', 'See it live, then Create', 'The selected mesh previews every change.\nThe result works without the plugin.', w3)
    c.convert('RGB').save(OUT / '02_workflow.png')


def looks():
    c = Image.new('RGBA', (1920, 1080), BG)
    d = ImageDraw.Draw(c)
    header(d, 'One texture set, many materials',
           'Color correction, parallax, detail noise and macro variation are switches and sliders in the same window.')
    s, gap, top = 400, 40, 210
    x0 = (1920 - 4 * s - 3 * gap) // 2
    for i, label in enumerate(('Original', 'Hue Shift', 'Desaturate + Contrast', 'Lerp Color')):
        x = x0 + i * (s + gap)
        paste_card(c, fit(Image.open(RAW / f'cc_{i}.png').convert('RGB'), s, s), x, top)
        d.text((x + s // 2, top + s + 16), label, font=font(22, True), fill=TEXT, anchor='mt')
    w, h, top2 = (4 * s + 3 * gap - gap) // 2, 300, top + s + 80
    for i, label in enumerate(('Parallax from the AO channel', '+ detail noise (moss) + macro variation')):
        x = x0 + i * (w + gap)
        paste_card(c, fit(Image.open(RAW / f'detail_{i}.png').convert('RGB'), w, h), x, top2)
        d.text((x + 20, top2 + h - 50), label, font=font(24, True), fill=TEXT, stroke_width=3, stroke_fill=(0, 0, 0))
    c.convert('RGB').save(OUT / '03_one_set_many_looks.png')


def cutout(path, size):
    """The sphere of a studio shot on a transparent background (studio floor is near white)."""
    im = fit(Image.open(path).convert('RGB'), 960, 960).crop((150, 150, 810, 810)).resize((size, size), Image.LANCZOS)
    mask = Image.new('L', im.size, 0)
    r = size * 0.433
    cx, cy = size * 0.5, size * 0.5 - size * 0.005
    ImageDraw.Draw(mask).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    out = Image.new('RGBA', im.size)
    out.paste(im, (0, 0), mask.filter(ImageFilter.GaussianBlur(1.2)))
    return out


def thumbnail():
    W, H = 1920, 1080
    c = Image.new('RGBA', (W, H), BG)
    glow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((900, 140, 1900, 1040), fill=(88, 166, 255, 70))
    c.alpha_composite(glow.filter(ImageFilter.GaussianBlur(160)))
    d = ImageDraw.Draw(c)
    d.text((110, 300), 'Glaze', font=font(200, True), fill=TEXT)
    d.text((118, 540), 'Texture set  →  Material Instance', font=font(52, True), fill=ACCENT)
    d.text((118, 620), 'One window. Live preview. No node graphs.', font=font(40), fill=DIM)
    d.text((118, 690), 'Unreal Engine 5.7 / 5.8  ·  18 materials + 21 noises included', font=font(30), fill=DIM)
    for (x, y, s, i) in ((1300, 110, 460, 0), (1530, 450, 380, 1), (1110, 560, 360, 3)):
        sp = cutout(RAW / f'cc_{i}.png', s)
        sh = Image.new('RGBA', (s, s), (0, 0, 0, 0))
        ImageDraw.Draw(sh).ellipse((s * 0.12, s * 0.12, s * 0.88, s * 0.88), fill=(0, 0, 0, 150))
        c.alpha_composite(sh.filter(ImageFilter.GaussianBlur(28)), (x + 18, y + 30))
        c.alpha_composite(sp, (x, y))
    c.convert('RGB').save(OUT / '01_thumbnail.png')


if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    thumbnail()
    workflow()
    looks()
    print('ok')
