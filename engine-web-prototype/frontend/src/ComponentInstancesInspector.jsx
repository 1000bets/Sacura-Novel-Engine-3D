import {ReflectedComponents} from './ReflectedFields.jsx';
import React,{useId,useState} from 'react';
import {ChevronRight,Code2,Copy,Plus,Puzzle,Search,Trash2,X} from 'lucide-react';
import MechanicFields from './MechanicFields.jsx';
import {mechanicsFor,mechanicDefinitions,addMechanic} from './mechanicsModel.js';
import {mechanicLabel} from './mechanicsPresentation.js';
import SchemaFields,{SchemaValue} from './SchemaFields.jsx';
import {addMechanicInstance,descriptionFor} from './mechanicDescriptions.js';
import {duplicateInstance} from './authoredMechanics.js';
import {componentInstanceName} from './componentCatalog.js';
import {componentStateView,componentParameterView} from './componentFieldBindings.js';
import {reference,validateValue} from './valueSchemas.js';
import ComponentCallFields from './ComponentCallFields.jsx';
import {installLockedDoor} from './componentExamples.js';
import MeshInspector from './MeshInspector.jsx';
import {hasStaticMesh,canAddStaticMesh} from './staticMeshComponent.js';
import './components.css';
import './componentInspector.css';

function BoundFields({project,definition,instance,target,onChange}){
 const schema=target==='state'?definition.stateSchema:definition.parameters;
 return Object.entries(schema.properties).map(([field,s])=>{
  const binding=instance.fieldBindings?.find(b=>b.target===target&&b.field===field);
  const variables=project.gameplay.variableBindings.filter(v=>!validateValue(s,project.variables[v.key],{project,allowDeferred:true}).length);
  const edit=variable=>onChange(p=>{const m=p.gameplay.mechanics.find(x=>x.id===instance.id);m.fieldBindings=(m.fieldBindings||[]).filter(b=>b.target!==target||b.field!==field);if(variable)m.fieldBindings.push({target,field,variable:reference('variable',variable)});});
  return <div key={field}><label className="component-property-field"><span>{s.label||field}</span><select aria-label={'Постоянная привязка '+target+' '+field} value={binding?.variable?.id||''} onChange={e=>edit(e.target.value)}><option value="">Данные экземпляра</option>{binding&&!variables.some(v=>v.id===binding.variable.id)&&<option value={binding.variable.id}>Переменная недоступна</option>}{variables.map(v=><option key={v.id} value={v.id}>{v.key}</option>)}</select></label>{binding&&<small className="component-binding-note">Текущее значение истории · только чтение. Меняйте переменную-источник.</small>}</div>;
 });
}

