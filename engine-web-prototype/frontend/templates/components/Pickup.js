// Предмет/получатель назначаются ссылками в инспекторе. Без имён объектов в коде.
const component = {
  name: "Pickup",
  parameters: {
    type: "object",
    properties: { count: { type: "integer", min: 1, default: 1, label: "Количество" } },
  },
  state: {
    type: "object",
    properties: { collected: { type: "boolean", default: false, label: "Получен" } },
  },
  references: {
    type: "object",
    properties: {
      inventory: {
        type: "union",
        variants: [
          { type: "reference", referenceType: "object", label: "Объект" },
          { type: "reference", referenceType: "project", label: "Проект" },
        ],
      },
      item: { type: "reference", referenceType: "item", label: "Предмет" },
    },
  },
  methods: {
    pickup: {
      name: "Подобрать",
      result: { type: "boolean" },
      async run({ params, state, refs, engine, owner }) {
        if (state.collected) return false;
        state.collected = true;
        try {
          await engine.operation("sacura.inventory.add", {
            owner: refs.inventory,
            item: refs.item,
            count: params.count,
          });
        } catch (error) {
          state.collected = false;
          throw error;
        }
        await engine.operation("sacura.object.visible", { object: owner, visible: false });
        await engine.operation("sacura.game.message", { message: "Предмет в инвентаре" });
        return true;
      },
    },
  },
};
