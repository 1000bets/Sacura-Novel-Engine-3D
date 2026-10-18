import {makeAction} from '../src/model.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {addMechanic,bindDoorKey,removeMechanic} from '../src/mechanicsModel.js';
import {createFromRule,createFromFunction,createFromEvent,duplicateInstance,updateDefinitionParameters,syncFunctionDefinitions} from '../src/authoredMechanics.js';
import {addMechanicInstance,createMechanicDefinition,descriptionFor} from '../src/mechanicDescriptions.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {reference,validateValue} from '../src/valueSchemas.js';
import {createRule} from '../src/ruleModel.js';
import {gameSave,restoreGameSave,validateGameSave} from '../src/gameSave.js';
import {validateGameplay} from '../src/gameplayModel.js';
import {callBlueprint,newBlueprintFunction} from '../src/blueprintFunctions.js';
import {capabilityDataDefinitions} from '../src/capabilityRegistry.js';
import {signalDefinitions} from '../src/signalModel.js';
import {operationDefinitions} from '../src/operationRegistry.js';
const rt=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio in this test');}));
const scope=id=>({kind:'object',id});
const call=(id,parameters)=>({id,version:1,parameters});
function fixture(){const p=createMechanicsProject('Авторские механики');p.subscenes[0].kind='empty';for(const id of ['door1','door2','door3','key1','key2','key3'])p.objects.push({id,name:id,type:'Активный меш',active:true,subsceneId:p.subscenes[0].id,transforms:{[p.subscenes[0].id]:{position:[0,0,0],rotation:[0,0,0],scale:[1,1,1]}}});return p;}
async function settled(runtime){for(let n=0;n<300&&!runtime.game.ruleRuntime.idle;n++)await new Promise(r=>setTimeout(r,2));assert.ok(runtime.game.ruleRuntime.idle);assert.equal(runtime.snapshot.error,null);}
function doorDefinition(p){const ready=addMechanic(p,'door',scope('door1'));bindDoorKey(p,ready,'key1');const rule=p.gameplay.rules.find(r=>r.id===ready.ruleId),d=createFromRule(p,rule,{name:'Дверь с ключом',scope:ready.scope});removeMechanic(p,ready);const transition={...d.rules[0],id:'open',from:'closed',to:'open'};d.rules=[];d.stateSchema={type:'object',properties:{uses:{type:'integer',default:0}}};d.initialState={uses:0};d.machine={initial:'closed',states:[{id:'closed',name:'Закрыта'},{id:'open',name:'Открыта'},{id:'locked',name:'Заперта'}],transitions:[transition]};for(const e of p.events)for(const g of e.groups)for(const a of g.actions)a.duration=0;return d;}
function key(p,id){let item=p.gameplay.items.find(i=>i.worldObjectId===id);if(!item){const m=addMechanic(p,'pickup',scope(id));item=p.gameplay.items.find(i=>i.id===m.recordId);}return item.id;}

test('one editable door definition serves three independent doors with different keys and saves per-instance state',async()=>{
 const p=fixture(),d=doorDefinition(p),ms=[1,2,3].map(n=>addMechanicInstance(p,d.id,scope('door'+n),{key:reference('item',key(p,'key'+n))}));assert.deepEqual(validateGameplay(p),[]);
 const runtime=rt();try{await runtime.start(p);runtime.game.addItem(key(p,'key1'),1,{publish:false});await runtime.game.trigger('interact',{objectId:'door2'});assert.equal(runtime.game.state.mechanicMachines[ms[1].id].state,'closed');await runtime.game.trigger('interact',{objectId:'door1'});await settled(runtime);
 assert.equal(runtime.game.state.mechanicMachines[ms[0].id].state,'open');assert.equal(runtime.game.state.mechanicMachines[ms[1].id].state,'closed');assert.equal(runtime.game.state.mechanicMachines[ms[2].id].state,'closed');assert.equal(runtime.snapshot.world.doors.door1,'Открыть');assert.equal(runtime.snapshot.world.doors.door2,undefined);
 runtime.game.state.mechanicStates[ms[0].id].uses=7;const save=JSON.parse(JSON.stringify(gameSave(runtime)));await restoreGameSave(runtime,save);assert.equal(runtime.game.state.mechanicStates[ms[0].id].uses,7);assert.equal(runtime.game.state.mechanicStates[ms[1].id].uses,0);assert.equal(runtime.game.state.mechanicMachines[ms[0].id].state,'open');assert.equal(runtime.game.state.mechanicMachines[ms[2].id].state,'closed');
 const bad=structuredClone(save);bad.world.game.mechanicMachines[ms[0].id].state='unknown';assert.throws(()=>validateGameSave(runtime.project,bad),/переходов/);
 }finally{runtime.stop();}
});

