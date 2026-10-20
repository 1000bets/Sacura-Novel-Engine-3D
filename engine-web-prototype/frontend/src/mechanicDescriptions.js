import {assertComponentFieldBindings} from './componentFieldBindings.js';
import {validateAuthoredDefinition} from './authoredMechanics.js';
import {isComponent} from './componentModel.js';
import {validateRules} from './ruleModel.js';
import {signalDefinition} from './signalModel.js';
import {uid} from './model.js';
import {assertSchema,assertSerializable,validateValue,schemaDefaults,referenceRecords} from './valueSchemas.js';
import {namespacedId,operationDefinition,validateOperationCall} from './operationRegistry.js';
export const MECHANIC_SCHEMA_VERSION=1;
export function ensureMechanicDescriptions(project){
 const g=project.gameplay;if(g.mechanicSchemaVersion!==undefined&&g.mechanicSchemaVersion!==1)return project;
 g.mechanicSchemaVersion??=1;g.mechanicDefinitions??=[];g.signals??=[];g.variableBindings??=[];
 // Stable identities for existing variable keys. Values stay in project.variables.
 // Rename updates key; IDs and object identities are not derived from user labels.
 for(const key of Object.keys(project.variables||{}))if(!g.variableBindings.some(x=>x.key===key)){let hash=2166136261;for(const ch of String(project.id)+'\0'+key)hash=Math.imul(hash^ch.charCodeAt(0),16777619)>>>0;let id='variable-legacy-'+hash.toString(16),suffix=2;while(g.variableBindings.some(x=>x.id===id))id='variable-legacy-'+hash.toString(16)+'-'+suffix++;g.variableBindings.push({id,key});}
 return project;
}
export function createSignalDefinition(project,{id=uid('signal').replace(/^signal-/,'user.signal.'),name,payloadSchema={type:'object',properties:{}}}){
 ensureMechanicDescriptions(project);assertSchema(payloadSchema);if(!namespacedId(id)||!name?.trim()||signalDefinition(project,id))throw Error('У сигнала нужны уникальный ID и название.');
 const signal={id,version:1,name:name.trim(),payloadSchema:structuredClone(payloadSchema)};project.gameplay.signals.push(signal);return signal;
}
export function createMechanicDefinition(project,definition){
 ensureMechanicDescriptions(project);assertSerializable(definition);
 const d={id:uid('mechanic').replace(/^mechanic-/,'user.mechanic.'),version:1,scopes:['object','scene','project'],when:'start',stateSchema:{type:'object',properties:{}},initialState:{},requirements:[],...structuredClone(definition)};
 if(!namespacedId(d.id)||!d.name?.trim()||!Number.isInteger(d.version)||d.version<1||project.gameplay.mechanicDefinitions.some(x=>x.id===d.id&&x.version===d.version))throw Error('У механики нужны уникальный ID, версия и название.');
 assertSchema(d.parameters);assertSchema(d.stateSchema);if(d.parameters.type!=='object'||d.stateSchema.type!=='object')throw Error('Опишите именованные параметры и состояние.');
 if(!d.scopes.length||d.scopes.some(x=>!['object','scene','project'].includes(x)))throw Error('Некорректное размещение механики.');
 const fn=project.functions?.find(f=>f.id===d.logic?.functionId);if(d.logic?.functionId&&!fn)throw Error('Выберите существующую функцию механики.');validateAuthoredDefinition(project,d);
 for(const name of Object.keys(d.parameters.properties))if(fn&&!fn.parameters.some(p=>p.name===name))throw Error('В функции нет параметра «'+name+'».');
 const invalid=validateValue(d.stateSchema,{...schemaDefaults(d.stateSchema),...d.initialState},{project,allowDeferred:true});if(invalid.length)throw Error(invalid.map(i=>i.message).join(' '));
 project.gameplay.mechanicDefinitions.push(d);return d;
}
export const descriptionFor=(project,id,version=1)=>project.gameplay?.mechanicDefinitions?.find(d=>d.id===id&&d.version===version);
export function addMechanicInstance(project,definitionId,scope,values={},version=1){
 ensureMechanicDescriptions(project);const d=descriptionFor(project,definitionId,version);if(!d)throw Error('Определение механики недоступно.');
 if(!d.scopes.includes(scope.kind)||scope.kind==='project'&&scope.id!==project.id||scope.kind!=='project'&&!referenceRecords(project,scope.kind).some(x=>x.id===scope.id))throw Error('Выберите подходящий объект, сцену или проект.');
 const parameters={...schemaDefaults(d.parameters),...structuredClone(values)};
 // Missing values remain editable, but malformed assigned values are rejected.
 const errors=validateValue(d.parameters,parameters,{project,scope,allowDeferred:true});if(errors.some(e=>!e.message.includes('обязательно')))throw Error(errors.map(e=>e.message).join(' '));
 const m={id:uid('mechanic'),type:'definition',definitionId,definitionVersion:version,scope:structuredClone(scope),values:parameters,...(isComponent(d)?{enabled:true,references:schemaDefaults(d.references||{type:'object',properties:{}})}:{})};if(isComponent(d)){const used=new Set(project.gameplay.mechanics.filter(x=>x.scope?.kind===scope.kind&&x.scope.id===scope.id).map(x=>x.name||descriptionFor(project,x.definitionId,x.definitionVersion)?.name));m.name=d.name;let suffix=2;while(used.has(m.name))m.name=d.name+' '+suffix++;}if(!isComponent(d)&&!d.rules&&!d.machine)m.ruleId=m.id+'-rule';
 project.gameplay.mechanics.push(m);if(m.ruleId)project.gameplay.rules.push({id:m.ruleId,name:d.name,mechanicInstanceId:m.id,trigger:d.when,sceneId:scope.kind==='scene'?scope.id:'',objectId:scope.kind==='object'?scope.id:'',once:false,enabled:true,seconds:2,repeat:false,center:[0,0,0],size:[3,2,3]});project.gameplay.enabled=true;return m;
}
export function validateMechanicDescriptions(project,{allowExpressions=false,validateExpression,checkReferences=true,isKnownSignal}={}){
 const g=project.gameplay||{},issues=[],bad=(id,message,level='error')=>issues.push({id:'mechanic-schema-'+id,level,title:'Проверьте определение механики',detail:message});
 if(g.mechanicSchemaVersion!==undefined&&g.mechanicSchemaVersion!==1){bad('version','Версия модели механик недоступна. Данные сохранены.');return issues;}
 const unique=(records,label,key=x=>x.id)=>{if(!Array.isArray(records)){bad(label,'Нужна библиотека '+label+'.');return false;}const ids=new Set();for(const r of records){if(!r?.id||ids.has(key(r)))bad(label+'-'+r?.id,'Повторяющийся или отсутствующий ID: '+label+'.');ids.add(key(r));}return true;};
 if(g.variableBindings!==undefined&&unique(g.variableBindings,'variables')){const keys=new Set();for(const x of g.variableBindings){if(keys.has(x.key)||!Object.hasOwn(project.variables||{},x.key))bad(x.id,'Привязка переменной повреждена.');keys.add(x.key);}}
 if(g.signals!==undefined&&unique(g.signals,'signals'))for(const s of g.signals)try{if(!namespacedId(s.id)||s.version!==1||!s.name)throw Error('Повреждён сигнал.');assertSchema(s.payloadSchema);}catch(e){bad(s.id,e.message);}
 if(g.mechanics!==undefined)unique(g.mechanics,'instances');
 if(g.mechanicDefinitions!==undefined&&unique(g.mechanicDefinitions,'definitions',d=>d.id+'@'+d.version))for(const d of g.mechanicDefinitions)try{
  assertSerializable(d);if(!namespacedId(d.id)||!Number.isInteger(d.version)||d.version<1||!d.name||!Array.isArray(d.scopes)||d.scopes.some(x=>!['object','scene','project'].includes(x)))throw Error('Повреждено описание механики.');
  if(typeof d.when!=='string'||!d.when||isKnownSignal&&!isKnownSignal(d.when))throw Error('Сигнал механики недоступен.');assertSchema(d.parameters);assertSchema(d.stateSchema);const fn=project.functions?.find(f=>f.id===d.logic?.functionId);if(d.logic?.functionId&&!fn)throw Error('Функция механики удалена.');const nested=validateAuthoredDefinition(project,d).map(r=>({...r,scope:{kind:'project',id:project.id}}));for(const issue of validateRules({...project,gameplay:{...g,rules:nested}}))bad(d.id,issue.detail);for(const name of Object.keys(d.parameters.properties))if(fn&&!fn.parameters?.some(p=>p.name===name))throw Error('В функции нет параметра «'+name+'».');
  for(const requirement of d.requirements||[])if(!operationDefinition(requirement.id,requirement.version))throw Error('Требуемая операция недоступна: '+requirement.id);
  for(const e of validateValue(d.stateSchema,{...schemaDefaults(d.stateSchema),...d.initialState},{project,allowDeferred:true}))bad(d.id,e.message);
 }catch(e){bad(d.id,e.message);}
 for(const m of g.mechanics||[])if(m.definitionId){const d=descriptionFor(project,m.definitionId,m.definitionVersion);if(!d){bad(m.id,'Неизвестное определение или версия: '+m.definitionId+'. Данные сохранены.');continue;}
  if(!d.scopes.includes(m.scope?.kind))bad(m.id,'Это размещение не поддерживается.');
  if(isComponent(d)){try{assertComponentFieldBindings(project,m,d);}catch(e){bad(m.id,e.message);}if(typeof m.enabled!=='boolean')bad(m.id,'У компонента нужно состояние enabled.');for(const e of validateValue(d.references||{type:'object',properties:{}},m.references||{},{project,scope:m.scope,allowDeferred:true,checkReferences}))bad(m.id,e.message);}
  for(const e of validateValue(d.parameters,{...schemaDefaults(d.parameters),...m.values},{project,scope:m.scope,allowDeferred:true,checkReferences,allowExpressions,validateExpression}))bad(m.id+'-'+e.path,e.message);
 }
 return issues;
}
// Stage 5A contract: typed internal closure, external named parameters and engine requirements.
// No heuristics over literal strings, no installers/closures in authored definitions.
export function mechanicTransferContract(project,definition){
 const internal=[],external=[],seen=new Set(),requirements=new Map((definition.requirements||[]).map(x=>[x.id+'@'+x.version,x]));
 const walkSchema=(schema,path)=>{if(schema.type==='reference')external.push({parameter:path,type:schema.referenceType});if(schema.type==='object')for(const [k,s]of Object.entries(schema.properties))walkSchema(s,path?path+'.'+k:k);if(schema.type==='collection')walkSchema(schema.items,path+'[]');};walkSchema(definition.parameters,'');
 const visit=(type,id)=>{if(!id||seen.has(type+':'+id))return;seen.add(type+':'+id);internal.push({type,id});const library=type==='event'?project.events:type==='function'?project.functions:type==='widget'?project.widgets:type==='item'?project.gameplay.items:[];const record=library?.find(r=>r.id===id);if(record)scan(record);};
 const scan=value=>{if(!value||typeof value!=='object')return;if(value.kind==='reference'&&value.mode==='internal')visit(value.type,value.id);
  if(value.kind==='operation'&&operationDefinition(value.call?.id,value.call?.version))requirements.set(value.call.id+'@'+value.call.version,{id:value.call.id,version:value.call.version});
  if(value.kind==='command'&&operationDefinition(value.command?.command,value.command?.operationVersion??1))requirements.set(value.command.command+'@'+(value.command.operationVersion??1),{id:value.command.command,version:value.command.operationVersion??1});
  if(value.type==='gameplay'&&operationDefinition(value.command,value.operationVersion??1))requirements.set(value.command+'@'+(value.operationVersion??1),{id:value.command,version:value.operationVersion??1});
  for(const [key,x]of Object.entries(value)){if(['functionId','conditionFunctionId','elseFunctionId'].includes(key)&&typeof x==='string')visit('function',x);if(['eventId','elseEventId'].includes(key)&&typeof x==='string')visit('event',x);if(['widgetId','dialogueWidgetId'].includes(key)&&typeof x==='string')visit('widget',x);if(['assetId','footstepAssetId'].includes(key)&&typeof x==='string')visit('asset',x);scan(x);}
 };scan(definition);return {schemaVersion:1,internal,external,requiredOperations:[...requirements.values()]};
}
export function validateProjectOperationCalls(project,options={}){
 const issues=[],visit=(call,kind,id)=>{for(const e of validateOperationCall(call,{project,kind,allowDeferred:true,allowExpressions:true,...options}))issues.push({id:'operation-'+id+'-'+e.path,level:'error',title:'Проверьте операцию',detail:e.message});};
 for(const e of project.events||[])for(const g of e.groups||[])for(const a of g.actions||[])if(a.type==='gameplay'&&a.command?.includes('.'))visit({id:a.command,version:a.operationVersion??1,parameters:a.value},'event',a.id);
 for(const f of project.functions||[])for(const n of f.nodes||[]){if(n.kind==='command'&&n.command?.command?.includes('.'))visit({id:n.command.command,version:n.command.operationVersion??1,parameters:n.command.value},'function',n.id);if(n.kind==='operation')visit(n.call,'function',n.id);}
 for(const r of project.gameplay?.rules||[])for(const k of ['conditionOperation','operation','elseOperation'])if(r[k]){visit(r[k],'rule',r.id+'-'+k);if(k==='conditionOperation'&&operationDefinition(r[k].id,r[k].version)?.result.type!=='boolean')issues.push({id:'operation-condition-'+r.id,level:'error',title:'Проверьте условие',detail:'Условие должно возвращать логическое значение.'});}
 for(const a of project.input?.actions||[])if(a.behavior==='operation')visit(a.operation,'event','input-'+a.id);
 for(const w of project.widgets||[])for(const e of w.elements||[])if(e.action==='gameplay'&&e.command?.includes('.'))visit({id:e.command,version:e.operationVersion??1,parameters:e.value},'widget',w.id+'-'+e.id);
 return issues;
}
