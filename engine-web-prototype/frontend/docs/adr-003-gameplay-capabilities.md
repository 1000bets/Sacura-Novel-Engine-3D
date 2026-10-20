# ADR-003: существующие игровые возможности на общем контракте

Дата: 13 октября 2026 года. Ветка: `feature/new-ui-fix`.
Статус: реализовано в текущей рабочей копии после ADR-001 и ADR-002.
Основание: [продуктовые требования](../../../docs/PRODUCT-REQUIREMENTS.md),
[аудит](gameplay-unification.md), [операции](adr-001-operation-foundation.md),
[общие правила](adr-002-common-rules.md).

## Решение и данные

GameSession остаётся фасадом PreviewRuntime. В нём остаются ссылки на проект,
сцену и объекты, общий жизненный цикл, RuleRuntime, вызов существующих Events и
функций, переходы истории и адаптер physicsApi. Команды здоровья, оружия,
инвентаря, взаимодействия, движения, камеры и таймеров вынесены в установленные
JS-модули. В command нет ветвления по жанру. Настройки preset используются только
старым явным конструктором шаблона и адаптером старых данных.

`registerCapability` подключает lifecycle, методы совместимости, операции и
читатели типизированных значений. Операции регистрируются через существующий
`registerOperation`: namespaced ID, версия, схемы аргументов и результата,
проверки, defaults, контексты и сериализуемые метаданные редактора. Общая форма,
Event, функция, правило, Input Action и выполнение используют эти определения.
Новый исполнитель добавляется модулем и импортом в gameCapabilities; списки
GameplayWorkspace и центральный dispatch менять не требуется. Это доверенный
код установленного движка. Импорт авторского пакета регистрацию не вызывает.

Авторские определения и назначения остаются в `project.gameplay`, объекты — в
`project.objects`. `capabilitySchemaVersion: 1` версионирует адаптер существующих
данных, не заменяя version 1 описаний механик, правил и операций. Состояние
прохождения хранится в `snapshot.world.game`. Функции и Events по-прежнему
сериализуются как редактируемые авторские данные, без замыканий.

| Модуль | Определения в project.gameplay | Собственное состояние / граница |
|---|---|---|
| `health.js` · sacura.health | actors[существующий objectId].maxHP | actors[id]: hp, maxHP, dead; только объекты с явно заданным здоровьем |
| `weapons.js` · sacura.weapon | weapons[] с явным ownerId | weapons[weaponId], selectedWeapons[ownerId]; отдельный магазин, резерв, cooldown, reload |
| `inventory.js` · sacura.inventory | inventory с owner; inventories[] с owner object/scene/project | inventories[kind:id]: items и equipment; ключ выводится из существующей ссылки |
| `interaction.js` · sacura.interaction | items[].worldObjectId, inventoryOwner, Event-привязки | подбор и маршрут используют выбранного получателя; сцена/проект могут владеть UI-инвентарём |
| `movement.js` · sacura.movement | controller.ownerId; movements[]; native object.physics | bodies, directions, disabled; прежние moveBody, collision, navigation и гравитация |
| `behavior.js` · sacura.behavior | actors[id].ai с явным targetId | behaviors[id]: направление, enabled, nextAttack; здоровье не требуется |
| `timers.js` · sacura.timer | параметры операции / существующие sensors правил | timers[kind:id:name]; общий game clock, пауза, repeat, отмена владельца |
| `camera.js` · sacura.camera | camera.mode/targetId; cameraSettings | cameraParameters, yaw/pitch; настройки позы отделены от controller |
| `camera.js` · sacura.crosshair | crosshair.enabled | независимый HUD overlay; не проверяет preset |
| `resources.js` · sacura.resource | resources[]: native ID, owner, integer initial | resources[id]; явный счётчик с владельцем |
| `legacyViews.js` | legacyCompatibility: gameplay-v1 | только прежние read views weaponId/kills; не синхронизирует переменные по имени |

Инвентарь не добавляет здоровье или оружие. Камера не добавляет движение или
стрельбу. Оружие не назначается герою автоматически: общий shoot/reload принимает
owner и необязательную ссылку на оружие именно этого владельца. Начальный выбор
оружия и каждый магазин изолированы. Запас инвентаря сцены или проекта не требует
пространственного объекта. Ссылки project/item/weapon/recipe/resource расширяют
существующий typed reference resolver; пользовательские имена не становятся ID.

`cameraModel.js` читает cameraParameters с fallback на старый controller; формулы
позы и Three.js-рендер сохраняются. В этом этапе не изменялись gamePhysics.js,
scenePhysics.js, GameControls.js, Scene.jsx и LocationScene.jsx относительно
состояния в начале этапа. Данные движения используются через прежний physicsApi,
нового ECS или покадрового запуска асинхронных Events нет.

