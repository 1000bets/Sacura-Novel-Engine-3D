import React from 'react';
import CapabilityBindingFields from './CapabilityBindingFields.jsx';
import {GameSelect} from './GameplayCommandFields.jsx';
import {uid} from './model.js';
import {reference} from './valueSchemas.js';
export default function StateBindingFields({project,onChange}){
 const bindings=project.gameplay.stateBindings||[],edit=fn=>onChange(p=>fn(p.gameplay.stateBindings));
 return <details><summary>Значения возможностей для истории</summary><p>Выберите владельца и поле. Только выбранная переменная получает это значение.</p>{bindings.map(b=><fieldset key={b.id}><CapabilityBindingFields project={project} value={b} onChange={value=>{if(value)edit(list=>Object.assign(list.find(x=>x.id===b.id),value));}}/><GameSelect label="Переменная истории" value={b.variable?.id||''} options={[["","Выберите…"],...project.gameplay.variableBindings.map(v=>[v.id,v.key])]} onChange={id=>edit(list=>{list.find(x=>x.id===b.id).variable=reference('variable',id);})}/><button type="button" onClick={()=>edit(list=>list.splice(list.findIndex(x=>x.id===b.id),1))}>Убрать привязку</button></fieldset>)}<button type="button" onClick={()=>edit(list=>list.push({id:uid('state-binding'),variable:reference('variable','')}))}>Добавить привязку</button></details>;
}
