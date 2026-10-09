# Glaze: материалы для публикации на Fab

Этот файл для агента, который выкладывает Glaze на Fab вместе с Никитой. Здесь есть всё, что нужно: что это за плагин, готовые тексты листинга, где лежат сборки и картинки, и что проверить перед отправкой.

## Что это за плагин

**Glaze** — editor-плагин для Unreal Engine 5.7 и 5.8 (C++, Win64). Он превращает набор текстур (BaseColor / Normal / ORM / Roughness / Height…) в готовый **Material Instance** через одно окно, без нод.

Как это работает:
1. Выделяешь текстуры в Content Browser → ПКМ → **Glaze: Create Material**. Плагин раскладывает карты по слотам по именам и понимает соглашения Quixel, Poly Haven, ambientCG и Substance, а также упакованные ORM/ARM/RMA.
2. Открывается окно Glaze: цвет, ремапы roughness/metallic/AO, детали на нойзе, triplanar, параллакс, ветер и т.д. Выделенный меш на уровне сразу показывает результат (live preview).
3. **Create** → в проекте появляется обычный Material Instance от мастер-материала `/Game/Glaze/M_Glaze`, который плагин сам генерирует в проект.

**Результату плагин не нужен.** Мастер и инстансы — обычные ассеты проекта. Плагин можно отключить, и материалы продолжат работать, в том числе в собранной игре.

## Откуда текстуры

Все текстуры пака (18 материалов и 21 нойз) сделаны в **PocketTextureLab** — веб-приложении Никиты: https://intpoln.github.io/PocketTextureLab/

Это **обязательно** указать в описании листинга (см. текст ниже).

## Готовые тексты листинга (английский)

**Title:** Glaze — Texture Set to Material in One Window

**Short description (до ~200 символов):**
Turn any texture set into a ready Material Instance in one window: auto map matching, live preview on your mesh, color correction, detail noise, triplanar, parallax, wind. No node graphs.

**Long description:**

> Glaze turns a texture set into a finished Material Instance without opening a material graph.
>
> Select your textures in the Content Browser, right-click → **Glaze: Create Material**. Glaze matches the maps by name (Quixel, Poly Haven, ambientCG and Substance naming, packed ORM / ARM / RMA), opens one window with every setting, and previews each change live on the meshes selected in your level. Press **Create** to get a regular Material Instance.
>
> **Your materials do not depend on the plugin.** Glaze writes a standard master material and Material Instances into your project. Disable or remove the plugin and everything keeps working, in the editor and in packaged games.
>
> **Features**
> - Automatic map matching by name, including packed channels, DirectX/OpenGL normals and gloss → roughness.
> - Live preview on the selected meshes; Cancel restores their original materials.
> - Batch: select several texture sets at once → Create & Next / Create All.
> - Edit with Glaze: reopen any Glaze material with its settings (stored in the asset).
> - Automatic texture settings fix (Normalmap / Masks compression, sRGB), undone on Cancel.
> - Color correction: tint, desaturation, contrast, brightness, hue shift, overlay, lerp color.
> - Roughness / Metallic / Specular / AO remap: invert, contrast, bias, min/max.
> - UV tiling, offset and rotation (with correct normals), or World / Object triplanar projection.
> - Parallax Occlusion Mapping from a Height map or a packed channel: depth and step count.
> - Detail noise (procedural or texture) in World, Local or UV space, with color, roughness and bump.
> - Macro variation to break up tiling on large surfaces.
> - Opaque / Masked (with dithering) / Two Sided, emissive.
> - Wind: vertex sway by height or by vertex color, with gusts.
> - Works on static, skeletal, instanced, spline meshes, geometry caches and Niagara meshes.
>
> **Included content**
> - 18 stylized PBR materials (BaseColor / Normal / ORM, 2K and 1K, importable as 512 and 256): Content Browser → right-click → **Glaze: Material from Pack**.
> - 21 tileable grayscale noises with a noise window (preview, tiling, invert, levels, gamma, contrast): **Glaze: Noise from Pack**.
> - All pack textures were made with **PocketTextureLab** — https://intpoln.github.io/PocketTextureLab/
>
> **Technical details**
> - Editor-only plugin (one Editor module), Win64. Unreal Engine 5.7 and 5.8.
> - Nothing from Glaze is needed at runtime: the generated material is a regular project asset.
> - Pack textures are imported into `/Game/Glaze/Pack` only when you ask for them.

