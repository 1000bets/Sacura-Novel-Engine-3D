export function counterSource({multiply=false,formula='args.amount * params.step'}={}){return `// Метаданные и исполняемый код — один ресурс проекта.
function append(history, value) { history.push(value); }
const component = {
  name: "Counter",
  parameters: { type: "object", properties: { step: { type: "number", default: 1 } } },
  state: { type: "object", properties: {
    value: { type: "number", default: 0 },
    history: { type: "collection", items: { type: "number" }, default: [] }
  } },
  references: { type: "object", properties: { other: { type: "reference", referenceType: "mechanic", optional: true } } },
  signals: { changed: { name: "Changed", payloadSchema: { type: "object", properties: { value: { type: "number" } } } } },
  methods: {
    add: {
      name: "Add", parameters: { type: "object", properties: { amount: { type: "number", default: 1 } } }, result: { type: "number" },
      async run({ args, params, state, engine }) {
        const delta = ${formula};
        if (Number.isFinite(delta)) state.value += delta;
        append(state.history, state.value);
        await engine.emit("changed", { value: state.value });
        return state.value;
      }
    }${multiply?`,
    multiply: {
      name: "Multiply", parameters: { type: "object", properties: { factor: { type: "number" } } }, result: { type: "number" },
      run({ args, state }) { state.value *= args.factor; return state.value; }
    }`:''}
  }
};
`;}
