// Откройте onDeath: изменяйте награду/победу здесь, а не в скрытом preset.
const component = {
  name: "DeathReaction",
  parameters: {
    type: "object",
    properties: { hide: { type: "boolean", default: true, label: "Скрыть после смерти" } },
  },
  state: {
    type: "object",
    properties: { handled: { type: "boolean", default: false, label: "Смерть обработана" } },
  },
  references: {
    type: "object",
    properties: {
      reward: { type: "reference", referenceType: "mechanic", optional: true, label: "Награда" },
      objective: { type: "reference", referenceType: "mechanic", optional: true, label: "Задание" },
      behavior: {
        type: "reference",
        referenceType: "mechanic",
        optional: true,
        label: "Дерево погибшего",
      },
      battle: {
        type: "reference",
        referenceType: "mechanic",
        optional: true,
        label: "Остановить бой",
      },
      ending: {
        type: "reference",
        referenceType: "beat",
        optional: true,
        label: "Финал после смерти",
      },
    },
  },
  methods: {
    onDeath: {
      name: "Реакция на смерть",
      parameters: {
        type: "object",
        properties: {
          defeated: { type: "reference", referenceType: "object", label: "Погибший объект" },
        },
      },
      async run({ args, params, state, refs, engine }) {
        if (state.handled) return;
        state.handled = true;
        await engine.operation("sacura.movement.enabled", { owner: args.defeated, enabled: false });
        if (params.hide)
          await engine.operation("sacura.object.visible", {
            object: args.defeated,
            visible: false,
          });
        if (refs.reward) await engine.invoke(refs.reward, "award", {});
        if (refs.objective) await engine.invoke(refs.objective, "check", {});
        if (refs.ending) await engine.operation("sacura.story.enter", { beat: refs.ending });
        if (refs.behavior)
          await engine.operation("sacura.component.start", {
            instance: refs.behavior,
            methodId: "stop",
            args: {},
            lifetime: "component",
            lifetimeOwner: refs.behavior,
          });
        if (refs.battle)
          await engine.operation("sacura.component.start", {
            instance: refs.battle,
            methodId: "stop",
            args: {},
            lifetime: "component",
            lifetimeOwner: refs.battle,
          });
      },
    },
  },
};
