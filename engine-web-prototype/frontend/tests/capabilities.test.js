import {handlerOf} from './fixtures/packageDoor.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createMechanicsProject,parseProjectFile,copyProjectAs} from '../src/projectLifecycle.js';
import {addMechanic,connectMechanicInput} from '../src/mechanicsModel.js';
import {ensureGameplay,validateGameplay,newItem} from '../src/gameplayModel.js';
import {makeAction} from '../src/model.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {reference,validateValue} from '../src/valueSchemas.js';
import {createRule} from '../src/ruleModel.js';
import {registerCapability,capabilityDefinitions} from '../src/capabilityRegistry.js';
import {operationDefinitions,validateOperationCall} from '../src/operationRegistry.js';
import {widgetValue} from '../src/widgetBindings.js';
import {availableCapabilityBindings,validateCapabilityBinding} from '../src/capabilityBindings.js';
import {gameSave,restoreGameSave,validateGameSave} from '../src/gameSave.js';
import {readProject} from '../src/projectFiles.js';
import {exportAuthoringPackage,planPackageImport} from '../src/mechanicPackages.js';

function fixture(){const p=createMechanicsProject('Независимые возможности'),s=p.subscenes[0];s.kind='empty';for(const [id,x]of [['a',0],['b',4],['target',0]])p.objects.push({id,name:id,type:'Меш',active:true,subsceneId:s.id,collision:{enabled:true,custom:true,size:[1,2,1],offset:[0,1,0]},transforms:{[s.id]:{position:[x,0,id==='target'?-3:0],rotation:[0,0,0],scale:[1,1,1]}}});return p;}
const scope=id=>({kind:'object',id});
const rt=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('Этот тест не воспроизводит звук.');}));
const call=(id,parameters)=>({id:'sacura.'+id,version:1,parameters});
const owner=id=>reference('object',id);
async function settled(runtime){for(let n=0;n<200&&!runtime.game.ruleRuntime.idle;n++)await new Promise(r=>setTimeout(r,2));assert.equal(runtime.game.ruleRuntime.idle,true);assert.equal(runtime.snapshot.error,null);}
const binding=(capability,id,field,valueType,recordId)=>({source:'capability',capability:'sacura.'+capability,owner:owner(id),field,valueType,...(recordId?{recordId}:{})});

test('capabilities: registered movement drives existing bodies for primary and additional owners without combat',async()=>{
 const p=fixture();addMechanic(p,'movement',scope('a'));p.gameplay.controller.mode='wasd';p.gameplay.movements.push({ownerId:'b',mode:'wasd',speed:2,jumpSpeed:4,gravity:9.8,eyeHeight:1.6});
 const runtime=rt();try{await runtime.start(p);for(const id of ['a','b'])await runtime.game.execute(call('movement.direction',{owner:owner(id),direction:[0,0,-1]}));runtime.game.tick(.05);
 assert.ok(runtime.snapshot.world.positions.a[2]<0);assert.ok(runtime.snapshot.world.positions.b[2]<0);assert.deepEqual(runtime.game.state.actors,{});assert.deepEqual(runtime.game.state.weapons,{});
 await runtime.game.execute(call('movement.enabled',{owner:owner('a'),enabled:false}));const before=[...runtime.snapshot.world.positions.a];runtime.game.tick(.05);assert.deepEqual(runtime.snapshot.world.positions.a,before);assert.equal(runtime.snapshot.world.playerControl,null);
 await runtime.game.execute(call('movement.enabled',{owner:owner('a'),enabled:true}));assert.equal(runtime.snapshot.world.playerControl.characterId,'a');runtime.game.tick(.05);assert.ok(runtime.snapshot.world.positions.a[2]<before[2]);
 }finally{runtime.stop();}
});

