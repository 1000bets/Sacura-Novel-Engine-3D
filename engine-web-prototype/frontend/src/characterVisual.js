import * as THREE from 'three';
import {animatePose} from './sceneEffects.js';
import {parseCharacterGlb,loadModelBuffer,disposeCharacterAsset} from './characterAssets.js';
import {characterAnimationId,characterAnimations} from './characterModel.js';

export function updateCharacterVisual(mesh,character,delta,pose,moving,paused,editing){
 if(character.type!=='Персонаж')return;
 let visual=mesh.userData.characterVisual;
 const source=character.model?.src||null;
 if(!visual||visual.source!==source){
  disposeCharacterVisual(mesh);
  visual={source,originals:[...mesh.children],active:true,clips:[],clipId:null,extraKey:null};mesh.userData.characterVisual=visual;
  if(source)Promise.resolve().then(async()=>parseCharacterGlb(await loadModelBuffer(source))).then(asset=>{
   if(!visual.active){disposeCharacterAsset(asset.scene);return;}
   const wrapper=new THREE.Group(),bounds=new THREE.Box3().setFromObject(asset.scene),size=bounds.getSize(new THREE.Vector3()),center=bounds.getCenter(new THREE.Vector3());
   const factor=1.6/Math.max(size.y,.01);wrapper.scale.setScalar(factor);wrapper.position.set(-center.x*factor,-bounds.min.y*factor,-center.z*factor);wrapper.add(asset.scene);
   asset.scene.traverse(node=>{if(node.isMesh){node.castShadow=true;node.receiveShadow=true;}});
   for(const child of visual.originals)child.visible=false;
   mesh.add(wrapper);visual.wrapper=wrapper;visual.root=asset.scene;visual.mixer=new THREE.AnimationMixer(asset.scene);visual.clips=asset.animations;
  }).catch(error=>{if(visual.active){visual.error=error.message;mesh.userData.characterError=error.message;}});
 }
 if(!source){
  const selected=editing?null:characterAnimationId(character,pose,moving);
  const clip=characterAnimations(character).find(item=>item.id===selected),speed=Math.max(.1,Math.min(4,Number(character.animationSettings?.[selected]?.speed)||1));
  if(mesh.userData.proceduralClip!==selected||mesh.userData.proceduralCue!==character.animationPreviewToken){mesh.userData.characterTime=0;mesh.userData.proceduralClip=selected;mesh.userData.proceduralCue=character.animationPreviewToken;}
  animatePose(mesh,clip?.basePose||selected,mesh.userData.characterTime=(mesh.userData.characterTime||0)+(paused?0:delta*speed),moving&&!editing,paused);return;
 }
 if(!visual.mixer)return;
 if(visual.extraData!==character.extraAnimations||!visual.extraClips){
  visual.extraData=character.extraAnimations;const extraKey=JSON.stringify(character.extraAnimations||[]);
  if(visual.extraKey!==extraKey){visual.extraKey=extraKey;visual.extraClips=new Map((character.extraAnimations||[]).map(clip=>[clip.id,THREE.AnimationClip.parse(clip.clip)]));visual.clipId=undefined;}
 }
 const id=editing?null:characterAnimationId(character,pose,moving),settings=character.animationSettings?.[id]||{};
 if(visual.clipId!==id||visual.cue!==character.animationPreviewToken){
  visual.cue=character.animationPreviewToken;
  visual.mixer.stopAllAction();visual.clipId=id;
  const clip=id?.startsWith('model:')?visual.clips[Number(id.slice(6))]:visual.extraClips.get(id);
  visual.action=clip?visual.mixer.clipAction(clip):null;
  if(visual.action){visual.action.reset();visual.action.clampWhenFinished=true;visual.action.play();}
 }
 if(visual.action){visual.action.setLoop(settings.loop===false?THREE.LoopOnce:THREE.LoopRepeat,settings.loop===false?1:Infinity);visual.action.timeScale=Math.max(.1,Math.min(4,Number(settings.speed)||1));}
 if(!paused&&!editing)visual.mixer.update(delta);
}
export function disposeCharacterVisual(mesh){
 const visual=mesh.userData.characterVisual;if(!visual)return;
 visual.active=false;visual.mixer?.stopAllAction();if(visual.root)visual.mixer?.uncacheRoot(visual.root);
 if(visual.wrapper){mesh.remove(visual.wrapper);disposeCharacterAsset(visual.wrapper);}
 for(const child of visual.originals)child.visible=true;
 delete mesh.userData.characterVisual;delete mesh.userData.characterError;
}
