# ADR-002: общие сигналы и правила поверх существующего runtime

Дата: 13 октября 2026 года. Статус: реализован текущий этап.
Основание: [ADR-001](adr-001-operation-foundation.md),
[аудит](gameplay-unification.md), [требования продукта](../../../docs/PRODUCT-REQUIREMENTS.md).
Ветка `feature/new-ui-fix`; исходные рабочие изменения сохранены. Commit/push,
переключение ветки, новый ECS, язык, renderer и новый движок не выполнялись.

## Модель и точки подключения

`signalModel.js` регистрирует схемы сигналов и проверяет payload до приёма.
Сигнал — уведомление о случившемся; Event — существующее определение с группами
действий. Они остаются разными сущностями. Пользовательские сигналы хранятся в
`project.gameplay.signals`, встроенные регистрируются доверенными модулями.

Правила хранятся в прежнем `project.gameplay.rules`, состояние срабатываний —
в прежнем `world.game.rules`. Объекты, сцены, функции, Events, виджеты и Input
Actions используют свои существующие библиотеки и ID. Отдельного хранилища
объектов и подписок на каждый React mount нет.

Минимальный сериализуемый контракт правила v1:

```js
{
  id, name, version: 1, enabled: true,
  signal: {type: 'button', source: /* необязательный typed selector */},
  scope: {kind: 'project' /* object | scene | project */, id},
  filter: {source, target, initiator}, // необязательные typed selectors
  condition: /* существующее безопасное выражение либо null */,
  actions: [{kind: 'event', eventId, join: 'FLOW_END'}],
  otherwise: [],
  once: false, cooldown: 0, reentry: 'queue', maxConcurrent: 1, maxQueued: 16
}
```

Действия: `operation` с `{call:{id,version,parameters}}`, `event`, `function`
с arguments/expressions, `story` и ссылка на имеющийся экземпляр `mechanic`.
Последняя обслуживает описания механик ADR-001. Основной UI предлагает операции,
Events, функции и продолжение истории; список операций берётся из общего реестра.
Условия могут использовать expression, существующую condition function или
зарегистрированную boolean operation с `pure:true`. Здесь используется
`evaluateExpression`/`callBlueprint`, отдельного интерпретатора нет.

Контекст сигнала содержит реальные `source`, `target`, `initiator` как typed
references, `sceneId`, `mechanicInstanceId`, если они существуют. Например,
кнопка передаёт widget ID и element ID, Input Action — свой ID, подбор предмета —
объект-источник и известного инициатора. Программное получение предмета может
иметь пустой objectId и отсутствующий источник. Отсутствующая цель не становится
героем. Фильтры поддерживают прямые, контекстные, параметризованные ссылки и теги;
контекстная ссылка на инициатора добавлена к существующему resolver.

| Путь | Публикация / исполнение |
|---|---|
| Кнопка виджета | Editor/GamePlayer → `uiAction` → типизированный `button` |
| Дискретный Input Action | `inputAction` записывает значение → `input` |
| Получение предмета | `addItem` записывает inventory → `pickup`; физический подбор также скрывает объект, выполняет прежний pickup Event и публикует один pickup |
| Таймер | `GameSession.tick` обновляет/удаляет timer → `timer` |
| Переменная | `sacura.variable.set` записывает значение → `sacura.variable.changed` |
| Hit/death/victory | Legacy producer сначала завершает HP/death/visibility/kills commit → публикует hit, death, victory в этом порядке |
| Правило | `RuleRuntime` допускает запуск → `GameSession.performRuleAction` → registry, `callBlueprint` или `runtime.binding` |
| История | `sacura.story.enter` вызывает настоящий `runtime.enter`; Event действия истории могут вызывать те же зарегистрированные операции |
| Standalone | `play.jsx → GamePlayer → runtime`; shared model и registry не импортируют React |

Сенсоры областей, таймеров и столкновений, input adapter, физика и combat пока
остаются в GameSession. Все их реакции проходят общую модель правил. Условия и
исполнение правил не выбираются по жанру/preset. Удаление combat facade и перенос
legacy victory producer в шаблон относятся к последующим этапам аудита.