test('capabilities: a chained damage action publishes the changed owner as its target',async()=>{
 const p=fixture();addMechanic(p,'health',scope('a'));addMechanic(p,'health',scope('b'));const counter=addMechanic(p,'resource',{kind:'project',id:p.id});p.gameplay.rules.push(createRule({signal:'death',scope:scope('a'),filter:{target:owner('a')},actions:[{kind:'operation',call:call('resource.add',{owner:reference('project',p.id),resource:reference('resource',counter.recordId),amount:1})}]}));
 const runtime=rt();try{await runtime.start(p);await runtime.game.execute(call('health.damage',{owner:owner('a'),amount:100}),{target:{type:'object',id:'b'},source:{type:'object',id:'b'}});await settled(runtime);assert.equal(runtime.game.state.resources[counter.recordId],1);assert.equal(runtime.game.state.actors.b.hp,100);
 }finally{runtime.stop();}
});

test('capabilities: UI-only project with a scene inventory and counter has no hero, health, weapon or camera preset',async()=>{
 const p=createMechanicsProject('Интерфейс'),scene=p.subscenes[0];addMechanic(p,'inventory',{kind:'scene',id:scene.id});const resource=addMechanic(p,'resource',{kind:'project',id:p.id});
 const runtime=rt();try{await runtime.start(p);assert.equal(runtime.snapshot.phase,'PLAYING');assert.equal(runtime.game.controlledId,'');assert.deepEqual(runtime.game.state.actors,{});assert.deepEqual(runtime.game.state.weapons,{});assert.equal(runtime.game.state.weaponId,undefined);assert.equal(runtime.game.state.kills,undefined);
 await runtime.game.execute(call('resource.add',{owner:reference('project',p.id),resource:reference('resource',resource.recordId),amount:3}));assert.equal(runtime.game.state.resources[resource.recordId],3);assert.deepEqual(validateGameplay(runtime.project),[]);
 }finally{runtime.stop();}
});

test('capabilities: inventory-only owners have independent stacks and no weapon or health state',async()=>{
 const p=fixture();addMechanic(p,'inventory',scope('a'));addMechanic(p,'inventory',scope('b'));p.gameplay.items.push({...newItem(),id:'key',stackLimit:2});
 const runtime=rt();try{await runtime.start(p);await runtime.game.execute(call('inventory.add',{owner:owner('a'),item:reference('item','key'),count:2}));
 assert.equal(runtime.game.count('key','a'),2);assert.equal(runtime.game.count('key','b'),0);assert.deepEqual(runtime.game.state.weapons,{});assert.deepEqual(runtime.game.state.actors,{});
 runtime.game.removeItem('key',1,'a');assert.equal(runtime.game.count('key','a'),1);assert.equal(runtime.game.count('key','b'),0);
 assert.equal(await runtime.game.execute(call('inventory.count',{owner:owner('a'),item:reference('item','key')})),1);
 }finally{runtime.stop();}
});

test('capabilities: the same shoot operation uses each explicit owner and its own magazine, reload and health',async()=>{
 const p=fixture();for(const id of ['a','b','target'])addMechanic(p,'health',scope(id));p.gameplay.actors.a.maxHP=40;p.gameplay.actors.b.maxHP=90;
 const wa=addMechanic(p,'weapon',scope('a')),wb=addMechanic(p,'weapon',scope('b'));Object.assign(p.gameplay.weapons.find(w=>w.id===wa.recordId),{magazine:2,reserve:3,reloadTime:.05});Object.assign(p.gameplay.weapons.find(w=>w.id===wb.recordId),{magazine:5,reserve:9});
 const runtime=rt();try{await runtime.start(p);assert.equal(await runtime.game.execute(call('weapon.shoot',{owner:owner('a'),direction:[0,0,-1]})),true);assert.equal(await runtime.game.execute(call('weapon.shoot',{owner:owner('b'),direction:[0,1,0]})),true);
 assert.equal(runtime.game.state.actors.target.hp,75);assert.equal(runtime.game.state.actors.a.hp,40);assert.equal(runtime.game.state.actors.b.hp,90);assert.equal(runtime.game.state.weapons[wa.recordId].ammo,1);assert.equal(runtime.game.state.weapons[wb.recordId].ammo,4);
 await runtime.game.execute(call('health.damage',{owner:owner('b'),amount:10}));assert.equal(runtime.game.state.actors.b.hp,80);assert.equal(runtime.game.state.actors.a.hp,40);
 assert.equal(await runtime.game.execute(call('weapon.reload',{owner:owner('a')})),true);runtime.game.tick(.05);assert.equal(runtime.game.state.weapons[wa.recordId].ammo,2);assert.equal(runtime.game.state.weapons[wa.recordId].reserve,2);assert.equal(runtime.game.state.weapons[wb.recordId].reserve,9);
 assert.equal(await runtime.game.execute(call('weapon.shoot',{owner:owner('b'),weapon:reference('weapon',wa.recordId)})),false);
 }finally{runtime.stop();}
});

