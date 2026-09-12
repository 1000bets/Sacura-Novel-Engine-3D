import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {OrbitControls} from 'three/examples/jsm/controls/OrbitControls.js';
import {upgradeProject,newEvent,allBeats,addToBatch,validateStudio} from '../src/studioModel.js';
import {cameraPose,cameraFromView,resolveCamera,newCamera} from '../src/cameraModel.js';
import {setObjectTransform} from '../src/sceneEditing.js';
import {createCameraRig} from '../src/CameraRig.js';
import {PreviewRuntime} from '../src/runtime.js';

const state={location:'living',camera:'Общий план',positions:{},visible:{}};
test('camera migration is idempotent and preserves an intentionally empty camera list',()=>{
 const p=upgradeProject();assert.equal(p.subscenes.length,3);assert.ok(p.subscenes.every(s=>s.cameras.length===1&&s.defaultCameraId===s.cameras[0].id));assert.deepEqual(upgradeProject(JSON.parse(JSON.stringify(p))),p);
 p.subscenes[0].cameras=[];p.subscenes[0].defaultCameraId=null;assert.deepEqual(upgradeProject(p).subscenes[0].cameras,[]);
});
test('follow tracks interpolated character motion and keeps a constant relative frame',()=>{
 const p=upgradeProject(),o=p.objects.find(o=>o.id==='alice'),camera={...p.subscenes[0].cameras[0],mode:'follow',followTargetId:'alice',yaw:0,distance:3,height:1.7,targetHeight:1.1};
 setObjectTransform(p,o.id,'living',{position:[2,0,2],rotation:[0,0,0],scale:[1,1.5,1]});
 const first=cameraPose(camera,state,p.objects,'living'),moving={...state,motions:{alice:{from:[2,0,2],to:'окно',progress:.5}}},next=cameraPose(camera,moving,p.objects,'living');
 assert.deepEqual(first.position,[2,1.7,5]);assert.ok(Math.abs(first.target[1]-1.65)<1e-8);assert.ok(next.position[0]<first.position[0]);assert.equal(next.position[2]-next.target[2],3);
 assert.deepEqual(cameraPose(camera,{...state,visible:{alice:false}},p.objects,'living').position,camera.position);
});
test('current view converts to a following offset without changing its framing',()=>{
 const p=upgradeProject(),c={...p.subscenes[0].cameras[0],mode:'follow',followTargetId:'alice'},view={position:[-3.95,2,2.6],target:[-1.95,1.2,-.4],fov:42};
 const saved=cameraFromView(c,view,state,p.objects,'living'),pose=cameraPose(saved,state,p.objects,'living');pose.position.forEach((v,i)=>assert.ok(Math.abs(v-view.position[i])<1e-8));assert.deepEqual(pose.target,view.target);assert.equal(pose.fov,42);
});
test('main follow camera survives General shot, yields to inspection, then resumes',()=>{
 const p=upgradeProject(),s=p.subscenes[0],c=s.cameras[0];Object.assign(c,{mode:'follow',followTargetId:'alice'});
 assert.equal(resolveCamera(s,state,p.objects).id,c.id);assert.equal(resolveCamera(s,{...state,interactionTarget:'letter'},p.objects).temporary,true);assert.equal(resolveCamera(s,state,p.objects).id,c.id);
 const second=newCamera({position:[0,2,3],target:[0,1,0]},'Другая');s.cameras.push(second);assert.equal(resolveCamera(s,{...state,cameraId:second.id},p.objects).id,second.id);assert.equal(resolveCamera(s,{...state,interactionTarget:'letter'},p.objects,second.id).id,second.id);
});
test('deleted or foreign cameras are diagnosed and camera switches still share one resource',()=>{
 const p=upgradeProject(),b=allBeats(p).find(b=>b.id==='a2');b.bindings=[];b.batches={};const e=newEvent('camera','camera','Общий план','Сменить камеру');e.groups[0].actions[0].cameraId=p.subscenes[1].defaultCameraId;p.events.push(e);addToBatch(p,b.id,'ON_START',null,e.id);
 assert.ok(validateStudio(p).some(i=>i.id.startsWith('camera-missing-')&&i.beatId===b.id));
 e.groups[0].actions[0].cameraId=p.subscenes[0].defaultCameraId;const other=structuredClone(e);other.id='second-camera-event';other.groups[0].actions[0].id='second-camera-action';p.events.push(other);addToBatch(p,b.id,'ON_START',b.batches.ON_START[0].id,other.id);b.batches.ON_START[0].mode='PARALLEL';assert.ok(validateStudio(p).some(i=>i.id.startsWith('overlap-')&&i.beatId===b.id));
});
test('scripted camera action chooses a placed camera and preset action clears that choice',async()=>{
 const p=upgradeProject(),e=newEvent('camera','camera','Общий план','Выбрать камеру');e.groups[0].actions[0].cameraId=p.subscenes[0].defaultCameraId;p.events.push(e);const rt=new PreviewRuntime({stopAll(){}});
 await rt.previewEvent(p,e.id,'a2');assert.equal(rt.snapshot.world.cameraId,e.groups[0].actions[0].cameraId);
 delete e.groups[0].actions[0].cameraId;e.groups[0].actions[0].value='Крупный план';await rt.previewEvent(p,e.id,'a2');assert.equal(rt.snapshot.world.cameraId,null);assert.equal(rt.snapshot.world.camera,'Крупный план');rt.stop();
});
test('camera controller preserves free view and FOV across pilot, preview and pause',()=>{
 const p=upgradeProject(),s=p.subscenes[0],scene=new THREE.Scene(),view=new THREE.PerspectiveCamera(58,1,.05,100),canvas=new EventTarget();canvas.style={};
 view.position.set(2,2,2);const orbit={target:new THREE.Vector3(0,1,0),enabled:true,update(){}};
 const live={sceneId:s.id,kind:s.kind,cameraScene:s,state,objects:p.objects,mode:'scene',editing:true,editTool:'select'};const rig=createCameraRig(scene,view,orbit,canvas,()=>live);rig.update(.016);const free=rig.capture();
 s.cameras[0].fov=35;live.cameraPilotId=s.cameras[0].id;rig.update(.016);assert.equal(view.fov,35);view.position.x=8;rig.update(.016);assert.equal(view.position.x,8);
 live.cameraPilotId=null;rig.update(.016);assert.deepEqual(rig.capture(),free);
 live.mode='game';rig.update(.016);assert.equal(view.fov,35);const frozen=rig.capture();live.state={...state,paused:true};s.cameras[0].position=[9,9,9];rig.update(1);assert.deepEqual(rig.capture(),frozen);
 const other=newCamera({position:[3,2,1],target:[0,1,0],fov:44},'Другой кадр');s.cameras.push(other);live.cameraPreviewId=other.id;rig.update(.016);assert.equal(view.fov,44);assert.deepEqual(view.position.toArray(),other.position);
 live.mode='scene';rig.update(.016);assert.deepEqual(rig.capture(),free);rig.dispose();assert.equal(scene.children.length,0);
});

