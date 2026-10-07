Design an ORIGINAL plugin icon for "Glaze", an Unreal Engine 5 editor plugin that instantly builds PBR materials from a set of textures (select textures -> one window -> material instance with color correction, roughness/specular tweaks, noise details). The name means a ceramic glaze (sibling plugin of "Kiln", a pottery kiln) and also a glossy material surface.

Used as Resources/Icon128.png (128x128); must read clearly at 32x32 on dark and light backgrounds, transparent background.

Make 4 distinctly different concepts, e.g. (feel free to improve):
 A. a shiny glazed sphere (material-preview ball) with a drip of glaze running down, split into 2-3 bands (color / roughness / normal look);
 B. a glaze drip/droplet whose inside shows stacked texture tiles (base color, normal-blue, grey roughness) merging into one glossy surface;
 C. three small texture squares (offset stack) flowing via one bold arrow/drip into a glossy ball;
 D. a ceramic bowl/vase with a glossy glaze pouring over it, specular highlight.
Palette: glossy teal/cyan glaze (#19C3C3 -> #6EF0E0) with a warm accent (#FF8A3D) and deep charcoal outline (#1E1E24) so it stands out in UE's grey UI and pairs with the orange Kiln icon. Bold shapes, thick strokes, strong specular highlight, no thin details, no text, no engine/brand logos, not a copy of any existing logo.

Deliver in the current directory:
 - glaze_A.svg ... glaze_D.svg (viewBox 0 0 128 128, transparent background, self-contained, no external refs/fonts);
 - PNG renders at 128 and 32 px (glaze_A_128.png, glaze_A_32.png, ...). If you have an image generation tool you may use it in addition, but SVG sources are required. python + PIL are installed; pip install resvg/cairosvg or any offline method works; PIL drawing as fallback;
 - preview.png: contact sheet with all concepts at 128 and 32 px, each on dark grey (#2A2A2A) and light grey (#BFBFBF) tiles, labelled A..D;
 - notes.md: one line per concept (in Russian) and which one you recommend.
Work only inside this directory.
