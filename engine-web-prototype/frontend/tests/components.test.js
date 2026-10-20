import test from 'node:test';
import assert from 'node:assert/strict';
import {lockedDoors,componentFunction,objectSchema,doorProject} from './fixtures/componentDoor.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {gameSave,restoreGameSave} from '../src/gameSave.js';
import {createMechanicDefinition,addMechanicInstance} from '../src/mechanicDescriptions.js';
import {reference} from '../src/valueSchemas.js';
import {validateGameplay} from '../src/gameplayModel.js';
import {registerOperation} from '../src/operationRegistry.js';
import {exportAuthoringPackage,planPackageImport,readAuthoringPackage} from '../src/mechanicPackages.js';
import {inputRecords} from '../src/packageReferences.js';
import {compileMechanicScript} from '../src/mechanicScriptCompiler.js';
import {readProject} from '../src/projectFiles.js';
import {duplicateInstance} from '../src/authoredMechanics.js';
import {addMechanic} from '../src/mechanicsModel.js';

const runtime=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('Unexpected audio');}));
async function idle(rt){for(let n=0;n<400;n++){if(rt.game.ruleRuntime.idle&&!rt.game.flights.size)return;await new Promise(r=>setTimeout(r,3));}throw Error('Runtime did not settle');}
async function started(p){const rt=runtime();await rt.start(p);assert.equal(rt.snapshot.error,null);await idle(rt);return rt;}
const call=(instance,methodId,args={})=>({id:'sacura.component.call',version:1,parameters:{instance:reference('mechanic',instance),methodId,args}});

test('two LockedDoor instances dispatch bool to a method, keep defaults independent, and open native doors through the actual runtime',async()=>{
 const {p,a,b}=lockedDoors();assert.deepEqual(validateGameplay(p),[]);const rt=await started(p);
 try{
  assert.deepEqual(rt.game.state.mechanicStates[a.id],{IsLocked:true});assert.deepEqual(rt.game.state.mechanicStates[b.id],{IsLocked:false});
  assert.equal(await rt.game.execute(call(a.id,'interact')),false);assert.equal(rt.snapshot.world.doors['door-a'],undefined);
  assert.equal(await rt.game.execute(call(a.id,'set-locked',{value:false})),false);assert.equal(rt.snapshot.world.doors['door-a'],undefined);
  assert.equal(rt.game.config.mechanics.find(m=>m.id===a.id).values.InitiallyLocked,true);assert.equal(p.gameplay.mechanicDefinitions[0].parameters.properties.InitiallyLocked.default,true);
  assert.deepEqual(rt.game.state.mechanicStates[b.id],{IsLocked:false});
  assert.equal(await rt.game.execute(call(a.id,'interact')),true);assert.equal(rt.snapshot.world.doors['door-a'],'Открыть');assert.equal(rt.snapshot.world.doors['door-b'],undefined);
  await rt.game.trigger('interact',{objectId:'door-b'});await idle(rt);assert.equal(rt.snapshot.world.doors['door-b'],'Открыть');
  await assert.rejects(rt.game.callMechanic(a.id,{value:true}),/конкретный метод/);
  await assert.rejects(rt.game.callComponent(a.id,'set-locked',{value:1}),/boolean|тип/);
  await assert.rejects(rt.game.callComponent(a.id,'SetLocked',{value:true}),/Метод/);
  assert.deepEqual(rt.game.state.actors,{});assert.deepEqual(rt.game.state.weapons,{});
 }finally{rt.stop();}
});

test('same method IDs on unrelated components use their own graph/source, including scene and project owners',async()=>{
 const {p,a}=lockedDoors(),m=componentFunction(p,'OtherInteract','function main({owner}) { return owner.type; }',{}, {type:'string'});
 const d=createMechanicDefinition(p,{name:'Другой компонент',componentVersion:1,stateVersion:1,parameters:objectSchema({}),methods:{interact:{...m,name:'Interact'}}});
 const scene=addMechanicInstance(p,d.id,{kind:'scene',id:p.subscenes[0].id}),project=addMechanicInstance(p,d.id,{kind:'project',id:p.id});
 const rt=await started(p);try{assert.equal(await rt.game.callComponent(a.id,'interact'),false);assert.equal(await rt.game.callComponent(scene.id,'interact'),'scene');assert.equal(await rt.game.callComponent(project.id,'interact'),'project');}finally{rt.stop();}
});

