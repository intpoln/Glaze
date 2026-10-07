"""Original Glaze vector artwork and offline SVG-to-PNG rendering.

Only writes beside this script. Requires the already installed Pillow,
NumPy and Matplotlib. The small renderer supports the SVG subset used here.
"""
from pathlib import Path
import os
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
# Keep renderer caches beside the deliverables, including on repeat runs.
os.environ['MPLCONFIGDIR'] = str(ROOT / '.render-cache')

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from matplotlib.figure import Figure
from matplotlib.backends.backend_agg import FigureCanvasAgg
from matplotlib.path import Path as MPath
from matplotlib.patches import PathPatch, Ellipse, FancyBboxPatch

INK = '#1E1E24'
DEFS = '''<defs>
  <linearGradient id="glaze" gradientUnits="userSpaceOnUse" x1="30" y1="20" x2="100" y2="110">
    <stop offset="0" stop-color="#6EF0E0"/><stop offset="0.48" stop-color="#19C3C3"/><stop offset="1" stop-color="#087C88"/>
  </linearGradient>
  <linearGradient id="normal" gradientUnits="userSpaceOnUse" x1="62" y1="40" x2="110" y2="106">
    <stop offset="0" stop-color="#A8ABFF"/><stop offset="0.5" stop-color="#727AE8"/><stop offset="1" stop-color="#3E52A0"/>
  </linearGradient>
  <linearGradient id="rough" gradientUnits="userSpaceOnUse" x1="22" y1="60" x2="80" y2="114">
    <stop offset="0" stop-color="#CCD3D5"/><stop offset="1" stop-color="#727E88"/>
  </linearGradient>
  <linearGradient id="warm" gradientUnits="userSpaceOnUse" x1="28" y1="28" x2="94" y2="96">
    <stop offset="0" stop-color="#FFB16C"/><stop offset="1" stop-color="#FF8A3D"/>
  </linearGradient>
  <linearGradient id="ceramic" gradientUnits="userSpaceOnUse" x1="25" y1="60" x2="98" y2="115">
    <stop offset="0" stop-color="#FFE3B4"/><stop offset="0.6" stop-color="#FFAF67"/><stop offset="1" stop-color="#D96B31"/>
  </linearGradient>
</defs>'''

