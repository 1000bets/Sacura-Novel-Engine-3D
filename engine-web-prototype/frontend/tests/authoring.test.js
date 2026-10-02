import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {upgradeProject,allBeats,addToBatch} from '../src/studioModel.js';
import {blankAsset,copyGroup,eventFromAsset,authoringProblems} from '../src/authoringModel.js';
import {objectTransform,setObjectTransform,resolvedPosition} from '../src/sceneEditing.js';
import {cloneSceneObject,groupObjects} from '../src/SceneGizmo.js';
import {gameCamera} from '../src/sceneEffects.js';
import {PreviewRuntime} from '../src/runtime.js';

test('library migration keeps existing IDs, overrides and intentionally empty catalogs',()=>{
 const p=upgradeProject(),before=p.events.map(e=>[e.id,...e.groups.flatMap(g=>[g.id,...g.actions.map(a=>a.id)])]);
 p.actionTemplates=[];p.groupTemplates=[];p.objects=p.objects.filter(o=>o.id!=='room-table');
 const migrated=upgradeProject(JSON.parse(JSON.stringify(p)));
 assert.deepEqual(migrated.events.map(e=>[e.id,...e.groups.flatMap(g=>[g.id,...g.actions.map(a=>a.id)])]),before);
 assert.deepEqual(migrated.actionTemplates,[]);assert.deepEqual(migrated.groupTemplates,[]);assert.ok(!migrated.objects.some(o=>o.id==='room-table'));
});
test('action to group to event creates isolated instances and keeps audio parameters',()=>{
 const action={...blankAsset('action','sound'),name:'Голос',assetId:'voice-alice',duck:true,duckDb:-16,volume:.7};
 const group={...blankAsset('group'),name:'Вместе',actions:[action]},copy=copyGroup(group),event=eventFromAsset('group',group);
 assert.notEqual(copy.id,group.id);assert.notEqual(copy.actions[0].id,action.id);assert.notEqual(event.groups[0].actions[0].id,copy.actions[0].id);
 assert.equal(event.groups[0].actions[0].duckDb,-16);action.volume=.1;assert.equal(event.groups[0].actions[0].volume,.7);
 const p=upgradeProject();p.events.push(event);addToBatch(p,'a2','AFTER',null,event.id);assert.ok(allBeats(p).find(b=>b.id==='a2').bindings.some(b=>b.eventId===event.id&&b.hook==='AFTER'));
});
test('authoring blocks empty steps, conflicting moves and incomplete replacement settings',()=>{
 const e=blankAsset('event');e.name='Реакция';assert.ok(authoringProblems('event',e).length);
 const a={...blankAsset('action'),name:'Идти'};e.groups=[{id:'group',actions:[a,{...a,id:'second'}]}];assert.match(authoringProblems('event',e).join(' '),/два действия/);
 e.groups[0].actions.pop();e.retention='HOLD_UNTIL_REPLACED';assert.match(authoringProblems('event',e).join(' '),/роль/);e.channel='alice.move';assert.deepEqual(authoringProblems('event',e),[]);
});
test('transforms survive serialization, are isolated per scene, and named move preserves prop height',()=>{
 const p=upgradeProject(),alice=p.objects.find(o=>o.id==='alice'),original=objectTransform(alice,'living');
 setObjectTransform(p,'alice','garden',{position:[3,0,2],rotation:[0,60,0],scale:[1.2,1.2,1.2]});
 assert.deepEqual(objectTransform(alice,'living'),original);
 const loaded=JSON.parse(JSON.stringify(p)),a=loaded.objects.find(o=>o.id==='alice');assert.deepEqual(objectTransform(a,'garden','garden').position,[3,0,2]);
 const letter=p.objects.find(o=>o.id==='letter');setObjectTransform(p,letter.id,'living',{position:[2,1.1,1],rotation:[0,0,0],scale:[1,1,1]});
 assert.equal(resolvedPosition(letter,{location:'living',positions:{letter:'окно'}},'living')[1],1.1);
});
test('grouping furniture preserves world placement and duplication owns its materials',()=>{
 const scene=new THREE.Scene(),mesh=new THREE.Mesh(new THREE.BoxGeometry(2,.1,1),new THREE.MeshStandardMaterial({color:'#a58568'}));mesh.position.set(-.25,.7,1.7);scene.add(mesh);const before=mesh.getWorldPosition(new THREE.Vector3());
 const root=groupObjects(scene,[mesh],'table',[-.25,0,1.7]);assert.deepEqual(mesh.getWorldPosition(new THREE.Vector3()).toArray(),before.toArray());
 const copy=cloneSceneObject(root,'copy');assert.notEqual(copy.children[0].material,mesh.material);copy.children[0].material.color.set('#ffffff');assert.notEqual(copy.children[0].material.color.getHex(),mesh.material.color.getHex());
});
test('inspection camera follows a note moved in a different subscene with the same location kind',()=>{
 const note={id:'garden-note',type:'Активный меш',transforms:{'garden-two':{position:[4,1,-2],rotation:[0,0,0],scale:[1,1,1]}}};
 const [position,target]=gameCamera('garden',{location:'garden-two',interactionTarget:'garden-note'},[note]);assert.deepEqual(target,[4,1,-2]);assert.ok(new THREE.Vector3(...position).distanceTo(new THREE.Vector3(...target))<3);
});
test('preview starts movement at authored position and never writes placement back to project',async()=>{
 const p=upgradeProject();setObjectTransform(p,'alice','living',{position:[2,.2,2],rotation:[0,30,0],scale:[1,1,1]});
 const event=p.events.find(e=>e.id==='visual-walk');event.groups[0].actions[0].duration=.3;const frozen=JSON.stringify(p);const rt=new PreviewRuntime({stopAll(){}});
 const work=rt.previewEvent(p,event.id,'a1');await new Promise(r=>setTimeout(r,20));assert.deepEqual(rt.snapshot.world.motions.alice.from,[2,.2,2]);rt.stop();await work;
 assert.deepEqual(objectTransform(p.objects.find(o=>o.id==='alice'),'living').position,[2,.2,2]);assert.equal(JSON.stringify(p),frozen);
});
