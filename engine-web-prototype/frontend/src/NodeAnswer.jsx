import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import {Handle,Position,useUpdateNodeInternals} from '@xyflow/react';
import {Icon} from './StudioParts.jsx';
import {ValueField} from './ChoiceInspector.jsx';
import {VALUE_TYPES,typedValue} from './choiceModel.js';
export default function NodeAnswer({choice:c,index,beatId,onChange,onRemove,onDisconnect,available,data}){
 useLocale();
 const update=useUpdateNodeInternals();
 const type=c.result?.type||'string',result=type!=='none';
 return <section className="node-answer" onClick={e=>e.stopPropagation()} aria-label={tr("Ответ ")+(index+1)}>
  <header className="node-answer-heading"><strong>{tr("Ответ ")}{index+1}</strong><button title={tr("Удалить ответ ")+(index+1)} aria-label={tr("Удалить ответ ")+(index+1)} onClick={onRemove}><Icon name="Trash2" size={14}/></button></header>
  <label className="node-answer-text"><textarea rows={1} aria-label={tr("Текст ответа ")+(index+1)+' '+beatId} value={c.label} onChange={e=>onChange({label:e.target.value})}/></label>
  <div className="node-answer-enabled"><Handle className="data-pin" type="target" position={Position.Left} id={'enabled:'+c.id} title={tr("Доступность ответа ")+(index+1)+tr(" · вход Да / нет")}/><div><span>{c.enabledSource?tr("Доступность по условию"):tr("Всегда доступен")}</span></div>{c.enabledSource&&<button title={tr("Отключить условие ответа ")+(index+1)} onClick={()=>onChange({enabledSource:null})}><Icon name="Unplug" size={14}/></button>}</div>
  <details className="node-answer-settings" onToggle={()=>update(beatId)}><summary><Icon name="ArrowRight" size={13}/>{result?tr("Возвращает: ")+(typeof c.result?.value==='boolean'?(c.result.value?tr("Да"):tr("Нет")):String(c.result?.value??c.label)):tr("Без возвращаемого значения")}<Icon name="ChevronDown" size={13}/></summary><div><label>{tr("Тип результата")}<select aria-label={tr("Результат ответа ")+(index+1)+' '+beatId} value={type} onChange={e=>onChange({result:e.target.value==='none'?{type:'none'}:{type:e.target.value,value:typedValue(c.result?.value??c.label,e.target.value)}})}><option value="none">{tr("Без результата")}</option>{VALUE_TYPES.map(([v,label])=><option key={v} value={v}>{tr(label)}</option>)}</select></label>{result&&<label>{tr("Вернуть значение")}<ValueField label={tr("Значение ответа ")+(index+1)+' '+beatId} type={type} value={c.result?.value??c.label} onChange={value=>onChange({result:{type,value}})}/></label>}<p>{tr("Значение доступно на пине «Результат выбора» этой ноды.")}</p></div></details>
  <div className="node-answer-route"><span>{tr("Выход ответа")}</span>{c.next&&<button title={tr("Отключить переход ответа ")+(index+1)} onClick={onDisconnect}><Icon name="Unplug" size={12}/></button>}<Handle type="source" position={Position.Right} id={'choice:'+c.id} title={c.label+tr(" · продолжение ответа")}/></div>
 </section>;
}
