import {t as tr, useLocale, message} from './i18n.jsx';
import React,{useState} from 'react';
import {Icon} from './StudioParts.jsx';
import {ValueField} from './ChoiceInspector.jsx';
import {ANSWER_VARIABLE} from './choiceModel.js';
import {VARIABLE_TYPES,variableType,runtimeVariableType,variableReferences} from './variableModel.js';
import './blueprintVariables.css';

export default function VariableInspector({variable,project,preview,disabled,onEdit,onAddNode}){
 useLocale();
 const [draftName,setDraftName]=useState(variable),[error,setError]=useState('');
 const system=variable===ANSWER_VARIABLE,refs=variableReferences(project,variable);
 const edit=patch=>{const issue=onEdit(variable,patch);setError(issue||'');return !issue;};
 const rename=()=>{if(draftName.trim()!==variable)edit({name:draftName});};
 return <section className="bp-variable-details variable-inspector" aria-label={tr("Свойства переменной")}>
  <strong>{tr("Свойства переменной")}</strong>
  {system?<p>{tr("Системный результат последнего выбора.")}</p>:<label>{tr("Имя")}<input aria-label={tr("Имя выбранной переменной")} value={draftName} disabled={disabled} onChange={e=>setDraftName(e.target.value)} onBlur={rename} onKeyDown={e=>{if(e.key==='Enter')e.currentTarget.blur();}}/></label>}
  <label>{tr("Тип")}<select aria-label={tr("Тип выбранной переменной")} value={variableType(project,variable)} disabled={disabled} onChange={e=>edit({type:e.target.value})}>{VARIABLE_TYPES.map(([id,label])=><option key={id} value={id}>{tr(label)}</option>)}</select></label>
  {!system&&<label>{tr("Значение по умолчанию")}<fieldset disabled={disabled}><ValueField label={tr("Значение по умолчанию переменной")} type={runtimeVariableType(project,variable)} value={project.variables[variable]} onChange={value=>!disabled&&edit({value})}/></fieldset></label>}
  {preview?.variables&&<small>{tr("В игре: ")}{String(preview.variables[variable]??tr('не задано'))}</small>}
  <div className="bp-variable-actions"><button disabled={disabled} onClick={()=>onAddNode(variable,'variable')}>{tr("Get · Получить")}</button>{!system&&<button disabled={disabled} onClick={()=>onAddNode(variable,'set-variable')}>{tr("Set · Задать")}</button>}</div>
  {!system&&<button className="bp-variable-delete" disabled={disabled||refs.length>0} title={refs.length?tr("Используется в ")+refs.length+tr(" нодах или событиях"):''} onClick={()=>edit({remove:true})}><Icon name="Trash2" size={13}/>{tr("Удалить")}{refs.length>0&&<small> {tr("· используется: ")}{refs.length}</small>}</button>}
  {error&&<p role="alert">{message(error)}</p>}
 </section>;
}
