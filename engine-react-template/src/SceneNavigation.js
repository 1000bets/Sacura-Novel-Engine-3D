import * as THREE from 'three';

const FLY_KEYS = new Set(['KeyW','KeyA','KeyS','KeyD','KeyQ','KeyE','ShiftLeft','ShiftRight']);

// Orbit-compatible interface shared by object framing and the camera rig.
// Gestures are captured before TransformControls so navigation never drags a prop.
export function createSceneNavigation(camera, canvas, getLive) {
  const target = new THREE.Vector3();
  const host = canvas.closest?.('.scene-viewport');
  const win = canvas.ownerDocument?.defaultView || globalThis.window;
  const keys = new Set(), forward = new THREE.Vector3(), right = new THREE.Vector3();
  let gesture = null, pointerId, previousX=0, previousY=0, ignoreUntil=0;
  const clock = () => globalThis.performance?.now() || Date.now();
  const navigation = {
    target, enabled:true, minPolarAngle:0, maxPolarAngle:Math.PI,
    minDistance:.1, maxDistance:150, flySpeed:3,
    get navigating(){return !!gesture;},
    blocksClick(){return !!gesture || clock()<ignoreUntil;},
    update(){},
    tick(dt){
      if(gesture!=='fly'||!navigation.enabled||getLive().mode!=='scene')return;
      camera.getWorldDirection(forward);
      right.set(1,0,0).applyQuaternion(camera.quaternion);
      const direction=new THREE.Vector3()
        .addScaledVector(forward,Number(keys.has('KeyW'))-Number(keys.has('KeyS')))
        .addScaledVector(right,Number(keys.has('KeyD'))-Number(keys.has('KeyA')));
      direction.y+=Number(keys.has('KeyE'))-Number(keys.has('KeyQ'));
      const speed=navigation.flySpeed*(keys.has('ShiftLeft')||keys.has('ShiftRight')?3:1);
      if(direction.lengthSq()){direction.normalize().multiplyScalar(speed*Math.min(dt,.1));camera.position.add(direction);target.add(direction);}
    },
    dispose(){
      stop();
      for(const [name,handler,options] of listeners)canvas.removeEventListener(name,handler,options);
      win?.removeEventListener('keydown',keydown,true);win?.removeEventListener('keyup',keyup,true);win?.removeEventListener('blur',stop);
    },
  };
  const consume=e=>{e.preventDefault?.();e.stopImmediatePropagation?.();};
  const stop=()=>{
    const captured=pointerId;gesture=null;pointerId=undefined;keys.clear();
    if(host)delete host.dataset.navigating;
    if(captured!==undefined){ignoreUntil=clock()+120;if(canvas.hasPointerCapture?.(captured))canvas.releasePointerCapture(captured);}
    canvas.style.cursor='';
  };
  const down=e=>{
    if(getLive().mode!=='scene'||!navigation.enabled)return;
    const requested=e.button===2?'fly':e.button===1?'pan':e.button===0&&e.altKey?'orbit':null;
    if(!requested)return;
    consume(e);gesture=requested;pointerId=e.pointerId;previousX=e.clientX;previousY=e.clientY;
    if(e.shiftKey)keys.add('ShiftLeft');
    if(host){host.dataset.navigating='true';host.focus?.({preventScroll:true});}
    canvas.style.cursor=gesture==='pan'?'grabbing':'move';canvas.setPointerCapture?.(pointerId);
  };
  const move=e=>{
    if(!gesture||e.pointerId!==pointerId)return;
    consume(e);
    const dx=e.clientX-previousX,dy=e.clientY-previousY;previousX=e.clientX;previousY=e.clientY;
    if(!navigation.enabled||getLive().mode!=='scene'){stop();return;}
    if(gesture==='orbit'){
      const sphere=new THREE.Spherical().setFromVector3(camera.position.clone().sub(target));
      sphere.theta-=dx*.006;sphere.phi=THREE.MathUtils.clamp(sphere.phi-dy*.006,.01,Math.PI-.01);
      camera.position.copy(target).add(new THREE.Vector3().setFromSpherical(sphere));camera.lookAt(target);
    }else if(gesture==='pan'){
      const height=Math.max(1,canvas.getBoundingClientRect().height);
      const scale=2*camera.position.distanceTo(target)*Math.tan(THREE.MathUtils.degToRad(camera.fov/2))/height;
      const delta=new THREE.Vector3(-dx*scale,dy*scale,0).applyQuaternion(camera.quaternion);
      camera.position.add(delta);target.add(delta);
    }else{
      const distance=Math.max(.5,camera.position.distanceTo(target));
      const rotation=new THREE.Euler().setFromQuaternion(camera.quaternion,'YXZ');
      rotation.y-=dx*.003;rotation.x=THREE.MathUtils.clamp(rotation.x-dy*.003,-Math.PI/2+.01,Math.PI/2-.01);
      camera.quaternion.setFromEuler(rotation);camera.getWorldDirection(forward);target.copy(camera.position).addScaledVector(forward,distance);
    }
  };
  const up=e=>{if(gesture&&e.pointerId===pointerId){consume(e);stop();}};
  const wheel=e=>{
    if(getLive().mode!=='scene'||!navigation.enabled)return;
    consume(e);
    const delta=e.deltaY*(e.deltaMode===1?16:e.deltaMode===2?canvas.getBoundingClientRect().height:1);
    const offset=camera.position.clone().sub(target),distance=offset.length();
    if(distance<.0001)return;
    const next=THREE.MathUtils.clamp(distance*Math.exp(THREE.MathUtils.clamp(delta,-300,300)*.0015),navigation.minDistance,navigation.maxDistance);
    camera.position.copy(target).add(offset.multiplyScalar(next/distance));
  };
  const keydown=e=>{if(gesture==='fly'&&FLY_KEYS.has(e.code)){keys.add(e.code);consume(e);}};
  const keyup=e=>{if(gesture==='fly'&&FLY_KEYS.has(e.code)){keys.delete(e.code);consume(e);}};
  const context=e=>{if(getLive().mode==='scene')e.preventDefault();};
  const listeners=[['pointerdown',down,true],['pointermove',move,true],['pointerup',up,true],['pointercancel',up,true],['lostpointercapture',stop,true],['wheel',wheel,{capture:true,passive:false}],['contextmenu',context,true]];
  for(const [name,handler,options] of listeners)canvas.addEventListener(name,handler,options);
  win?.addEventListener('keydown',keydown,true);win?.addEventListener('keyup',keyup,true);win?.addEventListener('blur',stop);
  return navigation;
}
