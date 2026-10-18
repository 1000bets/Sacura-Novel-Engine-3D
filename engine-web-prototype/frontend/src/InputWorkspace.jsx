import {inputLabel,bindingControlLabel,keyboardLabel,bindingDirection,INPUT_DIRECTIONS,POINTER_CONTROLS,GAMEPAD_AXES,GAMEPAD_BUTTONS} from './inputPresentation.js';
import React,{useEffect,useRef,useState} from 'react';
import {Plus,Trash2,Keyboard,MousePointer2,Gamepad2,Smartphone,SlidersHorizontal,ChevronDown,Radio,ArrowRight,Move,Settings2} from 'lucide-react';
import {t as tr, useLocale, message} from './i18n.jsx';
import {uid} from './model.js';
import {runtimeVariableType} from './variableModel.js';
import {INPUT_TYPES,INPUT_TRIGGERS,INPUT_BEHAVIORS,INPUT_DEVICES,inputContextsFor,validateInputSettings} from './inputModel.js';
import {OperationFields} from './SchemaFields.jsx';
import './inputWorkspace.css';
const options=items=>Object.entries(items).map(([id,name])=><option key={id} value={id}>{tr(name)}</option>);
const deviceGroups=[['keyboard','Клавиатура',Keyboard],['pointer','Мышь',MousePointer2],['gamepad','Геймпад',Gamepad2],['touch','На экране',Smartphone],['external','Другие устройства',Radio]];
const groupFor=binding=>binding.device.startsWith('gamepad-')?'gamepad':Object.hasOwn(INPUT_DEVICES,binding.device)?binding.device:'external';
const bindingSummary=bindings=>{
 const keys=new Set(bindings.filter(binding=>binding.device==='keyboard').map(binding=>binding.control)),labels=[];
 for(const [controls,label] of [[['KeyW','KeyA','KeyS','KeyD'],'WASD'],[['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'],'↑ ↓ ← →']])if(controls.every(code=>keys.has(code))){labels.push(label);controls.forEach(code=>keys.delete(code));}
 labels.push(...[...keys].map(keyboardLabel));
 const values=labels.length?labels:[...new Set(bindings.map(bindingControlLabel))];
 return values.slice(0,4).join(' · ')+(values.length>4?' …':'');
};

export function InputContextSettings({project,beat,onChange,onOpen,disabled}){
 useLocale();const active=inputContextsFor(project,beat),custom=beat.inputContexts!==undefined;
 return <fieldset className="input-context-settings" disabled={disabled} style={{border:0,padding:0}}>
  <p>{tr('Контекст связывает устройства с действиями в этом моменте истории. Системный контекст действует всегда.')}</p>
  {project.input.contexts.filter(c=>c.id!=='system').map(c=><label className="check" key={c.id}><input type="checkbox" checked={active.includes(c.id)} onChange={e=>onChange({inputContexts:e.target.checked?[...active.filter(id=>id!=='system'),c.id]:active.filter(id=>id!=='system'&&id!==c.id)})}/>{inputLabel(c)}</label>)}
  <button onClick={onOpen}>{tr('Настроить действия и привязки')}</button>{custom&&<button onClick={()=>onChange({inputContexts:undefined})}>{tr('Вернуть автоматический контекст')}</button>}
 </fieldset>;
}

