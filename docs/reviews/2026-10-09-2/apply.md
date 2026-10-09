Нашёл **5 новых проблем**, включая регрессию статического обновления. Проверил код, коммит `925b31a` и реализации API UE 5.8. Редактор и тесты не запускал; файлы не менял.

**Новые находки**

1. **high — изменение только `TwoSided` или `OpacityClip` не пересобирает шейдер.**  
   [GlazeApply.cpp:364](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:364).  
   `BasePropertyOverrides` меняются непосредственно в MI, после чего вызывается `UpdateStaticPermutation(Statics)`. Этот overload сравнивает overrides с тем же уже изменённым объектом. Если switches прежние, пересборки нет. Последующий `PostEditChange()` её также не форсирует. Это регрессия замены библиотечного update.
   
   **Воспроизведение:** дождаться компиляции Masked-превью → изменить только `OpacityClip` с `0.3333` на `0.8` → порог в рецепте и overrides изменится, шейдер останется прежним. Аналогично переключить только `TwoSided`. Изменение любого switch затем применит накопленные изменения.  
   **Исправление:** подготовить отдельную копию overrides и передать её в `UpdateStaticPermutation(Statics, NewOverrides)`, сохранив старое состояние MI до вызова.  
   **Основание:** [MaterialInstance.cpp:4633](D:/EGL/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/MaterialInstance.cpp:4633), [MaterialInstance.cpp:4830](D:/EGL/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/MaterialInstance.cpp:4830).

2. **high — успешное сохранение MI не гарантирует сохранение его мастера.**  
   [GlazeApply.cpp:379](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:379), [GlazeApply.cpp:427](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:427).  
   `EnsureMaster()` возвращает материал даже при ошибке сохранения. `WriteInstanceAsset` проверяет только ненулевой указатель, а список `ToSave` не включает мастер и созданные для него default-текстуры. Повторный `EnsureMaster()` видит актуальную версию в памяти и не повторяет сохранение.
   
   **Воспроизведение:** старый `M_Glaze` версии 3 недоступен для записи → открыть Glaze: мастер перестраивается до версии 5, сохранение завершается ошибкой → создать MI с Detail Noise в доступной папке → Glaze сообщает успех → перезапустить проект без плагина → на диске остаётся старый мастер без графа Detail Noise.  
   **Исправление:** передавать ошибку сохранения зависимостей из `EnsureMaster`; считать Create успешным только после сохранения мастера, его созданных default-текстур и MI.

3. **medium — Create сохраняет посторонние правки исходных текстур.**  
   [GlazeApply.cpp:392](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:392).  
   Новый список сохранения включает **любую** dirty-текстуру активного рецепта независимо от того, менял ли её Glaze и включён ли `bFixTextureSettings`. Таким образом, исправление прежней потери настроек расширило область записи до чужих несохранённых изменений.
   
   **Воспроизведение:** изменить `LOD Bias` у BaseColor в Texture Editor, оставив изменение несохранённым → отключить Fix Texture Settings → создать MI → вместе с ним без отдельного выбора сохраняется `LOD Bias` текстуры.  
   **Исправление:** передавать в запись список пакетов, изменённых Glaze; для ранее dirty-текстур явно показывать, что сохранение исправлений затронет весь ассет.

4. **medium — восстановление после rename выбирает чужую текстуру, если старый путь занят повторно.**  
   [GlazeApply.cpp:465](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:465).  
   Восстановление из MI выполняется только для пустого слота. Если устаревший путь JSON разрешается в другой существующий ассет, действующий override MI игнорируется.
   
   **Воспроизведение:** создать MI с `T_Wood_BC` → переименовать текстуру в `T_Oak_BC`, исправить redirectors → импортировать другую текстуру под прежним именем `T_Wood_BC` → Edit with Glaze → слот содержит новую чужую текстуру → Apply заменяет правильное назначение MI. Аналогичная проверка отсутствует для Detail Noise.  
   **Исправление:** для активных назначений брать текстуры из действующих overrides MI, включая Packed Map, даже когда путь JSON успешно разрешился.

5. **medium — автоисправление HDR-карты не делает её совместимой с Color sampler.**  
   [GlazeApply.cpp:156](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:156), [GlazeApply.cpp:209](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:209).  
   Для Color код сохраняет HDR-compression и включает sRGB. Однако `UTexture::PostEditChangeProperty` снова выключает sRGB для HDR. Результат требует `LinearColor`, тогда как мастер использует `Color`. План исправления продолжает предлагать включение sRGB.
   
   **Воспроизведение:** назначить HDR `UTexture2D` с `TC_HDR` в отдельный Emissive, включить Emissive и автоисправление → текстура остаётся линейной → материал получает несовместимый sampler; повторное исправление снова меняет и пересобирает текстуру без результата.  
   **Исправление:** поддержать отдельный `LinearColor`-вариант либо отклонять несовместимый формат с понятным предупреждением; проверять конечный sampler type после исправления.

**Предыдущие находки**

- №1, №2, №3, №6, №7, №8 и №9 исправлены по коду; удаление texture override сопровождается обновлением render proxy. Регрессию статического обновления см. выше.
- **high — старый №4 остаётся:** [GlazeApply.cpp:425](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:425): рецепт A → Update до B → Undo возвращает параметры MI к A, но JSON остаётся B; требуется синхронное транзакционное восстановление метаданных.
- **medium — старый №5 исправлен частично:** активные карты восстанавливаются после обычного rename; неактивные слоты без overrides после rename и удаления redirectors теряют назначение без предупреждения — нужны диагностика неразрешённых путей и сохранение возможности переназначить их.

**Параметры, память и GC**

Двусторонняя сверка Apply ↔ мастер: **52 scalar, 12 vector, 10 texture, 24 static switch**; несовпадений имён или типов нет. Height намеренно не записывается.

Новых подтверждённых утечек, опасных долгоживущих сырых указателей или GC-проблем на участке не обнаружил. Текстуры рецепта защищены `UPROPERTY/TObjectPtr`; `FTextureFix` используется синхронно. Прежние массовые аллокации editor-прокси устранены. Величину пиков памяти не измерял.

**UX-предложения по пользе**

1. Перед сохранением показать точный список изменяемых исходных ассетов и ранее несохранённых правок.
2. При чтении рецепта показывать потерянные ссылки и восстановленные из MI назначения.
3. Для несовместимых форматов предлагать конкретное действие вместо повторяющегося автоисправления.