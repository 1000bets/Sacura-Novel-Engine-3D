import {uid} from './model.js';
import {createSubscene,LOCATION_TEMPLATES} from './subsceneModel.js';
import {isObjectInScene,objectTransform,BUILTIN_KINDS} from './sceneEditing.js';
export function exportLocation(project,sceneId){
 const scene=project.subscenes.find(s=>s.id===sceneId);if(!scene)throw new Error('Локация не найдена.');
 return {format:'sacura-location',version:1,scene:structuredClone(scene),objects:project.objects.filter(o=>isObjectInScene(o,scene)).map(o=>({...structuredClone(o),transforms:{[scene.id]:objectTransform(o,scene.id,scene.kind)}}))};
}
export function readLocation(raw){
 const value=typeof raw==='string'?JSON.parse(raw):structuredClone(raw);
 const vector=v=>Array.isArray(v)&&v.length===3&&v.every(Number.isFinite);
 if(value?.format!=='sacura-location'||value.version!==1||!value.scene||!LOCATION_TEMPLATES.some(t=>t.id===value.scene.kind)||!Array.isArray(value.objects))throw new Error('Неподдерживаемый файл локации.');
 const s=value.scene;
 if(typeof s.id!=='string'||typeof s.name!=='string'||!s.name.trim()||new Set(value.objects.map(o=>o.id)).size!==value.objects.length)throw new Error('Повреждён файл локации.');
 for(const o of value.objects){const t=o?.transforms?.[s.id];if(typeof o?.id!=='string'||!o.id||typeof o.name!=='string'||!t||!vector(t.position)||!vector(t.rotation)||!vector(t.scale))throw new Error('Повреждены объекты локации.');}
 if(!Array.isArray(s.cameras)||s.cameras.some(c=>typeof c.id!=='string'||!vector(c.position)||!vector(c.target)))throw new Error('Повреждены камеры локации.');
 if(!Array.isArray(s.stagingPoints)||s.stagingPoints.some(p=>typeof p.id!=='string'||!vector(p.position)))throw new Error('Повреждены точки локации.');
 return value;
}
export function importLocation(project,raw){
 const data=readLocation(raw),source=data.scene;
 const scene=createSubscene(project,{name:source.name,location:source.location,kind:source.kind,weather:source.weather,time:source.time,characters:[],firstText:'Новая история начинается здесь.',placement:'separate'});
 project.objects=project.objects.filter(o=>o.subsceneId!==scene.id);
 const ids=new Map(data.objects.map(o=>[o.id,uid('object')])),cameraIds=new Map(source.cameras.map(c=>[c.id,uid('camera')]));
 Object.assign(scene,{description:source.description||'',...(source.navMesh?{navMesh:structuredClone(source.navMesh)}:{}),excludedObjectIds:project.objects.filter(o=>!o.subsceneId).map(o=>o.id),
 cameras:source.cameras.map(c=>({...structuredClone(c),id:cameraIds.get(c.id),followTargetId:ids.get(c.followTargetId)||null})),defaultCameraId:cameraIds.get(source.defaultCameraId)||null,
 stagingPoints:source.stagingPoints.map((p,i)=>({...structuredClone(p),id:`${scene.id}:point:${i}`,objectId:ids.get(p.objectId)||null}))});
 for(const o of data.objects)project.objects.push({...structuredClone(o),id:ids.get(o.id),...((o.builtin||BUILTIN_KINDS[o.id])?{builtin:o.builtin||o.id}:{}),subsceneId:scene.id,transforms:{[scene.id]:structuredClone(o.transforms[source.id])}});
 return scene;
}
