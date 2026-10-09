Нашёл **3 новые medium-находки**. Critical/high на этом участке не обнаружил. Проверка статическая: код, история исправлений, исходники UE 5.8 и воспроизведение алгоритма матчера в памяти. Редактор и импорт не запускал; PNG не открывал; файлы не менял.

**Новые находки**

1. **medium — одинаковое разрешение в разных обозначениях разделяет материал на неполные наборы.**  
   [GlazeMatcher.cpp:117](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMatcher.cpp:117), также строка 301.

   `Resolution` хранит исходный токен: `2k` и `2048` считаются разными значениями, хотя обозначают одно разрешение. Это регрессия после добавления разделения наборов по разрешению.
   
   - **Воспроизведение:** в одной папке выделить `Oak_BaseColor_2K`, `Oak_Normal_2048`, `Oak_ORM_2K` → набор `Oak (2K)` получает BC/ORM, а `Oak (2048)` — только Normal. Вместо одного материала предлагаются два неполных.
   - **Исправление:** нормализовать разрешение в числовой ключ: `2K → 2048`, `4K → 4096`; отображаемую подпись хранить отдельно.

2. **medium — точные названия каналов из двух токенов остаются сломанными.**  
   [GlazeMatcher.cpp:132](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMatcher.cpp:132), также строки 145–152.

   Объединение двух токенов выполняется только при наличии третьего — имени набора. Поэтому исправление точных имён работает для `NormalGL` и `BaseColor`, но не для `Normal_GL` и `Base_Color`.

   - **Воспроизведение:** в `/Game/Textures/Oak` выделить `Base_Color`, `Normal_GL`, `Roughness` → три набора: `Base`, `Normal_GL`, `Oak`. `Normal_GL` не распознаётся и как единственная текстура своего набора попадает через fallback в **BaseColor**; автоисправление настроек может переключить её на цветное сжатие/sRGB.
   - **Исправление:** разрешить точное составное имя канала при двух токенах, оставляя `SetName` пустым для последующей группировки по папке.

3. **medium — Unicode-группировка зависит от языка редактора.**  
   [GlazeMatcher.cpp:87](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMatcher.cpp:87).

   `FText::ToLower()` использует ICU lowercase с текущей локалью, а не независимый от локали case folding. UE меняет локаль ICU при смене языка: [ICUInternationalization.cpp:529](/D:/EGL/UE_5.8/Engine/Source/Runtime/Core/Private/Internationalization/ICUInternationalization.cpp:529).

   - **Воспроизведение:** установить язык редактора Turkish, выделить в одной папке `T_Iron_BC`, `T_iron_N`, `T_iron_ORM` → `Iron` преобразуется в `ıron`, а `iron` остаётся `iron`; цвет отделяется от Normal/ORM. В английском и русском эти имена объединяются.
   - **Исправление:** использовать Unicode case folding без зависимости от языка для ключей наборов и папок.

**Предыдущее ревью**

Прежние сценарии №1, №2, №4–8 исправлены: папки и разные разрешения разделяются; ошибка сохранения возвращает неуспех и допускает повторное сохранение; двойной переворот GL-нормали устранён; одиночные длинные имена каналов и приклеенные суффиксы распознаются; кириллица объединяется; неполный импорт сопровождается уведомлением. Новые границы этих исправлений описаны выше.

- **medium, прежняя №3 — не исправлено:** [GlazeMatcher.cpp:42](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazeMatcher.cpp:42): `Leaves_AlbedoTransparency` заполняет только BaseColor, альфа не становится Opacity; исправление требует совместного использования цветной текстуры и её A-канала, пункт оставлен в ROADMAP.
- **low, прежняя №9 — не исправлено:** [GlazePack.cpp:39](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:39): открыть меню без `pack.json`, затем восстановить файл → списки останутся пустыми до перезапуска; устанавливать `bLoaded` после успешного чтения и разбора.

**20 проверенных имён**

Примеры в форматах Quixel, Poly Haven, ambientCG и Substance; идентификаторы Quixel условные. Проверяется имя UE-ассета без расширения.

