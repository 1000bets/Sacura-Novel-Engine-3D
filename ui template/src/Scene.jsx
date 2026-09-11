import React,{useEffect,useRef} from 'react';
import * as THREE from 'three';
import {OrbitControls} from 'three/examples/jsm/controls/OrbitControls.js';

export default function Scene({objects,state,selected,onSelect,onLetter}){
 const host=useRef(),api=useRef();const callbacks=useRef({onSelect,onLetter});callbacks.current={onSelect,onLetter};
 useEffect(()=>{
  const element=host.current;let renderer;
  try{renderer=new THREE.WebGLRenderer({antialias:true,alpha:true});}catch{element.textContent='3D-превью недоступно в этом браузере. Сценарий и редакторы продолжают работать.';return;}
  renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.PCFSoftShadowMap;renderer.setClearColor(0x191c22);renderer.outputColorSpace=THREE.SRGBColorSpace;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=1.25;element.appendChild(renderer.domElement);
  const scene=new THREE.Scene();scene.fog=new THREE.Fog(0x191c22,20,40);
  const camera=new THREE.PerspectiveCamera(36,1,.1,100);camera.position.set(9,7.8,12);const controls=new OrbitControls(camera,renderer.domElement);controls.target.set(0,1,0);controls.enableDamping=true;controls.maxPolarAngle=Math.PI*.47;controls.minDistance=6;controls.maxDistance=22;controls.enablePan=false;
  scene.add(new THREE.HemisphereLight(0xbfcce7,0x3d292d,2));const sunlight=new THREE.DirectionalLight(0xa5b8e9,3);sunlight.position.set(-3,9,-4);sunlight.castShadow=true;sunlight.shadow.mapSize.set(1024,1024);scene.add(sunlight);
  const lamp=new THREE.PointLight(0xffa553,35,8);lamp.position.set(-2.7,1.6,-1.5);scene.add(lamp);
  const meshes=[],characters=new Map(),pickables=[];
  const mat=(color,extra={})=>new THREE.MeshStandardMaterial({color,roughness:.83,...extra});
  const box=(w,h,d,x,y,z,color)=>{const m=new THREE.Mesh(new THREE.BoxGeometry(w,h,d),mat(color));m.position.set(x,y,z);m.castShadow=true;m.receiveShadow=true;scene.add(m);meshes.push(m);return m;};
  box(9,.22,6.5,0,-.15,0,0x49404a);box(9,3,.16,0,1.3,-3.2,0x515c63);box(.16,3,6.5,-4.5,1.3,0,0x515961);
  for(let x=-4;x<4.5;x+=.45)box(.014,.012,6.5,x,-.024,0,0x6c6263);
  box(9,.08,.13,0,.05,-3.05,0x777c79);box(.14,.08,6.4,-4.35,.05,0,0x777c79);
  // Window, simple mullions and rain; everything in the scene is a primitive.
  box(2.35,2.2,.13,1.35,1.53,-3.06,0x242e3c);const glass=box(2.08,1.94,.04,1.35,1.53,-2.97,0x7b9eac);glass.material.emissive=new THREE.Color(0x34424f);
  box(.07,2,.09,1.35,1.53,-2.9,0x9ca4a3);box(2.15,.07,.09,1.35,1.55,-2.9,0x9ca4a3);box(2.6,.14,.48,1.35,.48,-2.9,0x88918d);
  box(1.45,1.75,.72,-3.1,.85,-2.65,0x8d7c74);box(1.68,.18,.94,-3.1,1.67,-2.57,0xa39485);box(.95,1,.05,-3.1,.64,-2.26,0x29282b);
  for(let i=0;i<5;i++){const flame=new THREE.Mesh(new THREE.ConeGeometry(.09+Math.random()*.06,.4+Math.random()*.2,5),mat(0xf4ae58,{emissive:0xe6852f,emissiveIntensity:1.3}));flame.position.set(-3.42+i*.16,.43,-2.2);scene.add(flame);}
  box(3,.18,2.2,-.6,.01,.8,0x776370);for(let i=0;i<4;i++)box(3,.005,.023,-.6,.11,.05+i*.45,0x99858c);
  box(2.55,.4,.9,2.8,.38,.6,0x9e8175);box(2.55,.67,.26,2.8,.74,.97,0xa58c7b);box(.24,.63,.95,1.56,.58,.6,0xb49b85);box(.24,.63,.95,4.04,.58,.6,0xb49b85);for(let i=0;i<3;i++)box(.72,.14,.7,2.02+i*.77,.63,.51,0xc0a48e);
  box(1.8,.12,1.05,-.25,.71,1.7,0xa58568);for(const x of [-.98,.48])for(const z of [1.3,2.1])box(.09,.66,.09,x,.34,z,0x605450);
  const letter=box(.47,.012,.3,-.35,.782,1.55,0xf4e4bd);letter.rotation.y=.18;letter.userData.id='letter';pickables.push(letter);
  const door=box(1,2.5,.08,3.55,1.13,-3.08,0x718c87);door.userData.id='door';pickables.push(door);box(.07,.07,.1,3.88,1,-2.99,0xbda477);
  box(.23,.23,.23,.2,.85,1.72,0xadb6b3);
  box(.75,.12,.75,-3.65,.78,.85,0x7a6655);box(.15,.9,.15,-3.65,.38,.85,0x605047);const shade=new THREE.Mesh(new THREE.ConeGeometry(.32,.42,12,1,true),mat(0xd6b886,{side:THREE.DoubleSide}));shade.position.set(-3.65,1.73,.85);scene.add(shade);box(.05,.68,.05,-3.65,1.12,.85,0xb49d7a);
  const grid=new THREE.GridHelper(36,36,0x323842,0x282d35);grid.position.y=-.28;scene.add(grid);
  const ring=new THREE.Mesh(new THREE.RingGeometry(.34,.365,48),new THREE.MeshBasicMaterial({color:0xc4adff,side:THREE.DoubleSide}));ring.rotation.x=-Math.PI/2;ring.position.y=.02;scene.add(ring);
  const rainGeometry=new THREE.BufferGeometry();const drops=[];for(let i=0;i<90;i++){let x=.4+Math.random()*1.9,y=.7+Math.random()*1.7;drops.push(x,y,-2.86,x-.035,y-.15,-2.86);}rainGeometry.setAttribute('position',new THREE.Float32BufferAttribute(drops,3));const rain=new THREE.LineSegments(rainGeometry,new THREE.LineBasicMaterial({color:0xcad3dd,transparent:true,opacity:.35}));scene.add(rain);
  const resize=()=>{const {width,height}=element.getBoundingClientRect();renderer.setSize(width,height);camera.aspect=width/height;camera.updateProjectionMatrix();};const ro=new ResizeObserver(resize);ro.observe(element);resize();
  const ray=new THREE.Raycaster(),mouse=new THREE.Vector2();let down;
  const pointerDown=e=>{down=[e.clientX,e.clientY];};
  const click=e=>{if(!down||Math.hypot(e.clientX-down[0],e.clientY-down[1])>5)return;const r=element.getBoundingClientRect();mouse.set((e.clientX-r.left)/r.width*2-1,-(e.clientY-r.top)/r.height*2+1);ray.setFromCamera(mouse,camera);const h=ray.intersectObjects(pickables,true)[0];if(h){let o=h.object;while(!o.userData.id&&o.parent)o=o.parent;if(o.userData.id){callbacks.current.onSelect(o.userData.id);if(o.userData.id==='letter')callbacks.current.onLetter();}}};renderer.domElement.addEventListener('pointerdown',pointerDown);renderer.domElement.addEventListener('pointerup',click);
  api.current={scene,camera,controls,characters,pickables,ring,rain,glass,sunlight,letter,mat,box};let frame;const render=()=>{frame=requestAnimationFrame(render);controls.update();renderer.render(scene,camera);};render();
  return()=>{cancelAnimationFrame(frame);ro.disconnect();controls.dispose();scene.traverse(o=>{o.geometry?.dispose();if(o.material)Array.isArray(o.material)?o.material.forEach(m=>m.dispose()):o.material.dispose();});renderer.dispose();renderer.domElement.remove();api.current=null;};
 },[]);
 useEffect(()=>{
  const a=api.current;if(!a)return;const coords={'камин':[-1.95,0,-.4],'окно':[1.15,0,-1.85],'стол':[-.9,0,1],'диван':[2.7,0,-.4]};
  objects.forEach((o,i)=>{if(o.type==='Персонаж'&&!a.characters.has(o.id)){const group=new THREE.Group();const body=new THREE.Mesh(new THREE.CapsuleGeometry(.23,.65,5,12),a.mat(o.color));body.position.y=.64;body.castShadow=true;const head=new THREE.Mesh(new THREE.SphereGeometry(.18,20,12),a.mat(0xe1c6ad));head.position.y=1.26;head.castShadow=true;group.add(body,head);group.userData.id=o.id;a.scene.add(group);a.characters.set(o.id,group);a.pickables.push(group);}else if(!['alice','bob','letter','fireplace','door'].includes(o.id)&&!a.characters.has(o.id)){const m=a.box(.5,.5,.5,0,.25,0,o.color||'#98b4ac');m.userData.id=o.id;a.characters.set(o.id,m);a.pickables.push(m);}
   const mesh=a.characters.get(o.id);if(mesh){const pos=coords[state.positions?.[o.id]||o.position]||[-2+i*.7,0,2];mesh.position.set(...pos);mesh.visible=o.active&&state.visible?.[o.id]!==false;if(o.type==='Персонаж'){mesh.children[0].material.color.set(o.color);mesh.rotation.y=state.poses?.[o.id]==='задумчивость'?.4:0;}}
  });
  a.characters.forEach((mesh,id)=>{if(!objects.some(o=>o.id===id))mesh.visible=false;});
  const selectedMesh=a.characters.get(selected)|| (selected==='letter'?a.letter:null);a.ring.visible=!!selectedMesh;if(selectedMesh){a.ring.position.x=selectedMesh.position.x;a.ring.position.z=selectedMesh.position.z;}
  a.letter.visible=!!objects.find(o=>o.id==='letter')?.active&&state.visible?.letter!==false;a.rain.visible=state.weather!=='Ясно';a.sunlight.intensity=state.time==='Ночь'?.7:3;a.glass.material.color.set(state.time==='Ночь'?0x465b77:state.time==='Рассвет'?0xcbb39c:0x7b9eac);
  if(state.camera==='Крупный план'){a.camera.position.set(5,4,7);a.controls.target.set(-.7,.8,.3);}else{a.camera.position.set(9,7.8,12);a.controls.target.set(0,1,0);}
 },[objects,state,selected]);
 return <div className="scene-canvas" ref={host} aria-label="Интерактивная 3D-гостиная. Вращайте мышью; нажмите на персонажа или письмо."/>;
}
