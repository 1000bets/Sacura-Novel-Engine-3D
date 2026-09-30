# Sacura Novel Engine 3D

Движок для визуальных новелл в 3D и историй с point-and-click. Реплика может запускать постановку: персонаж подходит к окну, камера меняет план, начинается дождь, музыка уходит тише под голос.

Сейчас в репозитории — C++-каркас движка в `Root/` (сцена, GameObject/Component, подсистемы) и сборка через CMake. Графический стек: Qt6 (окна Editor и Player, runtime UI) + Diligent Engine (RHI над D3D12 / Vulkan / OpenGL). SDL3 сохранён для низкоуровневого host и тестов.

## Сборка

На Windows нужны CMake 3.25+, Visual Studio 2022 с компонентами Desktop development with C++ и C++ CMake tools for Windows (включая Ninja), Windows SDK, Python 3 x64 и Git. Отдельно устанавливать Qt не нужно. Из корня:

```sh
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target SakuraEditor --parallel 4
```

Первая конфигурация скачивает исходники Qt Base v6.8.3, собирает shared-библиотеки и инструменты Qt с Ninja и устанавливает их внутри `build/windows-msvc-debug/Qt/`. Это занимает время уже на шаге `cmake --preset`. Последующие конфигурации используют готовую сборку; при изменении версии Qt или скрипта его подготовки сборка обновляется. Исходники лежат в `Root/Engine/ThirdParty/Qt/Source/` и исключены из Git. Число параллельных задач Qt регулируется параметром `-DSAKURA_QT_BUILD_JOBS=4`. Тесты и примеры самого Qt отключены.

После сборки staged layout: `build/windows-msvc-debug/Stage/Bin/SakuraEditor.exe` + `build/windows-msvc-debug/Stage/Engine/{Content,Shaders,Config}`. Qt DLL и плагины копируются автоматически.

Открытие проекта:

```sh
build/windows-msvc-debug/Stage/Bin/SakuraEditor.exe --project "M:/path/to/MyGame/MyGame.project"
```

Без `--project` открывается Project Browser (New / Open / Recent).

Основной подход для сторонних библиотек — скачивание закреплённых исходников и сборка через CMake. Зависимости находятся в `Root/Engine/ThirdParty` или `Root/Editor/ThirdParty` (у владельца модуля), не в общей Root-папке. Qt собирается отдельно перед `find_package(Qt6)`, поскольку подключение его исходников через `add_subdirectory` не поддерживается. vcpkg и aqtinstall не требуются.

Чтобы явно использовать внешний Qt SDK, конфигурируйте отдельный каталог с `-DSAKURA_BUILD_QT_FROM_SOURCE=OFF` и задайте `Qt6_DIR` или `SAKURA_QT_ROOT`. В основном режиме эти пути не выбирают внешний SDK. Для Release используйте пресет `windows-msvc-release`: Qt собирается для него отдельно. На Linux необходимы Ninja и системные зависимости платформы Qt; Linux-путь не проверялся.

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
