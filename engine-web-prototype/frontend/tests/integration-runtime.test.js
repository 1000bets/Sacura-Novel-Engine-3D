import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {gameSave,restoreGameSave,validateGameSave} from '../src/gameSave.js';
import {preparePlayableGame} from '../src/gameExport.js';
import {registerOperation} from '../src/operationRegistry.js';
import {createRule} from '../src/ruleModel.js';
import {createSignalDefinition,createMechanicDefinition,addMechanicInstance} from '../src/mechanicDescriptions.js';
import {reference} from '../src/valueSchemas.js';
import {JOURNAL_LIMIT,lastRuleExecution} from '../src/executionJournal.js';
import {addMechanic} from '../src/mechanicsModel.js';
import {registerCapability} from '../src/capabilityRegistry.js';
import {activityValue} from '../src/runtimePresentation.js';
import {makeAction} from '../src/model.js';
const runtime=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio output requested');}));
const p=()=>{const p=createMechanicsProject('Интеграция');p.variables={count:0};p.variableTypes={count:'integer'};return p;};
async function settle(rt){for(let i=0;i<300&&(!rt.game.ruleRuntime.idle||rt.game.flights.size);i++)await new Promise(r=>setTimeout(r,2));assert.ok(rt.game.ruleRuntime.idle);assert.equal(rt.game.flights.size,0);}
async function waitFor(check){for(let i=0;i<300&&!check();i++)await new Promise(r=>setTimeout(r,2));assert.ok(check());}
const built=async()=>({js:await readFile('public/player-runtime.js','utf8'),css:await readFile('public/player-runtime.css','utf8')});
const embedded=html=>JSON.parse(html.split('globalThis.SACURA_PROJECT=')[1].split(';</script>')[0]);

test('player export keeps function/instance audio references and embeds relative resources outside Event Actions',async()=>{
 const project=p();project.audioAssets=[{id:'function-audio',name:'Звук функции',url:'sound/You_came_back_after_all.mp3',kind:'sound'},{id:'unused',name:'Наступает вечер',url:'not-present.wav',kind:'sound'}];
 project.functions=[{id:'asset-function',name:'Ресурс в функции',parameters:[{name:'assetId',type:'string',default:'function-audio'}],locals:{picture:{src:'game-templates/previews/quest.jpg'}},entry:'return',nodes:[{id:'return',kind:'return',value:{op:'get',path:'args.assetId'}}]}];
 const before=JSON.stringify(project),reads=[];const html=await preparePlayableGame(project,{readRuntime:built,readAsset:async url=>{reads.push(url);return new Blob([await readFile('public/'+url)],{type:url.endsWith('jpg')?'image/jpeg':'audio/mpeg'});}}),copy=embedded(html);
 assert.equal(JSON.stringify(project),before);assert.deepEqual(reads.sort(),['game-templates/previews/quest.jpg','sound/You_came_back_after_all.mp3']);assert.deepEqual(copy.audioAssets.map(a=>a.id),['function-audio']);assert.match(copy.audioAssets[0].url,/^data:audio\/mpeg;base64,/);assert.match(copy.functions[0].locals.picture.src,/^data:image\/jpeg;base64,/);
 const rt=runtime();try{await rt.start(copy);assert.equal(await rt.game.callFunction('asset-function'), 'function-audio');}finally{rt.stop();}
});

test('player export refuses an external GLB dependency instead of producing a hidden network dependency',async()=>{
 const project=p(),model={asset:{version:'2.0'},images:[{uri:'texture.png'}]},raw=new TextEncoder().encode(JSON.stringify(model).padEnd(Math.ceil(JSON.stringify(model).length/4)*4,' ')),data=new Uint8Array(20+raw.length),v=new DataView(data.buffer);v.setUint32(0,0x46546c67,true);v.setUint32(4,2,true);v.setUint32(8,data.length,true);v.setUint32(12,raw.length,true);v.setUint32(16,0x4e4f534a,true);data.set(raw,20);
 project.functions=[{id:'model-function',name:'Модель функции',parameters:[],locals:{asset:{src:'/model.glb'}},entry:'return',nodes:[{id:'return',kind:'return',value:0}]}];await assert.rejects(preparePlayableGame(project,{readRuntime:built,readAsset:async()=>new Blob([data],{type:'model/gltf-binary'})}),/GLB содержит внешний ресурс/);
});