test('parameter assignments, shared copies, private definition copies and dependency identities are isolated',()=>{
 const p=fixture(),d=doorDefinition(p),a=addMechanicInstance(p,d.id,scope('door1'),{key:reference('item',key(p,'key1'))}),b=duplicateInstance(p,a,{scope:scope('door2')}),c=duplicateInstance(p,a,{independent:true,scope:scope('door3')});b.values.key=reference('item',key(p,'key2'));assert.notEqual(a.values,b.values);assert.notEqual(a.values.key.id,b.values.key.id);assert.equal(a.definitionId,b.definitionId);assert.notEqual(a.definitionId,c.definitionId);
 const privateD=descriptionFor(p,c.definitionId);assert.notEqual(privateD.machine.transitions[0].conditionFunctionId,d.machine.transitions[0].conditionFunctionId);assert.notEqual(privateD.machine.transitions[0].actions[0].eventId,d.machine.transitions[0].actions[0].eventId);d.machine.transitions[0].condition=null;d.machine.transitions[0].to='locked';assert.equal(descriptionFor(p,b.definitionId).machine.transitions[0].to,'locked');assert.equal(privateD.machine.transitions[0].to,'open');assert.deepEqual(validateGameplay(p),[]);
 const event=p.events.find(e=>e.id===privateD.machine.transitions[0].actions[0].eventId);const ids=Object.keys(privateD.machine.transitions[0].actions[0].targetBindings);assert.ok(ids.every(id=>event.groups.some(g=>g.actions.some(a=>a.id===id))));
});

test('simultaneous state transitions pick the first passing declaration and reject stale queued transitions',async()=>{
 const p=fixture(),d=doorDefinition(p);d.machine.transitions.forEach(t=>{t.conditionFunctionId='';t.actions=[];});d.machine.transitions.push({...structuredClone(d.machine.transitions[0]),id:'lock',to:'locked'});const m=addMechanicInstance(p,d.id,scope('door1'));const runtime=rt();try{await runtime.start(p);await Promise.all([runtime.game.trigger('interact',{objectId:'door1'}),runtime.game.trigger('interact',{objectId:'door1'})]);await settled(runtime);assert.equal(runtime.game.state.mechanicMachines[m.id].state,'open');assert.equal(runtime.game.state.mechanicMachines[m.id].revision,1);assert.equal(runtime.game.state.ruleTrace.filter(r=>r.kind==='transition').length,1);}finally{runtime.stop();}
});

