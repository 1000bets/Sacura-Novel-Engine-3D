# Sacura Novel Engine 3D

Движок для визуальных новелл в 3D и историй с point-and-click. Реплика может запускать постановку: персонаж подходит к окну, камера меняет план, начинается дождь, музыка уходит тише под голос.

Сейчас в репозитории — C++-каркас движка в `Root/` (сцена, GameObject/Component, подсистемы) и сборка через CMake. Графический стек: Qt6 (окна Editor и Player, runtime UI) + Diligent Engine (RHI над D3D12 / Vulkan / OpenGL). SDL3 сохранён для низкоуровневого host и тестов.

## Сборка

На Windows нужны CMake 3.25+, Visual Studio 2022 с компонентами Desktop development with C++ и C++ CMake tools for Windows (включая Ninja), Windows SDK, Python 3 x64 и Git. Отдельно устанавливать Qt не нужно. Из корня:

CMake автоматически передаёт `core.longpaths=true` своим дочерним процессам Git, включая скачивание подмодулей зависимостей. Глобальные настройки Git и Windows не меняются, права администратора не нужны. Это устраняет ограничение Git при загрузке длинных путей вроде тестовых файлов SPIRV-Cross; ограничения других программ эта настройка не снимает. Если предыдущая конфигурация остановилась на `Filename too long`, повторите команду конфигурации ниже.

