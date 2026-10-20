# ADR-004: создание авторских механик, экземпляры и конечные состояния

Дата: 13 октября 2026 года. Ветка: feature/new-ui-fix.
Статус: реализовано в текущей рабочей копии поверх ADR-001–003.
Продуктовый контракт: [требования](../../../docs/PRODUCT-REQUIREMENTS.md),
[проверка изменений](../../../docs/CHANGE-CHECKLIST.md).

## Конечная операция автора

«Игровые механики» → «Создать свою механику» → выбрать существующую функцию,
Event или реакцию → назвать → «Сохранить в библиотеку» → добавить к выбранному
объекту, сцене или проекту. При применении показываются открытые именованные
параметры. В «Изменить общее определение» доступны описание, типы, значения по
умолчанию, объектные слоты, открытые поля, переменные экземпляра, правила и
конечные состояния. Сложная функция открывается в существующем Blueprint editor.

Обычное создание не требует JSON, ручных ID или сборки. При сборке из Event
создаётся обычная blueprint function с native event node: предметные цели Event
становятся типизированными слотами. Первый слот по умолчанию — «этот объект»;
его можно назначить явно при размещении на сцене или проекте. Event и группы
действий сохраняются в прежней библиотеке. Target assignments адаптируются к
существующим actionOverrides при запуске; исходные действия не изменяются.

Сборка из готовой реакции сохраняет условия, вызовы функции, Event, операции,
ветку иначе и исходную семантику ожидания. Для выбранного объекта сохраняется
явный фильтр «этот объект»; его цели в Event становятся контекстными привязками.
Не угадываем значение произвольных строк как object ID. Полностью произвольная
реакция может сохранить ссылки, которые автору нужно открыть параметрами;
это не объявляется автоматической параметризацией любого графа.

## Общий контракт и копии

Используется существующий gameplay.mechanicDefinitions, mechanic schema v1.
Дополнительные поля: rules[], exposed[] и machine с version 1, initial,
states[], transitions[]. Parameters/stateSchema используют тот же valueSchemas
и reference resolver, что реестр операций. Definition.logic.functionId остаётся
поддержанным для старых определений. Новое определение может состоять из правил
или конечных состояний без фиктивной функции. Неизвестная версия машины
сохраняется с диагностикой и не исполняется как известная схема.

Экземпляр в прежнем gameplay.mechanics содержит native ID, definitionId/version,
scope и values. Definition не содержит состояние конкретной двери. Во время
прохождения переменные находятся в world.game.mechanicStates[instanceId], а
состояние машины и revision — в mechanicMachines[instanceId]. Идентичности
объектов и сцен остаются прежними. Нового ECS и языка графов нет.

«Копия с общим определением» создаёт независимые assignments и состояние,
сохраняя ссылку на общую логику. «Создать независимую копию» создаёт новое
определение и копирует транзитивные зависимости Events/функций с новыми IDs
функций, узлов, групп и действий. Внутренние ссылки и target override keys
переназначаются; native object references, пользовательские литералы и IDs
зарегистрированных операций сохраняются. Metadata источника остаётся в копии.
Наследования и цепочек prefab overrides нет.

Редактор явно сообщает количество затрагиваемых общим определением экземпляров.
Изменение assigned value одного экземпляра не меняет defaults или соседей.
Изменение графа общей функции затрагивает всех её потребителей; независимая
копия получает отдельную функцию. Изменение публичной сигнатуры синхронизирует
типизированную сигнатуру корневой функции; редактирование функции синхронизирует
её связанные определения. Узлы и связи графа при этом сохраняются. Явное
удаление параметра удаляет его assignments у связанных экземпляров; это действие
в редакторе общего определения, а не скрытая очистка при чтении проекта.

Редактирование во время запуска остаётся отключено существующим UI. Новые
authoring данные применяются при следующем запуске; live hot reload машин в
текущее прохождение здесь не введён. Копия получает начальные значения определения,
а не случайную копию текущего прогресса другого экземпляра.

## Правила, состояния и отмена

authoredRules разворачивает локальные правила/переходы definition в обычные
version 1 rules с ID instanceId:localRuleId. Их выполняет прежний RuleRuntime.
Он делегирует действия тем же операциям, Events и blueprintFunctions. Нет
отдельного state-machine scheduler или второго async Event executor.

Условия читают снимок variables, inventory, переменных экземпляра и его
назначенных параметров в момент публикации сигнала. Actions получают актуальные
переменные именно этого экземпляра. Контекстные ссылки используют существующие
source/target/initiator/self; отсутствие объекта остаётся отсутствием.

Порядок перехода:

1. Из переходов исходного состояния проверяются условия в порядке объявления.
2. Первый принятый переход проверяет актуальное состояние и revision. Приём
   атомарно назначает новое состояние и увеличивает revision до async действий.
3. Отменяются старые активные/ожидающие реакции, running/paused/held Events и
   игровые таймеры этого экземпляра. Native controlInstance выполняет cleanup.
4. Последовательно выполняются exit старого состояния → действия перехода →
   enter нового состояния через существующий lifecycle и лимиты RuleRuntime.

