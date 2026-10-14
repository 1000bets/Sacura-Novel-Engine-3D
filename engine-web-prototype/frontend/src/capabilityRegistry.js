import {namespacedId,operationDefinition,registerOperation} from './operationRegistry.js';
import {registerSignalDefinition,signalDefinition} from './signalModel.js';
import {assertSchema,validateValue,schemaDefaults}
 from './valueSchemas.js';
import {assertSerializable} from './valueSchemas.js';

// Installed engine code only. Authoring packages never invoke registration.
const modules=new Map(),methods=new Map(),commands=new Map();
export function registerCapability({id,version=1,initialize,enter,sync,cleanup,dispose,update,methods:handlers={},commands:aliases={},operations=[],bindings={},data,signals=[],validateSavedState,validateProject}){
 if(!namespacedId(id)||!Number.isInteger(version)||version<1||modules.has(id))throw Error('Возможность с этим ID уже зарегистрирована или её версия неверна.');
 for(const hook of [initialize,enter,sync,cleanup,update,validateProject,validateSavedState])if(hook!==undefined&&typeof hook!=='function')throw Error('Lifecycle возможности должен быть функцией.');
 for(const b of Object.values(bindings))if(!b?.schema||typeof b.read!=='function')throw Error('Привязке нужны схема и обработчик чтения.');
 const operationIds=new Set();for(const op of operations){const key=op.definition.id+'@'+(op.definition.version??version);if(operationIds.has(key)||operationDefinition(op.definition.id,op.definition.version??version))throw Error('Повтор ID операции возможности: '+key);operationIds.add(key);}
 for(const name of Object.keys(handlers))if(['__proto__','constructor','prototype'].includes(name)||methods.has(name)||typeof handlers[name]!=='function')throw Error('Повторный обработчик возможности: '+name);
 for(const name of Object.keys(aliases))if(commands.has(name)||typeof aliases[name]!=='function')throw Error('Повторная команда возможности: '+name);
 if(data){assertSerializable(data);assertSchema(data.parameters);assertSchema(data.stateSchema);if(!data.name||!data.scopes?.length||data.scopes.some(s=>!['object','scene','project'].includes(s)))throw Error('Данным возможности нужны имя и размещение.');}
 const signalIds=new Set();for(const signal of signals){if(signalIds.has(signal.id)||signalDefinition({},signal.id))throw Error('Повтор ID сигнала возможности.');signalIds.add(signal.id);assertSerializable(signal);assertSchema(signal.payloadSchema);}
 for(const signal of signals)registerSignalDefinition(signal);
 const metadata={id,version,...(data?{data}:{}),bindings:Object.fromEntries(Object.entries(bindings).map(([field,b])=>[field,b.schema]))};assertSerializable(metadata);
 for(const op of operations)registerOperation({...op.definition,version:op.definition.version??version,contexts:op.definition.contexts||['event','function','rule','mechanic','widget'],editor:{category:'Игровые возможности',...op.definition.editor}},op.execute,op.validate);
 const module={...metadata,initialize,enter,sync,cleanup,dispose,update,bindings,data,validateSavedState,validateProject};modules.set(id,module);
 for(const [name,fn]of Object.entries(handlers))methods.set(name,fn);
 for(const [name,fn]of Object.entries(aliases))commands.set(name,fn);
 return module;
}
export const capabilityDefinitions=()=>[...modules.values()].map(({id,version,bindings})=>({id,version,bindings:Object.fromEntries(Object.entries(bindings).map(([field,b])=>[field,b.schema]))}));
export const capabilityBinding=(id,field)=>modules.get(id)?.bindings[field];
export const hasCapabilityCommand=name=>commands.has(name);
export function capabilityCommand(session,name,value,context){return commands.get(name).call(session,value,context);}
export function capabilityMethod(session,name,...args){const fn=methods.get(name);if(!fn)throw Error('Обработчик возможности недоступен: '+name);return fn.apply(session,args);}
export function initializeCapabilities(session){for(const m of modules.values())m.initialize?.call(session);}
export function attachCapabilities(session){for(const [name,fn]of methods)session[name]=(...args)=>{session.host.assert(session.generation);return fn.apply(session,args);};}
export function updateCapabilities(session,dt,publish){for(const m of modules.values())m.update?.call(session,dt,publish);}
export function enterCapabilities(session){for(const m of modules.values())m.enter?.call(session);}
export function syncCapabilities(session){for(const m of modules.values())m.sync?.call(session);}
export function cleanupCapabilities(session){for(const m of modules.values())m.cleanup?.call(session);}

export const capabilityDataDefinitions=()=>[...modules.values()].filter(m=>m.data).map(m=>({id:m.id,version:m.version,...structuredClone(m.data)}));
export const capabilityDataDefinition=id=>modules.get(id)?.data;
export function validateCapabilityData(project){const issues=[];for(const m of project.gameplay?.mechanics||[])if(m.capabilityId){const d=modules.get(m.capabilityId);if(!d?.data||d.version!==m.capabilityVersion){issues.push('Возможность или её версия недоступна: '+m.capabilityId);continue;}if(!d.data.scopes.includes(m.scope.kind))issues.push('Размещение возможности не поддерживается.');issues.push(...validateValue(d.data.parameters,{...schemaDefaults(d.data.parameters),...m.values},{project,scope:m.scope,allowDeferred:true}).map(e=>e.message));}for(const d of modules.values())issues.push(...(d.validateProject?.(project)||[]));return issues;}
export function validateSavedCapabilities(project,state){for(const m of project.gameplay?.mechanics||[])if(m.capabilityId){const d=modules.get(m.capabilityId);if(!d?.data||d.version!==m.capabilityVersion)throw Error('Возможность или версия сохранения недоступна.');const errors=validateValue(d.data.stateSchema,state.capabilityStates?.[m.capabilityId]?.[m.id],{project,scope:m.scope});if(errors.length)throw Error('Состояние возможности повреждено: '+errors.map(e=>e.message).join(' '));}for(const d of modules.values())d.validateSavedState?.(project,state);}

// Disposal is synchronous: clear subscriptions/handles before a new session starts.
export function disposeCapabilities(session){const errors=[];for(const m of modules.values())try{const result=m.dispose?.call(session);if(result?.then){result.catch(()=>{});throw Error('Очистка модуля должна быть синхронной.');}}catch(e){errors.push({moduleId:m.id,error:e.message});}return errors;}