test('player export embeds the native default music and resume fallback instead of relying on Editor audio catalog URLs',async()=>{
 const project=p();project.events=[{id:'background',name:'Музыка',groups:[{id:'music-group',actions:[makeAction('music'),makeAction('resume')]}]}];const reads=[];const copy=embedded(await preparePlayableGame(project,{readRuntime:built,readAsset:async url=>{reads.push(url);return new Blob([await readFile('public'+url)],{type:'audio/wav'});}}));assert.equal(reads.length,1);assert.equal(copy.audioAssets.length,1);assert.equal(copy.audioAssets[0].id,'music-main');assert.match(copy.audioAssets[0].url,/^data:audio\/wav;base64,/);
});

test('built player module graph contains shared executors, trusted modules and widgets but no Editor/workspaces',async()=>{
 const manifest=JSON.parse(await readFile('public/player-runtime.manifest.json','utf8'));for(const file of ['src/runtime.js','src/operationRegistry.js','src/RuleRuntime.js','src/blueprintFunctions.js','src/capabilities/turns.js','src/WidgetRenderer.jsx'])assert.ok(manifest.modules.includes(file),file);assert.ok(!manifest.modules.some(x=>/(Editor|Workspace|ProjectHome|SchemaFields)\.jsx$/.test(x)));
});

test('activity descriptions resolve registered operations and existing functions without exposing object stringification',()=>{
 const project=p();project.functions.push({id:'activity-fn',name:'Начислить награду'});assert.equal(activityValue(project,{type:'gameplay',command:'callFunction',value:{functionId:'activity-fn',args:{amount:2}}}),'Начислить награду');assert.equal(activityValue(project,{type:'gameplay',command:'sacura.resource.add',operationVersion:1,value:{amount:2}}),'Изменить счётчик');assert.equal(activityValue(project,{type:'variable',target:'count',value:2}),'count · 2');
});

test('widget design preview uses actual initial capability data without a fake player or executing start rewards',async()=>{
 const project=p(),rt=runtime();project.objects.push({id:'independent',active:true});project.gameplay.actors.independent={maxHP:37};project.gameplay.resources.push({id:'coins',name:'Монеты',owner:{kind:'project',id:project.id},type:'integer',initial:8});project.gameplay.variableBindings.push({id:'design-count',key:'count'});project.gameplay.rules.push(createRule({signal:'start',scope:{kind:'project',id:project.id},actions:[{kind:'operation',call:{id:'sacura.variable.set',version:1,parameters:{variable:reference('variable','design-count'),value:99}}}]}));const before=JSON.stringify(project),initial=rt.initialPreviewContext(project);assert.equal(initial.game.actors.independent.hp,37);assert.equal(initial.game.actors[''],undefined);assert.equal(initial.game.resources.coins,8);assert.equal(initial.variables.count,0);assert.equal(rt.running,false);assert.equal(JSON.stringify(project),before);
});

test('save v2 rejects changed execution data atomically, accepts graph layout/title edits and reads v1 without replay',async()=>{
 const project=p(),rt=runtime();const start=createRule({signal:'start',scope:{kind:'project',id:project.id},once:true,actions:[]});project.gameplay.rules.push(start);try{await rt.start(project);await settle(rt);const s=gameSave(rt);assert.equal(s.version,2);const original=rt.game;rt.project.gameplay.storyPolicy.ending='stop';await assert.rejects(restoreGameSave(rt,s),/Логика или состав/);assert.equal(rt.game,original);assert.ok(rt.running);rt.project.gameplay.storyPolicy.ending=project.gameplay.storyPolicy.ending;rt.project.title='Новое название';rt.project.graphLayout={any:{zoom:2}};await restoreGameSave(rt,s);assert.equal(rt.game.state.rules[start.id]?.fired,s.world.game.rules[start.id]?.fired);const legacy={...s,version:1};delete legacy.compatibility;await restoreGameSave(rt,legacy);assert.match(rt.snapshot.hint,/Старое сохранение|старое сохранение/);}finally{rt.stop();}
});