test('persistent state, subscriptions and initialization are not recreated across repeat scene entry, save/load or restart',async()=>{
 const {p,d,a}=lockedDoors();p.variables.initializations=0;
 d.methods.initialize=componentFunction(p,'Initialize','function main({global}) { global.initializations += 1; return true; }');d.lifecycle={initialize:'initialize'};
 const rt=await started(p);try{
  assert.equal(rt.snapshot.variables.initializations,2);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,2);
  await rt.game.callComponent(a.id,'set-locked',{value:false});
  const state=rt.game.state.mechanicStates[a.id];await rt.game.enter();await rt.game.enter();assert.equal(rt.game.state.mechanicStates[a.id],state);assert.equal(rt.snapshot.variables.initializations,2);
  const save=JSON.parse(JSON.stringify(gameSave(rt)));await restoreGameSave(rt,save);await idle(rt);
  assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);assert.equal(rt.snapshot.variables.initializations,2);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,2);
  const old=rt.game;await rt.start(p);assert.equal(old.ruleRuntime.signalWaiters.size,0);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,2);assert.equal(rt.snapshot.variables.initializations,2);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,true);
 }finally{rt.stop();}
});

test('story BEFORE Events call an existing instance; advancing dialogue preserves state and subscriptions',async()=>{
 const {p,a}=lockedDoors();p.gameplay.storyPolicy.mode='story';p.gameplay.storyPolicy.mechanics='continuous';
 const scene=p.subscenes[0],event={id:'unlock-event',name:'Снять замок',groups:[{id:'unlock-group',mode:'SEQUENTIAL',actions:[{id:'unlock-action',type:'gameplay',target:'world',command:'sacura.component.call',operationVersion:1,value:call(a.id,'set-locked',{value:false}).parameters,duration:0}]}]};p.events.push(event);
 const first={id:'component-story-1',kind:'dialogue',text:'Первая',speaker:'',subsceneId:scene.id,bindings:[{id:'unlock-binding',eventId:event.id,hook:'BEFORE',join:'FLOW_END',overrides:{}}]},second={id:'component-story-2',kind:'dialogue',text:'Вторая',speaker:'',subsceneId:scene.id,bindings:[]};
 first.batches={BEFORE:[{id:'before-batch',mode:'SEQUENTIAL',bindingIds:['unlock-binding']}]};
 p.chapters=[{id:'component-chapter',name:'Глава',beats:[first,second]}];
 const rt=runtime();try{await rt.start(p,first.id);assert.equal(rt.snapshot.error,null);await idle(rt);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);const state=rt.game.state.mechanicStates[a.id];await rt.enter(second.id,rt.generation);assert.equal(rt.game.state.mechanicStates[a.id],state);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,2);}finally{rt.stop();}
});

test('unknown or missing state versions fail before replacing a running world; restored values outrank instance defaults',async()=>{
 const {p,a}=lockedDoors(),rt=await started(p);try{
  await rt.game.callComponent(a.id,'set-locked',{value:false});const save=gameSave(rt),current=rt.game;
  for(const version of [0,2,undefined]){const bad=structuredClone(save);bad.world.game.componentLifecycles[a.id].stateVersion=version;await assert.rejects(restoreGameSave(rt,bad),/миграции/);assert.equal(rt.game,current);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);}
  await restoreGameSave(rt,save);assert.equal(await rt.game.callComponent(a.id,'interact'),true);
 }finally{rt.stop();}
});

