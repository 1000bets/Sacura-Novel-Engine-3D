import * as THREE from 'three';

// Right-clicking a prop selects it; an existing multi-selection stays intact.
export function openSceneContext(point,camera,canvas,pickables,live){
 const rect=canvas.getBoundingClientRect(),ray=new THREE.Raycaster();
 ray.setFromCamera(new THREE.Vector2((point.x-rect.left)/rect.width*2-1,-(point.y-rect.top)/rect.height*2+1),camera);
 let hit=ray.intersectObjects(pickables,true).find(hit=>{for(let node=hit.object;node;node=node.parent)if(!node.visible)return false;return true;})?.object;
 while(hit&&!hit.userData.id)hit=hit.parent;
 if(hit?.userData.id&&!(live.selectedIds||[live.selected]).includes(hit.userData.id))live.onSelect?.(hit.userData.id);
 const world=Math.abs(ray.ray.direction.y)>1e-6?ray.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0,1,0),0),new THREE.Vector3()):null;
 live.onContext?.({...point,...(world?{worldPosition:world.toArray()}:{})});
}
