import {validateGameplay} from './gameplayModel.js';
import {validateFunctions} from './blueprintFunctions.js';
import {validateInputSettings} from './inputModel.js';
import {validateWidgets} from './widgetModel.js';
import {upgradeProject,allBeats,TYPES,validateStudio} from './studioModel.js';

export function readProject(raw){
 const p=typeof raw==='string'?JSON.parse(raw):raw;
 const fail=message=>{throw new Error('Проект не загружен: '+message);};
 if(!p||![1,2].includes(p.version)||!Array.isArray(p.events)||!Array.isArray(p.objects)||!Array.isArray(p.chapters))fail('неподдерживаемый формат.');
 if(!p.chapters.length||p.chapters.some(c=>!Array.isArray(c.beats))||!p.chapters.some(c=>c.beats.length))fail('в сценарии должна быть хотя бы одна реплика.');
 const unique=(items,name)=>{if(items.some(x=>!x||typeof x.id!=='string'||!x.id)||new Set(items.map(x=>x.id)).size!==items.length)fail(name+': некорректные или повторяющиеся идентификаторы.');};
 unique(p.chapters,'Эпизоды');unique(allBeats(p),'Реплики');unique(p.objects,'Объекты');unique(p.events,'События');
 for(const b of allBeats(p))if(!Array.isArray(b.bindings)||typeof b.text!=='string'||(b.kind==='choice'&&!Array.isArray(b.choices)))fail('повреждён блок сценария '+b.id+'.');
 const actions=[];
 for(const e of p.events){if(!Array.isArray(e.groups)||e.groups.some(g=>!Array.isArray(g.actions)))fail('повреждено событие '+e.id+'.');actions.push(...e.groups.flatMap(g=>g.actions));}
 for(const a of actions)if(!TYPES[a.type])fail('неизвестный тип действия '+a.type+'.');
 if(p.version===2){
  if(!Array.isArray(p.subscenes)||!p.subscenes.length)fail('нет сабсцен.');unique(p.subscenes,'Сабсцены');
  if(p.chapters.some(c=>!p.subscenes.some(s=>s.id===c.subsceneId)))fail('эпизод ссылается на отсутствующую сабсцену.');
  for(const s of p.subscenes)if(!p.chapters.filter(c=>c.subsceneId===s.id).flatMap(c=>c.beats).some(b=>b.id===s.entry))fail('неверное начало сабсцены «'+s.name+'».');
 }
 if(p.widgets!==undefined){if(!Array.isArray(p.widgets))fail('повреждена библиотека виджетов.');const issues=validateWidgets(p);if(issues.length)fail(issues[0].title+': '+issues[0].detail);}
 const inputIssues=validateInputSettings(p);if(inputIssues.length)fail(inputIssues[0].detail);
 if(p.gameplay!==undefined){const issues=validateGameplay(p);if(issues.some(i=>i.id==='game-shape'))fail(issues[0].detail);}
 if(p.functions!==undefined&&(!Array.isArray(p.functions)||validateFunctions(p).some(i=>i.detail==='Функции нужны параметры, ноды и точка входа.')))fail('повреждена библиотека функций.');
 const next=upgradeProject(p);
 // Exercise the data shape before the live editor receives it.
 if(!allBeats(next).length)fail('нет реплик.');
 try{validateStudio(next);}catch{fail('повреждены параметры постановки реплик.');}
 return next;
}
