import {assertSchema,validateValue,reference,resolveReference,assertSerializable} from './valueSchemas.js';
const definitions=new Map();
export function registerSignalDefinition(definition){assertSerializable(definition);assertSchema(definition.payloadSchema);if(!definition.id||definitions.has(definition.id))throw Error('ID сигнала уже зарегистрирован.');definitions.set(definition.id,Object.freeze(structuredClone(definition)));return definition;}
export const signalDefinitions=()=>[...definitions.values()];
export const signalDefinition=(project,id)=>project.gameplay?.signals?.find(s=>s.id===id)||definitions.get(id);
const optionalString={type:'string',optional:true},number={type:'number'},string={type:'string'};
const define=(id,properties)=>registerSignalDefinition({id,version:1,payloadSchema:{type:'object',properties}});
for(const id of ['start','victory'])define(id,{});
for(const id of ['interact','death','enter','exit','tick'])define(id,{objectId:optionalString});
define('hit',{objectId:string,amount:number});define('pickup',{objectId:string,itemId:string});define('collision',{objectId:string,actorId:string});
define('timer',{objectId:optionalString,timerId:optionalString});define('button',{elementId:string,action:string});
define('input',{actionId:string,phase:string,value:{type:'union',variants:[number,{type:'boolean'},{type:'vector',size:2}]},device:optionalString});
define('sacura.variable.changed',{variableId:string,oldValue:{type:'variableValue',parameter:'variableId'},newValue:{type:'variableValue',parameter:'variableId'}});
export function signalContext(project,type,payload,context={},sceneId){
 const object=id=>id?{type:'object',id}:undefined;
 // Legacy subject fields are adapted here. Absence stays absent, never playerId.
 const legacySource=type==='pickup'?object(payload.objectId):type==='collision'?object(payload.actorId):undefined;
 const legacyTarget=['interact','hit','death','collision','enter','exit'].includes(type)?object(payload.objectId):undefined;
 const lineage=Object.fromEntries(['bindings','rootId','causeChain','nativeRunId','reactionId','mechanicInstanceId'].filter(k=>context[k]!==undefined).map(k=>[k,structuredClone(context[k])]));
 const result={...lineage,source:context.source||object(context.sourceId)||legacySource,target:context.target||object(context.targetId)||legacyTarget,initiator:context.initiator||object(context.initiatorId),sceneId:context.sceneId||sceneId};
 for(const name of ['source','target','initiator'])if(result[name])resolveReference(reference(result[name].type,result[name].id),result[name].type,{project});
 if(result.sceneId&&!project.subscenes.some(s=>s.id===result.sceneId))throw Error('Сцена сигнала недоступна.');
 if(result.mechanicInstanceId&&!project.gameplay.mechanics.some(m=>m.id===result.mechanicInstanceId))throw Error('Экземпляр сигнала удалён.');
 return structuredClone(Object.fromEntries(Object.entries(result).filter(([,value])=>value!==undefined)));
}
export function validateSignalPayload(project,type,payload){const d=signalDefinition(project,type);if(!d)throw Error('Определение сигнала недоступно: '+type);const errors=validateValue(d.payloadSchema,payload,{project,parameters:payload,allowResolvedReferences:true});if(errors.length)throw Error(errors.map(e=>e.message).join(' '));return d;}
