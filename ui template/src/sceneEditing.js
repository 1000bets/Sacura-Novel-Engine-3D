export const ANCHORS={
 living:{камин:[-1.95,0,-.4],окно:[1.15,0,-1.85],стол:[-.9,0,1],диван:[2.7,0,-.4],вход:[3.1,0,-2.3]},
 garden:{камин:[-1.8,0,-1],окно:[1,0,-1.4],стол:[1,0,.3],диван:[2.4,0,-.8],вход:[-.8,0,2.2]},
 station:{камин:[-2,0,-.4],окно:[.8,0,-.9],стол:[.8,0,.6],диван:[2.3,0,-.1],вход:[-2.6,0,1.5]},
};
export const BUILTIN_TRANSFORMS={letter:[-.35,.782,1.55],door:[3.05,1.13,-3.08],fireplace:[-3.1,0,-2.6],'room-table':[-.25,0,1.7],'room-sofa':[2.8,0,.6],'garden-bench':[2,0,.5],'station-train':[0,0,-2],'garden-note':[2,.77,.4],ticket:[1.4,.83,1.2]};
export const BUILTIN_KINDS={letter:'living',door:'living',fireplace:'living','room-table':'living','room-sofa':'living','garden-bench':'garden','garden-note':'garden','station-train':'station',ticket:'station'};
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
 const at=value=>Array.isArray(value)?value:anchors[value]?anchors[value].map((v,i)=>i===1&&o.type!=='Персонаж'?base[1]:v):base;
 if(motion){const from=at(motion.from),to=at(motion.to),t=motion.progress||0;return from.map((v,i)=>v+(to[i]-v)*t);}
 return state.positions?.[o.id]?at(state.positions[o.id]):base;
}
export function ensureSceneEditing(p){
 if(p.sceneEditingVersion)return p;
 for(const [id,name,kind,color]of [['room-table','Журнальный стол','living','#a58568'],['room-sofa','Диван','living','#9e8175'],['garden-bench','Скамья','garden','#a49076'],['station-train','Поезд','station','#638c8c']]){
  const scene=p.subscenes.find(s=>s.kind===kind);if(scene&&!p.objects.some(o=>o.id===id))p.objects.push({id,builtin:id,name,type:'Декорация',active:true,color,subsceneId:scene.id});
 }
 p.sceneEditingVersion=1;return p;
}
