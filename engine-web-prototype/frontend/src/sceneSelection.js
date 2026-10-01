import * as THREE from 'three';
import {cleanTransform,objectTransform} from './sceneEditing.js';

export function selectedObjectIds(selection,objects){
 if(selection?.kind!=='object')return [];
 const available=objects&&new Set(objects.map(object=>object.id));
 return [...new Set(selection.ids||[selection.id])].filter(id=>id&&(!available||available.has(id)));
}
export function selectSceneObject(selection,id,additive=false,objects){
 const current=selectedObjectIds(selection,objects);
 if(!id)return additive?selection:{kind:'object',id:null,ids:[]};
 const ids=additive?(current.includes(id)?current.filter(value=>value!==id):[...current,id]):[id];
 return {kind:'object',id:ids.at(-1)||null,ids};
}
export function matrixFromTransform(transform){
 const value=cleanTransform(transform),rotation=new THREE.Euler(...value.rotation.map(THREE.MathUtils.degToRad));
 return new THREE.Matrix4().compose(new THREE.Vector3(...value.position),new THREE.Quaternion().setFromEuler(rotation),new THREE.Vector3(...value.scale));
}
export function transformFromMatrix(matrix){
 const position=new THREE.Vector3(),quaternion=new THREE.Quaternion(),scale=new THREE.Vector3();matrix.decompose(position,quaternion,scale);
 const rotation=new THREE.Euler().setFromQuaternion(quaternion);
 return cleanTransform({position:position.toArray(),rotation:[rotation.x,rotation.y,rotation.z].map(THREE.MathUtils.radToDeg),scale:scale.toArray()});
}
// Capture world matrices once, so every drag frame is based on its start rather than accumulating error.
export function captureSelectionTransform(meshes,pivot){
 pivot.updateWorldMatrix(true,false);
 return {inversePivot:pivot.matrixWorld.clone().invert(),objects:meshes.map(mesh=>{mesh.updateWorldMatrix(true,false);return {mesh,matrix:mesh.matrixWorld.clone()};})};
}
export function applySelectionTransform(capture,pivot){
 pivot.updateWorldMatrix(true,false);const delta=new THREE.Matrix4().multiplyMatrices(pivot.matrixWorld,capture.inversePivot);
 for(const {mesh,matrix}of capture.objects){
  const local=new THREE.Matrix4().multiplyMatrices(delta,matrix);
  if(mesh.parent){mesh.parent.updateWorldMatrix(true,false);local.premultiply(mesh.parent.matrixWorld.clone().invert());}
  local.decompose(mesh.position,mesh.quaternion,mesh.scale);mesh.updateMatrixWorld(true);
 }
}
export function transformSelection(objects,scene,change){
 const transforms=objects.map(object=>({id:object.id,value:objectTransform(object,scene.id,scene.kind)}));
 if(!transforms.length)return [];
 const center=transforms.reduce((sum,item)=>sum.add(new THREE.Vector3(...item.value.position)),new THREE.Vector3()).divideScalar(transforms.length);
 const delta=new THREE.Matrix4().makeTranslation(...center.toArray()).multiply(matrixFromTransform(change)).multiply(new THREE.Matrix4().makeTranslation(...center.clone().negate().toArray()));
 return transforms.map(({id,value})=>({id,value:transformFromMatrix(delta.clone().multiply(matrixFromTransform(value)))}));
}
