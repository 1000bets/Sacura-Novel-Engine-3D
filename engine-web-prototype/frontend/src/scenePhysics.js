import * as THREE from 'three';

const finite=(v,fallback)=>Number.isFinite(Number(v))?Number(v):fallback;
export function navMeshSettings(scene={}){
 const n=scene.navMesh||{},size=scene.kind==='living'?[8.6,6]:[20,20];
 return {enabled:n.enabled===true,show:n.show!==false,showColliders:n.showColliders===true,
  center:[0,0,0].map((v,i)=>finite(n.center?.[i],v)),
  size:size.map((v,i)=>Math.max(.5,Math.min(200,finite(n.size?.[i],v)))),
  cellSize:Math.max(.1,Math.min(2,finite(n.cellSize,.3)))};
}
export function collisionSettings(object={}){
 const c=object.collision||{};
 return {enabled:c.enabled===true,custom:c.custom===true,
  size:[.6,1.6,.6].map((v,i)=>Math.max(.01,Math.min(200,finite(c.size?.[i],v)))),
  offset:[0,.8,0].map((v,i)=>finite(c.offset?.[i],v))};
}
// Geometry only: labels, lights and editor helpers never enlarge a collider.
export function objectCollisionBox(mesh,object){
 const config=collisionSettings(object),box=new THREE.Box3();mesh.updateWorldMatrix(true,true);
 if(config.custom){const half=new THREE.Vector3(...config.size).multiplyScalar(.5),center=new THREE.Vector3(...config.offset);return box.set(center.clone().sub(half),center.clone().add(half)).applyMatrix4(mesh.matrixWorld);}
 const visit=node=>{if(!node.visible)return;if(node.isMesh){if(!node.geometry.boundingBox)node.geometry.computeBoundingBox();box.union(node.geometry.boundingBox.clone().applyMatrix4(node.matrixWorld));}for(const child of node.children)visit(child);};visit(mesh);return box;
}
// Slab intersection with open interiors permits touching a floor or a wall.
export function segmentBlocked(from,to,boxes){
 return boxes.some(box=>{
  let lo=0,hi=1;
  for(let i=0;i<3;i++){
   const axis=['x','y','z'][i],min=box.min[axis]+1e-5,max=box.max[axis]-1e-5,d=to[i]-from[i];
   if(min>=max)return false;
   if(Math.abs(d)<1e-10){if(from[i]<=min||from[i]>=max)return false;}
   else {let a=(min-from[i])/d,b=(max-from[i])/d;if(a>b)[a,b]=[b,a];lo=Math.max(lo,a);hi=Math.min(hi,b);if(lo>hi)return false;}
  }
  return hi>=0&&lo<=1;
 });
}
export function samplePath(path,progress){
 if(!path?.length)return null;if(path.length===1)return [...path[0]];
 const lengths=path.slice(1).map((p,i)=>Math.hypot(...p.map((v,j)=>v-path[i][j]))),total=lengths.reduce((a,b)=>a+b,0);
 let distance=THREE.MathUtils.clamp(progress||0,0,1)*total;
 for(let i=0;i<lengths.length;i++){if(distance<=lengths[i]){const t=lengths[i]?distance/lengths[i]:0;return path[i].map((v,j)=>v+(path[i+1][j]-v)*t);}distance-=lengths[i];}
 return [...path.at(-1)];
}
// Preserve corners when a single runtime tick spans multiple route edges.
export function pathSection(path,fromProgress,toProgress){
 const lengths=path.slice(1).map((p,i)=>Math.hypot(...p.map((v,j)=>v-path[i][j]))),total=lengths.reduce((a,b)=>a+b,0);
 const points=[samplePath(path,fromProgress)];let distance=0;
 for(let i=0;i<lengths.length;i++){distance+=lengths[i];if(distance>fromProgress*total&&distance<toProgress*total)points.push(path[i+1]);}
 points.push(samplePath(path,toProgress));return points;
}
export function findFlatPath(from,to,nav,boxes=[],padding=[0,0]){
 const y=nav.center[1],start=[from[0],y,from[2]],goal=[to[0],y,to[2]];
 const min=[nav.center[0]-nav.size[0]/2+padding[0],nav.center[2]-nav.size[1]/2+padding[1]],max=[nav.center[0]+nav.size[0]/2-padding[0],nav.center[2]+nav.size[1]/2-padding[1]];
 const inside=p=>p[0]>=min[0]&&p[0]<=max[0]&&p[2]>=min[1]&&p[2]<=max[1];
 if(!inside(start)||!inside(goal))throw new Error('Начало или цель движения за пределами nav mesh. Увеличьте область ходьбы или измените точку.');
 if(segmentBlocked(start,start,boxes)||segmentBlocked(goal,goal,boxes))throw new Error('Точка движения находится внутри коллизии. Переместите точку или отключите коллизию препятствия.');
 if(!segmentBlocked(start,goal,boxes))return [start,goal];
 // Bound the search to 128 × 128 nodes even on a large authored plane.
 const step=Math.max(nav.cellSize,...nav.size.map(v=>v/127)),nx=Math.floor((max[0]-min[0])/step)+1,nz=Math.floor((max[1]-min[1])/step)+1;
 const nodes=[],neighbors=[[1,0],[-1,0],[0,1],[0,-1],[1,1],[-1,1],[1,-1],[-1,-1]];
 for(let z=0;z<nz;z++)for(let x=0;x<nx;x++)nodes.push([min[0]+x*step,y,min[1]+z*step]);
 const nearest=p=>{let best=-1,d=Infinity;for(let i=0;i<nodes.length;i++){const dist=Math.hypot(nodes[i][0]-p[0],nodes[i][2]-p[2]);if(dist<d&&!segmentBlocked(p,nodes[i],boxes)){best=i;d=dist;}}return best;};
 const first=nearest(start),last=nearest(goal);if(first<0||last<0)throw new Error('Нет прохода на nav mesh. Проверьте коллизии и размер ячейки.');
 const scores=new Map([[first,0]]),parents=new Map(),open=new Set([first]),closed=new Set(),heuristic=i=>Math.hypot(nodes[i][0]-nodes[last][0],nodes[i][2]-nodes[last][2]);
 while(open.size){let current=-1,best=Infinity;for(const i of open){const score=scores.get(i)+heuristic(i);if(score<best){best=score;current=i;}}open.delete(current);
  if(current===last){const route=[goal];for(let i=last;i!==undefined;i=parents.get(i))route.push(nodes[i]);route.push(start);route.reverse();const simplified=[route[0]];for(let i=1;i<route.length;){let end=route.length-1;while(end>i&&segmentBlocked(simplified.at(-1),route[end],boxes))end--;simplified.push(route[end]);i=end+1;}return simplified;}
  closed.add(current);const x=current%nx,z=Math.floor(current/nx);
  for(const [dx,dz]of neighbors){const xx=x+dx,zz=z+dz;if(xx<0||xx>=nx||zz<0||zz>=nz)continue;const next=zz*nx+xx;if(closed.has(next)||segmentBlocked(nodes[current],nodes[next],boxes))continue;const score=scores.get(current)+Math.hypot(dx,dz)*step;if(score<(scores.get(next)??Infinity)){scores.set(next,score);parents.set(next,current);open.add(next);}}
 }
 throw new Error('Путь по nav mesh не найден. Проверьте проходы между коллизиями.');
}