## Смерть, сигналы, история и жизненный цикл

Damage меняет HP и dead, синхронизирует явные привязки, затем публикует hit и
death с изменённым объектом как target, даже если вызвавшее правило имело другую
цель. Выстрел сначала списывает патрон и синхронизирует его привязки; source
попадания — владелец оружия. Подбор
сначала меняет выбранный инвентарь и видимость предмета, затем выполняет его
прежний pickup Event и публикует pickup. Ошибки и отмена Event сохраняют прежний
порядок; новый сеанс не получает отложенный сигнал старого.

Death самостоятельно не скрывает тело, не отключает управление или оружие,
не увеличивает kills, не вызывает Game Over и не выбирает продолжение истории.
Эти последствия задаются обычными редактируемыми правилами. Победа — явный
сигнал через зарегистрированную операцию, а не встроенный исход damage.
AI имеет выбранную цель; отсутствующая цель не заменяется героем.

Регистрация систем задаёт прежний порядок обновления: behavior выбирает
направление, movement выполняет native интеграцию, weapons обновляет снаряды и
перезарядку, timers — игровые часы/сенсоры. После update выполняется sync и
публикуются накопленные сигналы движения/таймеров. Damage из оружия публикует
сигналы после собственной транзакции состояния. Обработка правил, снимок
условий, лимиты, once/cooldown и reentry остаются в RuleRuntime из ADR-002.

Стоп/рестарт защищены generation существующего runtime. Удаление владельца и
смена сцены останавливают принадлежащий ему Event через controlInstance,
включая его cleanup; native assert проверяет владельцев перед следующим
действием. Контекст Event сохраняет только данные, без сервиса capabilities и
исполнительных замыканий. Перезарядка и снаряды старой сцены отменяются, таймеры
сцены/объекта удаляются, таймер проекта может пережить смену сцены. Пауза
останавливает общий игровой clock и системы. Физический детерминизм между
платформами этим решением не обещается.

## Привязки UI, ввода и истории

HUD читает типизированный binding: capability, owner reference, field,
valueType и, где нужно, recordId оружия/счётчика. В редакторе выбирают именованное
поле выбранного владельца. Progress использует также типизированный maxBinding;
inventory widget выполняет equip/use/combine для своего владельца через общие
операции. Старые string path bindings читаются для совместимости и раскрываются
в дополнительных настройках; новый обычный сценарий их не требует.

История связывается через явные gameplay.stateBindings с существующими stable
variable references. Одноимённые hp/ammo/kills сами по себе больше не изменяются.
Коллекции, boolean и численные поля проверяются по схемам. Number-источник в
integer-переменной обновляется только при целом значении: для дробного здоровья
следует выбрать number-переменную. Произвольное округление не вводится.

Input Action с behavior operation содержит тот же call, выражения и native
контекст, что функции и правила. Для оружия кнопки подключаются явным действием
«Подключить клавиши и мышь»; установка оружия сама не занимает глобальный fire.
Удаление механики удаляет только её собственные назначения ввода. Камера
от первого лица не устанавливает оружие или прицел. У нового проекта без
выбранного HUD стандартные показатели HP/Ammo автоматически не показываются.

## Недеструктивное чтение старых проектов и сохранений

capabilityMigration переносит старые данные в те же поля рабочей копии:
назначает старого playerId владельцем уже существующего controller/оружия,
получателем инвентаря и явной целью старого AI; копирует параметры камеры;
сохраняет прежний прицел отдельным полем. Shipped JSON-файлы не переписываются.

Старые зарегистрированные variableBindings превращаются в явные typed bindings.
Для старого шутера создаются обычные death rules: отключение управления,
поведения и оружия, скрытие врагов, изменение отдельного проектного счётчика.
Обычное once-правило проверяет смерть старого набора врагов и публикует victory;
существующие правила истории обрабатывают этот сигнал. Новые проекты этих
правил и глобальных состояний не получают.

Детерминированные IDs и compatibilityRules/compatibilityBindings делают чтение
повторяемым. Удалённое автором совместимое правило/привязка не появляется снова;
отредактированное условие победы не затирается. Неизвестная версия сохраняется
с диагностикой и не приводится молча к v1. Copy project переназначает только
настоящие project references/owners, не произвольные строковые значения.

capabilityState читает старое плоское прохождение без повтора стартовых Event
и стартовых предметов. Переносит инвентарь, выбранное оружие, счётчик и AI в
owner maps. Погибший старый герой остаётся без управления и оружия. Новые saves
содержат независимое состояние владельцев; проверка отклоняет чужое назначение
оружия и повреждённые предметы/счётчики до применения.

## Локальный обмен: сохранённые границы