function Binding({binding,action,actions,onChange,onDelete}){
 const [listening,setListening]=useState(false),[expanded,setExpanded]=useState(false);
 const vector=action.valueType==='axis2d',scale=Array.isArray(binding.scale)?binding.scale:[binding.scale??1,0];
 const controlOptions=binding.device==='pointer'?POINTER_CONTROLS:binding.device==='gamepad-axis'?GAMEPAD_AXES:binding.device==='gamepad-button'?GAMEPAD_BUTTONS:null;
 const analog=binding.device==='gamepad-axis'||binding.device==='touch'||binding.device==='pointer'&&binding.control==='delta';
 const directions=analog?{right:'По горизонтали',left:'Инверсия X',up:'По вертикали',down:'Инверсия Y',both:'Обе оси'}:{up:'Вверх',down:'Вниз',left:'Влево',right:'Вправо',both:'Обе оси'};
 const changeDevice=device=>{
  setListening(false);
  onChange({device,control:device==='keyboard'?'KeyE':device==='touch'?action.id:device==='pointer'?'primary':device==='external'?'custom':'0',deadZone:device==='gamepad-axis'?.18:0,...(vector&&(device==='touch'||device==='gamepad-axis')?{scale:device==='touch'?[1,1]:[1,0]}:{})});
 };
 return <div className="input-binding">
  <div className="input-binding-row">
   <div className="input-binding-control">
    {binding.device==='keyboard'?<button className={'input-key-button'+(listening?' listening':'')} aria-label={listening?tr('Нажмите клавишу…'):tr('Изменить клавишу {0}',[keyboardLabel(binding.control)])} aria-pressed={listening} onClick={e=>{setListening(value=>!value);e.currentTarget.focus();}} onBlur={()=>setListening(false)} onKeyDown={e=>{if(!listening)return;e.preventDefault();e.stopPropagation();if(e.code!=='Escape')onChange({control:e.code});setListening(false);}}>{listening?<span role="status">{tr('Нажмите клавишу…')}</span>:<><kbd>{keyboardLabel(binding.control)}</kbd><span>{tr('Изменить')}</span></>}</button>:
     controlOptions?<label><span className="input-sr-only">{tr('Кнопка или ось')}</span><select value={binding.control} onChange={e=>onChange({control:e.target.value})}>{options(controlOptions)}{!Object.hasOwn(controlOptions,binding.control)&&<option value={binding.control}>{tr('Свой код: {0}',[binding.control])}</option>}</select></label>:
     binding.device==='touch'?<span className="input-touch-label">{tr(vector?'Экранный стик':action.valueType==='axis1d'?'Экранный ползунок':'Экранная кнопка')}</span>:
     <label><span className="input-sr-only">{tr('Код управления')}</span><input aria-label={tr('Код управления')} value={binding.control} onChange={e=>onChange({control:e.target.value})}/></label>}
   </div>
   {vector&&<label className="input-binding-direction"><span className="input-sr-only">{tr('Направление')}</span><select value={bindingDirection(binding)} onChange={e=>{if(e.target.value==='custom')setExpanded(true);else onChange({scale:[...INPUT_DIRECTIONS[e.target.value]]});}}>{options(directions)}<option value="custom">{tr('Свои значения…')}</option></select></label>}
   <button className="input-icon-button" aria-expanded={expanded} title={tr('Параметры привязки')} aria-label={tr('Параметры привязки {0}',[bindingControlLabel(binding)])} onClick={()=>setExpanded(value=>!value)}><SlidersHorizontal aria-hidden="true" size={15}/></button>
   <button className="input-icon-button input-delete" aria-label={tr('Удалить привязку {0}',[bindingControlLabel(binding)])} title={tr('Удалить привязку')} onClick={onDelete}><Trash2 aria-hidden="true" size={15}/></button>
  </div>
  {expanded&&<div className="input-binding-advanced"><div className="input-fields">
   <label>{tr('Устройство')}<select value={binding.device} onChange={e=>changeDevice(e.target.value)}>{options(INPUT_DEVICES)}{!INPUT_DEVICES[binding.device]&&<option value={binding.device}>{binding.device}</option>}</select></label>
   <label>{tr('Масштаб X')}<input type="number" step=".1" value={scale[0]} onChange={e=>onChange({scale:vector?[Number(e.target.value),scale[1]]:Number(e.target.value)})}/></label>
   {vector&&<label>{tr('Масштаб Y')}<input type="number" step=".1" value={scale[1]} onChange={e=>onChange({scale:[scale[0],Number(e.target.value)]})}/></label>}
   <label>{tr('Мёртвая зона')}<input type="number" min="0" max=".95" step=".01" value={binding.deadZone||0} onChange={e=>onChange({deadZone:Math.min(.95,Math.max(0,Number(e.target.value)))})}/></label>
   <label>{tr('Код управления')}<input value={binding.control} onChange={e=>onChange({control:e.target.value})}/></label>
   <label>{tr('Действие')}<select value={binding.actionId} onChange={e=>{const next=actions.find(item=>item.id===e.target.value);onChange({actionId:next.id,scale:next.valueType==='axis2d'?[1,0]:1});}}>{actions.map(item=><option key={item.id} value={item.id}>{inputLabel(item)}</option>)}</select></label>
  </div></div>}
 </div>;
}