test('direct async operations and functions cannot save a Promise or complete after restart',async()=>{
 let release;registerOperation({id:'test.integration.await',version:1,parameters:{type:'object',properties:{}},result:{type:'void'},contexts:['function','rule'],editor:{label:'Ожидание интеграции'}},async(_,c)=>{await new Promise(r=>release=r);c.assertActive();c.variables.count=99;});
 const project=p(),rt=runtime();project.functions.push({id:'async-function',name:'Ожидающая функция',parameters:[],locals:{},entry:'work',nodes:[{id:'work',kind:'command',command:{command:'test.integration.await',operationVersion:1,value:{}}}]});try{await rt.start(project);const result=rt.game.callFunction('async-function').catch(e=>e.message);await waitFor(()=>release);assert.throws(()=>gameSave(rt),/после выполнения.*функций и операций/);await rt.start(project);release();assert.equal(await result,'CANCELLED');assert.equal(rt.snapshot.variables.count,0);assert.equal(rt.game.flights.size,0);rt.snapshot.variables.bad=Promise.resolve();assert.throws(()=>gameSave(rt),/несериализуемые/);}finally{rt.stop();}
});

test('save data validates timers, once state, variable types and restores a partially elapsed paused timer exactly once',async()=>{
 const project=p(),rt=runtime();createSignalDefinition(project,{id:'test.signal.save',name:'После таймера'});const r=createRule({id:'timer-rule',signal:'timer',scope:{kind:'project',id:project.id},once:true,actions:[{kind:'operation',call:{id:'sacura.variable.set',version:1,parameters:{variable:reference('variable','count-binding'),value:1}}}]});project.gameplay.variableBindings.push({id:'count-binding',key:'count'});project.gameplay.rules.push(r);
 try{await rt.start(project);await rt.game.execute({id:'sacura.timer.start',version:1,parameters:{owner:reference('project',project.id),name:'Один раз',seconds:1}});for(let i=0;i<6;i++)rt.game.tick(.05);rt.togglePause();const save=gameSave(rt),remaining=Object.values(save.world.game.timers)[0].remaining;assert.ok(remaining<1&&remaining>.6);for(let i=0;i<30;i++)rt.game.tick(.05);assert.equal(Object.values(rt.game.state.timers)[0].remaining,remaining);await restoreGameSave(rt,save);assert.equal(rt.snapshot.paused,false);for(let i=0;i<20;i++)rt.game.tick(.05);await settle(rt);assert.equal(rt.snapshot.variables.count,1);const after=gameSave(rt);assert.equal(after.world.game.rules[r.id].fired,true);await restoreGameSave(rt,after);await rt.game.trigger('timer',{objectId:'',timerId:'Один раз'});await settle(rt);assert.equal(rt.snapshot.variables.count,1);
 for(const damage of [s=>s.variables.count='1',s=>s.world.game.rules[r.id].fired='yes',s=>s.world.game.timers.invalid={remaining:-1,seconds:1,repeat:false}]){const bad=structuredClone(after);damage(bad);assert.throws(()=>validateGameSave(rt.project,bad),/сохранени/);}
 }finally{rt.stop();}
});

test('journal records actual snapshot reads, filter misses, action errors and recovery without suppressing another rule',async()=>{
 const afterStop=[{kind:'publish',seq:3},{kind:'condition',seq:3,ruleId:'door:opened',passed:true},{kind:'stop'}];assert.equal(lastRuleExecution(afterStop,{id:'door'}).seq,3);assert.equal(lastRuleExecution(afterStop,{id:'counter'}),null);
 const project=p(),rt=runtime();createSignalDefinition(project,{id:'test.signal.journal',name:'Проверка',payloadSchema:{type:'object',properties:{value:{type:'number'}}}});project.objects.push({id:'source',name:'Источник',active:true});
 const bad=createRule({id:'invalid-condition',scope:{kind:'project',id:project.id},signal:'test.signal.journal',condition:{op:'div',a:1,b:0},actions:[]}),good=createRule({id:'valid-condition',scope:{kind:'project',id:project.id},signal:'test.signal.journal',condition:{op:'eq',a:{op:'get',path:'args.value'},b:2},actions:[{kind:'operation',call:{id:'sacura.condition.compare',version:1,parameters:{left:true,right:1,operator:'eq'}}}]}),miss=createRule({id:'missing-source',scope:{kind:'project',id:project.id},signal:'test.signal.journal',filter:{source:reference('object','source')},actions:[]});project.gameplay.rules.push(bad,good,miss);
 try{await rt.start(project);await rt.game.trigger('test.signal.journal',{value:2});await settle(rt);const t=rt.game.state.ruleTrace;assert.ok(t.some(e=>e.ruleId===bad.id&&e.kind==='condition-error'));assert.ok(t.some(e=>e.ruleId===miss.id&&e.kind==='match'&&/отсутствует источник/.test(e.reason)));assert.ok(t.some(e=>e.ruleId===good.id&&e.kind==='condition'&&e.passed&&e.readings.some(r=>r.path==='args.value'&&r.value===2)));assert.ok(t.some(e=>e.ruleId===good.id&&e.kind==='action-error'&&/одного типа/.test(e.error)));assert.ok(t.some(e=>e.ruleId===good.id&&e.kind==='finish'&&e.result==='ошибка'));assert.equal(lastRuleExecution(t,{ruleId:good.id}).seq,rt.game.ruleRuntime.sequence);
 for(let i=0;i<80;i++)await rt.game.trigger('test.signal.journal',{value:1});assert.ok(rt.game.state.ruleTrace.length<=JOURNAL_LIMIT);assert.equal(gameSave(rt).world.game.ruleTrace,undefined);rt.stop();assert.equal(rt.game.state.ruleTrace.at(-1).kind,'stop');
 }finally{rt.stop();}
});

