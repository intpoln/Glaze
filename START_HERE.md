# Glaze — START HERE (входной файл для любого агента)

Обновлено: 2026-10-11. **Glaze 1.0.0 выпущен** (UE 5.7 / 5.8, Win64), листинг на Fab создан, репозиторий публичный.

Прочитай этот файл целиком, потом по задаче: план `docs/ROADMAP.md`, ручные проверки `CHECKLIST.md`, публикация `fab/FAB_LISTING.md`, история решений — `git log`.

## 0. Кто и как
- Владелец — Никита Полушин (intpoln), моушн-дизайнер, раньше бэкенд на Python. **Отвечать на русском**, на «ты», коротко и по делу. Читает только чат, часто пишет голосом (слова искажены — толковать по смыслу).
- На длинных задачах — пауза примерно каждые 15 минут в безопасной точке: что готово / что дальше / % плана, потом ждать «продолжай».
- Скриншотов и картинок — минимум и только с понятной целью (Никита это явно просил).

## 1. Что такое Glaze
Editor-плагин UE (C++, один модуль `GlazeEditor`, Win64): набор текстур → одно окно → обычный **Material Instance** от мастер-материала `/Game/Glaze/M_Glaze`.
- Выделил текстуры → ПКМ → **Glaze: Create Material**. Матчер раскладывает карты по слотам по именам (Quixel, Poly Haven, ambientCG, Substance, packed ORM/ARM/RMA, GL/DX нормали, gloss).
- Окно: цветокоррекция, ремапы Roughness/Metallic/Specular/AO, UV или triplanar (World/Object), Parallax Occlusion, детали на нойзе (UV/World/Local/Pre-Skinned, процедурный или текстурный, Detail Bump), Macro Variation, Masked/Dither/Two Sided, Emissive, ветер (WPO). Live preview на выделенных мешах уровня, Cancel всё откатывает. Батч (Create & Next / Create All), **Edit with Glaze** (рецепт хранится JSON в метаданных инстанса).
- Пак: 18 материалов + 21 нойз (PNG в `Resources/Pack`, 2K/1K, импорт в 2K/1K/512/256) — ПКМ в Content Browser → **Glaze: Material from Pack** / **Glaze: Noise from Pack** (окно нойзов: Invert/Levels/Gamma/Contrast, запекает Grayscale-текстуру). Импорт в `/Game/Glaze/Pack/...`.
- Текстуры пака сделаны в **PocketTextureLab** — https://intpoln.github.io/PocketTextureLab/ (приложение Никиты). Упоминать в любых описаниях **обязательно**.

**Главный принцип: всё, что создаёт Glaze, работает без плагина** (мастер и инстансы — обычные ассеты в `/Game`, в том числе в собранной игре). Ничего не класть в контент плагина, не писать вне `/Game`.