test('cancelled task cannot mutate after late await, while another task on the same component completes',async()=>{
 const {p,d,a,b}=lockedDoors();let release,entered;const gate=new Promise(r=>release=r),waiting=new Promise(r=>entered=r);
 registerOperation({id:'test.component.delay',version:1,contexts:['function'],editor:{label:'Test delay'},parameters:objectSchema({}),result:{type:'void'}},async()=>{entered();await gate;});
 d.methods.late=componentFunction(p,'Late','async function main({state,engine}) { await engine.operation("test.component.delay",{}); state.IsLocked = false; await engine.operation("sacura.door.write",{object:{kind:"reference",type:"object",mode:"context",role:"self"},open:true}); return true; }');
 const rt=await started(p);try{
  const task=rt.game.startComponentTask(a.id,'late');await waiting;task.cancel();
  assert.equal(await rt.game.callComponent(a.id,'set-locked',{value:true}),true);assert.equal(await rt.game.callComponent(b.id,'interact'),true);
  await assert.rejects(task.promise,/INSTANCE_STOPPED/);release();assert.equal(task.status,'cancelled');await idle(rt);
  assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,true);assert.equal(rt.snapshot.world.doors['door-a'],undefined);assert.equal(rt.snapshot.world.doors['door-b'],'Открыть');assert.equal(rt.game.componentTasks.size,0);
 }finally{release();rt.stop();}
});

test('disable/enable and destroy remove owned subscriptions and awaiting tasks without stopping other instances',async()=>{
 const {p,d,a,b}=lockedDoors();d.methods.wait=componentFunction(p,'Wait','async function main({engine}) { await engine.waitSignal("victory"); return true; }');
 const rt=await started(p);try{
  const task=rt.game.startComponentTask(a.id,'wait');while(!rt.game.ruleRuntime.signalWaiters.get('victory'))await new Promise(r=>setTimeout(r,2));
  await rt.game.setComponentEnabled(a.id,false);await assert.rejects(task.promise,/INSTANCE_STOPPED/);assert.equal(rt.game.ruleRuntime.signalWaiters.has('victory'),false);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,1);
  await rt.game.trigger('interact',{objectId:'door-b'});await idle(rt);assert.equal(rt.snapshot.world.doors['door-b'],'Открыть');
  await rt.game.setComponentEnabled(a.id,true);await rt.game.setComponentEnabled(a.id,true);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,2);
  await rt.game.destroyComponent(a.id);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,1);assert.throws(()=>rt.game.startComponentTask(a.id,'open'),/уничтожен/);
  const save=gameSave(rt);await restoreGameSave(rt,save);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,1);assert.throws(()=>rt.game.startComponentTask(a.id,'open'),/уничтожен/);assert.equal(await rt.game.callComponent(b.id,'open'),true);
 }finally{rt.stop();}
});

test('component references and event source/target reach a different exact instance, with no fallback for missing links',async()=>{
 const {p,d,a,b}=lockedDoors();d.references=objectSchema({other:{type:'reference',referenceType:'mechanic'}});a.references.other=reference('mechanic',b.id);b.references.other=reference('mechanic',a.id);
 d.methods.unlockOther=componentFunction(p,'UnlockOther','async function main({engine,refs,context}) { await engine.invoke(refs.other,"set-locked",{value:false}); return context.target.id; }',{}, {type:'string'});
 const rt=await started(p);try{
  assert.equal(await rt.game.callComponent(b.id,'unlockOther',{}, {source:{type:'object',id:'door-a'},target:{type:'object',id:'door-b'}}),'door-b');assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);
  rt.game.config.mechanics.find(m=>m.id===a.id).references.other=reference('mechanic','missing');assert.throws(()=>rt.game.startComponentTask(a.id,'unlockOther'),/недоступна/);assert.equal(rt.game.state.mechanicStates[b.id].IsLocked,false);
 }finally{rt.stop();}
});

test('input and widget dispatch use the same exact method without running the entire component',async()=>{
 const {p,a,b}=lockedDoors();p.input.actions.push({id:'unlock-a',name:'Снять замок A',valueType:'boolean',behavior:'operation',trigger:'pressed',operation:call(a.id,'set-locked',{value:false})});
 p.input.contexts.find(c=>c.id==='gameplay').bindings.push({id:'unlock-binding',actionId:'unlock-a',device:'external',control:'unlock',scale:1});
 const rt=await started(p);try{
  await rt.inputAction({actionId:'unlock-a',phase:'triggered',value:true,device:'external'});assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);assert.equal(rt.snapshot.world.doors['door-a'],undefined);
  await rt.uiAction('gameplay',{id:'open-b',command:'sacura.component.call',operationVersion:1,value:call(b.id,'open').parameters});assert.equal(rt.snapshot.world.doors['door-b'],'Открыть');assert.equal(rt.snapshot.world.doors['door-a'],undefined);
 }finally{rt.stop();}
});

