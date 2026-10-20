import {componentStateView,componentParameterView} from './componentFieldBindings.js';
import {evaluateExpression} from './blueprintFunctions.js';
import {instanceValues} from './authoredMechanics.js';
import {reference,schemaDefaults,assertSerializable} from './valueSchemas.js';
import {componentMethodTarget,assertComponentMethodReceiver} from './componentCatalog.js';
import {componentError} from './componentDiagnostics.js';

export function assertBehaviorTree(project,resource){
 if(!resource||resource.version!==1||resource.kind!=='behavior.tree'||!Array.isArray(resource.nodes)||resource.nodes.length>256)throw Error('Ресурс Behavior Tree повреждён или версия недоступна.');
 assertSerializable(resource);const nodes=new Map(resource.nodes.map(n=>[n.id,n]));if(nodes.size!==resource.nodes.length||!nodes.has(resource.root))throw Error('Выберите корень Behavior Tree; ID узлов должны быть уникальными.');
 const visit=(id,ancestors=new Set())=>{const n=nodes.get(id);if(!n||ancestors.has(id))throw Error('Behavior Tree: отсутствующий узел или цикл '+id);const next=new Set([...ancestors,id]);if(next.size>64)throw Error('Behavior Tree слишком глубокое.');
  if(['selector','sequence'].includes(n.kind)){if(!Array.isArray(n.children))throw Error('У составного узла нужен список детей.');for(const child of n.children)visit(child,next);}
  else if(n.kind==='task'){const target=componentMethodTarget(project,{componentMethod:n.componentMethod,call:{parameters:n.call}});if(!target.method)throw Error('Behavior Tree: метод задачи удалён: '+n.call?.methodId);}
  else if(n.kind!=='condition'&&n.kind!=='value')throw Error('Неизвестный узел Behavior Tree: '+n.kind);
 };for(const n of resource.nodes)visit(n.id);
}
export function stopComponentBehavior(game,id){const handle=game.componentHandles?.get(id),tree=handle?.tree;if(tree){tree.epoch++;tree.task?.cancel();tree.controller.abort();delete handle.tree;}if(game.state.componentBehaviors?.[id])game.state.componentBehaviors[id].status='Stopped';}
function failTree(game,tree,state,resource,error,nodeId){
 if(tree.failed)return;tree.failed=true;tree.epoch++;tree.task?.cancel();tree.task=null;tree.controller.abort();state.status='Failure';error.nodeId??=nodeId||state.nodeId||resource.root;error.message=resource.path+' · '+error.nodeId+' · '+error.message;componentError(error,game.host.project,tree.instanceId,'start');if(error.componentDiagnostic){error.componentDiagnostic.nodeId=error.nodeId;error.componentDiagnostic.behaviorResourceId=resource.id;error.componentDiagnostic.message=error.message;}state.error=error.message;game.componentHandles.get(tree.instanceId)?.behaviorRun?.reject(error);game.error(error);
}
function evaluateTree(tree,resource,scope){
 const nodes=new Map(resource.nodes.map(n=>[n.id,n])),reading=new Set();let budget=512;
 scope.readNode=id=>{const n=nodes.get(id);if(!n||n.kind!=='value'||reading.has(id)||--budget<0)throw Error('Недоступный выход данных Behavior Tree: '+id);reading.add(id);try{return evaluateExpression(n.value,scope);}finally{reading.delete(id);}};
 const visit=id=>{if(--budget<0)throw Error('Behavior Tree превысило бюджет обхода.');const n=nodes.get(id);if(!n)throw Error('Узел дерева удалён: '+id);
  if(n.kind==='condition')return {status:evaluateExpression(n.condition,scope)?'Success':'Failure'};
  if(n.kind==='task')return tree.outcomes.has(id)?{status:tree.outcomes.get(id)}:{status:'Running',leaf:n};
  if(['sequence','selector'].includes(n.kind)){for(const child of n.children){const result=visit(child);if(n.kind==='sequence'?result.status!=='Success':result.status!=='Failure')return result;}return {status:n.kind==='sequence'?'Success':'Failure'};}
  throw Error('Выход данных не является ветвью Behavior Tree.');
 };return visit(resource.root);
}
// Native scheduling only selects/cancels an authored branch. Every task invokes
// GameSession's common component API; no Rules publication or project snapshot.
export function updateComponentBehaviors(game,dt){
 for(const m of game.config.mechanics){const handle=game.componentHandles?.get(m.id);if(!handle?.active)continue;const d=game.config.mechanicDefinitions.find(d=>d.id===m.definitionId&&d.version===m.definitionVersion),config=d?.behaviorTree;if(!config||(config.autoStart===false?!game.state.componentBehaviors?.[m.id]?.enabled:!game.state.active))continue;
  const resource=game.host.project.logicResources?.find(r=>r.id===config.treeResourceId);
  if(!handle.tree){assertBehaviorTree(game.host.project,resource);handle.tree={instanceId:m.id,elapsed:Infinity,outcomes:new Map(),epoch:0,controller:new AbortController()};}
  const tree=handle.tree;if(tree.failed)continue;tree.elapsed+=dt;if(tree.elapsed<(config.interval??.1)||game.host.snapshot.paused)continue;tree.elapsed=0;
  const params=componentParameterView(game.host.project,m,d,game.host.snapshot.variables),refs={...schemaDefaults(d.references||{type:'object',properties:{}}),...m.references},scope={project:game.host.project,global:game.host.snapshot.variables,args:params,params,refs,state:componentStateView(game.host.project,m,d,game.state.mechanicStates[m.id],game.host.snapshot.variables),self:reference('mechanic',m.id),owner:reference(m.scope.kind,m.scope.id)};
  game.state.componentBehaviors??={};const state=game.state.componentBehaviors[m.id]??={status:'Running',nodeId:null,ticks:0};state.ticks++;
  try{
   let result=evaluateTree(tree,resource,scope);
   if(tree.task&&tree.nodeId!==result.leaf?.id){tree.epoch++;tree.task.cancel();tree.task=null;tree.outcomes.clear();result=evaluateTree(tree,resource,scope);}
   state.status=result.status;state.nodeId=result.leaf?.id||null;
   if(!result.leaf){tree.outcomes.clear();continue;}
   if(tree.task)continue;
   const node=result.leaf,epoch=++tree.epoch;tree.nodeId=node.id;
   const receiver=evaluateExpression(node.call.instance,scope),args=evaluateExpression(node.call.args||{},scope);
   assertComponentMethodReceiver(game.host.project,node.componentMethod,receiver,node.call.methodId);
   tree.task=game.startComponentTask(receiver,node.call.methodId,args,{abortSignal:tree.controller.signal,functionAssert:()=>{if(!handle.active||epoch!==tree.epoch)throw Error('INSTANCE_STOPPED');}});
   tree.task.promise.then(value=>{if(epoch!==tree.epoch)return;const status=value===true||value===undefined?'Success':value===false?'Failure':value;if(!['Success','Failure','Running'].includes(status))throw Error('Задача Behavior Tree должна вернуть bool, void или Success/Failure/Running.');if(status!=='Running')tree.outcomes.set(node.id,status);tree.task=null;state.status=status;}).catch(error=>{if(epoch!==tree.epoch)return;if(['CANCELLED','INSTANCE_STOPPED'].includes(error.message)){tree.task=null;tree.outcomes.set(node.id,'Failure');state.status='Failure';return;}failTree(game,tree,state,resource,error,node.id);});
  }catch(error){failTree(game,tree,state,resource,error);}
 }
}

