import {isObjectInScene,objectTransform,resolvedPosition} from './sceneEditing.js';
import {gameCamera} from './sceneEffects.js';

const sameVector=(a,b)=>a?.length===b.length&&a.every((v,i)=>Math.abs(v-b[i])<.0001);

// A camera placed or adjusted by the author keeps its saved composition.
export function isDefaultCamera(camera,kind){
 if(!camera)return true;
 const [position,target]=gameCamera(kind,{camera:'Общий план'});
 return camera.mode!=='follow'&&Math.abs((camera.fov??58)-58)<.0001&&sameVector(camera.position,position)&&sameVector(camera.target,target);
}

export function dialogueCamera(scene,state,objects,elapsed=0){
 const dialogue=state.dialogue;
 if(!dialogue||dialogue.kind==='gate')return null;
 const index=dialogue.index||0;
 const characters=objects.filter(o=>o.type==='Персонаж'&&o.active!==false&&state.visible?.[o.id]!==false&&isObjectInScene(o,scene));
 const speaker=characters.find(o=>o.id===dialogue.speakerId);
 const cue=state.cameraCueKey===dialogue.key?state.camera:null;
 // One slow, bounded move per line; waiting to read never creates a repeating sway.
 const t=Math.max(0,Math.min(1,elapsed/9)),travel=t*t*(3-2*t);
 if(!speaker||dialogue.kind==='choice'||cue==='Общий план'){
  const [position,target]=gameCamera(scene.kind,{camera:'Общий план'}),side=index%2?-1:1;
  return {position:position.map((v,i)=>v+(i===0?side*.32*travel:i===2?-.2*travel:0)),target,fov:58,smoothing:.8,name:'Общий план диалога',automatic:true};
 }
 const p=resolvedPosition(speaker,state,scene.kind),scale=objectTransform(speaker,state.location,scene.kind).scale[1];
 const listener=characters.find(o=>o.id!==speaker.id);
 const listenerX=listener?resolvedPosition(listener,state,scene.kind)[0]:0;
 const side=Math.sign(listenerX-p[0])||1;
 const close=cue==='Крупный план'||index%3===2;
 const distance=(close?2.05:2.65)*scale;
 const lateral=(close?.35:.6)*scale;
 return {
  position:[p[0]+side*(lateral+.22*scale*travel),p[1]+1.45*scale,p[2]+distance-.24*scale*travel],
  target:[p[0],p[1]+1.08*scale,p[2]],
  fov:close?42:48,smoothing:.65,name:(close?'Крупный':'Средний')+' план · '+speaker.name,automatic:true,
 };
}