test('capabilities: first/third person cameras and a reticle do not install movement, weapon, health or inventory',async()=>{
 const p=fixture();addMechanic(p,'camera',scope('a'));p.gameplay.camera.mode='thirdPerson';addMechanic(p,'crosshair',{kind:'project',id:p.id});
 const runtime=rt();try{await runtime.start(p);assert.equal(runtime.game.state.camera.mode,'thirdPerson');assert.deepEqual(runtime.game.state.weapons,{});assert.deepEqual(runtime.game.state.actors,{});assert.deepEqual(runtime.game.state.inventories,{});assert.ok(runtime.snapshot.world.playerControl==null);assert.equal(runtime.game.shoot(),false);assert.equal(runtime.project.gameplay.crosshair.enabled,true);}
 finally{runtime.stop();}
});

test('capabilities: preset has no runtime side effects in the new schema and missing new camera stays authored',async()=>{
 const p=fixture();p.gameplay.preset='fps';delete p.gameplay.camera;ensureGameplay(p);const runtime=rt();try{await runtime.start(p);assert.equal(p.gameplay.camera.mode,'authored');assert.deepEqual(runtime.game.state.actors,{});assert.deepEqual(runtime.game.state.weapons,{});assert.equal(runtime.game.state.camera.mode,'authored');assert.notEqual(runtime.project.gameplay.crosshair?.enabled,true);}
 finally{runtime.stop();}
});

test('capabilities: death only commits health and signals; controls, visibility, counters and victory require rules',async()=>{
 const p=fixture();addMechanic(p,'movement',scope('a'));p.gameplay.controller.mode='wasd';addMechanic(p,'health',scope('a'));p.gameplay.actors.a.team='enemy';
 const runtime=rt();try{await runtime.start(p);await runtime.game.execute(call('health.damage',{owner:owner('a'),amount:100}));await settled(runtime);
 assert.equal(runtime.game.state.actors.a.dead,true);assert.notEqual(runtime.snapshot.world.visible.a,false);assert.equal(runtime.snapshot.world.playerControl.characterId,'a');assert.equal(runtime.game.state.disabled.a,undefined);assert.equal(runtime.game.state.kills,undefined);assert.equal(runtime.snapshot.phase,'PLAYING');assert.equal(runtime.game.state.ruleTrace.some(x=>x.type==='victory'),false);
 assert.equal(runtime.game.damage('a',1),false);
 }finally{runtime.stop();}
});

test('capabilities: ordinary death rules can disable controls, hide a body and update an explicit project resource',async()=>{
 const p=fixture();addMechanic(p,'movement',scope('a'));addMechanic(p,'health',scope('a'));const r=addMechanic(p,'resource',{kind:'project',id:p.id});
 p.gameplay.rules.push(createRule({signal:'death',scope:scope('a'),filter:{target:owner('a')},actions:[{kind:'operation',call:call('movement.enabled',{owner:{kind:'reference',type:'object',mode:'context',role:'self'},enabled:false})},{kind:'operation',call:call('object.visible',{object:owner('a'),visible:false})},{kind:'operation',call:call('resource.add',{owner:reference('project',p.id),resource:reference('resource',r.recordId),amount:1})}]}));
 const runtime=rt();try{await runtime.start(p);runtime.game.damage('a',100);await settled(runtime);assert.equal(runtime.snapshot.world.playerControl,null);assert.equal(runtime.snapshot.world.visible.a,false);assert.equal(runtime.game.state.resources[r.recordId],1);}finally{runtime.stop();}
});

