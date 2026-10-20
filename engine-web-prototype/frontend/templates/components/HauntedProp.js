// Один исходник для всех осматриваемых предметов. Владелец, текст и ссылки — у экземпляра.
// Измените порог distance или добавьте реакцию в ветку kind — это реальное поведение.
const component = {
  name: "HauntedProp",
  parameters: { type: "object", properties: {
    kind: { type: "string", enum: ["examine", "fuse", "power", "tape", "mirror", "exit"], options: { examine: "Осмотреть", fuse: "Взять предохранитель", power: "Включить питание", tape: "Прослушать запись", mirror: "Заглянуть в зеркало", exit: "Выйти" }, default: "examine", label: "Действие предмета" },
    text: { type: "string", default: "Здесь кто-то был.", label: "Текст осмотра" },
    distance: { type: "number", min: 1, default: 3.2, label: "Расстояние взаимодействия" },
  } },
  state: { type: "object", properties: { visits: { type: "integer", default: 0, label: "Осмотров" }, taken: { type: "boolean", default: false, label: "Подобран" } } },
  references: { type: "object", properties: {
    room: { type: "reference", referenceType: "mechanic", label: "Правила обхода · HauntedRoom" },
    viewer: { type: "reference", referenceType: "object", label: "Кто осматривает" },
  } },
  methods: { interact: { name: "Осмотреть / использовать", result: { type: "boolean" }, async run({ params, state, refs, owner, engine }) {
    const seen = await engine.operation("sacura.spatial.observe", { owner, target: refs.viewer });
    if (!seen.available || seen.distance > params.distance) {
      await engine.operation("sacura.game.message", { message: "Подойдите ближе: WASD. Зажмите правую кнопку мыши, чтобы осмотреться." }); return false;
    }
    state.visits += 1;
    if (params.kind === "fuse") {
      if (state.taken) return false;
      if (await engine.invoke(refs.room, "takeFuse", {})) {
        state.taken = true;
        await engine.operation("sacura.object.visible", { object: owner, visible: false });
      }
    } else if (params.kind === "power") return await engine.invoke(refs.room, "power", {});
    else if (params.kind === "tape") return await engine.invoke(refs.room, "listen", {});
    else if (params.kind === "mirror") await engine.invoke(refs.room, "mirror", {});
    else if (params.kind === "exit") return await engine.invoke(refs.room, "escape", {});
    else await engine.operation("sacura.game.message", { message: params.text });
    return true;
  } } },
};
