import React from 'react';
import {useLocale} from './i18n.jsx';
import {GameSelect,GameField,GameNumber} from './GameplayCommandFields.jsx';
import CapabilityBindingFields from './CapabilityBindingFields.jsx';
export default function WidgetBindingFields({element:e,project,onChange}){
 useLocale();const binding=(key,value)=>onChange({binding:{...e.binding,[key]:value}});
 return <><GameSelect label="Источник значения" value={e.binding?.source||''} options={[["","Статический текст"],["variable","Переменная проекта"],["capability","Состояние компонента / возможности"],...(e.binding?.source==='component'?[["component","Состояние компонента"]]:[]),...(e.binding?.source==='game'?[["game","Старая привязка состояния"]]:[])]} onChange={source=>onChange({binding:source==='capability'?{source}:source==='variable'?{source,path:Object.keys(project.variables)[0]||''}:null})}/>
 {['capability','component'].includes(e.binding?.source)&&<CapabilityBindingFields project={project} valueTypes={e.type==='inventory'?['collection']:e.type==='progress'?['number','integer']:undefined} value={e.binding} onChange={value=>onChange({binding:value?{...value,format:e.binding.format||''}:null})}/>}
 {e.binding?.source==='variable'&&<GameSelect label="Переменная проекта" value={e.binding.path} options={Object.keys(project.variables).map(id=>[id,id])} onChange={path=>binding('path',path)}/>}
 {e.binding?.source==='game'&&<details><summary>Старая привязка</summary><GameField label="Сохранённый путь поля"><input value={e.binding.path} onChange={event=>binding('path',event.target.value)}/></GameField></details>}
 {e.binding&&e.type!=='inventory'&&<GameField label="Формат текста"><input placeholder="Здоровье: {value}" value={e.binding.format||''} onChange={event=>binding('format',event.target.value)}/></GameField>}
 {e.type==='progress'&&<><GameNumber label="Минимум" value={e.min||0} min={-100000} onChange={min=>onChange({min})}/><GameNumber label="Максимум" value={e.max??100} min={.001} onChange={max=>onChange({max})}/><details><summary>Максимум из состояния</summary><CapabilityBindingFields project={project} valueTypes={['number','integer']} label="Максимум из возможности" value={e.maxBinding} onChange={maxBinding=>onChange({maxBinding,maxVariable:''})}/><GameSelect label="Максимум из переменной" value={e.maxVariable||''} options={[["","Постоянный максимум"],...Object.keys(project.variables).map(id=>[id,id])]} onChange={maxVariable=>onChange({maxVariable,maxBinding:null})}/></details></>}
 <GameSelect label="Видимость по переменной" value={e.showIf?.path||''} options={[["","Всегда"],...Object.keys(project.variables).map(id=>[id,id])]} onChange={path=>onChange({showIf:path?{source:'variable',path}:null})}/></>;
}
