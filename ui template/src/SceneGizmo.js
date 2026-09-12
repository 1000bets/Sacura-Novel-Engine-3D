import * as THREE from 'three';
import {TransformControls} from 'three/examples/jsm/controls/TransformControls.js';
import {objectTransform,cleanTransform} from './sceneEditing.js';

export function meshGeometry(shape){return shape==='sphere'?new THREE.SphereGeometry(.3,24,16):shape==='cylinder'?new THREE.CylinderGeometry(.28,.28,.6,24):new THREE.BoxGeometry(.5,.5,.5);}
export function groupObjects(scene,meshes,id,position){const group=new THREE.Group();group.position.set(...position);group.userData.id=id;group.userData.color={'room-table':'#a58568','room-sofa':'#9e8175','garden-bench':'#a49076','station-train':'#638c8c'}[id];scene.add(group);meshes.forEach(m=>group.attach(m));return group;}
export function cloneSceneObject(source,id){
 const clonePart=node=>{const part=node.isMesh?new THREE.Mesh(node.geometry.clone(),Array.isArray(node.material)?node.material.map(m=>m.clone()):node.material.clone()):node.isSprite?new THREE.Sprite(node.material.clone()):new THREE.Group();part.position.copy(node.position);part.quaternion.copy(node.quaternion);part.scale.copy(node.scale);part.castShadow=node.castShadow;part.receiveShadow=node.receiveShadow;for(const child of node.children)part.add(clonePart(child));return part;};
 const copy=clonePart(source);copy.userData.id=id;copy.userData.color=source.userData.color;copy.traverse(child=>{if(child.isSprite){copy.userData.marker=child;child.visible=false;}});return copy;
}
export function createSceneGizmo(scene,camera,canvas,orbit,getLive,getMeshes){
 const control=new TransformControls(camera,canvas),helper=control.getHelper();scene.add(helper);control.setSize(.8);
 const outline=new THREE.BoxHelper(undefined,0xc69ab0);outline.visible=false;scene.add(outline);
 let lastFocus,ignoreUntil=0,pending=null;
 const snapshot=mesh=>cleanTransform({position:mesh.position.toArray(),rotation:[mesh.rotation.x,mesh.rotation.y,mesh.rotation.z].map(THREE.MathUtils.radToDeg),scale:mesh.scale.toArray()});
 control.addEventListener('dragging-changed',e=>{orbit.enabled=!e.value&&getLive().mode==='scene';if(e.value)canvas.closest('.scene-viewport')?.focus({preventScroll:true});});
 control.addEventListener('mouseUp',()=>{if(!control.object)return;const live=getLive(),mesh=control.object;pending={id:mesh.userData.id,sceneId:live.sceneId,value:snapshot(mesh),previous:JSON.stringify(live.objects.find(o=>o.id===mesh.userData.id)?.transforms?.[live.sceneId])};ignoreUntil=performance.now()+160;live.onTransform?.(pending.id,pending.value);});
 return {
  get dragging(){return control.dragging;},
  blocksClick(){return control.dragging||!!control.axis||performance.now()<ignoreUntil;},
  apply(mesh,o,position){
   const live=getLive();if(control.dragging&&control.object===mesh)return;
   let t=objectTransform(o,live.sceneId,live.kind||'living');
   if(pending?.id===o.id&&pending.sceneId===live.sceneId){if(live.mode!=='scene'||JSON.stringify(o.transforms?.[live.sceneId])!==pending.previous)pending=null;else t=pending.value;}
   mesh.position.set(...(live.mode==='game'||live.editing===false?position:t.position));mesh.rotation.set(...t.rotation.map(THREE.MathUtils.degToRad));mesh.scale.set(...t.scale);
   if(mesh.material&&o.color)mesh.material.color.set(o.color);
   if(o.builtin&&mesh.isGroup&&mesh.userData.color!==o.color){if(mesh.userData.color!==undefined)mesh.traverse(child=>{if(child.isMesh&&child.material?.color)child.material.color.set(o.color);});mesh.userData.color=o.color;}
   if(mesh.userData.isCharacter&&o.color)for(const part of [mesh.children[0],mesh.userData.parts?.left,mesh.userData.parts?.right])part?.material?.color.set(o.color);
   if(mesh.userData.primitive&&mesh.userData.primitive!==(o.primitive||'box')){mesh.geometry.dispose();mesh.geometry=meshGeometry(o.primitive);mesh.userData.primitive=o.primitive||'box';}
  },
  update(){
   const live=getLive(),editing=live.mode==='scene'&&live.editing!==false,mesh=getMeshes().find(m=>m.userData.id===live.selected&&m.visible),tool=live.editTool||'translate';
   if(editing&&mesh&&tool!=='select'){if(control.object!==mesh)control.attach(mesh);if(control.mode!==tool)control.setMode(tool);control.setSpace(live.editSpace||'world');control.setTranslationSnap(live.snap?.25:null);control.setRotationSnap(live.snap?Math.PI/12:null);control.setScaleSnap(live.snap?.1:null);}else if(control.object)control.detach();
   outline.visible=editing&&!!mesh;if(outline.visible)outline.setFromObject(mesh);
   if(editing&&mesh&&live.focusRequest!==lastFocus&&live.focusRequest){const bounds=new THREE.Box3().setFromObject(mesh),center=bounds.getCenter(new THREE.Vector3()),size=bounds.getSize(new THREE.Vector3()).length();orbit.target.copy(center);camera.position.copy(center).add(new THREE.Vector3(-1,.65,1.2).normalize().multiplyScalar(Math.max(1.7,Math.min(7,size*1.5))));camera.lookAt(center);lastFocus=live.focusRequest;}
  },
  dispose(){control.detach();control.dispose();scene.remove(helper,outline);outline.geometry.dispose();outline.material.dispose();},
 };
}