Существующий authoring package v1 остаётся единственным локальным форматом.
Native inventory экспортирует выбранные слоты, стартовые предметы, экипировку
и зависимые items; owner объекта/сцены становится именованным входом, project
owner относится к принимающему проекту. Counter/reticle/camera сохраняются как
обычные данные. ID установленной операции не перенумеровывается при импорте,
ID экземпляра и связанного Input Action перенумеровываются. Требования пакета
включают версии зарегистрированных capabilities/operations. Конфликт уже
подключённой возможности отклоняет план до изменения исходного проекта.

Прежние тесты переноса отдельных Events, функций и «Двери с ключом» A → B → C
сохранены. Добавлены проверки переноса inventory, camera с operation input,
counter и reticle. Ресурсы, metadata/авторство, графы, внешние привязки и
проверка запрещённого кода используют существующий пакетный конвейер.

Ограничение ADR-001/002 этапа 5А остаётся явным: полный перенос новой схемы
mechanicDefinitions, canonical v1 rules и вложенных typed reference calls ещё
отклоняется безопасной диагностикой. Например, weapon package после подключения
его typed operation input требует 5А. HTML/полный project export это ограничение
не имеет, но не заменяет обмен отдельной заготовкой. Не заявляем завершение
полного цикла новой схемы по успешно собранному standalone player. Следующий
шаг 5А должен сначала перенести один такой Event/функцию и их dependencies через
именованные входы, изменить в B и повторно импортировать в C, используя этот же
registry/schema/reference resolver; новые языки и магазин для этого не нужны.

## Последовательность и оставшиеся ограничения

1. Реализовано: сохранить native data/physics adapters, зарегистрировать модули
   и перенести их выполнение из GameSession; оставить legacy aliases фасада.
2. Реализовано: явные владельцы, сигналы состояния, typed UI/story bindings,
   Input Action calls, миграция старых authoring данных и прохождения.
3. Следующий этап: полный package round-trip нового typed call/rule/definition
   (5А), затем перевод оставшихся legacy authoring consumers на общие формы.
4. Только после перевода потребителей можно удалить deprecated playerId,
   controller camera fields, flat inventory/equipment/weaponId/kills read views,
   legacy commands/GAME_COMMANDS и старые string widget paths. В этом этапе они
   не удалены из сериализуемых старых проектов.

Редактор по-прежнему подключает один основной локальный контроллер и одну
камеру. Дополнительные movement definitions/directions доступны модели и
операциям; UI нескольких игроков/камер здесь не добавлялся. Resource пока
является integer-счётчиком. Прицел — независимая возможность существующего HUD,
не новый конструктор произвольных прицелов. Данные дополнительных владельцев
не создают второго объектного хранилища. Живая 3D/audio/browser и offline HTML
приёмка требует отдельного запуска; headless/DOM и сборки её не заменяют.

## Проверка трёх продуктовых принципов

- Простота: готовые независимые компоненты, выбранные владельцы и именованные
  поля HUD; обычный сценарий не требует JSON, путей или кода. DOM-тесты проверяют
  инвентарь объекта, явное подключение кнопок оружия и выбор поля HUD.
- Переносимость: прежний exchange regression suite и native capability package
  проверки; новые typed calls сохраняются в проектах и доступны в штатном
  редакторе. Полная пакетная приёмка новой схемы остаётся конкретной задачей 5А.
- Универсальность: UI-проект со счётчиком/инвентарём сцены без героя и HP;
  inventory-only объект; камера без стрельбы; два независимых магазина и HP;
  AI без здоровья и автоматической цели; авторские последствия death.

## Фактические проверки и воспроизведение

Исходный npm test: 444 passed, 0 failed. Финальный npm test: 475 passed,
0 failed, 0 skipped. Новые проверки: tests/capabilities.test.js (28 headless)
и 3 дополнительных JSDOM-сценария tests/mechanics-ui.test.js. Используются
настоящие PreviewRuntime, Event/Action Groups, save и package pipeline.

Во время проверки обнаружены и исправлены утечка продолжения Event после
удаления владельца, ошибочное перенумерование operation ID при импорте камеры,
сохранение управления/оружия при чтении старого dead save, потеря направления
основного владельца через зарегистрированную movement operation и перенос
чужого target в сигнал chained damage. Их наблюдаемые результаты покрыты
регрессионными assertions. Исходный прогон не имел проваленных тестов; D1 аудита
(literal shooter-hero внутри старой функции award-kill вместо типизированного
параметра исключаемого объекта) остаётся известным ограничением старой заготовки.
Общие операции нового контракта используют настоящие typed owner references.

