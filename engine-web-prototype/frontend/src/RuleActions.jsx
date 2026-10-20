import React from 'react';
import {GameSelect,GameField} from './GameplayCommandFields.jsx';
import {OperationFields} from './SchemaFields.jsx';
import FunctionArguments from './FunctionArguments.jsx';
import {referenceLabel} from './referencePresentation.js';
import {JOIN_LABELS} from './studioModel.js';
export default function RuleActions({project,value=[],onChange,onEvent,onFunction,expressionOptions=[],onCreateDialogue,scope}){
 const [dialogue,setDialogue]=React.useState(''),[createFor,setCreateFor]=React.useState(null);
 const update=(index,action)=>onChange(value.map((v,i)=>i===index?action:v));
 const choices=(items,type)=>[['','Выберите…'],...items.map(x=>[x.id,referenceLabel(project,type,x)])];
 return <>{value.map((action,index)=><section className="mechanics-action" key={index} aria-label={"Действие "+(index+1)}><strong>Действие {index+1}</strong><GameSelect label="Выполнить" value={action.kind} options={[["operation","Операцию"],["event","Event"],["function","Функцию"],["story","Продолжить историю"]]} onChange={kind=>update(index,{kind})}/>
 {action.kind==='operation'&&<div className="mechanics-action-settings"><OperationFields project={project} scope={scope} call={action.call} expressionOptions={expressionOptions} onChange={call=>update(index,{...action,call})}/></div>}
 {action.kind==='event'&&<><GameSelect label="Событие" value={action.eventId||''} options={choices(project.events,'event')} onChange={eventId=>update(index,{...action,eventId})}/><GameSelect label="Ожидание события" value={action.join||'FLOW_END'} options={Object.entries(JOIN_LABELS)} onChange={join=>update(index,{...action,join})}/>{action.eventId&&<button type="button" onClick={()=>onEvent?.(action.eventId)}>Открыть действия события</button>}</>}
 {action.kind==='function'&&<><GameSelect label="Функция" value={action.functionId||''} options={choices(project.functions,'function')} onChange={functionId=>update(index,{...action,functionId,args:{}})}/>{action.functionId&&<><FunctionArguments project={project} functionId={action.functionId} value={action.args} expressionOptions={expressionOptions} onChange={args=>update(index,{...action,args})}/><button type="button" onClick={()=>onFunction?.(action.functionId)}>Открыть функцию</button></>}</>}
 {action.kind==='story'&&<><GameSelect label="Продолжить историю" value={action.beatId||''} options={choices(project.chapters.flatMap(c=>c.beats),'beat')} onChange={beatId=>update(index,{...action,beatId})}/>{onCreateDialogue&&<details open={createFor===index}><summary onClick={e=>{e.preventDefault();setCreateFor(createFor===index?null:index);}}>Создать диалог</summary><GameField label="Текст нового диалога"><textarea value={dialogue} onChange={e=>setDialogue(e.target.value)}/></GameField><button type="button" disabled={!dialogue.trim()} onClick={()=>{onCreateDialogue(dialogue,index);setDialogue('');setCreateFor(null);}}>Добавить диалог</button></details>}</>}
 {!['operation','event','function','story'].includes(action.kind)&&<p>Действие сохранено. Его параметры доступны в исходной механике.</p>}
 <button type="button" disabled={!index} onClick={()=>{const next=[...value];[next[index-1],next[index]]=[next[index],next[index-1]];onChange(next);}}>Раньше</button><button type="button" onClick={()=>onChange(value.filter((_,i)=>i!==index))}>Убрать действие</button></section>)}<button type="button" onClick={()=>onChange([...value,{kind:'event',eventId:'',join:'FLOW_END'}])}>Добавить действие</button></>;
}