Одновременный сигнал со старым снимком/revision пропускается. Нет случайного
выбора по завершению await. Переход с ложным условием не принимается даже при
пустых actions. Политика once/cooldown и общий лимит запусков сохраняются.
Таймерные sensors переходов обновляются только для активного исходного состояния;
elapsed сбрасывается при выходе, а явно заданный once сохраняется на весь сеанс.

Смена состояния сохраняется даже при ошибке exit/transition/enter; ошибка
диагностируется, незавершённые Events отменяются. Это не транзакционный откат
мира: уже совершённые действия (например, открытие двери) не обращаются назад.
Автор задаёт обратные действия через exit/другой переход. Вложенные или
ортогональные statecharts не реализуются: лимит — 64 состояния, 128 переходов.
Игровая пауза, stop/restart, смена сцены и удаление владельца используют прежнюю
систему времени и отмены. Поведение/физика не превращаются в покадровые Events.

## Blueprint editor и типы

BlueprintFunctionWorkspace уже использовал общий GameplayCommandFields;
теперь контекст построен по схемам параметров, переменных экземпляра/локальных
данных и полей существующих проектных переменных. Для Get/Set, результата вызова
и аргументов доступны именованные selectors вместо обязательного global.hp или
JSON. Object/item slots доступны также в выражении количества предмета.
Сложные выражения и graph arguments сохраняются в расширенном редакторе.

FunctionParameters поддерживает общие scalar, integer, vector, collection и
typed reference schemas; локальные переменные редактируются по тем же схемам.
resultSchema проверяется штатным callBlueprint. Запись в типизированное локальное
состояние, изменение массива и запись результата проверяются до изменения
этого состояния. Лимиты прежние: 1000 шагов, 16 уровней вызова, 64 уровня
выражения. Условные функции читают снимок, а вложенная функция сохраняет свои
обычные локальные данные; переменные корневой механики принадлежат экземпляру.

RuleFields/RuleActions/FunctionArguments используют те же схемы и context fields.
Обычная реакция может вызвать сложную функцию; произвольный граф не переводится
безусловно обратно в form rules. Открытие формы не удаляет неподдержанные узлы
или сложные выражения. Расширенные структурные/глубокие выражения сохраняют
существующий расширенный редактор; полной визуальной формы любого выражения
этот этап не обещает.

## Доверенный модуль очередности ходов

capabilityRegistry расширен общими data descriptors, регистрацией сигналов и
хуками project/save validation. Общий каталог и SchemaFields читают metadata;
gameSave вызывает общий контракт проверки состояния установленного модуля.
Состояние такого экземпляра — capabilityStates[capabilityId][instanceId], его
конфигурация — values того же gameplay.mechanics. Это не новое хранилище объектов.

`capabilities/turns.js` регистрирует sacura.turn: список typed участников,
start/next/current, started/changed с typed payload и проверки сохранения.
Очереди проекта/сцены независимы; переключение хода сначала меняет состояние,
затем публикует общий сигнал с participant/round/mechanic context. Поле «Номер
круга» доступно существующим типизированным HUD/story bindings. Модуль не
использует героя, HP, оружие или жанр.

Единственная точка подключения конкретного модуля — статический import в
gameCapabilities.js. GameSession и общая UI-форма не содержат turn-specific
веток. Общие registry/data/save adapters расширены один раз; следующим модулям
доступен этот же контракт. Это доверенный код разработчика, входящий в сборку
движка. Авторская механика из Events — обычные локальные данные; создание,
применение и копирование таких заготовок сборки/установки кода не требуют.

JSON не вызывает registerCapability, eval, Function или dynamic import. Здесь
не реализован sandbox скачанного JavaScript, установка расширений или магазин.
Пакетные проверки запрета кода и установочных скриптов остаются прежними.

## Сохранения и обмен

authoringStateVersion: 1 различает новые сохранения состояния механик. В новом
save обязательны состояния экземпляров/машин; проверяются schemas, конечное
состояние и revision. Сохранение неизвестной версии отвергается до применения.
Чтение прежнего save добавляет только отсутствующие начальные данные, не повторяя
start Event, entry actions или стартовый инвентарь. Существующее сохранённое
состояние других экземпляров сохраняется.

mechanicTransferContract теперь включает правила, machine entry/exit/transition,
вложенные Events/функции, widgets/assets и зарегистрированные операции. Object
slots и другие typed parameters остаются именованными внешними входами. Native
world references не заменяются новыми object IDs при локальном копировании.

Примечание этапа 5А: описанное ниже ограничение этапа 5 закрыто локальным
форматом v2 с чтением v1; actual implementation, A/B/C, preview/Undo и
browser Play/standalone приведены в [ADR-005](adr-005-local-authoring-packages.md).
Ниже сохранён статус на момент завершения этапа 5.