## Точный порядок, снимки и ограничения

1. Producer завершает изменение состояния. После этого payload проверяется по
   схеме, контекст разрешается и сигнал получает возрастающий номер.
2. При публикации фиксируются payload, контекст, definitions правил, project,
   variables, world/game и результат фильтров источника/цели/тегов. Это момент
   проверки условий. Позднейшие действия и await не меняют этот снимок.
3. Сигналы разбираются FIFO. В каждом сигнале правила рассматриваются в порядке
   их объявления. Expressions и condition functions читают отдельную копию
   captured variables/inventory. Condition operations получают captured
   project/world/gameState. Их trusted executor обязан соблюдать `pure` контракт.
   Условная функция не может запустить Event или mutating operation; записи самой
   функции происходят только в её копии variables/local.
4. Выбирается `actions` либо `otherwise`. Пустая ветка не расходует `once`.
   Политика повторного входа применяется до запуска. По умолчанию выполняется
   один запуск всех правил; завершение действий сохраняет последовательный
   порядок. При явной параллельности гарантируется порядок допуска/старта,
   порядок завершения зависит от действий.
5. `once.fired` и cooldown фиксируются при фактическом старте выбранной ветки.
   Once считается использованным и после ошибки/отмены уже начатого запуска.
   Cooldown задаётся в секундах игрового времени. Сигнал, опубликованный внутри
   положительного cooldown, пропускается, а не ждёт его окончания в очереди.
   Повторная проверка при старте не даёт уже накопленным сигналам обойти cooldown.
6. Внутри ветки действия запускаются по порядку. `NONE`/`STARTED` сохраняют
   существующую семантику раннего возврата, поэтому следующий action может идти
   сразу. При этом слот правила считается занятым до конца реальной finite работы
   его native Events, которая отслеживается в `runtime.eventFlows`.

`RuleRuntime` — ограниченный приём сигналов и реакций. Он не исполняет native
Action Groups, не планирует их действия и не дублирует их delay/cancellation.
`runtime.binding`, группы с `Promise.all`, joins, owner/retention, audio cleanup,
ошибки и `controlInstance` остаются существующими исполнителями.

Ограничения: суммарно не более **128** ожидающих signals/reactions; не более
**16** ожидающих запусков одного правила; параллельных запусков по умолчанию **1**,
явный `gameplay.ruleSettings.maxRuns` и `rule.maxConcurrent` допускают **1–8**.
Очередь запускает следующий подходящий по лимитам job; занятое правило не
блокирует другие при явно разрешённой параллельности. Переполнение сообщает
короткую диагностическую ошибку. `world.game.ruleTrace` содержит до 128 записей
публикации, проверки, допуска, старта, пропуска, завершения и отмены.

| reentry | Поведение |
|---|---|
| skip | Новый сигнал пропускается, если занято разрешённое число запусков данного правила |
| queue | Допускается ограниченное число ожиданий; каждое сохраняет свой снимок |
| restart | Прежние запуски отменяются через native lifecycle, прежняя очередь данного правила очищается; замена ждёт cleanup и получает свой слот |

Дочерний сигнал наследует causal chain. Повторное правило в этой цепочке и
цепочка длиннее **32** отвергаются с диагностикой. Это подавляет синхронные
самовызовы и взаимные циклы. Сигнал будущего таймера — новый временной корень;
явные повторяющиеся таймеры разрешены, их выполнение ограничивают обычные лимиты.

Внешний `await game.trigger` ждёт конечную работу корня и его принятых потомков.
Публикация из реакций и native Events подтверждает приём сразу: ожидать собственный
родительский Event означало бы deadlock. Ошибки внутри реакции доступны через
hint/message/trace; отмена завершает ожидание корня без записи в новый сеанс.

## Жизненный цикл, время и сохранения

