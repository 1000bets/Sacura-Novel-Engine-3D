import test from 'node:test';
import assert from 'node:assert/strict';
import {upgradeProject,allBeats,newEvent,addToBatch,bindingActions,validateStudio} from '../src/studioModel.js';
import {createSubscene,cloneSubscene,newSceneDraft,beatsInScene,sceneTransitions,setSceneEntry,connectSubscene,changeSceneLocation} from '../src/subsceneModel.js';
import {isObjectInScene,objectTransform,setObjectTransform,sceneStagingPoints,stagingPointOptions,resolvedPosition} from '../src/sceneEditing.js';
import {DECORATION_CATALOG} from '../src/sceneDecorations.js';
import {PreviewRuntime} from '../src/runtime.js';

test('all location templates create independent props, a camera, a first line and the chosen cast',()=>{
 const p=upgradeProject(),before=structuredClone(p.chapters),created=[];
 for(const kind of ['living','garden','station']){
  const s=createSubscene(p,{...newSceneDraft(p),name:'Новая '+kind,kind,characters:['alice']});created.push(s);
  assert.equal(beatsInScene(p,s.id).length,1);assert.equal(beatsInScene(p,s.id)[0].id,s.entry);
  assert.equal(s.cameras.length,1);assert.equal(s.defaultCameraId,s.cameras[0].id);
  const visible=p.objects.filter(o=>isObjectInScene(o,s));assert.ok(visible.some(o=>o.id==='alice'));assert.ok(!visible.some(o=>o.id==='bob'));
  const props=visible.filter(o=>o.type!=='Персонаж');assert.equal(props.length,(kind==='living'?5:2)+DECORATION_CATALOG.filter(o=>o.kind===kind).length);
  assert.ok(props.every(o=>o.subsceneId===s.id&&o.transforms[s.id]));
  assert.ok(!validateStudio(p).some(i=>i.beatId===s.entry&&i.level==='error'));
 }
 assert.deepEqual(p.chapters.slice(0,before.length),before);
 assert.equal(new Set(p.objects.map(o=>o.id)).size,p.objects.length);
 assert.deepEqual(upgradeProject(JSON.parse(JSON.stringify(p))),p);
 assert.equal(new Set(created.map(s=>s.defaultCameraId)).size,3);
});

test('insert after one answer preserves conditions, the other branch, and the previous continuation',()=>{
 const p=upgradeProject(),from=allBeats(p).find(b=>b.id==='garden-choice'),answer=from.choices[1],oldNext=answer.next,other=structuredClone(from.choices[0]);
 const s=createSubscene(p,{...newSceneDraft(p),name:'Встреча',kind:'station',placement:'after',choiceId:answer.id},from.id);
 assert.equal(answer.next,s.entry);assert.equal(answer.condition,'trust');assert.equal(answer.threshold,2);
 assert.deepEqual(from.choices[0],other);assert.equal(beatsInScene(p,s.id)[0].next,oldNext);
 assert.equal(sceneTransitions(p,s.id).length,2);
 const snapshot=structuredClone(p);assert.throws(()=>createSubscene(p,{...newSceneDraft(p),name:'Ошибка',placement:'after',choiceId:'missing'},from.id));assert.deepEqual(p,snapshot);
});

test('entry edits optionally reroute only external links to the old entry',()=>{
 const p=upgradeProject(),scene=p.subscenes.find(s=>s.id==='garden'),old=scene.entry;
 const internal=allBeats(p).find(b=>b.id==='garden-letter');internal.next=old;
 const arrival=allBeats(p).find(b=>b.id==='station-voice');arrival.next=old;
 const deep=allBeats(p).find(b=>b.id==='station-final');deep.next='garden-reaction';
 setSceneEntry(p,scene.id,'garden-walk',false);assert.equal(arrival.next,old);
 setSceneEntry(p,scene.id,old);setSceneEntry(p,scene.id,'garden-walk',true);
 assert.equal(arrival.next,'garden-walk');assert.equal(internal.next,old);assert.equal(deep.next,'garden-reaction');
 assert.throws(()=>setSceneEntry(p,scene.id,'station-entry'),/принадлежать/);
});

test('connecting an answer edits the real narrative edge without changing its condition',()=>{
 const p=upgradeProject(),b=allBeats(p).find(b=>b.id==='garden-choice'),answer=b.choices[1];
 connectSubscene(p,b.id,answer.id,'station');assert.equal(answer.next,'station-entry');assert.equal(answer.condition,'trust');
 assert.ok(sceneTransitions(p,'station').some(e=>e.from===b.id&&e.choiceId===answer.id));
 assert.throws(()=>connectSubscene(p,'station-end',null,'living'));
});

