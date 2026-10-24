# Native Bridge v1

Это отчёт этапа базового контракта. Актуальная интеграция общих React панелей,
матрица функций и новые проверки описаны в [NATIVE-EDITOR-PANELS.md](NATIVE-EDITOR-PANELS.md).

Рабочая ветка: `feature/electoron_ui`. Bridge использует **существующий отдельный
SakuraElectronHost process** и существующие Engine / EditorCore. Полная миграция
Hierarchy, Inspector, Story Graph в этот этап не входит.

## Почему отдельный process

| Подход | Надёжность и потоки | Стоимость / ограничения |
|---|---|---|
| Node-API addon в main | ABI Node-API стабилен, но native crash завершает main. Нужно согласовать Qt/Game loop с Node и владение движком. | Меньше IPC, однако вызовы нельзя выполнять синхронно в main. |
| Отдельный native process — реализовано | Qt/Game thread владеет SceneWorkspace и выполняет команды; имеющийся RenderThread владеет Diligent. Сбой host отключает Bridge, Electron может открыть новый host. | Асинхронный IPC, сериализация только команд/состояния. Процесс запускается один на проект. |
| Process + addon для GPU | Имеет смысл, если нужен недоступный Electron GPU API. | Сейчас лишняя нативная ABI/сборочная поверхность: существующий sharedTexture путь уже работает. |

