// Transport adapters for native typed references. No evaluator, code loading or runtime state.
import {referenceRecords,variableKey,assertSchema,assertSerializable,validateValue,schemaDefaults} from './valueSchemas.js';
import {validateFunctions} from './blueprintFunctions.js';
import {namespacedId} from './operationRegistry.js';
import {parameterSchema} from './authoredMechanics.js';
import {operationDefinition,validateOperationCall} from './operationRegistry.js';
import {validateExpression} from './expressionValidation.js';
import {validateAuthoredDefinition} from './authoredMechanics.js';
import {validateRules} from './ruleModel.js';
import {capabilityDefinitions} from './capabilityRegistry.js';

export const PACKAGE_FORMAT_VERSION=2;
export const PACKAGE_LIMITS=Object.freeze({bytes:150_000_000,assets:128,assetBytes:100*1024*1024,records:4096,depth:64,values:100000});
export const packageContentFields=['version','visualExamplesVersion','sceneEditingVersion','standardEventsVersion','events','functions','actionTemplates','groupTemplates','widgets','audioAssets','gameplay','variables','variableTypes','objects','subscenes','chapters','input','ui','audioSettings','timelineLayout','storyLayout','variableLayout','files','folders','description','objectGroups','assetFiles','assetFolders','assetOverrides','graphLayout'];
export function visitData(value,fn,path=''){
 if(!value||typeof value!=='object')return;
 fn(value,path);
 for(const [key,child] of Object.entries(value))visitData(child,fn,path+'/'+key.replace(/~/g,'~0').replace(/\//g,'~1'));
}
export function visitTypedReferences(value,fn){visitData(value,(v,path)=>{if(v.kind==='reference')fn(v,path);});}
export function inputRecords(project,input){
 const type=input.type==='story'?'beat':input.type;
 if(type==='variable'&&!input.nativeReference)return Object.keys(project.variables||{}).filter(key=>!input.valueType||(project.variableTypes?.[key]||typeof project.variables[key])===input.valueType).map(id=>({id,name:id}));
 const records=referenceRecords(project,type).filter(r=>type!=='variable'||!input.valueType||(project.variableTypes?.[variableKey(project,r.id)]||typeof project.variables[variableKey(project,r.id)])===input.valueType),names=new Map(),counts=new Map();
 for(const r of records){const name=r.name||r.text||type;counts.set(name,(counts.get(name)||0)+1);}
 return records.map(r=>{const name=r.name||r.text||type,n=(names.get(name)||0)+1;names.set(name,n);const scene=type==='object'&&project.subscenes?.find(s=>s.id===r.subsceneId);return {...r,name:counts.get(name)>1?name+' ('+n+')'+(scene?' · '+scene.name:''):name};});
}
export function typedInputInfo(project,ref){
 const record=referenceRecords(project,ref.type).find(r=>r.id===ref.id);
 if(!record)throw Error('Ссылка заготовки недоступна: '+ref.type+'.');
 return {name:record.name||record.key||record.text||ref.type,type:ref.type,nativeReference:true,schema:{type:'reference',referenceType:ref.type},...(ref.type==='variable'?{valueType:project.variableTypes?.[variableKey(project,ref.id)]||typeof project.variables[variableKey(project,ref.id)]}:{})};
}
export function remapTypedReferences(content,ids){visitTypedReferences(content,r=>{if(['direct','internal'].includes(r.mode||'direct')&&ids.has(r.id))r.id=ids.get(r.id);});}
export function supportedEngineModules(){return capabilityDefinitions().map(({id,version})=>({id,version}));}
// Legacy signal callbacks receive a native object ID in args.objectId. Only this
// documented call site establishes that a comparison operand is an object link.
// Adapt the export copy to ordinary editable function parameters, never all strings.
export function parameterizeLegacyCallback(project,fn){
 const subject=fn.parameters?.find(p=>p.name==='objectId'&&p.type==='string');
 if(!subject||!project.gameplay.rules.some(r=>([r.functionId,r.conditionFunctionId].includes(fn.id)||(r.actions||[]).some(a=>a.kind==='function'&&a.functionId===fn.id))))return;
 const slots=new Map();
 visitData(fn.nodes,e=>{
  if(!['eq','ne'].includes(e.op))return;
  for(const [literal,argument]of [['a','b'],['b','a']]){
   if(e[argument]?.op!=='get'||e[argument].path!=='args.objectId'||typeof e[literal]!=='string')continue;
   const object=project.objects.find(o=>o.id===e[literal]);if(!object)continue;
   let key=slots.get(object.id);if(!key){key='comparedObject';let n=2;while(fn.parameters.some(p=>p.name===key))key='comparedObject'+n++;slots.set(object.id,key);fn.parameters.push({name:key,label:object.name,type:'string',referenceType:'object',default:object.id});}
   e[literal]={op:'get',path:'args.'+key};subject.referenceType='object';subject.label??='Объект сигнала';
  }
 });
}
export function validatePublicParameters(parameters,kind){
 if(kind!=='function'){assertSchema(parameters);return;}
 if(!Array.isArray(parameters)||new Set(parameters.map(p=>p.name)).size!==parameters.length)throw Error('Повреждены публичные параметры функции.');
 for(const p of parameters){if(typeof p.name!=='string'||!p.name)throw Error('Параметру нужно имя.');assertSchema(parameterSchema(p));}
}
export function validateTypedContent(content,references=[],inputs=[]){
 content=structuredClone(content);
 const at=path=>{const parts=path.split('/').slice(2).map(s=>s.replace(/~1/g,'/').replace(/~0/g,'~'));const key=parts.pop();return {parent:parts.reduce((v,k)=>v[k],content),key};};
 const subjects={object:'objects',scene:'subscenes'};
 for(const r of references){if(r.typed||r.key)continue;const input=inputs.find(i=>i.id===r.inputId),{parent,key}=at(r.at),id='package-slot-'+input.id;if(r.variablePath)parent[key]=[...(r.widgetPath?[]:['global']),id,...r.memberPath].join('.');else parent[key]=id;
  if(subjects[input.type]){const records=content[subjects[input.type]]??=[];if(!records.some(x=>x.id===id))records.push({id,name:input.name,active:true});}
  // Validation-only representative of a required destination slot. Never serialized or executed.
  if(input.type==='story'){const chapters=content.chapters??=[];if(!chapters.flatMap(c=>c.beats||[]).some(b=>b.id===id))chapters.push({id:'slot-chapter-'+input.id,beats:[{id,kind:'dialogue',text:'',bindings:[]}]});}
 }
 visitData(content,v=>{if(v.kind==='project'&&v.id===null)v.id='package-project';});
 const project={...content,id:'package-project',objects:content.objects||[],subscenes:content.subscenes||[],chapters:content.chapters||[],variables:content.variables||{},events:content.events||[],functions:content.functions||[],widgets:content.widgets||[],gameplay:content.gameplay||{},input:content.input||{actions:[],contexts:[]}};
 const context={project,allowDeferred:true,checkReferences:false,allowExpressions:true,validateExpression};
 for(const f of project.functions){
  for(const p of f.parameters||[]){const s=parameterSchema(p);assertSchema(s);if(s.default!==undefined){const errors=validateValue(s,s.default,context);if(errors.length)throw Error(errors.map(e=>e.message).join(' '));}}
  if(f.localSchema){assertSchema(f.localSchema);const errors=validateValue(f.localSchema,f.locals,context);if(errors.length)throw Error(errors.map(e=>e.message).join(' '));}
  if(f.resultSchema)assertSchema(f.resultSchema);
 }
 for(const d of project.gameplay.mechanicDefinitions||[]){
  assertSerializable(d);if(!namespacedId(d.id)||!Number.isInteger(d.version)||d.version<1||!d.name?.trim()||!Array.isArray(d.scopes)||!d.scopes.length||d.scopes.some(s=>!['object','scene','project'].includes(s)))throw Error('Повреждено определение механики.');assertSchema(d.parameters);assertSchema(d.stateSchema);const rules=validateAuthoredDefinition(project,d).map(r=>({...r,scope:{kind:'project',id:project.id}}));const issues=validateRules({...project,gameplay:{...project.gameplay,rules}});if(issues.length)throw Error(issues.map(i=>i.detail).join(' '));
  if(d.logic?.functionId&&!project.functions.some(f=>f.id===d.logic.functionId))throw Error('Функция определения отсутствует.');
  const errors=validateValue(d.stateSchema,{...schemaDefaults(d.stateSchema),...d.initialState},context);if(errors.length)throw Error(errors.map(e=>e.message).join(' '));
 }
 const functionIssues=validateFunctions(project);if(functionIssues.length)throw Error(functionIssues.map(i=>i.detail).join(' '));
 const issues=validateRules(project);if(issues.length)throw Error(issues.map(i=>i.detail).join(' '));
 visitData(content,v=>{
  if(v.op)validateExpression(v);
  const call=v.kind==='operation'?v.call:v.behavior==='operation'?v.operation:undefined;
  if(call){const errors=validateOperationCall(call,{...context,kind:v.behavior?'event':'rule'});if(errors.length)throw Error(errors.map(e=>e.message).join(' '));}
  if(v.conditionOperation){if(operationDefinition(v.conditionOperation.id,v.conditionOperation.version)?.result.type!=='boolean')throw Error('Условие должно возвращать логическое значение.');const errors=validateOperationCall(v.conditionOperation,{...context,kind:'rule'});if(errors.length)throw Error(errors.map(e=>e.message).join(' '));}
  if(v.kind==='function'){const f=project.functions.find(f=>f.id===v.functionId);for(const p of f?.parameters||[]){const s=parameterSchema(p),errors=validateValue(s,v.args?.[p.name]??schemaDefaults(s),context);if(errors.length)throw Error(errors.map(e=>e.message).join(' '));}}
 });
}
