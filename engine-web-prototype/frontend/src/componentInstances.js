import {releaseComponentNativeHandles} from './componentAdapters.js';
import {componentOwnerName} from './componentCatalog.js';
import {componentError} from './componentDiagnostics.js';
import {assertComponentFieldBindings,componentStateView,componentParameterView,removeBoundState} from './componentFieldBindings.js';
import {stopComponentBehavior} from './behaviorTrees.js';
import {descriptionFor} from './mechanicDescriptions.js';
import {isComponent,componentInitialState,assertComponentDefinition} from './componentModel.js';
import {reference,resolveReference,schemaDefaults,validateValue,assertSerializable} from './valueSchemas.js';
import {ownerExists} from './ruleModel.js';

const stopped=()=>Error('INSTANCE_STOPPED');
const check=(schema,value,context,label)=>{const errors=validateValue(schema,value,context);if(errors.length)throw Error(label+': '+errors.map(e=>e.message).join(' '));};

// Lifecycle/handle ownership for the existing mechanics, not another world or
// executor. All method bodies run through GameSession.performFunction.
export function initializeComponents(game){
 game.componentHandles??=new Map();game.componentTasks??=new Map();
 game.state.componentLifecycles??={};
 for(const m of game.config.mechanics){const d=descriptionFor(game.host.project,m.definitionId,m.definitionVersion);if(!isComponent(d))continue;
  assertComponentDefinition(game.host.project,d);assertComponentFieldBindings(game.host.project,m,d);
  if(!game.state.componentLifecycles[m.id]){
   game.state.mechanicStates[m.id]=componentInitialState(d,componentParameterView(game.host.project,m,d,game.host.snapshot.variables));
   game.state.componentLifecycles[m.id]={stateVersion:d.stateVersion,initialized:false,enabled:m.enabled!==false,destroyed:false};
  }
  if(game.state.componentLifecycles[m.id].stateVersion!==d.stateVersion)throw Error('Версия состояния компонента требует миграции: '+d.name);
  removeBoundState(m,game.state.mechanicStates[m.id]);check(d.stateSchema,componentStateView(game.host.project,m,d,game.state.mechanicStates[m.id],game.host.snapshot.variables),{project:game.host.project,scope:m.scope,allowDeferred:true},d.name);
 }
}
function component(game,id){
 const m=game.config.mechanics.find(m=>m.id===id),d=descriptionFor(game.host.project,m?.definitionId,m?.definitionVersion);
 if(!isComponent(d))throw Error('Компонент или его версия недоступны: '+id);
 const record=game.state.componentLifecycles?.[id];if(!record||record.destroyed)throw Error('Компонент уничтожен или не создан: '+id);
 return {m,d,record};
}
function available(game,m){return ownerExists(game.host.project,m.scope)&& (m.scope.kind==='project'||m.scope.kind==='scene'&&m.scope.id===game.scene.id||m.scope.kind==='object'&&!!game.object(m.scope.id));}
function resolveSlots(game,m,d,context){
 const params=componentParameterView(game.host.project,m,d,game.host.snapshot.variables),refs={...schemaDefaults(d.references||{type:'object',properties:{}}),...m.references};
 const scope={...context,project:game.host.project,scope:m.scope,bindings:{...params,...refs}};
 check(d.parameters,params,scope,'Параметры компонента');check(d.references||{type:'object',properties:{}},refs,scope,'Ссылки компонента');
 return {params,refs,scope};
}
export function startComponentTask(game,instance,methodId,args={},context={},internal=false){
 const id=typeof instance==='string'?instance:resolveReference(instance,'mechanic',{...context,project:game.host.project,allowResolvedReferences:true});
 try{return createComponentTask(game,id,methodId,args,context,internal);}catch(error){throw componentError(error,game.host.project,id,methodId);}
}
function createComponentTask(game,id,methodId,args,context,internal){
 const {m,d,record}=component(game,id),handle=game.componentHandles?.get(id),method=d.methods[methodId];
 internal=internal||!!context.componentLifecycle&&context.mechanicInstanceId===id;
 if(!Object.hasOwn(d.methods,methodId))throw Error('Метод компонента недоступен: '+methodId);
 if(!internal&&(!record.enabled||!handle?.active)||!available(game,m))throw Error('INSTANCE_STOPPED · Компонент «'+(m.name||d.name)+'» владельца «'+componentOwnerName(game.host.project,m.scope)+'» недоступен: владелец выгружен, удалён или компонент отключён.');
 assertSerializable(args);if(!args||typeof args!=='object'||Array.isArray(args))throw Error('Передайте именованные аргументы метода.');
 const {params,refs,scope}=resolveSlots(game,m,d,context),values={...schemaDefaults(method.parameters),...structuredClone(args)};
 check(method.parameters,values,{...scope,allowResolvedReferences:true},'Аргументы метода '+method.name);
 if((context.componentDepth||0)>=16)throw Error('Слишком много вложенных вызовов методов.');
 if(game.componentTasks.size>=128)throw Error('Превышен лимит задач компонентов.');
 const taskId='component-task:'+game.generation+':'+(game.componentTaskSequence=(game.componentTaskSequence||0)+1),controller=new AbortController();
 const parentAssert=context.functionAssert,parentSignal=context.abortSignal;
 const task={id:taskId,instanceId:id,methodId,parentNativeRunId:context.nativeRunId,status:'running',controller,cancel(){if(this.status!=='running')return;this.status='cancelled';releaseComponentNativeHandles(game,taskId);controller.abort();
  for(const work of game.graphRuns?.values()||[])if(work.componentTaskId===taskId)work.cancel();
  for(const event of Object.values(game.host.snapshot.instances||{}))if(event.operationContext?.componentTaskId===taskId&&['running','paused','held'].includes(event.status))game.host.controlInstance(event.id,'stop');
 }};
 const active=()=>{if(controller.signal.aborted||game.disposed||record.destroyed||!internal&&(!record.enabled||!game.componentHandles.get(id)?.active)||!available(game,m))throw stopped();parentAssert?.();game.host.assert(game.generation,context.nativeRunId);game.assertReaction({...context,functionAssert:undefined});};
 const cancel=()=>task.cancel();parentSignal?.addEventListener('abort',cancel,{once:true});if(parentSignal?.aborted)cancel();
 game.componentTasks.set(taskId,task);game.flights.add(taskId);
 const callContext={...context,kind:'mechanic',...scope,sceneId:game.scene.id,mechanicInstanceId:id,componentTaskId:taskId,componentDepth:(context.componentDepth||0)+1,componentParams:params,componentRefs:refs,self:reference('mechanic',id),owner:reference(m.scope.kind,m.scope.id),localState:componentStateView(game.host.project,m,d,game.state.mechanicStates[id],game.host.snapshot.variables),functionAssert:active,abortSignal:controller.signal};
 task.promise=Promise.resolve().then(()=>{active();return game.performFunction(method.logic.functionId,values,callContext);}).then(result=>{
  active();check(method.result,result,{...scope,allowResolvedReferences:true},'Результат метода '+method.name);task.status='completed';task.result=result;return result;
 }).catch(error=>{if(task.status!=='cancelled'){task.status='failed';componentError(error,game.host.project,id,methodId);}throw error;}).finally(()=>{parentSignal?.removeEventListener('abort',cancel);releaseComponentNativeHandles(game,taskId);game.componentTasks.delete(taskId);game.flights.delete(taskId);});
 // Public handles may be cancelled before their caller starts awaiting them.
 task.promise.catch(()=>{});return task;
}
export async function callComponent(game,...args){return startComponentTask(game,...args).promise;}

