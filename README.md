# Sacura Novel Engine 3D

Движок для визуальных новелл в 3D и историй с point-and-click. Реплика может запускать постановку: персонаж подходит к окну, камера меняет план, начинается дождь, музыка уходит тише под голос.

Сейчас в репозитории — C++-каркас движка в `Root/` (сцена, GameObject/Component, подсистемы) и сборка через CMake. Графический стек: Qt6 (окна Editor и Player, runtime UI) + Diligent Engine (RHI над D3D12 / Vulkan / OpenGL). SDL3 сохранён для низкоуровневого host и тестов.

## Сборка

Нужны CMake 3.20+, C++17 компилятор, Python 3 (для Diligent), Git и **Qt 6** (модули Core, Gui, Widgets) как зависимость Engine. Из корня:

```sh
# Пример: локальный Qt (aqt) в Root/Engine/ThirdParty/Qt/<ver>/<arch>
cmake -S . -B build
cmake --build build --config Debug --target SakuraEditor
```

После сборки staged layout: `build/Stage/Bin/SakuraEditor.exe` + `build/Stage/Engine/{Content,Shaders,Config}`.

Открытие проекта:

```sh
build/Stage/Bin/SakuraEditor.exe --project "M:/path/to/MyGame/MyGame.project"
```

Без `--project` открывается Project Browser (New / Open / Recent).

Зависимости подтягиваются из `Root/Engine/ThirdParty` или `Root/Editor/ThirdParty` (у владельца модуля), не из общей Root-папки. Qt ищется через `find_package(Qt6)`; можно задать `Qt6_DIR`, `CMAKE_PREFIX_PATH` или `SAKURA_QT_ROOT`.

## Структура

- `Root/Engine` — runtime движка (без пользовательских проектов)
- `Root/Engine/Content` — engine content (`/Engine/...`)
- `Root/Engine/ThirdParty` — Qt6, SimpleMath, SDL3, Diligent, PhysX, asset libs
- `Root/Editor` — Editor library + `apps/SakuraEditorMain.cpp`
- `Root/Editor/ThirdParty` — editor-only (ufbx)
- `Samples/SampleProject` — пример внешнего проекта (`.project` + Content/Scripts/Config)
- `AGENTS.md` — архитектурные правила

## Текущее состояние

Базовый runtime-каркас есть. Полноценный игровой цикл, рендер-пайплайн и редакторский viewport — в работе.

План развития графики и критерии готовности: [RenderingRoadmap](Docs/RenderingRoadmap.md). Архитектурные изменения runtime UI и владения сценами: [Task 15](Docs/Implementation/Task-15.md).
