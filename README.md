# Sacura Novel Engine 3D

Движок для визуальных новелл в 3D и историй с point-and-click. Реплика может запускать постановку: персонаж подходит к окну, камера меняет план, начинается дождь, музыка уходит тише под голос.

Сейчас в репозитории — C++-каркас движка в `Root/` (сцена, GameObject/Component, подсистемы) и сборка через CMake. Графический стек: SDL3 (окно/input) + Diligent Engine (RHI над D3D12 / Vulkan / OpenGL).

## Сборка

Нужны CMake 3.20+, C++17 компилятор, Python 3 (для Diligent) и Git. Из корня:

```sh
cmake -S . -B build
cmake --build build --config Debug --target SakuraTest
```

Зависимости подтягиваются из `Root/Engine/ThirdParty` или `Root/Editor/ThirdParty` (у владельца модуля), не из общей Root-папки.

## Структура

- `Root/Engine` — ядро движка
- `Root/Engine/ThirdParty` — SimpleMath, SDL3, Diligent (Core/Tools/FX/Samples), Dear ImGui, PhysX
- `Root/Editor` — C++-редактор (каркас)
- `Root/Editor/ThirdParty` — только editor-only зависимости
- `AGENTS.md` — архитектурные правила (рендер, third-party, стиль кода)

## Текущее состояние

Базовый runtime-каркас есть. Полноценный игровой цикл, рендер-пайплайн и редакторский viewport — в работе.