export default function InputWorkspace({project,onChange,beat,preview,devices=[],running,onStop}){
 useLocale();
 const input=project.input,[tab,setTab]=useState('actions'),[selectedAction,setAction]=useState(input.actions[0]?.id||''),[selectedContext,setContext]=useState('gameplay'),[bindingDevice,setBindingDevice]=useState('keyboard');
 const nameField=useRef(null),focusName=useRef(false);
 const action=input.actions.find(item=>item.id===selectedAction)||input.actions[0],context=input.contexts.find(item=>item.id===selectedContext)||input.contexts[0];
 useEffect(()=>{if(focusName.current){nameField.current?.focus();nameField.current?.select();focusName.current=false;}},[selectedAction,selectedContext,tab]);
 const edit=fn=>{if(!running)onChange(p=>fn(p.input));};
 const patchAction=patch=>edit(settings=>Object.assign(settings.actions.find(item=>item.id===action.id),patch));
 const patchContext=patch=>edit(settings=>Object.assign(settings.contexts.find(item=>item.id===context.id),patch));
 const chooseAction=(id,keepContext=false)=>{
  setAction(id);setTab('actions');
  if(!keepContext&&!context?.bindings.some(binding=>binding.actionId===id))setContext(input.contexts.find(item=>item.bindings.some(binding=>binding.actionId===id))?.id||context?.id||'');
 };
 const addAction=()=>{if(running)return;const id=uid('input-action');edit(settings=>settings.actions.push({id,name:tr('Новое действие'),valueType:'boolean',behavior:'event',trigger:'pressed',holdSeconds:.4,consume:true,eventId:''}));focusName.current=true;setAction(id);};
 const addContext=()=>{if(running)return;const id=uid('input-context');edit(settings=>settings.contexts.push({id,name:tr('Новый контекст'),priority:30,bindings:[]}));focusName.current=true;setContext(id);};
 const activeContexts=inputContextsFor(project,beat),issues=validateInputSettings(project);
 const bindings=context?.bindings.filter(binding=>binding.actionId===action?.id)||[];
 const addBinding=()=>edit(settings=>settings.contexts.find(item=>item.id===context.id).bindings.push({id:uid('input-binding'),actionId:action.id,device:bindingDevice,control:bindingDevice==='keyboard'?(action.valueType==='axis2d'?'KeyW':'KeyE'):bindingDevice==='touch'?action.id:bindingDevice==='pointer'?'primary':bindingDevice==='external'?'custom':'0',scale:action.valueType==='axis2d'?(bindingDevice==='touch'?[1,1]:bindingDevice==='keyboard'?[0,1]:[1,0]):1,deadZone:bindingDevice==='gamepad-axis'?.18:0}));
 const removeAction=()=>{edit(settings=>{settings.actions=settings.actions.filter(item=>item.id!==action.id);settings.contexts.forEach(item=>item.bindings=item.bindings.filter(binding=>binding.actionId!==action.id));});setAction('');};
 const removeContext=()=>{if(running)return;onChange(p=>{p.input.contexts=p.input.contexts.filter(item=>item.id!==context.id);p.chapters.flatMap(chapter=>chapter.beats).forEach(item=>{if(item.inputContexts)item.inputContexts=item.inputContexts.filter(id=>id!==context.id);});});setContext('gameplay');};
 return <div className="input-workspace">
  <header className="input-header"><nav aria-label={tr('Разделы ввода')}>{[['actions','Действия'],['contexts','Контексты'],['devices','Устройства']].map(([id,label])=><button key={id} aria-pressed={tab===id} onClick={()=>setTab(id)}>{tr(label)}</button>)}</nav>{running&&<button className="input-stop" onClick={onStop}>{tr('Остановить для редактирования')}</button>}</header>
  {issues.length>0&&<div className="input-errors">{issues.map(issue=><p className="input-error" role="alert" key={issue.id}>{message(issue.detail)}</p>)}</div>}
  {tab==='actions'&&<div className="input-editor-grid">
   <aside className="input-library"><div className="input-library-heading"><h2>{tr('Действия')}</h2><button className="input-icon-button" disabled={running} aria-label={tr('Создать действие')} title={tr('Создать действие')} onClick={addAction}><Plus aria-hidden="true" size={17}/></button></div><label className="input-mobile-picker"><span className="input-sr-only">{tr('Выбрать действие')}</span><select value={action?.id||''} onChange={e=>chooseAction(e.target.value)}>{input.actions.map(item=><option key={item.id} value={item.id}>{inputLabel(item)}</option>)}</select></label><nav aria-label={tr('Действия ввода')}>{input.actions.map(item=>{
    const summary=bindingSummary(input.contexts.flatMap(group=>group.bindings).filter(binding=>binding.actionId===item.id)),Icon=item.valueType==='axis2d'?Move:Keyboard;
    return <button key={item.id} aria-pressed={action?.id===item.id} onClick={()=>chooseAction(item.id)}><Icon aria-hidden="true" size={16}/><span><strong>{inputLabel(item)}</strong><small>{summary||tr('Нет привязок')}</small></span></button>;
   })}</nav></aside>
   <div className="input-editor">{action?<fieldset className="input-edit-fields" disabled={running}>
    <div className="input-section-heading"><h2>{inputLabel(action)}</h2><button className="input-icon-button input-delete" aria-label={tr('Удалить действие')} title={tr('Удалить действие')} onClick={removeAction}><Trash2 aria-hidden="true" size={16}/></button></div>
    <div className="input-fields input-action-fields"><label>{tr('Название')}<input ref={nameField} value={action.name} onChange={e=>patchAction({name:e.target.value})}/></label><label>{tr('Что делает')}<select value={action.behavior} onChange={e=>patchAction({behavior:e.target.value,...(e.target.value==='move'?{valueType:'axis2d',trigger:'continuous',variable:''}:{})})}>{options(INPUT_BEHAVIORS)}</select></label><label>{tr('Когда срабатывает')}<select value={action.trigger} onChange={e=>patchAction({trigger:e.target.value})}>{options(INPUT_TRIGGERS)}</select></label>
     {action.trigger==='hold'&&<label>{tr('Удержание, с')}<input type="number" min="0" step=".1" value={action.holdSeconds} onChange={e=>patchAction({holdSeconds:Math.max(0,Number(e.target.value))})}/></label>}
     {action.behavior==='event'&&<label>{tr('Событие движка')}<select value={action.eventId||''} onChange={e=>patchAction({eventId:e.target.value})}><option value="">{tr('Выберите событие')}</option>{project.events.map(item=><option key={item.id} value={item.id}>{item.name}</option>)}</select></label>}
    </div>
    {action.behavior==='operation'&&<OperationFields project={project} call={action.operation} kind="event" expressionOptions={[["args.value","Значение действия ввода"]]} onChange={operation=>patchAction({operation})}/>}
    <section className="input-bindings-section" aria-label={tr('Привязки')}><div className="input-section-heading"><h3>{tr('Привязки')}</h3><label className="input-context-picker">{tr('Контекст')}<select value={context?.id||''} onChange={e=>setContext(e.target.value)}>{input.contexts.map(item=><option key={item.id} value={item.id}>{inputLabel(item)}</option>)}</select></label></div>
     {context?deviceGroups.map(([id,label,Icon])=>{
      const group=bindings.filter(binding=>groupFor(binding)===id);
      return group.length>0&&<section className="input-device-group" aria-label={tr(label)} key={id}><h4><Icon aria-hidden="true" size={15}/>{tr(label)}</h4><div className="input-binding-grid">{group.map(binding=><Binding key={binding.id} binding={binding} action={action} actions={input.actions} onChange={patch=>edit(settings=>Object.assign(settings.contexts.find(item=>item.id===context.id).bindings.find(item=>item.id===binding.id),patch))} onDelete={()=>edit(settings=>{const group=settings.contexts.find(item=>item.id===context.id);group.bindings=group.bindings.filter(item=>item.id!==binding.id);})}/>)}</div></section>;
     }):<p className="input-empty">{tr('Сначала создайте контекст во вкладке «Контексты».')}</p>}
     {context&&!bindings.length&&<p className="input-empty">{tr('В этом контексте пока нет привязок для действия.')}</p>}
     <div className="input-add-controls"><label><span className="input-sr-only">{tr('Устройство новой привязки')}</span><select value={bindingDevice} onChange={e=>setBindingDevice(e.target.value)}>{options(INPUT_DEVICES)}</select></label><button className="input-add-binding" disabled={!context} onClick={addBinding}><Plus aria-hidden="true" size={15}/>{tr('Добавить привязку')}</button></div>
    </section>
    <details className="input-advanced"><summary><Settings2 aria-hidden="true" size={15}/>{tr('Дополнительно')}<ChevronDown aria-hidden="true" size={14}/></summary><div className="input-fields">
     <label>{tr('Тип значения')}<select value={action.valueType} onChange={e=>patchAction({valueType:e.target.value,variable:'',variableX:'',variableY:'',behavior:action.behavior==='move'&&e.target.value!=='axis2d'?'event':action.behavior})}>{options(INPUT_TYPES)}</select></label>
     {(action.valueType==='axis2d'?['variableX','variableY']:['variable']).map(field=><label key={field}>{tr(field==='variableX'?'Переменная X':field==='variableY'?'Переменная Y':'Переменная значения')}<select value={action[field]||''} onChange={e=>patchAction({[field]:e.target.value})}><option value="">{tr('Не записывать')}</option>{Object.keys(project.variables).filter(id=>runtimeVariableType(project,id)===(action.valueType==='boolean'?'boolean':'number')).map(id=><option key={id} value={id}>{id}</option>)}</select></label>)}
    </div><label className="input-check" title={tr('Не передавать ввод контекстам с меньшим приоритетом.')}><input type="checkbox" checked={action.consume!==false} onChange={e=>patchAction({consume:e.target.checked})}/>{tr('Перехватывать ввод')}</label></details>
   </fieldset>:<div className="input-empty"><Keyboard aria-hidden="true" size={28}/><h2>{tr('Нет действий')}</h2><button disabled={running} onClick={addAction}><Plus aria-hidden="true" size={15}/>{tr('Создать действие')}</button></div>}</div>
  </div>}
  {tab==='contexts'&&<div className="input-editor-grid">
   <aside className="input-library"><div className="input-library-heading"><h2>{tr('Контексты')}</h2><button className="input-icon-button" aria-label={tr('Создать контекст')} title={tr('Создать контекст')} disabled={running} onClick={addContext}><Plus aria-hidden="true" size={17}/></button></div><label className="input-mobile-picker"><span className="input-sr-only">{tr('Выбрать контекст')}</span><select value={context?.id||''} onChange={e=>setContext(e.target.value)}>{input.contexts.map(item=><option key={item.id} value={item.id}>{inputLabel(item)}</option>)}</select></label><nav aria-label={tr('Контексты ввода')}>{input.contexts.map(item=><button key={item.id} aria-pressed={context?.id===item.id} onClick={()=>setContext(item.id)}><span className={'input-context-dot'+(activeContexts.includes(item.id)?' active':'')} aria-hidden="true"/><span><strong>{inputLabel(item)}</strong><small>{tr('Привязок: {0}',[item.bindings.length])}</small></span></button>)}</nav></aside>
   <div className="input-editor">{context?<fieldset className="input-edit-fields" disabled={running}><div className="input-section-heading"><h2>{inputLabel(context)}</h2>{activeContexts.includes(context.id)&&<span className="input-active-context">{tr('Активен сейчас')}</span>}{!['system','dialogue','gameplay'].includes(context.id)&&<button className="input-icon-button input-delete" aria-label={tr('Удалить контекст')} title={tr('Удалить контекст')} onClick={removeContext}><Trash2 aria-hidden="true" size={16}/></button>}</div>
    <div className="input-fields"><label>{tr('Название контекста')}<input ref={nameField} value={context.name} onChange={e=>patchContext({name:e.target.value})}/></label><label>{tr('Приоритет')}<input type="number" value={context.priority} onChange={e=>patchContext({priority:Number(e.target.value)})}/></label></div>
    {context.id==='system'&&<p className="input-note">{tr('Системный контекст действует всегда.')}</p>}
    <h3 className="input-context-actions-title">{tr('Действия в контексте')}</h3><div className="input-context-actions">{input.actions.filter(item=>context.bindings.some(binding=>binding.actionId===item.id)).map(item=><button key={item.id} onClick={()=>chooseAction(item.id,true)}><span><strong>{inputLabel(item)}</strong><small>{bindingSummary(context.bindings.filter(binding=>binding.actionId===item.id))}</small></span><ArrowRight aria-hidden="true" size={16}/></button>)}</div>
    {!context.bindings.length&&<p className="input-empty">{tr('Нет привязок. Выберите действие и добавьте его в этот контекст.')}</p>}
    <button onClick={()=>setTab('actions')}><Plus aria-hidden="true" size={15}/>{tr('Настроить привязки')}</button>
   </fieldset>:<div className="input-empty"><p>{tr('Нет контекстов')}</p><button disabled={running} onClick={addContext}>{tr('Создать контекст')}</button></div>}</div>
  </div>}
  {tab==='devices'&&<div className="input-devices-panel"><fieldset className="input-edit-fields" disabled={running}><h2><Smartphone aria-hidden="true" size={18}/>{tr('Экранное управление')}</h2><label className="input-touch-mode">{tr('Показывать кнопки и стик')}<select value={input.touchMode} onChange={e=>edit(settings=>settings.touchMode=e.target.value)}><option value="auto">{tr('На сенсорных устройствах')}</option><option value="always">{tr('Всегда')}</option><option value="off">{tr('Не показывать')}</option></select></label></fieldset>
   <section className="input-device-status"><h2><Gamepad2 aria-hidden="true" size={18}/>{tr('Геймпады')}</h2>{devices.length?<ul>{devices.map((device,index)=><li key={device.id||index}><span className="input-context-dot active" aria-hidden="true"/><strong>{device.name}</strong><small>{tr(device.mapping==='standard'?'Стандартная раскладка':'Своя раскладка')}</small></li>)}</ul>:<p>{tr('Нажмите кнопку на геймпаде, чтобы подключить его.')}</p>}</section>
   <details className="input-advanced input-device-monitor"><summary><Radio aria-hidden="true" size={15}/>{tr('Проверка ввода')}<ChevronDown aria-hidden="true" size={14}/></summary><div role="status"><p>{tr('Активные контексты')}: {activeContexts.map(id=>inputLabel(input.contexts.find(item=>item.id===id))).join(', ')||'—'}</p>{preview?.lastInput?<p>{preview.lastInput.name} · {preview.lastInput.phase} · {JSON.stringify(preview.lastInput.value)} · {preview.lastInput.device}</p>:<p>{tr('Запустите игру и нажмите назначенную клавишу.')}</p>}</div></details>
  </div>}
 </div>;
}
