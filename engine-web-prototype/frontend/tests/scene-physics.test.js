import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {navMeshSettings,collisionSettings,findFlatPath,segmentBlocked,samplePath,objectCollisionBox,createScenePhysics} from '../src/scenePhysics.js';
import {resolvedPosition} from '../src/sceneEditing.js';

const box=(min,max)=>new THREE.Box3(new THREE.Vector3(...min),new THREE.Vector3(...max));
const nav={enabled:true,center:[0,0,0],size:[8,8],cellSize:.25};

test('flat navigation routes around obstacles and every segment stays clear',()=>{
 const obstacle=box([-.8,-1,-1],[.8,2,1]),path=findFlatPath([-3,0,0],[3,0,0],nav,[obstacle],[.3,.3]);
 assert.ok(path.length>2);assert.deepEqual(path[0],[-3,0,0]);assert.deepEqual(path.at(-1),[3,0,0]);
 for(let i=1;i<path.length;i++)assert.equal(segmentBlocked(path[i-1],path[i],[obstacle]),false);
 for(const p of path){assert.equal(p[1],0);assert.ok(Math.abs(p[0])<=3.7&&Math.abs(p[2])<=3.7);}
});
test('unreachable, obstructed and out-of-bounds targets report an error',()=>{
 assert.throws(()=>findFlatPath([-3,0,0],[3,0,0],nav,[box([-.2,-1,-5],[.2,2,5])]),/не найден/);
 assert.throws(()=>findFlatPath([0,0,0],[5,0,0],nav),/пределами/);
 assert.throws(()=>findFlatPath([-3,0,0],[0,0,0],nav,[box([-1,-1,-1],[1,2,1])]),/внутри/);
});
test('swept collision prevents tunneling and permits floor contact',()=>{
 assert.equal(segmentBlocked([-10,0,0],[10,0,0],[box([-.01,-1,-1],[.01,1,1])]),true);
 assert.equal(segmentBlocked([-2,0,0],[2,0,0],[box([-3,-1,-3],[3,0,3])]),false);
});
test('path progress uses distance and the runtime position is preserved',()=>{
 const path=[[0,0,0],[0,0,1],[3,0,1]];
 assert.deepEqual(samplePath(path,.5),[1,0,1]);
 assert.deepEqual(resolvedPosition({id:'a',type:'Персонаж'},{location:'s',motions:{a:{path,progress:.5}}}),[1,0,1]);
 assert.deepEqual(resolvedPosition({id:'a'},{location:'s',motions:{a:{path,progress:1,position:[.5,0,1]}}}),[.5,0,1]);
});
test('auto bounds include transforms but exclude hidden models and labels',()=>{
 const root=new THREE.Group(),mesh=new THREE.Mesh(new THREE.BoxGeometry(1,2,3));root.add(mesh);
 const hidden=new THREE.Mesh(new THREE.BoxGeometry(100,100,100));hidden.visible=false;root.add(hidden);root.add(new THREE.Sprite());
 root.position.set(2,0,1);root.scale.set(2,1,1);root.rotation.y=Math.PI/2;
 const bounds=objectCollisionBox(root,{}),size=bounds.getSize(new THREE.Vector3());
 assert.ok(size.distanceTo(new THREE.Vector3(3,2,2))<1e-6);
 const manual=objectCollisionBox(root,{collision:{custom:true,size:[1,1,1],offset:[0,1,0]}});
 assert.ok(manual.getCenter(new THREE.Vector3()).distanceTo(new THREE.Vector3(2,1,1))<1e-6);
});
test('per-object disable, hidden and inactive blockers, nav overlay cleanup',()=>{
 const scene=new THREE.Scene(),actor=new THREE.Mesh(new THREE.BoxGeometry(.5,1,.5)),wall=new THREE.Mesh(new THREE.BoxGeometry(1,2,2));
 actor.position.set(-3,.5,0);actor.userData.id='a';wall.position.y=1;wall.userData.id='wall';scene.add(actor,wall);
 const a={id:'a',type:'Персонаж',active:true,collision:{enabled:true}},w={id:'wall',active:true,collision:{enabled:true}};
 const live={sceneId:'s',mode:'scene',objects:[a,w],cameraScene:{navMesh:{...nav,show:true,showColliders:true}}};
 const physics=createScenePhysics(scene,()=>live,()=>[actor,wall]);
 assert.ok(physics.planMotion(a,[-3,.5,0],[3,.5,0]).length>2);
 assert.throws(()=>physics.validateStep(a,[-3,0,0],[3,0,0]),/коллизия/);
 w.collision.enabled=false;assert.equal(physics.planMotion(a,[-3,0,0],[3,0,0]).length,2);
 w.collision.enabled=true;wall.visible=false;assert.equal(physics.planMotion(a,[-3,0,0],[3,0,0]).length,2);
 wall.visible=true;w.active=false;assert.equal(physics.planMotion(a,[-3,0,0],[3,0,0]).length,2);
 w.active=true;a.collision.enabled=false;assert.equal(physics.planMotion(a,[-3,0,0],[3,0,0]).length,2);
 physics.update();assert.equal(scene.getObjectByName('Navigation debug').children.length,2);
 live.mode='game';physics.update();assert.equal(scene.getObjectByName('Navigation debug').children[0].visible,false);
 physics.dispose();assert.equal(scene.getObjectByName('Navigation debug'),undefined);
});
test('legacy scenes opt in and invalid dimensions are normalized',()=>{
 assert.equal(navMeshSettings().enabled,false);assert.equal(collisionSettings().enabled,false);
 assert.deepEqual(navMeshSettings({navMesh:{size:[-1,Infinity]}}).size,[.5,20]);
});

