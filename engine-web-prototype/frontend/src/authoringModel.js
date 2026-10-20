import {makeAction, uid, TYPES} from './model.js';

export const LIBRARY_KEYS={action:'actionTemplates',group:'groupTemplates',event:'events'};
export function copyAction(action){return {...structuredClone(action),id:uid('action'),sourceTemplateId:action.sourceTemplateId||action.id};}
export function copyGroup(group){return {...structuredClone(group),id:uid('group'),sourceTemplateId:group.sourceTemplateId||group.id,actions:group.actions.map(copyAction)};}
export function blankAsset(kind, type='move', target){
 if(kind==='action')return {...makeAction(type,target),name:'',description:''};
 if(kind==='group')return {id:uid('group'),name:'',actions:[]};
 return {id:uid('event'),name:'',description:'',retention:'AUTO_CLOSE_ON_FLOW_END',owner:'SubScene',groups:[]};
}
export function eventFromAsset(kind,asset){
 if(kind==='event')return structuredClone(asset);
 return {...blankAsset('event'),name:asset.name,groups:kind==='group'?[copyGroup(asset)]:[{id:uid('group'),name:'Основное действие',actions:[copyAction(asset)]}],retention:(kind==='action'?[asset]:asset.actions).some(a=>TYPES[a.type]?.completion==='CONTINUOUS')?'HOLD_UNTIL_STOPPED':'AUTO_CLOSE_ON_FLOW_END'};
}
export function ensureCreationLibrary(p){
 if(!p.actionTemplates)p.actionTemplates=[['move','Подойти к окну'],['pose','Улыбнуться'],['weather','Начать дождь'],['lighting','Тёплый свет'],['sound','Озвучка Алисы']].map(([type,name])=>({...makeAction(type),name,...(type==='sound'?{assetId:'voice-alice',duck:true}:{} )}));
 if(!p.groupTemplates)p.groupTemplates=[{id:uid('group'),name:'Реакция крупным планом',actions:[makeAction('pose','alice','улыбка'),makeAction('camera','camera','Крупный план')]}];
 if(!p.standardEventsVersion){
 for(const o of p.objects.filter(o=>o.type==='Персонаж'&&!o.model)){if(Array.isArray(o.enabledAnimations))o.enabledAnimations=[...new Set([...o.enabledAnimations,'ходит','сидит','машет'])];}
 const target=p.objects.find(o=>o.type==='Персонаж')?.id||'';
 const group=(name,actions)=>({id:uid('group'),name,actions});
 const presets=[
 ['Подойти с анимацией',[group('Подойти',[{...makeAction('move',target,[0,0,0]),animationId:'ходит'}])]],
 ['Подойти и сесть',[group('Подойти к месту',[{...makeAction('move',target,[0,0,0]),animationId:'ходит'}]),group('Сесть',[makeAction('pose',target,'сидит')])]],
 ['Приветствие крупным планом',[group('Приветствие',[makeAction('pose',target,'машет'),makeAction('camera','camera','Крупный план')])]],
 ['Наступает вечер',[group('Окружение',[makeAction('time','world','Закат'),makeAction('lighting','world','Тёплый свет')])]],
 ['Появиться в кадре',[group('Появление',[makeAction('visibility',target,'Показать'),makeAction('pose',target,'стоит')])]],
 ];
 p.events||=[];for(const [name,groups] of presets)p.events.push({...blankAsset('event'),name,groups,standardPreset:true});
 p.standardEventsVersion=1;
 }
 return p;
}
export function authoringProblems(kind,asset){
 const problems=[];if(kind==='event'&&asset.retention==='HOLD_UNTIL_REPLACED'&&!asset.channel?.trim())problems.push('Укажите роль, по которой следующее событие заменит это.');if(!asset.name.trim())problems.push('Дайте название, чтобы найти это в библиотеке.');
 const groups=kind==='event'?asset.groups:kind==='group'?[asset]:[{actions:[asset]}];
 if(!groups.length||groups.some(g=>!g.actions.length))problems.push('Добавьте хотя бы одно действие в каждый шаг.');
 for(const [i,g] of groups.entries()){
  const resources=new Set();for(const a of g.actions){const t=TYPES[a.type];if(!t){problems.push('Неизвестный тип действия.');continue;}
   if(t.completion==='CONTINUOUS'&&(a.wait==='COMPLETED'||a.scope==='SELF'))problems.push('Музыка должна ждать запуска и жить до завершения события. Измените тип и выберите музыку заново для стандартных настроек.');
   const key=a.target+'/'+t.domain;if(t.domain&&a.type!=='sound'){if(resources.has(key))problems.push(`Шаг ${i+1}: два действия одновременно управляют ресурсом «${t.domain}» одного объекта. Разнесите их по шагам.`);resources.add(key);}
  }
 }
 return problems;
}
