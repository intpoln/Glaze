# Glaze — что проверить руками

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
