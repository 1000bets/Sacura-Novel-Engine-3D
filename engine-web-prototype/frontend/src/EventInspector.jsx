import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import ActionFields from './ActionFields.jsx';
import {Button} from './StudioParts.jsx';
import {makeAction,TYPES,uid} from './model.js';
export default function EventInspector({event,project,sceneId,binding,onEdit,onAction,onPickPosition}){
 useLocale();
 const patch=value=>onEdit(e=>Object.assign(e,value));
 return <div className="inline-event-editor">
 {binding&&<p className="resource-note">{tr("Изменения применяются к этому размещению события.")}</p>}
 <label>{tr("Название события")}<input value={event.name} onChange={e=>patch({name:e.target.value})}/></label>
 <label>{tr("Описание")}<textarea value={event.description||''} onChange={e=>patch({description:e.target.value})}/></label>
 <label>{tr("После действий")}<select value={event.retention} onChange={e=>patch({retention:e.target.value})}><option value="AUTO_CLOSE_ON_FLOW_END">{tr("Завершить событие")}</option><option value="HOLD_UNTIL_STOPPED">{tr("До остановки")}</option><option value="HOLD_UNTIL_REPLACED">{tr("До замены")}</option></select></label>
 {event.retention==='HOLD_UNTIL_REPLACED'&&<label>{tr("Канал замены")}<input value={event.channel||''} onChange={e=>patch({channel:e.target.value})}/></label>}
 <label>{tr("Область события")}<select value={event.owner} onChange={e=>patch({owner:e.target.value})}><option value="SubScene">{tr("Сабсцена")}</option><option value="Scene">{tr("Сцена")}</option><option value="GameSession">{tr("Игровая сессия")}</option></select></label>
 {event.groups.map((g,index)=><section key={g.id} className="inline-event-group">
 <label>{tr("Группа ")}{index+1}<input value={g.name} onChange={e=>onEdit(ev=>ev.groups.find(x=>x.id===g.id).name=e.target.value)}/></label>
 <div><Button icon="ArrowUp" title={tr("Переместить группу выше")} disabled={!index} onClick={()=>onEdit(e=>{[e.groups[index-1],e.groups[index]]=[e.groups[index],e.groups[index-1]];})}/><Button icon="ArrowDown" title={tr("Переместить группу ниже")} disabled={index===event.groups.length-1} onClick={()=>onEdit(e=>{[e.groups[index+1],e.groups[index]]=[e.groups[index],e.groups[index+1]];})}/><Button icon="Trash2" title={tr("Удалить группу")} onClick={()=>onEdit(e=>e.groups=e.groups.filter(x=>x.id!==g.id))}/></div>
 {g.actions.map(source=>{const a={...source,...(source.id===event.groups[0]?.actions[0]?.id?binding?.overrides:{}),...binding?.actionOverrides?.[source.id]};return <details key={a.id} open className="inline-event-action"><summary>{tr(TYPES[a.type]?.label)}</summary>
 <label>{tr("Тип действия")}<select value={a.type} onChange={e=>onAction(a.id,{...makeAction(e.target.value),id:a.id})}>{Object.entries(TYPES).map(([id,t])=><option key={id} value={id}>{tr(t.label)}</option>)}</select></label>
 <ActionFields action={a} project={project} sceneId={sceneId} onChange={v=>onAction(a.id,v)} onPickPosition={onPickPosition}/>
 <details><summary>{tr("Дополнительные параметры действия")}</summary>
 <label>{tr("Если ресурс занят")}<select value={a.conflict} onChange={e=>onAction(a.id,{conflict:e.target.value})}><option value="FAIL_NEW">{tr("Ошибка")}</option><option value="REPLACE_CURRENT">{tr("Заменить")}</option><option value="QUEUE">{tr("Ожидать")}</option></select></label>
 <label>{tr("Ожидание")}<select value={a.wait} onChange={e=>onAction(a.id,{wait:e.target.value})}><option value="COMPLETED">{tr("Завершения")}</option><option value="STARTED">{tr("Запуска")}</option><option value="NONE">{tr("Продолжить сразу")}</option></select></label>
 <label>{tr("Область действия")}<select value={a.scope} onChange={e=>onAction(a.id,{scope:e.target.value})}><option value="SELF">{tr("Действие")}</option><option value="EVENT">{tr("Событие")}</option></select></label>
 {['queueTimeout','startTimeout','executionTimeout','stopTimeout'].map((key,i)=><label key={key}>{tr(['Лимит очереди','Лимит запуска','Лимит выполнения','Лимит остановки'][i])}{tr(", сек")}<input type="number" min="0" step="0.1" value={a[key]??0} onChange={e=>onAction(a.id,{[key]:Number(e.target.value)})}/></label>)}
 {['onFailure','finalState','fallback'].map((key,i)=><label key={key}>{tr(['При ошибке','Итоговое состояние','Резервное состояние'][i])}<input value={a[key]||''} onChange={e=>onAction(a.id,{[key]:e.target.value})}/></label>)}
 </details>
 <Button icon="Trash2" onClick={()=>onEdit(e=>e.groups.find(x=>x.id===g.id).actions=e.groups.find(x=>x.id===g.id).actions.filter(x=>x.id!==a.id))}>{tr("Удалить действие")}</Button></details>;})}
 <Button icon="Plus" onClick={()=>onEdit(e=>e.groups.find(x=>x.id===g.id).actions.push(makeAction('wait')))}>{tr("Добавить действие")}</Button>
 </section>)}
 <Button icon="Plus" onClick={()=>onEdit(e=>e.groups.push({id:uid('group'),name:'Следующий шаг',actions:[makeAction('wait')]}))}>{tr("Добавить группу")}</Button>
 </div>;
}
