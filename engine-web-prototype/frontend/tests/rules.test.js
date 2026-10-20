import test from 'node:test';
import assert from 'node:assert/strict';
import {createMechanicsProject,createEmptyProject} from '../src/projectLifecycle.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {createRule,RULE_LIMITS,validateRules} from '../src/ruleModel.js';
import {createSignalDefinition} from '../src/mechanicDescriptions.js';
import {registerOperation} from '../src/operationRegistry.js';
import {reference} from '../src/valueSchemas.js';
import {makeAction} from '../src/model.js';
import {readProject} from '../src/projectFiles.js';
import {ensureGameplay} from '../src/gameplayModel.js';
import {gameSave,restoreGameSave} from '../src/gameSave.js';
import {createWidgetElement} from '../src/widgetModel.js';
import {inputContextsFor} from '../src/inputModel.js';
const runtime=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));
const project=()=>{const p=createMechanicsProject('Правила без диалогов');p.variables={log:[],count:0,flag:false};p.variableTypes={log:'array',count:'integer',flag:'boolean'};ensureGameplay(p);createSignalDefinition(p,{id:'user.signal.test',name:'Сигнал',payloadSchema:{type:'object',properties:{value:{type:'integer',default:1}}}});return p;};
const op=(id,parameters)=>({kind:'operation',call:{id,version:1,parameters}});
const log=value=>op('test.rules.log',{value});
registerOperation({id:'test.rules.log',version:1,parameters:{type:'object',properties:{value:{type:'string'}}},result:{type:'void'},contexts:['rule','function','event'],editor:{label:'Записать метку'}},(p,c)=>{c.variables.log.push(p.value);});
const event=(p,id,actions)=>{p.events.push({id,name:id,owner:'GameSession',retention:'AUTO_CLOSE_ON_FLOW_END',groups:actions.map((a,i)=>({id:id+'-'+i,actions:[a]}))});return {kind:'event',eventId:id,join:'FLOW_END'};};
const wait=seconds=>({...makeAction('wait'),duration:seconds});
const write=(key,value)=>makeAction('variable',key,value);
const rule=(p,fields={})=>{const r=createRule({signal:'user.signal.test',scope:{kind:'project',id:p.id},...fields});p.gameplay.rules.push(r);return r;};
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function until(predicate){const end=Date.now()+2000;while(!predicate()){if(Date.now()>end)throw Error('Timed out');await sleep(2);}}
async function settle(rt){await until(()=>rt.game.flights.size===0);assert.equal(rt.snapshot.error,null);}
async function drive(rt,seconds){for(let n=0;n<Math.ceil(seconds/.05);n++){rt.playerStep([0,0,0],.05);await sleep(3);}}
async function start(p){const rt=runtime();await rt.start(readProject(p));await settle(rt);assert.equal(rt.snapshot.phase,'PLAYING');return rt;}

test('a project with no chapters/beat/dialogue runs registered rules and round-trips through the existing loader',async()=>{
 const p=project();rule(p,{actions:[log('UI game')]});const before=JSON.stringify(p),rt=await start(p);try{assert.equal(rt.snapshot.beatId,null);assert.deepEqual(rt.project.chapters,[]);await rt.game.trigger('user.signal.test',{value:1});assert.deepEqual(rt.snapshot.variables.log,['UI game']);assert.equal(JSON.stringify(p),before);assert.equal(rt.project.gameplay.playerId,'');}finally{rt.stop();}
});

test('button, received item and timer completion use the same rule model with committed payload/state',async()=>{
 const p=project(),widget=p.widgets[0];widget.elements.push(createWidgetElement('button','button'));
 for(const [signal,label]of [['button','button'],['pickup','item'],['timer','timer']])rule(p,{signal,actions:[log(label)]});
 p.gameplay.items.push({id:'key',name:'Ключ',stackLimit:10,heal:0,slot:'',weaponId:'',consumable:false,worldObjectId:'',useEventId:''});const rt=await start(p);try{await rt.uiAction('none',widget.elements.at(-1));rt.game.addItem('key');await settle(rt);rt.game.command('startTimer',{timerId:'clock',seconds:.05});await drive(rt,.05);await settle(rt);assert.deepEqual(rt.snapshot.variables.log,['button','item','timer']);assert.equal(rt.game.count('key'),1);assert.equal(rt.game.state.timers.clock,undefined);}finally{rt.stop();}
});

