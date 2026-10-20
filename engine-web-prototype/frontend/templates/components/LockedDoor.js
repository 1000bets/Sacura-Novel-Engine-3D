// Два подключения используют один исходник. Условия меняются здесь.
// Упражнение: вместо count > 0 потребуйте count >= 2 или добавьте вызов Reward.award.
// SetLocked(false) меняет только замок. Open — отдельная ожидаемая анимация.
const component = {
  name: "LockedDoor",
  parameters: {
    type: "object",
    properties: {
      InitiallyLocked: { type: "boolean", default: true, label: "Изначально заперта · IsLocked" },
      requiresKey: { type: "boolean", default: false, label: "Разрешать открытие ключом" },
      consumeKey: { type: "boolean", default: false, label: "Расходовать ключ" },
      OpenSeconds: { type: "number", min: 0, default: 0.5, label: "Время открытия, сек" },
    },
  },
  state: {
    type: "object",
    properties: { IsLocked: { type: "boolean", default: true, label: "Заперта · IsLocked" } },
  },
  stateFromParameters: { IsLocked: "InitiallyLocked" },
  references: {
    type: "object",
    properties: {
      inventory: {
        type: "union",
        optional: true,
        label: "Инвентарь",
        variants: [
          { type: "reference", referenceType: "object", label: "Инвентарь объекта" },
          { type: "reference", referenceType: "project", label: "Инвентарь проекта" },
        ],
      },
      key: { type: "reference", referenceType: "item", optional: true, label: "Нужный ключ" },
    },
  },
  signals: { opened: { name: "Дверь открыта", payloadSchema: { type: "object", properties: {} } } },
  methods: {
    setLocked: {
      name: "Задать замок",
      parameters: {
        type: "object",
        properties: { value: { type: "boolean", default: false, label: "Заперта" } },
      },
      result: { type: "boolean" },
      run({ args, state }) {
        state.IsLocked = args.value;
        return state.IsLocked;
      },
    },
    canUnlock: {
      name: "Проверить ключ",
      result: { type: "boolean" },
      async run({ params, refs, engine }) {
        if (!params.requiresKey || !refs.inventory || !refs.key) return false;
        const count = await engine.operation("sacura.inventory.count", {
          owner: refs.inventory,
          item: refs.key,
        });
        return count > 0;
      },
    },
    interact: {
      name: "Взаимодействовать",
      result: { type: "boolean" },
      async run({ state, params, refs, engine, self }) {
        if (state.IsLocked) {
          if (!(await engine.invoke(self, "canUnlock", {}))) {
            await engine.operation("sacura.game.message", { message: "Заперто" });
            return false;
          }
          if (params.consumeKey)
            await engine.operation("sacura.inventory.remove", {
              owner: refs.inventory,
              item: refs.key,
              count: 1,
            });
          await engine.invoke(self, "setLocked", { value: false });
        }
        return await engine.invoke(self, "open", {});
      },
    },
    open: {
      name: "Открыть",
      result: { type: "boolean" },
      async run({ engine, owner, params }) {
        await engine.operation("sacura.door.animate", {
          object: owner,
          open: true,
          seconds: params.OpenSeconds,
        });
        await engine.emit("opened", {});
        return true;
      },
    },
  },
};
