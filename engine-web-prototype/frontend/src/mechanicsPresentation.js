import {mechanicDefinition} from './mechanicsModel.js';
import {GAME_TRIGGERS} from './gameplayModel.js';
import {operationDefinition} from './operationRegistry.js';
import {signalDefinition} from './signalModel.js';
export const searchable=value=>String(value||'').toLocaleLowerCase('ru').replaceAll('ё','е');
export function ownerLabel(project,scope){if(scope?.kind==='project')return 'Проект · '+project.title;const record=(scope?.kind==='object'?project.objects:project.subscenes).find(x=>x.id===scope?.id);return (scope?.kind==='object'?'Объект':'Сцена')+' · '+(record?.name||'Удалённый владелец');}
export function mechanicLabel(project,m){const record=({resource:project.gameplay.resources,pickup:project.gameplay.items,weapon:project.gameplay.weapons}[m.type]||[]).find(x=>x.id===m.recordId);return m.name||record?.name||mechanicDefinition(m.definitionId||m.capabilityId||m.type,project,m.definitionVersion)?.name||'Недоступная механика';}
export function ruleSummary(project,rule){
 const signal=rule.signal?.type||rule.trigger,name=signalDefinition(project,signal)?.name||GAME_TRIGGERS[signal]||'Недоступный сигнал';
 const actions=rule.actions||[rule.operation&&{kind:'operation',call:rule.operation},rule.functionId&&{kind:'function',functionId:rule.functionId},rule.eventId&&{kind:'event',eventId:rule.eventId},rule.nextBeatId&&{kind:'story',beatId:rule.nextBeatId}].filter(Boolean);
 const label=a=>a.kind==='operation'?operationDefinition(a.call?.id,a.call?.version)?.editor.label||'Недоступная операция':a.kind==='event'?project.events.find(e=>e.id===a.eventId)?.name||'Недоступное событие':a.kind==='function'?project.functions.find(f=>f.id===a.functionId)?.name||'Недоступная функция':a.kind==='story'?'Диалог / история':'Действие';
 return name+' → '+(actions.length?actions.slice(0,2).map(label).join(' → ')+(actions.length>2?' · ещё '+(actions.length-2):''):'добавьте действие')+(rule.condition||rule.conditionFunctionId||rule.conditionOperation?' · с условием':'');
}
export function mechanicSource(project,definition){return definition.packageSource||project.gameplay.mechanics.find(m=>m.definitionId===definition.id.split('@')[0]&&m.packageSource)?.packageSource;}