test('leaving a state cancels its native Event before a delayed action and executes exit then transition then entry',async()=>{
 const p=fixture();p.variables??={};p.variableTypes??={};p.variables.late=0;p.variableTypes.late='integer';const e={id:'wait',name:'Отложенный Event',owner:'GameSession',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'wait-g',name:'Ожидание',actions:[{...makeAction('wait'),duration:.08}]},{id:'write-g',name:'Записать',actions:[makeAction('variable','late',9)]}]};p.events.push(e);
 const fn={id:'record',name:'Добавить букву',parameters:[{name:'letter',type:'string',default:''}],locals:{},entry:'record-set',nodes:[{id:'record-set',kind:'array',path:'local.order',operation:'push',value:{op:'get',path:'args.letter'}}]};p.functions.push(fn);const action=letter=>({kind:'function',functionId:'record',args:{letter}});
 const d=createMechanicDefinition(p,{name:'Состояния',parameters:{type:'object',properties:{}},stateSchema:{type:'object',properties:{order:{type:'collection',items:{type:'string'},default:[]}}},machine:{initial:'closed',states:[{id:'closed',name:'Закрыта',enter:[{kind:'event',eventId:'wait'}],exit:[action('exit')]},{id:'open',name:'Открыта',enter:[action('entry')]}],transitions:[{id:'open',signal:'interact',from:'closed',to:'open',actions:[action('transition')]}]}});const m=addMechanicInstance(p,d.id,scope('door1'));const runtime=rt();try{await runtime.start(p);await new Promise(r=>setTimeout(r,5));await runtime.game.trigger('interact',{objectId:'door1'});await settled(runtime);await new Promise(r=>setTimeout(r,100));assert.equal(runtime.snapshot.variables.late,0);assert.deepEqual(runtime.game.state.mechanicStates[m.id].order,['exit','transition','entry']);assert.ok(Object.values(runtime.snapshot.instances).some(i=>i.eventId==='wait'&&i.status==='stopped'));}finally{runtime.stop();}
});

test('creation from an Event reuses the native Event graph and contextual object slots',async()=>{
 const p=fixture();const ready=addMechanic(p,'interaction',scope('door1')),d=createFromEvent(p,ready.eventId,{name:'Подсветка'});removeMechanic(p,ready);p.events[0].groups[0].actions[0].duration=0;const m=addMechanicInstance(p,d.id,scope('door2'));const runtime=rt();try{await runtime.start(p);await settled(runtime);assert.ok(runtime.snapshot.world.highlights.door2);assert.equal(runtime.snapshot.world.highlights.door1,undefined);assert.ok(runtime.game.state.mechanicStates[m.id]);}finally{runtime.stop();}
});

test('existing blueprint functions retain bounded recursion, typed results and safe expression data',async()=>{
 const p=fixture(),f=newBlueprintFunction();f.resultSchema={type:'number'};p.functions.push(f);const d=createFromFunction(p,f.id,{name:'Расчёт'});addMechanicInstance(p,d.id,{kind:'project',id:p.id},{amount:5});const runtime=rt();try{await runtime.start(p);await settled(runtime);assert.equal(Object.values(runtime.game.state.mechanicStates)[0].result,5);}finally{runtime.stop();}
 const bad={id:'bad',name:'Неверный результат',parameters:[],locals:{},resultSchema:{type:'boolean'},entry:'r',nodes:[{id:'r',kind:'return',value:1}]};p.functions.push(bad);await assert.rejects(callBlueprint(p,'bad',{},p.variables,()=>{}),/Результат функции/);
 const cycle={id:'cycle',name:'Цикл',parameters:[],locals:{},entry:'c',nodes:[{id:'c',kind:'call',functionId:'cycle',next:'c'}]};p.functions.push(cycle);await assert.rejects(callBlueprint(p,'cycle',{},p.variables,()=>{}),/вложенных/);
});

