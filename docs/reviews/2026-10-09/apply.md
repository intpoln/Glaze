Проверил код и соответствующие реализации UE 5.8. Это статическое ревью: редактор и тесты не запускал. Файлы не менял. Находок уровня **critical** нет.

**Находки**

1. **high — повреждённый рецепт позволяет записать за пределами `FLinearColor`.**  
   [GlazeRecipe.cpp:74](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeRecipe.cpp:74), [GlazeApply.cpp:64](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:64).  
   `FromJson` не валидирует enum. UE-конвертер принимает числовое значение enum без проверки диапазона, а `Component()` индексирует массив без bounds check.  
   **Воспроизведение:** JSON с `"RoughnessChannel":255` → `FromJson` принимает значение → `ApplyRecipe` вызывает `ChannelMask` → запись за пределами массива, неопределённое поведение. Текстура Roughness для этого даже не нужна.  
   **Исправление:** проверять все enum после десериализации; перед индексацией отдельно ограничить канал диапазоном `R…A`. Десериализовать во временный рецепт, чтобы ошибка не оставляла частично изменённый объект.

2. **high — исправленные в preview текстуры не сохраняются вместе с инстансом.**  
   [GlazeApply.cpp:360](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:360).  
   `ToSave` получает только текстуры, изменённые *этим* вызовом `FixTextureSettings`. Но окно уже исправляет их в `StartSet` и при смене слота (`SGlazeWindow.cpp:283,303`). Повторный вызов возвращает пустой список.  
   **Воспроизведение:** сохранённая ORM имеет `TC_Default`, sRGB включён → открыть Glaze → preview исправляет текстуру → Create сохраняет только MI → перезапустить редактор без Save All → ORM возвращается к прежним настройкам, сохранённый MI получает несовместимый Masks sampler.  
   **Исправление:** отслеживать пакеты, изменённые Glaze за весь сеанс, и сохранять их при Create; альтернативно переносить исправления исходных ассетов на этап записи.

3. **high — ошибка сохранения выдаётся за успешное создание/обновление.**  
   [GlazeApply.cpp:394](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:394).  
   Возвращаемый `bool` от `SavePackages` игнорируется; функция всегда возвращает MI. Окно вследствие этого показывает успех и продолжает батч либо закрывается.  
   **Воспроизведение:** обновить MI в недоступном для записи пакете, отменить необходимый checkout или получить ошибку диска → Glaze сообщает `Updated`, хотя на диске осталось старое содержимое.  
   **Исправление:** проверять результат сохранения, заполнять `OutError`, сохранять возможность повторной попытки. При частичном сохранении явно сообщать, какие пакеты остались несохранёнными.

4. **high — undo инстанса не отменяет JSON рецепта.**  
   [GlazeApply.cpp:391](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:391).  
   `MI->Modify()` захватывает состояние MI, но JSON пишется отдельно в пакетный `FMetaData`. В UE 5.8 это обычная структура; `SetValue` не участвует в транзакции, а `UPackage::Serialize` не сериализует её для undo.  
   **Воспроизведение:** сохранить рецепт A → через Glaze обновить до B → Undo → параметры MI возвращаются к A, метаданные остаются B → повторное Edit with Glaze открывает B; следующий Update снова уничтожает отменённые настройки.  
   **Исправление:** обеспечить транзакционное восстановление метаданных через editor-only механизм undo/redo, синхронный с транзакцией MI.

5. **high — переименование текстуры ломает повторное редактирование рецепта.**  
   [GlazeRecipe.cpp:62](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeRecipe.cpp:62), [GlazeApply.cpp:405](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:405).  
   Текстуры сохраняются строковыми путями внутри JSON. Обновление ссылок UE при rename не переписывает содержимое этих строк. Неразрешённая ссылка при импорте превращается в `nullptr`; `FromJson` при этом может вернуть успех.  
   **Воспроизведение:** создать MI → переименовать BaseColor → выполнить Fix Up Redirectors → перезапустить редактор → Edit with Glaze → слот пуст, хотя действующая ссылка MI указывает на переименованную текстуру → Update удаляет override и отключает карту.  
   **Исправление:** восстанавливать текстурные назначения из текущих параметров MI, сопоставляя каналы через Packed Map; для неразрешённых путей выдавать предупреждение и не удалять действующие назначения автоматически.