Полный authoring package round-trip новых definitions/rules/typed calls остаётся
этапом 5А из ADR-001–003; действующий importer отклоняет неподдержанную схему до
применения. В этой работе реализованы локальное создание и независимая копия,
сохранение/чтение проекта и прохождения. Не объявляем их завершённым обменом новой
заготовки между проектами. Прежние native Event/function/door package regression
tests сохранены. Следующий 5А должен перенести именно описанное definition с
тремя экземплярами и зависимостями A → B → C, сохранив редактируемые графы,
происхождение и явные привязки; установки turns.js для author package нет.

## Проверки продукта и воспроизведение

- Простота: DOM-путь создать из Event, назвать, применить, изменить общее
  определение, создать независимую копию, добавить состояния/переход без ID/JSON.
- Переносимость: независимая локальная копия dependency closure, сохранённые
  native references/operation IDs, сохранения и прежние package tests. Полный
  новый package format 5А остаётся явно непроверенным ограничением.
- Универсальность: проект без героя; очереди ходов проекта/сцены через общую
  форму/операции/сигналы/save. Три двери с разными ключами и отдельным прогрессом.

Из frontend:

```sh
node --test tests/authored-mechanics.test.js tests/mechanics-ui.test.js
npm test
npm run build:player
npm run build
```

Точный сквозной сценарий «Дверь с ключом» находится в
tests/authored-mechanics.test.js: заготовка собирается из настоящего native
door Event и существующей безопасной key function, применяется к трём native
дверям, открывается только при своём ключе, сохраняется и читается независимо.
Проверяются shared/private graph edits, IDs, отмена Event/таймеров, порядок
переходов, schemas, recursion/step limits и независимые очереди ходов.

Проверки после последнего изменения кода (13 октября 2026):

| Команда | Результат |
| --- | --- |
| npm test до этапа | 475 passed, 0 failed |
| npm test после этапа | 489 passed, 0 failed, 0 skipped, exit 0 |
| npm run build:player | exit 0; 121 modules transformed |
| npm run build | exit 0; prebuild повторяет player build, editor — 1951 modules |

Добавлены 12 headless и 2 DOM characterization/regression теста. Build редактора
выдаёт предупреждения об игнорируемом `use client` в @xyflow/react и размере
bundle > 500 kB; сборка завершилась успешно. Новых ошибок baseline не обнаружено.
Живой браузер/3D и offline HTML здесь не проверялись; DOM/headless тесты и player
build их не заменяют. D1 из первоначального аудита остаётся открытым.

Дополнительно: git diff --check — exit 0; относительные ссылки ADR проверены.
Статический граф локальных JS/JSX импортов runtime.js/GameSession.js — 54 модуля,
без импортов React. Все 272 файла снимка начала этапа остались на месте;
ветка feature/new-ui-fix сохранена, commit/push/reset не выполнялись.

## Файлы этапа

Список сравнивает файлы с рабочей копией в начале этапа; ранее существовавшие
изменения пользователя сохранены. Физика, рендер и shipped JSON не менялись.

- [src/AuthoredMechanicEditor.jsx](../src/AuthoredMechanicEditor.jsx)
- [src/BlueprintFunctionWorkspace.jsx](../src/BlueprintFunctionWorkspace.jsx)
- [src/FunctionArguments.jsx](../src/FunctionArguments.jsx)
- [src/FunctionParameters.jsx](../src/FunctionParameters.jsx)
- [src/GameSession.js](../src/GameSession.js)
- [src/GameplayWorkspace.jsx](../src/GameplayWorkspace.jsx)
- [src/MechanicFields.jsx](../src/MechanicFields.jsx)
- [src/RuleActions.jsx](../src/RuleActions.jsx)
- [src/RuleRuntime.js](../src/RuleRuntime.js)
- [src/SchemaFields.jsx](../src/SchemaFields.jsx)
- [src/TypedProperties.jsx](../src/TypedProperties.jsx)
- [src/authoredMechanics.js](../src/authoredMechanics.js)
- [src/authoringContext.js](../src/authoringContext.js)
- [src/blueprintFunctions.js](../src/blueprintFunctions.js)
- [src/capabilities/timers.js](../src/capabilities/timers.js)
- [src/capabilities/turns.js](../src/capabilities/turns.js)
- [src/capabilityBindings.js](../src/capabilityBindings.js)
- [src/capabilityModel.js](../src/capabilityModel.js)
- [src/capabilityRegistry.js](../src/capabilityRegistry.js)
- [src/gameCapabilities.js](../src/gameCapabilities.js)
- [src/gameSave.js](../src/gameSave.js)
- [src/mechanicDescriptions.js](../src/mechanicDescriptions.js)
- [src/mechanicsModel.js](../src/mechanicsModel.js)
- [src/ruleModel.js](../src/ruleModel.js)
- [src/valueSchemas.js](../src/valueSchemas.js)
- [tests/authored-mechanics.test.js](../tests/authored-mechanics.test.js)
- [tests/mechanics-ui.test.js](../tests/mechanics-ui.test.js)
- [docs/adr-004-authored-mechanics.md](../docs/adr-004-authored-mechanics.md)
- [docs/gameplay-unification.md](../docs/gameplay-unification.md)
