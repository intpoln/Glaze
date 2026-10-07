**Glaze я бы строил вокруг небольшого семейства master materials и быстрого создания MIC.** Генерацию самостоятельного графа добавил бы как отдельный режим экспорта. Главная ценность продукта — уверенно сопоставить текстуры, сразу показать результат и сократить повторяющиеся настройки.

**1. Архитектура**

| Вариант | Сильные стороны | Ограничения |
|---|---|---|
| **A. Мастера + static switches + MIC** | Быстрое создание, единые параметры, удобное повторное редактирование | Новые комбинации switches требуют компиляции; экземпляры зависят от мастеров |
| **B. Генерация настоящего Material** | Только выбранные модули; доступный художнику граф | Каждый материал нужно собирать, компилировать и сопровождать; обновления библиотеки сложнее |
| **C. Гибрид** | MIC для ежедневной работы, самостоятельный Material для передачи и доработки | Два способа создания должны давать одинаковый результат |

**Рекомендация: A для MVP, C как развитие продукта.**

- Начать с `M_Glaze_Surface_Opaque` и `M_Glaze_Surface_Masked`.
- Позже добавить отдельные семейства Foliage, Decal и Translucent.
- Wood и Metal обычно отличаются настройками и текстурами; отдельный мастер для каждого не нужен.
- Fabric может требовать Cloth, Foliage — Two Sided Foliage: такие пресеты меняют и модель шейдинга.
- Decal требует отдельного material domain и своей логики выходов.
- Общие операции держать в библиотеке `MF_Glaze_*`: UV, Color, Remap, PackedMaps, Detail, WPO.
- Интерфейс хранит **рецепт материала**: текстуры, каналы, модули, параметры, семейство и версию схемы.
- Генератор самостоятельного графа использует тот же рецепт.

**Перестановки и компиляция**

- Static switches использовать для дорогих ветвей: triplanar, detail normal, noise, WPO, дополнительных texture samples.
- Tint, brightness, remap и UV регулировать scalar/vector parameters.
- Нулевой scalar или `Lerp` не гарантирует удаления дорогой ветви из шейдера.
- `N` независимых переключателей допускают до `2ᴺ` комбинаций, но движок не обязательно компилирует их все заранее.
- Реальная стоимость зависит от используемых комбинаций, платформ, quality levels и material usages.
- MIC с одинаковой статической конфигурацией обычно переиспользуют соответствующие шейдеры.
- Смена обычных значений не требует новой статической перестановки; смена switches может потребовать её. [Epic: static permutations](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Materials/UMaterialInstance?application_version=5.5)
- Изменения переключателей применять после небольшой паузы или кнопкой **Apply Features**.
- Не обещать мгновенный preview при первой компиляции новой конфигурации.
- Генерация компактного графа уменьшает ненужные ветви, но создание отдельных Materials не гарантирует сокращения общего времени компиляции.

**Legacy, Substrate и версии**