6. **medium — неактивные слоты мешают исправлению активных текстур и меняют чужие ассеты без необходимости.**  
   [GlazeApply.cpp:96](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:96), [GlazeApply.cpp:127](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:127).  
   `CollectUses` учитывает Emissive даже при выключенном Emissive или использовании BaseColor, а Opacity — при Opaque. Реальное использование в `ApplyRecipe` отличается.  
   **Воспроизведение:** одна текстура с `TC_Default` назначена в Roughness и оставлена в выключенном Emissive → обнаруживается конфликт Color/Masks → исправление пропускается → активная Roughness не компилируется с Masks sampler. Отдельная неактивная Emissive-текстура также может получить изменения compression/sRGB/VT.  
   **Исправление:** единообразно вычислять активные назначения для Apply, планирования исправлений и packed-выборки. Учитывать, что BaseColor может читаться отдельно для opacity/emissive.

7. **low — статический ключ теряет точность `OpacityClip`.**  
   [GlazeApply.cpp:323](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:323).  
   Ключ округляет float до четырёх знаков, но override хранит исходное значение.  
   **Воспроизведение:** Masked, применить `0.33331`, затем `0.33332` с тем же ключом → ключ не меняется → `BasePropertyOverrides.OpacityMaskClipValue` остаётся прежним. При сохранении с пустым ключом применяется новое значение, поэтому preview и результат расходятся.  
   **Исправление:** сравнивать структурированное статическое состояние с точным значением float либо одинаково нормализовать значение и ключ.

**Память и GC**

8. **medium — каждый статический апдейт создаёт множество временных UObject.**  
   [GlazeApply.cpp:336](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:336).  
   Цикл вызывает библиотечный setter **24 раза**. В UE 5.8 каждый вызов, даже с `bUpdateMaterialInstance=false`, создаёт `UMaterialEditorInstanceConstant`; его `SetSourceInstance` создаёт ещё preview-MI и через `RegenerateArrays` создаёт объекты параметров. Также выполняется `UpdateStaticPermutation` из деструктора update context.  
   **Воспроизведение:** несколько раз переключать Color Correction либо создавать большой батч → повторные построения editor-прокси, лишние аллокации и мусор до GC. Это подтверждённая лишняя работа; величину пика и задержки нужно измерить в редакторе.  
   **Исправление:** писать switches через прямой EditorOnly-setter и выполнять один финальный update; необходимую подготовку Material Layers проводить один раз.

9. **low — выключенная Opacity остаётся жёсткой зависимостью инстанса.**  
   [GlazeApply.cpp:260](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeApply.cpp:260).  
   Opacity-текстура записывается независимо от Blend Mode, тогда как Emissive и Detail очищаются при отключении.  
   **Воспроизведение:** назначить отдельную большую Opacity-карту → переключить Masked в Opaque → сохранить → `Opacity Map` продолжает удерживать текстуру, хотя материал её не использует.  
   **Исправление:** исключать неактивную Opacity из packed-поиска и удалять её override; выбранный слот можно оставить в рецепте.

Подтверждённых постоянных утечек и опасных долгоживущих сырых указателей на этом участке не нашёл. Текстуры рецепта защищены `UPROPERTY/TObjectPtr`; окно удерживает рецепт через `TStrongObjectPtr`. Подписки используют `AddSP` и снимаются. Сырые указатели `FTextureFix` используются синхронно при живом рецепте.

**Сверка с мастером**

Имена и типы параметров совпадают в обе стороны:

| Тип | Мастер | Apply |
|---|---:|---:|
| Scalar | 52 | 52 |
| Vector | 12 | 12 |
| Texture | 10 | 10 |
| Static switch | 24 | 24 |

Height намеренно не используется согласно roadmap. Обычные EditorOnly-сеттеры обновляют render proxy напрямую; удаление texture override корректно сопровождается полным update. Старые рецепты с отсутствующими новыми полями сохраняют дефолты при чтении в свежий объект.

**UX-предложения, по пользе**

1. Применять изменения исходных текстур при Create/Update; preview строить без необратимого для Cancel изменения ассетов.
2. Показывать отсутствующие текстуры и предлагать восстановление из текущего MI вместо молчаливого очищения слотов.
3. Различать «создан в редакторе» и «успешно сохранён»; при ошибке сохранять окно и доступную повторную попытку.