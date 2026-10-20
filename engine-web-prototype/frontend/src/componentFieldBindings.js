import {variableKey,validateValue,schemaDefaults} from './valueSchemas.js';

export function assertComponentFieldBindings(project,m,d){
 const seen=new Set();for(const b of m.fieldBindings||[]){const key=b.target+'.'+b.field,schema=(b.target==='state'?d.stateSchema:b.target==='parameters'?d.parameters:null)?.properties?.[b.field];if(!schema||seen.has(key))throw Error('Проверьте постоянную привязку поля компонента.');seen.add(key);const name=variableKey(project,b.variable?.id);if(b.variable?.kind!=='reference'||b.variable.type!=='variable'||validateValue(schema,project.variables[name],{project,allowDeferred:true}).length)throw Error('Тип переменной не соответствует полю «'+(schema.label||b.field)+'».');}
}
export function componentFieldView(project,m,d,values,variables,target='state'){
 const schema=target==='state'?d.stateSchema:d.parameters,view={};
 for(const [field,s]of Object.entries(schema.properties)){
  const binding=m.fieldBindings?.find(b=>b.target===target&&b.field===field);
  Object.defineProperty(view,field,{enumerable:true,configurable:true,get(){if(!binding)return values[field];const key=variableKey(project,binding.variable.id),value=variables[key],issues=validateValue(s,value,{project,allowResolvedReferences:true});if(issues.length)throw Error('Привязка «'+(s.label||field)+'» → '+key+': '+issues.map(x=>x.message).join(' '));return value;},set(value){if(binding)throw Error('Поле «'+(s.label||field)+'» постоянно связано с переменной «'+variableKey(project,binding.variable.id)+'». Измените переменную истории явно.');values[field]=value;}});
 }
 return view;
}
export function componentStateView(project,m,d,state,variables){return componentFieldView(project,m,d,state,variables);}
export function componentParameterView(project,m,d,variables){return componentFieldView(project,m,d,{...schemaDefaults(d.parameters),...Object.fromEntries(Object.entries(m.values||{}).filter(([,value])=>value!==undefined))},variables,'parameters');}
export function removeBoundState(m,state){for(const b of m.fieldBindings||[])if(b.target==='state')delete state[b.field];return state;}
