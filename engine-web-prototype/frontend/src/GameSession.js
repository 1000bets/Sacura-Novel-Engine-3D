import {authoredRules,eventTargetOverrides} from './authoredMechanics.js';
import {RuleRuntime} from './RuleRuntime.js';
import {ownerExists} from './ruleModel.js';
import {executeOperation,operationDefinition} from './operationRegistry.js';
import {descriptionFor} from './mechanicDescriptions.js';
import {schemaDefaults,validateValue,resolveReference} from './valueSchemas.js';
import {validateCapabilityData,attachCapabilities,initializeCapabilities,updateCapabilities,enterCapabilities,syncCapabilities,cleanupCapabilities,disposeCapabilities,hasCapabilityCommand,capabilityCommand} from './capabilityRegistry.js';
import {syncCapabilityBindings} from './capabilityBindings.js';
import {ensureGameplay,gameCommandExtension} from './gameplayModel.js';
import {allBeats,sceneFor} from './studioModel.js';
import {resolvedPosition,isObjectInScene} from './sceneEditing.js';
import {bodyBounds,rayBox,clamp} from './gamePhysics.js';
import {evaluateExpression,callBlueprint} from './blueprintFunctions.js';
export function assertRuntimeDefinitions(project){for(const m of project.gameplay?.mechanics||[])if(m.definitionId){const d=descriptionFor(project,m.definitionId,m.definitionVersion);if(!d||d.machine?.version!==undefined&&d.machine.version!==1)throw Error('Определение или версия механики недоступна: '+m.definitionId+'. Данные сохранены.');}const issues=validateCapabilityData(project);if(issues.length)throw Error(issues.join(' · '));}
export class GameSession {
 constructor(host){this.host=host;this.generation=host.generation;this.worldState=host.snapshot.world;this.config=ensureGameplay(host.project).gameplay;this.started=false;this.flights=new Set();this.direction=[0,0,0];this.ruleRuntime=new RuleRuntime(this);attachCapabilities(this);}
 async flushTransition(){this.host.assert(this.generation);if(this.pendingTransition){const id=this.pendingTransition;this.pendingTransition=null;await this.host.enter(id,this.host.generation);return true;}return false;}
 get state(){return this.world.game;}
 get world(){if(this.generation===this.host.generation)this.worldState=this.host.snapshot.world;return this.worldState;}
 get scene(){return this.host.project.subscenes.find(s=>s.id===this.world.location)||sceneFor(this.host.project,this.host.snapshot.beatId);}
 get controlAllowed(){const beat=allBeats(this.host.project).find(b=>b.id===this.host.snapshot.beatId);return beat?.kind!=='dialogue'||this.config.storyPolicy?.dialogueControl!=='suspend';}
 get objects(){return this.host.project.objects.filter(o=>o.active!==false&&isObjectInScene(o,this.scene));}
 object(id){return this.objects.find(o=>o.id===id);}
 position(id){const o=this.object(id);return o?resolvedPosition(o,this.world,this.scene.kind):null;}
 initialize(){assertRuntimeDefinitions(this.host.project);this.world.game={capabilitySchemaVersion:1,authoringStateVersion:1,clock:0,yaw:0,pitch:0,actors:{},behaviors:{},disabled:{},directions:{},weapons:{},selectedWeapons:{},inventories:{},resources:{},bodies:{},timers:{},rules:{},projectiles:[],tracers:[],message:'',mechanicStates:{}};initializeCapabilities(this);authoredRules(this);this.sync();}
 get controlledId(){return this.config.controller.ownerId||'';}
 enter(){this.destination=null;this.route=null;this.pendingPickup=null;this.direction=[0,0,0];for(const r of this.config.rules)if(this.state.rules[r.id])delete this.state.rules[r.id].inside;this.state.active=true;enterCapabilities(this);if(!this.started){this.started=true;this.sync();this.post('start');}this.sync();}
 sync(){if(this.generation!==this.host.generation||!this.host.running||!this.state)return;syncCapabilities(this);syncCapabilityBindings(this);}
 error(e){if(this.generation!==this.host.generation||!this.host.running)return;this.state.message=e.message;this.host.snapshot.hint=e.message;this.host.emit();}
 async trackWork(work){const id='work:'+((this.workSequence||0)+1);this.workSequence=(this.workSequence||0)+1;this.flights.add(id);try{return await work();}finally{this.flights.delete(id);}}
 callFunction(id,args={},context={}){return this.trackWork(()=>this.performFunction(id,args,context));}
 performFunction(id,args={},context={}){const token=this.host.generation;return callBlueprint(this.host.project,id,args,this.host.snapshot.variables,c=>{this.host.assert(token);this.assertReaction(context);return this.command(c.command||c.type,c.value,{...context,kind:'function',version:c.operationVersion??1});},{assertActive:()=>{this.host.assert(token);this.assertReaction(context);},itemCount:(id,owner)=>this.count(id,owner||context.ownerId),localState:context.localState,beforeStep:context.reactionId?async()=>{while(this.host.snapshot.paused)await this.host.delay(5,token);this.assertReaction(context);}:undefined,parameterContext:{...context,stateSchema:context.mechanicInstanceId?descriptionFor(this.host.project,this.config.mechanics.find(m=>m.id===context.mechanicInstanceId)?.definitionId,this.config.mechanics.find(m=>m.id===context.mechanicInstanceId)?.definitionVersion)?.stateSchema:undefined,project:this.host.project,allowResolvedReferences:true}});}
 operationContext(context={}){const token=this.host.generation,plain={nativeRunId:context.nativeRunId,scope:context.scope,bindings:context.bindings,source:context.source,target:context.target,initiator:context.initiator,sceneId:context.sceneId,mechanicInstanceId:context.mechanicInstanceId,reactionId:context.reactionId,rootId:context.rootId,causeChain:context.causeChain,timeSource:context.timeSource,sourceId:context.sourceId,targetId:context.targetId};return {...context,sourceId:context.source?.type==='object'?context.source.id:context.sourceId,targetId:context.target?.type==='object'?context.target.id:context.targetId,project:this.host.project,variables:this.host.snapshot.variables,world:this.world,gameState:this.state,capabilities:this,assertActive:()=>{this.host.assert(token);this.assertReaction(context);},evaluate:value=>evaluateExpression(value,{global:this.host.snapshot.variables,args:context.args||{},local:context.localState||{},reference:value=>resolveReference(value,value.type,{...context,project:this.host.project}),itemCount:(id,owner)=>this.count(id,owner||context.ownerId)}),emit:()=>this.host.emit(),signal:async(id,payload)=>{await this.trigger(id,payload,undefined,plain);},enterStory:beatId=>this.host.enter(beatId,token),enterScene:sceneId=>this.host.enterScene(sceneId,token),runEvent:(eventId,join)=>this.command('runEvent',{eventId,join},plain)};}