- Для общей базы UE 5.4–5.8 выбрать legacy PBR с Default Lit.
- Substrate предоставить отдельным backend с проверкой настроек проекта.
- В 5.4 Substrate был Experimental; в 5.7 получил Production-Ready. Поэтому единый Substrate-only продукт для всего диапазона усложнит поддержку. [Epic: 5.4](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5.4-release-notes?application_version=5.4), [Epic: 5.7](https://www.unrealengine.com/news/unreal-engine-5-7-is-now-available)
- Не переключать Substrate в проекте автоматически.
- Базовые `.uasset` авторить в 5.4; новые возможности выпускать отдельными версиями контента.
- Совместимость ассетов направлена вперёд: сохранённый в 5.8 материал нельзя считать открываемым в 5.4.
- Открытие старого ассета новой версией ещё не подтверждает одинаковый рендер. Нужны проверки каждой поддерживаемой версии. [Epic: asset versioning](https://dev.epicgames.com/documentation/unreal-engine/versioning-of-assets-and-packages-in-unreal-engine)

**Packaging и сопровождение**

- Editor UI и скрипты исключать из runtime-зависимостей.
- Используемые мастера, функции и текстуры должны попадать в cook по ссылкам.
- Отключение плагина с мастерами нарушит зависимости его MIC.
- Предусмотреть **Copy Dependencies to Project**.
- «Standalone Material» должен копировать необходимые функции либо раскрывать их граф; иначе зависимость от Glaze сохранится.
- Обновления мастеров версионировать: изменение общей функции может затронуть множество материалов и вызвать перекомпиляцию.
- Для C++-версии потребуется сборка editor module под поддерживаемые версии UE.

**2. Можно ли без C++**

**Да: рабочий MVP с контекстным действием, EUW, сопоставлением текстур и созданием MIC реализуем без C++.** Полноценный встроенный preview viewport и управление большими batch-операциями заметно удобнее делать через editor module.

| Задача | Конкретные классы / функции | Граница |
|---|---|---|
| Действие на текстурах | `UAssetActionUtility`, `SupportedClasses = UTexture2D` | Стандартное размещение — подменю Scripted Asset Actions |
| Получение selection | `UEditorUtilityLibrary::GetSelectedAssetData`, `GetSelectedAssets` | Зафиксировать selection при запуске окна |
| Окно инструмента | `UEditorUtilityWidget`, `UEditorUtilitySubsystem::SpawnAndRegisterTab` | Это вкладка; её можно отделить |
| Создание ассетов | `UAssetToolsHelpers::GetAssetTools`, `IAssetTools::CreateAsset`, `CreateUniqueAssetName` | Через соответствующую factory |
| Создание MIC | `UMaterialInstanceConstantFactoryNew`, `UMaterialInstanceConstant` | Для BP можно также дублировать шаблон MIC |
| Создание Material | `UMaterialFactoryNew`, `UMaterial` | Граф собирается отдельно |
| Параметры MIC | `UMaterialEditingLibrary::SetMaterialInstanceParent`, `SetMaterialInstanceTextureParameterValue`, `SetMaterialInstanceScalarParameterValue`, `SetMaterialInstanceVectorParameterValue` | Editor API; сохраняет настройки постоянного instance |
| Static switches | `SetMaterialInstanceStaticSwitchParameterValue`, `UpdateMaterialInstance` | Изменение статической конфигурации может запустить компиляцию |
| Генерация графа | `CreateMaterialExpression`, `ConnectMaterialExpressions`, `ConnectMaterialProperty`, `LayoutMaterialExpressions`, `RecompileMaterial` | BP/Python доступны; Python удобнее для сложной сборки |

Контекстные действия и запуск EUW подтверждены документацией Epic. [Scripted Actions](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripted-actions-in-unreal-engine), [EditorUtilitySubsystem](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/Blutility/UEditorUtilitySubsystem)

**Важная деталь:** документация UE 5.4 указывает `SetMaterialInstanceStaticSwitchParameterValue` как BlueprintCallable. Утверждение «static switches всегда требуют C++» здесь неверно. Однако сигнатуры и обновление instance нужно проверить на каждой целевой версии. [MaterialEditingLibrary 5.4](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/MaterialEditor/UMaterialEditingLibrary?application_version=5.4)

**Текстуры и sampler type**

- Определение назначения: имя → правила экспортёра → текущие texture settings → ручное подтверждение.
- Движок не предоставляет универсального достоверного классификатора «это Roughness».
- Настройки текстуры: `UTexture::SRGB`, `CompressionSettings`; в Python — `srgb`, `compression_settings`.
- Из BP использовать **Set Editor Property** / `UKismetSystemLibrary::SetEditorProperty`; из Python — `set_editor_property`.
- Для изменения свойств нужны корректные уведомления редактора и проверка успешности операции. [Epic: SetEditorProperty](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Kismet/UKismetSystemLibrary/SetEditorProperty?application_version=5.5)
- BaseColor: обычно sRGB + Color sampler.
- Normal: linear + Normalmap compression + Normal sampler.
- ORM и маски: linear + Masks compression/sampler; отдельные grayscale-карты могут использовать другую согласованную пару.
- Sampler type принадлежит **узлу графа**, а не параметру MIC.
- При генерации задаётся `UMaterialExpressionTextureSample::SamplerType`; Python-свойство — `sampler_type`.
- В мастерах sampler types подготовлены заранее; Glaze проверяет совместимость назначенной текстуры.
- Virtual Textures требуют соответствующих virtual sampler types.
- Исправление compression/sRGB меняет общий texture asset и может затронуть другие материалы. Показывать изменения до применения. [Texture2D](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/Texture2D?application_version=5.4), [TextureSample](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialExpressionTextureSample?application_version=5.4)

**Сохранение и дополнительные зависимости**

- `UEditorAssetLibrary::SaveLoadedAsset/SaveLoadedAssets` — сохранять только созданные или изменённые ассеты.
- `SetMetadataTag/GetMetadataTag` — хранить editor-рецепт и версию Glaze. [Epic: EditorAssetLibrary](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/EditorScriptingUtilities/UEditorAssetLibrary)
- Потребуются Editor Utility Widgets и необходимые editor scripting возможности; Python Editor Script Plugin — только при использовании Python.
- Прямой пункт меню возможен и через Python `unreal.ToolMenus`: `get`, `extend_menu`, `ToolMenuEntryScript`. Это добавляет регистрацию при старте и зависимость от Python. [Epic: ToolMenus](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ToolMenus?application_version=5.6)
- Настоящее модальное Slate-окно: `SWindow`, `FSlateApplication::AddModalWindow`; для него практичнее C++. [Epic: AddModalWindow](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Slate/FSlateApplication/AddModalWindow)
- Для основного рабочего окна я бы выбрал **немодальную вкладку**: можно продолжать выбирать текстуры и объекты.

**Где C++ даст существенный выигрыш**

- `UToolMenus` + `UContentBrowserAssetContextMenuContext` — собственное меню с корректным контекстом selection.
- `SEditorViewport`, `FEditorViewportClient`, `FAdvancedPreviewScene` — управляемый preview с мешами и освещением.
- `FScopedTransaction`, `FEditorUndoClient` — транзакции и реакция интерфейса на Undo/Redo.
- `FScopedSlowTask` — прогресс и отмена batch между этапами.
- Доступ к исходным пикселям текстур, качественный packing и расширенная диагностика.
- **Для самой PBR-математики, обычных MIC и библиотеки material functions C++ не нужен.**

**3. UX**

**Один экран, минимум обязательных решений**

| Область | Содержимое |
|---|---|
| Верх | Имя набора, preset, семейство материала |
| Слева | Texture slots: thumbnail, asset, channel, статус совпадения |
| Центр | Preview: sphere / plane / cube / выбранный mesh |
| Справа | Модули с раскрывающимися настройками |
| Низ | Папка, итоговое имя, предупреждения, Create / Apply / Cancel |

- По умолчанию открывать **Textures + Basic**; дополнительные параметры — по раскрытию.
- Для каждого назначения показывать причину: «суффикс `_N`», «правило Substance», «назначено вручную».
- Неоднозначные совпадения выделять; создавать материал можно после явного выбора.
- Drag-and-drop, asset picker, очистка слота, preview канала, кнопка «найти в Content Browser».
- Пустой слот допускает константу. Отсутствие Metallic или AO не является ошибкой.
- Безопасные значения: Metallic = 0, AO = 1, Normal = плоская нормаль.
- Текстура Height может быть назначена заранее; интерфейс явно показывает, используется ли она.

**Правила сопоставления**

| Слот | Примеры суффиксов |
|---|---|
| BaseColor | `_BC`, `_BaseColor`, `_Albedo`, `_D`, `_Diffuse` |
| Normal | `_N`, `_Normal`, `_NRM`; `_NormalGL` предлагает Flip Green |
| Roughness | `_R`, `_Rough`, `_Roughness` |
| Metallic | `_M`, `_Metal`, `_Metallic`, `_Metalness` |
| AO | `_AO`, `_Occlusion`, `_AmbientOcclusion` |
| Height | `_H`, `_Height`, `_Displacement` |
| Emissive | `_E`, `_Emissive`, `_Emission` |
| Opacity | `_O`, `_Opacity`, `_Alpha`, `_Mask` — неоднозначные варианты требуют проверки |
| Specular | `_S`, `_Spec`, `_Specular` — сверять workflow экспортёра |
| Packed | `_ORM`, `_ARM`, `_RMA`, `_MRA`, пользовательские схемы |

- Искать целые токены без учёта регистра; длинные точные суффиксы имеют приоритет.
- Учитывать префикс `T_`, служебные токены разрешения и формат экспортёра.
- Сохранять значимые номера варианта: `Wood_01` и `Wood_02` — разные наборы.
- ORM/ARM обычно означают RGB = AO, Roughness, Metallic; RMA — Roughness, Metallic, AO.
- Всегда показывать явную раскладку каналов: название упаковки не гарантирует содержимое.
- Gloss/Smoothness предлагать как инвертированную Roughness.
- Отдельная карта и packed-канал могут существовать одновременно; источник каждого свойства выбирается отдельно.
- Alpha BaseColor назначать в Opacity только по правилу preset/exporter либо вручную.

**Пресеты**

- **Wood:** dielectric, умеренная roughness, UV и tint.
- **Metal:** metallic workflow, roughness controls; предупреждение при неполном наборе.
- **Fabric:** dielectric; detail normal, Cloth — отдельная опция семейства.
- **Foliage:** Masked, two-sided, subsurface tint; wind включается явно.
- **Decal:** отдельный domain; выбор воздействий на цвет, нормаль и roughness.
- Пресет устанавливает стартовые параметры и модули; обнаруженные карты сохраняют приоритет.
- Пользовательские пресеты и правила экспортёров — важнее большого числа встроенных «типов поверхности».

**Preview, batch и повторное открытие**

- Preview работает на временной копии MIC; исходный ассет меняется только через Apply.
- MID подходит для быстрых scalar/vector изменений, но не заменяет MIC для проверки static switches.
- Фиксированная экспозиция, несколько lighting presets, A/B и просмотр отдельных карт.
- BP-прототип: отдельная preview-сцена с SceneCapture; полноценный editor viewport — следующий этап.
- Batch: сначала таблица распознанных наборов, затем создание по очереди.
- Дать общий preset, настройки отдельных строк, Skip и повтор только ошибочных наборов.
- Загружать подробный preview только активной строки.
- Имена: `MI_Glaze_Wood_01`, варианты — `MI_Glaze_Wood_01_Dark`.
- По умолчанию сохранять рядом с набором в `Materials`; поддержать `/Game/Materials/Glaze`.
- При конфликте имени: Update / Create Variant / Skip, без молчаливой перезаписи.
- **Edit with Glaze** доступно для MIC с распознанным мастером или метаданными.
- При открытии читать реальные параметры, включая наследование и overrides; рецепт помогает восстановить намерение.
- Если пользователь менял MIC вручную, предложить синхронизацию вместо восстановления устаревшего рецепта.

**Undo**

- Изменения существующего MIC оформлять одной транзакцией Apply.
- BP: `UKismetSystemLibrary::BeginTransaction`, `TransactObject`, `EndTransaction`.
- Каждое изменяемое UObject зарегистрировать до изменения. [Epic: transactions](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UKismetSystemLibrary/BeginTransaction?lang=en-US)
- Undo параметров и удаление уже сохранённых файлов — разные операции.
- Для batch предусмотреть собственный откат созданных ассетов; Cancel не должен оставлять скрытые изменения.

**4. Функции по этапам**

| Этап | Состав |
|---|---|
| **MVP** | Auto-match; ручное назначение каналов; Opaque/Masked; ORM/ARM/RMA; MIC; базовый batch; повторное открытие; preview; проверки текстур |
| **MVP: Color** | Desaturation, Contrast, Brightness, Tint, Lerp to Color |
| **MVP: PBR** | Bias, contrast, min/max, invert для Roughness/Metallic/AO/Specular; normal strength; emissive tint/intensity |
| **MVP: UV** | Tiling, offset, rotation, UV channel |
| **v2: Color** | Hue shift, Overlay color, Color-to-Lerp с tolerance/softness и просмотром маски |
| **v2: Surface** | Detail normal, macro variation, texture noise, world/local mapping, triplanar |
| **v2: Motion** | Wind/wobble, ограниченный noise WPO, DitherTemporalAA, distance/camera fade |
| **v2: Workflow** | Foliage/Decal, пользовательские presets, генерация самостоятельного Material |
| **Позже** | Curve remap через LUT, procedural 3D noise, POM, отдельный Nanite displacement workflow, Substrate layers, translucency |

- Для Color-to-Lerp по умолчанию строить маску из исходного BaseColor: последующая цветокоррекция не должна неожиданно менять выделение.
- Разделить **input range** и **output range** в remap; фиксировать порядок операций.
- Curve remap реализовывать через LUT/Curve Atlas либо ограниченное приближение; `CurveFloat` напрямую в shader не исполняется.
- Wind нужен anchor mask: Vertex Color, texture channel или маска высоты относительно pivot.
- Per-instance random: `PerInstanceRandom` для ISM/HISM; обычным мешам нужен явный seed или Custom Primitive Data.
- Модули должны открывать только связанные параметры.

**Дополнительные идеи, которые экономят время**

- **Fix Texture Settings**: список предлагаемых исправлений compression, sRGB, VT и normal orientation.
- **Packing Inspector**: RGB/A отдельно, проверка размеров и потери alpha; семантику каналов подтверждает пользователь.
- **Quick Variants**: Dry/Wet, Clean/Dirty, Light/Dark через overrides дочерних MIC.
- **Replace Texture Set**: заменить набор карт, сохранив художественные настройки.
- **Apply to Selected**: preview на выбранных мешах с восстановлением прежних назначений.
- **Copy Look**: перенести коррекцию, remap и UV на другой материал.
- Профили Substance, Megascans и пользовательского naming convention.
- Индикатор стоимости: samples, наличие WPO/triplanar, static configuration и реальные shader statistics после компиляции.
- Параметры, предназначенные для Sequencer: scalar/vector с понятными именами и единицами.

**5. Риски и ловушки**

- **Один мегамастер:** тяжёлый UI, слишком много комбинаций и широкое влияние обновлений.
- **Material functions:** улучшают сопровождение, но сами по себе не уменьшают shader cost.
- **Nanite:** Opaque/Masked поддерживаются; Translucent не поддерживается. WPO требует ограничений displacement и проверки culling. [Epic: Nanite](https://dev.epicgames.com/documentation/it-it/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)
- **WPO:** меняет вершины; качество зависит от геометрии. Это не универсальная замена tessellation.
- **Lumen:** Software Ray Tracing по distance fields не отражает WPO-деформацию; возможны расхождения освещения. [Epic: Lumen limitations](https://dev.epicgames.com/documentation/es-es/unreal-engine/lumen-technical-details-in-unreal-engine)
- **Material AO:** эффект зависит от renderer/GBuffer; для описанного Epic Lumen workflow требуется отключённый Allow Static Lighting. Glaze должен диагностировать настройку. [Epic: Material AO](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine)
- **Dithered Masked:** зависит от temporal reconstruction; shimmer, ghosting и поведение MRQ/Path Tracer нужно проверять отдельно.
- **World-aligned:** может «плыть» при движении объекта; local-space mapping чувствителен к scale и трансформациям.
- **Triplanar normals:** требуют правильного преобразования нормалей и смешивания проекций.
- **UV rotation:** для normal maps нужно корректировать ориентацию tangent-space нормали.
- **Noise displacement:** vertex-stage sampling требует корректного mip level; учитывать bounds, velocity и shadow cost.
- **Height:** POM, WPO и Nanite displacement — разные технологии с разными ограничениями.
- **Автоисправления текстур:** затрагивают общие ассеты; предусмотреть исправление копии.
- **Packing:** объединение карт с разными UV, разрешением или требованиями к compression может ухудшить результат.
- **Preview:** сфера не выявляет все проблемы масштаба, UV, foliage и движения камеры.
- **Скорость:** измерять cold/warm DDC отдельно, одиночное создание и batch; время shaders показывать отдельно от времени работы Glaze.
- **Критерий MVP:** обычный согласованно названный набор должен превращаться в пригодный MIC за один просмотр и одно подтверждение; спорные назначения должны быть видны до создания.