test('conditions and source/target/tag filters are sampled at publication, not after earlier awaited actions',async()=>{
 const p=project();p.objects.push({id:'source',active:true,tags:['keys']},{id:'target',active:true});const a=event(p,'change',[write('flag',true)]);rule(p,{actions:[a]});
 rule(p,{filter:{source:{kind:'reference',type:'object',mode:'tag',tag:'keys'},target:reference('object','target')},condition:{op:'eq',a:{op:'get',path:'global.flag'},b:false},actions:[log('snapshot')],otherwise:[log('live')]});
 const rt=await start(p);try{const task=rt.game.trigger('user.signal.test',{value:1},undefined,{sourceId:'source',targetId:'target'});rt.project.objects.find(o=>o.id==='source').tags=[];await task;assert.equal(rt.snapshot.variables.flag,true);assert.deepEqual(rt.snapshot.variables.log,['snapshot']);}finally{rt.stop();}
});

test('otherwise uses the same operation/Event/function execution and false conditions do not consume once',async()=>{
 const p=project(),r=rule(p,{once:true,condition:{op:'get',path:'global.flag'},actions:[log('yes')]});const rt=await start(p);try{await rt.game.trigger('user.signal.test',{value:1});assert.notEqual(rt.game.state.rules[r.id]?.fired,true);rt.snapshot.variables.flag=true;await rt.game.trigger('user.signal.test',{value:1});await rt.game.trigger('user.signal.test',{value:1});assert.deepEqual(rt.snapshot.variables.log,['yes']);}finally{rt.stop();}
});

test('FIFO signal admission and declaration order have an observable and bounded trace',async()=>{
 const p=project();rule(p,{actions:[log('a')]});rule(p,{actions:[log('b')]});const rt=await start(p);try{const a=rt.game.trigger('user.signal.test',{value:1}),b=rt.game.trigger('user.signal.test',{value:2});await Promise.all([a,b]);assert.deepEqual(rt.snapshot.variables.log,['a','b','a','b']);const starts=rt.game.state.ruleTrace.filter(e=>e.kind==='start');assert.ok(starts.every((e,i)=>!i||e.seq>=starts[i-1].seq));assert.ok(rt.game.state.ruleTrace.length<=128);}finally{rt.stop();}
});

test('skip rejects re-entry while a native Event is running',async()=>{
 const p=project();rule(p,{reentry:'skip',actions:[event(p,'wait',[wait(.1),write('count',1)])]});const rt=await start(p);try{const a=rt.game.trigger('user.signal.test',{value:1});await until(()=>rt.game.ruleRuntime.active.size===1);const b=rt.game.trigger('user.signal.test',{value:2});await drive(rt,.2);await Promise.all([a,b]);assert.equal(rt.snapshot.variables.count,1);assert.equal(rt.game.state.ruleTrace.filter(e=>e.kind==='start').length,1);}finally{rt.stop();}
});

test('queue preserves each admitted snapshot and executes sequentially with an explicit bound',async()=>{
 const p=project();rule(p,{maxQueued:2,actions:[event(p,'wait',[wait(.1)]),log('done')]});const rt=await start(p);try{const a=rt.game.trigger('user.signal.test',{value:1});await until(()=>rt.game.ruleRuntime.active.size===1);const b=rt.game.trigger('user.signal.test',{value:2});await until(()=>rt.game.ruleRuntime.pending.length===1);await drive(rt,.3);await Promise.all([a,b]);assert.deepEqual(rt.snapshot.variables.log,['done','done']);}finally{rt.stop();}
});

