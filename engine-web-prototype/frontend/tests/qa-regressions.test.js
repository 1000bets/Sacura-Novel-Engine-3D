import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {upgradeProject,newEvent,allBeats,addToBatch,validateStudio} from '../src/studioModel.js';
import {cloneSubscene,changeSceneLocation,beatsInScene} from '../src/subsceneModel.js';
import {setObjectTransform,isObjectInScene,objectTransform} from '../src/sceneEditing.js';
import {gameCamera,updateObjectHighlight} from '../src/sceneEffects.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {readProject} from '../src/projectFiles.js';
const tick=(ms=10)=>new Promise(resolve=>setTimeout(resolve,ms));
class Audio extends EventTarget{constructor(){super();this.volume=1;this.duration=10;this.currentTime=0;this.playCount=0;this.pauseCount=0;}play(){this.playCount++;return Promise.resolve();}pause(){this.pauseCount++;}}
const setup=()=>{const p=upgradeProject();for(const id of ['a5','garden-entry']){const b=allBeats(p).find(b=>b.id===id);b.bindings=[];b.batches={};}return p;};
const event=(p,type,target,value)=>{const e=newEvent(type,target,value);p.events.push(e);return e;};
const music=p=>{const e=event(p,'music','audio','Главная тема');e.groups[0].actions[0].fade=0;return e;};

test('loading audio is paused and global resume cannot resurrect an explicitly stopped track',async()=>{
 const p=setup(),e=music(p),desk=new SoundDesk(()=>new Audio()),rt=new PreviewRuntime(desk);
 await rt.previewEvent(p,e.id,'a5');rt.togglePause();rt.controlEffect('music','stop');rt.togglePause();await tick();assert.equal(desk.get('background').status,'stopped');rt.stop();
 let release;const waiting=new SoundDesk(()=>{const a=new Audio();a.play=()=>new Promise(r=>release=r);return a;}),paused=new PreviewRuntime(waiting);paused.running=true;
 waiting.play('music-main',{key:'loading'});await tick();assert.equal(waiting.get('loading').status,'loading');paused.togglePause();release();await tick();assert.equal(waiting.get('loading').status,'paused');paused.stop();
});

test('old fade-stop cannot stop a replacement music track',async()=>{
 const p=setup(),a=music(p),b=music(p),stop=event(p,'stop','audio','Остановить');stop.groups[0].actions[0].duration=.12;
 const desk=new SoundDesk(()=>new Audio()),rt=new PreviewRuntime(desk);await rt.previewEvent(p,a.id,'a5');const old=desk.get('background');
 const fading=rt.previewEvent(p,stop.id,'a5');await tick(20);await rt.previewEvent(p,b.id,'a5');await fading;
 assert.notEqual(desk.get('background'),old);assert.equal(desk.get('background').status,'playing');assert.equal(rt.snapshot.effects.music.eventId,b.id);assert.equal(rt.snapshot.effects.music.status,'held');rt.stop();
});

test('two parallel placements of the same voice keep distinct audio instances',async()=>{
 const p=setup(),voice=event(p,'sound','world','Голос');voice.groups[0].actions[0].assetId='voice-bob';
 addToBatch(p,'a5','ON_START',null,voice.id);const beat=allBeats(p).find(b=>b.id==='a5'),batch=beat.batches.ON_START[0];addToBatch(p,beat.id,'ON_START',batch.id,voice.id);beat.batches.ON_START[0].mode='PARALLEL';
 const desk=new SoundDesk(()=>new Audio()),rt=new PreviewRuntime(desk),start=rt.start(p,beat.id);await tick(20);
 const tracks=[...desk.tracks.values()];assert.equal(tracks.length,2);assert.ok(tracks.every(t=>t.status==='playing'&&t.audio.pauseCount===0));
 tracks[0].audio.dispatchEvent(new Event('ended'));await tick();assert.equal(rt.snapshot.ready,false);tracks[1].audio.dispatchEvent(new Event('ended'));await start;assert.equal(rt.snapshot.ready,true);rt.stop();
});

test('older local music cannot stop newer persistent music at a location boundary',async()=>{
 const p=setup(),a=music(p),b=music(p);b.owner='Scene';const beat=allBeats(p).find(b=>b.id==='a5');beat.next='garden-entry';addToBatch(p,beat.id,'BEFORE',null,a.id);addToBatch(p,beat.id,'BEFORE',null,b.id);
 const desk=new SoundDesk(()=>new Audio()),rt=new PreviewRuntime(desk);await rt.start(p,beat.id);const current=desk.get('background');await rt.advance();
 assert.equal(rt.snapshot.world.location,'garden');assert.equal(desk.get('background'),current);assert.equal(current.status,'playing');assert.equal(rt.snapshot.effects.music.eventId,b.id);rt.stop();
});