test('copy remaps local story, gates, camera actions and batch IDs but keeps external exits and shared event definitions',()=>{
 const p=upgradeProject(),source=p.subscenes.find(s=>s.id==='garden'),gate=allBeats(p).find(b=>b.id==='garden-gate');
 const camera=newEvent('camera','camera','Общий план','Авторский план');camera.groups[0].actions[0].cameraId=source.defaultCameraId;
 const highlight=newEvent('highlight','letter','Подсветить','Заметить записку');p.events.push(camera,highlight);
 addToBatch(p,gate.id,'BEFORE',null,camera.id);const binding=addToBatch(p,gate.id,'ON_START',null,highlight.id);binding.overrides={target:'garden-note'};
 setObjectTransform(p,'garden-note','garden',{position:[1,.9,.2],scale:[1,1,1],rotation:[0,15,0]});
 const oldChapters=structuredClone(p.chapters),oldEvents=structuredClone(p.events),copy=cloneSubscene(p,source.id),lines=beatsInScene(p,copy.id),oldLines=beatsInScene(p,source.id);
 const clonedGate=lines.find(b=>b.text===gate.text),note=p.objects.find(o=>o.id===clonedGate.signal);
 assert.notEqual(clonedGate.id,gate.id);assert.notEqual(note.id,'garden-note');assert.equal(note.subsceneId,copy.id);
 assert.deepEqual(objectTransform(note,copy.id,copy.kind).position,[1,.9,.2]);
 assert.equal(bindingActions(p,clonedGate.bindings[0])[0].cameraId,copy.defaultCameraId);
 assert.equal(bindingActions(p,clonedGate.bindings[1])[0].target,note.id);
 const ids=new Set(lines.map(b=>b.id));for(let i=0;i<oldLines.length;i++){
  if(oldLines.some(b=>b.id===oldLines[i].next))assert.ok(ids.has(lines[i].next));
  else assert.equal(lines[i].next,oldLines[i].next);
 }
 const originalChoice=oldLines.find(b=>b.id==='garden-choice'),newChoice=lines.find(b=>b.text===originalChoice.text);
 assert.notEqual(newChoice.choices[0].id,originalChoice.choices[0].id);assert.ok(ids.has(newChoice.choices[0].next));assert.equal(newChoice.choices[2].next,'station-entry');
 assert.notEqual(clonedGate.batches.BEFORE[0].id,gate.batches.BEFORE[0].id);assert.deepEqual(clonedGate.batches.BEFORE[0].bindingIds,[clonedGate.bindings[0].id]);
 assert.deepEqual(p.chapters.slice(0,oldChapters.length),oldChapters);assert.deepEqual(p.events,oldEvents);
 assert.ok(!validateStudio(p).some(i=>i.beatId===clonedGate.id&&i.level==='error'));
});

test('copy of a global prop is visible only as its local instance; shared actors retain their placement',()=>{
 const p=upgradeProject();setObjectTransform(p,'alice','living',{position:[1,0,2],rotation:[0,25,0],scale:[1,1,1]});
 const s=cloneSubscene(p,'living'),visible=p.objects.filter(o=>isObjectInScene(o,s)),letter=visible.find(o=>o.builtin==='letter');
 assert.ok(letter);assert.notEqual(letter.id,'letter');assert.ok(!visible.some(o=>o.id==='letter'));
 assert.deepEqual(objectTransform(p.objects.find(o=>o.id==='alice'),s.id,s.kind).position,[1,0,2]);
 assert.equal(beatsInScene(p,s.id).find(b=>b.kind==='gate'&&b.text===allBeats(p).find(b=>b.id==='s2').text).signal,letter.id);
});