import {PreviewRuntime} from '../src/runtime.js';
import {upgradeProject} from '../src/studioModel.js';
import {cloneSubscene} from '../src/subsceneModel.js';
import {pathSection} from '../src/scenePhysics.js';

function runtimeFixture(){
 const project=upgradeProject(),location=project.subscenes.find(s=>s.kind==='living'),actor=project.objects.find(o=>o.id==='alice');
 location.navMesh={...nav};actor.collision={enabled:true};actor.transforms={[location.id]:{position:[-3,0,0],rotation:[0,0,0],scale:[1,1,1]}};
 const wall={id:'wall-test',name:'Стена',active:true,type:'Меш',subsceneId:location.id,collision:{enabled:true}};project.objects.push(wall);
 const action=project.events.find(e=>e.id==='visual-walk').groups[0].actions[0];action.value=[3,0,0];action.duration=.1;
 const actorMesh=new THREE.Group(),body=new THREE.Mesh(new THREE.BoxGeometry(.5,1,.5));body.position.y=.5;actorMesh.add(body);actorMesh.position.x=-3;actorMesh.userData.id='alice';
 const wallMesh=new THREE.Mesh(new THREE.BoxGeometry(1,2,2));wallMesh.position.y=1;wallMesh.userData.id='wall-test';
 const scene=new THREE.Scene();scene.add(actorMesh,wallMesh);
 const rt=new PreviewRuntime({stopAll(){},setSidechain(){}});
 const physics=createScenePhysics(scene,()=>({sceneId:location.id,cameraScene:rt.project?.subscenes.find(s=>s.id===location.id)||location,objects:rt.project?.objects||project.objects}),()=>[actorMesh,wallMesh]);
 rt.physicsApi={current:physics};return {project,rt,physics,wallMesh,location};
}
test('runtime follows a multi-corner route, completes at actual position and can start another move',async()=>{
 const {project,rt,physics}=runtimeFixture();
 await rt.previewEvent(project,'visual-walk','a1');
 assert.equal(rt.snapshot.error,null);
 const motion=rt.snapshot.world.motions.alice;assert.ok(motion.path.length>2);assert.deepEqual(motion.position,[3,0,0]);assert.deepEqual(rt.snapshot.world.positions.alice,[3,0,0]);
 project.events.find(e=>e.id==='visual-return').groups[0].actions[0].value=[-3,0,0];project.events.find(e=>e.id==='visual-return').groups[0].actions[0].duration=.1;
 await rt.previewEvent(project,'visual-return','a1');assert.equal(rt.snapshot.error,null);assert.deepEqual(rt.snapshot.world.motions.alice.from,[3,0,0]);
 rt.stop();physics.dispose();
});
test('runtime reports dynamic collision and preserves last safe position',async()=>{
 const {project,rt,physics}=runtimeFixture();let count=0;
 rt.physicsApi.current={...physics,validateStep(){if(++count===2)throw new Error('Движение остановлено: на пути появилась коллизия.');}};
 await rt.previewEvent(project,'visual-walk','a1');assert.match(rt.snapshot.error,/коллизия/);const motion=rt.snapshot.world.motions.alice;
 assert.equal(motion.stopped,true);assert.ok(motion.progress<1);assert.notDeepEqual(motion.position,[3,0,0]);
 assert.deepEqual(resolvedPosition(rt.project.objects.find(o=>o.id==='alice'),rt.snapshot.world),motion.position);
 rt.stop();physics.dispose();
});
test('tick sections retain route corners instead of cutting through obstacles',()=>{
 assert.deepEqual(pathSection([[0,0,0],[0,0,1],[3,0,1]],0,1),[[0,0,0],[0,0,1],[3,0,1]]);
});
test('nav mesh and object colliders survive JSON save and scene duplication',()=>{
 const {project,rt,physics,location}=runtimeFixture(),copy=cloneSubscene(project,location.id),saved=upgradeProject(JSON.parse(JSON.stringify(project)));
 assert.deepEqual(saved.subscenes.find(s=>s.id===copy.id).navMesh,location.navMesh);
 assert.equal(saved.objects.find(o=>o.name==='Стена'&&o.subsceneId===copy.id).collision.enabled,true);
 assert.equal(saved.objects.find(o=>o.id==='alice').collision.enabled,true);
 rt.stop();physics.dispose();
});