test('stopping a native Event cancels its awaiting component invocation without cancelling another invocation',async()=>{
 const {p,d,a,b}=lockedDoors();d.methods.wait=componentFunction(p,'WaitInEvent','async function main({engine,state}) { await engine.waitSignal("victory"); state.IsLocked = false; return true; }');
 p.events.push({id:'waiting-event',name:'Ожидающий Event',owner:'GameSession',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'waiting-group',actions:[{id:'waiting-action',target:'world',type:'gameplay',command:'sacura.component.call',operationVersion:1,value:call(a.id,'wait').parameters,duration:0}]}]});
 const rt=await started(p);try{
  const work=rt.binding({id:'waiting-binding',eventId:'waiting-event',join:'FLOW_END',overrides:{}},rt.generation);
  while(!rt.game.ruleRuntime.signalWaiters.get('victory'))await new Promise(r=>setTimeout(r,2));
  const native=Object.values(rt.snapshot.instances).find(i=>i.eventId==='waiting-event');rt.controlInstance(native.id,'stop');await work;
  assert.equal(rt.game.ruleRuntime.signalWaiters.has('victory'),false);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,true);assert.equal(await rt.game.callComponent(b.id,'open'),true);
 }finally{rt.stop();}
});

test('child work belongs to its invocation: foreign cancel is refused and cancelling one parent preserves the other child',async()=>{
 const {p,d,a}=lockedDoors();componentFunction(p,'ChildWork','async function main({engine,state}) { await engine.waitSignal("victory"); state.IsLocked = false; return true; }');
 d.methods.parent=componentFunction(p,'ParentWork','async function main({engine}) { engine.start("ChildWork",{}); await engine.waitSignal("start"); return true; }');
 d.methods.cancel=componentFunction(p,'ForeignCancel','function main({engine,args}) { engine.cancel(args.handle); return true; }',{handle:objectSchema({kind:{type:'string'},id:{type:'string'}})});
 const rt=await started(p);try{
  const first=rt.game.startComponentTask(a.id,'parent'),second=rt.game.startComponentTask(a.id,'parent');
  while((rt.game.ruleRuntime.signalWaiters.get('victory')?.size||0)<2)await new Promise(r=>setTimeout(r,2));
  const child=[...rt.game.graphRuns.values()].find(w=>w.componentTaskId===first.id);await assert.rejects(rt.game.callComponent(a.id,'cancel',{handle:{kind:'work',id:child.id}}),/другой задаче/);
  first.cancel();await assert.rejects(first.promise,/INSTANCE_STOPPED/);assert.equal(rt.game.ruleRuntime.signalWaiters.get('victory').size,1);assert.equal(rt.game.ruleRuntime.signalWaiters.get('start').size,1);
  await rt.game.trigger('victory');await rt.game.trigger('start');await second.promise;await idle(rt);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);
 }finally{rt.stop();}
});

test('method graph uses the same state schema',async()=>{
 const {p,d,a}=lockedDoors(),f=p.functions.find(f=>f.id===d.methods['set-locked'].logic.functionId);delete f.mechanicScript;
 f.nodes=[{id:'set',kind:'set',path:'state.IsLocked',value:{op:'get',path:'args.value'},next:'return'},{id:'return',kind:'return',value:{op:'get',path:'state.IsLocked'}}];f.entry='set';
 const rt=await started(p);try{assert.equal(await rt.game.callComponent(a.id,'set-locked',{value:false}),false);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);}finally{rt.stop();}
});

