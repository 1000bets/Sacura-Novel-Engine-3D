import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {instantiateAuthoringTemplate} from '../src/authoringTemplates.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {gameSave,restoreGameSave} from '../src/gameSave.js';
import {preparePlayableGame} from '../src/gameExport.js';
import {exportAuthoringPackage} from '../src/mechanicPackages.js';
import {editComponentSource,publishComponentSource} from '../src/componentResources.js';
import {validateStudio} from '../src/studioModel.js';
import * as THREE from 'three';
import {createScenePhysics} from '../src/scenePhysics.js';
const turn=()=>new Promise(r=>setTimeout(r,2));
async function settle(rt){for(let n=0;n<500;n++){if(rt.game.ruleRuntime.idle&&rt.game.flights.size===0)return;await turn();}throw Error('Runtime did not settle');}
async function drive(rt,seconds){for(let n=0;n<Math.ceil(seconds/.05);n++){rt.game.tick(.05);await turn();}}
const load=async name=>instantiateAuthoringTemplate(JSON.parse(await readFile('public/game-templates/'+name+'.sacura-template.json','utf8')),undefined,{reviewedSources:true});
const byName=(p,name)=>p.gameplay.mechanics.find(m=>m.name===name);
const local=(rt,m)=>rt.game.state.mechanicStates[m.id];
const runtime=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio requested');}));
const press=async(rt,p,id)=>{const names={'coin-button':'Получить монету','upgrade-button':'Улучшить (+1 за тик)','stop-button':'Остановить таймер','start-button':'Запустить таймер'},e=p.widgets.flatMap(w=>w.elements).find(e=>e.id===id||e.name===names[id]);assert.ok(e);await rt.uiAction(e.action,e);await settle(rt);};
test('horror WASD movement remains free beside the desk with the renderer collision adapter, and mouse payload turns the actual camera',async()=>{
 const p=await load('horror'),rt=runtime(),scene=new THREE.Scene(),sceneId=p.subscenes[0].id,meshes=p.objects.map(o=>{const mesh=new THREE.Mesh(new THREE.BoxGeometry(.5,.5,.5),new THREE.MeshBasicMaterial());const t=o.transforms[sceneId];mesh.position.fromArray(t.position);mesh.scale.fromArray(t.scale);mesh.rotation.set(...t.rotation.map(THREE.MathUtils.degToRad));mesh.userData.id=o.id;scene.add(mesh);return mesh;}),physics=createScenePhysics(scene,()=>({objects:p.objects,sceneId}),()=>meshes);rt.physicsApi={current:physics};
 try{await rt.start(p,p.subscenes[0].entry);await rt.advance();for(let n=0;n<10;n++)rt.playerStep([-1,0,0],.05);assert.ok(rt.game.position(p.gameplay.controller.ownerId)[0]<-1.3);for(let n=0;n<10;n++)rt.playerStep([0,0,-1],.05);assert.ok(rt.game.position(p.gameplay.controller.ownerId)[2]<5.2);
  const yaw=rt.game.state.yaw,pitch=rt.game.state.pitch,look=p.input.actions.find(a=>a.name==='Осмотреться правой кнопкой');await rt.inputAction({actionId:look.id,phase:'triggered',device:'pointer',value:[.8,.6],payload:{delta:[50,-20]}});assert.ok(rt.game.state.yaw<yaw-.1);assert.ok(rt.game.state.pitch>pitch+.05);
 }finally{rt.stop();physics.dispose();for(const m of meshes){m.geometry.dispose();m.material.dispose();}}
});
test('clicker upgrade changes the actual button and timer through one Reward; stop, restart and saved timer never duplicate awards',async()=>{
 const p=await load('ui-clicker'),rt=runtime(),coins=byName(p,'Монеты'),timer=byName(p,'Периодическая награда');
 try{await rt.start(p);await settle(rt);await press(rt,p,'coin-button');assert.equal(local(rt,coins).value,1);
  await press(rt,p,'upgrade-button');await press(rt,p,'coin-button');assert.equal(local(rt,coins).value,3,'manual reward uses upgraded rate');
  await drive(rt,1.05);await settle(rt);assert.equal(local(rt,coins).value,5);
  await press(rt,p,'stop-button');await drive(rt,2);await settle(rt);assert.equal(local(rt,coins).value,5);
  await press(rt,p,'start-button');await press(rt,p,'start-button');assert.equal(Object.keys(rt.game.state.timers).length,1);
  const save=JSON.parse(JSON.stringify(gameSave(rt)));await restoreGameSave(rt,save);assert.equal(Object.keys(rt.game.state.timers).length,1);assert.equal(local(rt,coins).value,5);
  await drive(rt,1.05);await settle(rt);assert.equal(local(rt,coins).value,7);assert.equal(local(rt,timer).rate,2);
 }finally{rt.stop();}
});
async function interact(rt,p,name){const o=p.objects.find(o=>o.name===name),hero=p.gameplay.controller.ownerId,pos=rt.game.position(o.id);rt.snapshot.world.positions[hero]=[pos[0]===0?1:pos[0]>0?pos[0]-1:pos[0]+1,0,pos[2]+.6];rt.playerClick(pos,o.id);if(name==='Зелёная дверь выхода')await drive(rt,1.3);await settle(rt);}
test('horror plays the real addressed interactions, independent props, puzzle, awaited exit and saved progress after portable import',async()=>{
 const p=await load('horror');assert.deepEqual(validateStudio(p),[]);assert.equal(p.gameplay.camera.mode,'thirdPerson');const rt=runtime(),room=byName(p,'Правила ночного обхода'),lock=byName(p,'Замок выхода');
 try{await rt.start(p,p.subscenes[0].entry);await rt.advance();await settle(rt);assert.equal(rt.snapshot.error,null);assert.equal(local(rt,lock).IsLocked,true);
  const fuse=p.objects.find(o=>o.name==='Керамический предохранитель');rt.playerClick(rt.game.position(fuse.id),fuse.id);await settle(rt);assert.equal(local(rt,room).fuse,false);assert.match(rt.game.state.message,/Подойдите ближе/);
  await interact(rt,p,'Зелёная дверь выхода');assert.match(rt.game.state.message,/Заперто/);assert.equal(local(rt,room).escaped,false);
  await interact(rt,p,'Красный электрощит');assert.equal(local(rt,room).power,false);
  await interact(rt,p,'Керамический предохранитель');assert.equal(local(rt,room).fuse,true);assert.equal(rt.snapshot.world.visible[fuse.id],false);
  await interact(rt,p,'Красный электрощит');assert.equal(local(rt,room).power,true);
  const save=JSON.parse(JSON.stringify(gameSave(rt)));await restoreGameSave(rt,save);assert.equal(local(rt,room).fuse,true);assert.equal(local(rt,room).power,true);assert.equal(rt.snapshot.world.visible[fuse.id],false);
  const mirror=byName(p,'Тусклое зеркало · осмотр'),journal=byName(p,'Журнал смен · осмотр');await interact(rt,p,'Тусклое зеркало');assert.equal(local(rt,mirror).visits,1);assert.equal(local(rt,journal).visits,0);
  await interact(rt,p,'Магнитофон последней смены');assert.equal(local(rt,room).record,true);assert.equal(local(rt,lock).IsLocked,false);
  const exit=p.objects.find(o=>o.name==='Зелёная дверь выхода'),pos=rt.game.position(exit.id);rt.snapshot.world.positions[p.gameplay.controller.ownerId]=[0,0,pos[2]+1];rt.playerClick(pos,exit.id);await drive(rt,1.3);await settle(rt);assert.equal(local(rt,room).escaped,true);assert.equal(rt.snapshot.phase,'ENDING');assert.match(rt.snapshot.world.doors[exit.id],/Открыть/);
 }finally{rt.stop();}
});
test('horror sources remain editable through A → B → C and standalone uses the edited rule without A or B',async()=>{
 const a=await load('horror'),original=a.logicResources.find(r=>r.name==='HauntedRoom').source,pkg=await exportAuthoringPackage(a,a,{kind:'template',name:'Обход A',author:'Test',license:'MIT'}),b=await instantiateAuthoringTemplate(pkg,undefined,{reviewedSources:true}),r=b.logicResources.find(r=>r.name==='HauntedRoom');
 editComponentSource(b,r.id,r.source.replace('!state.power || (params.requireRecord && !state.record)','!state.power'));publishComponentSource(b,r.id);
 const variant=await exportAuthoringPackage(b,b,{kind:'template',name:'Обход B',author:'Test',license:'MIT'}),c=await instantiateAuthoringTemplate(variant,undefined,{reviewedSources:true});assert.equal(a.logicResources.find(r=>r.name==='HauntedRoom').source,original);
 const html=await preparePlayableGame(c,{readRuntime:async()=>({js:await readFile('public/player-runtime.js','utf8'),css:await readFile('public/player-runtime.css','utf8')})}),built=JSON.parse(html.split('globalThis.SACURA_PROJECT=')[1].split(';</script>')[0]);
 assert.ok(built.logicResources.find(r=>r.name==='HauntedRoom').source.includes('if (!state.power)'));const rt=runtime();try{await rt.start(built,built.subscenes[0].entry);await rt.advance();await settle(rt);await interact(rt,built,'Керамический предохранитель');await interact(rt,built,'Красный электрощит');await interact(rt,built,'Зелёная дверь выхода');assert.equal(local(rt,byName(built,'Правила ночного обхода')).escaped,true);assert.equal(local(rt,byName(built,'Правила ночного обхода')).record,false);}finally{rt.stop();}
});
