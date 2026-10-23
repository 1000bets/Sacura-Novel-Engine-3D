# Совместимость проектов Qt ↔ Electron

Ветка `feature/electoron_ui`, Windows x64, 23 октября 2026.

## Реализованный путь

Electron main запускает отдельный `SakuraElectronHost`, связанный с теми же
Engine и EditorCore, что и Qt Editor. Создание выполняет C++ ProjectGenerator;
открытие — ProjectSession, SceneSerializer и StoryGraphDocument/StoryDocumentIO.
JavaScript передаёт команды и отображает снимки. Форматы не заменены:

| Данные | Формат / источник |
|---|---|
| Проект | `.project`, engineVersion `0.1`, относительные startupScene/startupStory |
| Сцена | `sakura.scene`, formatVersion 1, SceneSerializer |
| История | `sakura.story`, formatVersion 1, StoryGraphDocument / StoryDocumentIO |
| Ресурсы | Content + `.meta` schemaVersion 1, AssetRegistry / AssetImporter |
| Идентичность | persistent IDs объектов/компонентов; GUID ресурсов и subassets |
| Свойства | ReflectionJson и текущие C++ typeVersion/migrations |

`Content`, `Scripts`, `Config` остаются обычными папками проекта. LocalStorage
не является форматом desktop-проекта. ImportInfo может хранить исходный путь
импорта для диагностики; загруженный ресурс разрешается через Content и GUID.

### Пользовательский сценарий

1. **Файл → Новый C++ проект…**: имя, затем системный выбор родительской папки.
2. **Файл → Открыть C++ проект…**: существующий `.project`.
3. **Новая сцена…**, создание объектов, компонентов и редактирование Inspector.
4. Импорт в Content Browser; reflected asset fields предлагают выбор по имени.
5. Создание/редактирование истории в существующей панели Story Graph.
6. **Сохранить сцену и историю** / Ctrl+S. Отдельная команда **Использовать
   текущие сцену и историю при запуске** сохраняет startup paths через ProjectSession.
7. Закрыть Electron, открыть тот же `.project` в Qt. Обратный путь аналогичен.

Новый проект предварительно проверяется отдельным C++ процессом перед остановкой
текущего host. Ошибки загрузки и Degraded state не подменяются пустой сценой.
При закрытии/смене/перечитывании проекта предлагаются Save / Discard / Cancel;
ошибка сохранения оставляет текущую сессию открытой.

SceneDocument теперь запоминает fingerprint и запрещает перезапись внешнего
изменения, включая Save As в тот же файл. Это общий код Qt и Electron.
История использует существующую защиту StoryGraphDocument. Изменения descriptor
отслеживаются Bridge; ресурсный реестр периодически сканируется. UI показывает
конфликт и предлагает перечитать проект либо сохранить сцену под новым именем.
Автоматического слияния конфликтующих документов нет.

`component.setAsset` проверяет GUID, subasset и reflected type filter, затем
меняет основной и companion property одной группой EditorCommandStack.
Контейнеры Model теперь включены в Bridge вместе с subassets.

## Семантический round-trip — PASS

Основное доказательство: `build/compat-roundtrip-release-1/report.json`.
Запущен **упакованный Electron**, использующий bundled Release host;
Qt часть использует настоящий **Release EditorMainWindow** и его командный стек.
Qt shell в CLI тесте скрыт, GPU-сurface в нём не создаётся. Computer Use не применялся.

Проверено:

- ProjectGenerator вызван из Electron main; создание сцены и объектов через
  изолированный renderer Bridge.
- Родитель + дочерний объект + камера; MeshRenderer, Light, CameraComponent.
- GLB и PNG импортированы оригинальным AssetImporter; mesh/subasset назначены
  через отражённые свойства. PNG сохранён как самостоятельный Texture asset.
- Story Line → Action MoveTo → End, Event binding, startNodeId, layout и targetObjectId.
- Сохранение сцены/истории и startup paths из Electron.
- Qt проверил имена, родителей, transforms, component IDs/types, значения
  reflection, asset/subasset GUID, Story Graph и разрешение targetObjectId.
- Qt выполнил Rename и Transform через свой EditorCommandStack и OnSaveScene.
- Папка проекта скопирована в `Перенос проекта`, затем открыта новым Electron host.
- Electron проверил семантическое состояние всех объектов и компонентов,
  историю/связи/layout, GUID и относительные пути. Сравнения JSON-строк нет.
- Renderer загрузил реальный mesh; получены новые shared GPU frames и обновлённая
  React Hierarchy. `electron-before.png` проверен визуально.
- Логи Qt и host: освобождение объектов без отчёта об утечках.

Тест — `scripts/compat-roundtrip.cjs` + `native/QtCompatibilityTest.cpp`.
Manifest содержит ожидаемые семантические значения; это тестовый артефакт,
не дополнительный формат проекта.

## Сборки и проверки

- Release: собраны SakuraElectronHost, SakuraEditor, SakuraEditorCompatibilityTest.
- Release desktop/Bridge: **15/15**, включая external-change conflict, сохранность
  draft, Save As, restart, неподдерживаемые project/scene/component/metadata versions.
- Debug Core/Story/Qt shell regression: **15/15**.
- Web: полный набор **813/813** после добавления теста гонки чтения при смене epoch.
- Дополнительно adapter/client: **6/6**.
- Packaged Release round-trip: **PASS**; host SHA256 совпадает с CMake Release output.

Первый параллельный Web прогон дал таймаут теста редактора скриптов при активной
компиляции; повтор отдельно прошёл, полный прогон с concurrency=4 также прошёл.
Release проверка обнаружила `/GR-`, экспортируемый Diligent-BuildSettings в Engine.
Из-за отключённого RTTI падала загрузка объектов. Интеграция CMake теперь включает
RTTI; fetched исходники зависимостей не изменялись. Исправление действует и на Qt Release.