test('capabilities: familiar global names stay untouched until explicitly bound; HUD reads typed owner values',async()=>{
 const p=fixture();p.variables={hp:777,ammo:888,kills:999};p.variableTypes={hp:'number',ammo:'integer',kills:'integer'};addMechanic(p,'health',scope('a'));addMechanic(p,'health',scope('b'));const w=addMechanic(p,'weapon',scope('a'));
 const runtime=rt();try{await runtime.start(p);runtime.game.damage('a',25);await runtime.game.execute(call('weapon.shoot',{owner:owner('a'),direction:[0,1,0]}));assert.deepEqual(runtime.snapshot.variables,{hp:777,ammo:888,kills:999});
 const context={project:runtime.project,game:runtime.game.state};assert.equal(widgetValue(context,binding('health','a','hp','number')),75);assert.equal(widgetValue(context,binding('health','b','hp','number')),100);assert.equal(widgetValue(context,binding('weapon','a','ammo','integer',w.recordId)),5);
 const hp=runtime.project.gameplay.variableBindings.find(v=>v.key==='hp');runtime.project.gameplay.stateBindings.push({...binding('health','a','hp','number'),id:'explicit-hp',variable:reference('variable',hp.id)});runtime.game.sync();assert.equal(runtime.snapshot.variables.hp,75);assert.equal(runtime.snapshot.variables.ammo,888);
 assert.ok(availableCapabilityBindings(runtime.project).some(c=>c.binding.owner.id==='b'));assert.ok(validateCapabilityBinding(runtime.project,{...binding('health','a','hp','number'),valueType:'string'}).length);
 }finally{runtime.stop();}
});

test('capabilities: a pickup selects its receiver and publishes only after inventory and visibility commit',async()=>{
 const p=fixture();addMechanic(p,'inventory',scope('a'));addMechanic(p,'inventory',scope('b'));const m=addMechanic(p,'pickup',scope('target')),item=p.gameplay.items.find(i=>i.id===m.recordId);item.inventoryOwner=scope('b');
 const runtime=rt();try{await runtime.start(p);runtime.snapshot.world.positions.b=[0,0,-3];assert.equal(await runtime.game.execute(call('interaction.pickup',{owner:owner('b'),object:owner('target')})),true);await settled(runtime);assert.equal(runtime.game.count(item.id,'b'),1);assert.equal(runtime.game.count(item.id,'a'),0);assert.equal(runtime.snapshot.world.visible.target,false);
 const published=runtime.game.state.ruleTrace.find(x=>x.type==='pickup');assert.ok(published);assert.deepEqual(runtime.game.state.weapons,{});
 }finally{runtime.stop();}
});

test('capabilities: pursuit has a declared target, independent cooldown and does not need health or default to a hero',async()=>{
 const p=fixture();addMechanic(p,'movement',scope('a'));addMechanic(p,'health',scope('a'));addMechanic(p,'pursuit',scope('b'));const runtime=rt();try{await runtime.start(p);runtime.snapshot.world.positions.b=[0,0,-1];runtime.game.tick(.05);assert.equal(runtime.game.state.actors.a.hp,100);assert.equal(runtime.game.state.actors.b,undefined);
 runtime.game.config.actors.b.ai.targetId='a';runtime.game.tick(.05);assert.equal(runtime.game.state.actors.a.hp,90);assert.ok(runtime.game.state.behaviors.b.nextAttack>0);runtime.game.tick(.05);assert.equal(runtime.game.state.actors.a.hp,90);
 }finally{runtime.stop();}
});