test('scene-owned weather and time survive a transition; local defaults resume after stop',async()=>{
 const p=setup(),snow=event(p,'weather','world','Снег'),night=event(p,'time','world','Ночь');snow.owner='Scene';night.owner='GameSession';
 const beat=allBeats(p).find(b=>b.id==='a5');beat.next='garden-entry';for(const e of [snow,night])addToBatch(p,beat.id,'BEFORE',null,e.id);
 const rt=new PreviewRuntime(new SoundDesk(()=>new Audio()));await rt.start(p,beat.id);await rt.advance();assert.equal(rt.snapshot.world.weather,'Снег');assert.equal(rt.snapshot.effects.weather.owner,'Scene');assert.equal(rt.snapshot.world.time,'Ночь');
 rt.controlEffect('weather','stop');await rt.enter('a6',rt.generation);assert.equal(rt.snapshot.world.weather,p.subscenes[0].weather);assert.equal(rt.snapshot.effects.weather.owner,'SubScene');
 rt.stop();
});

test('door and highlight commands address one object and leave other instances intact',async()=>{
 const p=setup();p.objects.push({...structuredClone(p.objects.find(o=>o.id==='door')),id:'door-copy',builtin:'door',subsceneId:'living'});
 const a=event(p,'door','door-copy','Открыть'),b=event(p,'door','door','Открыть'),h1=event(p,'highlight','letter','Подсветить'),h2=event(p,'highlight','door','Подсветить'),rt=new PreviewRuntime(new SoundDesk(()=>new Audio()));
 for(const e of [a,b,h1,h2])await rt.previewEvent(p,e.id,'a5');assert.deepEqual(rt.snapshot.world.doors,{'door-copy':'Открыть',door:'Открыть'});assert.deepEqual(rt.snapshot.world.highlights,{letter:true,door:true});
 rt.controlEffect('door:door-copy','stop');assert.equal(rt.snapshot.world.doors.door,'Открыть');assert.equal(rt.snapshot.world.doors['door-copy'],null);
 rt.controlEffect('door:door','pause');assert.equal(rt.snapshot.world.pausedDoors.door,true);b.groups[0].actions[0].value='Закрыть';await rt.previewEvent(p,b.id,'a5');assert.equal(rt.snapshot.world.pausedDoors.door,false);assert.equal(rt.snapshot.world.doors.door,'Закрыть');
 rt.controlEffect('highlight:letter','stop');assert.equal(rt.snapshot.world.highlights.door,true);assert.equal(rt.snapshot.world.highlights.letter,false);rt.stop();
 const mesh=new THREE.Mesh(new THREE.BoxGeometry(),new THREE.MeshStandardMaterial({emissive:'#123456',emissiveIntensity:.7})),original=mesh.material.emissive.getHex();updateObjectHighlight(mesh,true,1);assert.notEqual(mesh.material.emissive.getHex(),original);updateObjectHighlight(mesh,false,1);assert.equal(mesh.material.emissive.getHex(),original);assert.equal(mesh.material.emissiveIntensity,.7);mesh.geometry.dispose();mesh.material.dispose();
});

test('item camera follows the placed note in a copied location without relying on fixed IDs',()=>{
 const p=setup();setObjectTransform(p,'garden-note','garden',{position:[6,.9,-2],rotation:[0,0,0],scale:[1,1,1]});const s=cloneSubscene(p,'garden'),objects=p.objects.filter(o=>isObjectInScene(o,s));
 assert.deepEqual(gameCamera('garden',{location:s.id,camera:'План предмета'},objects)[1],[6,.9,-2]);
});

test('location roundtrip retains original prop identity, placement and interaction gates',()=>{
 const p=setup(),s=p.subscenes[0];setObjectTransform(p,'letter',s.id,{position:[2,.8,1],rotation:[0,20,0],scale:[1,1,1]});changeSceneLocation(p,s.id,'garden');changeSceneLocation(p,s.id,'living');
 const note=p.objects.find(o=>o.id==='letter');assert.ok(isObjectInScene(note,s));assert.deepEqual(objectTransform(note,s.id,s.kind).position,[2,.8,1]);assert.ok(!validateStudio(p).some(i=>i.id==='gate-location-s2'));
 assert.equal(p.objects.filter(o=>isObjectInScene(o,s)&&(o.builtin||o.id)==='letter').length,1);
});

test('custom variable actions validate and execute without false missing-object failures',async()=>{
 const p=setup();p.variables.questDone=false;const e=event(p,'variable','questDone','true');addToBatch(p,'a5','BEFORE',null,e.id);assert.ok(!validateStudio(p).some(i=>i.beatId==='a5'&&i.level==='error'));
 const rt=new PreviewRuntime(new SoundDesk(()=>new Audio()));await rt.start(p,'a5');assert.equal(rt.snapshot.variables.questDone,true);rt.stop();
});

test('project loading rejects empty, malformed and duplicate story data before replacing live state',()=>{
 const p=setup();assert.deepEqual(readProject(JSON.stringify(p)),upgradeProject(p));
 for(const chapters of [[],[{id:'x',beats:[]}],[{id:'x',beats:null}]])assert.throws(()=>readProject({...p,chapters}),/Проект не загружен/);
 const duplicate=structuredClone(p);duplicate.chapters[0].beats[1].id=duplicate.chapters[0].beats[0].id;assert.throws(()=>readProject(duplicate),/идентификаторы/);
 const broken=structuredClone(p);broken.subscenes[0].entry='missing';assert.throws(()=>readProject(broken),/неверное начало/);
});
