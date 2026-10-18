# ADR-001: общий реестр операций и описание механик

Дата: 13 октября 2026 года. Статус: реализована основа, этап 5А не входит в неё.
Основание: [архитектурный аудит](gameplay-unification.md),
[требования продукта](../../../docs/PRODUCT-REQUIREMENTS.md).
Существующие рабочие изменения сохранены; ветка `feature/new-ui-fix`.

## Решение

Операции регистрируются в `operationRegistry.js` с namespaced ID, целочисленной
версией, parameter/result schemas, defaults, contexts и сериализуемыми editor
metadata. Например, `sacura.variable.set@1`. ID не зависит от видимого названия.
Несколько версий могут сосуществовать; одинаковые ID+version отвергаются.
Неизвестная версия не перенаправляется на первую, в том числе version=0.

`registerOperation(definition, execute, validate?)` отдельно хранит trusted
исполнитель и необязательную чистую проверку связей между параметрами. В JSON
описания нет этих функций. Descriptor глубоко скопирован и заморожен. Runtime
реестр, схемы и базовые операции не импортируют React.

Новые операции проходят единственную точку `GameSession.command → executeOperation`.
Она проверяет контекст, defaults, параметры, вычисленные выражения и результат,
а также generation до и после исполнения. Trusted async исполнитель обязан
проверять `context.assertActive()` перед изменениями после собственного await.
Наличие registry не создаёт новый scheduler и не сериализует Promise.

Старая команда и её семантика остаются в legacy dispatcher. Старый
`registerGameCommand` продолжает работать; новые модули используют registerOperation.
Полное извлечение movement/physics/combat из GameSession и перевод всех старых
команд в descriptors не выполнялись в этой основе.

## Одна схема для форм и исполнения

`valueSchemas.js` поддерживает:

* number и integer: конечные числа, строгий тип без coercion, диапазон;
* boolean и string, enum;
* vector из 2/3/4 конечных числовых компонентов;
* collection с явной схемой элемента и ограничением длины;
* object с явно объявленными полями, optional/default;
* union из явно объявленных схем;
* reference на object, scene, variable, event, function, widget, inputAction,
  а также пользовательский signal;
* зависимое значение переменной / payload выбранного сигнала и void result.

Параметры структуры не принимают неизвестные поля. Произвольный JSON не является
универсальным типом параметра. Для существующих переменных schema выводится из
variableTypes и начального значения: пустой массив по умолчанию допускает scalar
элементы; богатая коллекция должна иметь явную schema в параметре. Новый реестр
не меняет legacy преобразования значений старых действий.

`SchemaFields/SchemaValue` строят обычные поля, выбор ссылок, векторы, коллекции и
структуры. Ошибки поля связаны с input через aria-describedby/aria-invalid;
проверки нескольких параметров приходят из того же registry. GAME_COMMANDS не
дополняется новыми ID вручную. Изменение registry уведомляет уже смонтированную
библиотеку. Версию операции можно выбрать, если установлено несколько.

Та же схема используется в:

* Event gameplay action и кнопке виджета;
* command node существующей функции, с сохранением результата в существующий local;
* typed параметре функции (`parameter.schema`), её вызовах и аргументах правил;
* conditionOperation / operation / elseOperation существующего игрового правила;
* параметрах экземпляра механики;
* импортном preflight и проверке итогового проекта;
* executeOperation перед выполнением и после вычисления выражения.

Выражения вычисляет прежний `evaluateExpression`, функции — прежний callBlueprint.
`expressionValidation.js` только проверяет данные выражений. Список command nodes,
phases, Event retention, parallel groups и отмена остаются штатными.

## Типизированные ссылки

Прямой выбор: `{kind:'reference', type:'object', mode:'direct', id:'existing-id'}`.
Все object/scene/event/function/widget/inputAction ID принадлежат прежним библиотекам
проекта. Реестр не хранит копии объектов и не создаёт новые actor IDs.

Дополнительные режимы:

* `internal`: определение внутри переносимой зависимости;
* `external`: именованный параметр `parameter`, требующий назначения при применении;
* `context`: object role=self/source/target («этот объект», «источник», «цель»);
* `tag`: коллекция существующих объектов с заданным `objects[].tags`.

Self требует object scope; source/target требуют соответствующий payload/контекст.
Отсутствующий объект даёт ошибку, а не подставляет героя. Tag selector допускается
в collection<object>, но не в одиночном object-параметре. Форма показывает теги,
уже присутствующие в objects; отдельный редактор тегов не добавлен.