test('changing location preserves authored props and diagnoses inaccessible gate and action targets',()=>{
 const p=upgradeProject(),s=p.subscenes[0],before=structuredClone(p.chapters);s.cameras[0].mode='follow';s.cameras[0].followTargetId='alice';
 changeSceneLocation(p,s.id,'garden');assert.deepEqual(p.chapters,before);
 assert.ok(p.objects.filter(o=>isObjectInScene(o,s)).some(o=>o.builtin==='garden-note'));assert.ok(!isObjectInScene(p.objects.find(o=>o.id==='letter'),s));
 assert.ok(validateStudio(p).some(i=>i.id==='gate-location-s2'&&i.level==='error'));
 s.excludedObjectIds||=[];s.excludedObjectIds.push('alice');assert.ok(validateStudio(p).some(i=>i.id===`camera-target-${s.cameras[0].id}`));
 assert.ok(validateStudio(p).some(i=>i.id.startsWith('action-location-')&&i.level==='error'));
 changeSceneLocation(p,s.id,'living');assert.equal(p.objects.filter(o=>isObjectInScene(o,s)&&(o.builtin||o.id)==='letter').length,1);
});

test('a copied letter satisfies its gate and scene transition restores local visibility',async()=>{
 const p=upgradeProject(),s=cloneSubscene(p,'living'),b=beatsInScene(p,s.id).find(b=>b.kind==='gate'&&p.objects.find(o=>o.id===b.signal)?.builtin==='letter');
 b.bindings=[];b.batches={};const target=allBeats(p).find(b=>b.id==='station-voice');target.bindings=[];target.batches={};b.next=target.id;
 const rt=new PreviewRuntime({stopAll(){},stop(){}});await rt.start(p,b.id);assert.equal(rt.snapshot.phase,'WAITING_OBJECT');
 rt.snapshot.world.visible.alice=false;await rt.advance(null,b.signal);
 assert.equal(rt.snapshot.variables.letter,true);assert.equal(rt.snapshot.world.location,'station');assert.deepEqual(rt.snapshot.world.visible,{});rt.stop();
});

test('staging points use each location landmarks, follow their own props and survive reload',()=>{
 const p=upgradeProject(),living=p.subscenes.find(s=>s.id==='living'),garden=p.subscenes.find(s=>s.id==='garden');
 const points=sceneStagingPoints(garden,p.objects);
 assert.ok(points.some(point=>point.label==='У скамьи'));assert.ok(!points.some(point=>/камин|окн|диван/.test(point.label)));
 assert.equal(new Set(p.subscenes.flatMap(scene=>scene.stagingPoints.map(point=>point.id))).size,15);
 const benchPoint=points.find(point=>point.key==='bench'),beforeLiving=sceneStagingPoints(living,p.objects),bench=p.objects.find(o=>o.id===benchPoint.objectId),transform=objectTransform(bench,garden.id,garden.kind);
 transform.position[0]+=3;setObjectTransform(p,bench.id,garden.id,transform);
 const moved=sceneStagingPoints(garden,p.objects).find(point=>point.id===benchPoint.id);
 assert.deepEqual(moved.position,[benchPoint.position[0]+3,...benchPoint.position.slice(1)]);assert.deepEqual(sceneStagingPoints(living,p.objects),beforeLiving);
 const reloaded=upgradeProject(JSON.parse(JSON.stringify(p)));assert.deepEqual(sceneStagingPoints(reloaded.subscenes.find(s=>s.id===garden.id),reloaded.objects),sceneStagingPoints(garden,p.objects));
});

test('copied staging points and movement bindings target the copied local prop',()=>{
 const p=upgradeProject(),source=p.subscenes.find(s=>s.id==='garden'),point=source.stagingPoints.find(point=>point.key==='bench'),event=newEvent('move','alice',point.id,'К скамье');
 p.events.push(event);addToBatch(p,source.entry,'BEFORE',null,event.id);
 const copy=cloneSubscene(p,source.id),copyPoint=copy.stagingPoints.find(point=>point.key==='bench'),copyBeat=beatsInScene(p,copy.id).find(b=>b.id===copy.entry);
 assert.notEqual(copyPoint.id,point.id);assert.notEqual(copyPoint.objectId,point.objectId);assert.equal(p.objects.find(o=>o.id===copyPoint.objectId).subsceneId,copy.id);
 assert.equal(bindingActions(p,copyBeat.bindings.find(b=>b.eventId===event.id))[0].value,copyPoint.id);
 copyPoint.position[0]+=7;assert.notEqual(copyPoint.position[0],point.position[0]);assert.equal(event.groups[0].actions[0].value,point.id);
 const fresh=createSubscene(p,{...newSceneDraft(p),name:'Другой сад',kind:'garden'});
 assert.ok(fresh.stagingPoints.every(point=>point.id.startsWith(fresh.id+':')));assert.ok(fresh.stagingPoints.every(point=>!point.objectId||p.objects.find(o=>o.id===point.objectId).subsceneId===fresh.id));
 changeSceneLocation(p,fresh.id,'station');assert.ok(fresh.stagingPoints.some(point=>point.label==='У вагона'));assert.ok(!fresh.stagingPoints.some(point=>point.label==='У скамьи'));
});

