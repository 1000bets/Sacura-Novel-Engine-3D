import * as THREE from 'three';
import {clone} from 'three/addons/utils/SkeletonUtils.js';
import {parseCharacterGlb,loadModelBuffer,disposeCharacterAsset} from './characterAssets.js';
import {addCharacterDetails,animatePose} from './sceneEffects.js';

// All cards share one WebGL context. Only visible cards render, at 24 fps.
const items=new Set(),assets=new Map();
let renderer,frame,lastFrame=0;
function acquireModel(source){
 let entry=assets.get(source);
 if(!entry){entry={users:0,promise:Promise.resolve().then(async()=>parseCharacterGlb(await loadModelBuffer(source)))};assets.set(source,entry);}
 entry.users++;
 return {promise:entry.promise,release(){if(--entry.users===0){assets.delete(source);entry.promise.then(asset=>disposeCharacterAsset(asset.scene)).catch(()=>{});}}};
}
function ensureRenderer(){
 if(renderer)return;
 renderer=new THREE.WebGLRenderer({antialias:true,alpha:false});renderer.setPixelRatio(1);renderer.setClearColor('#252a34');renderer.toneMapping=THREE.ACESFilmicToneMapping;
}
function standardFigure(color){
 const root=new THREE.Group(),material=new THREE.MeshStandardMaterial({color});
 const body=new THREE.Mesh(new THREE.CapsuleGeometry(.24,.65,4,12),material);body.position.y=.76;root.add(body);
 const head=new THREE.Mesh(new THREE.SphereGeometry(.21,16,12),new THREE.MeshStandardMaterial({color:'#e4d2bc'}));head.position.y=1.43;root.add(head);addCharacterDetails(root,material);return root;
}
function tick(now){
 frame=requestAnimationFrame(tick);
 if(now-lastFrame<1000/24)return;lastFrame=now;
 for(const item of items){
  if(!item.visible||!item.active)continue;
  const {character,clip}=item.read();
  if(!item.started){
   item.started=true;
   if(character.model?.src){
    item.lease=acquireModel(character.model.src);
    item.lease.promise.then(asset=>{
     if(!item.active)return;
     item.model=clone(asset.scene);
     const bounds=new THREE.Box3().setFromObject(item.model),size=bounds.getSize(new THREE.Vector3()),center=bounds.getCenter(new THREE.Vector3()),factor=1.6/Math.max(size.y,.01);
     const wrapper=new THREE.Group();wrapper.scale.setScalar(factor);wrapper.position.set(-center.x*factor,-bounds.min.y*factor,-center.z*factor);wrapper.add(item.model);item.scene.add(wrapper);item.placeholder.visible=false;
     item.mixer=new THREE.AnimationMixer(item.model);item.modelClips=asset.animations;
    }).catch(()=>item.onError('Не удалось загрузить модель'));
   }
  }
  const delta=Math.min(.1,(now-item.previous)/1000);item.previous=now;
  const settings=character.animationSettings?.[clip.id]||{},speed=Math.max(.1,Math.min(4,Number(settings.speed)||1));
  if(item.mixer){
   const serialized=clip.clip;
   if(item.clipId!==clip.id||item.serialized!==serialized){
    item.mixer.stopAllAction();item.clipId=clip.id;item.serialized=serialized;
    const actual=clip.id.startsWith('model:')?item.modelClips[Number(clip.id.slice(6))]:serialized?THREE.AnimationClip.parse(serialized):null;
    item.action=actual?item.mixer.clipAction(actual):null;item.action?.reset().play();
   }
   // Thumbnails always repeat so the movement remains recognizable.
   if(item.action){item.action.setLoop(THREE.LoopRepeat,Infinity);item.action.timeScale=speed;}
   item.mixer.update(delta);
  }else if(!character.model){
   item.time+=delta*speed;item.placeholder.rotation.set(0,0,0);item.placeholder.children[0].material.color.set(character.color||'#bb99aa');
   animatePose(item.placeholder,clip.basePose||clip.id,item.time,false,false);
  }
  const width=Math.max(1,Math.round(item.canvas.clientWidth)),height=Math.max(1,Math.round(item.canvas.clientHeight));
  if(item.canvas.width!==width||item.canvas.height!==height){item.canvas.width=width;item.canvas.height=height;}
  if(renderer.domElement.width!==width||renderer.domElement.height!==height)renderer.setSize(width,height,false);
  item.camera.aspect=width/height;item.camera.updateProjectionMatrix();renderer.render(item.scene,item.camera);
  item.context.drawImage(renderer.domElement,0,0,width,height);
 }
}
export function registerAnimationThumbnail(canvas,read,onError){
 try{ensureRenderer();}catch{onError('3D-превью недоступно');return()=>{};}
 const scene=new THREE.Scene(),camera=new THREE.PerspectiveCamera(36,1,.05,100);
 camera.position.set(2.3,1.6,3.2);camera.lookAt(0,.85,0);
 scene.add(new THREE.HemisphereLight('#edf1ff','#484154',2.5));const light=new THREE.DirectionalLight('#fff0df',3);light.position.set(3,5,4);scene.add(light);
 const placeholder=standardFigure(read().character.color||'#bb99aa');scene.add(placeholder);
 const ground=new THREE.Mesh(new THREE.CircleGeometry(1.35,48),new THREE.MeshStandardMaterial({color:'#343b47',roughness:1}));ground.rotation.x=-Math.PI/2;ground.position.y=-.02;scene.add(ground);
 const item={canvas,context:canvas.getContext('2d'),read,onError,scene,camera,placeholder,active:true,visible:false,started:false,time:0,previous:performance.now()};
 if(!item.context){disposeCharacterAsset(scene);onError('Превью недоступно');return()=>{};}
 const observer=new IntersectionObserver(entries=>{item.visible=entries[0].isIntersecting;item.previous=performance.now();});observer.observe(canvas);items.add(item);
 if(items.size===1){lastFrame=0;frame=requestAnimationFrame(tick);}
 return()=>{
  observer.disconnect();item.active=false;items.delete(item);item.mixer?.stopAllAction();if(item.model)item.mixer?.uncacheRoot(item.model);
  item.lease?.release();disposeCharacterAsset(placeholder);ground.geometry.dispose();ground.material.dispose();
  if(!items.size){cancelAnimationFrame(frame);renderer.dispose();renderer.forceContextLoss();renderer=null;}
 };
}