Основание: [Node child_process](https://nodejs.org/api/child_process.html),
[Node-API](https://nodejs.org/api/n-api.html),
[Electron context isolation](https://www.electronjs.org/docs/latest/tutorial/context-isolation).
Изоляция процесса ограничивает последствия native crash; она не является sandbox
для проектного Python и не изолирует сбои общего GPU driver.

```text
React NativeBridgePanel / typed client
  → context-isolated preload, fixed bridge API
  → Electron main: sender/origin + request validation, Promise
  → length-prefixed control named pipe
  → C++ Game thread: validation + stable ID resolution
  → SceneWorkspace / SceneDocument / EditorCommandStack / PlaySession
  ← explicit result and state.changed events

Engine RenderThread → Diligent shared GPU texture → existing binary GPU pipe
  → Electron sharedTexture / VideoFrame → viewport image + React overlays
```

GPU frames не проходят через JSON. Control pipe отделён от stdout/stderr и от
GPU/release pipe; произвольная строка Engine log не может стать ответом команды.

## Изученные интерфейсы и использование

* `SceneWorkspace`: общий с viewport документ, selection и история команд.
* `SceneDocument`: Open / Save / SaveAs; неуспешный Open сохраняет текущий документ.
* `GameObject`, `Transform`: текущие native данные и persistent IDs; JS хранит только
  полученные snapshots и черновики полей ввода.
* `ReflectionSubsystem`, `PropertyAccess`, `ReflectionJson`: наследуемые свойства,
  concrete component catalog, metadata, ограничения редактирования и codec значений.
* `EditorCommandStack`: все объектные/компонентные/transform mutations, Undo/Redo.
* `AssetRegistry`: каталог сцен и проверка ссылок на зарегистрированные assets.
* `ProjectSession`: загрузка `.project`, Content root и закрытие проекта.
* `PlaySession`: существующий клонированный PlayWorld, Pause/Resume/Stop; редактирование
  и Save/Open во время Play отклоняются. Чтение и selection относятся к ActiveScene.

Исправлена обнаруженная тестом ошибка EditorCore: цели команд теперь сохраняют
проверяемый runtime handle **и persistent ID**. После delete/undo или remove/undo
предыдущие команды находят восстановленный объект/компонент в той же сцене.
Публичные Engine API и Qt UI для Bridge не изменялись.

## Контракт

`bridge/contract.json` — общая схема параметров для main и C++.
`bridge/contract.d.ts` — типизированные команды, результаты, reflected values,
события и context. Автотест компилирует примеры вызовов TypeScript и проверяет
совпадение всех методов, типов и optional-полей со схемой.

Запрос на control pipe:

```json
{"type":"bridge.command","version":1,"id":"42","session":"process-guid","epoch":1,"method":"object.rename","params":{"objectId":"persistent-id","name":"Lamp"}}
```

Успешный ответ содержит `ok:true,value,context`, ошибочный —
`ok:false,error:{code,message},context`. Каждый ответ содержит `version,type,id`.
Context — `{session,epoch,revision}`. Session меняется при перезапуске host;
epoch — при смене сцены / EditWorld / PlayWorld. Старые команды отклоняются.
Revision используется для упорядочивания snapshots; это не optimistic concurrency lock.

| Команда | Параметры | Результат |
|---|---|---|
| `scene.open` | `path`, `discard?` | state |
| `scene.save` | `path?` | state |
| `scene.getHierarchy` | — | objects, parent IDs, transforms, component IDs/types |
| `object.create` | `name`, `parentId?` | `{id}` |
| `object.destroy` | `objectId` | null |
| `object.rename` | `objectId`, `name` | null |
| `object.setParent` | `objectId`, `parentId` или null | null |
| `object.getComponents` | `objectId` | components |
| `object.addComponent` | `objectId`, `typeId` | `{id}` |
| `object.removeComponent` | `objectId`, `componentId` | null |
| `component.getProperties` | `objectId`, `componentId` | reflected metadata + values |
| `component.setProperty` | `objectId`, `componentId`, `property`, `value` | properties |
| `transform.get` | `objectId` | position / quaternion / scale |
| `transform.set` | `objectId`, `transform` | canonical native transform |
| `selection.set` | `objectId` или null | state |
| `editor.undo`, `editor.redo` | — | state |
| `play.start`, `play.pause`, `play.stop` | — | state |
| `bridge.describe` | — | dirty / path / selection / play / undo state |
| `reflection.getComponentTypes` | — | concrete reflected component types |
| `assets.getScenes` | — | AssetRegistry scene IDs and virtual paths |

Пример typed client:

```ts
const result = await bridge.request({
  method: 'transform.set', context,
  params: {objectId, transform: {
    position: [1, 2, 3], rotation: [0, 0, 0, 1], scale: [1, 1, 1]
  }}
});
if (!result.ok) showError(result.error.code, result.error.message);
```

### Свойства и валидация

Для компонентов нет отдельных DTO. Codec получает native type ID и данные Reflection.
`engine.int64` передаётся **десятичной строкой**, в том числе внутри reflected structs
и arrays. Остальные значения используют существующую типизированную JSON-сериализацию.
Object references содержат persistent IDs, asset references — существующий AssetKey.
Недоступные codec/read операции возвращают diagnostic в записи свойства.

Проверяются whitelist методов/полей, типы, размер/глубина JSON, конечность чисел,
float range, transform vectors, quaternion, ненулевая scale, parent cycles,
владелец компонента, reflection flags/ranges, concrete component class и AssetRegistry.
Удалённые IDs дают `NOT_FOUND`; pointers и адреса в протокол не попадают.
Scene paths относительны project Content, canonical path не может выйти за его корень.
Dirty Open требует Save или явного `discard:true`.

Нет eval, произвольного C++ вызова, адресов функций или доступа renderer к Node/native.
Main принимает только свой mainFrame с доверенного app URL. Web не имеет preload;
`createNativeBridgeClient()` в Web возвращает null.

### События и отключение

`state.changed` содержит context и компактный native state. Клиент читает нужные
snapshots; они не становятся вторым изменяемым world. После команды событие публикуется
сразу, изменения от native gizmo / gameplay обнаруживаются примерно раз в 240 ms.
Наблюдение сканирует hierarchy/properties на Game thread, по сравнению содержимого.
Это корректный базовый контракт для небольших сцен; O(scene) polling следует заменить
native change journal при масштабировании.

В очереди не более 64 запросов, request ≤8 MiB, response ≤8 MiB, timeout 30 s. Clipboard `subtree` uses `serializedJson`: up to 1 MiB of UTF-8 JSON, validated after parsing; ordinary text limits remain unchanged.
Timeout означает неизвестный результат выполнения: автоматического повтора mutations нет.
При закрытии/сбое/смене проекта все pending Promises завершаются `DISCONNECTED`,
подписчики получают событие. Старые ответы не восстанавливают context прежнего процесса.
GPU teardown продолжает использовать существующее ожидание release ACK и StopPresenting.

## React и запуск

В Scene Viewport откройте **Native Bridge** справа сверху. Доступны создание,
selection, rename/delete, XYZ position, Undo/Redo, Save и Play/Pause/Stop.
Это небольшая рабочая панель для базового контракта. Общие Hierarchy/Inspector/Story
пока не подключены к native данным. Их прежние JS authoring/undo/play пути в desktop
заблокированы с объяснением; данные этих панелей не являются состоянием C++ сцены.
Web сохраняет прежнее редактирование и Three.js renderer.

Из `desktop-electron`:

```powershell
npm ci
npm run native:configure
npm run native:build
npm run build:ui
npm run native:start
```

`native:start` открывает изолированную копию SampleProject. Для своего проекта
используйте кнопку открытия C++ `.project` в viewport либо запуск Electron с
`SACURA_NATIVE_PROJECT` (полный путь к descriptor). Результат `desktop:build` находится
в `release/win-unpacked`. CMake deploy копирует `BridgeContract.json` рядом с host;
packaging включает схему, JS transport и native runtime.

## Автоматическая проверка

```powershell
npm run test:bridge       # Реальный C++ host, схема/типы, transport, client
npm run native:smoke      # React → preload → main → Engine, реальный GPU viewport
npm test                 # Также существующие desktop tests
npm --prefix ../engine-web-prototype/frontend test
```

Headless Bridge test сам создаёт временный проект, запускает настоящий host и
проверяет все команды, ошибки, события native изменений, subtree/component restoration,
save/open, PlayWorld, invalid/stale IDs, границы Content, отключение и новый процесс
с сохранёнными persistent IDs. Tests не заменяют Engine mock-объектом.
В Electron smoke React выполняет create → selection → transform → undo/redo → delete
→ invalid ID; параллельно проверяются реальные meshes, native input/gizmo, resize,
dock layout, scale, overlays, project replacement и GPU shutdown.
Computer Use и UI automation не использовались: только project CLI test hooks.

Измерения Debug build на этой машине:

* Bridge suite: **7/7**; 47 успешных native команд плюс negative cases.
  Roundtrip p50 **15.93 ms**, p95 **19.18 ms**, max **34.36 ms**
  (учитывает QTimer polling и IPC, маленькая SampleProject scene).
* Electron GPU/React smoke: PASS; 15.1 s измерений, 936 frames, **61.96 FPS**.
  Frame interval p95 **16.5 ms**, GPU-ready → browser draw submission p95 **15.78 ms**.
  Эти величины не являются GPU execution time или задержкой до фотона на дисплее.
* Web suite: **802/802**.
* Повторный запуск финальной сборки: Bridge **7/7**, roundtrip p50 **15.94 ms**,
  p95 **22.03 ms**, max **37.14 ms**.
* Упакованный Electron с bundled native host, scale 125%: **PASS**, включая
  React Bridge команды, смену native process и корректный shutdown.
* EditorCore `Native.Regression.*`: **5/5**. Qt resize regression: **PASS**,
  200 свежих кадров за 7580 ms при непрерывном resize, 0 buffer rebuilds;
  финальная проверка real render/input/overlays завершилась успешно.

Логи/артефакты: `build/native-bridge-tests.log`, `native-bridge-web-tests.log`,
`native-bridge-electron-smoke.log`, `electron-native-smoke/report.json` и `viewport.png`.
Финальный пакет: `build/electron-bridge-packaged/report.json`;
Qt/Core: `build/native-bridge-qt-smoke.log`, `build/native-bridge-core-tests.log`.

## Изменённые файлы этого этапа

* `desktop-electron/bridge/{contract.json,contract.d.ts,transport.cjs}` — схема,
  типы и асинхронный transport.
* `desktop-electron/native/{NativeBridge.h,NativeBridge.cpp,ControlPipe.h}` —
  C++ dispatcher, reflection codec, события и framing.
* `desktop-electron/native/{Host.cpp,ViewportController.h,ViewportController.cpp,CMakeLists.txt}` —
  общий SceneWorkspace, headless test host, PlayWorld и deploy контракта.
* `desktop-electron/{main.cjs,preload.cjs,native-viewport.cjs,package.json,package-lock.json}` —
  безопасная поверхность IPC, lifecycle, packaging и TypeScript test dependency.
* `engine-web-prototype/frontend/src/{nativeBridgeClient.js,NativeBridgePanel.jsx,NativeViewport.jsx,Editor.jsx}` —
  native client/workbench, защита старых snapshots и отключение независимых JS mutations в desktop.
* `Root/EditorCore/src/Commands/EditorCommands.cpp` — восстановление целей Undo/Redo
  по persistent ID, без изменения публичных интерфейсов.
* `desktop-electron/tests/{bridge-contract.test.cjs,bridge-types.ts,native-bridge.test.cjs}`,
  `desktop-electron/scripts/native-smoke-main.cjs`,
  `engine-web-prototype/frontend/tests/nativeBridgeClient.test.js` — проверки.
* `desktop-electron/{README.md,NATIVE-BRIDGE.md}` — документация.

Существовавшие до этого этапа изменения RHI/shared texture и чужие изменения Qt/UI
не относятся к реализации Bridge. Qt UI исходники в этом этапе не редактировались.

## Ограничения и следующий этап

* Windows / Electron 45 / D3D12 shared texture — проверенная конфигурация viewport.
  Bridge не добавляет D3D11 или другие ОС.
* Load/save и команды выполняются в native Game thread. Electron UI не блокируется,
  однако большой document/Play clone может остановить native frames на время операции.
  Нужны CPU parsing snapshots / worker I/O и короткая Game-thread publication для больших сцен.
* Нет paging больших hierarchy responses, native change journal, cancellation уже начатой
  mutation или автоматического crash recovery. При превышении transport limit канал закрывается
  и pending команды получают ошибку отключения. Session/epoch предотвращают повтор старых команд.
* Редактирование PlayWorld запрещено; все scene mutations идут в EditWorld через EditorCore.
* Следующий этап: подключить существующие Hierarchy/Inspector к этому контракту и reflection
  metadata, затем добавить granular changes, paging и dirty-close UX. Автоматически он не начат.