test('restart cancels the previous native Event, cleanup and function continuation before replacement',async()=>{
 const p=project();rule(p,{reentry:'restart',actions:[event(p,'wait',[wait(.15)]),log('winner')]});const rt=await start(p);try{const a=rt.game.trigger('user.signal.test',{value:1});await until(()=>Object.values(rt.snapshot.instances).some(i=>i.status==='running'));const old=Object.values(rt.snapshot.instances)[0].id;const b=rt.game.trigger('user.signal.test',{value:2});await until(()=>rt.snapshot.instances[old].status==='stopped');await drive(rt,.3);await Promise.all([a,b]);assert.deepEqual(rt.snapshot.variables.log,['winner']);}finally{rt.stop();}
});

test('cooldown uses simulation time and a signal inside cooldown is dropped rather than deferred',async()=>{
 const p=project();rule(p,{cooldown:.1,actions:[log('run')]});const rt=await start(p);try{await rt.game.trigger('user.signal.test',{value:1});await rt.game.trigger('user.signal.test',{value:2});assert.deepEqual(rt.snapshot.variables.log,['run']);await drive(rt,.1);await rt.game.trigger('user.signal.test',{value:3});assert.deepEqual(rt.snapshot.variables.log,['run','run']);}finally{rt.stop();}
});

test('a self-generating and mutual cycle are stopped without deadlocking the existing Event executor',async()=>{
 const p=project();createSignalDefinition(p,{id:'user.signal.other',name:'Другой'});const emit=id=>op('sacura.signal.emit',{signal:reference('signal',id),payload:id==='user.signal.test'?{value:1}:{}});rule(p,{actions:[emit('user.signal.other')]});rule(p,{signal:'user.signal.other',actions:[emit('user.signal.test')]});const rt=await start(p);try{await rt.game.trigger('user.signal.test',{value:1});assert.match(rt.snapshot.hint,/сам себя/);assert.equal(rt.game.flights.size,0);assert.equal(rt.game.state.ruleTrace.filter(e=>e.kind==='start').length,2);}finally{rt.stop();}
});

test('pause freezes timers, cooldown time, queued admissions and game-clock Event waits',async()=>{
 const p=project();rule(p,{actions:[event(p,'wait',[wait(.1)]),log('end')]});const rt=await start(p);try{const task=rt.game.trigger('user.signal.test',{value:1});await until(()=>rt.game.ruleRuntime.active.size===1);rt.togglePause();rt.game.command('startTimer',{timerId:'paused',seconds:.1});const time=rt.game.state.clock;await drive(rt,.2);assert.equal(rt.game.state.clock,time);assert.equal(rt.game.state.timers.paused.remaining,.1);assert.deepEqual(rt.snapshot.variables.log,[]);rt.togglePause();await drive(rt,.2);await task;assert.deepEqual(rt.snapshot.variables.log,['end']);}finally{rt.stop();}
});

test('stop/restart cannot leave callbacks or completions writing into a new session',async()=>{
 const p=project();rule(p,{actions:[event(p,'wait',[wait(.15),write('count',9)])]});const rt=await start(p);const old=rt.game;const task=rt.game.trigger('user.signal.test',{value:1});await until(()=>old.ruleRuntime.active.size===1);rt.stop();await rt.start(p);await drive(rt,.25);await task;assert.equal(rt.snapshot.variables.count,0);assert.equal(old.ruleRuntime.active.size,0);assert.notEqual(rt.game,old);rt.stop();
});

test('changing scene cancels scene-owned queued/running reactions and timers, retaining project ownership',async()=>{
 const p=project(),s=p.subscenes[0];p.subscenes.push({...structuredClone(s),id:'second',name:'Вторая'});rule(p,{scope:{kind:'scene',id:s.id},actions:[event(p,'wait',[wait(.1),write('count',9)])]});const rt=await start(p);try{rt.game.command('startTimer',{timerId:'owned',seconds:1},{scope:{kind:'scene',id:s.id}});const task=rt.game.trigger('user.signal.test',{value:1});await until(()=>rt.game.ruleRuntime.active.size===1);await rt.enterScene('second');await drive(rt,.2);await task;assert.equal(rt.snapshot.variables.count,0);assert.equal(rt.game.state.timers.owned,undefined);}finally{rt.stop();}
});

