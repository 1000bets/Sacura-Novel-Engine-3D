import {persistModel} from './serverStorage.js';
import * as THREE from 'three';
import {OBJLoader} from 'three/addons/loaders/OBJLoader.js';
import {parseCharacterGlb,loadModelBuffer,disposeCharacterAsset} from './characterAssets.js';

export const MAX_MESH_BYTES=3*1024*1024;
export async function parseMeshAsset(model){
 if(model.format==='obj'){
  let text=model.src;if(text.startsWith('/api/meshes/')){const response=await fetch(text,{credentials:'same-origin'});if(!response.ok)throw new Error('Не удалось загрузить OBJ.');text=await response.text();}
  return new OBJLoader().parse(text);
 }
 return (await parseCharacterGlb(await loadModelBuffer(model.src))).scene;
}
export function materialSlots(root){
 const materials=new Map();
 root.traverse(node=>{if(!node.isMesh)return;for(const material of [].concat(node.material||[]))if(!materials.has(material))materials.set(material,{id:String(materials.size),name:material.name||`Материал ${materials.size+1}`});});
 return {materials,slots:[...materials.values()]};
}
export async function importMeshFile(file){
 const format=file.name.split('.').at(-1).toLowerCase();
 if(!['glb','obj'].includes(format))throw new Error('Выберите файл GLB или OBJ.');
 if(file.size>MAX_MESH_BYTES)throw new Error('Максимальный размер модели — 3 МБ.');
 const src=format==='obj'?await file.text():await new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result);reader.onerror=()=>reject(new Error('Не удалось прочитать модель.'));reader.readAsDataURL(file);});
 const model={format,src,name:file.name,bytes:file.size},root=await parseMeshAsset(model);
 try{
  let vertices=0;root.traverse(node=>{if(node.isMesh)vertices+=node.geometry?.attributes.position?.count||0;});
  if(!vertices)throw new Error('В файле нет геометрии меша.');
  const bounds=new THREE.Box3().setFromObject(root);
  if(![...bounds.min.toArray(),...bounds.max.toArray()].every(Number.isFinite))throw new Error('Модель содержит некорректные координаты.');
  return await persistModel({...model,slots:materialSlots(root).slots});
 }finally{disposeCharacterAsset(root);}
}