export function createScenePhysics(scene,getLive,getMeshes){
 const overlay=new THREE.Group();overlay.name='Navigation debug';scene.add(overlay);let key,plane;
 const helpers=new Map();
 const entries=()=>{const live=getLive();return getMeshes().flatMap(mesh=>{const object=live.objects.find(o=>o.id===mesh.userData.id);if(!object||object.active===false||!mesh.visible||!collisionSettings(object).enabled)return [];const box=objectCollisionBox(mesh,object);return box.isEmpty()?[]:[{mesh,object,box}];});};
 const obstacles=object=>{
  if(!collisionSettings(object).enabled)return {boxes:[],padding:[0,0]};
  const mesh=getMeshes().find(m=>m.userData.id===object.id);if(!mesh)return {boxes:[],padding:[0,0]};
  const bounds=objectCollisionBox(mesh,object),origin=mesh.getWorldPosition(new THREE.Vector3());
  if(bounds.isEmpty())return {boxes:[],padding:[0,0]};
  const lower=bounds.min.clone().sub(origin),upper=bounds.max.clone().sub(origin);
  const boxes=entries().filter(e=>e.object.id!==object.id).map(e=>new THREE.Box3(e.box.min.clone().sub(upper),e.box.max.clone().sub(lower)));
  return {boxes,padding:[Math.max(-lower.x,upper.x),Math.max(-lower.z,upper.z)]};
 };
 return {
  sceneId:()=>getLive().sceneId,
  colliders:()=>entries().map(e=>({id:e.object.id,box:{min:e.box.min.toArray(),max:e.box.max.toArray()}})),
  raycast(origin,direction,range,exclude=[]){const ray=new THREE.Raycaster(new THREE.Vector3(...origin),new THREE.Vector3(...direction).normalize(),0,range),meshes=getMeshes().filter(m=>!exclude.includes(m.userData.id)&&m.visible);for(const hit of ray.intersectObjects(meshes,true)){if(!hit.object.isMesh)continue;let visible=true,id;for(let n=hit.object;n;n=n.parent){if(n.visible===false||n.userData.marker===hit.object)visible=false;if(n.userData.id)id=n.userData.id;}if(visible&&id&&!exclude.includes(id))return {id,distance:hit.distance,point:hit.point.toArray()};}return null;},
  planMotion(object,from,to){const nav=navMeshSettings(getLive().cameraScene),{boxes,padding}=obstacles(object);if(object.type==='Персонаж'&&nav.enabled)return findFlatPath(from,to,nav,boxes,padding);if(segmentBlocked(from,to,boxes))throw new Error('Движение перекрыто коллизией. Включите nav mesh для обхода или отключите коллизию.');return [from,to];},
  validateStep(object,from,to){if(segmentBlocked(from,to,obstacles(object).boxes))throw new Error('Движение остановлено: на пути появилась коллизия.');},
  update(){const live=getLive(),nav=navMeshSettings(live.cameraScene),next=JSON.stringify([nav.center,nav.size]);
   if(key!==next){if(plane){overlay.remove(plane);plane.geometry.dispose();plane.material.dispose();}plane=new THREE.Mesh(new THREE.PlaneGeometry(...nav.size),new THREE.MeshBasicMaterial({color:0x60cdae,transparent:true,opacity:.22,side:THREE.DoubleSide,depthWrite:false}));plane.rotation.x=-Math.PI/2;plane.position.set(nav.center[0],nav.center[1]+.015,nav.center[2]);overlay.add(plane);key=next;}
   plane.visible=live.mode==='scene'&&nav.enabled&&nav.show;
   const active=new Set();if(live.mode==='scene'&&nav.showColliders)for(const {object,box}of entries()){active.add(object.id);if(!helpers.has(object.id)){const helper=new THREE.Box3Helper(box,0xf0bc66);overlay.add(helper);helpers.set(object.id,helper);}helpers.get(object.id).box.copy(box);}
   for(const [id,helper]of helpers)if(!active.has(id)){overlay.remove(helper);helper.geometry.dispose();helper.material.dispose();helpers.delete(id);}
  },
  dispose(){scene.remove(overlay);overlay.traverse(node=>{node.geometry?.dispose();node.material?.dispose();});helpers.clear();},
 };
}
