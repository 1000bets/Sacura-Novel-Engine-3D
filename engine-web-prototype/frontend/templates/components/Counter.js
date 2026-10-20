// Откройте Add: измените умножение на params.step — результат изменится во всех вызовах.
// Один исходник, независимое value у каждого подключения.
const component = {
  name: "Counter",
  parameters: {
    type: "object",
    properties: {
      initial: { type: "number", default: 0, label: "Начальное значение" },
      storySource: {
        type: "boolean",
        default: false,
        label: "Менять источник постоянной привязки явно",
      },
      step: { type: "number", default: 1, label: "Множитель изменения" },
    },
  },
  state: {
    type: "object",
    properties: { value: { type: "number", default: 0, label: "Текущее значение" } },
  },
  stateFromParameters: { value: "initial" },
  signals: {
    changed: {
      name: "Значение изменилось",
      payloadSchema: { type: "object", properties: { value: { type: "number" } } },
    },
  },
  methods: {
    add: {
      name: "Добавить",
      parameters: {
        type: "object",
        properties: { amount: { type: "number", default: 1, label: "Количество" } },
      },
      result: { type: "number" },
      async run({ args, params, state, engine }) {
        const next = state.value + args.amount * params.step;
        if (params.storySource) await engine.writeBinding("value", next);
        else state.value = next;
        await engine.emit("changed", { value: state.value });
        return state.value;
      },
    },
    read: {
      name: "Прочитать",
      result: { type: "number" },
      run({ state }) {
        return state.value;
      },
    },
    multiply: {
      name: "Умножить",
      parameters: {
        type: "object",
        properties: { factor: { type: "number", default: 2, label: "Множитель" } },
      },
      result: { type: "number" },
      async run({ args, params, state, engine }) {
        const next = state.value * args.factor;
        if (params.storySource) await engine.writeBinding("value", next);
        else state.value = next;
        await engine.emit("changed", { value: state.value });
        return state.value;
      },
    },
  },
};
