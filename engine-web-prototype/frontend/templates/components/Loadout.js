// Ввод адресует Loadout; Fire/Reload вызывают выбранный экземпляр Weapon.
// Это ссылка на конкретное подключение, а не «первое оружие такого типа».
const component = {
  name: "Loadout",
  parameters: {
    type: "object",
    properties: {
      initial: { type: "reference", referenceType: "mechanic", label: "Начальное оружие · Weapon" },
    },
  },
  state: {
    type: "object",
    properties: {
      selected: {
        type: "reference",
        referenceType: "mechanic",
        optional: true,
        label: "Выбранное оружие",
      },
    },
  },
  stateFromParameters: { selected: "initial" },
  lifecycle: { initialize: "prepare" },
  methods: {
    prepare: {
      name: "Подготовить выбор",
      async run({ state, engine }) {
        await engine.invoke(state.selected, "select", {});
      },
    },
    select: {
      name: "Выбрать оружие",
      parameters: {
        type: "object",
        properties: {
          weapon: { type: "reference", referenceType: "mechanic", label: "Конкретное оружие" },
        },
      },
      async run({ args, state, engine }) {
        await engine.invoke(args.weapon, "select", {});
        state.selected = args.weapon;
      },
    },
    fire: {
      name: "Огонь выбранным оружием",
      result: { type: "boolean" },
      async run({ state, engine }) {
        return await engine.invoke(state.selected, "shoot", {});
      },
    },
    reload: {
      name: "Перезарядить выбранное оружие",
      result: { type: "boolean" },
      async run({ state, engine }) {
        return await engine.invoke(state.selected, "reload", {});
      },
    },
  },
};
