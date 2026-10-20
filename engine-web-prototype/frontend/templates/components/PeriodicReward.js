// Timer — сериализуемый native handle этого компонента. Tick вызывает тот же Reward,
// что кнопка. Disable/Stop удаляет только его таймер; восстановление сохраняет remaining.
const component = {
  name: "PeriodicReward",
  parameters: {
    type: "object",
    properties: {
      seconds: { type: "number", min: 0.05, default: 1, label: "Интервал" },
      initialRate: { type: "number", default: 1, label: "Начальная скорость" },
      autoStart: { type: "boolean", default: true, label: "Запустить после подготовки" },
    },
  },
  state: {
    type: "object",
    properties: {
      rate: { type: "number", default: 1, label: "Награда за тик" },
      running: { type: "boolean", default: false, label: "Таймер включён" },
    },
  },
  stateFromParameters: { rate: "initialRate", running: "autoStart" },
  references: {
    type: "object",
    properties: {
      reward: { type: "reference", referenceType: "mechanic", label: "Общая награда" },
    },
  },
  lifecycle: { enable: "prepare", disable: "stop" },
  subscriptions: [{ id: "own-timer", signal: "timer", match: "timer", methodId: "tick" }],
  methods: {
    collect: {
      name: "Получить награду вручную",
      result: { type: "number" },
      async run({ state, refs, engine }) {
        // Улучшение меняет ручной сбор и таймер через один общий Reward.
        return await engine.invoke(refs.reward, "award", { factor: state.rate });
      },
    },
    prepare: {
      name: "Подготовить таймер",
      async run({ state, self, engine }) {
        if (state.running) await engine.invoke(self, "start", {});
      },
    },
    start: {
      name: "Запустить таймер",
      async run({ params, state, owner, engine }) {
        state.running = true;
        await engine.operation("sacura.timer.start", {
          owner,
          name: "reward",
          seconds: params.seconds,
          repeat: true,
          restart: false,
        });
      },
    },
    stop: {
      name: "Остановить таймер",
      async run({ state, owner, engine }) {
        state.running = false;
        await engine.operation("sacura.timer.stop", { owner, name: "reward" });
      },
    },
    tick: {
      name: "Выдать периодическую награду",
      async run({ state, refs, engine }) {
        if (state.running) await engine.invoke(refs.reward, "award", { factor: state.rate });
      },
    },
    upgrade: {
      name: "Улучшить",
      result: { type: "number" },
      run({ state }) {
        state.rate += 1;
        return state.rate;
      },
    },
  },
};