function clearHandles(game,id){
 stopComponentBehavior(game,id);
 const handle=game.componentHandles?.get(id);if(handle){handle.active=false;for(const unsubscribe of handle.subscriptions)unsubscribe();handle.subscriptions=[];}
 for(const task of game.componentTasks?.values()||[])if(task.instanceId===id)task.cancel();
 for(const work of game.graphRuns?.values()||[])if(work.componentInstanceId===id)work.cancel();
 for(const [scopeId,scope]of game.graphScopes||[])if(scope.context.mechanicInstanceId===id){scope.controller.abort();game.cancelGraphWork(scopeId);}
 for(const event of Object.values(game.host.snapshot.instances||{}))if(event.operationContext?.mechanicInstanceId===id&&['running','paused','held'].includes(event.status))game.host.controlInstance(event.id,'stop');
 for(const [timerId,timer]of Object.entries(game.state.timers))if(timer.mechanicInstanceId===id)delete game.state.timers[timerId];
}
async function hook(game,m,d,name){const methodId=d.lifecycle?.[name];if(methodId)await startComponentTask(game,m.id,methodId,{}, {componentLifecycle:name},true).promise;}
function subscribe(game,m,d,handle){
 for(const subscription of d.subscriptions||[])handle.subscriptions.push(game.ruleRuntime.subscribeSignal(subscription.signalId,(payload,context)=>{
  if(!handle.active)return;
  const match=subscription.match||'owner';
  if(match==='timer'&&context.mechanicInstanceId!==m.id)return;
  if(match==='owner'&&(m.scope.kind==='object'&&(context.target?.type!=='object'||context.target.id!==m.scope.id)||m.scope.kind==='scene'&&context.sceneId!==m.scope.id))return;
  if(match==='component'){
   const {refs,scope}=resolveSlots(game,m,d,context);
   if(resolveReference(refs[subscription.reference],'mechanic',scope)!==context.emitterInstanceId)return;
  }
  const key='component-subscription:'+m.id+':'+subscription.id;
  if(context.causeChain?.includes(key)){game.error(Error('Подписка компонента повторно вызывает сама себя.'));return;}
  // Delivery transfers ownership to the receiver. Preserve event identity and
  // cycle detection, never the publisher's cancellable native Event handle.
  const task=startComponentTask(game,m.id,subscription.methodId,subscription.passPayload?payload:{},{...context,nativeRunId:undefined,reactionId:undefined,rootId:undefined,causeChain:[...(context.causeChain||[]),key]});
  task.promise.catch(error=>{if(error.message!=='INSTANCE_STOPPED')game.error(error);});
 }));
}
export function reconcileComponents(game,{restoring=false}={}){
 // Serialize scene/lifecycle changes. Re-entering the same story scene does not
 // reinitialize state, subscribe again, or restart component tasks.
 const previous=game.componentReconcile||Promise.resolve();
 const next=previous.then(async()=>{
  if(game.disposed||game.host.snapshot.audition)return;
  initializeComponents(game);
  if(restoring&&!game.componentRestorePending)game.componentRestorePending=new Set(Object.entries(game.state.componentLifecycles).filter(([,r])=>r.initialized&&!r.destroyed).map(([id])=>id));
  for(const id of game.componentHandles.keys())if(!game.config.mechanics.some(m=>m.id===id)){clearHandles(game,id);game.componentHandles.delete(id);}
  for(const m of game.config.mechanics){const d=descriptionFor(game.host.project,m.definitionId,m.definitionVersion);if(!isComponent(d))continue;
   const record=game.state.componentLifecycles[m.id];if(record.destroyed)continue;
   let handle=game.componentHandles.get(m.id);
   if(handle?.active&&(!record.enabled||!available(game,m))){clearHandles(game,m.id);if(available(game,m))await hook(game,m,d,'disable');}
   if(!record.enabled||!available(game,m))continue;
   if(!handle){handle={active:false,subscriptions:[]};game.componentHandles.set(m.id,handle);}
   if(handle.active)continue;
   resolveSlots(game,m,d,{});
   if(!record.initialized){record.initialized=true;await hook(game,m,d,'initialize');}
   else if(game.componentRestorePending?.has(m.id)){await hook(game,m,d,'restore');game.componentRestorePending.delete(m.id);}
   if(game.disposed)throw stopped();
   handle.active=true;
   try{subscribe(game,m,d,handle);await hook(game,m,d,'enable');}catch(error){clearHandles(game,m.id);throw error;}
  }
 });
 game.componentReconcile=next;return next;
}
export async function setComponentEnabled(game,id,enabled){
 if(typeof enabled!=='boolean')throw Error('Включение компонента требует bool.');
 const {m,d,record}=component(game,id),wasActive=game.componentHandles.get(id)?.active;record.enabled=enabled;
 if(!enabled){clearHandles(game,id);try{if(wasActive&&available(game,m))await hook(game,m,d,'disable');}finally{clearHandles(game,id);}}
 await reconcileComponents(game);
}
export async function destroyComponent(game,id){
 const {m,d,record}=component(game,id);clearHandles(game,id);
 try{if(available(game,m))await hook(game,m,d,'destroy');}finally{clearHandles(game,id);record.destroyed=true;record.enabled=false;game.componentHandles.delete(id);}
}
export function disposeComponents(game){for(const id of game.componentHandles?.keys()||[])clearHandles(game,id);for(const task of game.componentTasks?.values()||[])task.cancel();game.componentHandles?.clear();}
export function cleanupComponentOwners(game){for(const [id,handle]of game.componentHandles||[]){const m=game.config.mechanics.find(m=>m.id===id);if(handle.active&&(!m||!available(game,m)))clearHandles(game,id);}}
export async function emitComponentEvent(game,id,eventId,payload,context){
 const {d}=component(game,id),event=d.events?.[eventId];if(!event)throw Error('Событие компонента недоступно: '+eventId);
 check(event.payloadSchema,payload,{project:game.host.project,allowResolvedReferences:true},'Событие '+event.name);
 context.functionAssert?.();
 return game.trigger(event.signalId,payload,undefined,{...context,source:{type:'mechanic',id},emitterInstanceId:id});
}

