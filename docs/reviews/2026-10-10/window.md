Проверено текущее дерево на `fc2ffd7`, включая появившуюся во время ревью правку `GlazeUndoKeys.h`. Это статическое ревью со сверкой исходников UE 5.8; редактор и тесты не запускал. Ничего не менял. `critical` не обнаружено.

Новые находки:

1. **high — Пересоздание Blueprint-компонента оставляет preview после Cancel.**  
   [SGlazeWindow.cpp:700](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:700)

   `Targets` и Undo-защита хранят weak pointer старого компонента. Подписки на `OnObjectsReplaced` нет. При повторном Construction Script UE переносит `OverrideMaterials` на новый компонент, а старый уничтожает. Восстановление пропускает уничтоженный компонент и не знает про замену.

   **Воспроизведение:** Blueprint Actor со StaticMeshComponent, созданным в Components, без `SetMaterial` в Construction Script → выделить → открыть Glaze → изменить exposed-параметр актёра, запускающий Construction Script → Cancel. На новом компоненте останется `GlazePreview`; последующие изменения Glaze также перестанут обновлять этот меш.

   **Исправление:** обрабатывать замену компонентов и переносить цели вместе с оригиналами в окне и guard. Поведение переноса подтверждено в UE: `ComponentInstanceDataCache.cpp:525`, `ActorConstruction.cpp:781`.

2. **medium — Undo восстанавливает рецепт, но ломает автоматические настройки следующего набора.**  
   [SGlazeWindow.cpp:885](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:885), [SGlazeWindow.h:92](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.h:92)

   `bAutoMasked`, `bAutoEmissive` и `bFolderEdited` находятся вне транзакционного рецепта. `OnUndoRedo()` обновляет отображение, но оставляет эти признаки прежними.

   **Воспроизведение:** первый набор содержит Opacity, второй — нет → Glaze автоматически включает Masked → вручную выбрать Opaque → Undo через меню редактора → Create & Next. Первый рецепт снова Masked, но `bAutoMasked == false`; второй набор ошибочно наследует Masked. Аналогично: изменить Folder → Undo → следующий набор сохраняет папку предыдущего вместо своей.

   **Исправление:** хранить признаки пользовательского/автоматического выбора в транзакционном состоянии рецепта.

3. **medium — Undo-защита удерживает материалы после окончания их полезного времени жизни.**  
   [SGlazeWindow.cpp:117](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:117), [SGlazeWindow.cpp:132](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:132)

   `Records` очищается только при Undo/Redo. Даже когда preview или компонент уже собран GC, `AddReferencedObjects()` продолжает удерживать `Originals` устаревшей записи.

   **Воспроизведение:** открывать и отменять Glaze на разных мешах/уровнях → переключать уровни и выполнять GC → не пользоваться Undo. Записи накапливаются, исходные материалы и их зависимости остаются сильно достижимыми до следующего Undo/Redo или выгрузки модуля.

   **Исправление:** очищать устаревшие записи после GC и при завершении сессий; сохранять только записи, preview которых действительно ещё может вернуться.

4. **medium — Первый preview при Edit with Glaze пропускает автоисправление текстур.**  
   [SGlazeWindow.cpp:450](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:450)

   После `ReadRecipe()` сразу вызывается `RefreshPreview()`. В отличие от открытия текстур, `FixTextures()` не выполняется, хотя `Fix Texture Settings` включён.

   **Воспроизведение:** создать Glaze MI с Normal → сохранить у этой текстуры `Compression = Default`, sRGB включён → Edit with Glaze. Первое preview использует несовместимые настройки для Normal sampler. Изменение постороннего параметра внезапно исправляет текстуру и preview.

   **Исправление:** вызвать `FixTextures()` перед первым preview в режиме Edit, используя существующий механизм отката при Cancel.

5. **medium — Динамическая выгрузка возвращает проблему preview в Undo.**  
   [SGlazeWindow.cpp:856](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:856), [GlazeEditorModule.cpp:35](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeEditorModule.cpp:35)

   `CloseAll()` удаляет guard, но оставляет транзакционные снимки компонентов со ссылками на preview. Закрытие окна восстанавливает только текущее состояние компонентов.

   **Воспроизведение:** выделить обычный StaticMeshActor → открыть Glaze → изменить Location mesh-компонента → Cancel → динамически выгрузить `GlazeEditor`, продолжая работать в редакторе → Undo. Снимок снова назначит transient preview, а обработчика восстановления уже нет.

   **Исправление:** перед удалением guard устранять preview из сохраняющейся истории либо запрещать такую выгрузку при наличии соответствующих снимков. Обычное завершение редактора этот сценарий не затрагивает.

Проверка предыдущего отчёта: исходные сценарии новых находок №1–4 исправлены; восстановление пустого `OverrideMaterials` и обычный сброс автоматических Masked/Emissive между наборами тоже исправлены. Оставшиеся проблемы:

- **medium, прежняя №5 — частично:** [SGlazeWindow.cpp:777](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:777): удалить текстуры уже открытого текущего набора → Create All создаёт неполный/пустой MI, поскольку `bSetHasMissing` вычислен раньше; проверять доступность входов непосредственно перед созданием.
- **medium, прежний хвост №9:** [SGlazeWindow.cpp:486](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:486): вручную изменить Invert/Flip Green → Next → значения перезаписываются матчером; отделить автоматические значения от пользовательских overrides.
- **medium, прежний хвост №13:** [SGlazeWindow.cpp:348](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/SGlazeWindow.cpp:348): настроить черновик → открыть другие текстуры/MI → черновик молча теряется; сохранять черновик либо предупреждать перед заменой окна.

По памяти и GC: находка №3 — подтверждённое избыточное удержание. Дополнительно сохраняется прежний пик памяти: все выбранные текстуры загружаются сразу, обработанные и пропущенные наборы остаются в очереди до закрытия. Размер пика не измерял. `Recipe`/preview защищены `TStrongObjectPtr`, очередь и оригиналы — `AddReferencedObjects`; новых висячих сырых указателей или забытых отписок не обнаружил.

UX-предложения по пользе:

1. Защита черновика при повторном открытии другого материала.
2. Create All: прогресс, остановка и итоговая сводка; исключить обновление Details/preview и синхронизацию Content Browser для каждого набора.
3. Видимая очередь со статусами и возможностью пропустить последний набор.
4. Явный список целей и material slots для preview, с понятным поведением при смене уровня или выделения.