| Проверка | Фактический результат после последних изменений кода |
|---|---|
| npm test | 475 passed, 0 failed, 0 skipped; исходная база — 444 |
| npm run build:player | Exit 0; 119 modules, общий runtime включён |
| npm run build | Exit 0, включая prebuild player; остаются предупреждения use client и размера chunks |
| Импорты runtime.js | 52 локальных JS/JSX-модуля, React imports отсутствуют |
| git diff --check | Exit 0; Git сообщает только настройки преобразования LF/CRLF |
| Shipped template JSON / native physics и render файлы | SHA-256 совпадают с началом этого этапа |
| Живой браузер, 3D/audio, offline HTML | В этом этапе не запускались |

```sh
node --test tests/capabilities.test.js tests/mechanics-ui.test.js
npm test
npm run build:player
npm run build
```

В редакторе создать пустой проект → «Игровые механики» → выбрать сцену/проект →
добавить «Инвентарь» или «Счётчик» → запустить без героя и истории. У двух
объектов отдельно добавить «Здоровье» и «Оружие», задать разные магазины;
у каждого раскрыть действия ввода и явно подключить нужную кнопку.
В виджете выбрать поле конкретного владельца. Для смерти добавить обычную
реакцию death и выбрать последствия. Отдельно добавить камеру от третьего
лица и запустить без оружия. Для старого поведения открыть встроенный шутер:
сгенерированные compatibility reactions доступны в обычном редакторе правил.

### Файлы текущего этапа

Полный список приведён ниже по сравнению с рабочей копией в начале этапа,
а не с HEAD ветки: он не приписывает этапу более ранние изменения пользователя.

- [src/BlueprintFunctionWorkspace.jsx](../src/BlueprintFunctionWorkspace.jsx)
- [src/CapabilityBindingFields.jsx](../src/CapabilityBindingFields.jsx)
- [src/GameHUD.jsx](../src/GameHUD.jsx)
- [src/GameSession.js](../src/GameSession.js)
- [src/GameplayWorkspace.jsx](../src/GameplayWorkspace.jsx)
- [src/InputWorkspace.jsx](../src/InputWorkspace.jsx)
- [src/InventoryFields.jsx](../src/InventoryFields.jsx)
- [src/MechanicFields.jsx](../src/MechanicFields.jsx)
- [src/RuleRuntime.js](../src/RuleRuntime.js)
- [src/SchemaFields.jsx](../src/SchemaFields.jsx)
- [src/StateBindingFields.jsx](../src/StateBindingFields.jsx)
- [src/WidgetBindingFields.jsx](../src/WidgetBindingFields.jsx)
- [src/WidgetRenderer.jsx](../src/WidgetRenderer.jsx)
- [src/blueprintFunctions.js](../src/blueprintFunctions.js)
- [src/cameraModel.js](../src/cameraModel.js)
- [src/capabilities/behavior.js](../src/capabilities/behavior.js)
- [src/capabilities/camera.js](../src/capabilities/camera.js)
- [src/capabilities/health.js](../src/capabilities/health.js)
- [src/capabilities/interaction.js](../src/capabilities/interaction.js)
- [src/capabilities/inventory.js](../src/capabilities/inventory.js)
- [src/capabilities/legacyViews.js](../src/capabilities/legacyViews.js)
- [src/capabilities/movement.js](../src/capabilities/movement.js)
- [src/capabilities/resources.js](../src/capabilities/resources.js)
- [src/capabilities/shared.js](../src/capabilities/shared.js)
- [src/capabilities/timers.js](../src/capabilities/timers.js)
- [src/capabilities/weapons.js](../src/capabilities/weapons.js)
- [src/capabilityBindings.js](../src/capabilityBindings.js)
- [src/capabilityMigration.js](../src/capabilityMigration.js)
- [src/capabilityModel.js](../src/capabilityModel.js)
- [src/capabilityRegistry.js](../src/capabilityRegistry.js)
- [src/capabilityState.js](../src/capabilityState.js)
- [src/gameCapabilities.js](../src/gameCapabilities.js)
- [src/gameSave.js](../src/gameSave.js)
- [src/gameplayAuthoring.js](../src/gameplayAuthoring.js)
- [src/gameplayModel.js](../src/gameplayModel.js)
- [src/inputModel.js](../src/inputModel.js)
- [src/mechanicDescriptions.js](../src/mechanicDescriptions.js)
- [src/mechanicPackages.js](../src/mechanicPackages.js)
- [src/mechanicsModel.js](../src/mechanicsModel.js)
- [src/projectLifecycle.js](../src/projectLifecycle.js)
- [src/runtime.js](../src/runtime.js)
- [src/valueSchemas.js](../src/valueSchemas.js)
- [src/widgetBindings.js](../src/widgetBindings.js)
- [src/widgetModel.js](../src/widgetModel.js)
- [tests/capabilities.test.js](../tests/capabilities.test.js)
- [tests/mechanics-ui.test.js](../tests/mechanics-ui.test.js)

Документация: этот ADR и ссылка/статус в [gameplay-unification.md](gameplay-unification.md).
