Нашёл **4 новые проблемы**. Проверил код, исправляющие коммиты и исходники UE 5.8. Редактор и тесты не запускал; ничего не менял.

**Новые находки**

1. **high — открытый Material Instance Editor может перезаписать результат Glaze старой копией параметров.**  
   [GlazeApply.cpp:470](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:470).  
   Запись меняет MI, но не синхронизирует открытый редактор этого инстанса. Его `UMaterialEditorInstanceConstant` хранит собственные `ParameterGroups`; очередное редактирование вызывает `CopyToSourceInstance()`, очищает параметры MI и записывает всю старую копию обратно.  
   **Воспроизведение:** открыть существующий MI в Material Instance Editor → через Glaze изменить только Roughness Value и нажать Apply → в оставшемся открытым редакторе изменить Tint → новое значение Roughness потеряется; JSON Glaze останется с новым значением.  
   **Исправление:** закрывать редактор целевого MI перед записью либо синхронизировать его proxy и транзакционное состояние. Для обновления после записи в UE есть `RefreshMaterialInstanceEditor()`.  
   **Основание:** [PreviewMaterial.cpp:592](D:/EGL/UE_5.8/Engine/Source/Editor/UnrealEd/Private/PreviewMaterial.cpp:592), [PreviewMaterial.cpp:917](D:/EGL/UE_5.8/Engine/Source/Editor/UnrealEd/Private/PreviewMaterial.cpp:917).

2. **medium — текстуры из Data LOD groups остаются несовместимыми после автоисправления.**  
   [GlazeApply.cpp:163](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:163).  
   План проверяет compression, sRGB и VT, но игнорирует `TEXTUREGROUP_8BitData` / `TEXTUREGROUP_16BitData`. Для этих групп UE требует sampler `Data` независимо от compression; мастер использует `Masks`, `Normal`, `Color` или `LinearGrayscale`.  
   **Воспроизведение:** Height-текстура с `LODGroup=TEXTUREGROUP_8BitData`, `TC_Masks`, sRGB off, VT off → назначить Height в UV-проекции → план исправлений пуст → материал получает несовместимый sampler.  
   **Исправление:** проверять итоговый sampler через `GetSamplerTypeForTexture()`; для Data-групп выдавать предупреждение или явно предлагать смену группы.  
   **Основание:** [MaterialExpressionUtils.cpp:25](D:/EGL/UE_5.8/Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionUtils.cpp:25).

3. **medium — удалённое назначение может молча замениться другой текстурой, занявшей прежний путь.**  
   [GlazeApply.cpp:528](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:528).  
   Исправление rename заменяет JSON-ссылку только при ненулевом override. Если override обнулён удалением, успешно разрешившаяся устаревшая JSON-ссылка остаётся в рецепте.  
   **Воспроизведение:** создать MI с `T_Wood_BC` → Force Delete текстуру и сохранить MI с пустым override → импортировать другую текстуру под тем же путём → Edit with Glaze → слот содержит новую текстуру → Apply назначает её без предупреждения. Аналогичная ветка есть для Detail Noise.  
   **Исправление:** учитывать наличие override отдельно от его значения; для активного назначения с пустым override не принимать JSON-путь как подтверждённую ссылку, показывать потерянное назначение.

4. **medium — исправление трёх настроек текстуры сохраняет её source payload в undo. Лишний расход памяти.**  
   [GlazeApply.cpp:207](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:207).  
   `T->Modify()` сериализует весь `UTexture`, включая source bulk data. Для обычного невиртуализированного payload, выгруженного из памяти, UE загружает его с диска и записывает в транзакцию. Это происходит ради compression/sRGB/VT.  
   **Воспроизведение:** открыть набор нескольких больших 8K-текстур с неправильными настройками → автоисправление вызывает `Modify()` для каждой → undo получает копии source payload, растут память и время обработки, быстрее вытесняется предыдущая история undo. Размер пика не измерял.  
   **Исправление:** хранить undo-снимок изменяемых настроек в отдельном транзакционном editor-объекте и восстанавливать текстуры через него, без сериализации source.  
   **Основание:** [Texture.cpp:1111](D:/EGL/UE_5.8/Engine/Source/Runtime/Engine/Private/Texture.cpp:1111), [EditorBulkData.cpp:927](D:/EGL/UE_5.8/Engine/Source/Runtime/CoreUObject/Private/Serialization/EditorBulkData.cpp:927).

**Проверка прошлого отчёта**

Исправления №1–4 из `2026-10-09-2/apply.md` устраняют исходные сценарии по коду: overrides передаются копией; dirty-зависимости мастера повторно сохраняются; список исходных текстур ограничен исправленными Glaze; действующий texture override имеет приоритет над старым JSON-путём.

Оставшиеся проблемы:

- **medium — №5 исправлен частично:** [GlazeApply.cpp:146](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:146): HDR-проверка пропускает `TC_HalfFloat` и `TC_SingleFloat`; UE снова выключает sRGB после исправления — использовать `UE::TextureDefines::IsHDR()`.
- **high — прежний №4 остаётся:** [GlazeApply.cpp:471](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:471): Update A→B → Undo возвращает MI к A, JSON остаётся B; требуется транзакционное восстановление метаданных.
- **medium — прежний №5 остаётся частично:** [GlazeApply.cpp:517](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:517): неактивные назначения после rename и удаления redirectors теряются без диагностики.
- **medium — у №3 остался хвост:** [GlazeApply.cpp:442](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:442): если Glaze исправляет уже dirty-текстуру, сохраняются и её посторонние правки; предупреждения об этом нет.

**Параметры, память и GC**

Двусторонняя сверка текущего Apply ↔ мастер: **60 scalar, 13 vector, 11 texture, 32 static switch**. Расхождений имён и типов нет; Height и Parallax Steps записываются.

EditorOnly-сеттеры обновляют render proxy напрямую. Удаление texture override сопровождается обновлением ресурсов как при прежнем, так и при изменившемся статическом ключе.

Подтверждённых постоянных утечек и опасных долгоживущих сырых указателей не обнаружил. Текстуры рецепта защищены `UPROPERTY/TObjectPtr`; `FTextureFix` используется синхронно. Лишний расход памяти undo описан в №4.

**UX-предложения, по пользе**

1. Показывать потерянные назначения с прежним путём и действием «Переназначить».
2. Перед сохранением исправленных текстур сообщать, какие уже содержат другие несохранённые правки.
3. Для несовместимого sampler показывать конкретную причину и действие вместо повторяющегося автоисправления.