ART = {
'A': '''
<defs><clipPath id="ball"><circle cx="64" cy="65" r="49"/></clipPath></defs>
<circle cx="64" cy="65" r="49" fill="url(#rough)"/>
<g clip-path="url(#ball)">
  <path d="M73 16 C112 27 125 76 103 104 C88 120 69 119 58 109 C77 84 81 54 73 16 Z" fill="url(#normal)"/>
  <path d="M75 57 C80 78 72 101 59 114" fill="none" stroke="#1E1E24" stroke-width="6"/>
  <path d="M11 14 L115 14 L116 48 C101 49 103 62 94 63 C82 65 88 46 78 47 C67 47 73 79 61 80 C48 81 54 52 43 52 C32 53 33 64 21 61 L10 58 Z" fill="url(#glaze)" stroke="#1E1E24" stroke-width="6" stroke-linejoin="round"/>
  <path d="M31 70 L36 75 L31 80 L26 75 Z" fill="#ECF0F1"/>
  <path d="M41 86 L46 91 L41 96 L36 91 Z" fill="#A5AFB6"/>
  <path d="M86 77 L97 70 L103 82 L92 89 Z" fill="#A2A9FF"/>
  <path d="M92 89 L103 82 L102 95 L91 102 Z" fill="#5964C7"/>
</g>
<circle cx="64" cy="65" r="49" fill="none" stroke="#1E1E24" stroke-width="7"/>
<path d="M32 39 C39 30 49 27 59 27" fill="none" stroke="#EEFFF8" stroke-width="9" stroke-linecap="round"/>
<path d="M67 28 L71 29" fill="none" stroke="#C8FFF3" stroke-width="6" stroke-linecap="round"/>
<path d="M61 58 L61 68" fill="none" stroke="#A7FFF0" stroke-width="4" stroke-linecap="round"/>
<path d="M102 16 L106 25 L115 29 L106 33 L102 42 L98 33 L89 29 L98 25 Z" fill="#FF8A3D" stroke="#1E1E24" stroke-width="4" stroke-linejoin="round"/>
''',
'B': '''
<defs><clipPath id="drop"><path d="M64 11 C55 26 24 52 21 76 C17 99 36 117 64 117 C92 117 111 99 107 76 C104 52 73 26 64 11 Z"/></clipPath></defs>
<path d="M64 11 C55 26 24 52 21 76 C17 99 36 117 64 117 C92 117 111 99 107 76 C104 52 73 26 64 11 Z" fill="url(#glaze)"/>
<g clip-path="url(#drop)">
  <path d="M31 75 L73 64 Q77 63 78 68 L84 91 L43 103 Q39 104 38 100 Z" fill="url(#rough)" stroke="#1E1E24" stroke-width="5" stroke-linejoin="round"/>
  <path d="M29 58 L71 47 Q75 46 76 51 L82 75 L41 87 Q37 88 36 83 Z" fill="url(#normal)" stroke="#1E1E24" stroke-width="5" stroke-linejoin="round"/>
  <path d="M45 60 L58 57 L62 69 L49 72 Z" fill="#ABAFFF"/>
  <path d="M30 43 L68 33 Q73 31 75 37 L80 58 L42 69 Q37 70 35 64 Z" fill="url(#warm)" stroke="#1E1E24" stroke-width="5" stroke-linejoin="round"/>
  <path d="M34 44 L52 54 L76 36" fill="none" stroke="#FFD3A2" stroke-width="5" stroke-linejoin="round"/>
  <path d="M82 33 C75 49 65 67 66 82 C67 95 61 103 49 116 L115 119 L115 45 Z" fill="url(#glaze)"/>
  <path d="M82 36 C77 52 66 67 66 82 C67 95 61 103 54 111" fill="none" stroke="#1E1E24" stroke-width="5" stroke-linecap="round"/>
</g>
<path d="M64 11 C55 26 24 52 21 76 C17 99 36 117 64 117 C92 117 111 99 107 76 C104 52 73 26 64 11 Z" fill="none" stroke="#1E1E24" stroke-width="7" stroke-linejoin="round"/>
<path d="M87 64 C94 76 96 86 89 94" fill="none" stroke="#EBFFF7" stroke-width="9" stroke-linecap="round"/>
''',
'C': '''
<rect x="31" y="12" width="39" height="39" rx="7" fill="url(#rough)" stroke="#1E1E24" stroke-width="6"/>
<path d="M43 24 L59 24 L59 39 L43 39 Z" fill="#E4EAEB"/>
<rect x="21" y="23" width="39" height="39" rx="7" fill="url(#normal)" stroke="#1E1E24" stroke-width="6"/>
<path d="M32 34 L46 34 L49 47 L35 47 Z" fill="#B6B7FF"/>
<rect x="11" y="34" width="39" height="39" rx="7" fill="url(#glaze)" stroke="#1E1E24" stroke-width="6"/>
<path d="M21 57 L29 46 L40 59" fill="none" stroke="#A7FFF0" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M82 20 C103 23 112 40 109 56 L118 56 L102 74 L88 55 L97 55 C99 45 94 35 81 33 Z" fill="url(#warm)" stroke="#1E1E24" stroke-width="5" stroke-linejoin="round"/>
<circle cx="76" cy="85" r="31" fill="url(#glaze)" stroke="#1E1E24" stroke-width="7"/>
<path d="M57 77 C62 69 69 65 76 65" fill="none" stroke="#EEFFF8" stroke-width="8" stroke-linecap="round"/>
<path d="M83 105 C90 103 96 98 98 91" fill="none" stroke="#0D929A" stroke-width="5" stroke-linecap="round"/>
''',
'D': '''
<path d="M49 104 L79 104 L85 116 L43 116 Z" fill="url(#ceramic)" stroke="#1E1E24" stroke-width="6" stroke-linejoin="round"/>
<path d="M18 59 C20 85 36 108 64 109 C92 108 108 85 110 59 Z" fill="url(#ceramic)" stroke="#1E1E24" stroke-width="7" stroke-linejoin="round"/>
<path d="M20 61 C31 66 35 61 41 66 C47 71 40 86 49 87 C59 88 54 66 65 66 C78 66 67 94 79 94 C89 94 84 71 94 68 C100 67 107 63 109 60 L105 51 L25 51 Z" fill="url(#glaze)" stroke="#1E1E24" stroke-width="5" stroke-linejoin="round"/>
<ellipse cx="64" cy="55" rx="46" ry="14" fill="#086C77" stroke="#1E1E24" stroke-width="6"/>
<path d="M31 54 C47 47 83 47 98 54" fill="none" stroke="#6EF0E0" stroke-width="5" stroke-linecap="round"/>
<path d="M73 13 L92 13 C89 25 78 34 74 45 C72 50 73 56 68 59 C63 62 57 59 57 53 C57 41 68 27 73 13 Z" fill="url(#glaze)" stroke="#1E1E24" stroke-width="6" stroke-linejoin="round"/>
<path d="M78 21 C76 30 67 38 65 46" fill="none" stroke="#ECFFF8" stroke-width="5" stroke-linecap="round"/>
<path d="M30 72 C33 81 37 86 42 89" fill="none" stroke="#ECFFF8" stroke-width="7" stroke-linecap="round"/>
<path d="M98 16 L101 22 L107 25 L101 28 L98 34 L95 28 L89 25 L95 22 Z" fill="#FF8A3D" stroke="#1E1E24" stroke-width="4" stroke-linejoin="round"/>
'''
}


