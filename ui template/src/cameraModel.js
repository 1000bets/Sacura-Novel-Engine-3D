import {uid} from './model.js';
import {resolvedPosition,objectTransform} from './sceneEditing.js';
import {gameCamera} from './sceneEffects.js';
import {dialogueCamera,isDefaultCamera} from './dialogueCamera.js';

const finite=(v,fallback)=>Number.isFinite(Number(v))?Number(v):fallback;
const vector=(v,fallback)=>fallback.map((n,i)=>finite(v?.[i],n));
export function cleanCamera(c){return {...c,position:vector(c.position,[0,1.65,3]),target:vector(c.target,[0,1,0]),fov:Math.max(20,Math.min(90,finite(c.fov,58))),mode:c.mode==='follow'?'follow':'fixed',distance:Math.max(.6,Math.min(20,finite(c.distance,3))),height:finite(c.height,1.65),yaw:finite(c.yaw,0),targetHeight:finite(c.targetHeight,1.08),smoothing:Math.max(0,Math.min(2,finite(c.smoothing,.2)))};}
export function newCamera(view,name='Новая камера'){return cleanCamera({id:uid('camera'),name,...view,mode:'fixed'});}
export function ensureCameras(p){for(const s of p.subscenes){if(!Array.isArray(s.cameras)){const [position,target]=gameCamera(s.kind,{camera:'Общий план'});const camera=newCamera({position,target},'Основная камера');s.cameras=[camera];s.defaultCameraId=camera.id;}}return p;}
export function cameraPose(camera,state,objects,kind){
 const c=cleanCamera(camera),result={...c};
 if(c.mode==='follow'){
  const o=objects.find(o=>o.id===c.followTargetId&&o.type==='Персонаж'&&o.active!==false&&(!o.subsceneId||o.subsceneId===state.location)&&state.visible?.[o.id]!==false);
  if(!o)return {...result,targetMissing:true};
  const pos=resolvedPosition(o,state,kind),scale=objectTransform(o,state.location,kind).scale[1],angle=c.yaw*Math.PI/180;
  result.target=[pos[0],pos[1]+c.targetHeight*scale,pos[2]];
  result.position=[pos[0]+Math.sin(angle)*c.distance,pos[1]+c.height,pos[2]+Math.cos(angle)*c.distance];
 }
 return result;
}
export function cameraFromView(camera,view,state,objects,kind){
 const c=cleanCamera({...camera,...view});
 if(c.mode==='follow'){
  const o=objects.find(o=>o.id===c.followTargetId);
  if(o){const p=resolvedPosition(o,state,kind),dx=c.position[0]-p[0],dz=c.position[2]-p[2],scale=objectTransform(o,state.location,kind).scale[1];c.distance=Math.max(.6,Math.hypot(dx,dz));c.yaw=Math.atan2(dx,dz)*180/Math.PI;c.height=c.position[1]-p[1];c.targetHeight=(c.target[1]-p[1])/scale;}
 }
 return cleanCamera(c);
}
export function resolveCamera(scene,state,objects,previewId,elapsed=0){
 const cameras=scene.cameras||[],explicit=cameras.find(c=>c.id===(previewId||state.cameraId));
 if(!previewId&&state.interactionTarget){const [position,target]=gameCamera(scene.kind,state,objects);return {position,target,fov:58,smoothing:.14,name:'Осмотр предмета',temporary:true};}
 if(explicit)return cameraPose(explicit,state,objects,scene.kind);
 const main=cameras.find(c=>c.id===scene.defaultCameraId)||cameras[0];
 if(!previewId&&(!state.camera||['Общий план','Крупный план'].includes(state.camera))&&isDefaultCamera(main,scene.kind)){
  const directed=dialogueCamera(scene,state,objects,elapsed);if(directed)return directed;
 }
 if(!state.camera||state.camera==='Общий план'){
  if(main)return cameraPose(main,state,objects,scene.kind);
 }
 const [position,target]=gameCamera(scene.kind,state,objects);return {position,target,fov:58,smoothing:.14,name:state.camera||'Общий план'};
}
