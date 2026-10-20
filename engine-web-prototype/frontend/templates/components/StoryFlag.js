// Осмотр предмета явно меняет выбранную сюжетную переменную.
// Упражнение: измените value:true на value:false — ветка чтения письма станет недоступна.
const component = {
  name: "StoryFlag",
  references: {
    type: "object",
    properties: {
      variable: { type: "reference", referenceType: "variable", label: "Сюжетный флаг" },
    },
  },
  methods: {
    interact: {
      name: "Осмотреть и запомнить",
      result: { type: "boolean" },
      async run({ refs, engine }) {
        await engine.operation("sacura.variable.set", { variable: refs.variable, value: true });
        return true;
      },
    },
  },
};
