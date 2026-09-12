import * as THREE from 'three';
import {TransformControls} from 'three/examples/jsm/controls/TransformControls.js';
import {objectTransform,cleanTransform} from './sceneEditing.js';
import {captureSelectionTransform,applySelectionTransform} from './sceneSelection.js';

export function meshGeometry(shape){return shape==='sphere'?new THREE.SphereGeometry(.3,24,16):shape==='cylinder'?new THREE.CylinderGeometry(.28,.28,.6,24):new THREE.BoxGeometry(.5,.5,.5);}
export function groupObjects(scene,meshes,id,position){const group=new THREE.Group();group.position.set(...position);group.userData.id=id;group.userData.color={'room-table':'#a58568','room-sofa':'#9e8175','garden-bench':'#a49076','station-train':'#638c8c'}[id];scene.add(group);meshes.forEach(m=>group.attach(m));return group;}
export function cloneSceneObject(source,id){
 const cloneMaterial=material=>Array.isArray(material)?material.map(m=>m.clone()):material?.clone();
 const clonePart=node=>{const part=node.isMesh?new THREE.Mesh(node.geometry.clone(),cloneMaterial(node.material)):node.isSprite?new THREE.Sprite(cloneMaterial(node.material)):node.isLight?node.clone(false):node.isLineSegments?new THREE.LineSegments(node.geometry.clone(),cloneMaterial(node.material)):new THREE.Group();part.position.copy(node.position);part.quaternion.copy(node.quaternion);part.scale.copy(node.scale);part.castShadow=node.castShadow;part.receiveShadow=node.receiveShadow;part.visible=node.visible;for(const child of node.children)part.add(clonePart(child));return part;};
 const copy=clonePart(source);copy.userData.id=id;copy.userData.color=source.userData.color;copy.traverse(child=>{if(child.isSprite){copy.userData.marker=child;child.visible=false;}});return copy;
}
export function createSceneGizmo(scene,camera,canvas,orbit,getLive,getMeshes){
 const control=new TransformControls(camera,canvas),helper=control.getHelper();scene.add(helper);control.setSize(.8);
 const pivot=new THREE.Object3D();scene.add(pivot);
 const outlines=new Map(),pending=new Map();
 let lastFocus,ignoreUntil=0,dragCapture=null;
 const selectionMeshes=()=>{const live=getLive(),ids=live.selectedIds||[live.selected];return getMeshes().filter(mesh=>ids.includes(mesh.userData.id)&&mesh.visible);};
 const snapshot=mesh=>cleanTransform({position:mesh.position.toArray(),rotation:[mesh.rotation.x,mesh.rotation.y,mesh.rotation.z].map(THREE.MathUtils.radToDeg),scale:mesh.scale.toArray()});
 control.addEventListener('dragging-changed',e=>{orbit.enabled=!e.value&&getLive().mode==='scene';if(e.value)canvas.closest('.scene-viewport')?.focus({preventScroll:true});else dragCapture=null;});
 control.addEventListener('mouseDown',()=>{if(control.object===pivot)dragCapture=captureSelectionTransform(selectionMeshes(),pivot);});
 control.addEventListener('objectChange',()=>{if(dragCapture&&control.object===pivot)applySelectionTransform(dragCapture,pivot);});
 control.addEventListener('mouseUp',()=>{
  if(!control.object)return;const live=getLive(),meshes=dragCapture?dragCapture.objects.map(item=>item.mesh):[control.object];
  const changes=meshes.map(mesh=>({id:mesh.userData.id,value:snapshot(mesh)})).filter(change=>change.id);
  for(const change of changes)pending.set(change.id,{...change,sceneId:live.sceneId,previous:JSON.stringify(live.objects.find(o=>o.id===change.id)?.transforms?.[live.sceneId])});
  ignoreUntil=performance.now()+160;
  if(live.onTransforms)live.onTransforms(changes);else for(const change of changes)live.onTransform?.(change.id,change.value);
 });
 return {
  get dragging(){return control.dragging;},
  blocksClick(){return control.dragging||!!control.axis||performance.now()<ignoreUntil;},
  apply(mesh,o,position){
   const live=getLive();if(control.dragging&&(control.object===mesh||dragCapture?.objects.some(item=>item.mesh===mesh)))return;
   let t=objectTransform(o,live.sceneId,live.kind||'living');
   const change=pending.get(o.id);if(change){if(live.mode!=='scene'||change.sceneId!==live.sceneId||JSON.stringify(o.transforms?.[live.sceneId])!==change.previous)pending.delete(o.id);else t=change.value;}
   mesh.position.set(...(live.mode==='game'||live.editing===false?position:t.position));mesh.rotation.set(...t.rotation.map(THREE.MathUtils.degToRad));mesh.scale.set(...t.scale);
   if(mesh.material&&o.color)mesh.material.color.set(o.color);
   if(o.builtin&&mesh.isGroup&&mesh.userData.color!==o.color){if(mesh.userData.color!==undefined)mesh.traverse(child=>{if(child.isMesh&&child.material?.color)child.material.color.set(o.color);});mesh.userData.color=o.color;}
   if(mesh.userData.isCharacter&&o.color)for(const part of [mesh.children[0],mesh.userData.parts?.left,mesh.userData.parts?.right])part?.material?.color.set(o.color);
   if(mesh.userData.primitive&&mesh.userData.primitive!==(o.primitive||'box')){mesh.geometry.dispose();mesh.geometry=meshGeometry(o.primitive);mesh.userData.primitive=o.primitive||'box';}
  },
  update(){
   const live=getLive(),editing=live.mode==='scene'&&live.editing!==false,meshes=selectionMeshes(),mesh=meshes.find(m=>m.userData.id===live.selected)||meshes.at(-1),tool=live.editTool||'translate';
   const target=meshes.length>1?pivot:mesh;
   if(editing&&mesh&&tool!=='select'){
    if(target===pivot&&!control.dragging){pivot.position.set(0,0,0);for(const item of meshes)pivot.position.add(item.getWorldPosition(new THREE.Vector3()));pivot.position.divideScalar(meshes.length);pivot.quaternion.identity();if(live.editSpace==='local')mesh.getWorldQuaternion(pivot.quaternion);pivot.scale.set(1,1,1);pivot.updateMatrixWorld(true);}
    if(control.object!==target)control.attach(target);if(control.mode!==tool)control.setMode(tool);control.setSpace(live.editSpace||'world');control.setTranslationSnap(live.snap?.25:null);control.setRotationSnap(live.snap?Math.PI/12:null);control.setScaleSnap(live.snap?.1:null);
   }else if(control.object)control.detach();
   for(const [item,outline]of outlines)if(!editing||!meshes.includes(item)){scene.remove(outline);outline.geometry.dispose();outline.material.dispose();outlines.delete(item);}
   if(editing)for(const item of meshes){if(!outlines.has(item)){const outline=new THREE.BoxHelper(item,0xc69ab0);scene.add(outline);outlines.set(item,outline);}outlines.get(item).setFromObject(item);}
   if(editing&&live.focusRequest&&live.focusRequest!==lastFocus){
    const request=live.focusRequest,focusMeshes=request.objectId?getMeshes().filter(m=>m.userData.id===request.objectId&&m.visible):Array.isArray(request.position)?[]:meshes;
    if(focusMeshes.length||Array.isArray(request.position)){
     const bounds=focusMeshes.length?focusMeshes.reduce((box,item)=>box.expandByObject(item),new THREE.Box3()):null;
     const center=bounds?bounds.getCenter(new THREE.Vector3()):new THREE.Vector3(...request.position);
     const size=bounds?bounds.getSize(new THREE.Vector3()).length():1.8;
     const direction=camera.position.clone().sub(orbit.target);if(direction.lengthSq()<.001)direction.set(-1,.65,1.2);
     const fieldOfView=Math.min(camera.fov,THREE.MathUtils.radToDeg(2*Math.atan(Math.tan(THREE.MathUtils.degToRad(camera.fov/2))*camera.aspect)));
     const distance=Math.max(1.7,Math.min(140,size*.55/Math.sin(THREE.MathUtils.degToRad(fieldOfView/2))));
     orbit.target.copy(center);camera.position.copy(center).add(direction.normalize().multiplyScalar(distance));camera.lookAt(center);lastFocus=request;
    }
   }
  },
  dispose(){control.detach();control.dispose();scene.remove(helper,pivot);for(const outline of outlines.values()){scene.remove(outline);outline.geometry.dispose();outline.material.dispose();}outlines.clear();},
 };
}
