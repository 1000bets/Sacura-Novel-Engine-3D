# Физика и NavMesh

Система использует PhysX 5.6.1 для столкновений и движения тел, Recast Navigation 1.6.0 для построения сетки и Detour/DetourCrowd для поиска пути и обхода персонажей. Подход к настройке соответствует основным понятиям Unreal Navigation System: Bounds Volume, профили агентов, области стоимости, Nav Links и режимы генерации.

## Запуск примера

Архитектурный ориентир: [Unreal Engine Navigation System](https://dev.epicgames.com/documentation/en-us/unreal-engine/navigation-system-in-unreal-engine). Используемая библиотека: [Recast Navigation](https://github.com/recastnavigation/recastnavigation). Версия задаётся в CMake через `SAKURA_RECAST_GIT_TAG`; стандартный путь сборки скачивает закреплённые исходники.

Из корня репозитория после Debug-сборки:

Для полного сценария примера требуется стандартная сборка с включённым Python, поскольку переход между платформами реализован проектным скриптом. Сами физика, построение сетки и C++-агенты работают и с `SAKURA_ENABLE_PYTHON=OFF`.

```powershell
M:\Sacura-Novel-Engine-3D\build\Root\Debug\SakuraEditor.exe --project M:\Sacura-Novel-Engine-3D\Samples\PhysicsNavigation\PhysicsNavigation.project
```

Нажмите P в viewport для показа сетки. Меню «Навигация» позволяет перестроить сетку и включить отображение коллизий. Во вкладке статистики выводятся состояние профиля, число тайлов/полигонов, размер данных и время построения.

В Play продолжите реплику во вкладке «Сценарий». Персонаж обходит стену, затем по следующей реплике поднимается по ступеням. Последний этап проверяет переход между двумя платформами через Nav Link. Падающий ящик использует динамическую физику, пол — треугольную коллизию импортированной StaticMesh. `Scripts/LinkTraversal.py` выполняет пример пользовательского перехода по дуге.

Данные сцены воспроизводятся командой:

```powershell
python M:\Sacura-Novel-Engine-3D\Samples\PhysicsNavigation\GenerateScene.py
```

## Граница PhysX

`Root/Engine/inc/Physics/IPhysicsWorld.h` определяет интерфейс физического backend. Закрытый класс `PhysXWorld : IPhysicsWorld` находится в `Root/Engine/src/Physics/Backends/PhysXWorld.cpp`. Он отвечает за SDK, акторы, cooking, character controllers, фильтрацию и сбор событий. `PhysicsWorld` связывает этот интерфейс со сценой и компонентами. Игровой код не включает заголовки PhysX.

Каждая Scene имеет отдельный физический мир. В редакторе EditWorld не симулируется; Play создаёт отдельные тела и контроллеры. Остановка Play уничтожает их, исходные трансформы остаются в EditWorld. Pause замораживает симуляцию, Step выполняет один шаг 1/60 секунды. Обычный кадр выполняет максимум восемь фиксированных шагов.

Единицы: метры, килограммы и секунды; вертикаль — Y. Скорость задаётся в м/с, сила — в ньютонах, импульс — в Н·с, угловая скорость — в радианах/с.

## Компоненты физики

Добавьте Physics Body через меню создания объекта либо Physics Body Component к существующему объекту. Поле Motion Type задаёт 0 Static, 1 Dynamic, 2 Kinematic, 3 Character. Shape задаёт 0 Box, 1 Sphere, 2 Capsule, 3 Triangle Mesh.

- Box Half Extents — половина размеров коробки; Shape Center — локальное смещение её центра.
- Capsule Cylinder Half Height — половина длины цилиндра без полусфер. Полная высота капсулы равна `2 * (Radius + CapsuleHalfHeight)`.
- Для Triangle Mesh выберите Collision Mesh через Inspector. Если поле пустое, используется StaticMesh владельца MeshRenderer. Поддерживаются Static и Kinematic; динамический треугольный меш и треугольный trigger отклоняются с диагностикой.
- Dynamic использует массу, трение, упругость, затухание и гравитацию. Continuous Collision Detection предназначен для быстрых динамических тел.
- Kinematic следует трансформу, заданному игровым кодом; силы не управляют им.
- Collision Layer содержит ровно один бит из 32, Collision Mask — разрешённые слои. Для физического столкновения обе маски должны разрешать другую сторону.
- Physical Collision включает блокирующую коллизию. Scene Queries отдельно включает видимость тела для raycast/sweep/overlap. Trigger выдаёт события пересечения без блокирования. Тело только для queries не блокирует персонажа; физическое тело с выключенными queries продолжает блокировать его.
- Affects Navigation включает коллайдер в геометрию сетки. Для подвижных мелких объектов обычно удобнее отключить это поле и использовать локальный обход.

Character Controller создаёт вертикальную капсулу. Начало координат объекта находится у ног, Shape Center должен быть нулём. Стандартные параметры: радиус 0.3 м, цилиндрическая полувысота 0.6 м, общая высота 1.8 м, ступень 0.3 м, наклон 45°. Визуальную модель удобно сделать дочерним объектом со смещением вверх. Контроллер использует вертикальную составляющую гравитации; произвольное направление Up не поддерживается.

Вызовы `AddForce` и `AddTorque` задают воздействие текущего кадра: вызывайте их каждый игровой Tick, пока сила должна действовать. `AddImpulse` и `AddAngularImpulse` задают разовое воздействие. Контакты Begin/Persist/End публикуются после завершения физики, без вызова игрового кода из callback SDK.

## Настройка NavMesh

У подвижного объекта должен быть один Physics Body. Дополнительные triggers или самостоятельные коллайдеры можно разместить на дочерних объектах; это отдельные физические тела, поэтому их слои должны исключать нежелательные столкновения с родителем. Сочетание нескольких тел на одном подвижном владельце отклоняется до запуска Play. Compound rigid body пока не реализован.

1. Создайте физические коллайдеры пола и препятствий. Одной видимой модели без Physics Body недостаточно.
2. Создайте NavMesh Bounds Volume и настройте Half Extents. Несколько объёмов одного профиля образуют объединение; пустое пространство между ними не заполняется сеткой.
3. Настройте Agent Profile, Agent Radius/Height, Maximum Step Height/Slope и размер ячейки. Размеры профиля должны быть не меньше физических размеров персонажа. Стандартный профиль имеет радиус 0.35 м и высоту 1.9 м, оставляя зазор для контроллера 0.3/1.8 м.
4. Объёмы одного профиля должны иметь одинаковые параметры построения и Generation Mode. Для другого размера персонажа создайте другой профиль.
5. Добавьте Navigation Agent Component объекту с Character Controller. Укажите тот же Agent Profile и задайте цель через C++, Python либо действие сценария NavigateTo.

Generation Mode:

- 0 Static: во время игры используется подготовленная сетка.
- 1 Dynamic: отслеживаются изменения коллизий, modifiers и links.
- 2 Dynamic Modifiers Only: исходная геометрия сохраняется, меняются области и связи.

В редакторе Auto Build управляет автоматической сборкой. При запуске Play необходимая начальная сетка готовится и с выключенным Auto Build. Построение работает на JobSystem с копией CPU-данных. Изменения положения существующих объектов перестраивают затронутые тайлы; изменения топологии геометрии, bounds или параметров требуют полной сборки. Старый корректный динамический mesh доступен во время следующей сборки.

Navigation Modifier Volume задаёт Area и Priority. Area 0 удаляет проходимость, 1 — обычная поверхность, 2–16 — пользовательские области. Более высокий Priority применяется позже. Пустой Agent Profile применяет modifier ко всем профилям. Стоимость области задаётся на агенте через SetAreaCost / SetNavigationAreaCost; значения начинаются с 1. В Excluded Areas бит 0 соответствует Area 1, бит 1 — Area 2 и так далее.

Navigation Link хранит локальные Start/End, радиус привязки, Area и Bidirectional. При достижении связи агент переходит в WaitingForLink. Игровой код выполняет прыжок, подъём либо другое движение и вызывает CompleteLinkTraversal. Успешное завершение разрешено только рядом с конечной точкой; автоматического телепорта нет. Отключение/удаление связи учитывается при перестроении.

Поиск различает Complete, Partial, NotReady, StartOutsideNavigation, GoalOutsideNavigation, Unreachable, SearchLimitReached и InvalidInput. Частичный путь разрешается явно и завершается состоянием PartialReached. Профиль без Bounds, неподходящий контроллер и недостижимая цель дают Failed с диагностикой. Агент ограничивает движение границей сетки, но окончательное положение определяет физический контроллер. Неподвижное блокирование завершается состоянием Blocked.

## C++ и Python

В C++ используйте `Scene::GetPhysics()`, `PhysicsBodyComponent`, `Scene::GetNavigation()` и `NavigationAgentComponent`. Для queries доступны Raycast, Sweep, Overlap, FindPath, ProjectPoint, Raycast по сетке, MoveAlongSurface и FindRandomPoint. Данные запросов используют типы движка; нативные SDK-типы не возвращаются.

В Python объект контекста — обычный `engine.Object`, в компоненте он доступен как `self.owner`. Координаты принимают `engine.Vector3` либо тройку чисел:

```python
import engine

Request = engine.MoveTo(Self.owner, (4.0, 0.0, 0.0))
State = engine.GetNavigationState(Self.owner)
Path = engine.FindNavigationPath(Self.owner, (-4.0, 0.0, 0.0), (4.0, 0.0, 0.0))
Hit = engine.Raycast(Self.owner, (0.0, 5.0, 0.0), (0.0, -1.0, 0.0), 10.0)
```

Доступны StopNavigation, GetNavigationDiagnostic, SetNavigationAreaCost, ProjectPointToNavigation, GetPendingNavigationLinkEnd, CompleteNavigationLink, AddForce, AddImpulse, AddTorque, AddAngularImpulse, SetPhysicsVelocity, GetPhysicsVelocity, SetAngularVelocity, GetAngularVelocity, SetCharacterVelocity, Jump, IsGrounded, SphereSweep, OverlapSphere, GetPhysicsContacts и SetWorldGravity. Объекты разрешаются с проверкой поколения; обращение через удалённый объект выдаёт Python-ошибку.

В сценарии NavigateTo принимает постоянный ID объекта, мировую position и необязательный timeout в durationSeconds. Действие ждёт физического достижения цели. Существующий MoveTo сохраняет линейное постановочное движение в локальных координатах и временно владеет трансформом, отменяя навигационное движение.

## Сохранение и ограничения

Сцена сохраняет компоненты, гравитацию и готовые тайлы с контрольными суммами. Нативные тела, скорости, активные запросы и crowd не сохраняются. Старые сцены без секций physics/navigation остаются читаемыми; сетка строится из компонентов. Текущий baked-формат привязан к версии Detour и размеру его polygon identity.

Текущий бюджет: до 32 профилей, 4096 тайлов на профиль, 2 миллионов вершин и 6 миллионов индексов во входной геометрии. DetourCrowd использует до 256 агентов и 16 одновременно разных фильтров на профиль; при отсутствии crowd-слота сохраняется обычное следование пути с физической коллизией. Projection extent ограничен 32 ширинами тайла по X/Z и 1000 м по Y, чтобы запрос не обходил неограниченную область.

Это реализация основного сценария UE-подобной навигации. Streaming мира, navigation invokers, многоуровневый tile cache, физические constraints, ragdoll, деформируемая/скелетная collision geometry и сетевой prediction не входят в текущую систему. Неоднородное масштабирование примитивов аппроксимируется: сфера берёт максимальный масштаб, капсула — максимальный X/Z для радиуса. Физические объекты должны иметь обратимый трансформ без shear.

## Локальные проверки

При наличии локального набора Root/Tests:

```powershell
cmake --build M:\Sacura-Novel-Engine-3D\build --config Debug --target SakuraPhysicsNavigationTest SakuraPhysicsNavigationEditorTest --parallel 6
ctest --test-dir M:\Sacura-Novel-Engine-3D\build -C Debug -R "^(Engine|Editor).Physics.Navigation$" --output-on-failure
```

Headless-тесты проверяют физику, filters, contacts, controllers, queries, построение/сохранение/перестроение сетки, профили, области, links, изоляцию PlayWorld, Python и выполнение сценария. Qt-тест открывает редактор, проверяет меню/Inspector/Undo/Play/Pause/Step и сравнивает реальные пиксели viewport до и после включения NavMesh.
