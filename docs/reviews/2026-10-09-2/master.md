Проверено по текущему коду, коммитам `25d7a6c` / `925b31a` и исходникам UE 5.8. **Две новые ошибки в математике и два пробела в исправлениях пересборки.** Редактор и компиляцию шейдеров не запускал; численные примеры проверены расчётом. Файлы не менял.

**Новые находки**

1. **medium — normal map влияет на Detail Bump даже при `Normal Strength = 0`.**  
   [GlazeMaster.cpp:525](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:525), причина также на строках 506–507.

   Перед whiteout-блендом `Base` не нормализуется. Strength обнуляет XY, но сохраняет исходный Z; затем этот Z умножается на Z bump-нормали. Финальная нормализация не устраняет ошибку.

   **Воспроизведение:** плоскость 10 см, normal map с декодированной нормалью `(0.8, 0, 0.6)`, Strength=0; Detail в UV, линейная grayscale-рампа, Bump=5 см → отключение normal map меняет освещение Detail Bump. Для соответствующего уклона:
   - без normal map: `(-0.447, 0, 0.894)`;
   - с normal map и Strength=0: `(-0.640, 0, 0.768)`.

   **Исправление:** нормализовать базовую нормаль после изменения Strength, до whiteout-бленда.

2. **medium — процедурный Noise выходит за ожидаемый диапазон; Macro получает значения выше 1.**  
   [GlazeMaster.cpp:243](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:243), использование — строка 464.

   UE задаёт `bTurbulence=true` по умолчанию; Glaze оставляет его включённым. Шейдер суммирует `abs(noise)` с весами `1, ½, ¼…`, без нормализации суммы. `OutputMin=0` / `OutputMax=1` **не ограничивают результат**. Detail затем обрезается через saturate, а Macro вычисляет `2*M−1` без ограничения.

   **Воспизведение:** Macro в World, Size=1000 см, Amount=1; точка около `(1521.5, 14189.9, 11862.4)` см → расчёт по генератору служебной текстуры UE и HLSL даёт `M≈1.138`, `Macro≈1.276`. Множитель цвета становится ≈2.276 вместо предполагаемого максимума 2. Turbulence также смещает среднюю яркость Macro; Detail теряет вариации в обрезанных пиках.

   **Исправление:** явно выбрать turbulence/обычный gradient, нормализовать октавы и обеспечить диапазон `[0,1]` перед ремапом Macro. Для нейтральных светлых/тёмных пятен использовать центрированный signed noise.

**Пробелы в исправлениях пересборки**

3. **high — Cancel в диалоге закрытия Material Editor не отменяет пересборку.**  
   [GlazeMaster.cpp:624](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:624).

   `CloseAllEditorsForAsset()` запрашивает закрытие, но редактор может отказать. Возвращаемое число — количество найденных редакторов, а не подтверждение закрытия. Glaze продолжает менять мастер.

   **Воспроизведение:** открыть старый мастер, изменить граф без Apply → вызвать Glaze → в диалоге закрытия нажать Cancel → окно остаётся со старой копией, мастер пересобирается → последующий Apply возвращает старый граф с новой metadata-версией. Это незакрытый сценарий прежней находки №3.

   **Подтверждение UE:** `AssetEditorSubsystem.cpp:307`, `MaterialEditor.cpp:2351` — отказ при Cancel; `MaterialEditor.cpp:2968` — сохранение metadata при Apply.

   **Исправление:** после запроса закрытия проверить оставшиеся редакторы; при отказе прервать обновление и создание MI с понятным сообщением.

4. **medium — исправление очистки графа не применяется к уже повреждённым мастерам версии 5.**  
   [GlazeMaster.h:11](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.h:11), ранний выход — [GlazeMaster.cpp:610](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:610).

   После исправления удаления узлов `MasterVersion` остался 5. Мастер, ранее обновлённый с версии 4 ошибочным обходом, уже имеет metadata=5 и больше не пересобирается.

   **Воспроизведение:** обновить мастер v4 старым кодом до v5 и сохранить → установить исправленный плагин → открыть Glaze → прежние лишние expressions, параметры и ссылки остаются.

   **Исправление:** поднять версию миграции, чтобы существующие v5 прошли исправленную очистку.

**Остальные находки предыдущего отчёта**

Очистка по копии массива, обновление дочерних MI через `RecompileMaterial`, запрет downgrade и отсутствие записи при `bSave=false` реализованы корректно.

- **medium, прежняя №4 — не исправлено полностью:** [cpp:642](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:642): отказ сохранения → возвращается успешный мастер, версия в памяти уже актуальна, следующий вызов пропускает сохранение; вернуть ошибку и повторять сохранение dirty-мастера вместе с зависимостями.
- **medium, прежняя №6 — не исправлено:** [cpp:410](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:410): повёрнутый ISM-инстанс → Local position и normal используют разные пространства; согласовать Instance space и преобразование нормали при неравномерном масштабе.
- **medium, прежняя №7 — не исправлено:** [cpp:504](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:504): отрицательный TilingXY → отражённая текстура с неверным направлением нормали; учитывать знаки масштаба перед обратным поворотом.
- **medium, прежняя №8 — не исправлено:** [cpp:527](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:527): Detail включён, Bump=0 → производные и бленд остаются в шейдере; добавить отдельный static switch.
- **medium, прежняя №9 — не исправлено:** [cpp:249](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMaster.cpp:249): мелкий procedural noise вдали → отсутствует фильтрация частот; подключить footprint к FilterWidth.

**Остальная математика и стоимость**

Поворот UV / обратный поворот нормали при положительном tiling, Flip Green, RGBA dot для packed, grayscale-remap, opacity/dither и emissive согласованы. Дубли параметров имеют совместимые defaults; конфликта не обнаружено. Статические свитчи выбирают только активную ветку в legacy translator и MIR. Pre-Skinned передаётся через VertexInterpolator; LWC World Position сохраняется до поддерживающих его операций. Sampler types соответствуют штатным настройкам и созданным defaults.

Оценка логических выборок для **BC + Normal + ORM**, без освещения и дополнительных проходов:

| Функции | Выборки |
|---|---:|
| Базовые карты | 3 |
| + Detail texture UV | 4 |
| + Detail texture triplanar | 6 |
| + Procedural Detail | 7 |
| + Macro | 6 |
| + Triplanar Detail и Macro | 9 |
| + Procedural Detail и Macro | 10 |

Procedural Detail использует четыре выборки служебной 3D-текстуры, Macro — три. Заголовок UE оценивает Fast Gradient примерно в **16 инструкций на октаву**: ≈64 и ≈48 соответственно, сверх остального графа. Bump не добавляет texture samples, но добавляет производные, cross/dot, преобразование пространства и нормализации. Точные инструкции требуют измерения скомпилированной пермутации.

**Память / GC**

Новых подтверждённых утечек или опасных долгоживущих UObject-указателей не обнаружено. Expressions удерживаются коллекцией материала; `FGraph` и `FPin` используются синхронно. Однако находка №4 сохраняет старое лишнее удержание expressions и зависимостей в ранее созданных v5. Пики памяти компиляции пяти Usage flags и дочерних пермутаций не измерены.

**UX-предложения, по пользе**

1. Cancel при обновлении мастера должен останавливать операцию; ошибки сохранения — отображаться в окне.
2. Автоматически выключать ветку Bump при нулевой силе.
3. Показывать стоимость Noise: UV — 1 выборка, triplanar — 3, procedural — 4; Macro — ещё 3.