test('graph method invokes another exact method through self using the common operation',async()=>{
 const {p,d,a}=lockedDoors(),f=p.functions.find(f=>f.id===d.methods.interact.logic.functionId);delete f.mechanicScript;f.entry='branch';
 f.nodes=[{id:'branch',kind:'branch',condition:{op:'get',path:'state.IsLocked'},trueNext:'locked',falseNext:'open'},
  {id:'locked',kind:'return',value:false},{id:'open',kind:'operation',call:{id:'sacura.component.call',version:1,parameters:{instance:{op:'get',path:'self'},methodId:'open',args:{}}},next:'result'},
  {id:'result',kind:'return',value:{op:'node',nodeId:'open'}}];
 const rt=await started(p);try{assert.equal(await rt.game.callComponent(a.id,'interact'),false);await rt.game.callComponent(a.id,'set-locked',{value:false});assert.equal(await rt.game.callComponent(a.id,'interact'),true);assert.equal(rt.snapshot.world.doors['door-a'],'Открыть');}finally{rt.stop();}
});

test('void methods and schema failures pass through the common dispatcher with no invalid state writes',async()=>{
 const {p,d,a}=lockedDoors();d.methods.void=componentFunction(p,'VoidMethod','function main({state}) { state.IsLocked = false; }',{}, {type:'void'});
 d.methods.invalid=componentFunction(p,'InvalidState','function main({state}) { state.IsLocked = "bad"; return true; }');
 const rt=await started(p);try{assert.equal(await rt.game.execute(call(a.id,'void')),undefined);await assert.rejects(rt.game.callComponent(a.id,'invalid'),/тип/);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);}finally{rt.stop();}
});

test('lifecycle hooks run in order, restore does not initialize, and disabled state survives load',async()=>{
 const {p,d,a,b}=lockedDoors();p.variables.lifecycle='';
 for(const phase of ['initialize','restore','enable','disable','destroy'])d.methods[phase]=componentFunction(p,'Lifecycle'+phase,'function main({global,owner}) { global.lifecycle += owner.id + ":'+phase+';"; return true; }');
 d.lifecycle=Object.fromEntries(['initialize','restore','enable','disable','destroy'].map(k=>[k,k]));
 const rt=await started(p);try{
  assert.equal(rt.snapshot.variables.lifecycle,'door-a:initialize;door-a:enable;door-b:initialize;door-b:enable;');
  await rt.game.setComponentEnabled(a.id,false);assert.match(rt.snapshot.variables.lifecycle,/door-a:disable;$/);const save=gameSave(rt);
  await restoreGameSave(rt,save);assert.match(rt.snapshot.variables.lifecycle,/door-b:restore;door-b:enable;$/);assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,1);
  await rt.game.setComponentEnabled(a.id,true);assert.match(rt.snapshot.variables.lifecycle,/door-a:restore;door-a:enable;$/);await rt.game.destroyComponent(a.id);assert.match(rt.snapshot.variables.lifecycle,/door-a:destroy;$/);
  assert.equal(rt.snapshot.variables.lifecycle.match(/initialize/g).length,2);assert.equal(await rt.game.callComponent(b.id,'interact'),true);
 }finally{rt.stop();}
});

test('native timer ownership isolates components on the same object; disabling one preserves the other timer',async()=>{
 const {p,d,a}=lockedDoors(),other=addMechanicInstance(p,d.id,a.scope);
 d.methods.timer=componentFunction(p,'Timer','async function main({engine,owner}) { await engine.operation("sacura.timer.start",{owner,name:"Pulse",seconds:10,repeat:true}); return true; }');
 d.methods.stopTimer=componentFunction(p,'StopTimer','async function main({engine,owner}) { await engine.operation("sacura.timer.stop",{owner,name:"Pulse"}); return true; }');
 const rt=await started(p);try{
  await rt.game.callComponent(a.id,'timer');await rt.game.callComponent(other.id,'timer');assert.equal(Object.keys(rt.game.state.timers).length,2);
  await rt.game.callComponent(a.id,'stopTimer');assert.equal(Object.keys(rt.game.state.timers).length,1);assert.equal(Object.values(rt.game.state.timers)[0].mechanicInstanceId,other.id);
  await rt.game.callComponent(a.id,'timer');await rt.game.setComponentEnabled(a.id,false);assert.equal(Object.keys(rt.game.state.timers).length,1);assert.equal(Object.values(rt.game.state.timers)[0].mechanicInstanceId,other.id);
 }finally{rt.stop();}
});

