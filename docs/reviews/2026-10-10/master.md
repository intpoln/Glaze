Проверено по текущему коду, истории исправлений и исходникам UE 5.8. Файлы не менял; редактор и компиляцию шейдеров не запускал. Численные примеры ниже проверены расчётом.

**Новые находки**

1. **high — пересборка выключает Detail Bump у существующих инстансов.**  
   [GlazeMaster.cpp:674](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:674), default нового свитча — строка 221.

   Старые MI содержат `Use Detail Noise` и `Detail Bump`, но не содержат нового `Use Detail Bump`. После пересборки он наследует `false`. `RecompileMaterial` обновляет ресурсы и имена параметров, но не переносит значение старого переключателя в новый.

   **Воспроизведение:** MI, созданный до `46ba25b`, с Detail Noise=true и Bump=2 → обновить мастер через открытие Glaze для другого набора → рельеф старого MI исчезает, хотя сохранённая сила остаётся 2. Возвращается после повторного Apply через Glaze.

   **Исправление:** мигрировать старые MI: установить новый свитч из прежнего включения Detail и ненулевой силы Bump; обработать также инстансы, загружаемые после пересборки, и сохранить изменения.

2. **medium — Normal Strength=0 не отключает влияние normal map в triplanar.**  
   [GlazeMaster.cpp:620](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:620), использование Z — строки 640–643.

   `Unpack` обнуляет XY, оставляя исходный Z. В triplanar этот Z меняет отдельную компоненту каждой проекции **до** их смешивания. Нормализация итоговой нормали на строке 655 уже не восстанавливает направление поверхности.

   **Воспроизведение:** World Triplanar, поверхность с нормалью `(0.7071, 0.7071, 0)`, Strength=0; X-проекция читает normal `(0.8, 0, 0.6)`, Y-проекция — `(0, 0, 1)` → итоговая world-нормаль `(0.6247, 0.7809, 0)`, отклонение **6.34°**. Отключение normal map меняет освещение.

   **Исправление:** нормализовать каждую normal после Strength/Flip Green, до проекционного whiteout-бленда; предусмотреть плоскую нормаль при нулевой силе.

3. **medium — Wind By Height отсчитывается от центра bounds, а не от пивота.**  
   [GlazeMaster.cpp:738](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:738).

   Для обычного StaticMesh `ObjectPositionWS` — центр bounds. Это подтверждается [PrimitiveUniformShaderParametersBuilder.h:194](/D:/EGL/UE_5.8/Engine/Source/Runtime/Engine/Public/PrimitiveUniformShaderParametersBuilder.h:194). Изменение в `af503c4` исправило единицы измерения, но сместило начало маски.

   **Воспроизведение:** вертикальный меш высотой 200 см, пивот у основания, bounds center=100 см; By Height, Full Sway Height=200 → нижняя половина неподвижна, вершина получает вес **0.25 вместо 1**. Одинаковая геометрия обычного StaticMesh и ISM может качаться по-разному.

   **Исправление:** вычитать world-позицию локального origin меша/инстанса через корректное преобразование позиции, сохранив расчёт высоты в мировых сантиметрах.

4. **medium — POM и основные текстуры используют разные режимы адресации.**  
   [GlazeMaster.cpp:461](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:461); основные выборки — строка 211.

   Основные карты принудительно используют shared Wrap. `TexSampler` в Custom POM берётся из настроек текстуры. В UE `TextureObjectParameter::Compile` вызывает `TextureParameter` без переопределения sampler source: [MaterialExpressions.cpp:4148](/D:/EGL/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/MaterialExpressions.cpp:4148).

   **Воспроизведение:** packed ORDp с Address X=Clamp, Height в B — рампа от чёрного слева до белого справа; Tiling=2 → на втором тайле POM читает белый край, считает поверхность плоской, тогда как основные карты повторяют содержимое тайла.

   **Исправление:** использовать одинаковую адресацию для ray march и основных выборок. Для нынешней модели тайлинга — явно выбрать Wrap в POM; простого присваивания `TextureObjectParameter::SamplerSource` недостаточно для legacy translator.