test('version one projects gain editable decorations and points without overwriting placement',()=>{
 const p=upgradeProject(),catalogIds=new Set(DECORATION_CATALOG.map(o=>o.id));p.sceneEditingVersion=1;
 p.objects=p.objects.filter(o=>!catalogIds.has(o.builtin||o.id));p.subscenes.forEach(scene=>delete scene.stagingPoints);
 setObjectTransform(p,'room-table','living',{position:[2,.3,-1],rotation:[0,25,0],scale:[1.2,1,1]});const oldEvents=structuredClone(p.events),oldTransform=objectTransform(p.objects.find(o=>o.id==='room-table'),'living');
 const migrated=upgradeProject(p);assert.equal(migrated.sceneEditingVersion,2);assert.deepEqual(migrated.events,oldEvents);
 assert.deepEqual(objectTransform(migrated.objects.find(o=>o.id==='room-table'),'living'),oldTransform);
 assert.deepEqual(sceneStagingPoints(migrated.subscenes.find(s=>s.id==='living'),migrated.objects).find(point=>point.key==='table').position,[1.35,.3,-1.7000000000000002]);
 for(const scene of migrated.subscenes){assert.equal(scene.stagingPoints.length,5);for(const decoration of DECORATION_CATALOG.filter(o=>o.kind===scene.kind))assert.ok(migrated.objects.some(o=>isObjectInScene(o,scene)&&(o.builtin||o.id)===decoration.id));}
 assert.deepEqual(upgradeProject(migrated),migrated);
});

test('runtime resolves authored staging point IDs and rejects points from another subscene',async()=>{
 const p=upgradeProject(),scene=p.subscenes.find(s=>s.id==='garden'),point=scene.stagingPoints.find(point=>point.key==='bench'),entry=allBeats(p).find(b=>b.id===scene.entry),event=newEvent('move','alice',point.id,'Движение к точке');
 entry.bindings=[];entry.batches={};event.groups[0].actions[0].duration=.1;p.events.push(event);addToBatch(p,entry.id,'BEFORE',null,event.id);
 setObjectTransform(p,point.objectId,scene.id,{position:[4,0,2],rotation:[0,0,0],scale:[1,1,1]});
 const rt=new PreviewRuntime({stopAll(){}});await rt.start(p,entry.id);
 assert.equal(rt.snapshot.error,null);assert.equal(rt.snapshot.world.positions.alice,point.id);
 assert.deepEqual(resolvedPosition(p.objects.find(o=>o.id==='alice'),rt.snapshot.world,scene.kind),sceneStagingPoints(scene,p.objects).find(p=>p.id===point.id).position);rt.stop();
 event.groups[0].actions[0].value=p.subscenes[0].stagingPoints[0].id;await rt.start(p,entry.id);assert.match(rt.snapshot.error,/Точка постановки отсутствует/);rt.stop();
 event.groups[0].actions[0].value=point.id;scene.stagingPoints=scene.stagingPoints.filter(p=>p.id!==point.id);await rt.start(p,entry.id);assert.match(rt.snapshot.error,/Точка постановки отсутствует/);assert.equal(rt.snapshot.world.positions.alice,undefined);rt.stop();
});

test('movement selectors offer only the current scene points and identify unavailable saved destinations',()=>{
 const p=upgradeProject(),garden=p.subscenes.find(s=>s.id==='garden'),living=p.subscenes.find(s=>s.id==='living');
 const options=stagingPointOptions(garden,p.objects,'окно');
 assert.equal(options[0][0],'окно');assert.equal(options[0][1],'У старого дерева');
 assert.ok(options.slice(1).every(([id])=>garden.stagingPoints.some(point=>point.id===id)));
 assert.ok(!options.some(([id])=>living.stagingPoints.some(point=>point.id===id)));
 const unavailable=stagingPointOptions(garden,p.objects,living.stagingPoints[0].id);assert.match(unavailable[0][1],/недоступна/);
 const coordinates=stagingPointOptions(garden,p.objects,[2,0,1]);assert.deepEqual(coordinates[0],['2, 0, 1','Координаты: 2, 0, 1']);
});
