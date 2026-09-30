import StoryDestination from './StoryDestination.jsx';
import React from 'react';
import {Handle,Position,useUpdateNodeInternals} from '@xyflow/react';
import {Icon} from './StudioParts.jsx';
import {ValueField} from './ChoiceInspector.jsx';
import {VALUE_TYPES,typedValue} from './choiceModel.js';
export default function NodeAnswer({choice:c,index,beatId,onChange,onRemove,onDisconnect,available,data}){
 const update=useUpdateNodeInternals();
 const type=c.result?.type||'string',result=type!=='none';
 return <section className="node-answer" onClick={e=>e.stopPropagation()} aria-label={'Ответ '+(index+1)}>
  <header className="node-answer-heading"><strong>Ответ {index+1}</strong><button title={'Удалить ответ '+(index+1)} aria-label={'Удалить ответ '+(index+1)} onClick={onRemove}><Icon name="Trash2" size={14}/></button></header>
  <label className="node-answer-text">Текст для игрока<textarea rows={2} aria-label={'Текст ответа '+(index+1)+' '+beatId} value={c.label} onChange={e=>onChange({label:e.target.value})}/></label>
  <div className="node-answer-enabled"><Handle className="data-pin" type="target" position={Position.Left} id={'enabled:'+c.id} title={'Доступность ответа '+(index+1)+' · вход Да / нет'}/><div><strong>Доступность</strong><span>{c.enabledSource?'По условию · Да / Нет':'Всегда доступен'}</span></div>{c.enabledSource&&<button title={'Отключить условие ответа '+(index+1)} onClick={()=>onChange({enabledSource:null})}><Icon name="Unplug" size={14}/></button>}</div>
  <details className="node-answer-settings" onToggle={()=>update(beatId)}><summary><Icon name="ArrowRight" size={13}/>{result?'Возвращает: '+(typeof c.result?.value==='boolean'?(c.result.value?'Да':'Нет'):String(c.result?.value??c.label)):'Без возвращаемого значения'}<Icon name="ChevronDown" size={13}/></summary><div><label>Тип результата<select aria-label={'Результат ответа '+(index+1)+' '+beatId} value={type} onChange={e=>onChange({result:e.target.value==='none'?{type:'none'}:{type:e.target.value,value:typedValue(c.result?.value??c.label,e.target.value)}})}><option value="none">Без результата</option>{VALUE_TYPES.map(([v,label])=><option key={v} value={v}>{label}</option>)}</select></label>{result&&<label>Вернуть значение<ValueField label={'Значение ответа '+(index+1)+' '+beatId} type={type} value={c.result?.value??c.label} onChange={value=>onChange({result:{type,value}})}/></label>}<p>Значение можно прочитать через «Последний ответ игрока».</p></div></details>
  <div className="node-answer-route"><span><strong>Продолжение</strong>{c.next?<StoryDestination data={data} source={beatId} port={'choice:'+c.id} target={c.next}/>:<small>После выбора</small>}</span>{c.next&&<button title={'Отключить переход ответа '+(index+1)} onClick={onDisconnect}><Icon name="Unplug" size={14}/></button>}<Handle type="source" position={Position.Right} id={'choice:'+c.id} title={c.label+' · продолжение ответа'}/></div>
 </section>;
}