test('capabilities: timers share game time but different owners do not overwrite each other',async()=>{
 const p=fixture(),runtime=rt();try{await runtime.start(p);for(const id of ['a','b'])await runtime.game.execute(call('timer.start',{owner:owner(id),name:'Сигнал',seconds:id==='a'?.05:.1}));assert.equal(Object.keys(runtime.game.state.timers).length,2);
 runtime.togglePause();runtime.game.tick(.05);assert.equal(runtime.game.state.clock,0);runtime.togglePause();runtime.game.tick(.05);await settled(runtime);assert.equal(Object.keys(runtime.game.state.timers).length,1);assert.equal(Object.values(runtime.game.state.timers)[0].owner.id,'b');
 runtime.project.objects=runtime.project.objects.filter(o=>o.id!=='b');runtime.game.tick(.05);assert.deepEqual(runtime.game.state.timers,{});assert.equal(runtime.game.state.ruleTrace.filter(x=>x.type==='timer').length,1);
 }finally{runtime.stop();}
});

test('capabilities: input actions invoke a registered owner operation and removing a mechanic removes its input routing',async()=>{
 const p=fixture(),m=addMechanic(p,'weapon',scope('b'));connectMechanicInput(p,m);const action=p.input.actions.find(a=>a.mechanicId===m.id&&a.operation.id==='sacura.weapon.shoot');assert.equal(action.behavior,'operation');const runtime=rt();try{await runtime.start(p);await runtime.inputAction({actionId:action.id,phase:'triggered',value:true,device:'pointer'});assert.equal(runtime.game.state.weapons[m.recordId].ammo,5);assert.equal(runtime.game.state.actors.b,undefined);}finally{runtime.stop();}
});

test('capabilities: typed number/vector/ref schemas reject bad assignments before side effects',async()=>{
 const p=fixture();addMechanic(p,'health',scope('a'));assert.ok(validateOperationCall(call('health.damage',{owner:owner('a'),amount:-1}),{project:p}).length);assert.ok(validateOperationCall(call('weapon.shoot',{owner:owner('a'),direction:[0,Infinity,1]}),{project:p}).length);assert.ok(validateOperationCall(call('health.damage',{owner:reference('scene',p.subscenes[0].id)}),{project:p}).length);
 const runtime=rt();try{await runtime.start(p);await assert.rejects(runtime.game.execute(call('health.damage',{owner:owner('a'),amount:NaN})),/число/);assert.equal(runtime.game.state.actors.a.hp,100);}finally{runtime.stop();}
});

test('capabilities: module registration extends the shared operation library without central dispatch or React',async()=>{
 const id='test.capability.echo';registerCapability({id,operations:[{definition:{id:id+'.run',parameters:{type:'object',properties:{value:{type:'integer',default:1}}},result:{type:'integer'},editor:{label:'Тестовая возможность'}},execute:p=>p.value}]});assert.ok(capabilityDefinitions().some(d=>d.id===id));assert.ok(operationDefinitions().some(d=>d.id===id+'.run'));assert.throws(()=>registerCapability({id}),/зарегистрирована/);assert.throws(()=>registerCapability({id:'test.capability.duplicate',operations:[{definition:{id:id+'.run'}}]}),/Повтор ID/);assert.ok(!capabilityDefinitions().some(d=>d.id==='test.capability.duplicate'));assert.throws(()=>registerCapability({id:'test.capability.invalid',update:true}),/Lifecycle/);
 const runtime=rt();try{await runtime.start(createMechanicsProject());assert.equal(await runtime.game.command(id+'.run',{value:4}),4);}finally{runtime.stop();}
});

