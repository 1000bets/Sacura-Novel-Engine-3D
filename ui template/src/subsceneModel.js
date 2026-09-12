import {uid,allBeats} from './model.js';
import {ensureCameras} from './cameraModel.js';
import {objectTransform,isObjectInScene,BUILTIN_KINDS} from './sceneEditing.js';
import {gameCamera} from './sceneEffects.js';

export const LOCATION_TEMPLATES=[
 {id:'living',name:'Гостиная',icon:'Armchair',detail:'Комната, камин, стол и письмо',weather:'Ясно',time:'Закат',color:'#c797ad'},
 {id:'garden',name:'Сад',icon:'Trees',detail:'Дорожка, деревья и скамья с запиской',weather:'Дождь',time:'День',color:'#95b5a3'},
 {id:'station',name:'Станция',icon:'TrainFront',detail:'Платформа, поезд и билет',weather:'Ясно',time:'Рассвет',color:'#b5aecb'},
];
const KIT={
 living:[['room-table','Журнальный стол','#a58568'],['room-sofa','Диван','#9e8175'],['letter','Письмо','#f1dfb2','Активный меш'],['door','Дверь в сад','#718c87','Активный меш'],['fireplace','Камин','#8d7c74']],
 garden:[['garden-bench','Скамья','#a49076'],['garden-note','Записка на скамье','#efdfb7','Активный меш']],
 station:[['station-train','Поезд','#638c8c'],['ticket','Билет','#e6d6b5','Активный меш']],
};
export const beatsInScene=(p,id)=>p.chapters.filter(c=>c.subsceneId===id).flatMap(c=>c.beats);
const ownerOf=(p,id)=>p.chapters.find(c=>c.beats.some(b=>b.id===id))?.subsceneId;
export function sceneTransitions(p,id){
 const result=[];for(const b of allBeats(p)){const source=ownerOf(p,b.id);for(const edge of b.kind==='choice'?b.choices.map(c=>({choiceId:c.id,label:c.label,to:c.next,condition:c.condition,threshold:c.threshold})):[{to:b.next,label:'После реплики'}]){const target=ownerOf(p,edge.to);if(edge.to&&source!==target&&(source===id||target===id))result.push({...edge,from:b.id,beat:b,source,target});}}
 return result;
}
export function addSceneKit(p,scene){
 for(const [builtin,name,color,type='Декорация']of KIT[scene.kind]||[]){
  if(p.objects.some(o=>isObjectInScene(o,scene)&&(o.builtin||o.id)===builtin))continue;
  const object={id:uid('object'),builtin,name,color,type,active:true,subsceneId:scene.id,kitKind:scene.kind};
  if(type==='Активный меш')object.interaction='Осмотреть';
  object.transforms={[scene.id]:objectTransform(object,scene.id,scene.kind)};p.objects.push(object);
 }
}
export function newSceneDraft(p){const t=LOCATION_TEMPLATES[0];return {name:'',location:t.name,kind:t.id,weather:t.weather,time:t.time,description:'',firstText:'Новая история начинается здесь.',characters:p.objects.filter(o=>o.type==='Персонаж'&&!o.subsceneId&&o.active!==false).map(o=>o.id),placement:'separate',choiceId:''};}
export function createSubscene(p,draft,fromBeatId){
 if(!draft.name?.trim())throw new Error('Укажите название сабсцены.');
 const type=LOCATION_TEMPLATES.find(t=>t.id===draft.kind);if(!type)throw new Error('Выберите шаблон локации.');
 const scene={id:uid('scene'),entry:uid('line'),sceneId:p.subscenes[0]?.sceneId||'chapter1',name:draft.name.trim(),location:draft.location?.trim()||type.name,kind:type.id,weather:draft.weather,time:draft.time,description:draft.description||'',color:type.color,excludedObjectIds:p.objects.filter(o=>!o.subsceneId&&(o.type!=='Персонаж'||!draft.characters.includes(o.id))).map(o=>o.id)};
 const beat={id:scene.entry,kind:'dialogue',speaker:'Рассказчик',text:draft.firstText.trim()||'Новая история начинается здесь.',next:null,mode:'SEQUENTIAL',bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]}};
 if(draft.placement==='after'){
  const from=allBeats(p).find(b=>b.id===fromBeatId);if(!from||from.kind==='end')throw new Error('Выберите реплику или ответ, после которого появится сабсцена.');
  if(from.kind==='choice'){const choice=from.choices.find(c=>c.id===draft.choiceId);if(!choice)throw new Error('Выберите ответ, из которого будет переход.');beat.next=choice.next||null;choice.next=beat.id;}
  else {beat.next=from.next||null;from.next=beat.id;}
 }
 p.subscenes.push(scene);p.chapters.push({id:uid('chapter'),subsceneId:scene.id,name:'Первый эпизод',beats:[beat]});addSceneKit(p,scene);ensureCameras(p);return scene;
}
export function setSceneEntry(p,id,entry,reroute=false){
 const scene=p.subscenes.find(s=>s.id===id);if(!scene||!beatsInScene(p,id).some(b=>b.id===entry))throw new Error('Начальная реплика должна принадлежать этой сабсцене.');
 const old=scene.entry;scene.entry=entry;if(!reroute)return;
 for(const b of allBeats(p)){if(ownerOf(p,b.id)===id)continue;if(b.next===old)b.next=entry;for(const c of b.choices||[])if(c.next===old)c.next=entry;}
}
export function connectSubscene(p,fromId,choiceId,toSceneId){
 const from=allBeats(p).find(b=>b.id===fromId),to=p.subscenes.find(s=>s.id===toSceneId);if(!from||!to||from.kind==='end')throw new Error('Выберите реплику и сабсцену назначения.');
 if(from.kind==='choice'){const choice=from.choices.find(c=>c.id===choiceId);if(!choice)throw new Error('Выберите ответ игрока.');choice.next=to.entry;}else from.next=to.entry;
}
export function changeSceneLocation(p,id,kind){
 const scene=p.subscenes.find(s=>s.id===id);if(!scene||!KIT[kind]||scene.kind===kind)return;
 scene.kind=kind;addSceneKit(p,scene);
 const main=scene.cameras?.find(c=>c.id===scene.defaultCameraId);if(main){const [position,target]=gameCamera(kind,{camera:'Общий план'});Object.assign(main,{position,target});}
}
export function cloneSubscene(p,sourceId){
 const source=p.subscenes.find(s=>s.id===sourceId);if(!source)throw new Error('Сабсцена не найдена.');
 const id=uid('scene'),chapters=structuredClone(p.chapters.filter(c=>c.subsceneId===sourceId)),beats=chapters.flatMap(c=>c.beats),beatIds=new Map(beats.map(b=>[b.id,uid('line')])),choiceIds=new Map(beats.flatMap(b=>(b.choices||[]).map(c=>[c.id,uid('choice')]))),objectIds=new Map(),cameraIds=new Map((source.cameras||[]).map(c=>[c.id,uid('camera')]));
 const scene={...structuredClone(source),id,name:source.name+' · копия',entry:beatIds.get(source.entry),excludedObjectIds:[...(source.excludedObjectIds||[])]};
 for(const o of [...p.objects]){
  if(o.subsceneId===sourceId||(!o.subsceneId&&o.type!=='Персонаж'&&isObjectInScene(o,source))){
   const copy={...structuredClone(o),id:uid('object'),subsceneId:id,builtin:o.builtin||(BUILTIN_KINDS[o.id]?o.id:undefined),transforms:{[id]:objectTransform(o,sourceId,source.kind)}};objectIds.set(o.id,copy.id);p.objects.push(copy);
  }else if(!o.subsceneId&&o.type==='Персонаж'){o.transforms||={};o.transforms[id]=objectTransform(o,sourceId,source.kind);}
 }
 scene.cameras=(scene.cameras||[]).map(c=>({...c,id:cameraIds.get(c.id),followTargetId:objectIds.get(c.followTargetId)||c.followTargetId}));scene.defaultCameraId=cameraIds.get(source.defaultCameraId)||null;
 scene.excludedObjectIds=scene.excludedObjectIds.map(id=>objectIds.get(id)||id);
 // Global originals must stay excluded even though their local copies use new IDs.
 scene.excludedObjectIds.push(...[...objectIds.keys()].filter(id=>p.objects.find(o=>o.id===id)&&!p.objects.find(o=>o.id===id).subsceneId));
 for(const ch of chapters){ch.id=uid('chapter');ch.subsceneId=id;for(const b of ch.beats){
  b.id=beatIds.get(b.id);b.next=beatIds.get(b.next)||b.next;if(b.branch)b.branch=choiceIds.get(b.branch);if(b.signal)b.signal=objectIds.get(b.signal)||b.signal;
  for(const c of b.choices||[]){c.id=choiceIds.get(c.id);c.next=beatIds.get(c.next)||c.next;}
  const bindingIds=new Map();for(const binding of b.bindings){const oldId=binding.id;binding.id=uid('binding');bindingIds.set(oldId,binding.id);const event=p.events.find(e=>e.id===binding.eventId);binding.actionOverrides||={};
   for(const action of event?.groups.flatMap(g=>g.actions)||[]){const effective={...action,...(event.groups[0]?.actions[0]?.id===action.id?binding.overrides:{}),...binding.actionOverrides[action.id]},patch={};if(objectIds.has(effective.target))patch.target=objectIds.get(effective.target);if(cameraIds.has(effective.cameraId))patch.cameraId=cameraIds.get(effective.cameraId);if(Object.keys(patch).length)binding.actionOverrides[action.id]={...binding.actionOverrides[action.id],...patch};}
  }
  for(const groups of Object.values(b.batches||{}))for(const g of groups){g.id=uid('batch');g.bindingIds=g.bindingIds.map(id=>bindingIds.get(id)).filter(Boolean);}
 }}
 scene.excludedObjectIds=[...new Set(scene.excludedObjectIds)];p.subscenes.push(scene);p.chapters.push(...chapters);return scene;
}
