Нашёл **3 medium и 1 low** новых находки. Critical/high не обнаружил. Проверка статическая: текущий код, git history, исходники UE 5.8 и воспроизведение алгоритма матчера в памяти. Редактор не запускал, PNG не открывал, файлы не менял.

**Новые находки**

1. **medium — одиночные `ORM`, `ARM`, `AO`, `BC` ошибочно становятся цветными картами.**  
   [GlazeMatcher.cpp:169](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMatcher.cpp:169), fallback — строка 251.

   Точное имя канала принимается только при длине ≥4. Ограничение исключает однозначные сокращения и packed-карты, хотя соответствующие правила существуют.

   **Воспроизведение:** в `/Game/Textures/Oak` выделить `Base_Color`, `Normal_GL`, `ORM` → первые две текстуры образуют набор `Oak`, а `ORM` — отдельный набор с картой в **BaseColor** вместо AO/Roughness/Metallic. Автоисправление настроек может включить для неё sRGB/Default. Аналогично работает одиночный `T_ORM_2K` после удаления префикса и разрешения.

   **Исправление:** разрешить точные многобуквенные сокращения, особенно packed-карты; неоднозначные одиночные `N`, `R`, `M` обрабатывать отдельно.

2. **medium — уменьшение до 512/256 искажает линейные данные Normal/ORM/noise.**  
   [GlazePack.cpp:72](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:72).

   `LoadImage` помечает 8-битные изображения как sRGB. `ResizeTo` получает эту гамму и фильтрует каналы с преобразованием sRGB. Затем `FixSettings` объявляет полученные байты линейными — исходные значения уже изменены.

   Это подтверждается [IImageWrapperModule.h:41](D:/EGL/UE_5.8/Engine/Source/Runtime/ImageWrapper/Public/IImageWrapperModule.h:41) и выбором sRGB-фильтра в [ImageCore.cpp:1853](D:/EGL/UE_5.8/Engine/Source/Runtime/ImageCore/Private/ImageCore.cpp:1853).

   **Воспроизведение:** впервые импортировать материал пака в 512/256 → ORM и нормаль уменьшаются как цветные изображения. При смешении равных долей значений 0/1 sRGB-фильтрация даёт примерно 0,735 вместо линейных 0,5: меняются roughness/AO и компоненты нормали. Для уменьшенного noise аналогично меняются значения маски.

   **Исправление:** до ресайза определять назначение карты; для `_N`, `_ORM`, `T_Noise_*` устанавливать `Image.GammaSpace = EGammaSpace::Linear` **без преобразования байтов**. BaseColor оставлять sRGB.

3. **medium — DX-нормали пака наследуют переворот зелёного канала из проектного Interchange pipeline.**  
   [GlazePack.cpp:89](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:89), импорт — строка 163.

   Импорт использует стандартный pipeline проекта. `FixSettings` задаёт Normalmap/linear, но не сбрасывает `bFlipGreenChannel`. UE применяет `bFlipNormalMapGreenChannel` при импорте нормалей: [InterchangeGenericTexturePipeline.cpp:697](D:/EGL/UE_5.8/Engine/Plugins/Interchange/Runtime/Source/Pipelines/Private/InterchangeGenericTexturePipeline.cpp:697).

   **Воспроизведение:** назначить проекту Interchange pipeline с включённым **Flip Normal Map Green Channel** → впервые импортировать, например, `Wood_Planks` → его DX-нормаль получает `bFlipGreenChannel=true`. Матчер `_N` дополнительный переворот не назначает; рельеф освещается с неправильным направлением Y. Повторный импорт сохраняет эту настройку.

   **Исправление:** для новых DX-нормалей пака явно задавать `bFlipGreenChannel=false`; учитывать это поле в условии изменения настроек. Лучше задавать параметры до первой сборки текстуры.

4. **low — новый путь ресайза полностью декодирует PNG даже для штатных 1K/2K.**  
   [GlazePack.cpp:67](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:67).

   Проверка `Image.SizeX <= Size` выполняется после полного `LoadImage`. Затем исходный файл передаётся импортёру, который снова декодирует его.

   **Воспроизведение:** первый импорт любого материала в 2K → каждый из BC/N/ORM сначала полностью распаковывается для проверки размера и освобождается, затем импортируется. Для BGRA8 2K это дополнительные 16 MiB временных пикселей на файл и лишняя работа на основном потоке.

   **Исправление:** для поставляемых 1024/2048 сразу возвращать исходный файл; декодировать для ресайза только 512/256.

**Проверка предыдущего отчёта**

