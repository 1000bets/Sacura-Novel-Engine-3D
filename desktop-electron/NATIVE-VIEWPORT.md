# C++ Engine в Electron: первый этап (историческая запись)

**Текущее состояние:** [интерактивный viewport](INTERACTIVE-VIEWPORT.md) использует
тот же host и Engine, дополняя их GPU shared texture export для React overlays.
Описанный ниже видимый child HWND — результат первого этапа, не текущий desktop путь.

Рабочая ветка: `feature/electoron_ui`. Прототип включается отдельной командой
`native:start`. Он не связывает React Inspector, Hierarchy или Story с C++.

## Архитектура существующего движка

- `Root/Engine/inc/Engine.h`, `src/Engine.cpp`: `InitializeHeadless`, загрузка
  ресурсов, `AdoptScene`, `StartPresenting`, `Tick`, `StopPresenting`, `Shutdown`.
- `ProjectSession::OpenProject` открывает `.project`, монтирует Game Content и
  Scripts, загружает startup scene через `SceneSerializer`. Engine владеет Scene.
- `PresentationController` и `SceneExtractor` на Game thread формируют неизменяемый
  `RenderFrameData`. `RenderThread` владеет Renderer; world pointers в него не идут.
- `Rendering/RHI/RenderDevice` создаёт Diligent device и swapchain по
  `NativeWindowInfo`. Реализованы D3D12, Vulkan, OpenGL; D3D11 backend отсутствует.
  `Renderer` выполняет forward rendering и Present на Render thread.
- `SakuraPlayerMain.cpp` показывает существующий host lifecycle с QApplication и
  QTimer. Новый host использует тот же цикл, но Win32 child HWND вместо QWidget.
- `Root/EditorCore`: SceneDocument, EditorCommandStack, import/asset tools —
  редакторские операции над Engine. Для доказательства viewport они не нужны;
  новый executable линкует только Engine, Qt-наследование и системные Win32 DLL.
- `desktop-electron`: main владеет BrowserWindow, preload предоставляет узкий
  IPC с context isolation и sandbox. Существующий React Editor сохранён;
  условно заменён только scene viewport. Qt Editor не изменён.

## Исследование API и выбор

Electron зафиксирован на **45.0.0**. `npm run native:probe` проверяет реально
установленный runtime: main import/sendSharedTexture, renderer receiver,
VideoFrame, sandbox/contextIsolation и `process.versions.napi` (10). Probe также
получает shared BGRA texture от Electron OSR, импортирует её в main, передаёт
изолированному renderer, создаёт/закрывает VideoFrame и ждёт allReferencesReleased.
Этот цикл успешно выполнен. Это **импорт текстуры Chromium, не Diligent**;
Node-API addon не собирался, проверена версия поддерживаемого API.

| Вопрос | GPU shared texture → Chromium | Native child HWND — реализовано |
|---|---|---|
| Существующий Engine | Нужен новый offscreen/export endpoint внутри RHI | Уже принимает HWND в StartPresenting |
| DX11 / DX12 | Windows NT shared texture handle; D3D12 export/совместимость импорта Chromium требуют проверки на одном адаптере. D3D11 в нашем RHI отсутствует | Существующий D3D12 swapchain, обмена DX11↔DX12 нет |
| Синхронизация | Producer GPU completion + Chromium lifetime/release; ring buffers и поколения при resize | Существующие RenderThread, DXGI Present и DWM |
| Resize | Новый пул, старые textures живут до allReferencesReleased | React bounds → физические пиксели → HWND → Engine.ResizePresentation |
| DPI | codedSize отдельно от CSS-размера | devicePixelRatio; DPI context родительского HWND |
| Input | DOM events можно передавать Engine | Win32 мышь внутри child; React получает события вне него |
| Clipping | Chromium-композиция, DOM clipping/overlays | Прямоугольник, ограниченный viewport/overflow предков; сложные CSS clip/masks не поддержаны |
| HTML поверх сцены | Естественная композиция VideoFrame | Не поддержано: native airspace. Известные модальные состояния скрывают child; есть явная кнопка скрытия |
| Lifetime | NT handles process-local, DuplicateHandle; VideoFrame.close + release; нельзя переиспользовать занятую текстуру | Host останавливает Render thread до DestroyWindow; parent ждёт завершения host |
| CPU копирование | Можно избежать readback, но нужна GPU interoperability | Кадры вообще не проходят через IPC/JS; только Diligent → swapchain |
| Платформы | Типы handles различны: NT HANDLE, IOSurface, native pixmap; нужна реализация каждого producer | Только Windows; macOS/Linux не реализованы |