test('piloting preserves an upward-looking camera and restores free limits',()=>{
 const canvas=new EventTarget();canvas.style={};canvas.getRootNode=()=>canvas;
 const scene=new THREE.Scene(),view=new THREE.PerspectiveCamera(58,1,.05,100);view.position.set(0,1.65,3);
 const orbit=new OrbitControls(view,canvas);orbit.target.set(0,1,0);orbit.maxPolarAngle=Math.PI*.47;orbit.enableDamping=true;
 const camera=newCamera({position:[0,1,3],target:[0,2,0]},'Вверх'),definition={id:'custom',kind:'living',cameras:[camera],defaultCameraId:camera.id};
 const live={sceneId:definition.id,kind:definition.kind,cameraScene:definition,state:{location:definition.id},objects:[],mode:'scene',editing:true,editTool:'select'};
 const rig=createCameraRig(scene,view,orbit,canvas,()=>live);
 try{
  rig.update(.016);const free=rig.capture();live.cameraPilotId=camera.id;rig.update(.016);
  assert.ok(view.position.distanceTo(new THREE.Vector3(...camera.position))<1e-9);assert.equal(orbit.maxPolarAngle,Math.PI);
  live.cameraPilotId=null;rig.update(.016);assert.ok(view.position.distanceTo(new THREE.Vector3(...free.position))<1e-9);assert.equal(orbit.maxPolarAngle,Math.PI*.47);
 }finally{rig.dispose();orbit.dispose();}
});