Историческая идентичность переменной — её ключ в project.variables. Для новых
ссылок используется `gameplay.variableBindings: [{id,key}]`: стабильный ID указывает
на этот ключ, значение продолжает храниться только в project.variables. Миграция
создаёт детерминированный ID; editVariable сохраняет его при переименовании.
Имя в select служит подписью, не ID. Удаление используемой через typed reference
переменной блокирует существующая проверка её потребителей.

## Определение и экземпляр

`gameplay.mechanicSchemaVersion=1` версионирует новую authoring модель, не заменяет
project version=2 или native gameplay version=1.

`gameplay.mechanicDefinitions[]` хранит ID/version/name, scopes, parameters schema,
stateSchema/initialState, when (signal ID), requirements и logic.functionId.
Логика остаётся обычной редактируемой функцией проекта. Именованные параметры
определения должны существовать в её сигнатуре; reference-параметр функции
использует parameter.schema. Автоматический ID формирует createMechanicDefinition.

Экземпляр находится в **существующем** `gameplay.mechanics[]`:

```js
{
  id: 'mechanic-…', type: 'definition',
  definitionId: 'user.mechanic.counter', definitionVersion: 1,
  scope: {kind: 'project', id: 'existing-project-id'},
  values: {counter: {kind:'reference',type:'variable',mode:'direct',id:'variable-…'}},
  ruleId: 'mechanic-…-rule'
}
```

`addMechanicInstance` создаёт привязку в прежнем gameplay.rules. GameSession
выбирает её в существующей очереди rule ID; actual trigger читает из общего
definition.when. Самостоятельного исполнителя механик нет. При выполнении
используются assigned values и существующая функция с отдельным local state.

Состояние находится в `snapshot.world.game.mechanicStates[instanceId]` и проходит
stateSchema. Начальное состояние глубоко копируется для каждого экземпляра;
оно не записывается в общий definition или functions[].locals. JSON save хранит
его вместе с прежним world, restore не запускает startup ещё раз. Старые saves
без mechanicStates остаются читаемыми. Известное ограничение сохранения
незавершённого Event (D3 из аудита) этим этапом не исправляется.

Размещение допустимо на существующем объекте, сцене или проекте. Project scope
работает без объектов, героя и HP. При copyProjectAs project placements получают
новый project ID; object IDs не меняются. Определения добавляются в прежний
каталог GameplayWorkspace, а inspector редактирует values экземпляра и открывает
существующую функцию. Полный визуальный конструктор definition schemas ещё не добавлен.

## Чтение и миграция

ensureGameplay вызывает ensureMechanicDescriptions, добавляющий отсутствующую
версию, пустые библиотеки и variable bindings. Существующие native mechanics,
события, правила, ID, gameplay-node/preset adapter и данные шаблонов сохраняются.
Повторное чтение одного legacy JSON даёт одинаковые данные; повторный upgrade
не создаёт новые привязки. readProject клонирует исходник перед миграцией.

Неизвестные операции, definitions и schema versions остаются в проекте с
диагностикой; редактор не удаляет их. Runtime не выполняет неизвестный код.
Будущая mechanicSchemaVersion не понижается на 1 и не переписывается. Проверки
охватывают дубли operation ID+version, definition ID+version, instance/signal IDs
и стабильных variable bindings.

## Контракт этапа 5А

mechanicTransferContract возвращает schemaVersion, internal typed dependencies,
external named parameters и requiredOperations с точной версией. Функции и
Events сохраняются исходными графами; literal strings не угадываются как ID.
Это основа typed closure collector для существующего sacura-authoring-package,
а не отдельный магазинный формат или runtime.

Импортный preflight уже использует registry schemas для operation nodes и проверяет
schemas параметров функций. Чистая операция сравнения может пройти существующий
пакетный путь. Однако полноценный сбор/remap новых typed references и definition
instances относится к 5А: их экспорт сейчас **явно отвергается** до изменения
проекта. Нельзя потерять новые fields, скопировать случайные библиотеки целиком
или объявить такой пакет переносимым. Старый локальный обмен и его A/B/C сценарий
остаются рабочими. Пакет не может registerOperation, выполнить installer или
динамически подключить JS-модуль.

## Работающий срез и подключение модуля

В coreOperations зарегистрированы:

1. sacura.variable.set — изменение выбранной переменной с проверкой её типа;
2. sacura.signal.emit — payload по schema выбранного пользовательского сигнала;
3. sacura.condition.compare — scalar сравнение через evaluateExpression;
4. sacura.event.run — обычный Event через native binding/join/отмену.

Минимальный trusted модуль:

