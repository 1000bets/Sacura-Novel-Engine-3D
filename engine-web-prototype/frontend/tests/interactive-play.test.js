import {objectTransform} from '../src/sceneEditing.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import {createStandardProject,createEmptyProject} from '../src/projectLifecycle.js';
import {allBeats,validateStudio,upgradeProject,normalizeBatches,makeAction} from '../src/studioModel.js';
import {readProject} from '../src/projectFiles.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {audioEnvelope} from '../src/audioEnvelope.js';
import {interactionTargets} from '../src/interactionModel.js';
import {createObjectGroup,addObjectsToGroup,removeObjectsFromGroups,sceneObjectGroups} from '../src/objectGroups.js';
import {gameSceneHit} from '../src/GameControls.js';

class Audio extends EventTarget{duration=10;currentTime=0;volume=1;play(){return Promise.resolve();}pause(){}}
test('visible interaction label selects its item above foreground geometry, respecting hidden ancestors',()=>{
 const marker={visible:true},item={visible:true,userData:{id:'letter',marker}},wall={visible:true,userData:{id:'wall'}};marker.parent=item;
 const hits=[{object:wall},{object:marker}],state={interactionTargets:['letter']};
 assert.equal(gameSceneHit(hits,state,'game').object,marker);assert.equal(gameSceneHit(hits,state,'scene').object,wall);
 item.visible=false;assert.equal(gameSceneHit(hits,state,'game').object,wall);item.visible=true;marker.visible=false;assert.equal(gameSceneHit(hits,state,'game').object,wall);
});
const desk=()=>new SoundDesk(()=>new Audio());
const example=()=>{const p=createStandardProject(),b=allBeats(p).find(b=>b.id==='rain-explore');b.bindings=[];b.batches={};normalizeBatches(b);b.controls.radius=0;return {p,b};};

test('template exploration survives save/load with both targets, controls and interaction events',()=>{
 const {p,b}=example();assert.equal(allBeats(p).find(b=>b.id==='a5').next,b.id);assert.equal(b.next,'a6');assert.deepEqual(interactionTargets(b),['letter','rain-keepsake']);assert.equal(b.controls.characterId,'alice');assert.equal(b.controls.mode,'both');assert.deepEqual(validateStudio(p),[]);assert.deepEqual(readProject(JSON.stringify(p)),p);assert.deepEqual(upgradeProject(p),p);
});
for(const order of [['letter','rain-keepsake'],['rain-keepsake','letter']])test('story waits for both objects in order '+order.join(', '),async()=>{
 const {p,b}=example(),rt=new PreviewRuntime(desk());await rt.start(p,b.id);assert.equal(rt.snapshot.phase,'WAITING_OBJECT');
 await rt.advance();await rt.advance(null,'door');assert.deepEqual(rt.snapshot.interacted,[]);
 await rt.advance(null,order[0]);assert.equal(rt.snapshot.beatId,b.id);assert.equal(rt.snapshot.ready,true);
 const runCount=Object.keys(rt.snapshot.instances).length;await rt.advance(null,order[0]);assert.equal(Object.keys(rt.snapshot.instances).length,runCount);
 await rt.advance(null,order[1]);assert.equal(rt.snapshot.beatId,'a6');assert.equal(rt.snapshot.variables.letter,true);assert.equal(rt.snapshot.variables.keepsakeFound,true);assert.equal(rt.snapshot.world.playerControl,null);rt.stop();
});
test('clicking twice while an interaction event executes cannot double-run it or continue early',async()=>{
 const {p,b}=example();const e=p.events.find(e=>e.id===b.interactionEvents['rain-keepsake']);e.groups.unshift({id:'wait-step',actions:[{...makeAction('wait'),duration:.05}]});const rt=new PreviewRuntime(desk());await rt.start(p,b.id);
 const pending=rt.advance(null,'rain-keepsake');await rt.advance(null,'rain-keepsake');await rt.advance(null,'letter');assert.equal(Object.values(rt.snapshot.instances).filter(i=>i.eventId===e.id).length,1);await pending;assert.deepEqual(rt.snapshot.interacted,['rain-keepsake']);rt.stop();
});
test('movement respects pause, bounds and obstacles; click walks and interacts only after arrival',async()=>{
 const {p,b}=example();b.controls.radius=1.5;const rt=new PreviewRuntime(desk());rt.physicsApi={current:{sceneId:()=> 'living',planMotion:(o,from,to)=>[from,to],validateStep(){}}};await rt.start(p,b.id);
 const original=objectTransform(p.objects.find(o=>o.id==='alice'),'living','living').position;rt.playerStep([1,0,0],.05);assert.ok(rt.snapshot.world.positions.alice[0]>original[0]);const moved=[...rt.snapshot.world.positions.alice];
 rt.togglePause();rt.playerStep([1,0,0],.05);assert.deepEqual(rt.snapshot.world.positions.alice,moved);rt.togglePause();
 rt.physicsApi.current.validateStep=()=>{throw new Error('Obstacle');};rt.playerStep([1,0,0],.05);assert.deepEqual(rt.snapshot.world.positions.alice,moved);assert.equal(rt.snapshot.hint,'Obstacle');rt.physicsApi.current.validateStep=()=>{};
 rt.snapshot.world.positions.alice=[4.09,0,0];rt.playerStep([1,0,0],.05);assert.deepEqual(rt.snapshot.world.positions.alice,[4.09,0,0]);
 rt.snapshot.world.positions.alice=[-2,0,0];await rt.advance(null,'rain-keepsake');assert.deepEqual(rt.snapshot.interacted,[]);
 rt.playerClick(null,'rain-keepsake');assert.ok(rt.playerDestination);assert.deepEqual(rt.snapshot.interacted,[]);
 for(let i=0;i<100&&rt.playerDestination;i++)rt.playerStep([0,0,0],.05);
 await new Promise(r=>setTimeout(r,70));assert.deepEqual(rt.snapshot.interacted,['rain-keepsake']);assert.equal(rt.snapshot.beatId,b.id);
 rt.stop();const stopped=[...rt.snapshot.world.positions.alice];rt.playerStep([1,0,0],.05);assert.deepEqual(rt.snapshot.world.positions.alice,stopped);
});
test('invalid player, second target outside scene and missing item event are diagnosed',()=>{
 const {p,b}=example();b.controls.characterId='letter';p.objects.find(o=>o.id==='rain-keepsake').subsceneId='garden';b.interactionEvents.letter='gone';const errors=validateStudio(p).filter(i=>i.beatId===b.id&&i.level==='error');assert.ok(errors.some(i=>i.id.startsWith('gate-player')));assert.ok(errors.some(i=>i.id.startsWith('gate-location')));assert.ok(errors.some(i=>i.id.startsWith('gate-event')));
});
test('audio envelope follows playback position through seek, pause, and resume',async()=>{
 const d=desk();d.play('voice-alice',{key:'v',fadeIn:2,fadeOut:4,volume:.8});await d.get('v').started;const audio=d.get('v').audio;
 assert.equal(audio.volume,0);d.seek('v',1);d.applyVolumes();assert.equal(audio.volume,.4);d.seek('v',8);d.applyVolumes();assert.equal(audio.volume,.4);
 d.pause('v');d.resume('v');await new Promise(r=>setTimeout(r,5));assert.equal(audio.volume,.4);
 d.seek('v',5);d.applyVolumes();assert.equal(audio.volume,.8);d.seek('v',0);d.applyVolumes();assert.equal(audio.volume,0);d.stopAll();
 assert.equal(audioEnvelope(0,10,0,0),1);assert.equal(audioEnvelope(10,10,0,2),0);assert.ok(Number.isFinite(audioEnvelope(1,NaN,2,3)));
});
test('empty hierarchy groups persist and objects move to group and back without changing transforms',()=>{
 const p=createEmptyProject(),group=createObjectGroup(p,p.subscenes[0].id,[],'Props',{allowEmpty:true});p.objects.push({id:'one',subsceneId:p.subscenes[0].id,transforms:{[p.subscenes[0].id]:{position:[1,2,3]}}});const before=structuredClone(p.objects);
 addObjectsToGroup(p,group.id,['one']);removeObjectsFromGroups(p,group.sceneId,['one']);assert.deepEqual(group.objectIds,[]);assert.deepEqual(p.objects,before);assert.equal(sceneObjectGroups(JSON.parse(JSON.stringify(p)),group.sceneId,p.objects).length,1);
});

