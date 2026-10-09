Проверен весь `Private`, кроме `Tests`, со сверкой исходников UE 5.8. **7 находок; подтверждённых утечек нет.** Редактор не запускал: объёмы ниже расчётные, время нужно измерить.

1. **high — все текстуры батча загружаются до открытия окна и удерживаются до его закрытия.**  
   [GlazeEditorModule.cpp:67](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeEditorModule.cpp:67), [SGlazeWindow.cpp:163](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:163).  
   **Сценарий:** выбрать 100 ещё не загруженных наборов BC/N/ORM → Create Material. Синхронно загружаются 300 текстур; `FGCObject` удерживает всю очередь, включая пропущенные наборы. Окно появляется только после загрузки и группировки.  
   **Величина:** при полном mip chain, BC1 для BC/ORM и BC5 для Normal — примерно **10,7 МиБ на набор 2K**, всего **1,04 ГиБ**; для 1K — около **267 МиБ** на 100 наборов. Это эквивалент полной GPU-резидентности, фактическая зависит от streaming; CPU-память и данные компрессии добавляются отдельно. Проверять через Memory Insights, `memreport -full`, `stat streaming`.  
   **Исправление:** группировать `FAssetData` по именам/путям; загружать текущий набор и ограниченный prefetch. Освобождать ссылки на обработанные и пропущенные наборы.

2. **high — Create All блокирует редактор и выполняет интерактивную работу для каждого набора.**  
   [SGlazeWindow.cpp:608](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:608), [SGlazeWindow.cpp:397](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:397), [SGlazeWindow.cpp:590](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:590).  
   **Сценарий:** 100 наборов → Create All. Один синхронный цикл создаёт и сохраняет каждый MI, перестраивает Details, обновляет preview, синхронизирует Content Browser и создаёт уведомление. Отменить оставшуюся очередь нельзя. При разных конфигурациях карт обновление preview дополнительно проходит через static permutation и `FlushRenderingCommands`.  
   **Величина:** 100 вызовов сохранения, 100 синхронизаций браузера/уведомлений, 99 ненужных для батча переходов через `StartSet`. Общее время — сумма операций всех наборов. Измерять Game Thread в Timing Insights; одинаковые пермутации могут переиспользовать shader cache.  
   **Исправление:** отдельный путь батча без Details/preview и уведомлений на каждый MI; обработка порциями с отменой, общая сводка и группировка сохранений.

3. **medium — Opacity Clip вызывает полный PostEditChange на каждом Interactive-изменении.**  
   [SGlazeWindow.cpp:421](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:421), [GlazeApply.cpp:331](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:331), [GlazeApply.cpp:373](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:373).  
   **Сценарий:** Masked → непрерывно тащить Opacity Clip. Каждое новое значение меняет статический ключ и вызывает `MI->PostEditChange()` без передачи признака `Interactive`. В UE это запускает `InitResources`, обновление кэшей и обход загруженных material instances в `RecacheMaterialInstanceUniformExpressions`.  
   **Величина:** при 120 изменениях и 5000 загруженных MI — около **600 000 посещений MI**, дополнительно к обновлениям render proxy. Измерять длительность `OnRecipeChanged` в Insights. Компиляцию нового shader map на каждом шаге здесь **не утверждаю**: код заранее изменяет `BasePropertyOverrides` на месте, что влияет на обнаружение изменений API.  
   **Исправление:** применять дорогие изменения base overrides после окончания drag либо с debounce; передавать в API отдельную новую структуру overrides.

4. **medium — автоисправление текстур запускает перекомпрессию при открытии и повторное обновление при Cancel.**  
   [SGlazeWindow.cpp:392](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:392), [GlazeApply.cpp:194](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:194), [SGlazeWindow.cpp:720](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:720).  
   **Сценарий:** Normal с Default compression и ORM с sRGB → открыть Glaze → Cancel. Открытие меняет реальные ассеты и вызывает `PostEditChange`; отмена возвращает настройки через второй `PostEditChange`. UE обновляет texture resource, при необходимости перестраивает derived data и уведомляет зависимые материалы. Транзакции исправления остаются в Undo и удерживают transactional-текстуры после закрытия окна.  
   **Величина:** до **двух циклов обновления на исправленную текстуру**; декодированный RGBA8-буфер одной текстуры занимает 16 МиБ при 2K, 64 МиБ при 4K, 256 МиБ при 8K. Это размер одного буфера, не полный пик компрессора. Проверять Memory Insights, `UTexture_PostEditChangeProperty` и GC-referencers после Cancel.  
   **Исправление:** по возможности откладывать исправление ассетов до Create; если оно необходимо для preview — выполнять явной операцией и корректно отменять её транзакцию вместе с Cancel.

