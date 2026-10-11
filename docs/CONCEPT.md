# Glaze — концепция (черновик 2026-10-07)

> Исторический документ. Что реально сделано: один мастер `M_Glaze` (static switch вместо семейства мастеров), C++ editor-модуль, окно — Slate. Актуальное — `START_HERE.md` и `docs/ROADMAP.md`.

Полный разбор Codex: `notes/codex_brainstorm.md`. Здесь — сводка и решения.

## Поток
1. Выделил текстуры (или папку) → ПКМ → **Glaze: Create Material**.
2. Окно: слоты (Base Color, Normal, Roughness, Metallic, AO, Height, Emissive, Opacity, Specular, packed ORM/ARM/RMA) уже заполнены по суффиксам; у каждого слота — миниатюра, канал (R/G/B/A), причина совпадения («суффикс _N»), спорные подсвечены.
3. Галочки модулей → ползунки модуля раскрываются под ним.
4. **Create** (Enter) → MIC рядом с текстурами (`MI_<Set>`), выделен в Content Browser.
5. Повторно: ПКМ на MIC → **Edit with Glaze** (рецепт в metadata).

## Архитектура (рекомендация)
- Семейство мастер-материалов `M_Glaze_Opaque` / `M_Glaze_Masked` (потом Foliage, Decal, Translucent) + библиотека `MF_Glaze_*`.
- Дорогие ветви (triplanar, нойзы, WPO, detail normal, лишние сэмплы) — static switch; дешёвые (tint, contrast, bias) — скаляры.
- Порядок операций фиксирован: UV → сэмплинг → цветокоррекция → ремап PBR → детали → выходы.
- Мастера генерируются **скриптом** (MaterialEditingLibrary), а не руками: воспроизводимо, лежит в git как текст, легко добавлять модули.
- Позже: режим «Standalone Material» — тот же рецепт, но собирается чистый граф без зависимости от плагина.
- Legacy Default Lit как база; Substrate — отдельно, позже.

## Модули
| Этап | Что |
|---|---|
| MVP | автоматч + ручная правка, packed-карты, Opaque/Masked, Color (Desaturation, Contrast, Brightness, Tint, Lerp to Color), ремап Roughness/Metallic/AO/Specular (bias, contrast, min/max, invert), Normal strength, Emissive, UV (tiling/offset/rotation) |
| v2 | Hue shift, Overlay color, Color-to-Lerp (маска по цвету с tolerance/softness), Dithered Masked + distance/camera fade, detail normal, macro variation, world/local-space нойзы (текстура / 3D), triplanar, WPO (wind/wobble/noise), пресеты Wood/Metal/Fabric/Foliage/Decal, batch |
| Потом | Curve remap через LUT, POM, Nanite displacement, Substrate, translucency |

## Фишки, экономящие время
- **Live preview прямо на выделенном меше уровня** (временно подменяем материал, на Cancel — возвращаем). Не нужен свой 3D-вьюпорт.
- Batch: выделил 10 наборов → таблица → 10 инстансов.
- Fix Texture Settings: sRGB/compression для Normal/Masks одной кнопкой (с показом, что изменится).
- Quick Variants (Dark/Wet/Dirty) — дочерние MIC.
- Replace Texture Set / Copy Look — перенести настройки на другой набор.
- Профили имён: Substance, Megascans, свой.
- Пользовательские пресеты.

## C++ или content-only
См. ответ в чате 2026-10-07: **можно без C++** (Asset Action Utility + Editor Utility Widget + генерируемые мастера), но с ограничениями:
пункт меню только в подменю Scripted Asset Actions, окно — вкладка, а не модалка, Undo/прогресс/превью слабее,
и главное — Blueprint/UMG-ассеты агенты почти не могут править как текст. Решение — за Никитой.

## Открытые вопросы
- Вариант реализации (content-only BP / Python / C++ editor module).
- Минимальная версия UE (ассеты сохраняются только «вперёд»).
- Название (рабочее — Glaze) и иконка (концепты в `icons/preview.png`, Codex рекомендует A).
