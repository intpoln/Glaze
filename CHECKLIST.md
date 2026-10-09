# Glaze — что проверить руками

> **Статус (2026-10-09): Никита ещё ничего не смотрел — все пункты ниже ждут проверки.** Смотрит позже, вместе с новыми фичами.
> Отмечай проверенное: `- [x]`. Нашёл проблему — пиши под пунктом.
>
> Открытые решения:
> - [ ] Пересобрать пакет `packages/5.8` с новыми фичами (после проверки)
> - [ ] Пак: оставляем процедурные материалы или добавляем сканы (ambientCG / Poly Haven, CC0; скачивание — только с разрешения)

## v0 + пак (2026-10-07)
1. **Пак.** Content Browser → ПКМ по пустому месту → Glaze: Material from Pack → Wood Planks → 2K. Текстуры появились в `/Game/Glaze/Pack/Wood_Planks/2k`, открылось окно Glaze со слотами BC / Normal / ORM (Roughness G, AO R, Metallic B).
2. **Мастер.** При первом создании появляется `/Game/Glaze/M_Glaze`. Шейдер компилируется без ошибок (Output Log: LogGlaze*, ошибки материала).
3. **Live preview.** Поставь на уровень куб/сферу, выдели его, потом открой окно Glaze: материал сразу виден на меше и меняется вживую. Cancel возвращает старый материал, Create при галке «Keep on selected meshes» оставляет новый.
4. **Color Correction.** Desaturation, Contrast, Brightness, Hue Shift, Overlay, Lerp Color + Lerp Amount (0 = исходный цвет, 1 = заданный), Tint.
5. **UV.** Tiling, Rotation — нормаль при повороте должна оставаться правильной (свет падает с той же стороны).
6. **Ремапы.** Roughness/Metallic/Specular/AO: Invert, Contrast, Bias, Min/Max.
7. **Masked + Dithered.** Blend Mode Masked, Opacity Amount, Dithered.
8. **Свои текстуры.** Выдели набор текстур (BaseColor/Normal/ORM или Roughness/AO) → ПКМ → Glaze: Create Material. Несколько наборов сразу → Create & Next / Skip / Create All.
9. **Edit with Glaze.** ПКМ по созданному инстансу → Edit with Glaze → настройки вернулись.
10. **Нойз.** Add → Glaze: Noise from Pack → любой → импортируется в `/Game/Glaze/Pack/Noises`.
11. **Горячие клавиши окна.** Ctrl+Enter — создать, Esc — отмена.

## v2: детали на нойзах (2026-10-08)
12. **Detail Noise.** В окне Glaze, раздел Noise, включи Detail Noise. Без текстуры работает процедурный нойз; с текстурой выбери нойз из пака (Add → Glaze: Noise from Pack). Color / Color Amount и Roughness Amount меняют грязь и блеск по маске.
13. **Пространства.** World: двигай куб, нойз «стоит» в мире. Local: нойз едет вместе с кубом. UV: по развёртке. Pre-Skinned: на анимированном скелетал-меше (Manny) детали не плывут при движении.
14. **Detail Bump.** Detail Bump = 1–2 см: появляется рельеф по нойзу, отрицательное значение — вмятины. Свет должен падать правильно со всех сторон куба.
15. **Macro Variation.** Включи на большой плоскости (пол 20×20 м) с плиткой или травой: повторяемость тайлинга должна заметно пропасть. Macro Size / Macro Amount / Macro Roughness.
