import {t as tr, useLocale, message} from './i18n.jsx';
import React,{useState} from 'react';
import {Field,Select,Button} from './StudioParts.jsx';
import {ValueField} from './ChoiceInspector.jsx';
import {VALUE_TYPES,ANSWER_VARIABLE,typedValue,variableName} from './choiceModel.js';
export default function VariablesEditor({variables,onChange,disabled}){
 useLocale();
 const [name,setName]=useState(''),[type,setType]=useState('boolean'),[error,setError]=useState('');
 return <div className="choice-editor"><p className="choice-note">{tr("Переменные связывают ответы и проверки с действиями в графе событий.")}</p>{Object.entries(variables||{}).map(([id,value])=><Field key={id} label={variableName(id)}>{disabled?<span>{typeof value==='boolean'?(value?tr("Да"):tr("Нет")):String(value)}</span>:<ValueField label={tr("Переменная ")+id} type={typeof value} value={value} onChange={value=>onChange(id,value)}/>}</Field>)}
 {!disabled&&<><Field label={tr("Новая переменная")}><input aria-label={tr("Название новой переменной")} placeholder={tr("Например, дверь открыта")} value={name} onChange={e=>{setName(e.target.value);setError('');}}/></Field><Field label={tr("Тип переменной")}><Select value={type} options={VALUE_TYPES} onChange={setType}/></Field><Button icon="Plus" onClick={()=>{const id=name.trim();if(!id||id===ANSWER_VARIABLE||Object.hasOwn(variables,id)){setError('Введите новое уникальное название.');return;}onChange(id,typedValue('',type));setName('');}}>{tr("Создать переменную")}</Button>{error&&<p role="alert" className="choice-note">{message(error)}</p>}</>}
 </div>;
}