**Category:** Code Plugins / Tools (Materials).
**Tags:** material, material instance, texture, PBR, workflow, editor tool, triplanar, parallax, noise, color correction.

## Где лежат файлы

Все пути относительно корня репозитория `C:\Users\intpo\Desktop\Glaze`.

### Плагин

| Что | Путь | Для чего |
|---|---|---|
| **Для загрузки на Fab, UE 5.7** | `packages/Glaze_5.7_source.zip` | Source + Resources + .uplugin, без Binaries/Intermediate. Fab сам компилирует code plugins |
| **Для загрузки на Fab, UE 5.8** | `packages/Glaze_5.8_source.zip` | то же для 5.8 |
| Собранный плагин 5.7 (с бинарниками) | `packages/5.7/Glaze` | ручная установка: скопировать в `<Project>/Plugins/Glaze` |
| Собранный плагин 5.8 (с бинарниками) | `packages/5.8/Glaze` | то же для 5.8 |

Обе сборки сделаны через `RunUAT BuildPlugin` без ошибок и предупреждений (`Tools/package.sh 5.7` / `5.8`). Пакет 5.7 дополнительно проверен в чистом проекте UE 5.7: все 7 автотестов Glaze прошли, включая компиляцию шейдеров на GPU.

`packages/` не в git: zip и сборки лежат только локально на этой машине.

### Картинки (1920×1080, PNG)

Папка `fab/images/`:

| Файл | Куда на Fab |
|---|---|
| `01_thumbnail.png` | **Thumbnail / обложка** листинга |
| `02_workflow.png` | галерея, первая: как работает (текстуры → окно → результат на меше) |
| `03_one_set_many_looks.png` | галерея: один набор текстур, разные материалы (цветокоррекция, нойз) |
| `04_included_materials.png` | галерея: 18 материалов пака, отрендерены в UE |

Исходные кадры из редактора лежат в `fab/raw/`. Скрипт, который собирает из них картинки: `Tools/fab/compose.py` (`python Tools/fab/compose.py`).

Если Fab требует другой размер thumbnail (например, квадрат), обрежь `01_thumbnail.png` или поправь функцию `thumbnail()` в скрипте.

## Что сделать/проверить перед отправкой

1. **`.uplugin`** (`GlazeLab/Plugins/Glaze/Glaze.uplugin`, его копии в обоих zip):
   - Сейчас `"VersionName": "0.1.0"` и `"IsBetaVersion": true`. Для релиза — поставить `1.0.0` / `false`, если Никита согласен.
   - Fab требует заполнить `"FabURL"` ссылкой на листинг (появится после создания черновика). `CreatedByURL`, `DocsURL` и `SupportURL` тоже пустые: вписать туда страницу/почту поддержки.
   - После правки пересобрать: `bash Tools/package.sh 5.7`, `bash Tools/package.sh 5.8`. Затем пересоздать zip (код ниже).
2. **Документация.** Fab просит ссылку на документацию. Подойдёт этот файл без раздела «Где лежат файлы» либо README на GitHub/PocketTextureLab.
3. **Видео.** Не делаем, только картинки (решение Никиты).
4. **Лицензия текстур.** Текстуры пака свои (PocketTextureLab), сторонних ассетов в плагине нет.

Пересоздать zip после пересборки:

```bash
cd C:/Users/intpo/Desktop/Glaze && python -c "
import zipfile; from pathlib import Path
for v in ('5.7','5.8'):
    src=Path('packages')/v/'Glaze'; out=Path('packages')/f'Glaze_{v}_source.zip'
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
        for p in src.rglob('*'):
            r=p.relative_to(src)
            if p.is_file() and r.parts[0] not in ('Binaries','Intermediate','UnrealBuildTool'): z.write(p, Path('Glaze')/r)
    print(out)"
```
