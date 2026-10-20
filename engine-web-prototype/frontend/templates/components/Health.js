// Упражнение: в damage замените Math.max(0, args.amount - params.armor)
// на Math.max(0, args.amount * 2 - params.armor). Один удар — одно изменение HP.
// Native health/raycast, HUD и сохранение читают этот же state через явный адаптер.
const component = {
  name: "Health",
  parameters: {
    type: "object",
    properties: {
      initialHP: { type: "number", min: 1, default: 100, label: "Максимум / начальные HP" },
      armor: { type: "number", min: 0, default: 0, label: "Защита" },
    },
  },
  state: {
    type: "object",
    properties: {
      hp: { type: "number", min: 0, default: 100, label: "HP" },
      maxHP: { type: "number", min: 1, default: 100, label: "Максимум HP" },
      dead: { type: "boolean", default: false, label: "Погиб" },
    },
  },
  stateFromParameters: { hp: "initialHP", maxHP: "initialHP" },
  references: {
    type: "object",
    properties: {
      deathReaction: {
        type: "reference",
        referenceType: "mechanic",
        optional: true,
        label: "Реакция на смерть",
      },
    },
  },
  methods: {
    damage: {
      name: "Нанести урон",
      parameters: {
        type: "object",
        properties: { amount: { type: "number", min: 0, label: "Урон" } },
      },
      result: { type: "boolean" },
      async run({ args, params, state, engine, self, owner }) {
        if (state.dead) return false;
        const damage = Math.max(0, args.amount - params.armor);
        state.hp = Math.max(0, state.hp - damage);
        const died = state.hp === 0;
        state.dead = died;
        await engine.operation("sacura.health.publish", { owner, signal: "hit", amount: damage });
        if (died) {
          await engine.operation("sacura.health.publish", { owner, signal: "death" });
          await engine.invoke(self, "die", {});
        }
        await engine.operation("sacura.game.commit", {});
        return true;
      },
    },
    heal: {
      name: "Лечить",
      parameters: {
        type: "object",
        properties: { amount: { type: "number", min: 0, label: "Лечение" } },
      },
      result: { type: "boolean" },
      async run({ args, state, engine }) {
        if (state.dead) return false;
        state.hp = Math.min(state.maxHP, state.hp + args.amount);
        await engine.operation("sacura.game.commit", {});
        return true;
      },
    },
    die: {
      name: "Обработать смерть",
      result: { type: "void" },
      async run({ refs, engine, owner }) {
        if (refs.deathReaction)
          await engine.invoke(refs.deathReaction, "onDeath", { defeated: owner });
      },
    },
  },
};