test('deleting an object/mechanic owner cancels native work and does not require a new object store',async()=>{
 const p=project();p.objects.push({id:'owner',active:true});rule(p,{scope:{kind:'object',id:'owner'},actions:[event(p,'wait',[wait(.1),write('count',9)])]});const rt=await start(p);try{const task=rt.game.trigger('user.signal.test',{value:1});await until(()=>rt.game.ruleRuntime.active.size===1);rt.project.objects=[];rt.game.tick(0);await task;assert.equal(rt.snapshot.variables.count,0);assert.ok(Object.values(rt.snapshot.instances).every(i=>i.status==='stopped'));}finally{rt.stop();}
});

test('parallel rule runs require explicit limits and detached native joins still count running work',async()=>{
 const p=project();p.gameplay.ruleSettings={maxRuns:2};const e=event(p,'wait',[wait(.1)]);e.join='NONE';rule(p,{maxConcurrent:2,actions:[e]});const rt=await start(p);try{const a=rt.game.trigger('user.signal.test',{value:1}),b=rt.game.trigger('user.signal.test',{value:2});await until(()=>rt.game.ruleRuntime.active.size===2);assert.equal(Object.values(rt.snapshot.instances).filter(i=>i.status==='running').length,2);await drive(rt,.2);await Promise.all([a,b]);assert.equal(rt.game.ruleRuntime.active.size,0);}finally{rt.stop();}
});

test('signal overload is bounded and exposes a clear diagnostic, never an unbounded promise chain',async()=>{
 const p=project();rule(p,{maxQueued:1,actions:[event(p,'wait',[wait(1)])]});const rt=await start(p);try{rt.togglePause();const tasks=[];for(let i=0;i<RULE_LIMITS.signals;i++)tasks.push(rt.game.trigger('user.signal.test',{value:i}));await assert.rejects(rt.game.trigger('user.signal.test',{value:999}),/заполнена/);assert.equal(rt.game.ruleRuntime.signals.length,RULE_LIMITS.signals);rt.stop();await Promise.all(tasks);}finally{rt.stop();}
});

test('typed payload and optional contexts do not invent a player target',async()=>{
 const p=project();rule(p,{actions:[log('all')]});rule(p,{filter:{target:reference('object','missing')},actions:[log('target')]});p.gameplay.rules.pop();const rt=await start(p);try{await assert.rejects(rt.game.trigger('user.signal.test',{value:'1'}),/число/);await rt.game.trigger('user.signal.test',{value:1});assert.deepEqual(rt.snapshot.variables.log,['all']);assert.equal(rt.game.config.playerId,'');}finally{rt.stop();}
});

test('state-changing operations publish only after their variable commit',async()=>{
 const p=project();rule(p,{signal:'sacura.variable.changed',condition:{op:'eq',a:{op:'get',path:'global.count'},b:7},actions:[log('committed')]});const rt=await start(p);try{const v=rt.project.gameplay.variableBindings.find(x=>x.key==='count');await rt.game.command('sacura.variable.set',{variable:reference('variable',v.id),value:7});assert.deepEqual(rt.snapshot.variables.log,['committed']);}finally{rt.stop();}
});

test('safe condition functions read a publication snapshot and cannot execute side effects',async()=>{
 const p=project();p.functions.push({id:'unsafe',name:'Условие',parameters:[],locals:{},entry:'command',nodes:[{id:'command',kind:'command',command:{command:'sacura.signal.emit',value:{signal:reference('signal','user.signal.test'),payload:{value:1}}}}]});const r=rule(p,{actions:[log('bad')],conditionFunctionId:'unsafe'});const rt=await start(p);try{await rt.game.trigger('user.signal.test',{value:1});assert.match(rt.snapshot.hint,/чтения/);assert.deepEqual(rt.snapshot.variables.log,[]);assert.notEqual(rt.game.state.rules[r.id]?.fired,true);}finally{rt.stop();}
});