export async function runComponentBehavior(game,id,context){
 const handle=game.componentHandles?.get(id),m=game.config.mechanics.find(m=>m.id===id),definition=game.config.mechanicDefinitions.find(d=>d.id===m?.definitionId&&d.version===m.definitionVersion);if(!handle?.active||!definition?.behaviorTree)throw Error('Компонент поведения недоступен.');if(handle.behaviorRun)throw Error('Поведение уже запущено.');
 game.state.componentBehaviors??={};const state=game.state.componentBehaviors[id]??={status:'Running',ticks:0,nodeId:null};state.enabled=true;let resolve,reject;const promise=new Promise((yes,no)=>{resolve=yes;reject=no;}),run={resolve,reject};handle.behaviorRun=run;const abort=()=>reject(Error('INSTANCE_STOPPED'));context.abortSignal?.addEventListener('abort',abort,{once:true});
 try{context.assertActive();await promise;context.assertActive();}finally{context.abortSignal?.removeEventListener('abort',abort);if(handle.behaviorRun===run){delete handle.behaviorRun;state.enabled=false;stopComponentBehavior(game,id);if(state.error)state.status='Failure';}}
}
export function endComponentBehavior(game,id){const handle=game.componentHandles?.get(id);handle?.behaviorRun?.resolve();if(game.state.componentBehaviors?.[id])game.state.componentBehaviors[id].enabled=false;stopComponentBehavior(game,id);}