 async execute(call,context={}){return this.trackWork(()=>executeOperation(call,this.operationContext({kind:'rule',...context})));}
 async callMechanic(instanceId,payload,context={}){const m=this.config.mechanics.find(m=>m.id===instanceId),d=descriptionFor(this.host.project,m?.definitionId,m?.definitionVersion);if(!d)throw Error('Определение механики недоступно.');
  const args={...schemaDefaults(d.parameters),...m.values},scope={...context,project:this.host.project,scope:m.scope,bindings:args,parameters:args,sourceId:context.source?.type==='object'?context.source.id:context.sourceId,targetId:context.target?.type==='object'?context.target.id:context.targetId};
  const errors=validateValue(d.parameters,args,scope);if(errors.length)throw Error(errors.map(e=>e.message).join(' '));
  this.state.mechanicStates??={};const localState=this.state.mechanicStates[m.id]??=structuredClone({...schemaDefaults(d.stateSchema),...d.initialState});
  const result=await this.callFunction(d.logic.functionId,args,{...scope,sceneId:this.scene.id,mechanicInstanceId:instanceId,localState});const invalid=validateValue(d.stateSchema,localState,scope);if(invalid.length)throw Error(invalid.map(e=>e.message).join(' '));return result;
 }
 get rules(){return [...authoredRules(this),...this.config.rules.map(r=>{if(!r.mechanicInstanceId)return r;const m=this.config.mechanics.find(m=>m.id===r.mechanicInstanceId),d=descriptionFor(this.host.project,m?.definitionId,m?.definitionVersion);return d?{...r,trigger:d.when,signal:r.signal?{...r.signal,type:d.when}:undefined}:r;})];}
 async trigger(type,payload={},ruleId,context={}){return this.ruleRuntime.publish(type,payload,ruleId,context);}
 post(...args){const task=this.trigger(...args);task.catch(e=>this.error(e));return task;}
 assertReaction(context={}){this.host.assert(this.generation);if(context.scope&&(!ownerExists(this.host.project,context.scope,context.scope.kind==='project'?undefined:context.sceneId,context.mechanicInstanceId)||context.scope.kind!=='project'&&context.sceneId&&context.sceneId!==this.world.location))throw Error('INSTANCE_STOPPED');if(context.capabilityOwner&&(!(context.capabilityOwners||[context.capabilityOwner]).every(owner=>ownerExists(this.host.project,owner,owner.kind==='project'?undefined:context.capabilitySceneId))||context.capabilitySceneId&&context.capabilitySceneId!==this.world.location))throw Error('INSTANCE_STOPPED');if(context.reactionId)this.ruleRuntime.assertRun(context.reactionId);}
 async performRuleAction(action,payload,context){
  if(action.kind==='operation')return this.execute(action.call,context);
  if(action.kind==='function')return this.callFunction(action.functionId,{...payload,...context.args,...this.operationContext(context).evaluate(action.args||{})},context);
  if(action.kind==='mechanic')return this.callMechanic(action.instanceId,payload,{...context,mechanicInstanceId:action.instanceId});
  if(action.kind==='story')return this.execute({id:'sacura.story.enter',version:1,parameters:{beat:{kind:'reference',type:'beat',mode:'direct',id:action.beatId}}},context);
  if(action.kind==='event')return this.command('runEvent',{eventId:action.eventId,join:action.join||'FLOW_END',overrides:action.overrides,actionOverrides:{...action.actionOverrides,...eventTargetOverrides(this.host.project,action.targetBindings,context)}},context);
  throw Error('Неизвестное действие правила.');
 }
 dispose(){if(this.disposed)return;this.disposed=true;const errors=disposeCapabilities(this);for(const error of errors)this.ruleRuntime.trace('cleanup-error',error);this.ruleRuntime.close();this.destination=null;this.route=null;this.pendingPickup=null;this.pendingTransition=null;this.direction=[0,0,0];}
 cleanupOwners(){this.ruleRuntime.reconcile();cleanupCapabilities(this);for(const i of Object.values(this.host.snapshot.instances||{})){const c=i.operationContext;if(c?.capabilityOwner&&['running','paused','held'].includes(i.status)&&(!(c.capabilityOwners||[c.capabilityOwner]).every(owner=>ownerExists(this.host.project,owner,owner.kind==='project'?undefined:c.capabilitySceneId))||c.capabilitySceneId&&c.capabilitySceneId!==this.world.location))this.host.controlInstance(i.id,'stop');}for(const [id,t]of Object.entries(this.state?.timers||{}))if(t.owner&&!ownerExists(this.host.project,t.owner,t.owner.kind==='project'?undefined:t.sceneId,t.mechanicInstanceId)||t.owner?.kind!=='project'&&t.sceneId&&t.sceneId!==this.scene.id)delete this.state.timers[id];}

