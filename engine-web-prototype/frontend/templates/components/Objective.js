// Цели — коллекция или тег в параметрах. Количество противников в код не зашито.
const component = {
  name: "Objective",
  parameters: {
    type: "object",
    properties: {
      targets: {
        type: "collection",
        items: { type: "reference", referenceType: "object" },
        default: [],
        label: "Цели задания · объекты / тег",
      },
    },
  },
  state: {
    type: "object",
    properties: {
      active: { type: "boolean", default: false },
      complete: { type: "boolean", default: false },
    },
  },
  references: {
    type: "object",
    properties: {
      battle: {
        type: "reference",
        referenceType: "mechanic",
        optional: true,
        label: "Остановить бой",
      },
      victory: { type: "reference", referenceType: "beat", label: "Победный финал" },
    },
  },
  methods: {
    begin: {
      name: "Начать задание",
      run({ state }) {
        state.active = true;
      },
    },
    check: {
      name: "Проверить задание",
      result: { type: "boolean" },
      async run({ params, state, refs, engine }) {
        if (!state.active || state.complete) return false;
        if (!(await engine.operation("sacura.health.all-dead", { objects: params.targets })))
          return false;
        state.complete = true;
        if (refs.battle)
          await engine.operation("sacura.component.start", {
            instance: refs.battle,
            methodId: "stop",
            args: {},
            lifetime: "component",
            lifetimeOwner: refs.battle,
          });
        await engine.operation("sacura.signal.victory", {});
        await engine.operation("sacura.story.enter", { beat: refs.victory });
        return true;
      },
    },
  },
};
