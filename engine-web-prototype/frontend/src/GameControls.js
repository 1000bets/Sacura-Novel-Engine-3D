import * as THREE from 'three';
import {InputSystem} from './InputSystem.js';
import {defaultInputSettings} from './inputModel.js';
export function gameSceneHit(hits,state,mode){
 const visible=hits.filter(h=>{for(let o=h.object;o;o=o.parent)if(o.visible===false)return false;return true;});
 return (mode==='game'&&visible.find(h=>h.object.parent?.userData?.marker===h.object&&state.interactionTargets?.includes(h.object.parent.userData.id)))||visible[0];
}
export function createGameControls(camera,canvas,getLive){
 const viewport=canvas.closest('.scene-viewport')||canvas,win=globalThis.window;
 let move=[0,0],system,settings,deviceSignature='',padIds=new Set(),padsNeedNeutral=false;
 const focused=()=>!globalThis.document||viewport.contains?.(document.activeElement)||document.activeElement===viewport;
 const enabled=()=>{const live=getLive();return live.mode==='game'&&(live.state.inputActive??!!live.state.playerControl);};
 const action=event=>{const live=getLive();
  if(event.action.behavior==='move'){move=event.phase==='triggered'?event.value:event.phase==='completed'||event.phase==='canceled'?[0,0]:move;}
  if(live.onInputAction)live.onInputAction(event);
  else if(event.phase==='triggered'&&!live.state.paused){if(event.action.behavior==='interact')live.onPlayerInteract?.();if(event.action.behavior==='point')live.onPlayerClick?.(event.payload?.point,event.payload?.id);}
 };
 const sync=()=>{
  const live=getLive(),config=live.inputSettings||defaults;
  if(settings!==config){system?.dispose();settings=config;system=new InputSystem(settings,action);move=[0,0];}
  system.setContexts(!enabled()?[]:live.state.paused||live.state.inputReady===false?['system']:live.state.inputContexts||['system','gameplay']);
  return enabled();
 };
 const defaults=defaultInputSettings();
 const clear=()=>{system?.clear();move=[0,0];padsNeedNeutral=true;};
 const uses=(device,control)=>settings.contexts.some(c=>system.contextIds.includes(c.id)&&c.bindings.some(b=>b.device===device&&b.control===control));
 const keydown=e=>{if(!sync()||(!focused()&&e.target!==viewport)||(e.target!==viewport&&!(getLive().state.paused&&e.code==='Escape'))||e.ctrlKey||e.metaKey||e.altKey||e.target.isContentEditable||['INPUT','TEXTAREA','SELECT'].includes(e.target.tagName))return;
  if(!uses('keyboard',e.code))return;e.preventDefault();if(e.repeat)return;
  system.feed('keyboard',e.code,1);system.tick(0);
 };
 const keyup=e=>{system?.feed('keyboard',e.code,0);system?.tick(0);};
 const pointerDown=e=>{if(e.target!==canvas||!sync())return;const code='button'+e.button;if(uses('pointer',code)){system.feed('pointer',code,1);system.tick(0);}};
 const pointerUp=e=>{system?.feed('pointer','button'+e.button,0);system?.tick(0);};
 const visibility=()=>{if(globalThis.document?.hidden)clear();};
 const external=e=>{if(!sync())return;const d=e.detail||{};if(typeof d.control!=='string')return;
  if(d.pulse)system.pulse(d.device||'external',d.control,d.payload,d.sourceId||'external-pulse');
  else {system.feed(d.device||'external',d.control,d.value,{sourceId:d.sourceId||d.device||'external',payload:d.payload});system.tick(0);}
 };
 const pollPads=()=>{
  let pads=[];try{pads=Array.from(globalThis.navigator?.getGamepads?.()||[]).filter(p=>p?.connected!==false&&p);}catch{}
  const signature=pads.map(p=>p.index+':'+p.id+':'+p.mapping).join('|');
  if(signature!==deviceSignature){deviceSignature=signature;getLive().onInputDevices?.(pads.map(p=>({index:p.index,name:p.id,mapping:p.mapping})));}
  if(!focused()||globalThis.document?.hidden){for(const id of padIds)system.removeSource(id,{cancel:true});padsNeedNeutral=true;return;}
  const neutral=pads.every(p=>p.axes.every(v=>Math.abs(v)<.2)&&p.buttons.every(b=>!(typeof b==='number'?b>.5:b.pressed)));
  if(padsNeedNeutral){if(neutral)padsNeedNeutral=false;else return;}
  const current=new Set();for(const pad of pads){const id='gamepad:'+pad.index;current.add(id);
   pad.axes.forEach((v,i)=>system.feed('gamepad-axis',String(i),v,{sourceId:id}));
   pad.buttons.forEach((b,i)=>system.feed('gamepad-button',String(i),typeof b==='number'?b:b.value??Number(b.pressed),{sourceId:id}));
  }
  for(const id of padIds)if(!current.has(id))system.removeSource(id,{cancel:true});padIds=current;
 };
 viewport.addEventListener('keydown',keydown);viewport.addEventListener('pointerdown',pointerDown);win?.addEventListener('pointerup',pointerUp);globalThis.document?.addEventListener('visibilitychange',visibility);win?.addEventListener('keyup',keyup);win?.addEventListener('blur',clear);viewport.addEventListener('blur',clear,true);viewport.addEventListener('sacura-input',external);
 sync();
 return {tick(dt){if(!sync()){clear();return;}pollPads();system.tick(dt);
  const c=getLive().state.playerControl;if(!c||getLive().state.paused)return;
  const allowed=['wasd','both'].includes(c.mode),x=allowed?move[0]:0,z=allowed?move[1]:0;
  const forward=camera.getWorldDirection(new THREE.Vector3());forward.y=0;if(forward.lengthSq()<.001)forward.set(0,0,-1);forward.normalize();const right=new THREE.Vector3(-forward.z,0,forward.x),direction=forward.multiplyScalar(z).add(right.multiplyScalar(x));
  getLive().onPlayerStep?.(direction.toArray(),dt);
 },click(ray,id){if(!sync()||getLive().state.paused)return false;if(!uses('pointer','primary'))return getLive().state.inputActive===true;
  const ground=ray.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0,1,0),0),new THREE.Vector3());system.pulse('pointer','primary',{point:ground?.toArray(),id});return true;
 },feed(device,control,value,options){if(sync()){system.feed(device,control,value,options);system.tick(0);}},get input(){return system;},dispose(){clear();system?.dispose();viewport.removeEventListener('keydown',keydown);viewport.removeEventListener('pointerdown',pointerDown);win?.removeEventListener('pointerup',pointerUp);globalThis.document?.removeEventListener('visibilitychange',visibility);win?.removeEventListener('keyup',keyup);win?.removeEventListener('blur',clear);viewport.removeEventListener('blur',clear,true);viewport.removeEventListener('sacura-input',external);}};
}
