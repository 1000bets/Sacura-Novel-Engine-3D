import React from 'react';
import {Button,Field,Select,Icon} from './StudioParts.jsx';
import {uid} from './studioModel.js';
import {ANSWER_VARIABLE,VALUE_TYPES,OPERATORS,variableName,availabilityOf,conditionSummary,typedValue,newChoice} from './choiceModel.js';
import './choiceInspector.css';

export function ValueField({value,type,onChange,label='Значение'}){
 return type==='boolean'?<Select aria-label={label} value={String(typedValue(value,type))} options={[["true","Да"],["false","Нет"]]} onChange={v=>onChange(v==='true')}/>:<input aria-label={label} type={type==='number'?'number':'text'} step={type==='number'?'any':undefined} value={value??''} onChange={e=>onChange(type==='number'?Number(e.target.value):e.target.value)}/>;
}
export function ConditionFields({condition,onChange,variables,label='Условие'}){
 const rules=condition?.rules||[];
 const change=(index,patch)=>onChange({...condition,rules:rules.map((r,i)=>i===index?{...r,...patch}:r)});
 return <div className="condition-fields" aria-label={label}>
  {rules.length>1&&<Field label="Когда срабатывает"><Select value={condition.mode||'all'} options={[["all","Все проверки выполнены"],["any","Хотя бы одна выполнена"]]} onChange={mode=>onChange({...condition,mode})}/></Field>}
  {rules.map((r,i)=><div className="condition-rule" key={i}><div className="choice-row"><strong>Проверка {i+1}</strong><Button icon="X" title={'Убрать проверку '+(i+1)} onClick={()=>onChange({...condition,rules:rules.filter((_,n)=>n!==i)})}/></div>
   <Field label="Что проверить"><Select aria-label={label+' · переменная '+(i+1)} value={r.variable||''} options={[["","Выберите переменную"],...(r.variable===ANSWER_VARIABLE||Object.hasOwn(variables||{},ANSWER_VARIABLE)?[[ANSWER_VARIABLE,variableName(ANSWER_VARIABLE)]]:[]),...Object.keys(variables||{}).filter(id=>id!==ANSWER_VARIABLE).map(id=>[id,variableName(id)]),...(!Object.hasOwn(variables||{},r.variable)&&r.variable&&r.variable!==ANSWER_VARIABLE?[[r.variable,r.variable+' · не найдена']]:[])]} onChange={variable=>change(i,{variable,type:variable===ANSWER_VARIABLE?r.type||'string':typeof variables[variable],value:variable===ANSWER_VARIABLE?'':typeof variables[variable]==='boolean'?true:typeof variables[variable]==='number'?0:''})}/></Field>
   <Field label="Тип значения"><Select value={r.type||'string'} options={VALUE_TYPES} onChange={type=>change(i,{type,value:typedValue(r.value,type),operator:'eq'})}/></Field>
   <Field label="Сравнение"><Select aria-label={label+' · сравнение '+(i+1)} value={r.operator||'eq'} options={OPERATORS.filter(([op])=>r.type==='number'||['eq','ne'].includes(op))} onChange={operator=>change(i,{operator})}/></Field>
   <Field label="С чем сравнить"><ValueField label={label+' · значение '+(i+1)} type={r.type||'string'} value={r.value} onChange={value=>change(i,{value})}/></Field>
   {r.variable&&r.variable!==ANSWER_VARIABLE&&!Object.hasOwn(variables||{},r.variable)&&<p className="choice-note">Переменная не найдена. Выберите существующую.</p>}
  </div>)}
  <Button icon="Plus" onClick={()=>onChange({mode:condition?.mode||'all',rules:[...rules,{variable:'',operator:'eq',type:'boolean',value:true}]})}>Добавить проверку</Button>
 </div>;
}
export default function ChoiceInspector({beat,project,onChange,onEnsureVariable}){
 const update=(id,patch)=>onChange({choices:beat.choices.map(c=>c.id===id?{...c,...patch}:c)});
 return <div className="choice-editor"><p className="choice-note">Ответы принадлежат только этой реплике. Каждый возвращает значение; дальнейшую реакцию задайте связями на графе.</p>
 <Field label="Сохранить ответ в переменную"><input aria-label="Переменная результата ответа" placeholder="Необязательно · например, решение" value={beat.resultVariable||''} onChange={e=>onChange({resultVariable:e.target.value})} onBlur={e=>{const id=e.target.value.trim();onChange({resultVariable:id});if(id)onEnsureVariable(id,typedValue('',beat.choices[0]?.result?.type||'string'));}}/></Field>
 <p className="choice-note">Результат хранится на выходе этой ноды выбора. Подключите его напрямую к проверке или ко входу Set переменной.</p>
 {(beat.choices||[]).map((c,i)=>{const availability=availabilityOf(c),result=c.result||{type:'string',value:c.label};return <section className="local-choice" key={c.id} aria-label={'Ответ '+(i+1)}><div className="choice-row"><strong>Ответ {i+1}</strong><Button icon="Trash2" title={'Удалить ответ '+(i+1)} onClick={()=>onChange({choices:beat.choices.filter(x=>x.id!==c.id)})}/></div>
 <Field label="Текст для игрока"><input aria-label={'Текст ответа '+(i+1)} value={c.label} onChange={e=>update(c.id,{label:e.target.value})}/></Field>
 <div className="choice-result"><Icon name="ArrowRight" size={14}/><strong>Вернуть значение</strong></div>
 <Field label="Тип результата"><Select aria-label={'Тип результата ответа '+(i+1)} value={result.type} options={VALUE_TYPES} onChange={type=>update(c.id,{result:{type,value:typedValue(result.value,type)}})}/></Field>
 <Field label="Значение результата"><ValueField label={'Результат ответа '+(i+1)} type={result.type} value={result.value} onChange={value=>update(c.id,{result:{...result,value}})}/></Field>
 <label className="check"><input type="checkbox" checked={!!availability} onChange={e=>update(c.id,{condition:'always',availability:e.target.checked?{mode:'all',rules:[{variable:'',operator:'eq',type:'boolean',value:true}]}:null})}/>Доступен по условию</label>
 {availability&&<ConditionFields label={'Ответ '+(i+1)} condition={availability} variables={project.variables} onChange={availability=>update(c.id,{availability,condition:'always'})}/>}
 <p className="choice-note"><Icon name={availability?'LockKeyhole':'Check'} size={12}/>{conditionSummary(availability)}</p>
 <p className="choice-route"><Icon name="GitBranch" size={13}/>{c.next?'Отдельный выход подключён':'Продолжит по выходу «После выбора»'}</p>
 </section>;})}
 <Button icon="Plus" onClick={()=>onChange({choices:[...(beat.choices||[]),newChoice(uid('choice'),'Ответ '+((beat.choices?.length||0)+1))]})}>Добавить свой ответ</Button>
 </div>;
}