def svg_document(letter):
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">\n'
            + DEFS + ART[letter] + '\n</svg>\n')


def parse_path(d):
    tokens = re.findall(r'[MLCQZmlcqz]|[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?', d)
    verts, codes = [], []
    cursor = (0, 0)
    start = cursor
    i = 0
    command = None
    while i < len(tokens):
        if tokens[i].isalpha():
            command = tokens[i]
            i += 1
        upper = command.upper()
        if upper == 'Z':
            verts.append(start)
            codes.append(MPath.CLOSEPOLY)
            cursor = start
            command = None
            continue
        count = {'M': 2, 'L': 2, 'C': 6, 'Q': 4}[upper]
        nums = [float(x) for x in tokens[i:i+count]]
        i += count
        points = list(zip(nums[::2], nums[1::2]))
        if command.islower():
            points = [(x+cursor[0], y+cursor[1]) for x, y in points]
        code = {'M': MPath.MOVETO, 'L': MPath.LINETO, 'C': MPath.CURVE4, 'Q': MPath.CURVE3}[upper]
        verts.extend(points)
        codes.extend([code]*len(points))
        cursor = points[-1]
        if upper == 'M':
            start = cursor
            command = 'L' if command.isupper() else 'l'
    return MPath(verts, codes)


def shape_patch(el, **kwargs):
    tag = el.tag.rsplit('}', 1)[-1]
    n = lambda name, default=0: float(el.get(name, default))
    if tag == 'path':
        return PathPatch(parse_path(el.get('d')), **kwargs)
    if tag == 'circle':
        return Ellipse((n('cx'), n('cy')), 2*n('r'), 2*n('r'), **kwargs)
    if tag == 'ellipse':
        return Ellipse((n('cx'), n('cy')), 2*n('rx'), 2*n('ry'), **kwargs)
    if tag == 'rect':
        radius = n('rx')
        return FancyBboxPatch((n('x'), n('y')), n('width'), n('height'),
                             boxstyle=f'round,pad=0,rounding_size={radius}', **kwargs)
    raise ValueError(tag)


