import * as THREE from 'three';
import {TransformControls} from 'three/examples/jsm/controls/TransformControls.js';
import {cameraPose,cameraFromView,resolveCamera} from './cameraModel.js';

export function createCameraRig(scene,view,orbit,canvas,getLive){
 const markers=new Map(),control=new TransformControls(view,canvas),gizmo=control.getHelper();scene.add(gizmo);control.setSize(.75);
 let lastKey,editorView,ignoreUntil=0,pending,previousPilotData;
 const freeLimits={min:orbit.minPolarAngle,max:orbit.maxPolarAngle};
 const capture=()=>({position:view.position.toArray(),target:orbit.target.toArray(),fov:view.fov});
 const apply=(pose,k=1)=>{view.position.lerp(new THREE.Vector3(...pose.position),k);orbit.target.lerp(new THREE.Vector3(...pose.target),k);view.fov=THREE.MathUtils.lerp(view.fov,pose.fov||58,k);view.updateProjectionMatrix();view.lookAt(orbit.target);};
 const makeMarker=c=>{
  const body=new THREE.Group();body.userData.cameraId=c.id;
  const mat=new THREE.MeshBasicMaterial({color:'#cba1b8',depthTest:false});
  const box=new THREE.Mesh(new THREE.BoxGeometry(.25,.17,.22),mat);body.add(box);
  const lens=new THREE.Mesh(new THREE.ConeGeometry(.11,.14,4),mat);lens.rotation.x=-Math.PI/2;lens.position.z=-.17;body.add(lens);
  body.renderOrder=20;scene.add(body);
  const camera=new THREE.PerspectiveCamera(c.fov,view.aspect,.12,1.25),helper=new THREE.CameraHelper(camera);helper.material.transparent=true;helper.material.opacity=.55;scene.add(helper);
  return {body,camera,helper};
 };
 const remove=m=>{scene.remove(m.body,m.helper);m.body.traverse(o=>{o.geometry?.dispose();o.material?.dispose();});m.helper.dispose();};
 control.addEventListener('dragging-changed',e=>{orbit.enabled=!e.value&&getLive().mode==='scene';});
 control.addEventListener('mouseUp',()=>{
  const live=getLive(),body=control.object,c=live.cameraScene.cameras?.find(c=>c.id===body?.userData.cameraId);if(!c)return;
  const pose=cameraPose(c,live.state,live.objects,live.kind),distance=new THREE.Vector3(...pose.position).distanceTo(new THREE.Vector3(...pose.target));
  const target=c.mode==='follow'?new THREE.Vector3(...pose.target):new THREE.Vector3(0,0,-1).applyQuaternion(body.quaternion).multiplyScalar(Math.max(.2,distance)).add(body.position);
  const updated=cameraFromView(c,{position:body.position.toArray(),target:target.toArray(),fov:c.fov},live.state,live.objects,live.kind);
  pending={id:c.id,value:updated,previous:JSON.stringify(c)};ignoreUntil=performance.now()+180;live.onCameraChange?.(c.id,updated);
 });
 return {
  capture,
  get dragging(){return control.dragging;},
  blocksClick(){return control.dragging||!!control.axis||performance.now()<ignoreUntil||(getLive().mode==='scene'&&!!getLive().cameraPilotId);},
  pick(ray){const live=getLive();if(live.mode!=='scene'||live.cameraPilotId)return null;const hit=ray.intersectObjects([...markers.values()].filter(m=>m.body.visible).map(m=>m.body),true)[0];let o=hit?.object;while(o&&!o.userData.cameraId)o=o.parent;return o?.userData.cameraId||null;},
  update(dt,objectDragging=false){
   const live=getLive(),definition=live.cameraScene||{kind:live.kind,cameras:[]},cameras=definition.cameras||[],pilot=cameras.find(c=>c.id===live.cameraPilotId),key=live.sceneId+'|'+live.mode+'|'+(pilot?.id||'')+'|'+(live.cameraPreviewId||'');
   orbit.minPolarAngle=pilot?0:freeLimits.min;orbit.maxPolarAngle=pilot?Math.PI:freeLimits.max;
   if(key!==lastKey){
    const changedScene=lastKey&&lastKey.split('|')[0]!==live.sceneId;
    if(changedScene)editorView=null;
    else if(lastKey?.split('|')[1]==='scene'&&!lastKey?.split('|')[2])editorView=capture();
    if(changedScene&&live.mode==='scene'&&!pilot)apply(resolveCamera(definition,live.state,live.objects));
    if(live.mode==='scene'&&!pilot&&editorView)apply(editorView);
    if(live.mode==='scene'&&pilot)apply(cameraPose(pilot,live.state,live.objects,live.kind));
    if(live.mode==='game')apply(resolveCamera(definition,live.state,live.objects,live.cameraPreviewId));
    previousPilotData=pilot?JSON.stringify(pilot):null;lastKey=key;
   }
   if(pilot&&live.mode==='scene'&&JSON.stringify(pilot)!==previousPilotData){apply(cameraPose(pilot,live.state,live.objects,live.kind));previousPilotData=JSON.stringify(pilot);}
   if(live.mode==='game'&&!live.state.paused){const pose=resolveCamera(definition,live.state,live.objects,live.cameraPreviewId);apply(pose,pose.smoothing?1-Math.exp(-dt/pose.smoothing):1);}
   orbit.enabled=live.mode==='scene'&&!control.dragging&&!objectDragging;
   if(orbit.enabled)orbit.update();
   for(const [id,m]of markers)if(!cameras.some(c=>c.id===id)){if(control.object===m.body)control.detach();remove(m);markers.delete(id);}
   for(let c of cameras){
    if(!markers.has(c.id))markers.set(c.id,makeMarker(c));const m=markers.get(c.id);
    if(pending?.id===c.id){if(JSON.stringify(c)!==pending.previous)pending=null;else c=pending.value;}
    const pose=cameraPose(c,live.state,live.objects,live.kind);m.body.visible=live.mode==='scene'&&!pilot&&live.showCameras!==false;
    if(!(control.dragging&&control.object===m.body)){m.camera.position.set(...pose.position);m.camera.lookAt(new THREE.Vector3(...pose.target));m.body.position.copy(m.camera.position);m.body.quaternion.copy(m.camera.quaternion);}
    m.camera.position.copy(m.body.position);m.camera.quaternion.copy(m.body.quaternion);m.camera.fov=c.fov;m.camera.aspect=view.aspect;m.camera.updateProjectionMatrix();m.camera.updateMatrixWorld();m.helper.update();m.helper.visible=m.body.visible&&c.id===live.selectedCameraId;
    m.body.children.forEach(x=>x.material.color.set(c.id===live.selectedCameraId?'#ffe1aa':'#c59ab2'));
   }
   const chosen=markers.get(live.selectedCameraId),definitionCamera=cameras.find(c=>c.id===live.selectedCameraId);
   if(live.mode==='scene'&&live.editing!==false&&!pilot&&chosen?.body.visible&&['translate','rotate'].includes(live.editTool)){
    if(control.object!==chosen.body)control.attach(chosen.body);control.setMode(definitionCamera?.mode==='follow'?'translate':live.editTool);control.setSpace('world');control.setTranslationSnap(live.snap?.25:null);control.setRotationSnap(live.snap?Math.PI/12:null);
   }else if(control.object)control.detach();
  },
  dispose(){control.detach();control.dispose();scene.remove(gizmo);for(const m of markers.values())remove(m);markers.clear();},
 };
}