## 2. Где что лежит
| Путь | Что |
|---|---|
| `C:\Users\intpo\Desktop\Glaze\` | корень репо, GitHub https://github.com/intpoln/Glaze (**публичный**) |
| `GlazeLab/` | тестовый проект UE 5.8; плагин в `GlazeLab/Plugins/Glaze` |
| `GlazeLab/Plugins/Glaze/Source/GlazeEditor/Private/` | весь код (см. §3) |
| `GlazeLab/Plugins/Glaze/Resources/Pack` | пак (PNG в **Git LFS**) + `pack.json` |
| `packages/5.7/Glaze`, `packages/5.8/Glaze` | собранный плагин с бинарниками (**в git**; ставится копированием в `<Project>/Plugins/Glaze`) |
| `packages/Glaze_<v>_source.zip` | zip для Fab (Source + Resources + .uplugin, без Binaries). **Не в git** (~180 МБ) — лежат в GitHub Release v1.0.0 |
| `fab/FAB_LISTING.md` | всё для публикации на Fab: тексты листинга, файлы, чек-лист |
| `fab/images/` | картинки листинга 1920×1080 PNG (01 thumbnail, 02 workflow, 03 one set many looks, 04 included materials); `fab/upload/` — те же в JPG для загрузки (в .gitignore); `fab/raw/` — исходные кадры; `Tools/fab/compose.py` собирает картинки |
| `README.md` | README на GitHub (= DocsURL плагина) |
| `docs/ROADMAP.md` | план и отложенные задачи |
| `docs/CONCEPT.md` | исходная концепция (2026-10-07), частично устарела — актуально то, что в коде и ROADMAP |
| `docs/reviews/<дата>/` | отчёты ревью Codex (2026-10-09, 2026-10-09-2, 2026-10-10) |
| `CHECKLIST.md` | ручные проверки Никиты |
| `Tools/` | `build.sh`, `runtests.sh`, `package.sh`, `ue_mcp.py`, `review/run_review.sh`, `texgen/` (генерация пака из PTL), `fab/compose.py` |
| `TexturePack/graphs` | исходные графы PTL для материалов пака |
| `icons/`, `notes/` | иконки и брейншторм Codex (история) |
| `D:\EGL\UE_5.8`, `D:\EGL\UE_5.7` | движки |

## 3. Код (GlazeEditor/Private)
| Файл | Что |
|---|---|
| `GlazeMaster.*` | генерация графа мастера `M_Glaze` кодом. `MasterVersion` (сейчас **9**) в метаданных пакета: при росте версии мастер пересобирается, downgrade запрещён. `SetMasterUsages` — usage-флаги (в 5.7 `SetUsageByFlag` приватный → флаги `bUsedWith*` под `#if ENGINE_*_VERSION`) |
| `GlazeMatcher.*` | разбор имён текстур → наборы и слоты |
| `GlazeRecipe.*` | `UGlazeRecipe` — все настройки окна, `ToJson/FromJson` |
| `GlazeApply.*` | рецепт → параметры MI, автоисправление настроек текстур (с откатом на Cancel) |
| `SGlazeWindow.*` | окно Glaze: Details рецепта, preview, батч, undo-guard превью |
| `GlazePack.*` | импорт пака (ресайз 512/256 линейно для `_N`/`_ORM`/нойзов) |
| `GlazeNoise.*`, `SGlazeNoiseWindow.*` | окно нойзов |
| `GlazeUndoKeys.h` | Ctrl+Z / Ctrl+Y в окнах |
| `GlazeEditorLibrary.*` | Python API `unreal.GlazeEditorLibrary` для проверок: окно (`open_window_for_textures`, `set_window_property`, `window_create`…), нойзы, `import_pack_material` |
| `GlazeEditorModule.cpp` | меню Content Browser |
| `Tests/` | автотесты `Glaze.*`: Compile, Instance, Master, Matcher, Noise, Pack, RecipeJson (7 шт.) |

## 4. Сборка, тесты, упаковка
- Сборка: `bash Tools/build.sh` — **GlazeLab должен быть закрыт** (иначе DLL занята).
- Тесты: `bash Tools/runtests.sh Glaze rhi` (`rhi` — с GPU, нужен для Glaze.Compile). Норма — 7 passed, exit 0.
- Закрыть GlazeLab через MCP: `python Tools/ue_mcp.py py "unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True); unreal.SystemLibrary.quit_editor()"`.
- Запустить: `cmd //c start "" "D:\\EGL\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "C:\\Users\\intpo\\Desktop\\Glaze\\GlazeLab\\GlazeLab.uproject"`; уровень грузить через `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/GlazeShowcase/L_Showcase')` (аргумент карты в командной строке игнорируется).
- Упаковка: `bash Tools/package.sh 5.7` и `bash Tools/package.sh 5.8` → `packages/<v>/Glaze` (RunUAT BuildPlugin; старый пакет заменяется только при успехе; пути с `'` и `&` не поддерживаются). Лог — `packages/<v>/package_log.txt`, должно быть 0 warnings.
- Zip для Fab: `python Tools/fab/make_zips.py` → `packages/Glaze_5.7_source.zip`, `Glaze_5.8_source.zip`.
- Проверка пакета в чистом проекте: создать в scratchpad папку с `T.uproject` = `{ "FileVersion": 3, "EngineAssociation": "5.7", "Plugins": [ { "Name": "Glaze", "Enabled": true } ] }`, скопировать пакет в `Plugins/Glaze`, запустить `D:\EGL\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <uproject> "-ExecCmds=Automation RunTests Glaze; Quit" -unattended -nosplash -nosound -NoLiveCoding -log=T.log` → в логе 7 тестов Success.
- **Выпуск новой версии:** поднять `VersionName` (и `Version`) в `GlazeLab/Plugins/Glaze/Glaze.uplugin` → build + тесты → package 5.7 и 5.8 → проверка в чистых проектах → zip → коммит (`packages/`) → `gh release create vX.Y.Z packages/Glaze_5.7_source.zip packages/Glaze_5.8_source.zip --repo intpoln/Glaze` (или `gh release upload <tag> ... --clobber` для замены) → загрузить zip на Fab (это делает Никита / агент публикации, см. `fab/FAB_LISTING.md`).

