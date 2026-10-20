import {assertBehaviorTree} from './behaviorTrees.js';
import {componentError} from './componentDiagnostics.js';
import {uid} from './model.js';
import {createMechanicDefinition,descriptionFor} from './mechanicDescriptions.js';
import {compileComponentSource,parseComponentSource,sourceFingerprint,assertPublishedSource} from './componentSources.js';
import {componentSignalName,componentFunctionOwner} from './componentCatalog.js';
import {assertSerializable,schemaDefaults} from './valueSchemas.js';
import {assertComponentDefinition} from './componentModel.js';

export const logicResource=(project,id)=>project.logicResources?.find(r=>r.id===id);
export const emptyComponentSource=name=>'const component = {\n  name: '+JSON.stringify(name)+',\n  parameters: { type: "object", properties: {} },\n  state: { type: "object", properties: {} },\n  methods: {}\n};\n';
export function createComponentResource(project,{name='Новый компонент',kind='component.code',path}={}){
 if(!['component.code','component.graph','behavior.tree'].includes(kind)||!name.trim())throw Error('Выберите вид и название ресурса.');
 project.logicResources??=[];const id=uid('logic'),definitionId='user.component.'+uid('type').toLowerCase(),extension=kind==='component.code'?'.js':kind==='behavior.tree'?'.bt':'.graph';
 const stem=(name.trim().replace(/[\\/]/g,'-')||'Component'),base=path||'Компоненты/'+stem+extension;let filePath=base,n=2;while(project.logicResources.some(r=>r.path===filePath))filePath=base.replace(/(\.[^/.]+)?$/, ' '+n+++'$1');
 const resource={id,version:1,kind,name:name.trim(),path:filePath,definitionId,status:'ready',approved:true};
 if(kind==='component.code'){resource.source=emptyComponentSource(name.trim());resource.fingerprint=sourceFingerprint(resource.source);resource.dependencies={};resource.signalBindings={};}
 if(kind==='behavior.tree'){resource.root=uid('bt');resource.nodes=[{id:resource.root,kind:'selector',name:'Выбор поведения',children:[],position:{x:80,y:60}}];}
 project.logicResources.push(resource);
 const definition=createMechanicDefinition(project,{id:definitionId,name:name.trim(),componentVersion:1,stateVersion:1,parameters:{type:'object',properties:{}},stateSchema:{type:'object',properties:{}},references:{type:'object',properties:{}},methods:{},events:{},logicResourceId:id,...(kind==='behavior.tree'?{behaviorTree:{treeResourceId:id,interval:.1,autoStart:false}}:{})});
 if(kind==='component.graph')addGraphComponentMethod(project,definition.id,{name:'Start'});
 if(kind==='behavior.tree')for(const [key,name]of [['start','Запустить поведение'],['stop','Остановить поведение']]){const method=addGraphComponentMethod(project,definition.id,{methodId:key,name}),f=project.functions.find(f=>f.id===definition.methods[method].logic.functionId),node={id:uid('bt-control'),kind:'operation',call:{id:key==='start'?'sacura.behavior-tree.run':'sacura.behavior-tree.stop',version:1,parameters:{instance:{op:'get',path:'self'}}},next:f.entry};f.nodes.unshift(node);f.entry=node.id;}

 return resource;
}
export function editComponentSource(project,id,source){const r=logicResource(project,id);if(r?.kind!=='component.code')throw Error('Исходник не найден.');r.source=source;r.status='draft';r.approved=false;delete r.error;}
export function publishComponentSource(project,id){
 const original=logicResource(project,id);if(!original)throw Error('Ресурс компонента удалён.');
 try{
  const compiled=compileComponentSource(project,original),next=structuredClone(project),r=logicResource(next,id),d=next.gameplay.mechanicDefinitions.find(d=>d.id===r.definitionId&&d.logicResourceId===r.id),declaration=compiled.declaration,oldMethods=d.methods;
  let displayName=declaration.name,indexName=2;while(next.gameplay.mechanicDefinitions.some(other=>other.id!==d.id&&other.name===displayName)||next.logicResources.some(other=>other.id!==r.id&&other.name===displayName))displayName=declaration.name+' ('+indexName+++')';
  const events={};r.signalBindings??={};
  for(const [key,event]of Object.entries(declaration.signals)){
   const signalId=r.signalBindings[key]?.signalId||componentSignalName(id,key);r.signalBindings[key]={signalId};
   const record={id:signalId,version:1,name:declaration.name+' · '+(event.name||key),payloadSchema:event.payloadSchema};
   const index=next.gameplay.signals.findIndex(s=>s.id===signalId);if(index<0)next.gameplay.signals.push(record);else next.gameplay.signals[index]=record;
   events[key]={name:event.name||key,signalId,payloadSchema:event.payloadSchema};
  }
  const methods={};
  for(const [methodId,method]of Object.entries(declaration.methods)){
   const functionId=oldMethods[methodId]?.logic.functionId||uid('method'),index=next.functions.findIndex(f=>f.id===functionId);
   const fn={id:functionId,name:declaration.name+' · '+method.name,graphVersion:1,logicResourceId:id,componentMethodId:methodId,parameters:Object.entries(method.parameters.properties).map(([name,schema])=>({name,schema})),resultSchema:method.result,locals:{},entry:'',nodes:[]};
   if(index<0)next.functions.push(fn);else next.functions[index]=fn;
   methods[methodId]={...method,logic:{functionId}};
  }
  const removed=Object.entries(oldMethods).filter(([key])=>!Object.hasOwn(methods,key)).map(([,m])=>m.logic.functionId);next.functions=next.functions.filter(f=>!removed.includes(f.id));
  Object.assign(d,{name:displayName,parameters:declaration.parameters,stateSchema:declaration.stateSchema,stateVersion:declaration.stateVersion,initialState:declaration.initialState,stateFromParameters:declaration.stateFromParameters,references:declaration.references,lifecycle:declaration.lifecycle,methods,events,subscriptions:declaration.subscriptions.map(s=>{const {signal,...rest}=s;return {...rest,signalId:events[signal]?.signalId||compiled.dependencies.signals?.[signal]?.signalId||signal||s.signalId};})});
  Object.assign(r,{name:displayName,status:'ready',approved:true,fingerprint:compiled.fingerprint,dependencies:compiled.dependencies,locations:declaration.locations});delete r.error;
  assertComponentDefinition(next,d);assertSerializable(r);Object.assign(project,next);return logicResource(project,id);
 }catch(error){original.status='error';original.approved=false;original.error={message:error.message,line:error.line||1,column:error.column||1};throw error;}
}
export function addGraphComponentMethod(project,definitionId,{name='Новый метод',methodId,definitionVersion=1,parameters={type:'object',properties:{}},result={type:'void'}}={}){
 const d=descriptionFor(project,definitionId,definitionVersion),r=logicResource(project,d?.logicResourceId);if(!r||!['component.graph','behavior.tree'].includes(r.kind))throw Error('Методы добавляются в графовый компонент.');
 const key=methodId||uid('method'),functionId=uid('function');if(Object.hasOwn(d.methods,key))throw Error('ID метода уже используется.');
 project.functions.push({id:functionId,name,graphVersion:1,parameters:Object.entries(parameters.properties).map(([name,schema])=>({name,schema})),resultSchema:result,stateSchema:structuredClone(d.stateSchema),initialState:structuredClone(d.initialState),locals:{},entry:functionId+'-return',nodes:[{id:functionId+'-return',kind:'return',...(result.type==='void'?{}:{value:schemaDefaults(result)??(result.type==='boolean'?false:result.type==='string'?'':0)})}],componentDefinitionId:d.id});
 d.methods[key]={name,parameters,result,logic:{functionId}};return key;
}
export function removeGraphComponentMethod(project,definitionId,methodId,definitionVersion=1){const d=descriptionFor(project,definitionId,definitionVersion),method=d?.methods[methodId];if(!method)throw Error('Метод удалён.');delete d.methods[methodId];project.functions=project.functions.filter(f=>f.id!==method.logic.functionId);}
export function assertComponentResources(project){
 for(const m of project.gameplay?.mechanics||[]){const d=descriptionFor(project,m.definitionId,m.definitionVersion);if(!d?.logicResourceId)continue;try{const r=logicResource(project,d.logicResourceId);if(!r)throw Error('Исходный ресурс компонента недоступен: '+d.name);if(r.kind==='component.code')assertPublishedSource(r);if(r.kind==='behavior.tree')assertBehaviorTree(project,r);}catch(error){throw componentError(error,project,m.id,'');}}
}
export function validateLogicResources(project){
 if(project.logicResources===undefined)return;
 if(!Array.isArray(project.logicResources))throw Error('Библиотека исходных ресурсов повреждена.');
 const ids=new Set();for(const r of project.logicResources){
  if(!r?.id||ids.has(r.id)||r.version!==1||!['component.code','component.graph','behavior.tree'].includes(r.kind)||!r.path||r.path.split(/[\\/]/).includes('..'))throw Error('Проверьте ID, версию и путь ресурса логики.');ids.add(r.id);
  const d=project.gameplay.mechanicDefinitions.find(d=>d.id===r.definitionId&&d.logicResourceId===r.id);if(!d||d.logicResourceId!==r.id)throw Error('Определение исходного ресурса недоступно.');
  if(r.kind==='component.code'){
   let declaration;try{declaration=compileComponentSource(project,r).declaration;}catch(error){throw Object.assign(Error(r.path+': '+error.message),{line:error.line,column:error.column});}
   if(JSON.stringify(Object.keys(declaration.methods).sort())!==JSON.stringify(Object.keys(d.methods).sort()))throw Error('Индекс методов не соответствует исходнику компонента. Сохраните компонент.');
   for(const field of ['parameters','stateSchema','stateVersion','initialState','stateFromParameters','references','lifecycle'])if(JSON.stringify(declaration[field])!==JSON.stringify(d[field]))throw Error(r.path+': Индекс '+field+' не соответствует исходнику. Сохраните компонент.');
   if(JSON.stringify(Object.keys(declaration.signals).sort())!==JSON.stringify(Object.keys(d.events||{}).sort()))throw Error(r.path+': Индекс сигналов не соответствует исходнику.');for(const [key,event]of Object.entries(declaration.signals))if(JSON.stringify(event.payloadSchema)!==JSON.stringify(d.events[key]?.payloadSchema))throw Error(r.path+': Схема сигнала '+key+' не соответствует исходнику.');
   for(const [key,method]of Object.entries(d.methods)){if(JSON.stringify(method.parameters)!==JSON.stringify(declaration.methods[key].parameters)||JSON.stringify(method.result)!==JSON.stringify(declaration.methods[key].result))throw Error(r.path+': Сигнатура метода '+key+' не соответствует исходнику.');const fn=project.functions.find(f=>f.id===method.logic.functionId);if(fn?.logicResourceId!==r.id||fn.componentMethodId!==key)throw Error('Метод не связан со своим исходным ресурсом.');}
  }
  if(r.kind==='behavior.tree')assertBehaviorTree(project,r);
 }
}