test('trusted turns module extends data, operation, signal and save contracts without shooter dependencies',async()=>{
 assert.ok(capabilityDataDefinitions().some(d=>d.id==='sacura.turn'));assert.ok(signalDefinitions().some(s=>s.id==='sacura.turn.changed'));assert.ok(operationDefinitions().some(o=>o.id==='sacura.turn.next'));
 const p=fixture(),a=addMechanic(p,'sacura.turn',{kind:'project',id:p.id}),b=addMechanic(p,'sacura.turn',{kind:'scene',id:p.subscenes[0].id});a.values.participants=[reference('object','door1'),reference('object','door2')];b.values.participants=[reference('object','door3')];const runtime=rt();try{await runtime.start(p);const op=(name,m)=>runtime.game.execute(call('sacura.turn.'+name,{mechanic:reference('mechanic',m.id)}));await op('start',a);await op('next',a);assert.equal((await op('current',a)).id,'door2');await op('start',b);assert.equal((await op('current',b)).id,'door3');await op('next',a);assert.equal(runtime.game.state.capabilityStates['sacura.turn'][a.id].round,2);assert.equal(runtime.game.state.capabilityStates['sacura.turn'][b.id].round,1);assert.deepEqual(runtime.game.state.actors,{});await settled(runtime);const s=JSON.parse(JSON.stringify(gameSave(runtime)));await restoreGameSave(runtime,s);assert.equal(runtime.game.state.capabilityStates['sacura.turn'][a.id].round,2);const bad=structuredClone(s);bad.world.game.capabilityStates['sacura.turn'][a.id].index=20;assert.throws(()=>validateGameSave(runtime.project,bad),/очередности/);}finally{runtime.stop();}
});

test('editing shared function logic changes all shared instances while a private graph keeps its own nodes',async()=>{
 const p=fixture(),f=newBlueprintFunction();p.functions.push(f);const d=createFromFunction(p,f.id,{name:'Общее вычисление'}),a=addMechanicInstance(p,d.id,scope('door1'),{amount:2}),b=duplicateInstance(p,a,{scope:scope('door2')}),c=duplicateInstance(p,a,{independent:true,scope:scope('door3')});f.nodes.find(n=>n.kind==='set').value=40;
 const runtime=rt();try{await runtime.start(p);await settled(runtime);assert.equal(runtime.game.state.mechanicStates[a.id].result,40);assert.equal(runtime.game.state.mechanicStates[b.id].result,40);assert.equal(runtime.game.state.mechanicStates[c.id].result,2);}finally{runtime.stop();}
});

test('private graph copies preserve native object references inside arguments and registered operation identities',()=>{
 const p=fixture(),child={id:'child',name:'Вложенная функция',parameters:[{name:'object',schema:{type:'reference',referenceType:'object'}}],locals:{},entry:'child-op',nodes:[{id:'child-op',kind:'command',command:{command:'sacura.object.visible',operationVersion:1,value:{object:{op:'get',path:'args.object'},visible:true}}}]};p.functions.push(child);
 const parent={id:'parent',name:'Корень',parameters:[],locals:{},entry:'parent-call',nodes:[{id:'parent-call',kind:'call',functionId:'child',args:{object:reference('object','door1')}}]};p.functions.push(parent);const d=createFromFunction(p,parent.id),m=addMechanicInstance(p,d.id,scope('door1')),copy=duplicateInstance(p,m,{independent:true});const privateD=descriptionFor(p,copy.definitionId),privateFn=p.functions.find(f=>f.id===privateD.logic.functionId),privateChild=p.functions.find(f=>f.id===privateFn.nodes[0].functionId);
 assert.equal(privateFn.nodes[0].args.object.id,'door1');assert.equal(privateChild.nodes[0].command.command,'sacura.object.visible');assert.notEqual(privateChild.id,'child');assert.deepEqual(validateGameplay(p),[]);
});