def mask_for(el, size, stroke_width=0):
    # 72 dpi makes one figure point one output pixel. Scale SVG units explicitly.
    fig = Figure(figsize=(size/72, size/72), dpi=72, facecolor=(0, 0, 0, 0))
    canvas = FigureCanvasAgg(fig)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 128)
    ax.set_ylim(128, 0)
    ax.set_axis_off()
    ax.patch.set_alpha(0)
    patch = shape_patch(el, facecolor='none' if stroke_width else 'white',
                        edgecolor='white' if stroke_width else 'none',
                        linewidth=stroke_width*size/128,
                        joinstyle='round', capstyle='round', antialiased=True)
    ax.add_patch(patch)
    canvas.draw()
    result = Image.fromarray(np.asarray(canvas.buffer_rgba())[:, :, 3].copy())
    fig.clear()
    return result


def rgb(hexcolor):
    return np.array([int(hexcolor[j:j+2], 16) for j in (1, 3, 5)], dtype=float)


def gradient_image(el, size):
    n = lambda key: float(el.get(key))
    ys, xs = np.mgrid[0:size, 0:size].astype(float)
    xs = (xs+.5)*128/size
    ys = (ys+.5)*128/size
    dx, dy = n('x2')-n('x1'), n('y2')-n('y1')
    t = np.clip(((xs-n('x1'))*dx + (ys-n('y1'))*dy)/(dx*dx+dy*dy), 0, 1)
    stops = [(float(s.get('offset')), rgb(s.get('stop-color'))) for s in el]
    offsets = [s[0] for s in stops]
    arr = np.stack([np.interp(t, offsets, [s[1][ch] for s in stops]) for ch in range(3)], axis=2)
    return Image.fromarray(np.rint(arr).astype(np.uint8), 'RGB').convert('RGBA')


