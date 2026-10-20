// Author-written SDK source used by the integration acceptance, absent from templates.
export const ledgerSource=`const component = {
  name: "AcceptanceLedger",
  state: {type:"object", properties:{total:{type:"number",default:0,label:"Журнал"}}},
  methods: {record:{name:"Записать", parameters:{type:"object",properties:{amount:{type:"number",label:"Сумма"}}}, result:{type:"number"}, run({args,state}) { state.total += args.amount; return state.total; }}}
};`;
export const relaySource=`const component = {
  name: "ThresholdRelay",
  parameters: {type:"object",properties:{factor:{type:"number",default:2,label:"Множитель"},threshold:{type:"number",default:5,label:"Порог"},delay:{type:"number",min:0,default:0,label:"Задержка"}}},
  state: {type:"object",properties:{total:{type:"number",default:0,label:"Сумма"},initializations:{type:"integer",default:0,label:"Инициализации"}}},
  references: {type:"object",properties:{ledger:{type:"reference",referenceType:"mechanic",label:"Журнал-получатель"}}},
  signals: {accepted:{name:"Принято",payloadSchema:{type:"object",properties:{value:{type:"number"}}}}},
  lifecycle: {initialize:"prepare"},
  methods: {
    prepare: {name:"Подготовить",run({state}){state.initializations+=1;}},
    apply: {name:"Применить",parameters:{type:"object",properties:{amount:{type:"number",default:3,label:"Количество"}}},result:{type:"number"},async run({args,params,state,refs,engine}) {
      const next = state.total + args.amount * params.factor;
      if(next < params.threshold) return state.total;
      await engine.wait(params.delay);
      const recorded = await engine.invoke(refs.ledger,"record",{amount:next});
      state.total = next;
      await engine.emit("accepted",{value:next});
      await engine.operation("sacura.game.message",{message:"Принято: "+next+"; журнал: "+recorded});
      return next;
    }}
  }
};`;