test('an unknown capability blocks startup cleanly and never runs as an empty mock',async()=>{
 const project=p(),rt=runtime();project.gameplay.mechanics.push({id:'unknown',capabilityId:'uninstalled.module',capabilityVersion:1,scope:{kind:'project',id:project.id},values:{}});try{await rt.start(project);assert.equal(rt.snapshot.phase,'ERROR');assert.match(rt.snapshot.error,/недоступна/);assert.equal(rt.game.flights.size,0);}finally{rt.stop();}
});

test('a wrong typed Blueprint assignment fails before mutation and a deleted direct mechanic owner cannot resume its function',async()=>{
 const project=p(),rt=runtime();project.functions=[{id:'bad-assignment',name:'Тип',parameters:[],locals:{},entry:'set',nodes:[{id:'set',kind:'set',path:'global.count',value:'ошибка'}]}];try{await rt.start(project);await assert.rejects(rt.game.callFunction('bad-assignment'),/Тип переменной/);assert.equal(rt.snapshot.variables.count,0);assert.equal(rt.game.flights.size,0);
 const action={...makeAction('wait'),id:'pause',duration:.05};rt.project.events.push({id:'waiting-event',name:'Ожидание',groups:[{id:'g',actions:[action,{...makeAction('variable','count',7),duration:0}]}]});rt.project.objects.push({id:'owner',active:true});rt.project.functions.push({id:'owned-function',name:'Работа владельца',parameters:[],locals:{},entry:'wait',nodes:[{id:'wait',kind:'event',eventId:'waiting-event',next:'set'},{id:'set',kind:'set',path:'global.count',value:9}]});const d=createMechanicDefinition(rt.project,{name:'Владелец',scopes:['object'],logic:{functionId:'owned-function'},parameters:{type:'object',properties:{}}}),m=addMechanicInstance(rt.project,d.id,{kind:'object',id:'owner'});const task=rt.game.callMechanic(m.id).catch(e=>e.message);await waitFor(()=>Object.values(rt.snapshot.instances).some(i=>i.status==='running'));rt.project.objects=[];assert.equal(await task,'INSTANCE_STOPPED');assert.equal(rt.snapshot.variables.count,0);
 }finally{rt.stop();}
});

test('cold load restores without a start reward, pickup or entry Event and invalid cold load remains stopped',async()=>{
 const project=p(),rt=runtime(),cold=runtime(),store=new Map();const storage={getItem:k=>store.get(k),setItem:(k,v)=>store.set(k,v)};rt.storage=cold.storage=storage;project.gameplay.variableBindings.push({id:'cold-counter',key:'count'});project.gameplay.rules.push(createRule({signal:'start',scope:{kind:'project',id:project.id},once:true,actions:[{kind:'operation',call:{id:'sacura.variable.set',version:1,parameters:{variable:reference('variable','cold-counter'),value:1}}}]}));
 try{await rt.start(project);await settle(rt);rt.saveGame();assert.equal(rt.snapshot.variables.count,1);assert.equal(cold.running,false);await cold.uiAction('load',{},project);assert.equal(cold.running,true);assert.equal(cold.snapshot.variables.count,1);assert.equal(cold.game.ruleRuntime.sequence,0);assert.equal(cold.game.started,true);cold.stop();store.set('sacura-save:'+project.id,'{}');await cold.uiAction('load',{},project);assert.equal(cold.running,false);assert.match(cold.snapshot.hint,/не подходит/);}finally{rt.stop();cold.stop();}
});

