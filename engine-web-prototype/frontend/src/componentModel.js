import {componentStateView} from './componentFieldBindings.js';
import {assertSchema,assertSerializable,schemaDefaults,validateValue} from './valueSchemas.js';
import {signalDefinition} from './signalModel.js';

// Components extend authored mechanics. Function IDs point at the existing JS/graph
// documents; method keys are local, stable symbols and never project identities.
export const isComponent=d=>d?.componentVersion!==undefined;
export const componentMethods=d=>Object.entries(d.methods||{});
export function assertComponentDefinition(project,d){
 if(!isComponent(d))return;
 assertSerializable(d);
 if(d.componentVersion!==1)throw Error('Версия модели компонента недоступна.');
 if(!Number.isInteger(d.stateVersion)||d.stateVersion<1)throw Error('Укажите версию сохраняемого состояния компонента.');
 if(!d.methods||Array.isArray(d.methods))throw Error('Опишите методы компонента.');
 for(const [id,m] of componentMethods(d)){
  if(!id.trim()||!m.name?.trim())throw Error('Методу нужны стабильный ID и имя.');
  assertSchema(m.parameters);assertSchema(m.result);
  if(m.parameters.type!=='object')throw Error('Аргументы метода должны иметь имена.');
  const f=project.functions?.find(f=>f.id===m.logic?.functionId);
  if(!f)throw Error('Реализация метода недоступна: '+id);
  const names=(f.parameters||[]).map(p=>p.name);
  if(Object.keys(m.parameters.properties).some(k=>!names.includes(k))||names.some(k=>!Object.hasOwn(m.parameters.properties,k)))throw Error('Сигнатура функции не совпадает с методом: '+id);
 }
 assertSchema(d.references||{type:'object',properties:{}});
 if(d.references&&d.references.type!=='object')throw Error('Опишите именованные ссылки компонента.');
 for(const [field,parameter] of Object.entries(d.stateFromParameters||{})){
  if(!Object.hasOwn(d.stateSchema.properties,field)||!Object.hasOwn(d.parameters.properties,parameter))throw Error('Начальное состояние ссылается на отсутствующий параметр.');
 }
 for(const [hook,methodId] of Object.entries(d.lifecycle||{})){
  if(!['initialize','restore','enable','disable','destroy'].includes(hook)||!Object.hasOwn(d.methods,methodId))throw Error('Неизвестный lifecycle или метод: '+hook);
  if(validateValue(d.methods[methodId].parameters,schemaDefaults(d.methods[methodId].parameters),{project,allowDeferred:true}).length)throw Error('Lifecycle не принимает обязательных аргументов.');
 }
 for(const [id,event] of Object.entries(d.events||{})){
  if(!id||!event.name||!signalDefinition(project,event.signalId))throw Error('Определите сигнал события компонента: '+id);
  assertSchema(event.payloadSchema);
  if(JSON.stringify(event.payloadSchema)!==JSON.stringify(signalDefinition(project,event.signalId).payloadSchema))throw Error('Схема события не совпадает со схемой сигнала: '+id);
 }
 const ids=new Set();
 for(const s of d.subscriptions||[]){
  if(!s.id||ids.has(s.id)||!signalDefinition(project,s.signalId)||!Object.hasOwn(d.methods,s.methodId)||!['owner','any','component','timer'].includes(s.match||'owner'))throw Error('Проверьте подписку компонента.');
  if(s.match==='component'&&!Object.hasOwn(d.references?.properties||{},s.reference))throw Error('Выберите ссылку подписки на компонент.');
  ids.add(s.id);
 }
}
export function componentInitialState(d,values){
 const state=structuredClone({...schemaDefaults(d.stateSchema),...d.initialState});
 for(const [field,parameter] of Object.entries(d.stateFromParameters||{}))state[field]=structuredClone(values[parameter]);
 return state;
}
export function validateComponentSave(project,state,variables=project.variables){
 for(const m of project.gameplay.mechanics.filter(m=>m.definitionId)){
  const d=project.gameplay.mechanicDefinitions.find(d=>d.id===m.definitionId&&d.version===m.definitionVersion);if(!isComponent(d))continue;
  if(d.behaviorTree?.autoStart===false&&state.componentBehaviors?.[m.id]?.enabled)throw Error('Сохранение содержит выполняющееся дерево без сериализуемой контрольной точки. Завершите задачу перед сохранением.');
  const record=state.componentLifecycles?.[m.id];
  if(!record||record.stateVersion!==d.stateVersion)throw Error('Версия состояния компонента «'+d.name+'» требует миграции. Загрузка отменена.');
  if(typeof record.initialized!=='boolean'||typeof record.enabled!=='boolean'||typeof record.destroyed!=='boolean')throw Error('Сохранение lifecycle компонента повреждено.');
  const errors=validateValue(d.stateSchema,componentStateView(project,m,d,state.mechanicStates?.[m.id]||{},variables),{project,scope:m.scope,allowDeferred:true});
  if(errors.length)throw Error('Состояние компонента повреждено: '+errors.map(e=>e.message).join(' '));
 }
 for(const id of Object.keys(state.componentLifecycles||{}))if(!project.gameplay.mechanics.some(m=>m.id===id))throw Error('Сохранение содержит удалённый компонент.');
}