test('history can be entered by a common operation and dialog control/ticking follows explicit policy',async()=>{
 const p=createEmptyProject('История');p.gameplay.enabled=true;p.gameplay.storyPolicy={mode:'none',mechanics:'continuous',dialogueControl:'suspend',ending:'continue'};createSignalDefinition(p,{id:'user.signal.story',name:'История'});rule(p,{signal:'user.signal.story',actions:[op('sacura.story.enter',{beat:reference('beat',p.subscenes[0].entry)})]});const rt=await start(p);try{await rt.game.trigger('user.signal.story',{});assert.equal(rt.snapshot.phase,'WAITING_INPUT');const b=p.chapters[0].beats[0];assert.ok(!inputContextsFor(rt.project,b).includes('gameplay'));const before=rt.game.state.clock;rt.playerStep([1,0,0],.05);assert.ok(rt.game.state.clock>before);assert.equal(rt.snapshot.world.playerControl,null);}finally{rt.stop();}
});

test('save/restore of a storyless project cannot serialize pending reactions or restart old subscriptions',async()=>{
 const p=project();rule(p,{once:true,actions:[event(p,'wait',[wait(.1),write('count',4)])]});const rt=await start(p);try{const task=rt.game.trigger('user.signal.test',{value:1});await until(()=>rt.game.ruleRuntime.active.size===1);assert.throws(()=>gameSave(rt),/после выполнения/);await drive(rt,.2);await task;const save=JSON.parse(JSON.stringify(gameSave(rt)));await restoreGameSave(rt,save);await rt.game.trigger('user.signal.test',{value:1});assert.equal(rt.snapshot.variables.count,4);assert.equal(rt.snapshot.beatId,null);}finally{rt.stop();}
});

test('schema validates policy limits, scopes and registered action references before play',()=>{
 const p=project();rule(p,{reentry:'unknown',maxConcurrent:99,actions:[{kind:'event',eventId:'absent'}]});assert.ok(validateRules(p).some(i=>i.detail.includes('ограничения')));assert.ok(validateRules(p).some(i=>i.detail.includes('удалённое')));
});

test('native Event commands publish descendants without waiting on themselves and preserve group order',async()=>{
 const p=project();createSignalDefinition(p,{id:'user.signal.child',name:'Результат'});const emit=makeAction('gameplay');emit.command='sacura.signal.emit';emit.value={signal:reference('signal','user.signal.child'),payload:{}};rule(p,{actions:[event(p,'publish',[emit,write('count',3)])]});rule(p,{signal:'user.signal.child',actions:[log('child')]});const rt=await start(p);try{await rt.game.trigger('user.signal.test',{value:1});assert.equal(rt.snapshot.variables.count,3);assert.deepEqual(rt.snapshot.variables.log,['child']);assert.equal(rt.game.flights.size,0);}finally{rt.stop();}
});

test('otherwise invokes an existing function and Event through the same registered operations',async()=>{
 const p=project();p.functions.push({id:'branch',name:'Иначе',parameters:[],locals:{},entry:'set',nodes:[{id:'set',kind:'set',path:'global.count',value:5,next:'end'},{id:'end',kind:'return',value:true}]});rule(p,{condition:false,actions:[log('never')],otherwise:[{kind:'function',functionId:'branch'},event(p,'otherwise',[write('flag',true)]),log('otherwise')]});const rt=await start(p);try{await rt.game.trigger('user.signal.test',{value:1});assert.equal(rt.snapshot.variables.count,5);assert.equal(rt.snapshot.variables.flag,true);assert.deepEqual(rt.snapshot.variables.log,['otherwise']);}finally{rt.stop();}
});