def render_svg(svg_text, size=768):
    root = ET.fromstring(svg_text)
    ids = {el.get('id'): el for el in root.iter() if el.get('id')}
    gradients = {key: gradient_image(el, size) for key, el in ids.items()
                 if el.tag.endswith('linearGradient')}
    canvas = Image.new('RGBA', (size, size))
    clip_masks = {}

    def walk(el, parent_style=None, inherited_clip=None):
        style = dict(parent_style or {})
        style.update(el.attrib)
        tag = el.tag.rsplit('}', 1)[-1]
        if tag == 'defs':
            return
        clip = inherited_clip
        if 'clip-path' in el.attrib:
            key = re.search(r'#([^)]*)', el.get('clip-path')).group(1)
            if key not in clip_masks:
                clip_masks[key] = mask_for(ids[key][0], size)
            clip = clip_masks[key]
        if tag in ('svg', 'g'):
            for child in el:
                walk(child, style, clip)
            return
        if tag not in ('path', 'circle', 'ellipse', 'rect'):
            return
        for paint, stroke_width in ((style.get('fill', '#000000'), 0),
                                    (style.get('stroke', 'none'), float(style.get('stroke-width', 1)))):
            if paint == 'none':
                continue
            mask = mask_for(el, size, stroke_width)
            if clip is not None:
                mask = Image.fromarray(((np.asarray(mask, dtype=np.uint16)*np.asarray(clip, dtype=np.uint16)+127)//255).astype(np.uint8))
            if paint.startswith('url('):
                layer = gradients[re.search(r'#([^)]*)', paint).group(1)].copy()
            else:
                layer = Image.new('RGBA', canvas.size, paint)
            layer.putalpha(mask)
            canvas.alpha_composite(layer)
    walk(root)
    return canvas


def font(size, bold=False):
    candidates = [('C:/Windows/Fonts/segoeuib.ttf' if bold else 'C:/Windows/Fonts/segoeui.ttf'),
                  ('C:/Windows/Fonts/arialbd.ttf' if bold else 'C:/Windows/Fonts/arial.ttf')]
    for candidate in candidates:
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def preview():
    sheet = Image.new('RGB', (1040, 980), '#171B21')
    draw = ImageDraw.Draw(sheet)
    draw.text((30, 22), 'GLAZE / FOUR ORIGINAL CONCEPTS', fill='#E7F5F3', font=font(27, True))
    draw.text((30, 62), 'Transparent PNGs on #2A2A2A and #BFBFBF • shown at actual pixel size', fill='#A9BABD', font=font(17))
    cols = [(142, '128 px / dark', '#2A2A2A', 128),
            (354, '128 px / light', '#BFBFBF', 128),
            (566, '32 px / dark', '#2A2A2A', 32),
            (778, '32 px / light', '#BFBFBF', 32)]
    for x, label, _, _ in cols:
        draw.text((x, 108), label, fill='#C6D1D2', font=font(17, True))
    labels = {'A': 'Material ball', 'B': 'Texture droplet', 'C': 'Build material', 'D': 'Ceramic glaze'}
    for row, letter in enumerate(ART):
        y = 145+row*203
        draw.text((32, y+52), letter, fill='#6EF0E0', font=font(40, True))
        draw.text((142, y+172), labels[letter], fill='#98ADB1', font=font(15))
        for x, _, bg, size in cols:
            draw.rounded_rectangle((x, y, x+180, y+158), radius=12, fill=bg)
            icon = Image.open(ROOT/f'glaze_{letter}_{size}.png').convert('RGBA')
            sheet.paste(icon, (x+(180-size)//2, y+(158-size)//2), icon)
    draw.text((30, 954), 'Teal glaze • orange Kiln accent • charcoal contour', fill='#98ADB1', font=font(15))
    sheet.save(ROOT/'preview.png')


def main():
    for letter in ART:
        source = svg_document(letter)
        (ROOT/f'glaze_{letter}.svg').write_text(source, encoding='utf-8')
        large = render_svg(source)
        for size in (128, 32):
            pixels = np.array(large.resize((size, size), Image.Resampling.LANCZOS))
            # Remove only near-invisible Lanczos ringing outside the silhouette.
            pixels[pixels[:, :, 3] < 6] = 0
            Image.fromarray(pixels).save(ROOT/f'glaze_{letter}_{size}.png')
    preview()
    (ROOT/'notes.md').write_text(
        'A — Шар предпросмотра: бирюзовая глазурь стекает на серую roughness-зону и сине-фиолетовую normal-зону; оранжевая искра подчёркивает блеск.\n'
        'B — Капля объединяет стопку цветовой, normal и roughness-карт в единую глянцевую поверхность.\n'
        'C — Три текстурные плитки и крупная оранжевая стрелка ведут к готовому бирюзовому шару материала.\n'
        'D — Керамическая чаша с льющейся бирюзовой глазурью, крупными потёками и ярким бликом связывает Glaze с Kiln.\n'
        '\nРекомендую A: знакомый в Unreal силуэт шара материала, выразительный потёк глазури и три PBR-зоны хорошо объясняют назначение плагина. Для установки выбранный glaze_A_128.png скопируй в Resources/Icon128.png.\n'
        '\nSVG — исходники; PNG получены офлайн из тех же SVG с шестикратным суперсэмплингом (Pillow + Matplotlib). В preview.png все иконки показаны в реальном размере 128 и 32 px на двух заданных фонах. Для повторного рендера: python build_icons.py.\n',
        encoding='utf-8')
    for letter in ART:
        document = ET.parse(ROOT/f'glaze_{letter}.svg').getroot()
        assert document.get('viewBox') == '0 0 128 128'
        assert not any(el.tag.endswith(('text', 'image', 'script', 'foreignObject')) for el in document.iter())
        assert not any(k.endswith('href') and not v.startswith('#')
                       for el in document.iter() for k, v in el.attrib.items())
        for size in (128, 32):
            with Image.open(ROOT/f'glaze_{letter}_{size}.png') as png:
                assert png.mode == 'RGBA' and png.size == (size, size)
                assert png.getchannel('A').getextrema() == (0, 255)
                bounds = png.getbbox()
                assert 0 < bounds[0] < bounds[2] < size and 0 < bounds[1] < bounds[3] < size
                assert all(png.getpixel(xy)[3] == 0 for xy in [(0, 0), (size-1, 0), (0, size-1), (size-1, size-1)])
                print(f'{letter} / {size}px: RGBA, transparent background, bounds {bounds}')
    print('Created 4 SVGs, 8 RGBA PNGs, preview.png and notes.md.')


if __name__ == '__main__':
    main()
