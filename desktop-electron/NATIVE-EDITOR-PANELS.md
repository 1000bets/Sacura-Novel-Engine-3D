# React UI → Engine / EditorCore

Ветка: `feature/electoron_ui`. Проверки: Windows, 23 октября 2026.

## Что подключено

Desktop использует существующие Engine, SceneWorkspace, EditorCommandStack,
AssetRegistry, StoryGraphDocument и PlaySession в `SakuraElectronHost`.
React хранит полученные снимки и состояние представления: открытые панели,
поиск, фокус, выделенную ноду графа и неподтверждённый текст поля.
Изменения сцены/документов подтверждает C++; локального авторитетного мира нет.

Общие HierarchyTree, TransformInspector, ComponentInstancesInspector,
AssetExplorer, StoryFlow и NodeAnswer используются на обеих платформах.
Web сохраняет прежние обработчики и Three.js. Desktop не создаёт PreviewRuntime,
не монтирует Web viewport, Web dialogue/HUD и JS StoryRuntime.
Начальный пустой Web shape нужен существующему Editor для его shell/layout
defaults; native панели его не отображают, не сохраняют и не симулируют.
Запуск desktop сразу открывает Editor; промежуточный Web project.json не нужен.

## Матрица функциональности

«Полностью» относится к перечисленным операциям, а не ко всем возможностям Web UI.

| Область | Статус | Реализация и граница |
|---|---|---|
| Hierarchy: список, создание, удаление, имя, Parent, выбор | Работает полностью | Persistent IDs C++, дерево реальных родителей, drag-and-drop и Parent select; циклы отвергает EditorCore. Удаление поддерева очищает Inspector. |
| Hierarchy: мультивыбор, duplicate, clipboard, visibility toggle | Не реализовано | Native selection сейчас одиночный. Copy/duplicate не заменяются Web мутациями. |
| Inspector: компоненты и Reflection | Работает полностью | Реальные типы/компоненты; добавление, удаление, чтение и редактирование доступных отражённых свойств через команды EditorCore. |
| Inspector: специализированные редакторы всех типов | Работает частично | Скаляры и векторы используют общие поля. Структуры/массивы/сложные ссылки — JSON с C++ валидацией. Нет богатых asset/object pickers и enum dropdown для каждого типа. int64 сохраняется десятичной строкой. |
| Position / Rotation / Scale | Работает полностью | Общий TransformInspector, Euler degrees только как представление native quaternion. Gizmo и внешние native edits обновляют React через события. |
| Content Browser: список и обновления | Работает полностью | Зарегистрированные видимые типы, /Engine и /Game, GUID, metadata/subassets. Внешнее появление/удаление обнаруживается сканированием; изменение файла меняет version и URL миниатюры. |
| Content Browser: импорт и просмотр | Работает частично | Файловый импорт через OS picker → AssetImporter, поддерживаемые им GLB/FBX/изображения; конфликты не перезаписывают файл. Metadata и PNG/JPEG/BMP/WebP preview; Scene/Story открываются двойным щелчком. Нет model 3D preview/placement, HDR/TGA preview. |
| Content Browser: folder import, drag-and-drop, folder creation | Не реализовано | Недоступные кнопки/действия объясняют ограничение. |
| Story Graph: Line, Choice, Action, End; связи, позиции, старт, удаление | Работает полностью | Существующий StoryFlow отображает C++ документ. Все записи через StoryGraphDocument, ID назначаются C++. Автораскладка при открытии не изменяет документ. |
| Story Graph: Events, сохранение, Undo/Redo | Работает полностью | Создание/удаление размещения, изменение определения Event, Actions, фаз и порядка групп, native history; .story и layout сохраняет EditorCore. |
| Story Graph: удобство редактирования Actions/Events | Работает частично | Definition/actions/choices можно редактировать как JSON с native schema validation. Перенос Event реализован внутри одной ноды. Несколько соединений/удалений дают несколько native Undo шагов. |
| Story Graph: clipboard, перенос Event между нодами, Web Blueprint/variables/functions | Не реализовано | Не исполняются в JS вместо C++. Доступны четыре runtime-типа нод и действия текущего C++ формата. |
| Simulation: Play / Pause / Resume / Stop / Step | Работает полностью | Настоящий cloned PlayWorld, Step = 1/60 s в Paused. Редактирование scene/assets/story во время Play отклоняется. Play загружает startupStory проекта; открытие другого графа не меняет .project. Сохраните историю перед Play. |
| Dialogue interaction и отображение состояния StoryRuntime в React | Не реализовано | Нет Advance/Choice runtime-панели и native HUD. Старые Web widgets не показываются вместо native состояния. |
| Новый проект, Save As, export, прочие authoring panels | Не реализовано | Эта задача не переносит весь редактор. Существующие неподключённые панели показывают ограничение. |
| Web-only ноды/игровые механики, отсутствующие в native схеме | Требует изменений C++ Engine | Для них нужны native схема, команды и исполнение; включение JS runtime в desktop не является решением. |

