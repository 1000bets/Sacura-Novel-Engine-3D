import {actionSummary} from './authoringPresentation.js';
import {t as tr,useLocale} from './i18n.jsx';
import React from 'react';
import ActionFields from './ActionFields.jsx';
import Section from './InspectorSection.jsx';
import {Icon,Button,Field,Select} from './StudioParts.jsx';
import {makeAction,TYPES,uid} from './model.js';
import './eventInspector.css';

export function EventLifecycleFields({event,onChange}){
 useLocale();
 return <><Field label={tr('После действий')}><Select value={event.retention} options={[["AUTO_CLOSE_ON_FLOW_END","Завершить событие"],["HOLD_UNTIL_STOPPED","До остановки"],["HOLD_UNTIL_REPLACED","До замены"]]} onChange={retention=>onChange({retention,channel:event.channel||'Effect.'+event.id})}/></Field>{event.retention==='HOLD_UNTIL_REPLACED'&&<Field label={tr('Канал замены')}><input value={event.channel||''} onChange={e=>onChange({channel:e.target.value})}/></Field>}<Field label={tr('Область события')}><Select value={event.owner} options={[["SubScene","Сабсцена"],["Scene","Сцена"],["GameSession","Игровая сессия"]]} onChange={owner=>onChange({owner})}/></Field></>;
}
export function ActionExecutionFields({action:a,onChange}){
 useLocale();
 return <>
  <Field label={tr('Если ресурс занят')}><Select value={a.conflict} options={[["FAIL_NEW","Ошибка"],["REPLACE_CURRENT","Заменить"],["QUEUE","Ожидать"]]} onChange={conflict=>onChange({conflict})}/></Field>
  <Field label={tr('Ожидание')}><Select aria-label={tr('Ожидание действия')} value={a.wait} options={[["COMPLETED","Завершения"],["STARTED","Запуска"],["NONE","Продолжить сразу"]]} onChange={wait=>onChange({wait})}/></Field>
  <Field label={tr('Область действия')}><Select value={a.scope} options={[["SELF","Действие"],["EVENT","Событие"]]} onChange={scope=>onChange({scope})}/></Field>
  <Section title={tr('Лимиты времени')} open={false}>{['queueTimeout','startTimeout','executionTimeout','stopTimeout'].map((key,i)=><Field key={key} label={tr(['Лимит очереди','Лимит запуска','Лимит выполнения','Лимит остановки'][i])+tr(', сек')}><input type="number" min="0" step="0.1" value={a[key]??0} onChange={e=>onChange({[key]:Number(e.target.value)})}/></Field>)}</Section>
  <Section title={tr('Обработка ошибки')} open={false}>{['onFailure','finalState','fallback'].map((key,i)=><Field key={key} label={tr(['При ошибке','Итоговое состояние','Резервное состояние'][i])}><input value={a[key]||''} onChange={e=>onChange({[key]:e.target.value})}/></Field>)}</Section>
 </>;
}
export default function EventInspector({event,project,sceneId,binding,onEdit,onAction,onPickPosition,showProperties=true,disabled=false,onSelectAction}){
 useLocale();const edit=fn=>{if(!disabled)onEdit(fn);},patch=value=>edit(e=>Object.assign(e,value));
 return <fieldset disabled={disabled} className="inline-event-editor">
  {showProperties&&<><Field label={tr('Название события')}><input value={event.name} onChange={e=>patch({name:e.target.value})}/></Field><Section title={tr('Время жизни события')} open={false}><EventLifecycleFields event={event} onChange={patch}/></Section><Section title={tr('Описание')} open={false}><textarea aria-label={tr('Описание события')} value={event.description||''} onChange={e=>patch({description:e.target.value})}/></Section></>}
  {event.groups.map((g,index)=><div key={g.id} className="event-group-card">
   <div className="event-group-heading"><input aria-label={tr('Название группы {0}',[index+1])} value={g.name} onChange={e=>edit(ev=>ev.groups.find(x=>x.id===g.id).name=e.target.value)}/><div className="event-icon-actions"><Button icon="ArrowUp" title={tr('Переместить группу выше')} disabled={disabled||!index} onClick={()=>edit(e=>{[e.groups[index-1],e.groups[index]]=[e.groups[index],e.groups[index-1]];})}/><Button icon="ArrowDown" title={tr('Переместить группу ниже')} disabled={disabled||index===event.groups.length-1} onClick={()=>edit(e=>{[e.groups[index+1],e.groups[index]]=[e.groups[index],e.groups[index+1]];})}/><Button icon="Trash2" title={tr('Удалить группу')} onClick={()=>edit(e=>e.groups=e.groups.filter(x=>x.id!==g.id))}/></div></div>
   {g.actions.map(source=>{const a={...source,...(source.id===event.groups[0]?.actions[0]?.id?binding?.overrides:{}),...binding?.actionOverrides?.[source.id]};return onSelectAction?<button key={a.id} className="event-action-link" onClick={()=>onSelectAction(a.id)}><Icon name={TYPES[a.type]?.icon||'Zap'} size={14}/><span>{actionSummary(a,project)}</span><Icon name="ChevronRight" size={13}/></button>:<details key={a.id} className="event-action-card"><summary><Icon name="ChevronRight" size={12}/><span>{actionSummary(a,project)}</span></summary><div className="event-action-fields">
    <Field label={tr('Тип')}><Select aria-label={tr('Тип действия')} value={a.type} options={Object.entries(TYPES).map(([id,t])=>[id,t.label])} onChange={type=>onAction(a.id,{...makeAction(type),id:a.id})}/></Field>
    <ActionFields inspector action={a} project={project} sceneId={sceneId} onChange={v=>onAction(a.id,v)} onPickPosition={onPickPosition}/>
    <Section title={tr('Выполнение действия')} open={false}><ActionExecutionFields action={a} onChange={v=>onAction(a.id,v)}/></Section>
    <Button className="event-remove" icon="Trash2" title={tr('Удалить действие')} onClick={()=>edit(e=>e.groups.find(x=>x.id===g.id).actions=e.groups.find(x=>x.id===g.id).actions.filter(x=>x.id!==a.id))}>{tr('Удалить действие')}</Button>
   </div></details>;})}
   <div className="event-add-actions"><Button icon="Puzzle" onClick={()=>edit(e=>e.groups.find(x=>x.id===g.id).actions.push({...makeAction('gameplay'),command:'sacura.component.call',operationVersion:1,value:{}}))}>{tr('Вызов метода')}</Button><Button icon="Plus" onClick={()=>edit(e=>e.groups.find(x=>x.id===g.id).actions.push(makeAction('wait')))}>{tr('Действие')}</Button></div>
  </div>)}
  <Button icon="Plus" onClick={()=>edit(e=>e.groups.push({id:uid('group'),name:tr('Следующий шаг'),actions:[makeAction('wait')]}))}>{tr('Добавить группу')}</Button>
 </fieldset>;
}