test('new native timer, door and interaction installers use canonical scoped rules, while unknown authored definitions stay inert and diagnosed',async()=>{
 const project=p();project.objects.push({id:'door',name:'Дверь',active:true});for(const type of ['timer','door','interaction']){const m=addMechanic(project,type,{kind:type==='timer'?'project':'object',id:type==='timer'?project.id:'door'}),r=project.gameplay.rules.find(r=>r.id===m.ruleId);assert.equal(r.version,1);assert.equal(r.trigger,undefined);assert.equal(r.eventId,undefined);assert.ok(Array.isArray(r.actions));assert.deepEqual(r.scope,m.scope);}
 const rt=runtime();project.gameplay.mechanics.push({id:'missing',definitionId:'unavailable.mechanic',definitionVersion:99,scope:{kind:'project',id:project.id},values:{original:'preserved'}});const original=JSON.stringify(project);try{await rt.start(project);assert.equal(rt.snapshot.phase,'ERROR');assert.match(rt.snapshot.error,/версия механики недоступна/);assert.equal(JSON.stringify(project),original);}finally{rt.stop();}
});

test('save restores persistent static effects and held ownership without replay or asynchronous continuations',async()=>{
 const project=p(),rt=runtime();project.objects.push({id:'door',name:'Дверь',active:true});project.subscenes.push({...structuredClone(project.subscenes[0]),id:'other-scene',name:'Вторая сцена'});project.events.push({id:'persistent-door',name:'Открытая дверь',owner:'GameSession',retention:'KEEP_ALIVE',groups:[{id:'door-group',actions:[{...makeAction('door','door','Открыть'),duration:0}]}]});try{await rt.start(project);await rt.uiAction('event',{id:'held',eventId:'persistent-door'});const native=Object.values(rt.snapshot.instances).find(i=>i.eventId==='persistent-door'),s=gameSave(rt);await restoreGameSave(rt,s);assert.equal(rt.snapshot.instances[native.id].status,'held');assert.equal(rt.snapshot.world.doors.door,'Открыть');assert.equal(rt.game.ruleRuntime.active.size,0);assert.equal(rt.game.flights.size,0);await rt.enterScene('other-scene');assert.equal(rt.snapshot.world.doors.door,'Открыть');rt.controlInstance(native.id,'stop');assert.equal(rt.snapshot.world.doors.door,null);const corrupt=structuredClone(s);corrupt.retainedInstances[native.id].status='running';assert.throws(()=>validateGameSave(project,corrupt),/Удерживаемое/);}finally{rt.stop();}
});

test('a retained old GameSession facade cannot call native methods or sync state into a restarted session',async()=>{
 const project=p(),rt=runtime();try{await rt.start(project);const old=rt.game,world=old.world;await rt.start(project);assert.notEqual(old.world,rt.game.world);assert.equal(old.world,world);old.state.resources.stale=9;old.sync();old.tick(.05);assert.equal(rt.game.state.resources.stale,undefined);assert.throws(()=>old.command('startTimer',{seconds:1}),/CANCELLED/);assert.throws(()=>old.startTimer({seconds:1}),/CANCELLED/);assert.deepEqual(rt.game.state.timers,{});}finally{rt.stop();}
});

test('trusted module disposal is synchronous, idempotent and one cleanup failure cannot prevent other modules or stop',async()=>{
 const cleared=[];registerCapability({id:'test.integration.bad-cleanup',dispose(){cleared.push('bad');throw Error('Ошибка тестовой очистки');}});registerCapability({id:'test.integration.good-cleanup',dispose(){cleared.push('good');}});const project=p(),rt=runtime();await rt.start(project);const save=gameSave(rt);const old=rt.game;rt.stop();assert.deepEqual(cleared,['bad','good']);assert.equal(rt.running,false);assert.equal(old.ruleRuntime.closed,true);assert.ok(old.state.ruleTrace.some(e=>e.kind==='cleanup-error'&&e.moduleId==='test.integration.bad-cleanup'));old.dispose();assert.equal(cleared.length,2);assert.equal(save.world.game.ruleTrace,undefined);
});
