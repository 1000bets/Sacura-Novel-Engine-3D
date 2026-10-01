import React from 'react';
import ActionFields from './ActionFields.jsx';
import {Button} from './StudioParts.jsx';
import {makeAction,TYPES,uid} from './model.js';
export default function EventInspector({event,project,sceneId,binding,onEdit,onAction,onPickPosition}){
 const patch=value=>onEdit(e=>Object.assign(e,value));
 return <div className="inline-event-editor">
 {binding&&<p className="resource-note">Изменения применяются к этому размещению события.</p>}
 <label>Название события<input value={event.name} onChange={e=>patch({name:e.target.value})}/></label>
 <label>Описание<textarea value={event.description||''} onChange={e=>patch({description:e.target.value})}/></label>
 <label>После действий<select value={event.retention} onChange={e=>patch({retention:e.target.value})}><option value="AUTO_CLOSE_ON_FLOW_END">Завершить событие</option><option value="HOLD_UNTIL_STOPPED">До остановки</option><option value="HOLD_UNTIL_REPLACED">До замены</option></select></label>
 {event.retention==='HOLD_UNTIL_REPLACED'&&<label>Канал замены<input value={event.channel||''} onChange={e=>patch({channel:e.target.value})}/></label>}
 <label>Область события<select value={event.owner} onChange={e=>patch({owner:e.target.value})}><option value="SubScene">Сабсцена</option><option value="Scene">Сцена</option><option value="GameSession">Игровая сессия</option></select></label>
 {event.groups.map((g,index)=><section key={g.id} className="inline-event-group">
 <label>Группа {index+1}<input value={g.name} onChange={e=>onEdit(ev=>ev.groups.find(x=>x.id===g.id).name=e.target.value)}/></label>
 <div><Button icon="ArrowUp" title="Переместить группу выше" disabled={!index} onClick={()=>onEdit(e=>{[e.groups[index-1],e.groups[index]]=[e.groups[index],e.groups[index-1]];})}/><Button icon="ArrowDown" title="Переместить группу ниже" disabled={index===event.groups.length-1} onClick={()=>onEdit(e=>{[e.groups[index+1],e.groups[index]]=[e.groups[index],e.groups[index+1]];})}/><Button icon="Trash2" title="Удалить группу" onClick={()=>onEdit(e=>e.groups=e.groups.filter(x=>x.id!==g.id))}/></div>
 {g.actions.map(source=>{const a={...source,...(source.id===event.groups[0]?.actions[0]?.id?binding?.overrides:{}),...binding?.actionOverrides?.[source.id]};return <details key={a.id} open className="inline-event-action"><summary>{TYPES[a.type]?.label}</summary>
 <label>Тип действия<select value={a.type} onChange={e=>onAction(a.id,{...makeAction(e.target.value),id:a.id})}>{Object.entries(TYPES).map(([id,t])=><option key={id} value={id}>{t.label}</option>)}</select></label>
 <ActionFields action={a} project={project} sceneId={sceneId} onChange={v=>onAction(a.id,v)} onPickPosition={onPickPosition}/>
 <details><summary>Дополнительные параметры действия</summary>
 <label>Если ресурс занят<select value={a.conflict} onChange={e=>onAction(a.id,{conflict:e.target.value})}><option value="FAIL_NEW">Ошибка</option><option value="REPLACE_CURRENT">Заменить</option><option value="QUEUE">Ожидать</option></select></label>
 <label>Ожидание<select value={a.wait} onChange={e=>onAction(a.id,{wait:e.target.value})}><option value="COMPLETED">Завершения</option><option value="STARTED">Запуска</option><option value="NONE">Продолжить сразу</option></select></label>
 <label>Область действия<select value={a.scope} onChange={e=>onAction(a.id,{scope:e.target.value})}><option value="SELF">Действие</option><option value="EVENT">Событие</option></select></label>
 {['queueTimeout','startTimeout','executionTimeout','stopTimeout'].map((key,i)=><label key={key}>{['Лимит очереди','Лимит запуска','Лимит выполнения','Лимит остановки'][i]}, сек<input type="number" min="0" step="0.1" value={a[key]??0} onChange={e=>onAction(a.id,{[key]:Number(e.target.value)})}/></label>)}
 {['onFailure','finalState','fallback'].map((key,i)=><label key={key}>{['При ошибке','Итоговое состояние','Резервное состояние'][i]}<input value={a[key]||''} onChange={e=>onAction(a.id,{[key]:e.target.value})}/></label>)}
 </details>
 <Button icon="Trash2" onClick={()=>onEdit(e=>e.groups.find(x=>x.id===g.id).actions=e.groups.find(x=>x.id===g.id).actions.filter(x=>x.id!==a.id))}>Удалить действие</Button></details>;})}
 <Button icon="Plus" onClick={()=>onEdit(e=>e.groups.find(x=>x.id===g.id).actions.push(makeAction('wait')))}>Добавить действие</Button>
 </section>)}
 <Button icon="Plus" onClick={()=>onEdit(e=>e.groups.push({id:uid('group'),name:'Следующий шаг',actions:[makeAction('wait')]}))}>Добавить группу</Button>
 </div>;
}