```powershell
Set-Location -LiteralPath 'M:\Sacura-Novel-Engine-3D'
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
- `Root/EditorCore` — операции над ассетами, документы, команды undo и абстрактные editor actions; работает без QApplication
- `Root/Editor` — Qt-оболочка с Shell, Panels, Viewport, Launcher и `apps/SakuraEditorMain.cpp`
- `Root/Editor/ThirdParty` — инструменты редактора: ufbx и ImGuizmo
- `Samples/SampleProject` — пример внешнего проекта (`.project` + Content/Scripts/Config)
- `AGENTS.md` — архитектурные правила

## Текущее состояние

Работают внешний project descriptor, сцены со стабильными идентичностями, reflection, Python-компоненты, загрузка и импорт ассетов, StoryRuntime с репликами, выбором и Wait/MoveTo/CameraCut, отдельный PlayWorld и Qt-хосты Editor/Player. Рендерер использует forward pipeline через Diligent; пределы профиля описаны в [AGENTS.md](AGENTS.md#minimal-forward-renderer-contracts).

Объектное основание находится в `Engine/inc/src/Core/Object`, мир — в `World`, очереди рендера — в `Rendering/Threading`, кадры — в `Rendering/Frame`, загрузка GPU — в `Rendering/Assets`. Пользовательские Content и Scripts остаются во внешних проектах. Перенос C++ файлов не меняет сериализованные TypeId и PropertyId.

Чтение трансформации через `GetTransform()` теперь возвращает const-ссылку. Для изменения скопируйте Transform и вызовите `SetTransform()`. Сценарий временно удерживает управление трансформацией во время MoveTo/CameraCut. Python callbacks сохраняют прежние имена, но `on_create` вызывается при BeginPlay, а `on_destroy` — при EndPlay; открытие сцены в редакторе игровое поведение не запускает.

## Локальная проверка

Тесты в `Root/Tests`, включая `TestTargets.cmake`, локальные и исключены из Git. Чистый checkout без них собирает Engine, EditorCore, Editor и Player. Если локальный набор установлен, сборка всех целей и прогон тестов выполняются так:

```powershell
Set-Location -LiteralPath 'M:\Sacura-Novel-Engine-3D'
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --parallel 6
ctest --preset windows-msvc-debug
```

Пресет исключает намеренно падающий тест с меткой `probe`. Локальный `SakuraArchitectureRegressionTest` проверяет отказ атомарной замены, мутации мира внутри Tick, откат групп команд, revision dirty state, перемещение открытой сцены и startup-путей, typed custom resources, кратность компонентов, CameraCut между разными родителями и диагностики Python. Он также выводит измерения extraction и asset resolution для сцен из 100, 1000 и 3000 объектов. Полный прогон включает GPU forward и Qt smoke tests.

Проверка сборки runtime без оболочки редактора использует пресет `windows-msvc-debug-engine-only`. Цель EditorCore при этом доступна. Screenshot helpers редактора позволяют проверить launcher и проект без UI-автоматизации:

```powershell
& './build/windows-msvc-debug/Stage/Bin/SakuraEditor.exe' --screenshot-launcher './build/windows-msvc-debug/launcher.png'
& './build/windows-msvc-debug/Stage/Bin/SakuraEditor.exe' --project './Samples/SampleProject/SampleProject.project' --screenshot-editor './build/windows-msvc-debug/editor.png'
```

Подробные правила владения сценами, потоками, ассетами и документами находятся в [AGENTS.md](AGENTS.md#root-system-refactoring-contracts).

## Material и Material Instance

Material (`.material`, версия 2) задаёт HLSL, область применения, настройки рендера и параметры по умолчанию. Material Instance (`.materialinstance`, версия 1) наследуется от Material или другого экземпляра и хранит только локальные переопределения. Оба типа назначаются мешам; для отдельных material slots можно задать свои экземпляры. Старые материалы версии 1 загружаются без изменения GUID и файлов; явное сохранение через EditorCore MaterialDocument переводит их в новый формат.

В Content Browser доступны Create Material, Create Material Instance и Create Child Instance. Material и Material Instance используют общий механизм активации ассетов; редактор этих типов можно подключить через RegisterAssetEditor. Загрузка, назначение и рендеринг материалов работают независимо от редактора ассетов. Контракт подключения будущего интерфейса описан в [MaterialEditorUiContract.md](Root/EditorCore/MaterialEditorUiContract.md); предложенная сессия редактирования пока не реализована.

Surface поддерживает Lit, Unlit и Toon с Opaque, Masked, Translucent или Additive. Standard PBR использует Base Color, Normal, Metallic/Roughness, Occlusion и Emissive maps. Камера хранит упорядоченный список постэффектов с интенсивностью и включённостью; материал определяет выполнение до или после tone mapping. UI-материалы назначаются отдельному `MaterialWidget` из Engine UI. Текст, кнопки и компоновка остаются обычными средствами Qt.

HLSL реализует `EvaluateSurface`, `EvaluatePostProcess` или `EvaluateUserInterface`; необязательная `ModifyVertex` требует объявленного расширения bounds. Объявления параметров создаёт движок. Используйте `GetMaterialParameters()`, `SampleImage(UV)`, `SampleImageLevel(UV, Level)` и `HasImage()` для параметра Texture2D с идентификатором Image. StaticSwitch доступен через макрос `MATERIAL_SWITCH_<Identifier>`. Полный интерфейс находится в `Root/Engine/shaders/MaterialContract.hlsli`, готовые примеры — в `Root/Engine/Content/Materials`.

`Engine::CreateDynamicMaterialInstance` запрашивает загрузку асинхронно и возвращает экземпляр после готовности CPU-данных. C++ меняет типизированные параметры через `SetParameter`/`ResetParameter`; меш получает экземпляр через `SetDynamicMaterial`, UI — через `MaterialWidget::SetDynamicMaterial`. Python предоставляет `engine.create_dynamic_material_instance(path)` с возвратом None во время загрузки, `get_parameter`, `set_parameter`, `reset_parameter`, назначение мешу/постэффекту и `engine.MaterialWidget`. Runtime-переопределения не записываются в ассеты и сцену; статические переключатели фиксируются при создании экземпляра.

Изменения документов, HLSL, include и используемых текстур обновляются автоматически. Ошибка компиляции сохраняет последнюю успешную версию и показывает диагностику с файлом и строкой. Обычные параметры используют общий shader variant; переключатели создают варианты по необходимости. Компиляция и GPU-ресурсы принадлежат Render Thread. Пределы первой версии: восемь текстур, восемь StaticSwitch и 4 KiB обычных параметров.

Editor и Player принимают `--backend Auto`, `--backend D3D12`, `--backend Vulkan` или `--backend OpenGL`. Дополнительный screenshot helper Player работает после запуска сцены:

```powershell
& './build/Stage/Bin/SakuraPlayer.exe' --project './Samples/SampleProject/SampleProject.project' --backend Vulkan --screenshot-player './build/player-vulkan.png'
```
