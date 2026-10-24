# Sacura Novel Engine 3D

Движок на C++ с редактором и игровым Player на React + Electron. Симуляция, StoryRuntime, Python и рендеринг остаются в нативном SakuraElectronHost. Qt-интерфейс, Qt-Player и прежний MCP bridge удалены.

## Сборка и запуск

Требуются Windows x64, Visual Studio 2022 с C++, CMake и Node.js/npm. Нативные зависимости собираются из закреплённых исходников; официальный CPython загружается автоматически. Qt SDK не требуется.

Из корня репозитория:

```powershell
npm --prefix desktop-electron install
npm --prefix engine-web-prototype/frontend install
npm --prefix desktop-electron run native:configure
npm --prefix desktop-electron run native:build
npm --prefix desktop-electron run build:ui
npm --prefix desktop-electron run native:start
```

Пресеты используют build/windows-msvc-debug и build/windows-msvc-release. Если используется другой существующий каталог сборки, задайте SACURA_NATIVE_HOST абсолютным путём к SakuraElectronHost.exe. SACURA_NATIVE_PROJECT задаёт открываемый .project.

Для готового комплекта редактора и экспорта Game.exe см. [DISTRIBUTION.md](docs/DISTRIBUTION.md).

## Модули

- Root/Engine — сцены, компоненты, ассеты, Enhanced Input, Python, StoryRuntime, Diligent rendering.
- Root/EditorCore — документы, импорт, история команд и проверка изменений.
- Root/Editor/ThirdParty — зависимости редактора: ufbx и ImGuizmo.
- desktop-electron/native — Win32-хост, GPU transport и типизированный мост.
- desktop-electron — Electron shell, отдельный Player и экспорт.
- engine-web-prototype/frontend — общие React-компоненты редактора и Player.
- Samples/SampleProject — пример внешнего пользовательского проекта.

Проект не является CMake target: .project, Content и Config открываются во время работы. Объекты и компоненты сохраняют постоянные идентификаторы; Renderer читает неизменяемые снимки на отдельном потоке. Подробные архитектурные правила находятся в [AGENTS.md](AGENTS.md).

## Перенесённые возможности

Python Narrative редактируется в общем Story graph: каталог Python-типов, типизированные входы и выходы, связи данных, именованные переходы, проверка и Undo/Redo. Префабы открываются в изолированном workspace с Inspector слева, viewport в центре и Hierarchy справа; доступны overrides, apply/revert и unpack. Экспорт создаёт самостоятельный Electron Player с React-диалогами, выбором, паузой и нативным игровым миром.

Остальные возможности, существовавшие только в удалённом Qt-интерфейсе, не перенесены. Существующие React-редакторы и Enhanced Input runtime сохранены. Внешний MCP-адаптер для Electron пока отсутствует; см. [MCP-EDITOR.md](docs/MCP-EDITOR.md).

## Проверки

```powershell
cmake --build build/windows-msvc-debug --config Debug --target SakuraElectronHost SakuraIoServicesTest SakuraAssetCreationFactoryTest --parallel 8
ctest --test-dir build/windows-msvc-debug -C Debug --output-on-failure
npm --prefix desktop-electron test
```

Интеграционные тесты проверяют мост, документы, Narrative, префабы, Player и экспорт. Проектные Electron helpers сохраняют отчёты и снимки реальных GPU-окон; Computer Use в этом проекте запрещён.