Для реализованного набора новых публичных интерфейсов Engine не потребовалось.
Следующие UI улучшения вроде pickers и clipboard в первую очередь требуют
дополнительных bindings EditorCore и UI, а не второго движка.

## Адаптеры и синхронизация

* `editorPlatform.js`: Web adapter делегирует существующим данным/обработчикам;
  desktop store работает только с NativeBridgeClient. `useEditorPlatform`
  подписывает React через `useSyncExternalStore`.
* `nativeStoryPresentation.js` — чистая проекция документа в view model общего
  StoryFlow. Она не исполняет ноды и не записывает изменения.
* Ответы и события имеют `session`, `epoch`, `revision`. Mutation использует
  контекст отображённого snapshot. Устаревший процесс/мир не принимает его ID.
  Disconnect очищает объекты, компоненты, ресурсы и каталог типов; старый
  asynchronous domain read не восстанавливает состояние предыдущего проекта.
* Native Poll обнаруживает также gizmo/gameplay edits. `changes.scene/assets/story`
  инвалидируют нужные домены. Чтения объединяются, неизменённые object/component
  references сохраняются. Изменение свойства не перемонтирует React-приложение.
* Snapshot store не делает optimistic scene edits. Ошибка команды видна в UI;
  удалённый ID получает `NOT_FOUND`. Input drafts не становятся состоянием Engine
  до подтверждения команды.
* Ctrl+S, Ctrl+Z, Ctrl+Shift+Z/Ctrl+Y учитывают активную область scene/story.
  Активация viewport возвращает область native scene history. Delete удаляет
  выбранный объект; StoryFlow сохраняет свои shortcuts. F5 запускает/останавливает Play.

## Bridge, безопасность и потоки

Сохранён отдельный native process: отказ C++ не повреждает Electron UI, Engine
имеет свой Game thread и существующий Render Thread. Именованный control pipe
передаёт типизированные команды/ответы и события; пикселей в JSON IPC нет.
GPU путь остаётся sharedTexture → VideoFrame → GPU blit из предыдущего этапа.

Новые команды описаны в `bridge/contract.json` и `contract.d.ts`: `assets.*`,
`story.*`, `play.step`. Validation действует и в main, и в C++.
Команды scene/object/component используют существующий EditorCommandStack,
Story команды — собственную историю StoryGraphDocument. Reflection использует
общий serializer; для отдельных компонентов не создаются вручную DTO.

Renderer изолирован (`contextIsolation`, sandbox, без nodeIntegration).
`assets.import` и `assets.resolve` запрещены в renderer command API: абсолютный
source path поступает только из системного диалога в main. Миниатюра запрашивает
`sacura://native-asset/<session>/<asset-id>?v=<version>`; main разрешает только
зарегистрированную Texture с разрешённым расширением. Протокол работает и с Vite.
Web не имеет preload и Native Bridge. Нет API для произвольного C++ кода.

## Сборка и запуск

Из `desktop-electron`:

```powershell
npm run native:configure       # если build ещё не настроен
npm run desktop:build          # native host + UI + win-unpacked
npm run native:start           # временная копия SampleProject
```

Настоящий проект:

```powershell
$env:SACURA_NATIVE_PROJECT='D:/Games/MyGame/MyGame.project'
& '.\release\win-unpacked\Sacura Desktop Experiment.exe'
```

Без переменной выберите `.project` кнопкой viewport или через меню Файл.
Смена проекта останавливает прежний host, освобождает GPU import references и
создаёт отдельный process/session; Python state не переносится между проектами.

## Проверки и артефакты

* Web: **812/812**, включая прежние Web/Three.js тесты, 7 тестов общих панелей,
  3 проверки adapter store и прежние native-client tests.
* Desktop: **12/12**, реальные Engine/EditorCore процессы, restart, ошибки,
  persistent IDs, native changes, Texture import/conflict, external asset changes,
  Line/Choice/Action/End, Events, save/reopen и Play/Pause/Step/Stop.
* EditorCore/Story/Qt: **17/17**, включая Qt Story Graph UI/Shell/Speaker.
* Windows packaged smoke: результат, метрики и снимки в
  `build/electron-panels-packaged/{report.json,viewport.png,panels.png}`.
  Тест обращается к React adapter/hook и проверяет, что изменения появились
  в React snapshot; компонентные тесты отдельно проверяют обработчики общих UI.
  Smoke также проверяет native draw counts, GPU frames, overlays, resize,
  hide/show, minimize/restore, camera/picking/gizmo и смену процесса.
  Дополнительно пройден Vite development startup без Web project launcher.
