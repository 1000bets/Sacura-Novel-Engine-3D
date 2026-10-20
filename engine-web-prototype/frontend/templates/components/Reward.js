// Упражнение: замените base * multiplier на base * multiplier + 5.
// Получатель — явная ссылка на Counter, одинаковая логика для очков, монет и отношения.
// Добавьте новый метод bonus с вызовом engine.invoke(refs.counter, "add", {amount: ...}).
const component = {
  name: "Reward",
  parameters: {
    type: "object",
    properties: {
      base: { type: "number", default: 1, label: "Базовая награда" },
      multiplier: { type: "number", default: 1, label: "Множитель награды" },
    },
  },
  references: {
    type: "object",
    properties: {
      counter: { type: "reference", referenceType: "mechanic", label: "Куда начислять · Counter" },
    },
  },
  state: {
    type: "object",
    properties: { awards: { type: "integer", default: 0, label: "Выдано наград" } },
  },
  signals: {
    awarded: {
      name: "Награда выдана",
      payloadSchema: {
        type: "object",
        properties: { amount: { type: "number" }, value: { type: "number" } },
      },
    },
  },
  methods: {
    award: {
      name: "Начислить награду",
      parameters: {
        type: "object",
        properties: { factor: { type: "number", default: 1, label: "Разовый множитель" } },
      },
      result: { type: "number" },
      async run({ args, params, refs, state, engine }) {
        const amount = params.base * params.multiplier * args.factor;
        const value = await engine.invoke(refs.counter, "add", { amount });
        state.awards += 1;
        await engine.emit("awarded", { amount, value });
        return value;
      },
    },
  },
};