test('canonical timer sensor defaults, object filters and removal use native object identities',async()=>{
 const p=project();p.objects.push({id:'timer-owner',active:true});const r=rule(p,{signal:'timer',seconds:.05,scope:{kind:'object',id:'timer-owner'},filter:{source:reference('object','timer-owner')},actions:[log('timer')]});const rt=await start(p);try{await drive(rt,.05);await settle(rt);assert.deepEqual(rt.snapshot.variables.log,['timer']);assert.equal(rt.game.state.rules[r.id].fired,true);rt.game.command('startTimer',{timerId:'object-timer',seconds:1},{scope:{kind:'object',id:'timer-owner'}});rt.project.objects=[];rt.game.tick(0);assert.equal(rt.game.state.timers['object-timer'],undefined);}finally{rt.stop();}
});

test('discrete Input Actions publish typed input after writing variables; continuous movement stays a system',async()=>{
 const p=project();const action={id:'custom-input',name:'Подтвердить',valueType:'boolean',trigger:'pressed',holdSeconds:.4,behavior:'event',consume:false,variable:'flag'};p.input.actions.push(action);p.input.contexts.find(c=>c.id==='gameplay').bindings.push({id:'custom-binding',actionId:action.id,device:'keyboard',control:'KeyQ',scale:1,deadZone:0});rule(p,{signal:'input',filter:{source:reference('inputAction',action.id)},condition:{op:'get',path:'global.flag'},actions:[log('input')]});const rt=await start(p);try{await rt.inputAction({actionId:action.id,phase:'triggered',value:true,device:'keyboard'});await settle(rt);assert.deepEqual(rt.snapshot.variables.log,['input']);rt.project.input.actions.find(a=>a.id===action.id).trigger='continuous';await rt.inputAction({actionId:action.id,phase:'triggered',value:true});await settle(rt);assert.deepEqual(rt.snapshot.variables.log,['input']);}finally{rt.stop();}
});

test('restart replaces a paused function/Event continuation even with zero queued capacity',async()=>{
 const p=project();const e=event(p,'function-wait',[wait(.1)]);p.functions.push({id:'reaction-fn',name:'Работа',parameters:[],locals:{},entry:'wait',nodes:[{id:'wait',kind:'event',eventId:e.eventId,next:'set'},{id:'set',kind:'set',path:'global.count',value:{op:'add',a:{op:'get',path:'global.count'},b:1},next:'end'},{id:'end',kind:'return',value:true}]});rule(p,{reentry:'restart',maxQueued:0,actions:[{kind:'function',functionId:'reaction-fn'}]});const rt=await start(p);try{const first=rt.game.trigger('user.signal.test',{value:1});await until(()=>Object.values(rt.snapshot.instances).some(i=>i.status==='running'));const second=rt.game.trigger('user.signal.test',{value:2});await drive(rt,.25);await Promise.all([first,second]);assert.equal(rt.snapshot.variables.count,1);assert.equal(rt.game.flights.size,0);}finally{rt.stop();}
});

test('operation failure stops owned native effects and releases admission for the next signal',async()=>{
 const p=project();p.objects.push({id:'door',active:true});const held=event(p,'held',[makeAction('door','door','Открыть')]);p.events.find(e=>e.id==='held').retention='KEEP_ALIVE';rule(p,{actions:[held,op('sacura.condition.compare',{left:true,right:1,operator:'eq'})]});const rt=await start(p);try{await rt.game.trigger('user.signal.test',{value:1});assert.match(rt.snapshot.hint,/одного типа/);assert.ok(Object.values(rt.snapshot.instances).every(i=>i.status==='stopped'));assert.equal(rt.snapshot.world.doors.door,null);assert.equal(rt.game.flights.size,0);}finally{rt.stop();}
});

test('an unavailable rule version is preserved, diagnosed and never executed',async()=>{
 const p=project(),r=rule(p,{version:2,actions:[log('future')]});const before=JSON.stringify(r),rt=await start(p);try{assert.ok(validateRules(rt.project).some(i=>i.detail.includes('Версия')));await rt.game.trigger('user.signal.test',{value:1});assert.deepEqual(rt.snapshot.variables.log,[]);assert.equal(JSON.stringify(rt.project.gameplay.rules[0]),before);}finally{rt.stop();}
});