Наблюдение короткого packaged теста: 44 представленных кадра, median frame interval
16.0 ms, p95 26.2 ms; ready-to-draw median 15.70 ms, p95 25.91 ms.
Это короткий функциональный прогон со сменами документов, не длительный FPS benchmark
и не измерение физической задержки дисплея.

## Windows пакет

`desktop-electron/release-windows/Sacura-Desktop-0.1.0-Windows-x64.zip`

Размер: 266 178 791 байт. CRC всех 1965 записей ZIP проверен; Debug DLL,
тестовых EXE и PDB в архиве нет. SHA256:
`3980c20fcd809d482a8ef8090263301920d4c4246da2c4cd6bac04ba71773bd3`.
Рядом сохранён файл `.zip.sha256`.

Распаковать целиком; запустить `Sacura Desktop Experiment.exe`. Выбрать/создать
C++ проект через меню «Файл». Пакет portable и не подписан Authenticode.

В `resources/native`: Release C++ host, BridgeContract, Engine Content/Shaders/Config,
Diligent D3D12/Vulkan/OpenGL DLL, shader compiler DLL, Qt runtime/plugins, SDL,
Python 3.12 runtime/stdlib и app-local MSVC redistributable DLL. Тестовые EXE/PDB
не входят в пакет. Это Release-сборка, без подмены Debug host.

Воспроизводимая сборка из `desktop-electron`:

```powershell
npm run native:configure:release
npm run desktop:production
```

Если требуется закрепить конкретный Python SDK, передать CMake
`SAKURA_PYTHON_ROOT` и `Python3_EXECUTABLE` (в проверенной сборке —
`build/tools/python-3.12.10/tools`). Qt по умолчанию собирается из закреплённых исходников.

Повтор packaged round-trip из корня репозитория:

```powershell
$env:SACURA_SOURCE_ROOT = (Get-Location).Path
$env:SACURA_COMPAT_OUTPUT = "$env:SACURA_SOURCE_ROOT/build/compat-new-run"
$env:SACURA_QT_COMPAT_HOST = "$env:SACURA_SOURCE_ROOT/build/windows-msvc-release/ElectronNative/Release/SakuraEditorCompatibilityTest.exe"
& './desktop-electron/release-windows/win-unpacked/Sacura Desktop Experiment.exe' --compat-roundtrip
```

Использовать новую output-папку: создание существующего проекта намеренно отклоняется.
Для теста предварительно собрать `SakuraEditorCompatibilityTest`.

## Матрица и ограничения

| Возможность | Статус |
|---|---|
| `.project`, native Scene/Story, обычные reflected components | Совместимы; один C++ serializer, подтверждён round-trip |
| persistent IDs, parent links, mesh/subasset и Story target refs | Проверены в обоих редакторах и после переноса папки |
| Создание/открытие, Save, Scene Save As | Реализованы; записи выполняются C++ |
| Импорт GLB/PNG | Round-trip проверен; FBX/прочие поддерживаемые форматы используют существующий importer, отдельно здесь не проверялись |
| Обнаружение внешних изменений | Реализовано polling/fingerprints; ручной reload, без merge |
| Несохранённые native документы | Save/Discard/Cancel + защита save conflict; decision logic покрыта автотестами |
| Несовместимые версии | Явный отказ; штатные reflection migrations остаются в C++ |
| Полный набор типов компонентов / custom Python migrations | Общий формат; не все сочетания прошли этот round-trip |
| Полная эквивалентность UI Qt и Electron | Частично; сложные Story Actions/Events ещё имеют JSON editor |
| Save As всего проекта / копия истории при конфликте / автослияние | Не реализованы как UI операции; папку проекта можно переносить целиком |
| Windows production package | Собран и запущен; нет подписи, установщика, проверки на чистой VM или других GPU |

Одновременная конкурентная запись из двух процессов не является транзакцией:
fingerprint предотвращает обнаруженный конфликт, но нет межпроцессной блокировки
всего проекта и атомарного commit нескольких документов. Сохранение сцены, истории
и descriptor последовательно; ошибка не скрывается. Undo history между запусками
не переносится, сохранённое состояние и persistent IDs переносятся.

Компоновщик зависимостей ещё выдаёт LNK4098/C4743; их нельзя считать устранёнными
этим этапом. Проверенные Release сценарии проходят, расширенная ABI-аудит проверка
остаётся отдельной работой. Сценарии на чистой Windows VM не выполнялись.

## Изменённые области

- Engine: ProjectGenerator, ProjectDescriptor, ProjectSession; CMake RTTI integration.
- EditorCore: SceneDocument fingerprint и save conflict.
- Qt shell: только friend test probe в EditorMainWindow; производство UI не переписывалось.
- Electron native: ProjectCli, NativeBridge, Host, QtCompatibilityTest, CMake deploy.
- Electron main/preload: project lifecycle, draft guard, native file dialogs.
- Bridge contract/types: project.save, scene.create, component.setAsset, conflict state.
- React: прежние Editor/ProjectDialog/ReflectedFields; adapter retry для read race.
- Tests/build: native-project tests, native Bridge tests, semantic round-trip helper,
  production builder config и npm commands.

## Продуктовые принципы

Простота: создание — имя + выбор папки; asset binding — список имён, без ввода GUID.
Переносимость и редактируемость: Electron → Qt edit/save → перенос папки → Electron
подтверждён на настоящих C++ документах. Универсальность: пустой проект и произвольные
GameObject/components; жанровые объекты не обязательны. Полный сценарий
«Дверь с ключом A→B→C», все авторские графы и удобство сложных Event форм этим тестом
не проверены. Следующий этап автоматически не выполнялся.
