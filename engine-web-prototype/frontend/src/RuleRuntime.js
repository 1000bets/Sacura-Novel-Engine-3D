import {JOURNAL_LIMIT,journalValue} from './executionJournal.js';
import {descriptionFor} from './mechanicDescriptions.js';
import {validateValue,resolveReference} from './valueSchemas.js';
import {countInventory} from './capabilities/inventory.js';
import {normalizeRule,ruleMatchResult,ownerExists,RULE_LIMITS} from './ruleModel.js';
import {signalContext,validateSignalPayload} from './signalModel.js';
import {evaluateExpression,callBlueprint} from './blueprintFunctions.js';
import {executeOperation,operationDefinition} from './operationRegistry.js';
// Admission/backpressure only. All actions are delegated to the existing runtime.
export class RuleRuntime {
 constructor(game){this.game=game;this.host=game.host;this.generation=this.host.generation;this.signals=[];this.pending=[];this.active=new Map();this.roots=new Map();this.sequence=0;this.closed=false;this.pumping=false;}
 get maxRuns(){const limit=this.game.config.ruleSettings?.maxRuns;return Number.isInteger(limit)&&limit>0?Math.min(RULE_LIMITS.runs,limit):1;}
 get idle(){return !this.signals.length&&!this.pending.length&&!this.active.size&&!this.pumping;}
 trace(kind,fields={}){if(this.closed||this.generation!==this.host.generation)return;const s=this.game.state;if(!s)return;s.ruleTrace??=[];s.ruleTrace.push({kind,time:s.clock,...journalValue(fields)});if(s.ruleTrace.length>JOURNAL_LIMIT)s.ruleTrace.splice(0,s.ruleTrace.length-JOURNAL_LIMIT);}
 publish(type,payload={},ruleId,context={}){
  if(this.closed||this.host.snapshot.audition||!this.host.running||this.generation!==this.host.generation)return Promise.resolve();
  validateSignalPayload(this.host.project,type,payload);
  if(this.signals.length+this.pending.length>=RULE_LIMITS.signals)throw Error('Очередь сигналов заполнена.');
  if((context.causeChain?.length||0)>=RULE_LIMITS.chain)throw Error('Сигнал повторно вызывает сам себя: превышен лимит цепочки.');
  const seq=++this.sequence,ctx=signalContext(this.host.project,type,payload,context,this.game.scene.id),root=context.rootId&&this.roots.get(context.rootId)||this.newRoot(seq);
  root.count++;const snapshot={project:structuredClone(this.host.project),variables:structuredClone(this.host.snapshot.variables),world:structuredClone(this.game.world)};snapshot.game=snapshot.world.game;const item={seq,type,payload:structuredClone(payload),context:ctx,ruleId,root,snapshot,publishedAt:this.game.state.clock,rules:this.game.rules.map(r=>normalizeRule(this.host.project,structuredClone(r)))};
  this.trace('publish',{seq,type,payload:item.payload,context:ctx});item.rules=item.rules.filter(rule=>{const result=ruleMatchResult(this.host.project,rule,item);if(rule.signal?.type===type)this.trace('match',{seq,ruleId:rule.id,instanceId:rule.mechanicInstanceId,scope:rule.scope,...result});return result.matched;});if(!item.rules.length){this.release(root);return Promise.resolve();}this.signals.push(item);this.game.flights.add('signal:'+seq);this.schedule();
  // A descendant acknowledges publication. Waiting on its own parent would deadlock.
  return context.reactionId||context.nativeRunId?Promise.resolve():root.done;
 }
 newRoot(seq){let resolve;const done=new Promise(r=>resolve=r),root={id:'root:'+seq,count:0,done,resolve};this.roots.set(root.id,root);return root;}
 release(root){if(--root.count===0){this.roots.delete(root.id);root.resolve();}}
 schedule(){if(this.closed||this.pumping||this.host.snapshot.paused||!this.signals.length)return;this.pumping=true;queueMicrotask(()=>this.pump());}
 async condition(rule,item,readings){
  const observe=value=>{if(readings.length<32)readings.push(journalValue(value));},variables=structuredClone(item.snapshot.variables),counts=(id,owner)=>countInventory(item.snapshot.project.gameplay,item.snapshot.game,id,owner),scope={observe,global:variables,args:{...item.payload,...rule.bindings},local:structuredClone(item.snapshot.game.mechanicStates?.[rule.mechanicInstanceId]||{}),itemCount:counts,reference:value=>resolveReference(value,value.type,{...item.context,project:item.snapshot.project,scope:rule.scope,bindings:rule.bindings})};
  const query=async(call)=>{const d=operationDefinition(call.id,call.version);if(!d?.pure)throw Error('Условие может выполнять только операции чтения.');const result=await executeOperation(call,{...item.context,bindings:{...item.context.bindings,...rule.bindings},scope:rule.scope,sourceId:item.context.source?.type==='object'?item.context.source.id:undefined,targetId:item.context.target?.type==='object'?item.context.target.id:undefined,kind:'rule',project:item.snapshot.project,variables,world:structuredClone(item.snapshot.world),gameState:structuredClone(item.snapshot.game),evaluate:v=>evaluateExpression(v,scope),assertActive:()=>this.assertItem(item),emit:()=>{}});observe({operation:call.id,arguments:call.parameters,result});return result;};
  if(rule.conditionOperation)return !!await query(rule.conditionOperation);
  if(rule.conditionFunctionId)return !!await callBlueprint(item.snapshot.project,rule.conditionFunctionId,{...item.payload,...rule.bindings,...evaluateExpression(rule.conditionArgs||{},scope)},variables,c=>query({id:c.command,version:c.operationVersion??1,parameters:c.value}),{assertActive:()=>this.assertItem(item),itemCount:counts,localState:scope.local,observe,parameterContext:{project:item.snapshot.project,...item.context,scope:rule.scope,bindings:rule.bindings,allowResolvedReferences:true}});
  return rule.condition==null||!!evaluateExpression(rule.condition,scope);
 }
 assertItem(item){if(this.closed||this.generation!==this.host.generation||!this.host.running)throw Error('CANCELLED');}
 async pump(){
  try{while(this.signals.length&&!this.host.snapshot.paused&&!this.closed){const item=this.signals.shift();try{this.assertItem(item);for(const rule of item.rules){if(item.context.causeChain?.includes(rule.id)){this.game.error(Error('Сигнал повторно вызывает сам себя.'));this.trace('cycle',{seq:item.seq,ruleId:rule.id});continue;}
    if(rule.transition&&item.snapshot.game.mechanicMachines?.[rule.mechanicInstanceId]?.state!==rule.transition.from||rule.machineGuard&&!rule.transition&&item.snapshot.game.mechanicMachines?.[rule.mechanicInstanceId]?.state!==rule.machineGuard.state)continue;const readings=[];let passed;try{passed=await this.condition(rule,item,readings);}catch(e){this.trace('condition-error',{seq:item.seq,ruleId:rule.id,functionId:rule.conditionFunctionId,readings,error:e.message});if(e.message==='CANCELLED')throw e;this.game.error(e);continue;}this.assertItem(item);this.trace('condition',{seq:item.seq,ruleId:rule.id,functionId:rule.conditionFunctionId,expression:rule.condition,readings,passed});if(rule.transition&&!passed)continue;const actions=passed?rule.actions:rule.otherwise;if(!actions.length&&!rule.transition)continue;this.admit({rule,item,actions});}
   }catch(e){if(e.message!=='CANCELLED')this.game.error(e);}finally{this.game.flights.delete('signal:'+item.seq);this.release(item.root);}}
  }finally{this.pumping=false;this.launchPending();if(this.signals.length&&!this.host.snapshot.paused&&!this.closed)this.schedule();}
 }
 admit(job){const {rule,item}=job,record=this.game.state.rules[rule.id]??={};if(rule.once&&record.fired||rule.cooldown>0&&item.publishedAt<(record.cooldownUntil??0)){this.trace('skip',{seq:item.seq,ruleId:rule.id,reason:rule.once&&record.fired?'Однократное правило уже сработало.':'Не истёк cooldown.',cooldownUntil:record.cooldownUntil});return;}
  if(rule.transition){const machine=this.game.state.mechanicMachines[rule.mechanicInstanceId],snapshot=item.snapshot.game.mechanicMachines[rule.mechanicInstanceId];if(machine.state!==rule.transition.from||machine.revision!==snapshot.revision){this.trace('transition-skip',{seq:item.seq,ruleId:rule.id});return;}
   const m=this.game.config.mechanics.find(m=>m.id===rule.mechanicInstanceId),d=descriptionFor(this.host.project,m.definitionId,m.definitionVersion);for(const run of this.active.values())if(run.rule.mechanicInstanceId===m.id)this.cancelRun(run);this.pending=this.pending.filter(job=>{if(job.rule.mechanicInstanceId!==m.id)return true;this.release(job.item.root);return false;});for(const i of Object.values(this.host.snapshot.instances||{}))if(i.operationContext?.mechanicInstanceId===m.id&&['running','paused','held'].includes(i.status))this.host.controlInstance(i.id,'stop');
   for(const [id,timer]of Object.entries(this.game.state.timers))if(timer.mechanicInstanceId===m.id)delete this.game.state.timers[id];for(const r of this.game.rules.filter(r=>r.mechanicInstanceId===m.id&&r.transition)){const state=this.game.state.rules[r.id];if(state){delete state.elapsed;delete state.inside;if(!r.once)state.fired=false;}}
   job.actions=[...(d.machine.states.find(s=>s.id===machine.state)?.exit||[]),...job.actions,...(d.machine.states.find(s=>s.id===rule.transition.to)?.enter||[])];machine.state=rule.transition.to;machine.revision++;job.machineRevision=machine.revision;this.game.sync();this.trace('transition',{seq:item.seq,instanceId:m.id,state:machine.state});
  }
  let running=[...this.active.values()].filter(x=>x.rule.id===rule.id&&!x.cancelled),waiting=this.pending.filter(x=>x.rule.id===rule.id);
  if(rule.reentry==='skip'&&running.length>=rule.maxConcurrent){this.trace('skip',{seq:item.seq,ruleId:rule.id,reason:'Предыдущий запуск ещё выполняется; повторный вход пропускается.'});return;}
  if(rule.reentry==='restart'){running.forEach(r=>this.cancelRun(r));this.pending=this.pending.filter(x=>{if(x.rule.id!==rule.id)return true;this.release(x.item.root);return false;});running=[];waiting=[];}
  if(rule.reentry!=='restart'&&(running.length>=rule.maxConcurrent||this.active.size>=this.maxRuns)&&waiting.length>=Math.min(rule.maxQueued,RULE_LIMITS.perRule)||this.pending.length>=RULE_LIMITS.signals)throw Error('Очередь повторного входа заполнена.');
  item.root.count++;this.pending.push(job);this.trace('admit',{seq:item.seq,ruleId:rule.id});this.launchPending();
 }
 launchPending(){if(this.closed||this.host.snapshot.paused)return;this.reconcile();
  for(let i=0;i<this.pending.length&&this.active.size<this.maxRuns;){const job=this.pending[i],{rule,item}=job,record=this.game.state.rules[rule.id]??={};
   if(job.machineRevision!==undefined&&this.game.state.mechanicMachines[rule.mechanicInstanceId]?.revision!==job.machineRevision||!this.validOwner(rule,item.context.sceneId)||rule.once&&record.fired||rule.cooldown>0&&item.publishedAt<(record.cooldownUntil??0)){this.pending.splice(i,1);this.release(item.root);continue;}
   if([...this.active.values()].filter(x=>x.rule.id===rule.id).length>=rule.maxConcurrent){i++;continue;}
   this.pending.splice(i,1);const id='reaction:'+this.generation+':'+item.seq+':'+rule.id,run={...job,id,cancelled:false};this.active.set(id,run);delete record.error;record.fired=true;record.cooldownUntil=this.game.state.clock+rule.cooldown;this.game.flights.add(id);this.trace('start',{seq:item.seq,ruleId:rule.id});
   this.execute(run).catch(e=>{if(!['CANCELLED','INSTANCE_STOPPED'].includes(e.message)){record.error=e.message;this.trace('error',{seq:item.seq,ruleId:rule.id,error:e.message});this.game.error(e);}this.cancelEvents(id);}).finally(()=>{this.active.delete(id);this.game.flights.delete(id);this.trace(run.cancelled?'cancel':'finish',{seq:item.seq,ruleId:rule.id,result:run.cancelled?'отменено':record.error?'ошибка':'завершено'});this.release(item.root);this.launchPending();});
  }
 }
 validOwner(rule,sceneId){return this.game.rules.some(r=>r.id===rule.id&&r.enabled!==false)&&ownerExists(this.host.project,rule.scope,rule.scope.kind==='project'?undefined:sceneId,rule.mechanicInstanceId)&&(rule.scope.kind==='project'||this.game.scene.id===sceneId);}
 assertRun(id){const run=this.active.get(id);if(!run||run.cancelled||run.machineRevision!==undefined&&this.game.state.mechanicMachines[run.rule.mechanicInstanceId]?.revision!==run.machineRevision||!this.validOwner(run.rule,run.item.context.sceneId)||this.closed||this.generation!==this.host.generation){if(run)this.cancelRun(run);throw Error('INSTANCE_STOPPED');}}
 async execute(run){const {rule,item}=run,context={...item.context,kind:'rule',scope:rule.scope,mechanicInstanceId:rule.mechanicInstanceId||item.context.mechanicInstanceId,bindings:{...item.context.bindings,...rule.bindings},args:{...item.payload,...rule.bindings},localState:rule.mechanicInstanceId?this.game.state.mechanicStates[rule.mechanicInstanceId]:undefined,reactionId:run.id,rootId:item.root.id,causeChain:[...(item.context.causeChain||[]),rule.id],timeSource:rule.timeSource};
  for(const [index,action]of run.actions.entries()){this.assertRun(run.id);while(this.host.snapshot.paused){await this.host.delay(5,this.generation);this.assertRun(run.id);}this.trace('action-start',{seq:item.seq,ruleId:rule.id,index,action});try{const result=await this.game.performRuleAction(action,item.payload,context);this.assertRun(run.id);this.trace('action-result',{seq:item.seq,ruleId:rule.id,index,result});}catch(e){this.trace('action-error',{seq:item.seq,ruleId:rule.id,index,error:e.message});throw e;}if(context.localState){const m=this.game.config.mechanics.find(m=>m.id===rule.mechanicInstanceId),d=descriptionFor(this.host.project,m?.definitionId,m?.definitionVersion);const errors=validateValue(d.stateSchema,context.localState,{project:this.host.project,scope:rule.scope});if(errors.length)throw Error(errors.map(e=>e.message).join(' '));}}
  // Native NONE/STARTED still return early; admission counts their actual work too.
  await this.host.waitReactionWork?.(run.id);this.assertRun(run.id);await this.game.flushTransition();
 }
 cancelEvents(id){if(this.generation!==this.host.generation)return;for(const i of Object.values(this.host.snapshot.instances||{}))if(i.operationContext?.reactionId===id&&['running','paused','held'].includes(i.status))this.host.controlInstance(i.id,'stop');}
 cancelRun(run){if(run.cancelled)return;run.cancelled=true;this.trace('cancel',{seq:run.item.seq,ruleId:run.rule.id,reason:'Запуск отменён жизненным циклом, переходом или повторным входом.'});this.cancelEvents(run.id);}
 reconcile(){for(const run of this.active.values())if(!this.validOwner(run.rule,run.item.context.sceneId))this.cancelRun(run);
  this.pending=this.pending.filter(job=>{if(this.validOwner(job.rule,job.item.context.sceneId))return true;this.release(job.item.root);return false;});
  for(const i of Object.values(this.host.snapshot.instances||{}))if(i.operationContext?.reactionId&&!this.validScope(i.operationContext.scope,i.sceneId))this.host.controlInstance(i.id,'stop');
 }
 validScope(scope,sceneId){return scope&&ownerExists(this.host.project,scope,scope.kind==='project'?undefined:sceneId)&&(scope.kind==='project'||this.game.scene.id===sceneId);}
 sceneChanged(sceneId){for(const raw of this.game.rules){const rule=normalizeRule(this.host.project,raw);if(rule.scope.kind!=='project'){const record=this.game.state.rules[rule.id];if(record){delete record.elapsed;delete record.inside;}}}for(const run of this.active.values())if(run.rule.scope.kind!=='project'&&run.item.context.sceneId!==sceneId)this.cancelRun(run);this.pending=this.pending.filter(job=>{if(job.rule.scope.kind==='project'||job.item.context.sceneId===sceneId)return true;this.release(job.item.root);return false;});}
 close(){for(const run of this.active.values())this.cancelRun(run);this.trace('stop');this.closed=true;for(const job of this.pending)this.release(job.item.root);this.pending=[];for(const item of this.signals){this.game.flights.delete('signal:'+item.seq);this.release(item.root);}this.signals=[];}
 resume(){this.schedule();this.launchPending();}
}