 colliders(){const api=this.host.physicsApi?.current;if(api?.sceneId()===this.scene.id&&api.colliders)return api.colliders();return this.objects.filter(o=>o.collision?.enabled&&this.world.visible[o.id]!==false).map(o=>({id:o.id,box:bodyBounds(o,this.position(o.id),this.scene.id)}));}
 raycast(origin,direction,range,exclude=[]){const api=this.host.physicsApi?.current;if(api?.sceneId()===this.scene.id&&api.raycast)return api.raycast(origin,direction,range,exclude);return this.objects.filter(o=>!exclude.includes(o.id)&&this.world.visible[o.id]!==false&&(o.collision?.enabled||this.state.actors[o.id])).map(o=>{const distance=rayBox(origin,direction,bodyBounds(o,this.position(o.id),this.scene.id),range);return distance===null?null:{id:o.id,distance,point:origin.map((v,i)=>v+direction[i]*distance)};}).filter(Boolean).sort((a,b)=>a.distance-b.distance)[0]||null;}

 command(command,v={},context={}){this.host.assert(this.generation);
  if(operationDefinition(command,context.version)||command?.includes('.')&&!gameCommandExtension(command))return this.execute({id:command,version:context.version??1,parameters:v},{kind:'event',...context});
  if(typeof v!=='object'||v===null)v={amount:Number(v)};
  const extension=gameCommandExtension(command);if(extension)return extension(this,v,context);
  if(hasCapabilityCommand(command))return capabilityCommand(this,command,v,context);
  const native={
   goTo:()=>{const beat=allBeats(this.host.project).find(b=>b.id===v.beatId);if(!beat||!['dialogue','gameplay','gate','choice','end','branch','set-variable','merge'].includes(beat.kind))throw Error('Нода продолжения не найдена.');this.pendingTransition=v.beatId;},
   runEvent:()=>{const token=this.host.generation,binding={id:'input:function:'+v.eventId+':'+(++this.sequence||(this.sequence=1)),eventId:v.eventId,join:v.join||'FLOW_END',overrides:v.overrides||{},actionOverrides:v.actionOverrides||{},operationContext:context};return this.host.binding(binding,token).then(()=>{this.host.assert(token);if(this.host.snapshot.states[binding.id]==='stopped')throw Error('INSTANCE_STOPPED');});},
   callFunction:()=>this.callFunction(v.functionId,v.args||{},context),save:()=>this.host.saveGame(),load:()=>this.host.loadGame()
  };if(!Object.hasOwn(native,command))throw Error('Неизвестная игровая команда: '+command);return native[command]();
 }

