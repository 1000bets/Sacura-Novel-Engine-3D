import * as THREE from 'three';

import {resolvedPosition} from './sceneEditing.js';
export {ANCHORS} from './sceneEditing.js';
export function objectPosition(object,state,kind){return new THREE.Vector3(...resolvedPosition(object,state,kind));}
const highlightMaterials=new WeakMap();
export function updateObjectHighlight(mesh,active,time){
 mesh.traverse(part=>{for(const material of Array.isArray(part.material)?part.material:part.material?[part.material]:[]){
  if(!material.emissive)continue;
  if(active){if(!highlightMaterials.has(material))highlightMaterials.set(material,{color:material.emissive.clone(),intensity:material.emissiveIntensity});material.emissive.set('#c9a55e');material.emissiveIntensity=.28+Math.sin(time*3)*.1;}
  else if(highlightMaterials.has(material)){const base=highlightMaterials.get(material);material.emissive.copy(base.color);material.emissiveIntensity=base.intensity;highlightMaterials.delete(material);}
 }});
}
export function gameCamera(kind,state,objects=[]){
 const focus=state.interactionTarget||(state.camera==='План предмета'?(kind==='living'?'letter':kind==='garden'?'garden-note':'ticket'):null);
 const object=objects.find(o=>(state.interactionTarget?o.id===focus:(o.builtin||o.id)===focus)&&o.active!==false&&state.visible?.[o.id]!==false);
 if(object){const p=resolvedPosition(object,state,kind);return [[p[0]-1.35,p[1]+1.2,p[2]+1.6],[p[0],p[1],p[2]]];}
 if(focus==='letter')return [[-1.7,1.55,2.7],[-.35,.55,1.55]];
 if(focus==='garden-note')return [[.35,1.65,2.4],[2,.55,.5]];
 if(focus==='ticket')return [[0,1.6,2.5],[1.4,.6,1.2]];
 if(state.camera==='Крупный план')return kind==='living'?[[-.8,1.6,2.2],[-1.4,1.1,-.5]]:kind==='garden'?[[.1,1.6,2.5],[-.7,1,-.5]]:[[-.5,1.6,2],[.3,1,-.6]];
 return kind==='living'?[[-2.8,1.65,2.8],[.3,1.02,-1.3]]:kind==='garden'?[[0,1.7,3],[1,1.1,-1]]:[[-1,1.7,2.6],[.5,1.1,-1.7]];
}
export function labelSprite(text){
 const canvas=document.createElement('canvas');canvas.width=512;canvas.height=96;const c=canvas.getContext('2d');
 c.fillStyle='#22252deb';c.beginPath();c.roundRect(2,2,508,92,15);c.fill();c.strokeStyle='#d8b584';c.lineWidth=3;c.stroke();c.fillStyle='#f4e1bd';c.font='500 27px Segoe UI';c.textAlign='center';c.textBaseline='middle';c.fillText(text,256,48);
 const texture=new THREE.CanvasTexture(canvas),sprite=new THREE.Sprite(new THREE.SpriteMaterial({map:texture,depthTest:false}));sprite.scale.set(1.3,.245,1);sprite.renderOrder=10;return sprite;
}
export function decoratePaper(mesh,label){
 mesh.geometry.dispose();mesh.geometry=new THREE.BoxGeometry(.64,.025,.43);
 for(let i=0;i<4;i++){const line=new THREE.Mesh(new THREE.BoxGeometry(.36-i*.03,.002,.008),new THREE.MeshBasicMaterial({color:'#8c7961'}));line.position.set(-.025,.014,-.1+i*.055);mesh.add(line);}
 const seal=new THREE.Mesh(new THREE.CylinderGeometry(.043,.043,.005,16),new THREE.MeshStandardMaterial({color:'#9a5e69'}));seal.position.set(.21,.017,.12);mesh.add(seal);
 const marker=labelSprite(label);marker.position.set(0,.46,0);mesh.add(marker);mesh.userData.marker=marker;
}
export function animatePose(mesh,pose,t,moving,paused){
 if(!mesh.userData.isCharacter)return;
 const parts=mesh.userData.parts||{};
 const dance=pose==='танец',sad=pose==='грусть',smile=pose==='улыбка';
 mesh.rotation.z+=dance?Math.sin(t*4)*.12:0;
 mesh.rotation.y+=pose==='задумчивость'?.35:dance?Math.sin(t*2)*.7:0;
 if(parts.head)parts.head.rotation.z=sad?.24:smile?-.13:0;
 if(parts.left)parts.left.rotation.z=dance?-.8-Math.sin(t*5)*.3:smile?-.6:-.12;
 if(parts.right)parts.right.rotation.z=dance?.8+Math.sin(t*5)*.3:pose==='задумчивость'?-1.1:.12;
 if(parts.left)parts.left.rotation.x=moving?Math.sin(t*9)*.6:0;
 if(parts.right)parts.right.rotation.x=moving?-Math.sin(t*9)*.6:0;
 if(parts.face)parts.face.scale.y=smile?.6:sad?1.5:1;
}
export function addCharacterDetails(group,mat){
 const head=group.children[1];const parts={head};
 for(const [name,x]of [['left',-.29],['right',.29]]){const arm=new THREE.Mesh(new THREE.CapsuleGeometry(.075,.38,4,8),mat.clone());arm.position.set(x,.76,0);group.add(arm);parts[name]=arm;}
 for(const x of [-.065,.065]){const eye=new THREE.Mesh(new THREE.SphereGeometry(.026,8,6),new THREE.MeshBasicMaterial({color:'#343139'}));eye.position.set(x,.025,.163);head.add(eye);}
 const face=new THREE.Mesh(new THREE.BoxGeometry(.07,.014,.008),new THREE.MeshBasicMaterial({color:'#775150'}));face.position.set(0,-.065,.17);head.add(face);parts.face=face;
 group.userData.isCharacter=true;group.userData.parts=parts;
}