test('capabilities: legacy shooter migration is repeatable, editable and preserves bindings when copying a project',async()=>{
 const p=parseProjectFile(readFileSync(new URL('./fixtures/legacy-shooter.sacura-template.json',import.meta.url),'utf8')).project,before=JSON.stringify(p);ensureGameplay(p);assert.equal(JSON.stringify(p),before);
 const death=p.gameplay.rules.find(r=>r.id==='legacy-death:enemy-1');assert.equal(death.version,1);assert.ok(handlerOf(p,death).nodes.some(n=>n.kind==='operation'&&n.call.id==='sacura.object.visible'));p.gameplay.rules=p.gameplay.rules.filter(r=>r.id!==death.id);p.gameplay.stateBindings=p.gameplay.stateBindings.filter(b=>b.id!=='legacy-binding-hp');ensureGameplay(p);assert.equal(p.gameplay.rules.some(r=>r.id===death.id),false);assert.equal(p.gameplay.stateBindings.some(b=>b.id==='legacy-binding-hp'),false);
 const copy=copyProjectAs(p,'Другой шутер');assert.deepEqual(validateGameplay(copy),[]);assert.ok(copy.gameplay.stateBindings.filter(b=>b.owner.type==='project').every(b=>b.owner.id===copy.id));assert.deepEqual(readProject(JSON.stringify(copy)),copy);
});

test('capabilities: unknown capability schema is diagnosed without deleting data',()=>{
 const p=fixture();p.gameplay.capabilitySchemaVersion=99;p.gameplay.future={keep:'данные'};const before=JSON.stringify(p);ensureGameplay(p);assert.equal(JSON.stringify(p),before);assert.ok(validateGameplay(p).some(i=>i.detail.includes('Версия возможностей')));
});

test('capabilities: saving restores every owner independently without sharing item arrays or replaying starter items',async()=>{
 const p=fixture();for(const id of ['a','b']){addMechanic(p,'health',scope(id));addMechanic(p,'inventory',scope(id));addMechanic(p,'weapon',scope(id));}p.gameplay.items.push({...newItem(),id:'token'});
 const runtime=rt();try{await runtime.start(p);runtime.game.damage('a',30);runtime.game.addItem('token',2,{ownerId:'a',publish:false});runtime.game.addItem('token',1,{ownerId:'b',publish:false});await runtime.game.execute(call('weapon.shoot',{owner:owner('b'),direction:[0,1,0]}));await settled(runtime);const saved=JSON.parse(JSON.stringify(gameSave(runtime)));await restoreGameSave(runtime,saved);
 assert.equal(runtime.game.state.actors.a.hp,70);assert.equal(runtime.game.state.actors.b.hp,100);assert.equal(runtime.game.count('token','a'),2);assert.equal(runtime.game.count('token','b'),1);runtime.game.removeItem('token',1,'a');assert.equal(runtime.game.count('token','b'),1);assert.equal(runtime.game.state.weapons[p.gameplay.weapons[1].id].ammo,5);
 const bad=structuredClone(saved);bad.world.game.selectedWeapons.b=p.gameplay.weapons[0].id;assert.throws(()=>validateGameSave(runtime.project,bad),/другому владельцу/);
 }finally{runtime.stop();}
});

test('capabilities: an old flat shooter save adapts inventory, selected weapon and AI without replay',async()=>{
 const p=parseProjectFile(readFileSync(new URL('./fixtures/legacy-shooter.sacura-template.json',import.meta.url),'utf8')).project,runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);await runtime.advance();runtime.game.removeItem('medkit');await settled(runtime);const saved=JSON.parse(JSON.stringify(gameSave(runtime)));for(const k of ['capabilitySchemaVersion','inventories','selectedWeapons','resources','behaviors','disabled','directions'])delete saved.world.game[k];await restoreGameSave(runtime,saved);assert.equal(runtime.game.count('medkit'),0);assert.equal(runtime.game.state.weaponId,'pistol');assert.equal(runtime.game.state.capabilitySchemaVersion,1);assert.ok(runtime.game.state.behaviors['enemy-1']);const dead=structuredClone(saved),hero=p.gameplay.playerId;dead.world.game.actors[hero].hp=0;dead.world.game.actors[hero].dead=true;await restoreGameSave(runtime,dead);assert.equal(runtime.game.state.disabled[hero],true);assert.equal(runtime.game.state.weapons.pistol.enabled,false);assert.equal(runtime.snapshot.world.playerControl,null);
 }finally{runtime.stop();}
});

