// Упражнение: в chase замените direction на direction.map(x => -x): враг отступает.
// Behavior Tree выбирает эти публичные методы. Цель/HP/дальность — ссылки и параметры
// конкретного экземпляра; дерево и исходник общие для всех противников.
const component = {
  name: "EnemyAgent",
  parameters: {
    type: "object",
    properties: {
      detection: { type: "number", min: 0, default: 30, label: "Обнаружение" },
      range: { type: "number", min: 0, default: 1.8, label: "Дальность удара" },
      damage: { type: "number", min: 0, default: 9, label: "Урон" },
      windup: { type: "number", min: 0, default: 0.3, label: "Подготовка удара" },
      interval: { type: "number", min: 0, default: 1, label: "Между ударами" },
    },
  },
  state: {
    type: "object",
    properties: { nextAttack: { type: "number", default: 0, label: "Следующая атака" } },
  },
  references: {
    type: "object",
    properties: {
      target: { type: "reference", referenceType: "object", label: "Цель" },
      targetHealth: { type: "reference", referenceType: "mechanic", label: "Здоровье цели" },
    },
  },
  methods: {
    attack: {
      name: "Атаковать",
      result: { type: "boolean" },
      async run({ params, state, refs, owner, engine }) {
        const selfHealth = await engine.operation("sacura.health.read", { owner }),
          seen = await engine.operation("sacura.spatial.observe", { owner, target: refs.target });
        if (selfHealth.dead || !seen.available || !seen.lineOfSight || seen.distance > params.range)
          return false;
        if (seen.clock < state.nextAttack) {
          await engine.wait(0.1);
          return true;
        }
        await engine.wait(params.windup);
        const now = await engine.operation("sacura.spatial.observe", {
          owner,
          target: refs.target,
        });
        const currentHealth = await engine.operation("sacura.health.read", { owner });
        if (
          !currentHealth.dead &&
          now.available &&
          now.lineOfSight &&
          now.distance <= params.range
        ) {
          state.nextAttack = now.clock + params.interval;
          await engine.invoke(refs.targetHealth, "damage", { amount: params.damage });
        }
        return true;
      },
    },
    chase: {
      name: "Преследовать",
      result: { type: "boolean" },
      async run({ params, refs, owner, engine }) {
        const health = await engine.operation("sacura.health.read", { owner }),
          seen = await engine.operation("sacura.spatial.observe", { owner, target: refs.target });
        if (health.dead || !seen.available || seen.distance > params.detection) return false;
        const direction = seen.direction;
        await engine.operation("sacura.movement.direction", { owner, direction });
        await engine.wait(0.1);
        return true;
      },
    },
    idle: {
      name: "Ждать",
      result: { type: "boolean" },
      async run({ engine }) {
        await engine.wait(0.1);
        return true;
      },
    },
  },
};
