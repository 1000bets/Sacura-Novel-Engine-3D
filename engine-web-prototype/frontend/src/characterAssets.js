import {persistModel} from './serverStorage.js';
import * as THREE from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {retargetMixamoAnimations} from './mixamoAnimations.js';

export const MAX_CHARACTER_FILE_BYTES=3*1024*1024;
export const MAX_ANIMATION_FILE_BYTES=20*1024*1024;
// Files travel with the project JSON; reject external dependencies before parsing.
export function validateCharacterGlb(buffer){
 const view=new DataView(buffer);
 if(buffer.byteLength<20||view.getUint32(0,true)!==0x46546c67||view.getUint32(4,true)!==2||view.getUint32(8,true)!==buffer.byteLength)throw new Error('Нужен корректный файл GLB версии 2.');
 const length=view.getUint32(12,true);
 if(view.getUint32(16,true)!==0x4e4f534a||20+length>buffer.byteLength)throw new Error('Повреждён файл GLB.');
 const json=JSON.parse(new TextDecoder().decode(new Uint8Array(buffer,20,length)).trim());
 if([...(json.buffers||[]),...(json.images||[])].some(resource=>resource.uri&&!resource.uri.startsWith('data:')))throw new Error('Экспортируйте GLB со встроенными текстурами и данными. Внешние файлы не поддерживаются.');
 return json;
}
export async function parseCharacterGlb(buffer){
 validateCharacterGlb(buffer);
 return new GLTFLoader().parseAsync(buffer,'');
}
export function disposeCharacterAsset(root){
 const geometries=new Set(),materials=new Set(),textures=new Set();
 root?.traverse(node=>{if(node.geometry)geometries.add(node.geometry);for(const material of Array.isArray(node.material)?node.material:node.material?[node.material]:[]){materials.add(material);for(const value of Object.values(material))if(value?.isTexture)textures.add(value);}});
 for(const texture of textures){texture.dispose();texture.source?.data?.close?.();}
 for(const material of materials)material.dispose();for(const geometry of geometries)geometry.dispose();
}
export async function parseMixamoFbx(buffer){
 const {FBXLoader}=await import('three/addons/loaders/FBXLoader.js');
 // Animation import needs no textures, including those referenced by With Skin exports.
 const manager=new THREE.LoadingManager(),textures=[];
 manager.addHandler(/.*/,{path:'',setPath(path){this.path=path;return this;},load(url){
  if(url.startsWith('blob:'))URL.revokeObjectURL(url);
  const texture=new THREE.Texture();textures.push(texture);return texture;
 }});
 try{return new FBXLoader(manager).parse(buffer,'');}
 catch{throw new Error('Не удалось прочитать FBX. Скачайте анимацию из Mixamo в формате FBX Binary, желательно Without Skin.');}
 finally{for(const texture of textures)texture.dispose();}
}
async function importMixamoFile(file,{targetModel,inPlace=true}={}){
 if(file.size>MAX_ANIMATION_FILE_BYTES)throw new Error('Максимальный размер FBX анимации — 20 МБ.');
 if(!targetModel?.src)throw new Error('Сначала загрузите GLB-модель humanoid-персонажа со скелетом.');
 const source=await parseMixamoFbx(await file.arrayBuffer());let target;
 try{
  const clips=(source.animations||[]).filter(clip=>clip.tracks.length);
  if(!clips.length)throw new Error('В FBX нет анимаций. Скачайте движение из Mixamo, а не только модель.');
  target=await parseCharacterGlb(await loadModelBuffer(targetModel.src));
  let skinned=false;target.scene.traverse(node=>{if(node.isSkinnedMesh)skinned=true;});
  if(!skinned)throw new Error('У модели персонажа нет skinned mesh. Для Mixamo нужна GLB-модель со скелетом.');
  return retargetMixamoAnimations(source,target.scene,clips,{inPlace}).map((clip,index)=>({
   id:`mixamo:${index}`,name:clips.length===1?file.name.replace(/\.fbx$/i,''):clip.name||`Анимация ${index+1}`,
   duration:clip.duration,source:'mixamo',sourceFile:file.name,inPlace,
   description:`Движение из Mixamo, перенесённое на скелет персонажа${inPlace?' без перемещения по сцене':''}. Посмотрите движение и уточните его смысл в описании карточки.`,
   clip:THREE.AnimationClip.toJSON(clip),
  }));
 }finally{disposeCharacterAsset(source);if(target)disposeCharacterAsset(target.scene);}
}
export async function importCharacterFile(file,{animationsOnly=false,targetModel,inPlace=true}={}){
 if(animationsOnly&&/\.fbx$/i.test(file.name))return importMixamoFile(file,{targetModel,inPlace});
 if(!/\.glb$/i.test(file.name))throw new Error(animationsOnly?'Выберите анимацию GLB или FBX из Mixamo.':'Выберите модель GLB со встроенными ресурсами.');
 if(file.size>MAX_CHARACTER_FILE_BYTES)throw new Error('Максимальный размер GLB — 3 МБ.');
 const buffer=await file.arrayBuffer(),asset=await parseCharacterGlb(buffer);
 try{
  let hasMesh=false;asset.scene.traverse(node=>{if(node.isMesh)hasMesh=true;});
  if(!animationsOnly&&!hasMesh)throw new Error('В файле нет меша персонажа.');
  if(animationsOnly&&!asset.animations.length)throw new Error('В файле нет анимаций.');
  const animations=asset.animations.map((clip,index)=>{
   const kinds=[...new Set(clip.tracks.map(track=>track.name.endsWith('.quaternion')?'повороты':track.name.endsWith('.position')?'перемещение':track.name.endsWith('.scale')?'изменение размера':track.name.includes('morphTargetInfluences')?'мимику':'движение частей модели'))];
   return {id:`model:${index}`,name:clip.name||`Анимация ${index+1}`,duration:clip.duration,description:`Содержит ${kinds.join(', ')}. Посмотрите движение и уточните его смысл в описании карточки.`,...(animationsOnly?{clip:THREE.AnimationClip.toJSON(clip)}:{})};
  });
  if(animationsOnly)return animations;
  const data=await new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result);reader.onerror=()=>reject(new Error('Не удалось прочитать файл.'));reader.readAsDataURL(file);});
  return await persistModel({name:file.name,bytes:file.size,src:data,animations});
 }finally{disposeCharacterAsset(asset.scene);}
}
export function modelBuffer(src){
 const encoded=src.split(',')[1];if(!encoded)throw new Error('Повреждены данные модели.');
 const text=atob(encoded);return Uint8Array.from(text,char=>char.charCodeAt(0)).buffer;
}

export async function loadModelBuffer(src){
 if(src.startsWith('/api/meshes/')){const response=await fetch(src,{credentials:'same-origin'});if(!response.ok)throw new Error('Не удалось загрузить модель из хранилища.');return response.arrayBuffer();}
 return modelBuffer(src);
}
