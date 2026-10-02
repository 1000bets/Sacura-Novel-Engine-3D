import {PreviewRuntime} from '../src/runtime.js';
import * as THREE from 'three';
import {addCharacterDetails,animatePose} from '../src/sceneEffects.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import {upgradeProject,allBeats,addToBatch,bindingActions,validateStudio,removeEventBinding} from '../src/studioModel.js';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {createSubscene} from '../src/subsceneModel.js';
import {exportLocation,importLocation,readLocation} from '../src/locationFiles.js';
import {objectTransform,isObjectInScene,sceneStagingPoints} from '../src/sceneEditing.js';
import {createCharacter,enabledCharacterAnimations} from '../src/characterModel.js';
import {readProject} from '../src/projectFiles.js';

test('location file preserves authored transforms, environment, cameras and staging points with independent IDs',()=>{
 const p=upgradeProject(),source=p.subscenes[0];
 const obj=p.objects.find(o=>isObjectInScene(o,source));
 obj.transforms||={};obj.transforms[source.id]={position:[-1.25,0,3],rotation:[0,42,0],scale:[1,2,1]};
 source.description='Custom set';source.navMesh={enabled:true,center:[1,0,2],size:[12,15]};
 source.cameras[0].mode='follow';source.cameras[0].followTargetId=obj.id;
 const file=JSON.parse(JSON.stringify(exportLocation(p,source.id))),original=structuredClone(p);
 const copy=importLocation(p,file);
 assert.notEqual(copy.id,source.id);assert.equal(copy.description,source.description);assert.deepEqual(copy.navMesh,source.navMesh);
 const copied=p.objects.find(o=>o.subsceneId===copy.id&&o.name===obj.name);
 assert.notEqual(copied.id,obj.id);assert.deepEqual(objectTransform(copied,copy.id,copy.kind),objectTransform(obj,source.id,source.kind));
 assert.notEqual(copy.cameras[0].id,source.cameras[0].id);assert.equal(copy.cameras[0].followTargetId,copied.id);
 assert.equal(copy.defaultCameraId,copy.cameras[0].id);
 assert.equal(sceneStagingPoints(copy,p.objects).length,sceneStagingPoints(source,p.objects).length);
 assert.deepEqual(p.subscenes.find(s=>s.id===source.id),original.subscenes[0]);
 assert.deepEqual(readProject(JSON.stringify(p)),p);
});

test('location import rejects damaged transforms and does not mutate the destination project',()=>{
 const p=upgradeProject(),file=exportLocation(p,p.subscenes[0].id),before=structuredClone(p);
 file.objects[0].transforms[file.scene.id].position=[0,null,0];
 assert.throws(()=>importLocation(p,file),/объекты/);assert.deepEqual(p,before);
 assert.throws(()=>readLocation({format:'other'}),/формат|файл/);
});

test('empty authored locations can be saved, loaded repeatedly and retain an empty set',()=>{
 const p=createEmptyProject(),source=createSubscene(p,{name:'Studio',kind:'empty',weather:'Ясно',time:'Ночь',characters:[],firstText:'',placement:'separate'});
 const file=exportLocation(p,source.id),copy=importLocation(p,JSON.stringify(file)),second=importLocation(p,file);
 assert.equal(p.objects.filter(o=>isObjectInScene(o,copy)).length,0);
 assert.notEqual(copy.entry,second.entry);assert.equal(copy.time,'Ночь');assert.equal(allBeats(p).length,4);
});

test('standard pool is idempotent and placements adapt to the character in their destination scene',()=>{
 const p=createEmptyProject(),source=p.subscenes[0],character=createCharacter(p,source.id,'Hero');
 const event=p.events.find(e=>e.name==='Подойти и сесть');
 const b=addToBatch(p,source.entry,'BEFORE',null,event.id),actions=bindingActions(p,b);
 assert.equal(actions.length,2);assert(actions.every(a=>a.target===character.id));
 assert.equal(actions[0].animationId,'ходит');assert.equal(actions[1].value,'сидит');
 assert(enabledCharacterAnimations(character).some(c=>c.id==='сидит'));
 assert.deepEqual(validateStudio(p),[]);
 assert.equal(upgradeProject(upgradeProject(p)).events.length,p.events.length);
 b.actionOverrides[actions[0].id]={...b.actionOverrides[actions[0].id],value:[-4,0,7],animationId:'танец'};
 const loaded=readProject(JSON.stringify(p));assert.deepEqual(bindingActions(loaded,allBeats(loaded)[0].bindings[0])[0].value,[-4,0,7]);
 assert.equal(event.groups[0].actions[0].animationId,'ходит');
});

test('removing one placement cleans its batch but preserves reusable templates and other placements',()=>{
 const p=upgradeProject(),beat=allBeats(p)[0],event=p.events.find(e=>e.standardPreset);
 const first=addToBatch(p,beat.id,'BEFORE',null,event.id),second=addToBatch(p,beat.id,'BEFORE',null,event.id);
 removeEventBinding(p,beat.id,first.id);
 assert(beat.bindings.some(b=>b.id===second.id));assert(p.events.some(e=>e.id===event.id));
 assert(!Object.values(beat.batches).flatMap(g=>g.flatMap(b=>b.bindingIds)).includes(first.id));
 const local={...structuredClone(event),id:'local-event'};p.events.push(local);second.eventId=local.id;second.localEventId=local.id;
 removeEventBinding(p,beat.id,second.id);assert(!p.events.some(e=>e.id===local.id));
});

test('approach-and-sit actually moves with the chosen clip before applying the sitting pose',async()=>{
 const p=createEmptyProject(),scene=p.subscenes[0],character=createCharacter(p,scene.id,'Hero');
 const event=p.events.find(e=>e.name==='Подойти и сесть'),binding=addToBatch(p,scene.entry,'BEFORE',null,event.id);
 const move=event.groups[0].actions[0];binding.actionOverrides[move.id]={target:character.id,value:[-2,0,3],animationId:'танец',duration:.1};
 const rt=new PreviewRuntime({stopAll(){}}),clips=[];rt.delay=async(_,token)=>{clips.push(rt.snapshot.world.motions?.[character.id]?.animationId);rt.assert(token);};
 await rt.start(p,scene.entry);
 assert(clips.includes('танец'));assert.deepEqual(rt.snapshot.world.positions[character.id],[-2,0,3]);assert.equal(rt.snapshot.world.poses[character.id],'сидит');rt.stop();
});

test('built-in sitting bends legs and restores the standing pose without accumulating offsets',()=>{
 const mesh=new THREE.Group(),material=new THREE.MeshStandardMaterial();mesh.add(new THREE.Mesh(new THREE.BoxGeometry(),material),new THREE.Mesh(new THREE.SphereGeometry(),material));
 mesh.children[1].position.y=1;addCharacterDetails(mesh,material);
 animatePose(mesh,'сидит',0,false,false);const height=mesh.userData.parts.head.position.y;
 animatePose(mesh,'сидит',1,false,false);assert.equal(mesh.userData.parts.head.position.y,height);assert.equal(mesh.userData.parts.leftLeg.rotation.x,-Math.PI/2);
 animatePose(mesh,'стоит',2,false,false);assert.equal(mesh.userData.parts.head.position.y,1);assert.equal(mesh.userData.parts.leftLeg.rotation.x,0);
 mesh.traverse(o=>{o.geometry?.dispose();o.material?.dispose();});
});
