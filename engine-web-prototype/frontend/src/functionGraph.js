import {signalDefinition} from './signalModel.js';
import {componentMethodTarget,componentFunctionOwner,assertComponentMethodReceiver} from './componentCatalog.js';
import {operationDefinition} from './operationRegistry.js';
import {schemaFromValue,validateValue,dependentSchema} from './valueSchemas.js';
import {graphGridPositions,graphNodeSize} from './graphNodeLayout.js';

export const FUNCTION_GRAPH_VERSION=1;
const any={type:'any'},number={type:'number'},boolean={type:'boolean'},workHandle={type:'object',properties:{kind:{type:'string'},id:{type:'string'}}};
const safe=k=>!['__proto__','prototype','constructor'].includes(k);
export const VALUE_OPERATIONS={constant:'Значение',get:'Прочитать',itemCount:'Количество предметов',array:'Коллекция',struct:'Структура',field:'Поле структуры',at:'Элемент коллекции',length:'Длина',and:'И',or:'Или',not:'Не',eq:'Равно',ne:'Не равно',add:'Сложить',sub:'Вычесть',mul:'Умножить',div:'Разделить',min:'Минимум',max:'Максимум',lt:'Меньше',lte:'Не больше',gt:'Больше',gte:'Не меньше'};
const names={set:'Записать значение',branch:'Условие',array:'Изменить коллекцию',call:'Вызвать функцию',event:'Запустить Event',command:'Операция',operation:'Операция',return:'Результат',wait:'Ожидать время',waitSignal:'Ожидать сигнал',signal:'Отправить сигнал',parallel:'Параллельно',timeout:'Таймаут',start:'Запустить работу',await:'Дождаться работы',cancel:'Остановить работу',btSequence:'Последовательность поведения',btSelector:'Выбор поведения'};
const pathParts=path=>Array.isArray(path)?path:String(path||'').split('.').filter(Boolean);
export function graphInputValue(node,port){return pathParts(port.path||port).reduce((value,k)=>value?.[k],node);}
export function graphSetInput(node,port,value){const path=pathParts(port.path||port);if(!path.length||path.some(k=>!safe(k)))throw Error('Недопустимый вход узла.');let target=node;for(const k of path.slice(0,-1))target=target[k]??={};target[path.at(-1)]=structuredClone(value);return node;}
export function functionPathSchema(project,fn,path){const [root,key,...rest]=String(path||'').split('.');let schema;const definition=componentFunctionOwner(project,fn.id)?.definition||project.gameplay?.mechanicDefinitions?.find(d=>d.id===fn.componentDefinitionId);if(root==='self')return {type:'reference',referenceType:'mechanic'};if(root==='params'||root==='refs')schema=definition?.[root==='params'?'parameters':'references']?.properties?.[key];else if(root==='args')schema=fn.parameters?.find(p=>p.name===key)?.schema||((p=>p?schemaFromValue(p.default,p.type):any)(fn.parameters?.find(p=>p.name===key)));else if(root==='global')schema=schemaFromValue(project.variables?.[key],project.variableTypes?.[key]);else schema=(root==='state'?definition?.stateSchema||fn.stateSchema:fn.localSchema)?.properties?.[key]||schemaFromValue(fn.locals?.[key]);for(const k of rest)schema=schema?.type==='collection'?schema.items:schema?.properties?.[k];return schema?.type&&schema.type!=='undefined'?schema:any;}
const input=(path,label,schema=any,extra={})=>({name:path.join('.'),label,path,schema,type:schema.type,...extra});
const output=(name,label,schema=any,extra={})=>({name,label,schema,type:schema.type,...extra});
export function graphValueSchema(project,fn,value,seen=new Set()){
 if(value?.op==='node')return valueSchema(project,fn,fn.nodes.find(n=>n.id===value.nodeId),new Set(seen));
 if(value?.op==='get')return functionPathSchema(project,fn,value.path);
 if(value?.op)return valueSchema(project,fn,{kind:'value',value},new Set(seen));
 return Number.isInteger(value)?{type:'integer'}:schemaFromValue(value);
}
export function graphStructureFields(project,fn,value){
 const fields=[],walk=(schema,path=[],labels=[])=>{for(const [key,child]of Object.entries(schema?.properties||{})){const next=[...path,key],names=[...labels,child.label||key];fields.push({path:next.join('.'),label:names.join(' · '),schema:child});if(next.length<16&&child.type==='object')walk(child,next,names);}};
 walk(graphValueSchema(project,fn,value));return fields;
}
function valueSchema(project,fn,node,seen=new Set()){
 if(!node||node.id&&seen.has(node.id))return any;if(node.id)seen.add(node.id);
 if(node.kind==='start')return workHandle;
 if(['btSequence','btSelector'].includes(node.kind))return {type:'string',enum:['Running','Success','Failure']};
 if(node.kind==='call')return project.functions?.find(f=>f.id===node.functionId)?.resultSchema||any;
 if(node.kind==='operation'||node.kind==='command'){
  if(node.componentMethod)return componentMethodTarget(project,node).method?.result||any;
  const schema=operationDefinition(node.call?.id||node.command?.command,node.call?.version??node.command?.operationVersion)?.result||any;
  if(schema.type==='componentResult')try{return dependentSchema(schema,{project,parameters:node.call?.parameters||node.command?.value,allowDeferred:true,allowExpressions:true,checkReferences:false})||any;}catch{return any;}
  return schema;
 }
 const e=node.value;if(node.kind!=='value')return any;
 if(['and','or','not','eq','ne','lt','lte','gt','gte'].includes(e?.op))return boolean;
 if(['length','itemCount'].includes(e?.op))return {type:'integer'};
 if(e?.op==='get')return functionPathSchema(project,fn,e.path);
 if(e?.op==='constant')return node.schema||(Number.isInteger(e.value)?{type:'integer'}:schemaFromValue(e.value));
 const schemaOf=value=>graphValueSchema(project,fn,value,new Set(seen));
 if(['add','sub','mul','div','min','max'].includes(e?.op))return e.op!=='div'&&schemaOf(e.a).type==='integer'&&schemaOf(e.b).type==='integer'?{type:'integer'}:number;
 if(e?.op==='array')return {type:'collection',items:e.values?.length?schemaOf(e.values[0]):any};
 if(e?.op==='struct')return {type:'object',properties:Object.fromEntries(Object.entries(e.fields||{}).map(([k,v])=>[k,schemaOf(v)]))};
 if(e?.op==='at')return schemaOf(e.a)?.items||any;
 if(e?.op==='field')return String(e.field||'').split('.').reduce((s,k)=>s?.properties?.[k],schemaOf(e.a))||any;
 return any;
}
export function nodeDescription(project,fn,node){
 const inputs=[],outputs=[],add=(path,label,schema=any,extra={})=>inputs.push(input(path,label,schema,extra));
 const exec=node.kind!=='value';if(exec)inputs.push(input([], 'Выполнить',{type:'exec'},{name:'exec',control:true}));
 if(node.kind==='value'){
  const e=node.value||{};if(e.op==='constant')add(['value','value'],'Значение',node.schema||any,{literalOnly:true});
  else if(['field','length','not'].includes(e.op))add(['value','a'],e.op==='not'?'Условие':'Значение',e.op==='not'?boolean:any);
  else if(e.op==='itemCount'){add(['value','itemId'],'Предмет',{type:'union',variants:[{type:'reference',referenceType:'item'},{type:'string'}]});add(['value','owner'],'Владелец',{type:'union',variants:[{type:'reference',referenceType:'object'},{type:'string'}],optional:true});}
  else if(e.op==='array')(e.values||[]).forEach((_,i)=>add(['value','values',String(i)],'Элемент '+(i+1)));
  else if(e.op==='struct')Object.keys(e.fields||{}).forEach(k=>add(['value','fields',k],k));
  else if(e.op!=='get'){const s=['and','or'].includes(e.op)?boolean:['eq','ne','at'].includes(e.op)?any:number;add(['value','a'],'A',s);add(['value','b'],'B',e.op==='at'?number:s);}
 }else if(node.kind==='set'||node.kind==='return')add(['value'],node.kind==='return'?'Результат':'Значение',node.kind==='return'?fn.resultSchema||any:functionPathSchema(project,fn,node.path));
 else if(node.kind==='branch')add(['condition'],'Условие',boolean);
 else if(node.kind==='array'){if(node.operation!=='remove')add(['value'],'Элемент',functionPathSchema(project,fn,node.path)?.items||any);if(node.operation!=='push')add(['index'],'Индекс',number);}
 else if(node.kind==='call'||node.kind==='start'){const target=project.functions?.find(f=>f.id===node.functionId);for(const p of target?.parameters||[])add(['args',p.name],p.label||p.name,p.schema||{...schemaFromValue(p.default,p.type),...(p.default!==undefined?{default:p.default}:{})});}
 else if(node.kind==='operation'&&node.call?.id==='sacura.component.call'&&node.componentMethod){const target=componentMethodTarget(project,node);add(['call','parameters','instance'],'Получатель',{type:'reference',referenceType:'mechanic',componentDefinitionId:node.componentMethod.definitionId,componentDefinitionVersion:node.componentMethod.definitionVersion});for(const [key,schema]of Object.entries(target.method?.parameters.properties||{}))add(['call','parameters','args',key],schema.label||key,schema);}
 else if(node.kind==='operation'||node.kind==='command'){const d=operationDefinition(node.call?.id||node.command?.command,node.call?.version??node.command?.operationVersion??1);for(const [k,s]of Object.entries(d?.parameters.properties||{})){let schema=s;if(['variableValue','signalPayload','componentArguments','componentResult'].includes(s.type)){try{schema=dependentSchema(s,{project,parameters:node.call?.parameters||node.command?.value,allowDeferred:true,allowExpressions:true,checkReferences:false})||any;}catch{schema=any;}}add(node.kind==='operation'?['call','parameters',k]:['command','value',k],s.label||k,schema);}}
 else if(['wait','timeout'].includes(node.kind))add(['duration'],'Секунды',number);
 else if(node.kind==='signal')add(['payload'],'Данные сигнала',signalDefinition(project,node.signalId)?.payloadSchema||any);
 else if(node.kind==='waitSignal')add(['duration'],'Таймаут, секунды',{type:'number',optional:true});
 else if(['await','cancel'].includes(node.kind))add(['handle'],'Работа',workHandle);
 if(node.kind==='branch'){outputs.push(output('then','Да',{type:'exec'},{control:true,path:['trueNext']}),output('else','Нет',{type:'exec'},{control:true,path:['falseNext']}));}
 else if(exec&&node.kind!=='return')outputs.push(output('next','Дальше',{type:'exec'},{control:true,path:['next']}));
 if(node.kind==='parallel')(node.branches||[]).forEach((_,i)=>outputs.push(output('branch-'+i,'Ветка '+(i+1),{type:'exec'},{control:true,path:['branches',String(i)]})));
 if(node.kind==='timeout')outputs.push(output('body','Работа',{type:'exec'},{control:true,path:['body']}));
 if(['value','call','command','operation','start','await','waitSignal','parallel','timeout','btSequence','btSelector'].includes(node.kind)&&valueSchema(project,fn,node).type!=='void')outputs.push(output('value','Результат',valueSchema(project,fn,node)));
 const d=operationDefinition(node.call?.id||node.command?.command,node.call?.version??node.command?.operationVersion??1);
 return {title:node.name||d?.editor?.label||(node.kind==='value'?VALUE_OPERATIONS[node.value?.op]:names[node.kind])||node.kind,label:d?.editor?.label||names[node.kind],inputs,outputs,category:node.kind==='value'?'Данные':'Выполнение',source:d?.source};
}
export function schemasConnect(source,target){if(!source||!target||source.type==='any'||target.type==='any')return true;if(source.type==='union')return source.variants.every(s=>schemasConnect(s,target));if(target.type==='union')return target.variants.some(s=>schemasConnect(source,s));if(source.type==='integer'&&target.type==='number')return true;if(source.type!==target.type)return false;if(source.type==='reference')return source.referenceType===target.referenceType;if(source.type==='collection')return schemasConnect(source.items,target.items);if(source.type==='object')return Object.entries(target.properties||{}).every(([k,s])=>source.properties?.[k]&&schemasConnect(source.properties[k],s));return true;}
export function graphConnectionError(project,fn,sourceId,sourcePort,targetId,targetPort){
 const source=fn.nodes.find(n=>n.id===sourceId),target=fn.nodes.find(n=>n.id===targetId);if(!source||!target)return 'Узел связи удалён.';
 const out=nodeDescription(project,fn,source).outputs.find(p=>p.name===sourcePort),into=nodeDescription(project,fn,target).inputs.find(p=>p.name===targetPort);if(!out||!into)return 'Вход или выход связи недоступен.';if(!!out.control!==!!into.control)return 'Поток выполнения соединяется только с потоком; значение — с входом данных.';
 if(into.literalOnly)return 'Константа хранит введённое значение. Подключите выход к входу вычисления.';
 if(!schemasConnect(out.schema,into.schema))return 'Тип «'+out.schema.type+'» не подходит входу «'+into.label+'» ('+into.schema.type+').';
 if(!out.control){const reaches=(id,seen=new Set())=>{if(id===targetId)return true;if(seen.has(id))return false;seen.add(id);const n=fn.nodes.find(x=>x.id===id);return dataReferences(n).some(r=>reaches(r.nodeId,seen));};if(sourceId===targetId||source.kind==='value'&&target.kind==='value'&&reaches(sourceId))return 'Цикл проводов данных. Используйте последовательную запись состояния.';}
 return null;
}
export function dataReferences(value){const out=[];const walk=v=>{if(!v||typeof v!=='object')return;if(v.op==='node'){out.push(v);return;}if(v.op==='constant')return;Object.values(v).forEach(walk);};walk(value);return out;}
export function validateFunctionGraph(project,fn){const issues=[],bad=(nodeId,port,message)=>issues.push({nodeId,port,message});if(fn.graphVersion!==undefined&&fn.graphVersion!==FUNCTION_GRAPH_VERSION){bad(fn.entry,'exec','Версия графа '+fn.graphVersion+' недоступна. Исходные данные сохранены.');return issues;}
 const ids=new Set();for(const n of fn.nodes||[]){if(ids.has(n.id))bad(n.id,'exec','ID узла повторяется.');ids.add(n.id);if(n.kind==='value'&&!VALUE_OPERATIONS[n.value?.op])bad(n.id,'value','Неизвестная операция значения.');}
 const known=new Set(['value',...Object.keys(names)]);for(const n of fn.nodes||[])if(!known.has(n.kind))bad(n.id,'exec','Неизвестный тип узла: '+n.kind);
 if(!fn.nodes?.some(n=>n.id===fn.entry&&n.kind!=='value'))bad(fn.entry,'exec','Нужна исполняемая точка входа.');
 for(const n of fn.nodes||[]){const desc=nodeDescription(project,fn,n);for(const p of desc.inputs.filter(p=>!p.control)){const value=graphInputValue(n,p),refs=p.literalOnly?[]:dataReferences(value);if(fn.graphVersion===1&&p.schema.type!=='any'&&!value?.op){for(const issue of validateValue(p.schema,value,{project,checkReferences:false,allowDeferred:true,allowExpressions:true}))bad(n.id,p.name,issue.message);}for(const ref of refs){const error=graphConnectionError(project,fn,ref.nodeId,ref.port||'value',n.id,p.name);if(error)bad(n.id,p.name,error);}}
  for(const target of [n.next,n.trueNext,n.falseNext,...(n.branches||[]),n.body].filter(Boolean))if(!fn.nodes.some(x=>x.id===target&&x.kind!=='value'))bad(n.id,'exec','Поток ведёт в отсутствующий или вычислительный узел.');
  if(['waitSignal','signal'].includes(n.kind)&&!signalDefinition(project,n.signalId))bad(n.id,'signalId','Сигнал удалён или недоступен. Выберите существующий сигнал.');
  if(n.componentMethod){const target=componentMethodTarget(project,n);if(!target.method)bad(n.id,'methodId','Метод компонента удалён: '+n.componentMethod.methodId);else if(n.call?.parameters?.instance&&!n.call.parameters.instance.op)try{assertComponentMethodReceiver(project,n.componentMethod,n.call.parameters.instance,n.call.parameters.methodId);}catch(e){bad(n.id,'call.parameters.instance',e.message);}}
  if(n.kind==='call'&&!project.functions?.some(f=>f.id===n.functionId))bad(n.id,'functionId','Вызываемая функция удалена.');if(n.kind==='operation'&&!operationDefinition(n.call?.id,n.call?.version))bad(n.id,'call.id','Операция или её версия недоступна: '+n.call?.id);
 }
 // An effect result must have been produced on every control path reaching its reader.
 // Pure nodes remain lazy; their effect dependencies are checked at the executable consumer.
 const executable=(fn.nodes||[]).filter(n=>n.kind!=='value'),pred=new Map(executable.map(n=>[n.id,[]])),edges=n=>[n.next,n.trueNext,n.falseNext,...(n.branches||[]),n.body].filter(Boolean),reachable=new Set();
 const visit=id=>{if(reachable.has(id)||!pred.has(id))return;reachable.add(id);for(const target of edges(fn.nodes.find(n=>n.id===id))){if(pred.has(target))pred.get(target).push(id);visit(target);}};visit(fn.entry);
 const dom=new Map([...reachable].map(id=>[id,new Set(id===fn.entry?[id]:reachable)]));let changed=true;while(changed){changed=false;for(const id of reachable){if(id===fn.entry)continue;const previous=pred.get(id).filter(x=>reachable.has(x));const next=new Set([id,...[...reachable].filter(candidate=>previous.every(parent=>dom.get(parent).has(candidate)))]);if(next.size!==dom.get(id).size||[...next].some(x=>!dom.get(id).has(x))){dom.set(id,next);changed=true;}}}
 const dependencies=(value,seen=new Set())=>dataReferences(value).flatMap(ref=>{if(seen.has(ref.nodeId))return [];seen.add(ref.nodeId);const source=fn.nodes.find(n=>n.id===ref.nodeId);return source?.kind==='value'?dependencies(source.value,seen):source?[source.id]:[];});
 for(const n of executable)if(reachable.has(n.id))for(const p of nodeDescription(project,fn,n).inputs.filter(p=>!p.control&&!p.literalOnly))for(const source of dependencies(graphInputValue(n,p)))if(source===n.id||!dom.get(n.id).has(source))bad(n.id,p.name,'Результат узла «'+source+'» может читаться до его выполнения. Соедините поток выполнения.');
 return issues;
}
// The migration is a pure, deterministic copy. Unknown graph versions are retained intact.
export function migrateFunctionGraph(source,{persistentState=false}={}){
 const fn=structuredClone(source);if(fn.graphVersion!==undefined&&fn.graphVersion!==1)return fn;if(fn.graphVersion===1)return fn;if(!Array.isArray(fn.nodes))return fn;
 const ids=new Set(fn.nodes.map(n=>n.id)),added=[];let index=0;
 const rewritePath=path=>persistentState&&typeof path==='string'&&path.startsWith('local.')?'state.'+path.slice(6):path;
 const lift=(value,owner,path,top=false)=>{if(!value||typeof value!=='object')return value;if(Array.isArray(value))return value.map((v,i)=>lift(v,owner,path+'-'+i));if(value.op==='node')return value;const out={};for(const [k,v]of Object.entries(value))out[k]=value.op==='constant'&&k==='value'?structuredClone(v):k==='path'?rewritePath(v):lift(v,owner,path+'-'+k);if(value.op&&!top){let id=owner+'-value-'+path.replace(/[^a-zA-Z0-9_-]/g,'-');while(ids.has(id))id=owner+'-value-'+(++index);ids.add(id);added.push({id,kind:'value',name:VALUE_OPERATIONS[value.op]||value.op,value:out});return {op:'node',nodeId:id,port:'value'};}return out;};
 for(const n of fn.nodes){for(const key of ['path','output'])if(n[key])n[key]=rewritePath(n[key]);for(const key of ['value','condition','args','index','duration','handle','payload'])if(n[key]!==undefined)n[key]=lift(n[key],n.id,key,n.kind==='value'&&key==='value');if(n.command?.value!==undefined)n.command.value=lift(n.command.value,n.id,'command-value');if(n.call?.parameters)n.call.parameters=lift(n.call.parameters,n.id,'call-parameters');}
 const positioned=fn.nodes.filter(n=>n.position),missing=fn.nodes.filter(n=>!n.position),right=positioned.length?Math.max(...positioned.map(n=>n.position.x+graphNodeSize(n).width))+80:50,positions=graphGridPositions(missing,undefined,{columns:4,origin:{x:right,y:50}});for(const n of missing)n.position=positions[n.id];
 const left=Math.min(0,...fn.nodes.map(n=>n.position.x))-4*380,addedPositions=graphGridPositions(added,undefined,{columns:4,origin:{x:left,y:50}});for(const n of added)n.position=addedPositions[n.id];
 fn.nodes.push(...added);fn.graphVersion=1;if(persistentState&&fn.localSchema)fn.stateSchema=structuredClone(fn.localSchema);return fn;
}