**Выбор:** child HWND позволяет проверить настоящий Engine сейчас. SharedTexture
не признан «несовместимым вообще»: в текущем Engine нет экспортируемой текстуры,
а существующая screenshot/readback функция не отвечает требованию GPU-пути.
Наличие API Electron само по себе не доказывает D3D12 interoperability.

`VideoFrame` — контейнер кадра, а не способ получить HWND/swapchain texture из
Engine. Для GPU-варианта нужен `getVideoFrame()` от imported shared texture и
явное освобождение frame. `sharedTexture` API экспериментальный.

Node-API подходит для небольшого `.node` transport/handle bridge с ABI-stable C API.
V8/Node-зависимые addons требуют соответствующей Electron сборки. Node-API не
решает GPU fences и ownership автоматически; такой addon здесь не требуется:
`child_process.spawn` передаёт родительский HWND, JSON pipe — только управление.
Engine живёт в отдельном процессе со своим QApplication, Python и Game thread.

Electron offscreen rendering выдаёт **кадры Chromium** наружу, в bitmap или shared
GPU texture. Это обратное направление относительно нашей задачи. Перенос React
в OSR сам по себе не подключает Engine к React viewport.

### Официальные источники

- [Electron sharedTexture](https://www.electronjs.org/docs/latest/api/shared-texture)
- [SharedTextureHandle и ограничения handles](https://www.electronjs.org/docs/latest/api/structures/shared-texture-handle)
- [Дизайн sharedTexture в исходниках Electron v45.0.0](https://github.com/electron/electron/blob/v45.0.0/shell/common/api/shared_texture/README.md)
- [Electron offscreen rendering](https://www.electronjs.org/docs/latest/tutorial/offscreen-rendering)
- [Electron native modules](https://www.electronjs.org/docs/latest/tutorial/using-native-node-modules)
- [Node-API и ABI stability](https://nodejs.org/api/n-api.html)
- [VideoFrame, WebCodecs](https://www.w3.org/TR/webcodecs/#videoframe-interface)
- [BrowserWindow / getNativeWindowHandle](https://www.electronjs.org/docs/latest/api/browser-window#wingetnativewindowhandle)
- [Microsoft D3D12 shared heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/shared-heaps)
- [Win32 child windows / clipping](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)
- [SetParent: стили и DPI](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent)

## Сборка и запуск

Из `desktop-electron` (обычные CMake prerequisites проекта: MSVC x64, CMake,
Python development, pinned dependencies; Qt собирается штатным bootstrap):

```powershell
npm ci
npm --prefix ../engine-web-prototype/frontend install
npm run native:configure
npm run native:build
npm run build:ui
npm run native:probe
npm run native:start
```

`native:start` запускает установленный Electron, создаёт отдельный временный
React-проект и копию SampleProject для C++. Engine открывает настоящий `.project`
с `.scene` и GLB mesh. Два mesh объекта переставляются в памяти, один вращается.
Исходные SampleProject и сохранённые проекты пользователя не переписываются.
Временные данные удаляются после закрытия. Сохранение React-проекта не сохраняет
C++ сцену: редактирование и persistence между ними намеренно не подключены.

В заголовке viewport — пауза вращения и скрытие; ЛКМ/drag — орбита, колесо —
расстояние камеры. Нет передачи gameplay keyboard input. Панели React работают
со своим прежним JS-проектом и **не отражают** два объекта C++ сцены.

`SACURA_NATIVE_HOST` позволяет указать другой собранный host exe. Сборка доставляет
Engine assets, Diligent, Qt, SDL3 и Python DLL/stdlib рядом с ним. Debug-версия
требует MSVC Debug CRT. Обычный `desktop:build` пока не упаковывает native runtime:
это локальный developer prototype, не законченный установщик.

## Проверка

```powershell
npm run native:smoke
# Дополнительный запуск с Chromium device scale 1.25:
$env:SACURA_NATIVE_TEST_OUTPUT = "$PWD/../build/electron-native-smoke-125"
npm run native:smoke -- --force-device-scale-factor=1.25
Remove-Item Env:SACURA_NATIVE_TEST_OUTPUT
```

CLI helper запускает настоящий Electron и host, проверяет WS_CHILD/GetParent,
загрузку и draw реальных meshes, соответствие DOM/Win32/swapchain после resize,
React IPC heartbeat, pause/resume анимации, minimize/restore и graceful shutdown.
Не использует browser driver, DOM injection или Computer Use. Screenshot helper
однократно снимает клиентскую область родителя через BitBlt: это диагностика,
никогда не транспорт кадров. Окно должно быть видимым и не перекрытым для снимка.
Артефакты — `build/electron-native-smoke/report.json`, `viewport.png` и stdout log.

## Ограничения и следующий этап

### Фактические результаты (23 октября 2026)

- Windows 11 Pro x64, build 26200; Electron 45.0.0; Diligent D3D12;
  NVIDIA GeForce RTX 5080. Системный DPI child HWND — 144 (150%).
- CMake Debug target `SakuraElectronHost` собран; production Vite UI собран.
- `native:probe`: main import/send, sandboxed renderer receiver, VideoFrame,
  OSR BGRA texture → import → send → VideoFrame.close → release callback — PASS.
- `native:smoke`: PASS; два разрешённых GPU mesh handle без pending/missing
  placeholder, два видимых объекта, 6 draw calls, 26 треугольников с сеткой.
- Окна 1100×760, 1450×900, 960×680 DIP: совпадение DOM-derived bounds, реального
  child HWND и swapchain, растущий frame index и React heartbeat — PASS.
- Pause/resume, minimize/restore, RenderThread joined до уничтожения parent — PASS.
- Повтор `--force-device-scale-factor=1.25`: PASS, включая те же resize-проверки.
  Это программно заданный Chromium scale, не проверка переноса между мониторами.
- Снимки обоих прогонов просмотрены: реальная C++ сцена внутри панелей React.
  `npm test`: 2/2 теста файлового хранилища. Computer Use не применялся.
- Не проверены вручную все React меню и мышиная навигация, непрерывное drag-resize,
  GPU device loss, другие GPU/backends/платформы. React heartbeat доказывает живой
  renderer/IPC; он не заменяет полную проверку взаимодействия со всеми панелями.

Проблемы, исправленные во время запуска: доставка SDL3/Python, скрытый Electron
при `windowsHide`, начальная компоновка React, перемешивание JSON с логами Render
thread. Текущие успешные артефакты лежат в `build/electron-native-smoke` и
`build/electron-native-smoke-125`; предыдущие ошибки не считаются успешными тестами.

### Оставшиеся ограничения

- Native surface не является DOM texture. CSS opacity, скругления, transforms,
  произвольные popovers и HTML поверх сцены не поддержаны. Известные блокирующие
  состояния скрывают HWND; новые UI overlays требуют такого же согласования.
- Resize проходит асинхронно через два процесса; краткое отставание при drag
  возможно. Нет обещания бесшовной композиции Chromium и DXGI.
- Только один viewport и один доверенный C++ sample project на процесс. Нет
  production packaging, recovery после GPU device lost или перезапуска host.
- Наличие VideoFrame/sharedTexture/Node-API проверяется отдельно от render path.
  Реальный импорт Diligent shared texture, GPU fence interoperability, macOS/Linux
  и смешанные DPI мониторы не проверены.
- Рекомендуемый следующий отдельный этап: RHI export texture pool для D3D12,
  producer fences, импорт/возврат ownership через sharedTexture, тест resize и
  HTML overlays. Затем можно решить, сохранять ли child HWND как fallback.
  Inspector/Hierarchy/Story не следует подключать до фиксации viewport contract.

Продуктовая проверка: запуск одной командой после сборки; пути/ID внутри viewport
не вводятся. Переносимость/редактируемость пользовательской логики не оценивалась:
этот прототип не меняет форматы и не интегрирует authoring. Универсальность — тот
же Engine и forward pipeline, без отдельного renderer или жанровой реализации.
Приёмка authoring A→B→C не заявляется.

## Изменённые файлы

| Файлы | Назначение |
|---|---|
| `Root/CMakeLists.txt` | Opt-in target `SAKURA_BUILD_ELECTRON_HOST`, default OFF |
| `desktop-electron/native/CMakeLists.txt`, `Host.cpp` | Host, Engine lifecycle, child HWND, геометрия, input, диагностический screenshot |
| `desktop-electron/native-viewport.cjs` | Запуск host, pipe protocol, состояние и graceful shutdown |
| `desktop-electron/main.cjs`, `preload.cjs` | Native mode, HWND, доверенный IPC, isolated bridge |
| `desktop-electron/package.json`, `package-lock.json` | Команды и pin Electron 45.0.0 |
| `desktop-electron/scripts/native-start.cjs` | Временные проекты и воспроизводимый запуск |
| `desktop-electron/scripts/native-smoke-main.cjs` | CLI интеграционная проверка |
| `desktop-electron/scripts/probe-native-bridge.cjs` | Реальная проверка Electron API/texture lifecycle |
| `engine-web-prototype/frontend/src/NativeViewport.jsx` | Геометрия/видимость HWND, состояние и управление preview |
| `engine-web-prototype/frontend/src/Editor.jsx` | Условная подмена viewport и начальная компоновка native mode |
| `desktop-electron/README.md`, `NATIVE-VIEWPORT.md` | Запуск, исследование, результаты и границы прототипа |

Изменения frontend `package.json`/`package-lock.json`, уже находившиеся в рабочем
дереве перед этой задачей, сохранены; они не относятся к этой реализации.
