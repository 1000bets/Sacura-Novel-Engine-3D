// Последний обход. Здесь находятся правила загадки, а не скрытый жанровый preset.
// Упражнение: в escape разрешите выход без записи или измените реакцию mirror.
const component = {
  name: "HauntedRoom",
  parameters: { type: "object", properties: {
    requireRecord: { type: "boolean", default: true, label: "Для выхода нужна запись смены" },
  } },
  state: { type: "object", properties: {
    fuse: { type: "boolean", default: false, label: "Предохранитель найден" },
    power: { type: "boolean", default: false, label: "Питание включено" },
    record: { type: "boolean", default: false, label: "Запись прослушана" },
    mirrorSeen: { type: "boolean", default: false, label: "Отражение замечено" },
    escaped: { type: "boolean", default: false, label: "Обход завершён" },
    objective: { type: "string", default: "Найдите предохранитель на столе у входа.", label: "Текущая задача" },
  } },
  references: { type: "object", properties: {
    shadow: { type: "reference", referenceType: "object", label: "Чужой силуэт" },
    lamp: { type: "reference", referenceType: "object", label: "Аварийный свет" },
    exit: { type: "reference", referenceType: "mechanic", label: "Замок выхода · LockedDoor" },
    ending: { type: "reference", referenceType: "beat", label: "Финал истории" },
  } },
  signals: { changed: { name: "Обход изменился", payloadSchema: { type: "object", properties: { stage: { type: "string" } } } } },
  lifecycle: { initialize: "prepare" },
  methods: {
    prepare: { name: "Подготовить новый обход", async run({ refs, engine }) {
      await engine.operation("sacura.object.visible", { object: refs.shadow, visible: false });
      await engine.operation("sacura.object.visible", { object: refs.lamp, visible: false });
      await engine.operation("sacura.camera.look", { delta: [0, 40], device: "pointer" });
    } },
    takeFuse: { name: "Взять предохранитель", result: { type: "boolean" }, async run({ state, engine }) {
      if (state.fuse) return false;
      state.fuse = true;
      state.objective = "Установите предохранитель в красный щит у дальней стены.";
      await engine.operation("sacura.game.message", { message: "Керамика ещё тёплая. Кто-то вынул предохранитель перед вашим приходом." });
      await engine.emit("changed", { stage: "fuse" });
      return true;
    } },
    power: { name: "Восстановить питание", result: { type: "boolean" }, async run({ state, refs, engine }) {
      if (!state.fuse) { await engine.operation("sacura.game.message", { message: "Пустое гнездо. Нужен предохранитель со стола смотрителя." }); return false; }
      if (state.power) { await engine.operation("sacura.game.message", { message: "Щит гудит. Теперь работает магнитофон." }); return true; }
      state.power = true;
      state.objective = "Прослушайте магнитофон на синем столе. Не оборачивайтесь на стук.";
      await engine.operation("sacura.object.visible", { object: refs.lamp, visible: true });
      await engine.operation("sacura.object.visible", { object: refs.shadow, visible: true });
      await engine.operation("sacura.game.message", { message: "Свет вспыхнул. В конце коридора стоит человек в вашей форме. У него нет лица." });
      await engine.emit("changed", { stage: "power" });
      return true;
    } },
    listen: { name: "Прослушать запись", result: { type: "boolean" }, async run({ state, refs, engine }) {
      if (!state.power) { await engine.operation("sacura.game.message", { message: "Катушки неподвижны. Сначала восстановите питание щита." }); return false; }
      state.record = true;
      state.objective = "Выход разблокирован. Вернитесь к зелёной двери и завершите обход.";
      await engine.invoke(refs.exit, "setLocked", { value: false });
      await engine.operation("sacura.object.visible", { object: refs.shadow, visible: false });
      await engine.operation("sacura.game.message", { message: "Ваш голос на ленте: «Смена закончилась тридцать лет назад. Не отвечай, если назовут по имени». Замок выхода щёлкнул." });
      await engine.emit("changed", { stage: "record" });
      return true;
    } },
    mirror: { name: "Посмотреть в зеркало", async run({ state, refs, engine }) {
      state.mirrorSeen = !state.mirrorSeen;
      await engine.operation("sacura.object.visible", { object: refs.shadow, visible: state.mirrorSeen });
      await engine.operation("sacura.game.message", { message: state.mirrorSeen ? "В зеркале вы всё ещё стоите у входа. Отражение поднимает руку, хотя вы не двигались." : "Отражение исчезло. На стекле изнутри проступило: «Не оставайся на вторую смену»." });
    } },
    escape: { name: "Завершить обход", result: { type: "boolean" }, async run({ params, state, refs, engine }) {
      if (state.escaped) return true;
      if (!state.power || (params.requireRecord && !state.record)) {
        await engine.operation("sacura.game.message", { message: "Заперто. В журнале сказано: восстановить питание и прослушать последнюю запись." }); return false;
      }
      await engine.invoke(refs.exit, "setLocked", { value: false });
      await engine.invoke(refs.exit, "open", {});
      state.escaped = true;
      state.objective = "Вы выбрались. Сегодня в журнале впервые появилась новая дата.";
      await engine.emit("changed", { stage: "escaped" });
      await engine.operation("sacura.story.enter", { beat: refs.ending });
      return true;
    } },
  },
};