- `2K/2048`: исправлено корректно; одинаковый размер получает общий ключ. Аналогично работают `1024/1K`, `4096/4K`, `8192/8K`.
- `Base_Color`/`Normal_GL`: исправлено корректно; составные точные имена оставляют пустой `SetName` и группируются по папке. Новая граница исправления — короткие точные имена, находка №1.
- Turkish `I`: прежний сценарий исправлен ASCII-преобразованием до ICU. Группировка кириллицы сохранена.
- Отсутствующий `pack.json`: исправлено; неуспешное чтение/разбор больше не закрепляет пустой кеш.
- Разделение по папкам/разрешениям, приклеенные суффиксы, отсутствие двойного GL-flip, сообщения о частичном импорте и повтор сохранения нового несохранённого ассета сохранены.
- **medium, прежняя находка — не исправлено:** [GlazeMatcher.cpp:42](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMatcher.cpp:42): `Leaves_AlbedoTransparency` заполняет только BaseColor; альфа не назначается в Opacity — требуется чтение A-канала цветной текстуры.

**20 проверенных имён**

Примеры по соглашениям экспортов Quixel, Poly Haven, ambientCG и Substance; идентификатор Quixel условный. Проверяется имя UE-ассета без расширения. `∅` означает группировку по папке.

| Имя | Набор / разрешение | Результат |
|---|---|---|
| `T_vg5ncie_4K_Albedo` | `vg5ncie / 4k` | BaseColor |
| `T_vg5ncie_4K_N` | `vg5ncie / 4k` | Normal |
| `T_vg5ncie_4K_ORM` | `vg5ncie / 4k` | AO.R, Roughness.G, Metallic.B |
| `aerial_rocks_02_diff_4k` | `aerial_rocks_02 / 4k` | BaseColor |
| `aerial_rocks_02_nor_gl_4k` | `aerial_rocks_02 / 4k` | Normal, flip G¹ |
| `aerial_rocks_02_rough_4k` | `aerial_rocks_02 / 4k` | Roughness |
| `Concrete030_2K_Color` | `Concrete030 / 2k` | BaseColor |
| `Concrete030_2048_NormalDX` | `Concrete030 / 2k` | Normal |
| `Concrete030_2K_NormalGL` | `Concrete030 / 2k` | Normal, flip G¹ |
| `Concrete030_2K_AmbientOcclusion` | `Concrete030 / 2k` | AO |
| `MetalPlates006_1K_Metalness` | `MetalPlates006 / 1k` | Metallic |
| `Wood_Floor01BaseColor` | `Wood_Floor01` | BaseColor |
| `Wood_Floor01Normal` | `Wood_Floor01` | Normal |
| `Fabric01_Gloss` | `Fabric01` | Roughness, invert |
| `Glass01_Opacity` | `Glass01` | Opacity |
| `Panel01_Emissive` | `Panel01` | Emissive |
| `WoodFloor01_Height` | `WoodFloor01` | Height |
| `Base_Color` | `∅` | BaseColor |
| `Normal_GL` | `∅` | Normal, flip G¹ |
| `ORM` | `ORM` | Не распознано; одиночный набор → BaseColor |

¹ Дополнительный flip отключается, если текстура уже имеет `bFlipGreenChannel=true`.

Дополнительно: `Concrete` и `Conc_N` не смешиваются; `Normal_Concrete01_BaseColor` сохраняет внутренний `Normal` в имени набора. `t_Камень__02_aRm_4k` и `T_камень_02_N_4096` дают общий набор `Камень_02`, размер `4k`: цифры сохраняются, повторные подчёркивания схлопываются.

**Импорт и поставка**

- Manifest и папки совпадают: **18 × 2 × 3 + 21 × 2 = 150 PNG**, отсутствующих и лишних файлов нет. В `packages/5.8/Glaze` также 150 PNG и совпадающий manifest.
- Повторный импорт использует существующие текстуры без изменения настроек. Удалённая карта импортируется заново из исходника.
- Ошибки записи возвращают неуспех; новый ассет без `.uasset` допускает повторное сохранение.
- `GetBaseDir()` берёт каталог `.uplugin`, поэтому путь корректен для Project, Engine и Marketplace Plugins. `BuildPlugin` включает `/Resources/...`.
- Создаются стандартные `UTexture2D` в `/Game/Glaze/Pack`; runtime-зависимости от плагина на этом участке нет.

**Память и GC**

Подтверждённых утечек, опасных долгоживущих сырых указателей и делегатов без отписки на участке не обнаружил. Очередь окна удерживает текстуры через `AddReferencedObjects`; импорт синхронный.

Новый лишний расход памяти — находка №4. Ранее отмеченная повторная сборка после импорта остаётся при изменении Compression/SRGB в [GlazePack.cpp:106](C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:106); величину дополнительного пика не измерял.

**UX-предложения — по пользе**

1. Показывать конфликтующие карты одного слота и давать выбрать кандидата.
2. Для карт без разрешения при нескольких наборах предлагать привязку вместо назначения первому.
3. Объединить импорт BC/N/ORM в один прогресс с итоговой сводкой.
4. Показывать причину недоступности пака в меню, включая отсутствующий manifest.