function WebComponentInstancesInspector({project,scope,onChange,onOpen,preview,running,onCreate,onOpenFunction,onMeshChange,allowMeshUpload=true}){
 const [selected,setSelected]=useState(''),[type,setType]=useState(''),[query,setQuery]=useState(''),[searching,setSearching]=useState(false),[adding,setAdding]=useState(false),[error,setError]=useState('');
 const id=useId(),owns=m=>m.scope?.kind===scope.kind&&m.scope.id===scope.id;
 const definitions=project.gameplay.mechanicDefinitions.filter(d=>d.componentVersion===1&&d.scopes.includes(scope.kind));
 const compatible=mechanicDefinitions(project).filter(d=>!d.componentVersion&&d.scopes.includes(scope.kind));
 const instances=project.gameplay.mechanics.filter(m=>owns(m)&&descriptionFor(project,m.definitionId,m.definitionVersion)?.componentVersion===1);
 const native=mechanicsFor(project).filter(m=>owns(m)&&descriptionFor(project,m.definitionId,m.definitionVersion)?.componentVersion!==1);
 const m=instances.find(m=>m.id===selected),d=m&&descriptionFor(project,m.definitionId,m.definitionVersion),nativeSelected=native.find(m=>m.id===selected);
 const edit=fn=>{if(running)return;try{onChange(fn);setError('');}catch(e){setError(e.message);}};
 const add=()=>edit(p=>{
  if(type==='__static-mesh'){
   p.objects.find(o=>o.id===scope.id).staticMesh=true;
   setSelected('__static-mesh');setAdding(false);setType('');setQuery('');setSearching(false);
   return;
  }
  let record;
  if(type==='__locked-door'){
   const definition=installLockedDoor(p);record=addMechanicInstance(p,definition.id,scope);
   p.objects.find(o=>o.id===scope.id).interactionCall={id:'sacura.component.call',version:1,parameters:{instance:reference('mechanic',record.id),methodId:'interact',args:{}}};
  }else{
   const definition=definitions.find(d=>type===d.id+'@'+d.version);
   record=definition?addMechanicInstance(p,definition.id,scope,{},definition.version):addMechanic(p,type,scope);
  }
  setSelected(record.id);setAdding(false);setType('');setQuery('');setSearching(false);
 });
 const object=scope.kind==='object'&&project.objects.find(o=>o.id===scope.id),mesh=hasStaticMesh(object),count=instances.length+native.length+(mesh?1:0);
 const state=()=>{try{return componentStateView(project,m,d,preview?.world?.game?.mechanicStates?.[m.id]||{},preview?.variables||project.variables);}catch(e){return {error:e.message};}};
 const row=(record,isNative=false)=>{
  const definition=!isNative&&descriptionFor(project,record.definitionId,record.definitionVersion),name=isNative?(record.name||mechanicLabel(project,record)):componentInstanceName(project,record),subtitle=isNative?'Системный':definition.name;
  if(!(name+' '+subtitle).toLowerCase().includes(query.toLowerCase()))return null;
  return <React.Fragment key={record.id}><button className="component-instance-row" type="button" key={record.id} aria-expanded={record.id===selected} aria-controls={record.id===selected?id+'-properties':undefined} onClick={()=>setSelected(record.id===selected?'':record.id)}><ChevronRight size={13} aria-hidden="true"/><Puzzle size={15} aria-hidden="true"/><span><strong>{name}</strong>{subtitle!==name&&<small>{subtitle}</small>}</span>{record.enabled===false&&<small className="component-disabled-label">Выкл.</small>}</button>{record.id===selected&&properties}</React.Fragment>;
 };
 const sharedCount=d&&project.gameplay.mechanics.filter(x=>x.definitionId===d.id&&x.definitionVersion===d.version).length;
 const parameterValues=m&&d?componentParameterView(project,m,d,preview?.variables||project.variables):{};
 const properties=(m||nativeSelected)&&<div id={id+'-properties'} className="component-instance-properties">
   {nativeSelected&&<><fieldset disabled={running} className="component-parameter-fields"><MechanicFields compact project={project} mechanic={nativeSelected} onChange={edit} onFunction={onOpenFunction}/></fieldset><div className="component-implementation-actions">{Object.entries(nativeSelected.implementation?.functions||{}).map(([key,functionId])=><button type="button" key={functionId} onClick={()=>onOpenFunction?.(functionId,nativeSelected.id)}><Code2 size={14} aria-hidden="true"/>{project.functions.find(f=>f.id===functionId)?.name||key}</button>)}{nativeSelected.definitionId&&descriptionFor(project,nativeSelected.definitionId,nativeSelected.definitionVersion)?.logic?.functionId&&<button type="button" onClick={()=>onOpenFunction?.(descriptionFor(project,nativeSelected.definitionId,nativeSelected.definitionVersion).logic.functionId,nativeSelected.id)}>Открыть реализацию</button>}</div>{running&&<details className="component-inspector-details"><summary>Состояние</summary><pre>{JSON.stringify(nativeSelected.type==='health'?preview?.world?.game?.actors?.[scope.id]:nativeSelected.type==='weapon'?preview?.world?.game?.weapons?.[nativeSelected.recordId]:{},null,2)}</pre></details>}</>}
   {m&&d&&<>
    <div className="component-instance-identity"><label className="component-property-field"><span>Имя</span><input aria-label="Имя подключения" disabled={running} value={m.name||d.name} onChange={e=>edit(p=>{p.gameplay.mechanics.find(x=>x.id===m.id).name=e.target.value;})}/></label><label className="component-enabled"><input type="checkbox" disabled={running} checked={m.enabled!==false} onChange={e=>edit(p=>{p.gameplay.mechanics.find(x=>x.id===m.id).enabled=e.target.checked;})}/>Включён</label></div>
    <fieldset disabled={running} className="component-parameter-fields" aria-label="Параметры экземпляра">{Object.entries(d.parameters.properties).filter(([key])=>!m.fieldBindings?.some(b=>b.target==='parameters'&&b.field===key)).map(([key,s])=><SchemaValue key={key} project={project} schema={s} label={s.label||key} scope={m.scope} parameters={parameterValues} value={parameterValues[key]} onChange={value=>edit(p=>{p.gameplay.mechanics.find(x=>x.id===m.id).values[key]=value;})}/>)}<SchemaFields project={project} scope={m.scope} schema={d.references||{type:'object',properties:{}}} value={m.references||{}} onChange={references=>edit(p=>{p.gameplay.mechanics.find(x=>x.id===m.id).references=references;})}/></fieldset>
    {(Object.keys(d.parameters.properties).length>0||Object.keys(d.stateSchema.properties).length>0)&&<details className="component-inspector-details"><summary>Привязки к истории</summary><fieldset disabled={running}>{Object.keys(d.parameters.properties).length>0&&<><h4>Параметры</h4><BoundFields project={project} definition={d} instance={m} target="parameters" onChange={edit}/></>}{Object.keys(d.stateSchema.properties).length>0&&<><h4>Состояние</h4><BoundFields project={project} definition={d} instance={m} target="state" onChange={edit}/></>}</fieldset></details>}
    {running&&preview?.world?.game&&<section className="component-debug-state" aria-label="Состояние компонента"><h4>Состояние · только чтение</h4>{Object.entries(state()).map(([key,value])=><p key={key}><span>{d.stateSchema.properties[key]?.label||key}</span><output aria-label={'Состояние '+key}>{typeof value==='object'?JSON.stringify(value):String(value)}</output></p>)}</section>}
    <div className="component-implementation-actions"><button type="button" className="component-open-source" aria-label="Открыть общую реализацию" onClick={()=>onOpen?.(d.logicResourceId,m.id)}><Code2 size={15} aria-hidden="true"/>Открыть реализацию</button><button type="button" className="component-icon-button" aria-label="Создать независимый вариант" title="Создать независимый вариант" disabled={running} onClick={()=>edit(p=>{const copy=duplicateInstance(p,p.gameplay.mechanics.find(x=>x.id===m.id),{independent:true});setSelected(copy.id);onOpen?.(descriptionFor(p,copy.definitionId,copy.definitionVersion).logicResourceId,copy.id);})}><Copy size={15} aria-hidden="true"/></button><button type="button" className="component-icon-button component-remove" aria-label="Убрать подключение" title="Убрать подключение" disabled={running} onClick={()=>edit(p=>{p.gameplay.mechanics=p.gameplay.mechanics.filter(x=>x.id!==m.id);setSelected('');})}><Trash2 size={15} aria-hidden="true"/></button></div>
    <small className="component-source-scope">Общий исходник · подключений: {sharedCount}</small>
   </>}
  </div>;
 const meshMatches='статический меш static mesh'.includes(query.toLowerCase());
 return <section className="component-instances" aria-label="Компоненты владельца">
  <header className="component-inspector-heading"><h3>Компоненты <span>{count}</span></h3>{count>0&&<button type="button" className="component-icon-button" aria-label="Поиск компонентов" aria-expanded={searching} onClick={()=>{setSearching(!searching);setQuery('');}}><Search size={15} aria-hidden="true"/></button>}</header>
  {searching&&<label className="component-inspector-search"><Search size={14} aria-hidden="true"/><input autoFocus type="search" aria-label="Поиск компонентов" placeholder="Название или тип…" value={query} onChange={e=>setQuery(e.target.value)}/></label>}
  <div className="component-instance-list">{mesh&&meshMatches&&<React.Fragment><button className="component-instance-row" type="button" aria-expanded={selected==='__static-mesh'} aria-controls={selected==='__static-mesh'?id+'-mesh':undefined} onClick={()=>setSelected(selected==='__static-mesh'?'':'__static-mesh')}><ChevronRight size={13} aria-hidden="true"/><Puzzle size={15} aria-hidden="true"/><span><strong>Статический меш</strong><small>Системный</small></span></button>{selected==='__static-mesh'&&<div id={id+'-mesh'} className="component-instance-properties"><MeshInspector object={object} disabled={running} allowUpload={allowMeshUpload} onChange={onMeshChange}/><div className="component-implementation-actions"><button type="button" className="component-icon-button component-remove" aria-label="Удалить компонент Статический меш" title="Удалить компонент" disabled={running} onClick={()=>edit(p=>{p.objects.find(o=>o.id===scope.id).staticMesh=false;setSelected('');})}><Trash2 size={15} aria-hidden="true"/></button></div></div>}</React.Fragment>}{instances.map(record=>row(record))}{native.map(record=>row(record,true))}</div>
  {!count&&<p className="component-empty">Нет компонентов</p>}
  {query&&!(mesh&&meshMatches)&&!instances.some(m=>(componentInstanceName(project,m)+' '+descriptionFor(project,m.definitionId,m.definitionVersion).name).toLowerCase().includes(query.toLowerCase()))&&!native.some(m=>((m.name||mechanicLabel(project,m))+' Системный').toLowerCase().includes(query.toLowerCase()))&&<p className="component-empty">Ничего не найдено</p>}

  {!running&&(adding?<div className="component-add-form"><header><strong>Добавить компонент</strong><button type="button" className="component-icon-button" aria-label="Отменить добавление компонента" onClick={()=>{setAdding(false);setType('');}}><X size={14} aria-hidden="true"/></button></header><label className="component-property-field"><span>Компонент</span><select autoFocus aria-label="Добавить компонент к владельцу" value={type} onChange={e=>setType(e.target.value)}><option value="">Выберите компонент…</option>{definitions.length>0&&<optgroup label="Компоненты проекта">{definitions.map(d=><option key={d.id+'@'+d.version} value={d.id+'@'+d.version}>{d.name}{d.version!==1?' · v'+d.version:''}</option>)}</optgroup>}{scope.kind==='object'&&<optgroup label="Готовые примеры"><option value="__locked-door">Дверь с замком</option></optgroup>}{scope.kind==='object'&&canAddStaticMesh(object)&&!mesh&&<optgroup label="Системные компоненты"><option value="__static-mesh">Статический меш</option></optgroup>}{compatible.length>0&&<optgroup label="Системные возможности">{compatible.map(d=><option key={d.id} value={d.id}>{d.name}</option>)}</optgroup>}</select></label><div className="component-add-actions"><button type="button" aria-label="Подключить компонент" disabled={!type} onClick={add}><Plus size={14} aria-hidden="true"/>Добавить</button>{onCreate&&<button type="button" onClick={()=>{setAdding(false);setType('');onCreate();}}>Создать новый…</button>}</div></div>:<button type="button" className="component-add-button" onClick={()=>setAdding(true)}><Plus size={15} aria-hidden="true"/>Добавить компонент</button>)}
  {object&&<details className="component-inspector-details component-interaction"><summary>Взаимодействие</summary><fieldset disabled={running}><ComponentCallFields project={project} call={object.interactionCall||{id:'sacura.component.call',version:1,parameters:{}}} allowLifetime={false} onChange={call=>edit(p=>{p.objects.find(o=>o.id===scope.id).interactionCall=call;})}/>{object.interactionCall&&<button type="button" onClick={()=>edit(p=>{delete p.objects.find(o=>o.id===scope.id).interactionCall;})}>Убрать вызов</button>}</fieldset></details>}
  {error&&<p role="alert">{error}</p>}
 </section>;
}

export default function ComponentInstancesInspector(props){return props.reflection?<ReflectedComponents {...props}/>:<WebComponentInstancesInspector {...props}/>;}