test('native health and component methods read and write one canonical native state through adapters',async()=>{
 const {p,d,a}=lockedDoors();addMechanic(p,'health',a.scope);
 d.methods.heal=componentFunction(p,'Heal','async function main({engine,owner}) { await engine.operation("sacura.health.heal",{owner,amount:7}); const health = await engine.operation("sacura.health.read",{owner}); return health.hp; }',{}, {type:'number'});
 const rt=await started(p);try{
  const initial=rt.game.state.actors['door-a'].hp;await rt.game.execute({id:'sacura.health.damage',version:1,parameters:{owner:reference('object','door-a'),amount:12}});
  assert.equal(await rt.game.callComponent(a.id,'heal'),initial-5);assert.equal(rt.game.state.actors['door-a'].hp,initial-5);assert.deepEqual(Object.keys(rt.game.state.mechanicStates[a.id]),['IsLocked']);
 }finally{rt.stop();}
});

test('component event subscriptions filter the exact emitter instance and are restored once',async()=>{
 const {p,d,a,b}=lockedDoors();d.references=objectSchema({watched:{type:'reference',referenceType:'mechanic'}});a.references.watched=reference('mechanic',b.id);b.references.watched=reference('mechanic',b.id);
 d.methods.onOpened=componentFunction(p,'OnOpened','function main({state,context}) { if(context.source.type !== "mechanic") throw Error("Wrong source"); state.IsLocked = false; return true; }');
 d.subscriptions.push({id:'observe',signalId:d.events.opened.signalId,methodId:'onOpened',match:'component',reference:'watched'});
 const rt=await started(p);try{
  await rt.game.callComponent(a.id,'open');await idle(rt);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,true);
  await rt.game.callComponent(b.id,'open');await idle(rt);assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,false);
  const save=gameSave(rt);await restoreGameSave(rt,save);assert.equal(rt.game.ruleRuntime.signalWaiters.get(d.events.opened.signalId).size,2);
 }finally{rt.stop();}
});

test('new component data survives project read and local independent copies keep method IDs stable',async()=>{
 const {p,d,a}=lockedDoors(),copy=duplicateInstance(p,a,{independent:true}),next=readProject(JSON.stringify(p));
 const copied=next.gameplay.mechanicDefinitions.find(x=>x.id===copy.definitionId);assert.deepEqual(Object.keys(copied.methods),Object.keys(d.methods));assert.notEqual(copied.methods.open.logic.functionId,d.methods.open.logic.functionId);
 const rt=await started(next);try{await rt.game.callComponent(copy.id,'set-locked',{value:false});assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,true);}finally{rt.stop();}
});