* Системный OS file picker не автоматизируется: отдельно проверены UI callback,
  main capability boundary и фактический C++ import. Переключение физического
  монитора не автоматизировалось; проверяется Chromium scale factor.

Логи: `build/panels-{native-final-build,ui-final-build,package-build,
desktop-final-tests,web-final-tests,core-qt-tests,packaged-smoke}.log`.
Проверки выполняются CLI/project-owned helpers, без Computer Use.
Режим `--native-smoke` отключает Chromium occlusion throttling, чтобы другое
окно на рабочем столе не останавливало rAF/capturePage. Обычный запуск сохраняет
стандартную visibility policy. См. [Electron background throttling](https://www.electronjs.org/docs/latest/api/browser-window)
и [известное поведение Windows occlusion](https://github.com/electron/electron/issues/31016).

## Производительность и ограничения

* 47 native command round trips в headless test: p50 **15.92 ms**, p95 **19.08 ms**,
  max **38.12 ms**. Это время transport + native command, без React layout/scanout.
* Минутный packaged soak (`build/electron-panels-soak-60.json`): **3777 кадров /
  60.303 s = 62.63 FPS**, frame p50 **16.0 ms**, p95 **16.4 ms**; готовый native
  кадр → Chromium draw p50 **15.15 ms**, p95 **15.58 ms**. Input → draw p50
  **29.5 ms**, p95 **64 ms** на синтетических input packets. Это не измерение
  появления пикселя на физическом дисплее. Последняя native CPU/GPU выборка:
  **0.596 / 0.210 ms**, не среднее за интервал.
* GPU texture ring: **3 текстуры / 5,627,400 bytes** во всех шести выборках soak;
  после смены проекта прежние import references освобождены. Финальная сборка
  после исправления маршрутизации Ctrl+S повторно проходит packaged smoke.
* Native change detection примерно каждые 240 ms; AssetRegistry scan примерно
  каждые 1.9 s. Нет per-frame JSON snapshots. Пока это полные native comparisons
  и выборочные domain reads, не granular change journal/paging.
* Import, scene/story I/O, scans и Play clone выполняются в native Game thread.
  Electron UI остаётся отзывчивым, но большая операция может задержать C++ кадры.
* Smoke проверяет небольшую сцену и ограниченный интервал; это не доказательство
  стабильности многосуточной работы или проектов с тысячами объектов/ресурсов.
* Windows D3D12/Electron 45 — проверенный профиль. D3D11, другие ОС и упаковка
  Release не добавлены этим этапом. Vite предупреждает о размере общего UI bundle.
* Dirty scene/story open защищён отказом `UNSAVED_CHANGES`. Общий UX Save/Discard
  при закрытии приложения/смене проекта ещё требует доработки; сохраняйте документы
  перед этими действиями. Crash recovery/autosave native-документов не добавлены.

## Изменённые файлы этапа

* `frontend/src/editorPlatform.js`, `useEditorPlatform.js`, `nativeStoryPresentation.js`,
  `ReflectedFields.jsx` — adapters, store, generic reflected fields и graph projection.
* `frontend/src/{main.jsx,Editor.jsx,HierarchyTree.jsx,ComponentInstancesInspector.jsx,
  TransformInspector.jsx,AssetExplorer.jsx,StoryFlow.jsx,NodeAnswer.jsx}` — общие панели.
* `frontend/src/{SceneViewport.jsx,NativeViewport.jsx}` — активация области undo,
  удаление временной debug-панели из viewport.
* `desktop-electron/native/{NativeBridge.h,NativeBridge.cpp,Host.cpp}` — новые domain
  commands, события и scans через существующий EditorCore.
* `desktop-electron/bridge/{contract.json,contract.d.ts,transport.cjs}` — контракт/validation.
* `desktop-electron/{main.cjs,preload.cjs}` — import capability, asset protocol, startup.
* `desktop-electron/tests/{bridge-contract.test.cjs,native-bridge.test.cjs}`,
  `frontend/tests/{nativePanels.test.js,editorPlatform.test.js}`,
  `desktop-electron/scripts/{native-start.cjs,native-smoke-main.cjs}` — проверки.
* `desktop-electron/{README.md,NATIVE-EDITOR-PANELS.md}` — актуальная документация.

Здесь `frontend` = `engine-web-prototype/frontend`. Существовавшие изменения Qt,
RHI/shared texture и EditorCommandStack предыдущего этапа не являются новыми
изменениями панелей. Qt Editor исходники в этом этапе не редактировались.

## Рекомендуемый следующий этап

Dirty-close UX; native resource/object pickers; typed Action/Event forms поверх
существующей схемы; granular change journal и worker I/O с публикацией на Game
thread; paging и нагрузочные проверки. Затем отдельная интеграция runtime dialogue
и choices с C++ StoryRuntime. Следующий этап автоматически не начинается.