test('invalid instance writes, unavailable machine versions and graph step cycles are diagnosed without dropping data',async()=>{
 const p=fixture(),f={id:'bad-write',name:'Типизированная запись',parameters:[],locals:{count:0},entry:'set',nodes:[{id:'set',kind:'set',path:'local.count',value:'wrong'}]};p.functions.push(f);const d=createMechanicDefinition(p,{name:'Счётчик',parameters:{type:'object',properties:{}},stateSchema:{type:'object',properties:{count:{type:'integer',default:0}}},logic:{functionId:f.id},rules:[]}),m=addMechanicInstance(p,d.id,scope('door1'));
 const runtime=rt();try{await runtime.start(p);await assert.rejects(runtime.game.callMechanic(m.id,{}),/целое|число/);assert.equal(runtime.game.state.mechanicStates[m.id].count,0);}finally{runtime.stop();}
 d.machine={version:99,initial:'x',states:[{id:'x',name:'Сохранено'}],transitions:[]};assert.ok(validateGameplay(p).some(e=>e.detail?.includes('Версия состояний')));assert.equal(d.machine.version,99);
 const cycle={id:'step-cycle',name:'Цикл шагов',parameters:[],locals:{x:0},entry:'cycle-step',nodes:[{id:'cycle-step',kind:'set',path:'local.x',value:1,next:'cycle-step'}]};p.functions.push(cycle);await assert.rejects(callBlueprint(p,cycle.id,{},p.variables,()=>{}),/лимит шагов/);
 assert.throws(()=>createMechanicDefinition(p,{name:'Код',parameters:{type:'object',properties:{}},logic:{functionId:f.id},callback:()=>1}),/сериализуемые/);
});

test('definition and function signatures share typed schemas while graph edits preserve every node and assigned override',()=>{
 const p=fixture(),f=newBlueprintFunction();p.functions.push(f);const d=createFromFunction(p,f.id),m=addMechanicInstance(p,d.id,scope('door1'),{amount:8}),nodes=structuredClone(f.nodes);updateDefinitionParameters(p,d.id,d.version,{type:'object',properties:{amount:{type:'integer',label:'Количество'},enabled:{type:'boolean',label:'Разрешено'}}},{amount:3,enabled:true});
 assert.equal(f.parameters[0].schema.type,'integer');assert.deepEqual(f.nodes,nodes);assert.equal(m.values.amount,8);f.parameters.push({name:'door',schema:{type:'reference',referenceType:'object',default:reference('object','door1')}});syncFunctionDefinitions(p,f.id);assert.equal(d.parameters.properties.door.referenceType,'object');assert.equal(m.values.amount,8);assert.deepEqual(f.nodes,nodes);assert.deepEqual(validateGameplay(p),[]);
});

test('state exit cancels only its own game timers and current saves reject missing machine state',async()=>{
 const p=fixture(),d=createMechanicDefinition(p,{name:'Таймер состояния',parameters:{type:'object',properties:{}},machine:{version:1,initial:'waiting',states:[{id:'waiting',name:'Ждёт',enter:[{kind:'operation',call:call('sacura.timer.start',{owner:{kind:'reference',type:'object',mode:'context',role:'self'},name:'Ожидание',seconds:.5,repeat:false})}]},{id:'done',name:'Завершено'}],transitions:[{id:'finish',from:'waiting',to:'done',signal:'interact',filter:{target:{kind:'reference',type:'object',mode:'context',role:'self'}},actions:[]}]}}),a=addMechanicInstance(p,d.id,scope('door1')),b=addMechanicInstance(p,d.id,scope('door2'));const runtime=rt();try{await runtime.start(p);await settled(runtime);assert.equal(Object.keys(runtime.game.state.timers).length,2);await runtime.game.trigger('interact',{objectId:'door1'});await settled(runtime);assert.equal(Object.keys(runtime.game.state.timers).length,1);assert.equal(Object.values(runtime.game.state.timers)[0].mechanicInstanceId,b.id);const save=gameSave(runtime),bad=structuredClone(save);delete bad.world.game.mechanicMachines[a.id];assert.throws(()=>validateGameSave(runtime.project,bad),/переходов экземпляра/);
 const old=structuredClone(save);delete old.world.game.authoringStateVersion;delete old.world.game.mechanicStates[b.id];delete old.world.game.mechanicMachines[b.id];await restoreGameSave(runtime,old);assert.equal(runtime.game.state.mechanicMachines[a.id].state,'done');assert.equal(runtime.game.state.mechanicMachines[b.id].state,'waiting');assert.equal(Object.keys(runtime.game.state.timers).length,1);
 }finally{runtime.stop();}
});