test('component packages A → B → C preserve methods, initialization, source, dependencies and independent edits; import leaves JS disabled',async()=>{
 const {p,a}=lockedDoors(),meta={name:'Компонент двери',author:'A',license:'CC0'},pkg=await exportAuthoringPackage(p,a,meta);
 assert.ok(pkg.compatibility.requiredCapabilities.some(c=>c.id==='mechanic.components'));assert.equal(pkg.content.functions.length,3);assert.equal(pkg.content.gameplay.signals.length,1);
 const bind=(pkg,p)=>Object.fromEntries(pkg.inputs.map(i=>[i.id,inputRecords(p,i)[0]?.id]));
 const b=doorProject('B'),before=JSON.stringify(b),plan=await planPackageImport(b,pkg,bind(pkg,b));assert.equal(JSON.stringify(b),before);let imported=plan.apply(b);
 const m=imported.gameplay.mechanics.find(m=>m.definitionId),d=imported.gameplay.mechanicDefinitions.find(d=>d.id===m.definitionId);
 assert.equal(imported.functions.every(f=>f.mechanicScript.approved===false),true);assert.deepEqual(Object.keys(d.methods),['set-locked','interact','open']);
 const disabled=await started(imported);try{await assert.rejects(disabled.game.callComponent(m.id,'open'),/не включён/);assert.equal(disabled.snapshot.world.doors['door-a'],undefined);}finally{disabled.stop();}
 for(const f of imported.functions)f.mechanicScript=compileMechanicScript(imported,f.mechanicScript.source,f.mechanicScript.dependencies);
 const f=imported.functions.find(f=>f.id===d.methods['set-locked'].logic.functionId);f.mechanicScript=compileMechanicScript(imported,f.mechanicScript.source.replace('state.IsLocked = args.value','state.IsLocked = !args.value'),f.mechanicScript.dependencies);
 const variant=await exportAuthoringPackage(imported,m,{...meta,author:'B'}),c=doorProject('C'),last=(await planPackageImport(c,variant,bind(variant,c))).apply(c),mc=last.gameplay.mechanics.find(m=>m.definitionId);
 for(const f of last.functions)f.mechanicScript=compileMechanicScript(last,f.mechanicScript.source,f.mechanicScript.dependencies);
 const rt=await started(last);try{assert.equal(await rt.game.callComponent(mc.id,'set-locked',{value:true}),false);assert.equal(await rt.game.callComponent(mc.id,'interact'),true);assert.equal(rt.snapshot.world.doors['door-a'],'Открыть');assert.equal(rt.game.ruleRuntime.signalWaiters.get('interact').size,1);}finally{rt.stop();}
 assert.match(p.functions.find(f=>f.id==='SetLocked').mechanicScript.source,/= args.value/);assert.ok(variant.metadata.sources.length);
 for(const mutate of [d=>d.methods.open.logic.functionId='missing',d=>d.componentVersion=2]){const broken=structuredClone(pkg);mutate(broken.content.gameplay.mechanicDefinitions[0]);await assert.rejects(readAuthoringPackage(broken),/зависимость|недоступна/);assert.equal(JSON.stringify(b),before);}
});

test('a delivered component signal starts receiver-owned work that outlives cancellation of the publishing Event',async()=>{
 const {p,d,a}=lockedDoors();
 d.methods.publish=componentFunction(p,'PublishThenWait','async function main({engine}) { await engine.emit("opened",{}); await engine.waitSignal("start"); return true; }');
 const observed=componentFunction(p,'ReceiverOwnedWait','async function main({engine,state}) { await engine.waitSignal("victory"); state.finished=true; return true; }');
 const watcher=createMechanicDefinition(p,{name:'Independent observer',componentVersion:1,stateVersion:1,parameters:objectSchema({}),stateSchema:objectSchema({finished:{type:'boolean',default:false}}),references:objectSchema({sender:{type:'reference',referenceType:'mechanic'}}),methods:{receive:observed},subscriptions:[{id:'delivered',signalId:d.events.opened.signalId,methodId:'receive',match:'component',reference:'sender'}]});
 const receiver=addMechanicInstance(p,watcher.id,{kind:'object',id:'door-b'});receiver.references.sender=reference('mechanic',a.id);
 p.events.push({id:'publishing-event',name:'Publish and wait',owner:'GameSession',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'publishing-group',actions:[{id:'publishing-action',target:'world',type:'gameplay',command:'sacura.component.call',operationVersion:1,value:call(a.id,'publish').parameters,duration:0}]}]});
 const rt=await started(p);try{
  const work=rt.binding({id:'publishing-binding',eventId:'publishing-event',join:'FLOW_END',overrides:{}},rt.generation);
  for(let n=0;n<400&&!rt.game.ruleRuntime.signalWaiters.has('victory');n++)await new Promise(r=>setTimeout(r,2));
  assert.equal(rt.game.ruleRuntime.signalWaiters.has('victory'),true);
  const event=Object.values(rt.snapshot.instances).find(i=>i.eventId==='publishing-event');rt.controlInstance(event.id,'stop');await work;
  await rt.game.trigger('victory');await idle(rt);
  assert.equal(rt.game.state.mechanicStates[receiver.id].finished,true,'receiver owns its subscription task after delivery');
  assert.equal(rt.game.state.mechanicStates[a.id].IsLocked,true);
 }finally{rt.stop();}
});
