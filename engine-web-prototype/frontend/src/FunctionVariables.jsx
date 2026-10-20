import {componentFunctionOwner} from './componentCatalog.js';
import React,{useState} from 'react';
import {Plus,PanelLeftClose,ArrowRight,ArrowLeft,Trash2} from 'lucide-react';
import {parameterSchema} from './authoredMechanics.js';
import {schemaFromValue} from './valueSchemas.js';
import {schemaTypeLabel} from './referencePresentation.js';
import {SchemaValue} from './SchemaFields.jsx';
import {GameField,GameSelect} from './GameplayCommandFields.jsx';
import {fieldTypes,schemaType} from './TypedProperties.jsx';
import {t as tr} from './i18n.jsx';
import './blueprintVariables.css';

const groupLabels={args:'Параметры',local:'Локальные переменные',state:'Состояние экземпляра',global:'Переменные проекта'};
function DeclarationName({value,onChange}){
 const [draft,setDraft]=useState(value),cancel=React.useRef(false);React.useEffect(()=>setDraft(value),[value]);
 return <input value={draft} onChange={e=>setDraft(e.target.value)} onBlur={()=>{if(cancel.current){cancel.current=false;return;}if(draft!==value)onChange(draft);}} onKeyDown={e=>{if(e.key==='Enter'){e.preventDefault();e.currentTarget.blur();}if(e.key==='Escape'){e.stopPropagation();cancel.current=true;setDraft(value);e.currentTarget.blur();}}}/>;
}
export function functionDeclarations(project,fn){
 const result=[];
 for(const p of fn?.parameters||[])result.push({path:'args.'+p.name,key:p.name,scope:'args',label:p.label||p.schema?.label||p.name,schema:parameterSchema(p),value:p.default??p.schema?.default});
 const schemas={local:fn?.localSchema||schemaFromValue(fn?.locals||{}),state:componentFunctionOwner(project,fn?.id)?.definition.stateSchema||fn?.stateSchema||{properties:{}}};
 for(const [scope,schema]of Object.entries(schemas))for(const [key,s]of Object.entries(schema.properties||{}))result.push({path:scope+'.'+key,key,scope,label:s.label||key,schema:s,value:(scope==='local'?fn.locals:fn.initialState)?.[key]??s.default});
 for(const [key,value]of Object.entries(project.variables||{}))result.push({path:'global.'+key,key,scope:'global',label:key,schema:schemaFromValue(value,project.variableTypes?.[key]),value});
 return result;
}
export function declarationLabel(project,fn,path){
 const declaration=functionDeclarations(project,fn).find(d=>path===d.path||path?.startsWith(d.path+'.'));
 if(!declaration)return String(path||'').split('.').slice(1).join(' · ');
 return declaration.label+(path===declaration.path?'':' · '+path.slice(declaration.path.length+1).split('.').join(' · '));
}
export default function FunctionVariables({project,fn,selected,onSelect,onClose,onAdd,onEdit,onRemove,onNode}){
 const [query,setQuery]=useState(''),[error,setError]=useState(''),declarations=functionDeclarations(project,fn),active=declarations.find(d=>d.path===selected),types=active?.scope==='global'?fieldTypes.filter(([type])=>['number','integer','boolean','string','collection'].includes(type)):fieldTypes;
 const perform=work=>{try{work();setError('');}catch(e){setError(e.message);}};
 return <section className="bp-variables function-variables" aria-label={tr('Мой Blueprint')}>
  <header><strong>{tr('Мой Blueprint')}</strong><button type="button" aria-label={tr('Свернуть переменные')} onClick={onClose}><PanelLeftClose size={16} aria-hidden="true"/></button></header>
  <input type="search" aria-label={tr('Поиск переменных')} placeholder={tr('Поиск переменных…')} value={query} onChange={e=>setQuery(e.target.value)}/>
  <div className="function-variable-groups">{Object.entries(groupLabels).map(([scope,label])=>{
   const rows=declarations.filter(d=>d.scope===scope&&d.label.toLocaleLowerCase().includes(query.toLocaleLowerCase()));
   return <details key={scope} open={scope==='args'||scope==='local'||!!query||active?.scope===scope}>
    <summary><span>{tr(label)}</span><small>{rows.length}</small>{['args','local'].includes(scope)&&<button type="button" aria-label={tr(scope==='args'?'Добавить параметр':'Добавить локальную переменную')} onClick={e=>{e.preventDefault();perform(()=>onSelect(onAdd(scope)));}}><Plus size={14} aria-hidden="true"/></button>}</summary>
    <div className="function-variable-list">{rows.map(d=><div className={'function-variable-row type-'+d.schema.type} key={d.path}>
     <button type="button" className="function-variable-name" aria-pressed={selected===d.path} draggable onDragStart={e=>{e.dataTransfer.setData('application/sacura-function-variable',d.path);e.dataTransfer.effectAllowed='copy';}} onClick={()=>{onSelect(d.path);setError('');}} onDoubleClick={()=>onNode(d.path,'get')}><i className="variable-type-dot"/><span>{d.label}</span><small>{tr(schemaTypeLabel(d.schema))}</small></button>
     <button type="button" title={tr('Читать')+' · '+d.label} aria-label={tr('Читать')+' · '+d.label} onClick={()=>onNode(d.path,'get')}><ArrowRight size={13} aria-hidden="true"/></button>
     {scope!=='args'&&<button type="button" title={tr('Записать')+' · '+d.label} aria-label={tr('Записать')+' · '+d.label} onClick={()=>onNode(d.path,'set')}><ArrowLeft size={13} aria-hidden="true"/></button>}
    </div>)}</div>
   </details>;
  })}</div>
  {active&&<div className="function-variable-details" key={active.path}>
   <strong>{active.label}</strong>
   <GameField label="Название переменной"><DeclarationName value={active.label} onChange={label=>perform(()=>onEdit(active,{label}))}/></GameField>
   <GameSelect label="Тип переменной" value={schemaType(active.schema)} options={types.some(([type])=>type===schemaType(active.schema))?types:[[schemaType(active.schema),schemaTypeLabel(active.schema)],...types]} onChange={type=>perform(()=>onEdit(active,{type}))}/>
   <SchemaValue project={project} parameters={Object.fromEntries(declarations.filter(d=>d.scope==='args').map(d=>[d.key,d.value]))} schema={active.schema} label="Значение по умолчанию" value={active.value} onChange={value=>perform(()=>onEdit(active,{value}))}/>
   <div className="bp-variable-actions"><button type="button" onClick={()=>onNode(active.path,'get')}>{tr('Читать')}</button>{active.scope!=='args'&&<button type="button" onClick={()=>onNode(active.path,'set')}>{tr('Записать')}</button>}</div>
   {['args','local'].includes(active.scope)&&<button type="button" className="bp-variable-delete" onClick={()=>perform(()=>{onRemove(active);onSelect('');})}><Trash2 size={13} aria-hidden="true"/>{tr('Удалить переменную')}</button>}
  </div>}
  {error&&<p role="alert">{error}</p>}
  <footer>{tr('Перетащите переменную на граф.')}<br/>{tr('Двойной щелчок: Get')}</footer>
 </section>;
}
