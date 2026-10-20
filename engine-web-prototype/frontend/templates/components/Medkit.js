// Лечение и расход предмета открыты: сначала проверяем Health, затем расходуем одну аптечку.
const component = {
  name: "Medkit",
  parameters: {
    type: "object",
    properties: { heal: { type: "number", min: 0, default: 35, label: "Восстановить HP" } },
  },
  references: {
    type: "object",
    properties: {
      health: { type: "reference", referenceType: "mechanic", label: "Здоровье получателя" },
      inventory: { type: "reference", referenceType: "object", label: "Владелец инвентаря" },
      item: { type: "reference", referenceType: "item", label: "Предмет аптечки" },
    },
  },
  methods: {
    use: {
      name: "Использовать аптечку",
      result: { type: "boolean" },
      async run({ params, refs, engine }) {
        if (
          (await engine.operation("sacura.inventory.count", {
            owner: refs.inventory,
            item: refs.item,
          })) < 1
        )
          return false;
        const healed = await engine.invoke(refs.health, "heal", { amount: params.heal });
        if (!healed) return false;
        await engine.operation("sacura.inventory.remove", {
          owner: refs.inventory,
          item: refs.item,
          count: 1,
        });
        await engine.operation("sacura.game.message", { message: "Здоровье восстановлено" });
        return true;
      },
    },
  },
};