test('capabilities: original native door exchange remains portable after explicit recipient ownership',async()=>{
 const a=fixture(),door=addMechanic(a,'pickup',scope('target'));const pkg=await exportAuthoringPackage(a,door,{name:'Предмет',author:'Автор',license:'CC0-1.0'}),b=fixture();const next=(await planPackageImport(b,pkg,Object.fromEntries(pkg.inputs.map(i=>[i.id,'target'])))).apply(b);assert.equal(next.gameplay.items[0].inventoryOwner.id,b.id);const runtime=rt();try{await runtime.start(next);assert.equal(runtime.game.interact('target'),true);await settled(runtime);assert.equal(runtime.game.count(next.gameplay.items[0].id),1);}finally{runtime.stop();}
});

test('capabilities: native inventory package carries item dependencies and rebinds the existing owner without health',async()=>{
 const a=fixture(),m=addMechanic(a,'inventory',scope('a'));a.gameplay.items.push({...newItem(),id:'token'});a.gameplay.inventories[0].slots=3;a.gameplay.inventories[0].starting=[{itemId:'token',count:2}];
 const pkg=await exportAuthoringPackage(a,m,{name:'Инвентарь',author:'Автор',license:'CC0-1.0'});assert.equal(pkg.content.gameplay.items.length,1);assert.equal(pkg.content.gameplay.inventories.length,1);assert.equal(pkg.content.objects,undefined);assert.ok(pkg.compatibility.requiredCapabilities.some(c=>c.id==='capability.sacura.inventory'));
 const b=fixture(),next=(await planPackageImport(b,pkg,Object.fromEntries(pkg.inputs.map(i=>[i.id,'b'])))).apply(b),runtime=rt();try{await runtime.start(next);assert.equal(runtime.game.count(next.gameplay.items[0].id,'b'),2);assert.equal(next.gameplay.inventories[0].slots,3);assert.deepEqual(runtime.game.state.actors,{});assert.deepEqual(runtime.game.state.weapons,{});}finally{runtime.stop();}
 await assert.rejects(planPackageImport(next,pkg,Object.fromEntries(pkg.inputs.map(i=>[i.id,'b']))),/уже подключён/);
});

test('capabilities: a project inventory transfers with its capacity and startup items',async()=>{
 const a=fixture(),m=addMechanic(a,'inventory',{kind:'project',id:a.id});a.gameplay.items.push({...newItem(),id:'token'});a.gameplay.inventory.slots=2;a.gameplay.inventory.starting=[{itemId:'token',count:3}];const pkg=await exportAuthoringPackage(a,m,{name:'Инвентарь проекта',author:'Автор',license:'CC0-1.0'}),b=fixture(),next=(await planPackageImport(b,pkg,{})).apply(b);assert.equal(next.gameplay.inventory.owner.id,b.id);const runtime=rt();try{await runtime.start(next);assert.equal(runtime.game.count(next.gameplay.items[0].id),3);assert.equal(next.gameplay.inventory.slots,2);}finally{runtime.stop();}
});

test('capabilities: a camera package preserves its operation input and independent camera settings',async()=>{
 const a=fixture(),m=addMechanic(a,'camera',scope('a'));a.gameplay.cameraSettings.distance=6;const pkg=await exportAuthoringPackage(a,m,{name:'Камера',author:'Автор',license:'CC0-1.0'}),b=fixture(),next=(await planPackageImport(b,pkg,Object.fromEntries(pkg.inputs.map(i=>[i.id,'b'])))).apply(b),runtime=rt();assert.equal(next.gameplay.cameraSettings.distance,6);const action=next.input.actions.find(a=>a.operation?.id==='sacura.camera.look');assert.ok(action);assert.equal(action.mechanicId,next.gameplay.mechanics[0].id);try{await runtime.start(next);await runtime.inputAction({actionId:action.id,phase:'triggered',value:[10,0],device:'pointer'});assert.ok(runtime.game.state.yaw<0);assert.deepEqual(runtime.game.state.weapons,{});}finally{runtime.stop();}
});

