import {inputLabel} from './inputPresentation.js';
import React,{useState} from 'react';
import {t as tr, useLocale, message} from './i18n.jsx';
import {uid} from './model.js';
import {runtimeVariableType} from './variableModel.js';
import {INPUT_TYPES,INPUT_TRIGGERS,INPUT_BEHAVIORS,INPUT_DEVICES,inputContextsFor,validateInputSettings} from './inputModel.js';
import './input.css';
const options=items=>Object.entries(items).map(([id,name])=><option key={id} value={id}>{tr(name)}</option>);
export function InputContextSettings({project,beat,onChange,onOpen,disabled}){
 useLocale();const active=inputContextsFor(project,beat),custom=beat.inputContexts!==undefined;
 return <fieldset className="input-context-settings" disabled={disabled} style={{border:0,padding:0}}>
  <p>{tr('Контекст связывает устройства с действиями в этом моменте истории. Системный контекст действует всегда.')}</p>
  {project.input.contexts.filter(c=>c.id!=='system').map(c=><label className="check" key={c.id}><input type="checkbox" checked={active.includes(c.id)} onChange={e=>onChange({inputContexts:e.target.checked?[...active.filter(id=>id!=='system'),c.id]:active.filter(id=>id!=='system'&&id!==c.id)})}/>{inputLabel(c)}</label>)}
  <button onClick={onOpen}>{tr('Настроить действия и привязки')}</button>{custom&&<button onClick={()=>onChange({inputContexts:undefined})}>{tr('Вернуть автоматический контекст')}</button>}
 </fieldset>;
}
function Binding({binding,actions,onChange,onDelete}){
 const [listening,setListening]=useState(false),action=actions.find(a=>a.id===binding.actionId),vector=action?.valueType==='axis2d',scale=Array.isArray(binding.scale)?binding.scale:[binding.scale??1,0];
 return <div className="input-binding"><div className="input-row">
  <label>{tr('Действие')}<select value={binding.actionId} onChange={e=>{const a=actions.find(a=>a.id===e.target.value);onChange({actionId:a.id,scale:a.valueType==='axis2d'?[1,0]:1});}}>{actions.map(a=><option key={a.id} value={a.id}>{inputLabel(a)}</option>)}</select></label>
  <label>{tr('Источник')}<select value={binding.device} onChange={e=>{setListening(false);onChange({device:e.target.value,control:e.target.value==='keyboard'?'KeyE':e.target.value==='touch'?binding.actionId:e.target.value==='pointer'?'primary':e.target.value==='external'?'custom':'0',deadZone:e.target.value==='gamepad-axis'?.18:0});}}>{options(INPUT_DEVICES)}</select></label>
  <label>{tr('Код кнопки / оси')}<input value={binding.control} onChange={e=>onChange({control:e.target.value})}/></label>
  {binding.device==='keyboard'&&<button aria-pressed={listening} onClick={e=>{setListening(v=>!v);e.currentTarget.focus();}} onBlur={()=>setListening(false)} onKeyDown={e=>{if(!listening)return;e.preventDefault();e.stopPropagation();if(e.code!=='Escape')onChange({control:e.code});setListening(false);}}>{tr(listening?'Нажмите клавишу…':'Записать клавишу')}</button>}
  <button className="input-delete" aria-label={tr('Удалить привязку')} onClick={onDelete}>×</button>
 </div><details><summary>{tr('Модификаторы')}</summary><div className="input-row">
  <label>{tr('Масштаб X')}<input type="number" step=".1" value={scale[0]} onChange={e=>onChange({scale:vector?[Number(e.target.value),scale[1]]:Number(e.target.value)})}/></label>
  {vector&&<label>{tr('Масштаб Y')}<input type="number" step=".1" value={scale[1]} onChange={e=>onChange({scale:[scale[0],Number(e.target.value)]})}/></label>}
  <label>{tr('Мёртвая зона')}<input type="number" min="0" max=".95" step=".01" value={binding.deadZone||0} onChange={e=>onChange({deadZone:Math.min(.95,Math.max(0,Number(e.target.value)))})}/></label>
 </div><small>{tr('Отрицательный масштаб меняет направление. Для кнопки W → движение вверх задайте X = 0, Y = 1.')}</small></details></div>;
}
export default function InputWorkspace({project,onChange,beat,preview,devices=[],running,onStop}){
 useLocale();const input=project.input,[selectedAction,setAction]=useState(input.actions[0]?.id||''),[selectedContext,setContext]=useState('gameplay');
 const action=input.actions.find(a=>a.id===selectedAction)||input.actions[0],context=input.contexts.find(c=>c.id===selectedContext)||input.contexts[0];
 const edit=fn=>{if(!running)onChange(p=>fn(p.input));},patchAction=patch=>edit(s=>Object.assign(s.actions.find(a=>a.id===action.id),patch)),patchContext=patch=>edit(s=>Object.assign(s.contexts.find(c=>c.id===context.id),patch));
 const addAction=()=>{const id=uid('input-action');edit(s=>s.actions.push({id,name:'Новое действие',valueType:'boolean',behavior:'event',trigger:'pressed',holdSeconds:.4,consume:true,eventId:''}));setAction(id);};
 const addContext=()=>{const id=uid('input-context');edit(s=>s.contexts.push({id,name:'Новый контекст',priority:30,bindings:[]}));setContext(id);};
 const issues=validateInputSettings(project);
 return <div className="input-workspace"><header><strong>{tr('Действия ввода и контексты')}</strong>{running&&<button onClick={onStop}>{tr('Остановить для редактирования')}</button>}</header>
  <p>{tr('Сначала создайте действие игрока, затем привяжите к нему устройства. Один сценарий работает с клавиатурой, телефоном и геймпадом.')}</p>
  <div className="input-monitor" role="status"><span>{tr('Активные контексты')}: {inputContextsFor(project,beat).map(id=>input.contexts.find(c=>c.id===id)?.name).join(', ')}</span><span>{tr('Геймпады')}: {devices.length?devices.map(d=>d.name+(d.mapping==='standard'?'':' · '+tr('свои коды кнопок'))).join(', '):tr('не обнаружены · нажмите кнопку подключённого геймпада')}</span>{preview?.lastInput&&<span>{preview.lastInput.name}: {preview.lastInput.phase} · {JSON.stringify(preview.lastInput.value)} · {preview.lastInput.device}</span>}</div>
  {issues.map(i=><p className="input-error" role="alert" key={i.id}>{message(i.detail)}</p>)}
  <fieldset disabled={running}><legend>{tr('Действия ввода · Input Actions')}</legend><div className="input-list"><label>{tr('Действие')}<select value={action?.id||''} onChange={e=>setAction(e.target.value)}>{input.actions.map(a=><option key={a.id} value={a.id}>{inputLabel(a)}</option>)}</select></label><button onClick={addAction}>{tr('Создать действие')}</button></div>
   {action&&<><div className="input-row"><label>{tr('Название')}<input value={action.name} onChange={e=>patchAction({name:e.target.value})}/></label><label>{tr('Тип значения')}<select value={action.valueType} onChange={e=>patchAction({valueType:e.target.value,variable:'',variableX:'',variableY:'',behavior:action.behavior==='move'&&e.target.value!=='axis2d'?'event':action.behavior})}>{options(INPUT_TYPES)}</select></label><label>{tr('Обработчик')}<select value={action.behavior} onChange={e=>patchAction({behavior:e.target.value,...(e.target.value==='move'?{valueType:'axis2d',trigger:'continuous',variable:''}:{})})}>{options(INPUT_BEHAVIORS)}</select></label></div>
    <div className="input-row"><label>{tr('Триггер')}<select value={action.trigger} onChange={e=>patchAction({trigger:e.target.value})}>{options(INPUT_TRIGGERS)}</select></label>{action.trigger==='hold'&&<label>{tr('Удержание, с')}<input type="number" min="0" step=".1" value={action.holdSeconds} onChange={e=>patchAction({holdSeconds:Math.max(0,Number(e.target.value))})}/></label>}{action.behavior==='event'&&<label>{tr('Событие движка')}<select value={action.eventId||''} onChange={e=>patchAction({eventId:e.target.value})}><option value="">{tr('Выберите событие')}</option>{project.events.map(e=><option key={e.id} value={e.id}>{e.name}</option>)}</select></label>}</div>
    <div className="input-row">{(action.valueType==='axis2d'?['variableX','variableY']:['variable']).map(field=><label key={field}>{tr(field==='variableX'?'Переменная X':field==='variableY'?'Переменная Y':'Переменная значения')}<select value={action[field]||''} onChange={e=>patchAction({[field]:e.target.value})}><option value="">{tr('Не записывать')}</option>{Object.keys(project.variables).filter(id=>runtimeVariableType(project,id)===(action.valueType==='boolean'?'boolean':'number')).map(id=><option key={id} value={id}>{id}</option>)}</select></label>)}</div>
    <small>{tr('Значение доступно в переменной для Blueprint и событий. При отпускании, смене контекста и паузе оно сбрасывается. Переменные создаются во вкладке Blueprint.')}</small>
    <label className="check"><input type="checkbox" checked={action.consume!==false} onChange={e=>patchAction({consume:e.target.checked})}/>{tr('Перехватывать ввод у контекстов с меньшим приоритетом')}</label>
    <small>ID: {action.id} · {tr('Фазы действия: Started, Triggered, Completed, Canceled. Событие движка запускается на Triggered. Удержание срабатывает один раз; «Пока активно» — на каждом кадре. Одно событие действия выполняется в одном экземпляре.')}</small>
    <button className="input-delete" onClick={()=>{edit(s=>{s.actions=s.actions.filter(a=>a.id!==action.id);s.contexts.forEach(c=>c.bindings=c.bindings.filter(b=>b.actionId!==action.id));});setAction('');}}>{tr('Удалить действие')}</button>
   </>}
  </fieldset>
  <fieldset disabled={running}><legend>{tr('Контексты · Input Mapping Contexts')}</legend><div className="input-list"><label>{tr('Контекст')}<select value={context?.id||''} onChange={e=>setContext(e.target.value)}>{input.contexts.map(c=><option key={c.id} value={c.id}>{inputLabel(c)}</option>)}</select></label><button onClick={addContext}>{tr('Создать контекст')}</button></div>
   {context&&<><div className="input-row"><label>{tr('Название контекста')}<input value={context.name} onChange={e=>patchContext({name:e.target.value})}/></label><label>{tr('Приоритет')}<input type="number" value={context.priority} onChange={e=>patchContext({priority:Number(e.target.value)})}/></label></div>
    {context.bindings.map(b=><Binding key={b.id} binding={b} actions={input.actions} onChange={patch=>edit(s=>Object.assign(s.contexts.find(c=>c.id===context.id).bindings.find(x=>x.id===b.id),patch))} onDelete={()=>edit(s=>s.contexts.find(c=>c.id===context.id).bindings=context.bindings.filter(x=>x.id!==b.id))}/>)}
    <button disabled={!action} onClick={()=>edit(s=>s.contexts.find(c=>c.id===context.id).bindings.push({id:uid('input-binding'),actionId:action.id,device:'keyboard',control:'KeyE',scale:action.valueType==='axis2d'?[1,0]:1,deadZone:0}))}>{tr('Добавить привязку выбранного действия')}</button>
    {!['system','dialogue','gameplay'].includes(context.id)&&<button className="input-delete" onClick={()=>{onChange(p=>{p.input.contexts=p.input.contexts.filter(c=>c.id!==context.id);p.chapters.flatMap(c=>c.beats).forEach(b=>{if(b.inputContexts)b.inputContexts=b.inputContexts.filter(id=>id!==context.id);});});setContext('gameplay');}}>{tr('Удалить контекст')}</button>}
   </>}
  </fieldset>
  <fieldset disabled={running}><legend>{tr('Устройства')}</legend><label>{tr('Экранное управление')}<select value={input.touchMode} onChange={e=>edit(s=>s.touchMode=e.target.value)}><option value="auto">{tr('Автоматически на сенсорном устройстве')}</option><option value="always">{tr('Всегда показывать · для проверки')}</option><option value="off">{tr('Скрыть')}</option></select></label><p>{tr('Стандартный геймпад: левый стик / крестовина — движение, A / Cross — осмотр или подтверждение, Start — пауза. Для других раскладок измените коды осей и кнопок.')}</p><small>{tr('Клавиатура: физические коды KeyW, Space и другие. Геймпад: номера осей и кнопок с нуля. Указатель: primary — клик с координатами, button0 / button1 / button2 — удержание кнопок. Touch: код экранного элемента. Другой источник: произвольный код адаптера.')}</small></fieldset>
 </div>;
}
