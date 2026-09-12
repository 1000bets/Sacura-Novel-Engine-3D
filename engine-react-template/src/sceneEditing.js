import {DECORATION_CATALOG,DECORATION_TRANSFORMS,DECORATION_KINDS} from './sceneDecorations.js';
import {uid} from './model.js';

// Keep legacy action values and authored character positions readable in old projects.
export const ANCHORS={
 living:{камин:[-1.95,0,-.4],окно:[1.15,0,-1.85],стол:[-.9,0,1],диван:[2.7,0,-.4],вход:[3.1,0,-2.3]},
 garden:{камин:[-1.8,0,-1],окно:[1,0,-1.4],стол:[1,0,.3],диван:[2.4,0,-.8],вход:[-.8,0,2.2]},
 station:{камин:[-2,0,-.4],окно:[.8,0,-.9],стол:[.8,0,.6],диван:[2.3,0,-.1],вход:[-2.6,0,1.5]},
};
export const BUILTIN_TRANSFORMS={...DECORATION_TRANSFORMS,letter:[-.35,.782,1.55],door:[3.05,1.13,-3.08],fireplace:[-3.1,0,-2.6],'room-table':[-.25,0,1.7],'room-sofa':[2.8,0,.6],'garden-bench':[2,0,.5],'station-train':[0,0,-2],'garden-note':[2,.77,.4],ticket:[1.4,.83,1.2]};
export const BUILTIN_KINDS={...DECORATION_KINDS,letter:'living',door:'living',fireplace:'living','room-table':'living','room-sofa':'living','garden-bench':'garden','garden-note':'garden','station-train':'station',ticket:'station'};
const STAGING_TEMPLATES={
 living:[['fireplace','У камина','камин','fireplace'],['window','У окна','окно','room-window'],['table','У стола','стол','room-table'],['sofa','У дивана','диван','room-sofa'],['entrance','У двери в сад','вход','door']],
 garden:[['path','На садовой дорожке','камин','garden-path'],['tree','У старого дерева','окно','garden-tree-2'],['bench','У скамьи','стол','garden-bench'],['note','У записки на скамье','диван','garden-note'],['entrance','У входа в сад','вход','garden-fence']],
 station:[['platform','На платформе','камин','station-platform'],['train','У вагона','окно','station-train'],['ticket','У билета','стол','ticket'],['luggage','У багажа','диван','station-luggage'],['entrance','У входа на станцию','вход','station-canopy']],
};
export function createSceneStagingPoints(scene,objects=[]){
 return (STAGING_TEMPLATES[scene.kind]||STAGING_TEMPLATES.living).map(([key,label,alias,builtin])=>{
  const object=objects.find(o=>isObjectInScene(o,scene)&&(o.builtin||o.id)===builtin),position=[...(ANCHORS[scene.kind]||ANCHORS.living)[alias]];
  return {id:`${scene.id}:point:${key}`,key,label,aliases:[alias],position,...(object?{objectId:object.id,objectOrigin:[...(BUILTIN_TRANSFORMS[object.builtin||object.id]||objectTransform(object,scene.id,scene.kind).position)]}:{} )};
 });
}
export function ensureSceneStagingPoints(scene,objects=[]){
 if(!Array.isArray(scene.stagingPoints))scene.stagingPoints=createSceneStagingPoints(scene,objects);
 return scene.stagingPoints;
}
// Points belong to a subscene. Attached points follow the displacement of their prop.
export function sceneStagingPoints(scene,objects=[]){
 if(!scene)return [];
 return (scene.stagingPoints||createSceneStagingPoints(scene,objects)).map(point=>{
  const position=cleanTransform({position:point.position}).position,object=objects.find(o=>o.id===point.objectId&&isObjectInScene(o,scene));
  if(object&&point.objectOrigin){const current=objectTransform(object,scene.id,scene.kind).position;for(let i=0;i<3;i++)position[i]+=current[i]-point.objectOrigin[i];}
  return {...point,position,objectId:object?.id};
 });
}
export function findStagingPoint(points,value){return points?.find(point=>point.id===value||point.key===value||point.aliases?.includes(value));}
export function stagingPointOptions(scene,objects,value){
 const points=sceneStagingPoints(scene,objects),options=points.map(point=>[point.id,point.label]);
 if(value!=null&&!options.some(([id])=>id===value)){
  const point=findStagingPoint(points,value),coordinate=Array.isArray(value),stored=coordinate?value.join(', '):value;
  options.unshift([stored,point?.label||(coordinate?`Координаты: ${stored}`:String(value).includes(':point:')?'Точка недоступна в этой сабсцене · выберите другую':`Сохранённая точка: ${value}`)]);
 }
 return options;
}
export function isObjectInScene(o,scene){return (!o.subsceneId||o.subsceneId===scene.id)&&!scene.excludedObjectIds?.includes(o.id)&&(!o.kitKind||o.kitKind===scene.kind)&&(!BUILTIN_KINDS[o.builtin||o.id]||BUILTIN_KINDS[o.builtin||o.id]===scene.kind);}
export function cleanTransform(t){
 const vector=(key,fallback)=>fallback.map((v,i)=>Number.isFinite(Number(t?.[key]?.[i]))?Number(t[key][i]):v);
 return {position:vector('position',[0,0,0]),rotation:vector('rotation',[0,0,0]),scale:vector('scale',[1,1,1]).map(v=>Math.max(.05,Math.min(30,v)))};
}
export function objectTransform(o,sceneId,kind='living'){
 const base=BUILTIN_TRANSFORMS[o.builtin||o.id];
 return cleanTransform(o.transforms?.[sceneId]||{position:base||((ANCHORS[kind]||ANCHORS.living)[o.position]||[0,0,0]).map((v,i)=>v+(i===1&&o.type!=='Персонаж'?.25:0)),rotation:[0,(o.builtin||o.id)==='letter'?10.313:0,0],scale:[1,1,1]});
}
export function setObjectTransform(p,id,sceneId,transform){const o=p.objects.find(o=>o.id===id);if(!o)return;o.transforms||={};o.transforms[sceneId]=cleanTransform(transform);}
export function resolvedPosition(o,state,kind='living'){
 const anchors=ANCHORS[kind]||ANCHORS.living,base=objectTransform(o,state.location||kind,kind).position,motion=state.motions?.[o.id];
 const at=value=>{if(Array.isArray(value))return value;const point=findStagingPoint(state.stagingPoints,value),position=point?.position||anchors[value];return position?position.map((v,i)=>i===1&&o.type!=='Персонаж'?base[1]:v):base;};
 if(motion){const from=at(motion.from),to=at(motion.to),t=motion.progress||0;if(t>=1)return [...to];return from.map((v,i)=>v+(to[i]-v)*t);}
 return state.positions?.[o.id]?at(state.positions[o.id]):base;
}
export function ensureSceneEditing(p){
 if(!p.sceneEditingVersion)for(const [id,name,kind,color]of [['room-table','Журнальный стол','living','#a58568'],['room-sofa','Диван','living','#9e8175'],['garden-bench','Скамья','garden','#a49076'],['station-train','Поезд','station','#638c8c']]){
  const scene=p.subscenes.find(s=>s.kind===kind);if(scene&&!p.objects.some(o=>o.id===id))p.objects.push({id,builtin:id,name,type:'Декорация',active:true,color,subsceneId:scene.id});
 }
 if((p.sceneEditingVersion||0)<2)for(const scene of p.subscenes)addSceneDecorations(p,scene);
 for(const scene of p.subscenes)ensureSceneStagingPoints(scene,p.objects);
 p.sceneEditingVersion=Math.max(2,p.sceneEditingVersion||0);return p;
}
export function addSceneDecorations(p,scene){
 for(const item of DECORATION_CATALOG.filter(item=>item.kind===scene.kind)){
  if(p.objects.some(o=>isObjectInScene(o,scene)&&(o.builtin||o.id)===item.id))continue;
  const id=p.objects.some(o=>o.id===item.id)?uid('object'):item.id;
  const object={id,builtin:item.id,name:item.name,color:item.color,type:'Декорация',active:true,subsceneId:scene.id,kitKind:scene.kind};
  object.transforms={[scene.id]:objectTransform(object,scene.id,scene.kind)};p.objects.push(object);
 }
}
