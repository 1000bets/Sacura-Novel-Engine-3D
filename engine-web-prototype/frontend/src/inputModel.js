import {t} from './i18n.js';
import {runtimeVariableType} from './variableModel.js';
export const INPUT_TYPES={boolean:'Кнопка',axis1d:'Ось 1D',axis2d:'Оси 2D'};
export const INPUT_TRIGGERS={pressed:'Нажатие',released:'Отпускание',hold:'Удержание',continuous:'Пока активно'};
export const INPUT_BEHAVIORS={event:'Событие движка',move:'Движение персонажа',interact:'Осмотреть предмет',point:'Идти / осмотреть по указателю',advance:'Продолжить диалог',pause:'Пауза / продолжить',choiceNext:'Следующий ответ',choicePrevious:'Предыдущий ответ'};
export const INPUT_DEVICES={keyboard:'Клавиатура',pointer:'Мышь / касание сцены', 'gamepad-axis':'Ось геймпада','gamepad-button':'Кнопка геймпада',touch:'Экранное управление',external:'Другой источник'};
export function defaultInputSettings(){
 const action=(id,name,valueType,behavior,trigger='pressed')=>({id,name,valueType,behavior,trigger,holdSeconds:.4,consume:true});
 const key=(actionId,control,scale)=>({actionId,device:'keyboard',control,scale});
 const binding=(actionId,device,control,scale=1)=>({actionId,device,control,scale,deadZone:device==='gamepad-axis'?.18:0});
 const contexts=[
  {id:'system',name:'Система',priority:100,bindings:[key('pause','Escape',1),binding('pause','gamepad-button','9'),binding('pause','touch','pause')]},
  {id:'dialogue',name:'Диалог',priority:10,bindings:[key('advance','Enter',1),key('advance','Space',1),binding('advance','gamepad-button','0'),binding('advance','touch','advance'),key('choiceNext','ArrowDown',1),key('choicePrevious','ArrowUp',1),binding('choiceNext','gamepad-button','13'),binding('choicePrevious','gamepad-button','12')]},
  {id:'gameplay',name:'Ходьба и взаимодействие',priority:20,bindings:[...['KeyW','ArrowUp'].map(c=>key('move',c,[0,1])),...['KeyS','ArrowDown'].map(c=>key('move',c,[0,-1])),...['KeyA','ArrowLeft'].map(c=>key('move',c,[-1,0])),...['KeyD','ArrowRight'].map(c=>key('move',c,[1,0])),key('interact','KeyE',1),binding('move','gamepad-axis','0',[1,0]),binding('move','gamepad-axis','1',[0,-1]),...[[12,[0,1]],[13,[0,-1]],[14,[-1,0]],[15,[1,0]]].map(([c,s])=>binding('move','gamepad-button',String(c),s)),binding('interact','gamepad-button','0'),binding('point','pointer','primary'),binding('move','touch','move',[1,1]),binding('interact','touch','interact')]},
 ];
 for(const c of contexts)c.bindings.forEach((b,i)=>b.id=c.id+'-'+i);
 return {version:1,touchMode:'auto',actions:[action('move','Двигаться','axis2d','move','continuous'),action('interact','Осмотреть','boolean','interact'),action('point','Идти по указателю','boolean','point'),action('advance','Дальше','boolean','advance'),action('pause','Пауза','boolean','pause'),action('choiceNext','Следующий ответ','boolean','choiceNext'),action('choicePrevious','Предыдущий ответ','boolean','choicePrevious')],contexts};
}
export function ensureInputSettings(project){project.input??=defaultInputSettings();return project;}
export function inputContextsFor(project,beat){
 return [...new Set(['system',...(beat?.inputContexts??(beat?.kind==='gate'?['gameplay']:['dialogue']))])].filter(id=>project.input?.contexts?.some(c=>c.id===id));
}
export function inputHelp(settings,contexts,behaviors){
 if(!settings)return '';
 const defaults=defaultInputSettings();
 const names={ArrowUp:'↑',ArrowDown:'↓',ArrowLeft:'←',ArrowRight:'→',Space:t('Пробел'),Enter:'Enter',Escape:'Esc'};
 const bindings=settings.contexts.filter(c=>contexts.includes(c.id)).flatMap(c=>c.bindings);
 return settings.actions.filter(a=>behaviors.includes(a.behavior)).map(a=>{
  const rows=bindings.filter(b=>b.actionId===a.id),keys=[...new Set(rows.filter(b=>b.device==='keyboard').map(b=>names[b.control]||b.control.replace(/^Key|^Digit/,'')))];
  const sources=[keys.join('/'),rows.some(b=>b.device.startsWith('gamepad'))?t('геймпад'):'',rows.some(b=>b.device==='touch')?t('экран'):'',rows.some(b=>b.device==='pointer')?t('клик / касание'):''].filter(Boolean);
  const name=defaults.actions.find(d=>d.id===a.id)?.name===a.name?t(a.name):a.name;
  return sources.length?name+': '+sources.join(', '):null;
 }).filter(Boolean).join(' · ');
}
export function validateInputSettings(project){
 const s=project.input;if(s===undefined)return [];
 const issues=[],bad=(id,detail,beatId)=>issues.push({id:'input-'+id,level:'error',title:'Проверьте настройки ввода',detail,beatId});
 if(!s||s.version!==1||!Array.isArray(s.actions)||!Array.isArray(s.contexts)){bad('shape','Повреждена библиотека действий или контекстов.');return issues;}
 if(!['auto','always','off'].includes(s.touchMode))bad('touch','Неизвестный режим экранного управления.');
 const ids=new Set(),contexts=new Set();
 for(const a of s.actions){if(!a||typeof a.id!=='string'||!a.id||ids.has(a.id)){bad('action-id','ID действия должен быть уникальным.');continue;}ids.add(a.id);
  if(!INPUT_TYPES[a.valueType]||!INPUT_TRIGGERS[a.trigger]||!INPUT_BEHAVIORS[a.behavior])bad(a.id,'Неизвестный тип, обработчик или триггер действия.');
  if(typeof a.name!=='string'||!Number.isFinite(a.holdSeconds)||a.holdSeconds<0)bad(a.id+'-hold','Укажите имя и неотрицательное время удержания.');
  if(a.behavior==='event'&&a.eventId&&!project.events.some(e=>e.id===a.eventId))bad(a.id+'-event','Выберите событие для действия «'+a.name+'».');
  if(a.behavior==='move'&&a.valueType!=='axis2d')bad(a.id+'-type','Движение использует оси 2D.');
  for(const field of a.valueType==='axis2d'?['variableX','variableY']:['variable'])if(a[field]&&runtimeVariableType(project,a[field])!==(a.valueType==='boolean'?'boolean':'number'))bad(a.id+'-'+field,'Переменная значения действия «'+a.name+'» отсутствует или имеет другой тип.');
 }
 for(const c of s.contexts){if(!c||typeof c.id!=='string'||!c.id||contexts.has(c.id)){bad('context-id','ID контекста должен быть уникальным.');continue;}contexts.add(c.id);
  if(!Number.isFinite(c.priority)||typeof c.name!=='string'||!Array.isArray(c.bindings)){bad(c.id,'Повреждены параметры контекста.');continue;}
  const bindings=new Set();for(const b of c.bindings){if(!b||typeof b.id!=='string'||!b.id||bindings.has(b.id)||!ids.has(b.actionId)||typeof b.device!=='string'||!b.device||typeof b.control!=='string'||!b.control){bad(c.id+'-binding','Укажите действие, источник и код привязки.');continue;}bindings.add(b.id);
   const scale=Array.isArray(b.scale)?b.scale:[b.scale];if(![1,2].includes(scale.length)||scale.some(v=>!Number.isFinite(v))||!Number.isFinite(b.deadZone??0)||(b.deadZone??0)<0||(b.deadZone??0)>=1)bad(b.id,'Некорректный масштаб или мёртвая зона.');
  }
 }
 for(const beat of project.chapters?.flatMap(c=>c.beats)||[])if(beat.inputContexts!==undefined&&(!Array.isArray(beat.inputContexts)||beat.inputContexts.some(id=>!contexts.has(id))))bad(beat.id,'Контекст ввода удалён или повреждён.',beat.id);
 return issues;
}