 tick(dt){if(this.generation!==this.host.generation||!this.host.running)return;this.cleanupOwners();this.ruleRuntime.resume();if(!this.host.running||this.host.snapshot.paused||['EDIT','ERROR'].includes(this.host.snapshot.phase)||this.host.snapshot.phase==='ENDING'&&this.config.storyPolicy?.ending!=='continue')return;
  dt=clamp(Number(dt)||0,0,.05);if(this.state)this.state.clock+=dt;
  if(!this.state?.active||!this.host.snapshot.ready&&(this.config.storyPolicy?.mechanics||this.config.activation)==='gameplay-node')return;
  const published=[];updateCapabilities(this,dt,(...args)=>published.push(args));this.sync();for(const args of published)this.post(...args);this.host.emit();
 }

 fireRule(rule,publish=(...args)=>this.post(...args)){const objectId=rule.objectId|| (rule.scope?.kind==='object'?rule.scope.id:undefined),context={...(objectId?{sourceId:objectId}:rule.scope?{source:{type:rule.scope.kind,id:rule.scope.id}}:{}),...(rule.mechanicInstanceId?{mechanicInstanceId:rule.mechanicInstanceId}:{})};return publish(rule.signal?.type||rule.trigger,{...(objectId?{objectId}:{})},rule.id,context);}
}