Run принадлежит поколению PreviewRuntime, scope правила, сцене публикации и,
когда есть, экземпляру механики. `stop`, restart, fatal runtime error, смена сцены
для object/scene scope, удаление/отключение владельца и экземпляра отменяют его
native instances, эффекты и продолжение функции. Project scope переживает смену
сцены, но собственный native owner Event продолжает накладывать прежние ограничения.
Переход самой реакции может отменить её дальнейшую работу при выходе из scope.
Held backgrounds очищаются при прекращении ownership; конечная работа не заменяет
retention semantics. Generation проверки предотвращают поздние ошибки и завершения
старого сеанса. Timer states с ownership удаляются при потере владельца/сцены;
периодические scene/object sensors сбрасывают elapsed/inside при смене сцены.

Общий источник игрового времени — существующий `world.game.clock`, который
обновляется `playerStep/tick` с ограничением dt до 0.05. Clock идёт также
при dialogue/gameplay-node policy: выключение непрерывных систем не блокирует
конечную работу правил. Новые правила v1 передают
`timeSource:'game'` в native Event delay; pause останавливает clock, timers,
cooldown, допуск очереди, Event waits и следующий шаг reaction function. Технический
yield перед мгновенным action не требует продвижения игрового времени.

Legacy records читаются неразрушающим `normalizeRule`: trigger/objectId/sceneId,
condition/function/Event/continuation/else преобразуются в одну runtime модель
без записи миграции в проект и без повторного добавления данных. Они сохраняют
прежний `timeSource:'native'` для Event timing. Это явный compatibility adapter;
он не означает, что все старые аудио/animation clocks переведены на игровой clock.
Физика/движение обновляются системами, continuous input не публикует async Event
каждый кадр. Детерминизм физики между платформами не обещается.

Save допускается в безопасной точке без очередей/активных реакций и незавершённой
native finite работы. Once/cooldown/timers остаются сериализуемым состоянием в
прежнем save v1. Restore создаёт свежий GameSession и admission queue без replay
start/entry Events, сохраняет игровое время и fired markers. Незавершённые Promises
не записываются в сохранение.

## История и обычный пользовательский путь

В «Своя реакция» создаётся правило v1. «Когда / Если / Сделать / Иначе» открывает
его обычные данные. Источник и повторный вход раскрываются отдельно. Действия
выбираются по названиям; порядок можно менять, Event/функцию можно открыть в
существующем редакторе. Новая операция появляется из registry без новых switch
в формах. Существующие дверь/таймер и их native authoring records не переписаны.

В «Ввод, интерфейс и история» явно выбираются запуск с истории или сразу механик,
постоянная работа либо игровые ноды, allow/suspend управления в диалоге и
stop/continue после конца истории. Suspend ограничивает управление, не выключает
все системы только потому, что текущая нода — dialogue. Legacy activation
продолжает читаться при отсутствии policy.

`createMechanicsProject()` создаёт проект с пустыми chapters и null entry;
loader, editor, GamePlayer, save/restore принимают это. `enterScene` запускает
сцену без фиктивной реплики. View fallback в React не добавляет авторскую ноду.
В режиме запуска без истории можно удалить последнюю реплику; story mode
по-прежнему защищает свой обязательный entry. По запросу автора первая настоящая
story node создаёт эпизод; переход к ней
выполняет `sacura.story.enter`. В редакторе можно также отключить story start в
существующем проекте, сохранив его историю для вызовов правилами.

## Совместимость, перенос и продуктовые ограничения

Неизвестная версия правила сохраняется с diagnostic и не выполняется.
Validation проверяет ссылки, выражения, параметры операций, лимиты, scope,
условные операции и действия до экспорта/в редакторе. `copyProjectAs` переназначает
project scope canonical rules вместе с instances.

Существующий legacy локальный обмен не удалён. Перенос typed definitions/ссылок
ADR-001 и правил v1 явно отклоняется до этапа **5А**, чтобы старый collector не
выдал пакет с потерянными dependencies. Это не считается выполнением полного
A → B → C для новой схемы. Для 5А обязательны: closure по signal definitions,
operation requirements, Event/function actions обеих веток и conditions;
internal remap, внешние bindings scope/source/target/initiator/scene/continuation,
widgets/Input Actions/resources; atomic preview/import и редактируемость тех же
rules. Никакие callbacks, executor closures и JS installers не входят в пакет.

