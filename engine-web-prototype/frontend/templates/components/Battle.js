// История явно запускает Run с lifetime=scene. Компоненты Health сами бой не начинают.
// Обычная следующая реплика не отменяет Run. Stop/отмена владельца останавливают дочерние BT.
const component = {
  name: "Battle",
  state: {
    type: "object",
    properties: { running: { type: "boolean", default: false, label: "Бой идёт" } },
  },
  references: {
    type: "object",
    properties: {
      behaviors: {
        type: "collection",
        items: { type: "reference", referenceType: "mechanic" },
        default: [],
        label: "Деревья противников",
      },
      objective: { type: "reference", referenceType: "mechanic", label: "Задание" },
    },
  },
  lifecycle: { disable: "stop" },
  methods: {
    run: {
      name: "Запустить бой",
      async run({ state, refs, engine }) {
        if (state.running) return;
        state.running = true;
        await engine.invoke(refs.objective, "begin", {});
        try {
          await Promise.all(refs.behaviors.map((instance) => engine.invoke(instance, "start", {})));
        } finally {
          state.running = false;
        }
      },
    },
    stop: {
      name: "Остановить бой",
      async run({ state, refs, engine }) {
        state.running = false;
        for (const instance of refs.behaviors) await engine.invoke(instance, "stop", {});
      },
    },
  },
};