| Имя | Набор / разрешение | Результат |
|---|---|---|
| `T_ubmka_2K_Albedo` | `ubmka / 2k` | BaseColor |
| `T_ubmka_2K_Normal` | `ubmka / 2k` | Normal |
| `T_ubmka_2K_ORM` | `ubmka / 2k` | AO.R, Roughness.G, Metallic.B |
| `T_vg5ncie_4K_ORDp` | `vg5ncie / 4k` | AO.R, Roughness.G, Height.B |
| `aerial_rocks_02_diff_4k` | `aerial_rocks_02 / 4k` | BaseColor |
| `aerial_rocks_02_nor_gl_4k` | `aerial_rocks_02 / 4k` | Normal, flip G¹ |
| `aerial_rocks_02_rough_4k` | `aerial_rocks_02 / 4k` | Roughness |
| `Concrete030_2K_Color` | `Concrete030 / 2k` | BaseColor |
| `Concrete030_2K_NormalDX` | `Concrete030 / 2k` | Normal |
| `Concrete030_2K_NormalGL` | `Concrete030 / 2k` | Normal, flip G¹ |
| `Concrete030_2048_AmbientOcclusion` | `Concrete030 / 2048` | AO; отделится от вариантов `2K` |
| `MetalPlates006_1K_Metalness` | `MetalPlates006 / 1k` | Metallic |
| `Wood_Floor01BaseColor` | `Wood_Floor01` | BaseColor |
| `Wood_Floor01Normal` | `Wood_Floor01` | Normal |
| `Fabric01_Gloss` | `Fabric01` | Roughness, invert |
| `Glass01_Opacity` | `Glass01` | Opacity |
| `Panel01_Emissive` | `Panel01` | Emissive |
| `WoodFloor01_Height` | `WoodFloor01` | Height² |
| `Normal_Concrete01_BaseColor` | `Normal_Concrete01` | BaseColor |
| `t_Камень__02_aRm_4k` | `Камень_02 / 4k` | AO.R, Roughness.G, Metallic.B |

¹ Матчер отключает дополнительный flip, если `bFlipGreenChannel` уже включён у текстуры.  
² Слот распознаётся; использование Height материалом ещё находится в ROADMAP.

Дополнительные проверки: `Concrete` и `Conc_N` остаются разными наборами; внутренний `Normal` не мешает конечному суффиксу; цифры сохраняются; повторные подчёркивания схлопываются; `Камень`/`камень` объединяются.

**Импорт и поставка**

- Manifest соответствует структуре: **18 материалов × 2 разрешения × 3 карты + 21 нойз × 2 разрешения = 150 PNG**, отсутствующих и лишних файлов нет.
- Повторный импорт использует существующие ассеты; удалённая карта импортируется заново при доступном исходнике. Настройки существующих текстур сохраняются.
- Новые карты получают ожидаемые настройки: BC — Default/sRGB, N — Normalmap/linear, ORM — Masks/linear, нойз — Grayscale/linear.
- `GetBaseDir()` корректен для Project Plugins и Engine/Marketplace. UE `BuildPlugin` включает `Resources`; в имеющейся упаковке присутствуют 150 PNG и manifest.
- Импорт создаёт обычные текстуры в `/Game/Glaze/Pack`; runtime-зависимости от классов или ресурсов плагина на этом участке нет.

**Память и GC**

Подтверждённых утечек и делегатов без отписки не обнаружил. Прежняя проблема очереди исправлена: текстуры теперь хранятся в `TObjectPtr` и перечисляются в `SGlazeWindow::AddReferencedObjects`.

Сохраняется ранее отмеченная лишняя сборка после импорта: [GlazePack.cpp:149](/C:/Users/intpo/Desktop/Glaze/GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/GlazePack.cpp:149) меняет настройки уже импортированной текстуры и вызывает `PostEditChange`. Величина дополнительного пика памяти не измерена; настройки лучше задавать в import pipeline.

**UX-предложения — по пользе**

1. Показывать конфликтующие кандидаты одного слота вместо молчаливого выбора первой текстуры.
2. Для карт без обозначения разрешения предлагать выбор набора, когда доступны несколько разрешений.
3. Добавить единый прогресс импорта BC/N/ORM и итоговую сводку.
4. Показывать причину недоступности пака непосредственно в меню.