5. **medium — импорт пака последовательно ждёт сохранения каждой карты; настройки задаются после импорта.**  
   [GlazePack.cpp:133](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:133), [GlazePack.cpp:149](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:149), [GlazePack.cpp:198](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:198).  
   **Сценарий:** впервые импортировать материал 2K → выполняются три отдельных `ImportAssetTasks`, затем настройка и сохранение каждой текстуры перед импортом следующей. В UE `UTexture2D::PreSave` ждёт завершения texture compilation.  
   **Величина:** **3 импортных прохода и 3 отдельных сохранения** на материал; 1K уменьшает число пикселей в четыре раза, но сохраняет этот overhead. **Предположение:** ORM/noise первоначально строятся с неподходящими настройками, затем перестраиваются в `FixSettings`; проверить выбранный importer/pipeline и количество DDC build jobs в Insights.  
   **Исправление:** задавать compression/sRGB в factory или Interchange pipeline до первого обновления ресурса; импортировать карты одним заданием и сохранять пакеты общей операцией.

6. **medium — группировка наборов квадратичная, с повторной Unicode-нормализацией.**  
   [GlazeMatcher.cpp:275](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMatcher.cpp:275), [GlazeMatcher.cpp:334](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMatcher.cpp:334).  
   **Сценарий:** выбрать 1000 различных наборов → до появления окна для каждой текстуры линейно ищется группа, затем каждый результат сравнивается со всеми результатами. В последнем проходе оба имени заново проходят `FText::ToLower`.  
   **Величина:** для 1000 наборов последний проход выполняет **1 млн сравнений и 2 млн вызовов `FoldCase`**, даже если все имена уникальны. Измерять `GroupTextureSets` отдельно в Insights.  
   **Исправление:** индекс групп через `TMap`; нормализованные имена и частоты совпадений вычислять один раз.

7. **low — Apply без изменений всё равно обновляет и сохраняет MI.**  
   [GlazeApply.cpp:423](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:423), [GlazeApply.cpp:426](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:426).  
   **Сценарий:** открыть сохранённый MI через Edit with Glaze → ничего не менять → Apply. Пустой локальный ключ заставляет пройти полный путь применения; затем пакет безусловно помечается dirty и сохраняется.  
   **Величина:** **одно лишнее сохранение пакета на каждый такой Apply**, плюс PostEditChange и транзакция. Время зависит от диска и source control; измерять в Insights.  
   **Исправление:** хранить исходное состояние и пропускать запись при отсутствии фактических изменений.

**Память и GC отдельно:** подтверждённых потерянных UObject, циклов владения Slate или делегатов без отписки не обнаружено. Рецепт и preview защищены `TStrongObjectPtr`, очередь и исходные материалы — `FGCObject`, текстуры рецепта — `UPROPERTY`; brush освобождает `FSlateStyleSet`. Избыточное удержание и пики описаны в пунктах 1 и 4.

Обычные scalar/vector-слайдеры при неизменном статическом ключе не вызывают перекомпиляцию: UE отсекает неизменные значения. Static switches обновляются сразу; быстрое последовательное переключение функций может создавать промежуточные shader jobs.

**UX-предложения, по пользе:**

1. Create All: прогресс, отмена между наборами и одно итоговое уведомление.
2. Показывать окно до загрузки всей очереди, загружать наборы постепенно.
3. Opacity Clip применять после drag; быстрые изменения static switches объединять коротким debounce.
4. Показывать объём предстоящих исправлений текстур до запуска перекомпрессии.