5. **medium — Parallax Depth=0 сохраняет стоимость ray march.**  
   [GlazeMaster.cpp:489](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:489).

   Ветка зависит только от наличия Height. Нулевая глубина обнуляет смещение, но не отключает Custom и цикл с выборками.

   **Воспроизведение:** Height полностью чёрная, Steps=16, Depth=0 → изображение плоское, но код выполняет **17 выборок Height** на пиксель. Удаление Height убирает эту работу.

   **Исправление:** выключать POM статическим свитчем при нулевой глубине, по аналогии с исправленным Detail Bump.

6. **medium — World procedural noise разрывается на границах LWC-тайлов.**  
   [GlazeMaster.cpp:266](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:266), координаты Detail — строка 538.

   LWC сохраняется до Noise, однако UE передаёт в шум только `LWCNormalizeTile(Position).Offset`: [Common.ush:1845](/D:/EGL/UE_5.8/Engine/Shaders/Private/Common.ush:1845). Размер LWC-тайла — 2 097 152, а фактический период выбранного FastGradient — **15**: [Random.ush:447](/D:/EGL/UE_5.8/Engine/Shaders/Private/Random.ush:447). Отбрасываемое смещение не кратно периоду шума.

   **Воспроизведение:** Detail procedural, World, Size=1 см; поверхность пересекает X=1 048 576 см, Y/Z — нецелые → нормализованная X-координата скачет примерно с `+1 048 576` на `−1 048 576`. Маска перестаёт быть непрерывной; Detail Bump усиливает шов.

   **Исправление:** сворачивать LWC-координаты по реальному периоду выбранного шума до преобразования в float, вместо попадания в штатное отбрасывание LWC-тайла.

**Проверка прежнего отчёта**

Исправления нормализации Noise, Cancel при закрытии Material Editor, очистки expressions, повышения версии до 8, запрета downgrade и обновления ресурсов дочерних MI корректны. Отдельный Bump-свитч работает для заново применённого рецепта; регрессия миграции описана выше.

- **medium, частично исправлено — [cpp:836](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:836):** Create/Apply повторяет сохранение dirty-мастера и defaults; сам `EnsureMaster(true)` после отказа всё ещё возвращает успех, а следующий вызов пропускает сохранение — возвращать ошибку либо повторять запись dirty-зависимостей.
- **medium, не исправлено — [cpp:409](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:409):** Local position и normal расходятся для повёрнутого ISM; неравномерный scale также требует корректного преобразования нормали.
- **medium, не исправлено — [cpp:627](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:627):** отрицательный TilingXY отражает карту без соответствующего изменения направления normal.
- **medium, не исправлено — [cpp:266](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:266):** FilterWidth не подключён; мелкий procedural noise продолжает алиасить вдали.

**Математика и стоимость**

Дополнительных ошибок в обратном повороте UV-нормали при положительном tiling, Flip Green, RGBA dot, grayscale-remap, opacity/dither и emissive не обнаружил. Дубли параметров имеют согласованные defaults и sampler types. Статические свитчи выбирают активную ветку в legacy translator и MIR; Pre-Skinned передаётся через VertexInterpolator.

Логические выборки для **BC + Normal + ORM**, без освещения и дополнительных проходов:

| Ветка | Выборки |
|---|---:|
| Основные карты, UV | 3 |
| Основные карты, triplanar | 9 |
| Добавить Detail texture UV | +1 |
| Добавить Detail texture triplanar | +3 |
| Добавить procedural Detail, 4 октавы | +4 |
| Добавить Macro, 3 октавы | +3 |
| Добавить POM | +1…65 |

FastGradient оценивается заголовком UE примерно в **16 инструкций на октаву**: около 64 для Detail и 48 для Macro, сверх остального графа. Detail Bump добавляет производные, cross/dot, transform и нормализации, без новых texture samples. Точные инструкции и время требуют измерения скомпилированных вариантов.

**Память / GC**

Подтверждённых утечек, незакрытых делегатов и опасных долгоживущих UObject-указателей на участке не обнаружил. Expressions удерживаются материалом; `FGraph`/`FPin` используются синхронно. Старый граф очищается по копии массива. Пики памяти при пересборке загруженных MI и пяти Usage-вариантов не измерены.

**UX-предложения, по пользе**

1. При обновлении мастера автоматически мигрировать старые MI, сохраняя включённый рельеф.
2. Нулевая сила должна полностью отключать соответствующую дорогую ветку; в Glaze нужен явный переключатель POM.
3. Показывать стоимость выбранных проекций, Noise и POM; пояснить, что Object Texture Size измеряется до scale объекта.