```js
import {registerOperation} from './operationRegistry.js';
registerOperation({
  id: 'mytools.number.double', version: 1,
  parameters: {type:'object', properties:{
    value:{type:'number', default:1, label:'Число'}
  }},
  result:{type:'number'}, contexts:['event','function','rule','mechanic'],
  editor:{label:'Удвоить число', category:'Мои операции'}
}, ({value}) => value * 2);
```

Подключить этот модуль одним import в shared trusted bootstrap (coreOperations
загружается обоими entry через gameplayModel). Никаких изменений списков в
GameplayWorkspace или switch GameSession не требуется. Модуль должен попасть
также в build:player; registration только в editor entry не устанавливает его
в standalone. Пользовательский authoring package этого import не содержит.

Для проверки в редакторе: открыть Event → добавить «Игровая команда» → выбрать
«Изменить переменную» → выбрать переменную и значение. В графе функции новая
операция доступна в той же command node. В «Своя реакция» раскрыть
«Зарегистрированные операции»: можно создать именованный пользовательский сигнал,
выбрать сравнение как условие и Event как действие.

Для воспроизведения полного среза/состояния и регистрации модуля:

```sh
node --test tests/operation-model.test.js tests/operation-ui.test.js
npm test
npm run build:player
npm run build
```

В tests/operation-model: set → signal → condition → Event выполняют реальные
PreviewRuntime/GameSession/callBlueprint. Также проверяются shared definition,
разные scopes, независимые состояния, save/restore и отмена. Fixture
registeredOperation.js — отдельный модуль регистрации. DOM-тест монтирует настоящую
форму, регистрирует модуль после mount и проверяет появление и schema validation.
В этом этапе DOM-тесты не называются живой браузерной или offline HTML приёмкой.

## Проверка продукта и ограничения

Простота: существующие поля/каталог сохранены; новые operations строят формы с
выбором ссылок, без JSON/ручных ID. Ошибки показаны рядом с полем. Переносимость:
контракт выделяет internal/external/requirements; existing exchange не удалён,
неподдержанный typed export отклонён явно до 5А. Универсальность: runtime срез
и экземпляры project scope выполняются без героя, HP и spatial objects.

Прежние characterization, включая отмеченные D1/D2/D3, остаются проверками
наблюдаемого поведения. Их целевые исправления, весь combat refactoring, полноценный
редактор schemas и полный обмен новой моделью не подменяются этой основой.

## Файлы и фактические проверки этого этапа

Новые исходники: operationRegistry.js, valueSchemas.js, coreOperations.js,
mechanicDescriptions.js, expressionValidation.js, SchemaFields.jsx.

Адаптированы:

* runtime/model: GameSession.js, runtime.js, gameplayModel.js,
  blueprintFunctions.js, gameSave.js, variableModel.js, projectLifecycle.js;
* формы: GameplayCommandFields.jsx, ActionFields.jsx,
  BlueprintFunctionWorkspace.jsx, FunctionArguments.jsx, FunctionParameters.jsx,
  GameplayWorkspace.jsx, MechanicFields.jsx, WidgetWorkspace.jsx, translations.en.json;
* каталог/обмен: mechanicsModel.js, mechanicPackages.js.

Добавлены tests/operation-model.test.js, tests/operation-ui.test.js и отдельный
модуль tests/fixtures/registeredOperation.js. Аудит дополнен ссылкой на этот ADR.
Реальные встроенные JSON шаблоны, renderer, стили и прежние characterization-тесты
не изменены. Изменения других файлов в рабочей копии относятся к предыдущим этапам.

| Проверка | Результат |
|---|---|
| npm test | 408 passed, 0 failed, 0 skipped; 30 новых проверок поверх 378 прежних |
| npm run build:player | Exit 0, 99 modules; общий registry включён в player |
| npm run build | Exit 0, включая prebuild player |
| Граф локальных зависимостей runtime.js | Нет React imports; coreOperations достижим |
| DOM формы | Регистрация после mount, schema поля и ошибки, выбор ID по подписи, scopes и сохранение неизвестных данных проверены |

Сборка предупреждает об игнорируемой `use client` директиве @xyflow/react и большом
чанке Editor. Живой браузер, offline HTML, аудио и серверные интеграции в этом
этапе не проверялись. Переход к 5А должен реализовать typed dependency closure и
remap, затем повторить полный цикл A → B → C с новой схемой в редакторе.

## Следующий выполненный этап

[ADR-002](adr-002-common-rules.md) подключил operations к общей модели правил,
добавил typed signals, snapshot conditions, bounded admission и отмену через
native Event lifecycle. D2/D3 исправлены отдельно; неизвестные версии сохраняются.
Исторические результаты 408 проверок выше относятся к основе ADR-001;
актуальные проверки и ограничения переноса 5А приведены в ADR-002.
