# Glaze — план

Легенда: [x] готово · [~] в работе · [ ] не начато

## v0 — ядро (UE 5.8)
- [x] C++ editor-модуль GlazeEditor, тестовый проект GlazeLab, Tools/build.sh, Tools/runtests.sh
- [x] Мастер M_Glaze генерируется кодом в `/Game/Glaze` (переносимость без плагина), версия в метаданных пакета
- [x] Матчер имён текстур (BaseColor/N/ORM/ARM/gloss/nor_gl…, разрешения, группировка по наборам)
- [x] Окно Glaze: слоты, UV, Color Correction (Desaturation, Contrast, Brightness, HueShift, Overlay, Lerp to Color = цвет + float, Tint), ремап Roughness/Metallic/Specular/AO, Masked + Dither, Emissive
- [x] Packed-карты одной выборкой, автоисправление настроек текстур, Edit with Glaze, live preview на выделенных мешах, батч
- [x] Автотесты Glaze.* (4 шт.)
- [ ] Визуальная проверка в редакторе (компиляция шейдера, поворот нормали, preview)
- [ ] Проверка переноса инстанса в чистый проект без плагина

## v1 — пак текстур
- [ ] Бесшовные базовые текстуры 2k/1k: дерево, камень, кирпич, металл, ткань, земля, плитка, штукатурка… Не только сканы — нужны и простые/казуальные (stylized)
- [ ] Нойзы для деталей (бесшовные; генерация своими силами)
- [ ] Источники на проверку: CC0-библиотеки, PocketTextureLab (https://intpoln.github.io/PocketTextureLab/), Blender (процедурные шейдеры → bake, бесплатные аддоны), генерация через Codex
- [ ] При использовании пак копируется в `/Game/Glaze/...` (переносимость)

## v2 — детали и WPO
- [ ] Детали на нойзах: World Space, Local (Object) Space, **Pre-Skinned Local Position** (для скелетал-мешей — детали не «плывут» при анимации); текстурные и 3D-нойзы
- [ ] Detail normal, macro variation, triplanar
- [ ] WPO: ветер/покачивание/нойз (с учётом pre-skinned для скелеталок)
- [ ] Использование Height

## v3 — полировка
- [ ] Пресеты
- [ ] Код-ревью: Codex (Sol 6.1) + Sonnet 5.5; UX-ревью и предложения от них же; память/пики памяти — отдельный пункт
- [ ] Поддержка UE 5.7 (только после финализации)
