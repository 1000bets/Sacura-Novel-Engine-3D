// Упражнения: измените damage * 1 на damage * 2; замените cost = 1 на cost = 2.
// Только геометрия луча/снаряда и визуализация являются native-примитивами.
const component = {
  name: "Weapon",
  parameters: {
    type: "object",
    properties: {
      damage: { type: "number", min: 0, default: 25, label: "Базовый урон" },
      magazine: { type: "integer", min: 1, default: 6, label: "Ёмкость магазина" },
      reserve: { type: "integer", min: 0, default: 30, label: "Начальный запас" },
      cooldown: { type: "number", min: 0, default: 0.3, label: "Пауза выстрела" },
      reloadTime: { type: "number", min: 0, default: 1, label: "Время перезарядки" },
      range: { type: "number", min: 0, default: 40, label: "Дальность" },
      mode: {
        type: "string",
        enum: ["hitscan", "projectile"],
        default: "hitscan",
        label: "Луч / снаряд",
      },
      projectileSpeed: { type: "number", min: 0, default: 20, label: "Скорость снаряда" },
      recoil: { type: "number", min: 0, default: 0.025, label: "Отдача" },
    },
  },
  state: {
    type: "object",
    properties: {
      ammo: { type: "integer", min: 0, default: 6, label: "В магазине" },
      reserve: { type: "integer", min: 0, default: 30, label: "В запасе" },
      nextShot: { type: "number", default: 0 },
      reloadUntil: { type: "number", default: 0 },
      enabled: { type: "boolean", default: true },
    },
  },
  stateFromParameters: { ammo: "magazine", reserve: "reserve" },
  references: {
    type: "object",
    properties: {
      record: {
        type: "reference",
        referenceType: "weapon",
        label: "Native-геометрия этого оружия",
      },
    },
  },
  methods: {
    select: {
      name: "Выбрать это оружие",
      async run({ owner, refs, engine }) {
        await engine.operation("sacura.weapon.select", { owner, weapon: refs.record });
      },
    },
    shoot: {
      name: "Выстрелить",
      parameters: {
        type: "object",
        properties: {
          origin: { type: "vector", size: 3, optional: true, label: "Начало луча" },
          direction: { type: "vector", size: 3, optional: true, label: "Направление луча" },
        },
      },
      result: { type: "boolean" },
      async run({ args, params, state, refs, owner, engine }) {
        const clock = await engine.operation("sacura.game.clock", {}),
          health = await engine.operation("sacura.health.read", { owner });
        const cost = 1;
        const aim = await engine.operation("sacura.weapon.aim", {
          owner,
          weapon: refs.record,
          ...args,
        });
        if (
          !aim.valid ||
          !state.enabled ||
          health.dead ||
          state.ammo < cost ||
          clock < state.nextShot ||
          clock < state.reloadUntil
        )
          return false;
        state.ammo -= cost;
        state.nextShot = clock + params.cooldown;
        const damage = params.damage * 1;
        if (params.mode === "projectile")
          await engine.operation("sacura.weapon.projectile", {
            owner,
            origin: aim.origin,
            direction: aim.direction,
            range: params.range,
            speed: params.projectileSpeed,
            damage,
          });
        else {
          const hit = await engine.operation("sacura.weapon.raycast", {
            owner,
            origin: aim.origin,
            direction: aim.direction,
            range: params.range,
          });
          await engine.operation("sacura.weapon.tracer", { from: aim.origin, to: hit.point });
          if (hit.hit)
            await engine.operation("sacura.health.damage", { owner: hit.target, amount: damage });
        }
        await engine.operation("sacura.weapon.recoil", { owner, amount: params.recoil });
        await engine.operation("sacura.game.commit", {});
        return true;
      },
    },
    reload: {
      name: "Перезарядить",
      result: { type: "boolean" },
      async run({ params, state, engine, owner }) {
        const clock = await engine.operation("sacura.game.clock", {}),
          health = await engine.operation("sacura.health.read", { owner });
        if (
          !state.enabled ||
          health.dead ||
          state.ammo >= params.magazine ||
          state.reserve <= 0 ||
          state.reloadUntil > clock
        )
          return false;
        state.reloadUntil = clock + params.reloadTime;
        await engine.wait(params.reloadTime);
        const amount = Math.min(params.magazine - state.ammo, state.reserve);
        state.ammo += amount;
        state.reserve -= amount;
        state.reloadUntil = 0;
        await engine.operation("sacura.game.commit", {});
        return true;
      },
    },
  },
};