## 5. Проверки через MCP (живой редактор)
GlazeLab отдаёт MCP на `http://127.0.0.1:8000/mcp` (плагин VibeUE, включён **локально** в `GlazeLab.uproject`, не коммитится).
- `python Tools/ue_mcp.py py "<код>"`, `pyfile <файл.py>`, `shot out.png editor|window 1600` — компактный клиент.
- VibeUE **сохраняет все dirty-пакеты перед каждым py-вызовом** — тесты на dirty-состояние делать внутри одного вызова.
- `OnObjectModified` срабатывает раз на объект за кадр — многошаговые проверки через `register_slate_post_tick_callback` с проверкой `get_frame_count()`.
- Скриншот сцены: HighResShot ненадёжен — SceneCapture2D-актор (`SCS_FINAL_TONE_CURVE_HDR`, `RTF_RGBA8_SRGB`) в позицию камеры вьюпорта → `RenderingLibrary.export_render_target`. Окно — `ue_mcp.py shot ... window`.
- `GlazeLab/Content/` (сцены для скриншотов `/Game/GlazeShowcase`) — локальный, **не коммитить**.

## 6. Правила
- Субагенты (Agent tool) — только модель **sonnet**; могут писать код. Сборку/запуск редактора держит один главный агент, субагенты правят непересекающиеся файлы.
- **Codex — только поиск, ревью, предложения, код не пишет** (исключение — генерация картинок/иконок в отдельную папку). Находки помощников — гипотезы, проверять.
- Codex: `codex exec -s read-only "<prompt>" < /dev/null` (без закрытого stdin вешается). Слова «crash/corrupt» в промпте ловит фильтр OpenAI — писать «stability review». Параллельное ревью по участкам: `Tools/review/run_review.sh`.
- Коммит + push после каждого зелёного шага; без `Co-Authored-By` и других упоминаний Claude/AI в коммитах.
- **Не коммитить** `GlazeLab/GlazeLab.uproject` (там локальные VibeUE/MCP-плагины) и `GlazeLab/Content/`.
- После шага обновлять: план (`docs/ROADMAP.md`), `CHECKLIST.md` (что проверить руками), этот файл (§7, если изменился статус), `C:\Users\intpo\Desktop\VPS\WORKLOG.md` (строка «Glaze: …»).
- **Никогда не закрывать редактор Fablewood58** — рабочий проект Никиты. GlazeLab закрывать можно (сначала сохранить dirty-пакеты).
- Скачивание файлов — только с явного разрешения Никиты. Ключ ElevenLabs — только из переменной `ELEVENLABS_API_KEY`, никогда в чат.
- Удалять релизы/теги/ассеты на GitHub — только спросив.
- Память/утечки — отдельный пункт любого ревью.
- Нельзя класть копии плагина внутрь папки, которую сканирует тестовый проект (плагин увидится дважды) — поэтому `packages/` в корне.
- Совместимость 5.7: API, появившиеся в 5.8, закрывать `#if ENGINE_MAJOR_VERSION/ENGINE_MINOR_VERSION` (`Runtime/Launch/Resources/Version.h`) и проверять `package.sh 5.7`.
- В Bash долгий `sleep` заблокирован — ждать через `timeout N bash -c 'until ...; do sleep 3; done'` или run_in_background. Heredoc ломается на сложном коде — писать файлы через Write.

## 7. Статус
- **2026-10-10 — релиз 1.0.0.** Сборки 5.7/5.8 прошли все тесты в чистых проектах; zip в https://github.com/intpoln/Glaze/releases/tag/v1.0.0; FabURL вписан (`com.epicgames.launcher://ue/Fab/product/754988a2-ceff-495d-b973-fbc3f1402220`); репозиторий открыт. Видео не делали — только картинки (решение Никиты). Старый релиз v0.1.0 остался (удалять — только спросив).
- Дальше: отложенные находки ревью и хвосты в `docs/ROADMAP.md` (раздел «Релиз на Fab» и «Хвосты ревью Codex»); ручные пункты CHECKLIST (31 — клавиши Ctrl+Z/Y в окне нойзов).
- 2026-10-07…10: v0 (мастер, матчер, окно), пак, v2 (детали, triplanar, POM, ветер), окно нойзов, три раунда ревью Codex, поддержка 5.7.
