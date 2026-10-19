import {variableType} from './variableModel.js';
import {assertSerializable,resolveReference} from './valueSchemas.js';
import {eventTargetOverrides} from './authoredMechanics.js';
import {assertSchema} from './valueSchemas.js';
import {validateValue,schemaDefaults} from './valueSchemas.js';
import {uid} from './model.js';
import {migrateFunctionGraph,validateFunctionGraph} from './functionGraph.js';
import {assertMechanicScript} from './mechanicScriptModel.js';
import {runMechanicJavaScript} from './mechanicScriptRuntime.js';
const safe=k=>!['__proto__','constructor','prototype'].includes(String(k));
export function pathValue(object,path){return String(path||'').split('.').filter(Boolean).reduce((v,k)=>safe(k)?v?.[k]:undefined,object);}
export function writePath(object,path,value){const keys=String(path).split('.');if(keys.some(k=>!safe(k)||!k))throw Error('Недопустимое имя поля.');let target=object;for(const k of keys.slice(0,-1)){if(!target[k]||typeof target[k]!=='object')throw Error('Поле структуры не найдено: '+k);target=target[k];}target[keys.at(-1)]=structuredClone(value);}
// Expressions are data, never JavaScript; bounded evaluation also applies to imported graphs.
export function evaluateExpression(expression,scope,depth=0){const value=evaluateExpressionValue(expression,scope,depth);if(expression?.op)scope.observe?.({operator:expression.op,...(expression.path?{path:expression.path}:{}),value});return value;}
function evaluateExpressionValue(expression,scope,depth=0){
 if(depth>64)throw Error('Выражение слишком глубоко.');if(expression===null||typeof expression!=='object')return structuredClone(expression);if(Array.isArray(expression))return expression.map(x=>evaluateExpression(x,scope,depth+1));
 if(!expression.op)return Object.fromEntries(Object.entries(expression).filter(([k])=>safe(k)).map(([k,v])=>[k,evaluateExpression(v,scope,depth+1)]));
 const e=expression,read=x=>evaluateExpression(x,scope,depth+1),a=()=>read(e.a),b=()=>read(e.b);
 if(e.op==='constant')return structuredClone(e.value);
 if(e.op==='node'){if(!scope.readNode)throw Error('Провод данных доступен только внутри графа функции.');return scope.readNode(e.nodeId,e.port||'value',depth+1);}
 if(e.op==='itemCount'){if(!scope.itemCount)throw Error('Инвентарь недоступен.');const item=read(e.itemId),owner=e.owner?read(e.owner):undefined;return scope.itemCount(item?.kind==='reference'?scope.reference(item):item,owner?.kind==='reference'?scope.reference(owner):owner);}
 if(e.op==='get')return structuredClone(pathValue(scope,e.path));
 if(e.op==='array')return (e.values||[]).map(read);
 if(e.op==='struct')return Object.fromEntries(Object.entries(e.fields||{}).filter(([k])=>safe(k)).map(([k,v])=>[k,read(v)]));
 if(e.op==='field')return structuredClone(pathValue(a(),e.field));
 if(e.op==='at')return structuredClone(a()?.[b()]);if(e.op==='length')return a()?.length??0;
 if(e.op==='and')return Boolean(a())&&Boolean(b());if(e.op==='or')return Boolean(a())||Boolean(b());if(e.op==='not')return !a();
 if(e.op==='eq')return JSON.stringify(a())===JSON.stringify(b());if(e.op==='ne')return JSON.stringify(a())!==JSON.stringify(b());
 const x=a(),y=b();if(!Number.isFinite(x)||!Number.isFinite(y))throw Error('Арифметика принимает конечные числа.');
 const ops={add:()=>x+y,sub:()=>x-y,mul:()=>x*y,div:()=>x/y,min:()=>Math.min(x,y),max:()=>Math.max(x,y),lt:()=>x<y,lte:()=>x<=y,gt:()=>x>y,gte:()=>x>=y};
 if(!ops[e.op]||e.op==='div'&&y===0)throw Error('Недопустимая арифметическая операция.');const result=ops[e.op]();if(typeof result==='number'&&!Number.isFinite(result))throw Error('Результат вне диапазона.');return result;
}
export function newBlueprintFunction(){const id=uid('function');return {id,name:'Новая функция',parameters:[{name:'amount',type:'number',default:25}],locals:{result:0},entry:id+'-set',nodes:[{id:id+'-set',kind:'set',name:'Вычислить',path:'local.result',value:{op:'get',path:'args.amount'},next:id+'-return'},{id:id+'-return',kind:'return',name:'Вернуть результат',value:{op:'get',path:'local.result'}}]};}
// Both drivers use the same interpreter. The sync driver is for bounded, synchronous
// capability hooks; it never creates another model or evaluator.
async function driveAsync(iterator){let step=iterator.next();while(!step.done){try{step=iterator.next(await step.value);}catch(error){step=iterator.throw(error);}}return step.value;}
function driveAuto(iterator){let step=iterator.next();while(!step.done){if(step.value&&typeof step.value.then==='function')return (async()=>{try{const value=await step.value;return driveAsyncContinued(iterator,iterator.next(value));}catch(error){return driveAsyncContinued(iterator,iterator.throw(error));}})();step=iterator.next(step.value);}return step.value;}
async function driveAsyncContinued(iterator,step){while(!step.done){try{step=iterator.next(await step.value);}catch(error){step=iterator.throw(error);}}return step.value;}
function driveSync(iterator){let step=iterator.next();while(!step.done){if(step.value&&typeof step.value.then==='function'){step.value.catch?.(()=>{});iterator.return();throw Error('Асинхронная операция недоступна в синхронном поведении.');}step=iterator.next(step.value);}return step.value;}
export function callBlueprint(p,id,args,variables,command,options={}){return driveAsync(blueprintSteps(p,id,args,variables,command,options));}
export function callBlueprintAuto(p,id,args,variables,command,options={}){return driveAuto(blueprintSteps(p,id,args,variables,command,options));}
export function callBlueprintSync(p,id,args,variables,command,options={}){return driveSync(blueprintSteps(p,id,args,variables,command,options));}
function awaitOwnedWork(promise,signal,assertActive){assertActive();return new Promise((resolve,reject)=>{let settled=false;const cleanup=()=>signal?.removeEventListener('abort',cancel),cancel=()=>{if(settled)return;settled=true;cleanup();reject(Error('INSTANCE_STOPPED'));};if(signal?.aborted){cancel();return;}signal?.addEventListener('abort',cancel,{once:true});Promise.resolve(promise).then(value=>{if(settled)return;settled=true;cleanup();try{assertActive();resolve(value);}catch(error){reject(error);}},error=>{if(settled)return;settled=true;cleanup();reject(error);});});}
let workSequence=0;
function* blueprintSteps(p,id,args,variables,command,options={}){
 const {depth=0,budget={remaining:1000},assertActive=()=>{},itemCount,localState,parameterContext={},beforeStep,observe}=options;
 assertActive();if(depth>16)throw Error('Слишком много вложенных вызовов функции.');const source=p.functions?.find(f=>f.id===id);if(!source)throw Error('Функция не найдена: '+id+'.');
 if(source.graphVersion!==undefined&&source.graphVersion!==1)throw Error('Версия графа '+source.graphVersion+' недоступна: '+source.name+'. Исходные данные сохранены.');
 // Old persistent-local documents are adapted at the boundary to the same graph interpreter.
 const f=source.graphVersion===1?source:migrateFunctionGraph(source,{persistentState:!!localState});
 args={...args};for(const param of f.parameters||[])if(param.referenceType&&args[param.name]?.kind==='reference')args[param.name]=resolveReference(args[param.name],param.referenceType,{...parameterContext,project:p,bindings:args});
 for(const param of f.parameters||[]){if(param.schema){const issues=validateValue(param.schema,args?.[param.name]??param.default??schemaDefaults(param.schema),{...parameterContext,project:p,parameters:args});if(issues.length)throw Error(issues.map(x=>x.message).join(' '));continue;}const value=args?.[param.name]??param.default;const type=Array.isArray(value)?'array':value&&typeof value==='object'?'struct':typeof value;if(type!==param.type)throw Error('Тип параметра '+param.name+' должен быть '+param.type+'.');}
 const scope={observe,itemCount,reference:value=>resolveReference(value,value.type,{...parameterContext,project:p,bindings:args}),global:variables,args:{...args,...Object.fromEntries((f.parameters||[]).map(a=>[a.name,structuredClone(args?.[a.name]??a.default??(a.schema?schemaDefaults(a.schema):undefined))]))},local:structuredClone(options.initialLocals??{...schemaDefaults(f.localSchema||{type:'object',properties:{}}),...f.locals}),state:localState??options.state??structuredClone({...schemaDefaults(f.stateSchema||{type:'object',properties:{}}),...f.initialState})};
 const nodes=new Map(f.nodes.map(n=>[n.id,n])),results=new Map(Object.entries(options.nodeResults||{})),reading=new Set();
 scope.readNode=(nodeId,port='value',expressionDepth=0)=>{assertActive();if(port!=='value')throw Error('Неизвестный выход данных: '+port);const n=nodes.get(nodeId);if(!n)throw Object.assign(Error('Узел данных удалён: '+nodeId),{nodeId,functionId:id});if(n.kind!=='value'){if(!results.has(nodeId))throw Object.assign(Error('Результат узла «'+(n.name||nodeId)+'» недоступен до его выполнения.'),{nodeId,functionId:id});return structuredClone(results.get(nodeId));}if(reading.has(nodeId))throw Object.assign(Error('Цикл проводов данных: '+nodeId),{nodeId,functionId:id});if(--budget.remaining<0)throw Object.assign(Error('Функция превысила лимит шагов. Проверьте цикл.'),{nodeId,functionId:id});reading.add(nodeId);try{return evaluateExpression(n.value,scope,expressionDepth+1);}catch(error){error.nodeId??=nodeId;error.functionId??=id;throw error;}finally{reading.delete(nodeId);}};
 const assign=(path,value)=>{const root=String(path).split('.')[0];if(!['local','state','global'].includes(root))throw Error('Запись разрешена только в переменные, данные экземпляра или локальные данные.');const schema=root==='state'?parameterContext.stateSchema||f.stateSchema:f.localSchema;if(root==='global'){const key=path.split('.')[1],type=variableType(p,key);if(type){const proposed={global:{[key]:structuredClone(scope.global[key])}};writePath(proposed,path,value);const v=proposed.global[key];const invalid=type==='integer'?!Number.isInteger(v):type==='number'?!Number.isFinite(v):type==='array'?!Array.isArray(v):type==='struct'?!v||typeof v!=='object'||Array.isArray(v):typeof v!==type;if(invalid)throw Error('Тип переменной «'+key+'» должен быть '+type+'.');}}if(root!=='global'&&schema){const proposed=structuredClone(scope[root]);writePath({[root]:proposed},path,value);const issues=validateValue(schema,proposed,{project:p,...parameterContext});if(issues.length)throw Error(issues.map(e=>e.message).join(' '));}writePath(scope,path,value);};
 const read=e=>evaluateExpression(e,scope),childOptions=extra=>({...options,entry:undefined,nodeResults:undefined,initialLocals:undefined,stopBefore:undefined,returnContext:undefined,admittedBranch:undefined,depth:depth+1,budget,parameterContext:{...parameterContext,stateSchema:parameterContext.stateSchema},...extra});
 const send=call=>command(call,{assertActive,runId:options.runId,detached:options.detached});
 // The script override is opt-in at the gameplay boundary. Story, widget and
 // other function editors continue to use their existing graph implementation.
 if(options.allowMechanicScripts&&source.mechanicScript?.compiledSource){
  const script=source.mechanicScript;assertMechanicScript(script);if(script.version!==1)throw Error('Версия JavaScript-механики недоступна.');if(!script.approved)throw Error('Импортированный JavaScript не включён. Откройте механику и скомпилируйте скрипт.');
  const deps=script.dependencies||{},lookup=(group,key,idKey)=>{const value=deps[group]?.[key]?.[idKey];if(!value)throw Error('Зависимость скрипта не назначена: '+key);return value;};
  let alive=true;const active=()=>{if(!alive)throw Error('INSTANCE_STOPPED');assertActive();};
  const scriptOptions=extra=>childOptions({...extra,assertActive:active});
  const rootPath=(root,keys)=>{if(!['global','state'].includes(root)||keys.some(k=>!safe(k)))throw Error('Недопустимое поле скрипта.');if(root==='global'&&keys.length){const binding=deps.globals?.[keys[0]]?.path;if(!binding?.startsWith('global.'))throw Error('Переменная скрипта не назначена: '+keys[0]);return binding+(keys.length>1?'.'+keys.slice(1).join('.'):'');}return [root,...keys].join('.');};
  const bridge=(method,values)=>{active();const [a,b,c]=values;
   if(method==='get'){if(!b.length&&a==='global')return Object.fromEntries(Object.keys(deps.globals||{}).map(key=>[key,pathValue(scope,rootPath(a,[key]))]));return structuredClone(pathValue(scope,rootPath(a,b)));}
   if(method==='set'){if(options.readOnly)throw Error('Условие может только читать переменные и состояние.');assign(rootPath(a,b),c);return c;}
   if(method==='operation')return command({command:a,operationVersion:c??1,value:b||{}},{assertActive:active,runId:options.runId,detached:options.detached});
   if(method==='call')return driveAuto(blueprintSteps(p,lookup('functions',a,'functionId'),b||{},variables,command,scriptOptions()));
   if(method==='ref'){const value=deps.references?.[a];if(!value)throw Error('Объект скрипта не назначен: '+a);return structuredClone(value);}
   if(method==='count')return itemCount?.(a?.kind==='reference'?scope.reference(a):a,b?.kind==='reference'?scope.reference(b):b);
   if(options.readOnly)throw Error('Эта операция недоступна в условии механики.');
   if(method==='wait')return options.wait?.(a,{assertActive:active,timeSource:b||options.timeSource,signal:options.abortSignal});
   if(method==='signal')return options.signal?.(deps.signals?.[a]?.signalId||a,b||{},{assertActive:active,runId:options.runId,detached:options.detached});
   if(method==='waitSignal')return options.waitSignal?.(deps.signals?.[a]?.signalId||a,{assertActive:active,timeout:b,signal:options.abortSignal});
   if(method==='event'){const eventId=lookup('events',a,'eventId'),value=b||{};return command({command:'runEvent',value:{eventId,join:value.join||'FLOW_END',overrides:value.overrides||{},actionOverrides:{...value.actionOverrides,...eventTargetOverrides(p,value.targetBindings,{...parameterContext,bindings:scope.args})}}},{assertActive:active,runId:options.runId});}
   if(method==='log'){options.observe?.({script:source.name,message:values.map(v=>typeof v==='string'?v:JSON.stringify(v)).join(' ')});return;}
   if(method==='start'){
    if(!options.runRegistry)throw Error('Запуск работы недоступен.');if([...options.runRegistry.values()].filter(w=>w.status==='running').length>=128)throw Error('Превышен лимит работ.');const workId='script-work:'+id+':'+(++workSequence),controller=new AbortController();let cancelled=false;const workActive=()=>{(options.assertOwnerActive||assertActive)();if(cancelled)throw Error('INSTANCE_STOPPED');},work={id:workId,status:'running',cancel(){cancelled=true;controller.abort();this.status='cancelled';options.cancelWork?.(workId);}};options.runRegistry.set(workId,work);work.promise=driveAsync(blueprintSteps(p,lookup('functions',a,'functionId'),b||{},variables,command,childOptions({assertActive:workActive,abortSignal:controller.signal,runId:workId,detached:true,budget:{remaining:1000}}))).then(result=>{if(!cancelled){work.status='completed';work.result=result;}return result;},error=>{if(!cancelled){work.status='failed';options.onWorkError?.(error);}throw error;});work.promise.catch(()=>{});options.onStartWork?.(work);return {kind:'work',id:workId};
   }
   if(method==='join'||method==='cancel'){const work=options.runRegistry?.get(a?.id);if(!work)throw Error('Работа не найдена.');if(method==='cancel')return work.cancel();return awaitOwnedWork(work.promise,options.abortSignal,active);}
   throw Error('Неизвестный метод engine.'+method);
  };
  const task=runMechanicJavaScript(script.compiledSource,{args:scope.args,engine:{local:scope.local,cache:options.nodeResults||{},admitted:options.admittedBranch?.value??true},context:Object.fromEntries(['source','target','initiator','scope','sceneId'].filter(k=>parameterContext[k]!==undefined).map(k=>[k,parameterContext[k]]))},bridge,{assertActive:active,abortSignal:options.abortSignal,phase:options.stopBefore?'condition':options.admittedBranch?.value===false?'otherwise':'main'}).then(value=>{active();if(options.stopBefore)return value&&typeof value==='object'&&Object.hasOwn(value,'value')?value:{value:Boolean(value),nodeResults:{},local:scope.local};if(value!==undefined)assertSerializable(value);if(f.resultSchema){const issues=validateValue(f.resultSchema,value,{...parameterContext,project:p});if(issues.length)throw Error('Результат скрипта: '+issues.map(x=>x.message).join(' '));}return value;}).finally(()=>{alive=false;});
  return yield task;
 }
 let next=options.entry||f.entry,admissionUsed=false;
 while(next){let n;try{
  if(beforeStep){const ready=beforeStep(assertActive,options.abortSignal);if(ready&&typeof ready.then==='function')yield ready;}assertActive();if(--budget.remaining<0)throw Error('Функция превысила лимит шагов. Проверьте цикл.');n=nodes.get(next);if(!n)throw Error('Узел функции удалён: '+next);if(next===options.stopBefore){if(n.kind!=='branch')throw Error('Вход условия обработчика должен быть узлом ветвления.');return {value:read(n.condition),nodeResults:Object.fromEntries(results),local:structuredClone(scope.local)};}let result;
  if(n.kind==='value')throw Error('Вычислительный узел нельзя запускать потоком выполнения.');
  if(n.kind==='set'){if(options.readOnly&&!n.path?.startsWith('local.'))throw Error('Условие обработчика может изменять только локальные вычисления.');result=read(n.value);assign(n.path,result);}
  else if(n.kind==='branch'){const admitted=options.admittedBranch?.nodeId===n.id&&!admissionUsed;const condition=admitted?options.admittedBranch.value:read(n.condition);if(admitted)admissionUsed=true;next=condition?n.trueNext:n.falseNext;continue;}
  else if(n.kind==='array'){if(options.readOnly&&!n.path?.startsWith('local.'))throw Error('Условие обработчика может изменять только локальные вычисления.');const list=structuredClone(pathValue(scope,n.path));if(!Array.isArray(list))throw Error('Выберите массив.');if(n.operation==='push')list.push(read(n.value));else if(n.operation==='remove'||n.operation==='set'){const index=Number(read(n.index));if(!Number.isInteger(index)||index<0||index>=list.length)throw Error('Индекс вне массива.');if(n.operation==='remove')list.splice(index,1);else list[index]=read(n.value);}else throw Error('Неизвестная операция массива.');assign(n.path,list);result=list;}
  else if(n.kind==='call')result=yield* blueprintSteps(p,n.functionId,read(n.args)||{},variables,command,childOptions());
  else if(n.kind==='operation')result=yield send({command:n.call.id,operationVersion:n.call.version??1,value:read(n.call.parameters)||{}});
  else if(n.kind==='event')result=yield send({command:'runEvent',value:{eventId:n.eventId,join:n.join||'FLOW_END',overrides:read(n.overrides)||{},actionOverrides:{...read(n.actionOverrides),...eventTargetOverrides(p,n.targetBindings,{...parameterContext,bindings:scope.args})}}});
  else if(n.kind==='command')result=yield send({...n.command,value:read(n.command?.value)});
  else if(n.kind==='wait'){if(!options.wait)throw Error('Ожидание времени недоступно в этом контексте.');const duration=read(n.duration);if(!Number.isFinite(duration)||duration<0)throw Error('Время ожидания должно быть конечным неотрицательным числом.');yield options.wait(duration,{assertActive,timeSource:n.timeSource||options.timeSource,signal:options.abortSignal});}
  else if(n.kind==='signal'){if(!options.signal)throw Error('Сигналы недоступны в этом контексте.');result=yield options.signal(n.signalId,read(n.payload)||{},{assertActive,runId:options.runId,detached:options.detached});}
  else if(n.kind==='waitSignal'){if(!options.waitSignal)throw Error('Ожидание сигнала недоступно в этом контексте.');result=yield options.waitSignal(n.signalId,{assertActive,timeout:read(n.duration),signal:options.abortSignal});}
  else if(n.kind==='parallel'||n.kind==='timeout'){
   const branches=n.kind==='parallel'?(n.branches||[]):[n.body];if(!branches.length||branches.some(x=>!nodes.has(x)))throw Error('Подключите ветви выполнения.');let cancelled=false;const controller=new AbortController(),runId='graph:'+id+':'+(++workSequence),active=()=>{assertActive();if(cancelled)throw Error('INSTANCE_STOPPED');};
   const parentAbort=()=>controller.abort();options.abortSignal?.addEventListener('abort',parentAbort,{once:true});const cancel=()=>{cancelled=true;controller.abort();options.abortSignal?.removeEventListener('abort',parentAbort);options.cancelWork?.(runId);};
   const tasks=branches.map(entry=>driveAsync(blueprintSteps(p,id,scope.args,variables,command,childOptions({depth,entry,runId,abortSignal:controller.signal,assertActive:active,initialLocals:scope.local,nodeResults:Object.fromEntries(results),localState:scope.state,state:scope.state}))));
   try{if(n.kind==='parallel')result=yield Promise.all(tasks);else{if(!options.wait)throw Error('Таймаут недоступен в этом контексте.');const duration=read(n.duration);if(!Number.isFinite(duration)||duration<0)throw Error('Некорректное время таймаута.');const deadline=options.wait(duration,{assertActive:active,timeSource:n.timeSource||options.timeSource,signal:controller.signal}).then(()=>{throw Error('Превышено время ожидания.');});result=yield Promise.race([tasks[0],deadline]);}}finally{cancel();}
  }
  else if(n.kind==='start'){
   if(!options.runRegistry)throw Error('Запуск работы недоступен в этом контексте.');if([...options.runRegistry.values()].filter(w=>w.status==='running').length>=128)throw Error('Превышен лимит одновременных работ (128).');if(options.runRegistry.size>=256){const old=[...options.runRegistry].find(([,w])=>w.status!=='running');if(old)options.runRegistry.delete(old[0]);}const workId='graph-work:'+id+':'+(++workSequence);let cancelled=false;const controller=new AbortController(),active=()=>{(options.assertOwnerActive||assertActive)();if(cancelled)throw Error('INSTANCE_STOPPED');};const work={id:workId,status:'running',assertActive:active,cancel(){if(this.status!=='running')return;cancelled=true;controller.abort();this.status='cancelled';options.cancelWork?.(workId);}};options.runRegistry.set(workId,work);
   work.promise=driveAsync(blueprintSteps(p,n.functionId,read(n.args)||{},variables,command,childOptions({runId:workId,detached:true,abortSignal:controller.signal,assertActive:active,budget:{remaining:1000}}))).then(value=>{if(!cancelled){work.status='completed';work.result=value;}return value;},error=>{if(!cancelled){work.status=['CANCELLED','INSTANCE_STOPPED'].includes(error.message)?'cancelled':'failed';work.error=error;options.cancelWork?.(workId);if(work.status==='failed')options.onWorkError?.(error);}throw error;});work.promise.catch(()=>{});options.onStartWork?.(work);result={kind:'work',id:workId};
  }
  else if(n.kind==='await'||n.kind==='cancel'){const handle=read(n.handle),work=options.runRegistry?.get(handle?.id);if(!work)throw Error('Работа не найдена.');if(n.kind==='cancel')work.cancel();else{result=yield awaitOwnedWork(work.promise,options.abortSignal,assertActive);if(work.status==='cancelled')throw Error('INSTANCE_STOPPED');}}
  else if(n.kind==='btSequence'||n.kind==='btSelector'){result=n.kind==='btSequence'?'Success':'Failure';for(const functionId of n.children||[]){const status=yield* blueprintSteps(p,functionId,read(n.args)||scope.args,variables,command,childOptions());if(!['Running','Success','Failure'].includes(status))throw Error('Задача поведения должна вернуть Running, Success или Failure.');result=status;if(status==='Running'||n.kind==='btSequence'&&status==='Failure'||n.kind==='btSelector'&&status==='Success')break;}}
  else if(n.kind==='return'){if(options.stopBefore)throw Error('Поток условия не достигает ветвления обработчика: '+options.stopBefore);result=read(n.value);if(f.resultSchema){const issues=validateValue(f.resultSchema,result,{...parameterContext,project:p});if(issues.length)throw Error('Результат функции: '+issues.map(x=>x.message).join(' '));}return result;}
  else throw Error('Неизвестный узел функции: '+n.kind);
  assertActive();results.set(n.id,structuredClone(result));if(n.output)assign(n.output,result);observe?.({nodeId:n.id,functionId:id,result});next=n.next;
 }catch(error){error.nodeId??=n?.id||next;error.functionId??=id;if(!['CANCELLED','INSTANCE_STOPPED'].includes(error.message)&&!error.message.includes('[узел '))error.message+=' [узел '+error.nodeId+']';throw error;}}
 if(options.stopBefore)throw Object.assign(Error('Поток условия не достигает ветвления обработчика: '+options.stopBefore),{nodeId:options.stopBefore,functionId:id});
 if(f.resultSchema){const issues=validateValue(f.resultSchema,undefined,{project:p,...parameterContext});if(issues.length)throw Error('Функция не вернула результат нужного типа.');}return undefined;
}
export function validateFunctions(p){
 const issues=[],bad=(id,detail)=>issues.push({id:'function-'+id,level:'error',title:'Проверьте функцию Blueprint',detail});
 if(!Array.isArray(p.functions||[])){bad('shape','Повреждена библиотека функций.');return issues;}
 const functionIds=new Set();
 for(const f of p.functions||[]){
  if(!f?.id||!Array.isArray(f.parameters)||!Array.isArray(f.nodes)||f.nodes.some(n=>!n||typeof n!=='object')||!f.nodes.some(n=>n.id===f.entry)){bad(f?.id,'Функции нужны параметры, ноды и точка входа.');continue;}
  if(functionIds.has(f.id))bad(f.id,'ID функций повторяются.');functionIds.add(f.id);
  if(f.localSchema)try{assertSchema(f.localSchema);for(const e of validateValue(f.localSchema,f.locals,{project:p,allowDeferred:true}))bad(f.id,e.message);}catch(e){bad(f.id,e.message);}
  if(f.mechanicScript)try{assertMechanicScript(f.mechanicScript);}catch(e){bad(f.id,e.message);}
  if(f.resultSchema)try{assertSchema(f.resultSchema);}catch(e){bad(f.id,e.message);}
  const params=new Set();for(const a of f.parameters){if(a?.schema){try{assertSchema(a.schema);}catch(e){bad(f.id,e.message);}}if(!a||typeof a.name!=='string'||!a.name||!safe(a.name)||params.has(a.name)||!a.schema&&!['number','boolean','string','array','struct'].includes(a.type)||a.referenceType&&(!['object','item','scene','event','function'].includes(a.referenceType)||a.type!=='string'))bad(f.id,'Проверьте имена и типы параметров.');params.add(a?.name);}
  for(const issue of validateFunctionGraph(p,f))bad(issue.nodeId,issue.message);
  const ids=new Set(f.nodes.map(n=>n.id));if(ids.size!==f.nodes.length||f.nodes.some(n=>typeof n.id!=='string'||!n.id))bad(f.id,'ID нод повторяются или отсутствуют.');
  for(const n of f.nodes){if(n.kind==='event'&&!p.events?.some(e=>e.id===n.eventId))bad(n.id,'Событие функции удалено.');if(!['set','branch','array','call','event','command','operation','value','return','wait','waitSignal','signal','parallel','timeout','start','await','cancel','btSequence','btSelector'].includes(n.kind))bad(n.id,'Неизвестный тип ноды.');for(const target of [n.next,n.trueNext,n.falseNext].filter(Boolean))if(!ids.has(target))bad(n.id,'Связь ведёт в удалённую ноду.');if(n.kind==='call'&&!p.functions.some(x=>x.id===n.functionId))bad(n.id,'Вызванная функция удалена.');if(['set','array'].includes(n.kind)&&(!/^(global|local|state)\.[^.]+/.test(n.path)||n.path.split('.').some(k=>!safe(k))))bad(n.id,'Выберите изменяемое значение.');}
 }
 const checkArgs=(id,functionId,args={})=>{const f=p.functions?.find(f=>f.id===functionId);for(const a of f?.parameters||[])if(a.schema)for(const e of validateValue(a.schema,args?.[a.name]??a.default??schemaDefaults(a.schema),{project:p,allowDeferred:true,allowExpressions:true},a.name))bad(id+'-'+a.name,e.message);};
 for(const f of p.functions||[])for(const n of f.nodes||[])if(n.kind==='call')checkArgs(n.id,n.functionId,n.args);
 for(const r of p.gameplay?.rules||[]){checkArgs(r.id+'-condition',r.conditionFunctionId,r.conditionArgs);checkArgs(r.id+'-function',r.functionId,r.args);}
 return issues;
}