export function atmosphere(scene,{kind,renderer,sun,hemi}){
 const indoor=kind==='living';let elapsed=0,weatherClock=0,particleClock=0,lastWeather;
 const geometry=new THREE.BufferGeometry(),drops=[],seeds=[];
 for(let i=0;i<700;i++){const seed={x:indoor?.4+Math.random()*1.9:Math.random()*24-12,y:Math.random()*7,z:indoor?-2.84:Math.random()*22-12};seeds.push(seed);drops.push(seed.x,seed.y,seed.z,seed.x,seed.y-.2,seed.z);}
 geometry.setAttribute('position',new THREE.Float32BufferAttribute(drops,3));
 const rain=new THREE.LineSegments(geometry,new THREE.LineBasicMaterial({color:'#bdcddd',transparent:true,opacity:.65}));scene.add(rain);
 const snowGeo=new THREE.BufferGeometry();snowGeo.setAttribute('position',new THREE.Float32BufferAttribute(new Array(700*3).fill(0),3));
 const mask=document.createElement('canvas');mask.width=32;mask.height=32;const ctx=mask.getContext('2d'),gradient=ctx.createRadialGradient(16,16,2,16,16,16);gradient.addColorStop(0,'#ffffff');gradient.addColorStop(1,'#ffffff00');ctx.fillStyle=gradient;ctx.fillRect(0,0,32,32);const particleMap=new THREE.CanvasTexture(mask);
 const snow=new THREE.Points(snowGeo,new THREE.PointsMaterial({color:'#eaf0f8',size:indoor?2:4,sizeAttenuation:false,map:particleMap,transparent:true,depthWrite:false,opacity:.9}));scene.add(snow);
 const motesGeo=new THREE.BufferGeometry(),motes=[];for(let i=0;i<160;i++)motes.push(Math.random()*8-4,Math.random()*3,Math.random()*5-2.5);motesGeo.setAttribute('position',new THREE.Float32BufferAttribute(motes,3));
 const motesMesh=new THREE.Points(motesGeo,new THREE.PointsMaterial({color:'#efcda4',size:5,sizeAttenuation:false,map:particleMap,transparent:true,opacity:.8,depthWrite:false}));scene.add(motesMesh);
 const flash=new THREE.PointLight('#c3ddff',0,35);flash.position.set(0,5,-2);scene.add(flash);
 scene.fog=new THREE.FogExp2('#819098',0);
 const sky=new THREE.Color(),lightColor=new THREE.Color();
 return (state,dt)=>{
  if(!state.paused)elapsed+=dt;
  if(!state.paused&&!state.weatherPaused)weatherClock+=dt;
  if(!state.paused&&!state.particlesPaused)particleClock+=dt;
  const weather=state.weather||'Ясно',time=state.time||'День',storm=weather==='Гроза',fog=weather==='Туман',snowing=weather==='Снег';
  if(lastWeather!==weather){weatherClock=0;lastWeather=weather;}
  rain.visible=weather==='Дождь'||storm;snow.visible=snowing;
  geometry.setDrawRange(0,indoor?(storm?800:280):(storm?1400:600));
  const rp=geometry.attributes.position.array,sp=snowGeo.attributes.position.array;
  seeds.forEach((seed,i)=>{
   let y=indoor?.56+((seed.y-weatherClock*(storm?2.6:1.6))%1.9+1.9)%1.9:((seed.y-weatherClock*(storm?8:5))%7+7)%7;
   rp[i*6]=seed.x;rp[i*6+1]=y;rp[i*6+2]=seed.z;rp[i*6+3]=seed.x-(storm?.1:.025);rp[i*6+4]=y-(indoor?.13:storm?.5:.28);rp[i*6+5]=seed.z;
   sp[i*3]=seed.x+Math.sin(weatherClock+i)*.05;sp[i*3+1]=indoor?.56+((seed.y-weatherClock*.25)%1.9+1.9)%1.9:((seed.y-weatherClock*.65)%7+7)%7;sp[i*3+2]=seed.z;
  });geometry.attributes.position.needsUpdate=true;snowGeo.attributes.position.needsUpdate=true;
  const night=time==='Ночь',dawn=time==='Рассвет',dusk=time==='Закат';
  sky.set(fog?'#899da4':storm?(night?'#222f42':'#465b70'):night?'#182a43':dawn?'#a6b9c6':dusk?'#b99283':'#89bddb');
  if(!scene.background)scene.background=sky.clone();scene.background.lerp(sky,1-Math.exp(-dt*3));scene.fog.color.copy(scene.background);
  scene.fog.density=THREE.MathUtils.damp(scene.fog.density,fog?(indoor?.075:.12):indoor?0:storm?.026:.009,3,dt);
  sun.intensity=THREE.MathUtils.damp(sun.intensity,state.lighting==='Выключить'?.12:night?.65:storm?1.1:dusk?2.2:3.2,3,dt);
  lightColor.set(state.lighting==='Холодный свет'?'#77aaff':state.lighting==='Тёплый свет'?'#ffb070':night?'#7897d0':dawn?'#ffe0b4':dusk?'#ffb179':'#fff6df');sun.color.lerp(lightColor,1-Math.exp(-dt*3));
  if(hemi)hemi.intensity=THREE.MathUtils.damp(hemi.intensity,night?.85:1.7,3,dt);
  renderer.toneMappingExposure=THREE.MathUtils.damp(renderer.toneMappingExposure,state.lighting==='Приглушить'?.55:state.lighting==='Яркий свет'?1.65:1.12,3,dt);
  const lightning=storm&&weatherClock%7>.9&&weatherClock%7<1.12;flash.intensity=lightning?16:0;
  motesMesh.visible=!!state.particles&&state.particles!=='Выключить';motesMesh.material.color.set(state.particles==='Лепестки'?'#e7a2ba':'#f0dba3');
  motesMesh.material.size=state.particles==='Лепестки'?7:4;
  if(!state.paused&&!state.particlesPaused){const p=motesGeo.attributes.position.array;for(let i=0;i<160;i++){p[i*3]=motes[i*3]+Math.sin(particleClock*.6+i)*.24;p[i*3+1]=state.particles==='Лепестки'?3-((particleClock*.35+i*.13)%3):motes[i*3+1]+Math.sin(particleClock+i)*.12;}motesGeo.attributes.position.needsUpdate=true;}
  return elapsed;
 };
}