D2 из исходного аудита исправлен намеренно: `playerClick` выполняет pickup и
interaction signal; interaction rule больше не затеняет предмет. D3 исправлен:
save отклоняет незавершённую работу. Characterization-тесты этих дефектов заменены
regression assertions; остальные shipped-template expectations сохранены.
D1 и legacy combat/aliases ещё требуют последующих миграций. Исторический baseline
в аудите и ADR-001 не переписан.

Простота подтверждена DOM-сценариями добавления реакции, выбора сигнала/Event,
настройки reentry и story policy без JSON/ручных ID. Универсальность проверена
headless UI/game project без chapters, героя, HP и пространственных объектов.
Переносимость проверяется в рамках прежних package tests; новая схема остаётся
явным ограничением 5А. Живая 3D/browser/offline HTML приёмка этим не заменяется.

## Файлы и фактическая проверка

Добавлены `src/signalModel.js`, `src/ruleModel.js`, `src/RuleRuntime.js`,
`src/RuleActions.jsx`, `tests/rules.test.js` и этот ADR.

Адаптированы runtime/model: `GameSession.js`, `runtime.js`, `blueprintFunctions.js`,
`coreOperations.js`, `valueSchemas.js`, `gameplayModel.js`, `mechanicDescriptions.js`,
`mechanicsModel.js`, `gameSave.js`, `inputModel.js`, `projectFiles.js`,
`projectLifecycle.js`, `mechanicPackages.js`, `storyEditing.js`.

Адаптированы формы/entry: `Editor.jsx`, `GamePlayer.jsx`, `GameplayWorkspace.jsx`,
`GameplayCommandFields.jsx`, `MechanicFields.jsx`, `SchemaFields.jsx`.
Обновлены `tests/gameplay-characterization.test.js`, `tests/mechanics-ui.test.js`,
ссылки в audit/ADR-001. Стили и реальные shipped JSON templates не изменены в
этом этапе. Прочие изменения рабочей копии относятся к прежней работе пользователя.

| Проверка | Фактический результат |
|---|---|
| Исходный `npm test` | 408 passed, 0 failed |
| Финальный `npm test` | 444 passed, 0 failed, 0 skipped: 34 новых headless и 2 DOM проверки |
| `npm run build:player` | Exit 0; shared signals/rules включены в player |
| `npm run build` | Exit 0, включая prebuild player; warnings use client/chunk size остаются |
| Граф зависимостей runtime.js | 35 local modules, React imports отсутствуют; rules и registry достижимы |
| Shipped shooter/point-and-click JSON | SHA-256 совпадает с состоянием до текущего этапа |
| Живой браузер, 3D/audio, offline HTML | Не проверялись в этом этапе |

В промежуточных прогонах выявлена регрессия технической задержки старого move
Event; native delay восстановлен, связанный playtest и финальная база прошли.
Ошибки тестовых fixtures (неполный button RectTransform/Input Action) исправлены
через настоящие фабрики/модель, а не обход production validation.

Воспроизведение из frontend:

```sh
node --test tests/rules.test.js tests/mechanics-ui.test.js
npm test
npm run build:player
npm run build
```

В редакторе: «Игровые механики» → «Весь проект» → «Своя реакция» → добавить →
раскрыть «Когда / Если / Сделать / Иначе» → выбрать «Кнопка» → добавить Event
или зарегистрированную операцию. В виджете для кнопки можно оставить «Без действия»:
её button signal всё равно проходит правила. Аналогично выбрать получение
предмета или таймер. В дополнительных настройках выбрать reentry/cooldown,
запустить проект; для проверки отмены остановить/перезапустить во время Event.
Точные проверки порядка, снимков, лимитов и временных сценариев находятся в
`tests/rules.test.js` и не зависят от React/canvas.