// A launched task may explicitly outlive its invoking Sequence. It still uses
// the same dispatcher, task map and cleanup; only the lifetime owner changes.
export function startOwnedComponentTask(game,instance,methodId,args,lifetime,ownerId,context={}){
 if(!['scene','project','component'].includes(lifetime))throw Error('Выберите время жизни задачи.');
 const owner={kind:lifetime,id:ownerId},assertOwner=()=>{game.host.assert(game.generation);if(lifetime==='project'&&ownerId!==game.host.project.id||lifetime==='scene'&&(game.scene.id!==ownerId||!game.host.project.subscenes.some(s=>s.id===ownerId))||lifetime==='component'&&(!game.componentHandles?.get(ownerId)?.active||!game.state.componentLifecycles?.[ownerId]?.enabled||!game.config.mechanics.some(m=>m.id===ownerId&&available(game,m))))throw stopped();};assertOwner();
 const task=startComponentTask(game,instance,methodId,args,{...context,nativeRunId:undefined,reactionId:undefined,rootId:undefined,functionRunId:undefined,functionEntry:undefined,componentLifecycle:undefined,abortSignal:undefined,functionAssert:assertOwner});
 task.lifetimeOwner=owner;task.assertActive=assertOwner;task.componentInstanceId=task.instanceId;task.componentTaskId=task.id;game.graphRuns??=new Map();if(game.graphRuns.size>=256){const completed=[...game.graphRuns].find(([,t])=>t.status!=='running');if(completed)game.graphRuns.delete(completed[0]);}game.graphRuns.set(task.id,task);task.promise.catch(error=>{if(!['INSTANCE_STOPPED','CANCELLED'].includes(error.message))game.error(error);});return {kind:'work',id:task.id};
}