test('game keyboard uses physical WASD, normalizes diagonals, clears on pause and does not take form shortcuts',async()=>{
 const {createGameControls}=await import('../src/GameControls.js'),THREE=await import('three');
 const previousWindow=globalThis.window,windowEvents=new EventTarget();globalThis.window=windowEvents;
 const viewport=new EventTarget(),canvas={closest:()=>viewport},camera=new THREE.PerspectiveCamera();camera.lookAt(0,0,-1);camera.updateMatrixWorld();
 const steps=[],clicks=[];let interactions=0;const live={mode:'game',state:{playerControl:{mode:'both'},paused:false},onPlayerStep:(direction,dt)=>steps.push(direction),onPlayerInteract:()=>interactions++,onPlayerClick:(point,id)=>clicks.push({point,id})};
 const controls=createGameControls(camera,canvas,()=>live),key=(target,type,props)=>{const event=new Event(type,{cancelable:true});Object.assign(event,props);target.dispatchEvent(event);return event;};
 try{
  assert.equal(key(viewport,'keydown',{code:'KeyW',key:'ц'}).defaultPrevented,true);key(viewport,'keydown',{code:'KeyD',key:'в'});controls.tick(.02);assert.ok(Math.abs(Math.hypot(...steps.at(-1))-1)<1e-9);assert.ok(steps.at(-1)[0]>0&&steps.at(-1)[2]<0);
  key(viewport,'keydown',{code:'KeyE',repeat:false});key(viewport,'keydown',{code:'KeyE',repeat:true});assert.equal(interactions,1);
  live.state.paused=true;controls.tick(.02);live.state.paused=false;controls.tick(.02);assert.ok(steps.at(-1).every(value=>value===0));
  key(viewport,'keydown',{code:'KeyW',ctrlKey:true});controls.tick(.02);assert.ok(steps.at(-1).every(value=>value===0));
  controls.click(new THREE.Raycaster(new THREE.Vector3(0,2,2),new THREE.Vector3(0,-1,0)),'letter');assert.deepEqual(clicks[0],{point:[0,0,2],id:'letter'});
  live.mode='scene';assert.equal(controls.click(new THREE.Raycaster(),'letter'),false);
 }finally{controls.dispose();globalThis.window=previousWindow;}
});