test('capabilities: integer resource and reticle packages stay ordinary native data with project ownership',async()=>{
 const a=fixture(),counter=addMechanic(a,'resource',{kind:'project',id:a.id});a.gameplay.resources[0].initial=7;const metadata={name:'Ресурс',author:'Автор',license:'CC0-1.0'},pkg=await exportAuthoringPackage(a,counter,metadata),b=fixture(),next=(await planPackageImport(b,pkg,{})).apply(b);assert.equal(next.gameplay.resources[0].owner.id,b.id);assert.equal(next.gameplay.resources[0].initial,7);
 const crosshair=addMechanic(a,'crosshair',{kind:'project',id:a.id}),reticle=await exportAuthoringPackage(a,crosshair,{...metadata,name:'Прицел'}),copied=(await planPackageImport(b,reticle,{})).apply(b);assert.equal(copied.gameplay.crosshair.enabled,true);assert.equal(copied.gameplay.camera.mode,'authored');
});

test('capabilities: existing expression and function execution can read a selected inventory owner',async()=>{
 const p=fixture();addMechanic(p,'inventory',scope('a'));addMechanic(p,'inventory',scope('b'));p.gameplay.items.push({...newItem(),id:'token'});p.functions.push({id:'count',name:'Количество',parameters:[],locals:{},entry:'return',nodes:[{id:'return',kind:'return',value:{op:'itemCount',itemId:'token',owner:'b'}}]});
 const runtime=rt();try{await runtime.start(p);runtime.game.addItem('token',3,{ownerId:'b',publish:false});assert.equal(await runtime.game.callFunction('count'),3);assert.equal(runtime.game.count('token','a'),0);}finally{runtime.stop();}
});

test('capabilities: owner deletion cancels an inventory use Event before its late variable change',async()=>{
 const p=fixture();p.variables.finished=false;addMechanic(p,'inventory',scope('a'));p.gameplay.items.push({...newItem(),id:'token',useEventId:'use'});p.events.push({id:'use',name:'Применить',owner:'GameSession',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'wait',actions:[{...makeAction('wait'),duration:.05}]},{id:'set',actions:[makeAction('variable','finished',true)]}]});
 const runtime=rt();try{await runtime.start(p);runtime.game.addItem('token',1,{ownerId:'a',publish:false});const task=runtime.game.execute(call('inventory.use',{owner:owner('a'),item:reference('item','token')}));while(!Object.values(runtime.snapshot.instances).some(i=>i.eventId==='use'))await new Promise(r=>setTimeout(r,1));runtime.project.objects=runtime.project.objects.filter(o=>o.id!=='a');await assert.rejects(task,/INSTANCE_STOPPED/);assert.equal(runtime.snapshot.variables.finished,false);assert.equal(runtime.running,true);}finally{runtime.stop();}
});

test('capabilities: scene change cancels reload and old projectiles but leaves other owner resources intact',async()=>{
 const p=fixture(),m=addMechanic(p,'weapon',scope('a'));p.gameplay.weapons[0].mode='projectile';p.gameplay.weapons[0].reloadTime=1;
 const runtime=rt();try{await runtime.start(p);await runtime.game.execute(call('weapon.shoot',{owner:owner('a'),direction:[0,1,0]}));await runtime.game.execute(call('weapon.reload',{owner:owner('a')}));assert.ok(runtime.game.state.weapons[m.recordId].reloadUntil>0);const scene={...structuredClone(p.subscenes[0]),id:'other-scene',name:'Другая сцена'};runtime.project.subscenes.push(scene);await runtime.enterScene(scene.id);runtime.game.tick(.05);assert.equal(runtime.game.state.weapons[m.recordId].reloadUntil,0);assert.equal(runtime.game.state.weapons[m.recordId].ammo,5);assert.deepEqual(runtime.game.state.projectiles,[]);}finally{runtime.stop();}
});
