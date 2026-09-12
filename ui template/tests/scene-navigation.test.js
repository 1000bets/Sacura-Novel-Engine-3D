import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {createSceneNavigation} from '../src/SceneNavigation.js';
import {cloneSceneObject,groupObjects} from '../src/SceneGizmo.js';
import {DECORATION_CATALOG} from '../src/sceneDecorations.js';
import {upgradeProject} from '../src/studioModel.js';
import {objectTransform,setObjectTransform,isObjectInScene} from '../src/sceneEditing.js';

class Surface extends EventTarget {
  style={};dataset={};ownerDocument={defaultView:new EventTarget()};captured=null;
  closest(){return this;}
  focus(){}
  getBoundingClientRect(){return {width:800,height:600};}
  setPointerCapture(id){this.captured=id;}
  hasPointerCapture(id){return this.captured===id;}
  releasePointerCapture(){this.captured=null;}
}
function send(surface,type,props={}){
  const event=new Event(type,{cancelable:true});
  Object.assign(event,{pointerId:1,button:0,clientX:100,clientY:100,...props});
  surface.dispatchEvent(event);return event;
}
function setup(){
  const camera=new THREE.PerspectiveCamera(58,4/3,.05,400),canvas=new Surface(),live={mode:'scene'};
  camera.position.set(0,2,6);camera.lookAt(0,0,0);
  const nav=createSceneNavigation(camera,canvas,()=>live);return {camera,canvas,live,nav};
}
const near=(a,b)=>assert.ok(a.distanceTo(b)<.000001,`${a.toArray()} differs from ${b.toArray()}`);

test('plain left drag keeps editor view fixed, Alt-left orbits without moving the pivot',()=>{
  const {camera,canvas,nav}=setup(),position=camera.position.clone(),pivot=nav.target.clone();
  send(canvas,'pointerdown');send(canvas,'pointermove',{clientX:220});send(canvas,'pointerup',{clientX:220});near(camera.position,position);
  send(canvas,'pointerdown',{altKey:true});assert.equal(nav.navigating,true);
  send(canvas,'pointermove',{clientX:220,clientY:140,altKey:true});assert.ok(camera.position.distanceTo(position)>.5);near(nav.target,pivot);
  assert.ok(Math.abs(camera.position.distanceTo(pivot)-position.distanceTo(pivot))<.000001);
  send(canvas,'pointerup',{altKey:true});assert.equal(nav.navigating,false);assert.equal(nav.blocksClick(),true);nav.dispose();
});

test('middle-button pan translates the camera and its target by the same displacement',()=>{
  const {camera,canvas,nav}=setup(),offset=camera.position.clone().sub(nav.target);
  send(canvas,'pointerdown',{button:1});send(canvas,'pointermove',{button:1,clientX:180,clientY:130});
  assert.ok(nav.target.length()>.1);near(camera.position.clone().sub(nav.target),offset);
  send(canvas,'pointerup',{button:1});nav.dispose();
});

test('right-button look supports WASD flight, Q/E height, Shift boost and releases keys on blur',()=>{
  const {camera,canvas,nav}=setup(),win=canvas.ownerDocument.defaultView;
  send(canvas,'pointerdown',{button:2});send(canvas,'pointermove',{button:2,clientX:170,clientY:90});
  assert.equal(canvas.dataset.navigating,'true');const start=camera.position.clone();
  send(win,'keydown',{code:'KeyW'});nav.tick(.1);const normalDistance=camera.position.distanceTo(start);assert.ok(Math.abs(normalDistance-.3)<.00001);
  send(win,'keydown',{code:'ShiftLeft'});const fastStart=camera.position.clone();nav.tick(.1);assert.ok(Math.abs(camera.position.distanceTo(fastStart)-normalDistance*3)<.00001);
  send(win,'keyup',{code:'KeyW'});send(win,'keyup',{code:'ShiftLeft'});send(win,'keydown',{code:'KeyE'});const height=camera.position.y;nav.tick(.1);assert.ok(Math.abs(camera.position.y-height-.3)<.00001);
  send(win,'blur');const stopped=camera.position.clone();nav.tick(.1);near(camera.position,stopped);assert.equal(canvas.dataset.navigating,undefined);nav.dispose();
});

test('wheel zoom respects bounds and gameplay does not consume editor navigation gestures',()=>{
  const {camera,canvas,nav,live}=setup(),before=camera.position.length();
  send(canvas,'wheel',{deltaY:-100,deltaMode:0});assert.ok(camera.position.length()<before);
  for(let i=0;i<40;i++)send(canvas,'wheel',{deltaY:500,deltaMode:0});assert.ok(camera.position.length()<=150.000001);
  live.mode='game';const position=camera.position.clone();assert.equal(send(canvas,'wheel',{deltaY:-100,deltaMode:0}).defaultPrevented,false);
  send(canvas,'pointerdown',{button:2});send(canvas,'pointermove',{button:2,clientX:250});near(camera.position,position);assert.equal(nav.navigating,false);nav.dispose();
});

test('built-in decoration identities and transforms survive saving and remain scoped to the location',()=>{
  const project=upgradeProject();
  for(const item of DECORATION_CATALOG){
    const scene=project.subscenes.find(s=>s.kind===item.kind),object=project.objects.find(o=>(o.builtin||o.id)===item.id&&isObjectInScene(o,scene));
    assert.ok(object,`${item.id} is present in authoring`);assert.deepEqual(objectTransform(object,scene.id,scene.kind).position,item.position);
    setObjectTransform(project,object.id,scene.id,{position:[3,4,5],rotation:[10,20,30],scale:[2,2,2]});
  }
  const loaded=upgradeProject(JSON.parse(JSON.stringify(project)));
  for(const item of DECORATION_CATALOG){const scene=loaded.subscenes.find(s=>s.kind===item.kind),object=loaded.objects.find(o=>(o.builtin||o.id)===item.id);assert.deepEqual(objectTransform(object,scene.id,scene.kind).position,[3,4,5]);}
});

test('moving and duplicating compound props keeps child geometry and attached light with the prop',()=>{
  const scene=new THREE.Scene(),mesh=new THREE.Mesh(new THREE.BoxGeometry(),new THREE.MeshStandardMaterial()),light=new THREE.PointLight(0xffaa55,7);
  mesh.position.set(2,1,3);light.position.set(2,2,3);scene.add(mesh,light);
  const prop=groupObjects(scene,[mesh,light],'lamp',[2,0,3]);prop.position.x+=4;
  assert.deepEqual(mesh.getWorldPosition(new THREE.Vector3()).toArray(),[6,1,3]);assert.deepEqual(light.getWorldPosition(new THREE.Vector3()).toArray(),[6,2,3]);
  const copy=cloneSceneObject(prop,'lamp-copy');assert.equal(copy.children[1].isPointLight,true);assert.equal(copy.children[1].intensity,7);
  assert.notEqual(copy.children[0].material,mesh.material);assert.notEqual(copy.children[0].geometry,mesh.geometry);
});
