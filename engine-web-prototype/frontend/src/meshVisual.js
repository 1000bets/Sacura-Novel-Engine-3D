import * as THREE from 'three';
import {meshGeometry} from './SceneGizmo.js';
import {parseMeshAsset,materialSlots} from './meshAssets.js';
import {disposeCharacterAsset} from './characterAssets.js';

export const MATERIAL_PRESETS={
 matte:{name:'Матовый',color:'#bb99aa',roughness:.85,metalness:0},
 metal:{name:'Металл',color:'#b8bec8',roughness:.25,metalness:1},
 plastic:{name:'Пластик',color:'#c8adbc',roughness:.3,metalness:0},
 stone:{name:'Камень',color:'#92918b',roughness:1,metalness:0},
};
export function createMeshObject(object){
 const group=new THREE.Group(),placeholder=new THREE.Mesh(meshGeometry(object.primitive),new THREE.MeshStandardMaterial({color:object.color||'#bb99aa',roughness:.8}));
 placeholder.castShadow=true;placeholder.receiveShadow=true;group.add(placeholder);group.userData.id=object.id;group.userData.placeholder=placeholder;group.userData.primitive=object.primitive||'box';return group;
}
export function applyMaterialAssignments(root,object){
 let state=root.userData.materialAssignments;
 const key=JSON.stringify([object.materialOverrides||{},object.model?null:object.color]);
 if(state?.key===key)return;
 if(!state){const {materials}=materialSlots(root);state={entries:[],created:[]};root.traverse(node=>{if(node.isMesh)state.entries.push({node,original:node.material,ids:[].concat(node.material||[]).map(m=>materials.get(m)?.id)});});root.userData.materialAssignments=state;}
 for(const material of state.created)material.dispose();state.created=[];
 for(const entry of state.entries){
  const assigned=[].concat(entry.original||[]).map((original,index)=>{
   const config=object.materialOverrides?.[entry.ids[index]]??object.materialOverrides?.all;
   if(!config){if(!object.model&&original.color&&object.color&&root.userData.placeholder===entry.node)original.color.set(object.color);return original;}
   const unit=value=>Math.max(0,Math.min(1,Number(value)||0));
   const material=new THREE.MeshStandardMaterial({color:config.color||'#bb99aa',roughness:unit(config.roughness),metalness:unit(config.metalness),side:config.doubleSided?THREE.DoubleSide:THREE.FrontSide});state.created.push(material);return material;
  });entry.node.material=Array.isArray(entry.original)?assigned:assigned[0];
 }
 state.key=key;
}
export function disposeMaterialAssignments(root){
 const state=root?.userData.materialAssignments;if(!state)return;
 for(const entry of state.entries)entry.node.material=entry.original;
 for(const material of state.created)material.dispose();delete root.userData.materialAssignments;
}
export function updateMeshVisual(host,object){
 if(!host.userData.placeholder)return;
 const source=object.model?.src||null;
 let visual=host.userData.meshVisual;
 if(!visual||visual.source!==source){
  disposeMeshVisual(host);visual={source,active:true};host.userData.meshVisual=visual;
  if(source)parseMeshAsset(object.model).then(root=>{
   if(!visual.active){disposeCharacterAsset(root);return;}
   root.traverse(node=>{if(node.isMesh){node.castShadow=true;node.receiveShadow=true;}});
   host.userData.placeholder.visible=false;host.add(root);visual.root=root;
  }).catch(error=>{if(visual.active){visual.error=error.message;host.userData.meshError=error.message;}});
 }
 applyMaterialAssignments(visual.root||host,object);
}
export function disposeMeshVisual(host){
 const visual=host.userData.meshVisual;
 if(visual){visual.active=false;if(visual.root){disposeMaterialAssignments(visual.root);host.remove(visual.root);disposeCharacterAsset(visual.root);}}
 disposeMaterialAssignments(host);if(host.userData.placeholder)host.userData.placeholder.visible=true;
 delete host.userData.meshVisual;delete host.userData.meshError;
}
