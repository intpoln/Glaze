Проверено текущее дерево на `46ba25b` и исходники UE 5.8. Это статическое ревью: сценарии в редакторе не запускал. Ничего не менял. Находок `critical` нет.

**Проверка прошлого отчёта**

№2–5, №8, №10–12 и №14 исправлены: проверка принадлежности MI, закрытие при выгрузке, GC очереди, сохранение внешних назначений материалов, обработка ошибок батча, группировка, Folder и Ctrl+Enter.

- **low, №6 — частично:** [SGlazeWindow.cpp:545](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:545): восстановление использует `SetMaterial`, но исходный пустой массив превращается в массив `nullptr`; после Cancel `HasOverrideMaterials()` возвращает `true` — нужно восстанавливать длину, сохраняя внешние назначения.
- **medium, №9 — частично:** [SGlazeWindow.cpp:350](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:350): ручные Invert/Flip Green перезаписываются на следующем наборе, а автоматически включённые Emissive/Masked могут переноситься на набор без соответствующих карт — нужны отдельные признаки автоматического выбора и пользовательского override.
- **medium, №13 — частично:** [SGlazeWindow.cpp:212](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:212): повторный Edit того же MI поднимает окно, но открытие текстур или другого MI по-прежнему уничтожает текущий черновик — нужно сохранять его либо предупреждать.
- №1 и №7 исправлены неполностью: дополнительные сценарии описаны ниже.

**Находки**

1. **high — Обычная транзакция компонента всё ещё захватывает preview.**  
   [SGlazeWindow.cpp:184](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:184), [SGlazeWindow.cpp:524](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:524)

   Защита добавлена для delete/cut/copy/duplicate, но preview остаётся в `OverrideMaterials` во время обычного `Modify()` компонента. UE сохраняет полное состояние объекта в транзакцию, включая ссылки на материалы; при Undo загружает этот снимок.

   **Воспроизведение:** StaticMeshActor с материалом A → открыть Glaze → изменить Relative Location его mesh-компонента через Details → Cancel → Undo изменения координат. Вместе с координатами вернётся transient preview. Окно уже закрыто, поэтому материал A никто не восстановит.

   **Исправление:** исключить preview из любых транзакционных снимков компонентов; обработки только четырёх перечисленных операций недостаточно. Дополнительно обрабатывать Undo/Redo. Поведение сверено с `EditorTransaction.cpp:134,347`.

2. **high — Cancel скрывает несохранённые внешние изменения текстуры.**  
   [SGlazeWindow.cpp:724](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:724), [SGlazeWindow.cpp:731](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:731)

   Откат безусловно восстанавливает compression/sRGB/VT и снимает dirty-флаг по состоянию **до открытия Glaze**. Он не проверяет, менял ли пользователь текстуру после автоматического исправления.

   **Воспроизведение:** сохранённая Normal-текстура с `TC_Default` → открыть Glaze → в Texture Editor изменить `Maximum Texture Size`, не сохранять → Cancel. Новое значение останется в памяти, но пакет станет clean; Save All пропустит его, после перезапуска правка исчезнет. Внешние изменения compression/sRGB/VT будут непосредственно перезаписаны.

   **Исправление:** откатывать только принадлежащие Glaze изменения и сохранять dirty-флаг при внешних правках. Надёжнее отложить изменение исходных текстур до Create.

3. **medium — При добавлении Opacity/Emissive автоисправление выполняется до активации карты.**  
   [SGlazeWindow.cpp:407](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:407)

   `FixTextures()` вызывается раньше установки `bEmissive` или `BlendMode = Masked`. План исправлений игнорирует ещё неактивную карту. После активации сразу обновляется preview, уже использующий неправильные настройки текстуры. Переключатели активации также не запускают `FixTextures()`.

   **Воспроизведение:** открыть набор без Opacity → перетащить в Opacity текстуру с `TC_Default`, sRGB включён. Проверка пропустит карту, затем включится Masked; preview получает Color-текстуру для Masks sampler и ошибку соответствия sampler type. Create исправит настройки, но оценить результат до создания нельзя.

   **Исправление:** сначала обновлять флаги активации, затем исправлять текстуры и обновлять preview; учитывать переключение BlendMode, Emissive, источника Emissive, Detail Noise и Projection.

4. **medium — Правки рецепта не участвуют в Undo.**  
   [SGlazeWindow.cpp:62](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:62), [SGlazeWindow.cpp:323](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:323)

   Рецепт создаётся без `RF_Transactional`. Обычный PropertyEditor вызывает `Modify()`, но UE не записывает такой объект в Undo. Подписки на восстановление рецепта после Undo/Redo тоже нет.

   **Воспроизведение:** существующий мастер и текстуры с правильными настройками → открыть Glaze → изменить Normal Strength, завершить ввод → выполнить Edit → Undo. Normal Strength не восстановится; может отмениться предыдущая операция редактора.

   **Исправление:** создавать рецепт с `RF_Transactional` и обновлять Details/preview после Undo/Redo. При переходе между наборами учитывать, что это один изменяемый объект рецепта.

5. **medium — Create All создаёт пустой материал для удалённого набора.**  
   [SGlazeWindow.cpp:344](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:344), [SGlazeWindow.cpp:626](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:626)

   Исправление GC безопасно обрабатывает отсутствующие текстуры, но только добавляет предупреждение. Создание не блокируется даже при полностью пустом наборе. В синхронном Create All пользователь не успевает прочитать предупреждение следующего набора.

   **Воспроизведение:** открыть два набора → Force Delete всех текстур второго набора → Create All. Первый материал создастся правильно, второй — без карт, с уведомлением об успешном создании; окно закроется.

   **Исправление:** при исчезновении входных карт останавливать автоматический батч на проблемном наборе и давать восстановить карты либо явно пропустить его.

**Память и GC**

- Подтверждённой постоянной утечки в проверенных файлах не обнаружил. `Recipe`/preview защищены `TStrongObjectPtr`, материалы и очередь — `AddReferencedObjects`; делегаты отписываются.
- Находка №1 удерживает transient preview через Undo и может вернуть его после закрытия окна.
- Лишний пик памяти остаётся: меню загружает все выбранные текстуры сразу, очередь удерживает также обработанные и пропущенные наборы до закрытия. Размер пика не измерялся.

**UX-предложения по пользе**

1. Create All: прогресс, остановка, итоговая сводка; без перестройки Details/preview и синхронизации Content Browser на каждом наборе.
2. Явный выбор целей и material slots для preview, обновление целей после смены уровня/выделения.
3. Видимая очередь наборов со статусами и возможностью пропустить последний.
4. Показывать уже выполненные автоисправления текстур и объяснять последствия отключения `Fix Texture Settings`.