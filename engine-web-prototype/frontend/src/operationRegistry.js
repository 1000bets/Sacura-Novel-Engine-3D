import {validateExpression} from './expressionValidation.js';
import {assertSerializable,assertSchema,validateValue,schemaDefaults,materializeValue} from './valueSchemas.js';
export const OPERATION_SCHEMA_VERSION=1;
export const namespacedId=id=>typeof id==='string'&&/^[a-z][a-z0-9-]*(?:\.[a-z][a-z0-9-]*)+(?:\.[a-z0-9-]+)*$/.test(id);
const definitions=new Map(),executors=new Map(),validators=new Map(),listeners=new Set();let revision=0;
const key=(id,version)=>id+'@'+version;
const freeze=value=>{if(value&&typeof value==='object'){Object.values(value).forEach(freeze);Object.freeze(value);}return value;};
export function registerOperation(definition,execute,validate=()=>[]){
 assertSerializable(definition);if(definition.schemaVersion!==undefined&&definition.schemaVersion!==OPERATION_SCHEMA_VERSION)throw Error('Версия схемы операции недоступна.');
 if(!namespacedId(definition.id)||!Number.isInteger(definition.version)||definition.version<1||(typeof execute!=='function'||typeof validate!=='function'))throw Error('Операции нужны namespaced ID, версия и исполнитель.');
 if(definitions.has(key(definition.id,definition.version)))throw Error('Операция с этим ID и версией уже зарегистрирована.');
 if(!definition.contexts?.length||definition.contexts.some(c=>!['event','function','rule','mechanic','widget'].includes(c))||!definition.editor?.label)throw Error('Укажите контексты и название операции.');
 assertSchema(definition.parameters);assertSchema(definition.result);if(definition.parameters.type!=='object')throw Error('Параметры операции должны иметь именованные поля.');
 const defaults=schemaDefaults(definition.parameters),issues=validateValue(definition.parameters,defaults,{allowDeferred:true,checkReferences:false});
 // Required fields can be unassigned until the author configures them.
 if(issues.some(i=>!i.message.includes('обязательно')&&!i.message.includes('сначала выберите')))throw Error('Некорректные значения по умолчанию: '+issues.map(i=>i.message).join(' '));
 const value=freeze(structuredClone({...definition,schemaVersion:OPERATION_SCHEMA_VERSION}));definitions.set(key(value.id,value.version),value);executors.set(key(value.id,value.version),execute);validators.set(key(value.id,value.version),validate);revision++;listeners.forEach(fn=>fn());return value;
}
export const operationDefinition=(id,version=1)=>definitions.get(key(id,version));
export const operationDefinitions=()=>[...definitions.values()];
export const subscribeOperations=fn=>{listeners.add(fn);return()=>listeners.delete(fn);};
export const operationRevision=()=>revision;
export function validateOperationCall(call,context={}){
 const d=operationDefinition(call?.id,call?.version);if(!d)return [{path:'id',message:'Операция или её версия недоступна: '+call?.id,unknown:true}];
 if(context.kind&&!d.contexts.includes(context.kind))return [{path:'id',message:'Операция недоступна в этом контексте.'}];
 const values={...schemaDefaults(d.parameters),...call.parameters};
 const issues=validateValue(d.parameters,values,{validateExpression,...context,parameters:values,allowExpressions:!!context.allowExpressions});if(!issues.length)issues.push(...validators.get(key(d.id,d.version))(values,context));return issues;
}
function prepareOperation(call,context){
 const d=operationDefinition(call?.id,call?.version),parameters={...schemaDefaults(d?.parameters||{type:'object',properties:{}}),...call?.parameters};
 context.assertActive?.();const issues=validateOperationCall({...call,parameters},{...context,allowExpressions:true});if(issues.length)throw Error(issues.map(i=>i.message).join(' '));
 const evaluate=value=>value?.op?context.evaluate(value):Array.isArray(value)?value.map(evaluate):value&&typeof value==='object'?Object.fromEntries(Object.entries(value).map(([k,v])=>[k,evaluate(v)])):value;
 const evaluated=evaluate(parameters),after=validateOperationCall({...call,parameters:evaluated},{...context,allowExpressions:false});if(after.length)throw Error(after.map(i=>i.message).join(' '));
 return {d,values:materializeValue(d.parameters,evaluated,{...context,parameters:evaluated})};
}
function finishOperation(d,values,result,context){
 context.assertActive?.();const invalid=validateValue(d.result,result,{...context,parameters:values});if(invalid.length)throw Error('Результат операции: '+invalid.map(i=>i.message).join(' '));return result;
}
export function executeOperationAuto(call,context){
 const {d,values}=prepareOperation(call,context),result=executors.get(key(d.id,d.version))(values,context);
 return result&&typeof result.then==='function'?Promise.resolve(result).then(value=>finishOperation(d,values,value,context)):finishOperation(d,values,result,context);
}
export async function executeOperation(call,context){return executeOperationAuto(call,context);}
