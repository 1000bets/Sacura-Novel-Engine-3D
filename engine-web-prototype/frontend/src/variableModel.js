import {ANSWER_VARIABLE,typedValue} from './choiceModel.js';
export const VARIABLE_TYPES=[['boolean','Boolean · Да / нет'],['integer','Integer · Целое число'],['number','Float · Число'],['string','String · Текст']];
export const variableType=(p,id)=>p.variableTypes?.[id]||(id===ANSWER_VARIABLE?'string':(Object.hasOwn(p.variables||{},id)?typeof p.variables[id]:undefined));
export const runtimeVariableType=(p,id)=>variableType(p,id)==='integer'?'number':variableType(p,id);
export const variableValue=(p,id,value)=>variableType(p,id)==='integer'?Math.trunc(typedValue(value,'number')):typedValue(value,runtimeVariableType(p,id));
export function variableReferences(p,id){
 const refs=[];
 for(const b of (p.chapters||[]).flatMap(c=>c.beats||[])){
  if(b.variable===id||b.resultVariable===id||(b.test?.rules||[]).some(r=>r.variable===id)||(b.choices||[]).some(c=>c.condition===id||c.availability?.rules?.some(r=>r.variable===id))||(b.bindings||[]).some(bind=>bind.condition===id||Object.values(bind.actionOverrides||{}).some(a=>a.target===id)))refs.push(b.id);
 }
 for(const event of p.events||[])if(event.groups?.some(g=>g.actions.some(a=>a.type==='variable'&&a.target===id)))refs.push(event.id);
 for(const action of p.input?.actions||[])if(['variable','variableX','variableY'].some(field=>action[field]===id))refs.push(action.id);
 return [...new Set(refs)];
}
export function editVariable(p,id,patch){
 const name=(patch.name??id).trim();
 if(!name||name===ANSWER_VARIABLE&&id!==ANSWER_VARIABLE)return 'Введите имя переменной.';
 if(name!==id&&Object.hasOwn(p.variables,name))return 'Переменная с таким именем уже существует.';
 if(patch.remove){if(variableReferences(p,id).length)return 'Переменная используется в графе. Сначала удалите её ноды и действия.';delete p.variables[id];if(p.variableTypes)delete p.variableTypes[id];return null;}
 if(id===ANSWER_VARIABLE){if(name!==id)return 'Системную переменную нельзя переименовать.';p.variableTypes??={};if(patch.type)p.variableTypes[id]=patch.type;for(const b of p.chapters.flatMap(c=>c.beats))if(b.variable===id)b.valueType=runtimeVariableType(p,id);return null;}
 if(name!==id){
  if(Object.hasOwn(p.variables,id)){p.variables[name]=p.variables[id];delete p.variables[id];}
  if(p.variableTypes&&Object.hasOwn(p.variableTypes,id)){p.variableTypes[name]=p.variableTypes[id];delete p.variableTypes[id];}
  for(const b of (p.chapters||[]).flatMap(c=>c.beats||[])){
   if(b.variable===id)b.variable=name;if(b.resultVariable===id)b.resultVariable=name;
   for(const r of b.test?.rules||[])if(r.variable===id)r.variable=name;
   for(const c of b.choices||[]){if(c.condition===id)c.condition=name;for(const r of c.availability?.rules||[])if(r.variable===id)r.variable=name;}
   for(const bind of b.bindings||[]){if(bind.condition===id)bind.condition=name;for(const a of Object.values(bind.actionOverrides||{}))if(a.target===id)a.target=name;}
  }
  for(const e of p.events||[])for(const g of e.groups||[])for(const a of g.actions||[])if(a.type==='variable'&&a.target===id)a.target=name;
  for(const action of p.input?.actions||[])for(const field of ['variable','variableX','variableY'])if(action[field]===id)action[field]=name;
 }
 p.variableTypes??={};if(patch.type)p.variableTypes[name]=patch.type;
 p.variables[name]=variableValue(p,name,'value'in patch?patch.value:p.variables[name]);
 for(const b of (p.chapters||[]).flatMap(c=>c.beats||[]))if(b.variable===name)b.valueType=runtimeVariableType(p,name);
 return null;
}