test('a pure registered condition reads project/world at publication, including when evaluated later',async()=>{
 registerOperation({id:'test.rules.visible',version:1,pure:true,parameters:{type:'object',properties:{}},result:{type:'boolean'},contexts:['rule'],editor:{label:'Видимость'}},(_,c)=>c.world.visible.marker===true&&c.project.objects[0].tags.includes('old'));
 const p=project();p.objects.push({id:'marker',active:true,tags:['old']});rule(p,{conditionOperation:{id:'test.rules.visible',version:1,parameters:{}},actions:[log('snapshot')]});const rt=await start(p);try{rt.snapshot.world.visible.marker=true;const task=rt.game.trigger('user.signal.test',{value:1});rt.snapshot.world.visible.marker=false;rt.project.objects[0].tags=[];await task;assert.deepEqual(rt.snapshot.variables.log,['snapshot']);}finally{rt.stop();}
});

test('saving a native widget Event outside rules is also gated until its finite work ends',async()=>{
 const p=project(),e=event(p,'widget-wait',[wait(.05),write('count',2)]);const rt=await start(p);try{const task=rt.uiAction('event',{id:'widget-button',eventId:e.eventId});await until(()=>Object.values(rt.snapshot.instances).some(i=>i.status==='running'));assert.throws(()=>gameSave(rt),/после выполнения/);await task;assert.equal(gameSave(rt).variables.count,2);}finally{rt.stop();}
});

test('legacy function commands inside native rule Events retain cancellation ownership and game time',async()=>{
 const p=project();const e=event(p,'nested-wait',[wait(.1)]);p.functions.push({id:'nested',name:'Вложенная',parameters:[],locals:{},entry:'wait',nodes:[{id:'wait',kind:'event',eventId:e.eventId,next:'set'},{id:'set',kind:'set',path:'global.count',value:9,next:'end'},{id:'end',kind:'return',value:true}]});const call=makeAction('gameplay');call.command='callFunction';call.value={functionId:'nested'};rule(p,{actions:[event(p,'caller',[call])]});const rt=await start(p);const task=rt.game.trigger('user.signal.test',{value:1});await until(()=>Object.values(rt.snapshot.instances).some(i=>i.eventId==='nested-wait'));rt.stop();await rt.start(p);await drive(rt,.2);await task;assert.equal(rt.snapshot.variables.count,0);rt.stop();
});

test('game time advances rule-owned waits while explicit gameplay-node policy suspends continuous systems during dialogue',async()=>{
 const p=createEmptyProject('До игрового узла');p.variables={count:0};p.variableTypes={count:'integer'};p.gameplay.enabled=true;p.gameplay.activation='gameplay-node';rule(p,{signal:'button',actions:[event(p,'before-combat',[wait(.1),write('count',4)])]});const rt=runtime();await rt.start(p,p.subscenes[0].entry);try{assert.equal(rt.game.state.active,false);const task=rt.game.trigger('button',{elementId:'example',action:'none'});await until(()=>rt.game.ruleRuntime.active.size===1);await drive(rt,.2);await task;assert.equal(rt.snapshot.variables.count,4);assert.equal(rt.snapshot.phase,'WAITING_INPUT');assert.equal(rt.game.state.active,false);}finally{rt.stop();}
});

test('an author can remove the last dialogue after choosing a storyless start and still load/run the project',async()=>{
 const {deleteStoryNode}=await import('../src/storyEditing.js');const p=createEmptyProject('Интерфейс');p.gameplay.enabled=true;p.gameplay.storyPolicy={mode:'none',mechanics:'continuous'};const result=deleteStoryNode(p,p.subscenes[0].entry);assert.equal(result.error,undefined);assert.equal(p.subscenes[0].entry,null);assert.equal(p.chapters[0].beats.length,0);const rt=await start(p);rt.stop();
});
