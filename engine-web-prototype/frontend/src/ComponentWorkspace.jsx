import React,{useEffect,useId,useRef,useState} from 'react';
import {Plus,Code2,Download,Link2,X,ChevronRight} from 'lucide-react';
import ComponentCodeEditor from './ComponentCodeEditor.jsx';
import SchemaFields from './SchemaFields.jsx';
import {createComponentResource,logicResource} from './componentResources.js';
import {addMechanicInstance} from './mechanicDescriptions.js';
import {componentFunctionOwner,componentInstanceName,componentOwnerName} from './componentCatalog.js';
import './components.css';

export default function ComponentWorkspace({project,onChange,requestedResourceId,onSelectResource,requestedInstanceId,requestedFunctionId,createRequest,onCreateFinished,scope,disabled,onBack,onExport}){
 const [scopeValue,setScopeValue]=useState((scope?.kind||'project')+':'+(scope?.id||project.id));
 const resources=(project.logicResources||[]).filter(resource=>resource.kind==='component.code');
 const [selected,setSelected]=useState(requestedResourceId||resources[0]?.id||''),[creating,setCreating]=useState(!!createRequest),[name,setName]=useState('Новый компонент'),[error,setError]=useState(''),[settings,setSettings]=useState(false),[instanceId,setInstance]=useState('');
 const panelId=useId(),connectionsButton=useRef(null),propertiesPanel=useRef(null);
 useEffect(()=>{if(requestedInstanceId)setInstance(requestedInstanceId);},[requestedInstanceId]);
 useEffect(()=>{if(scope?.kind&&scope?.id)setScopeValue(scope.kind+':'+scope.id);},[scope?.kind,scope?.id]);
 useEffect(()=>{if(createRequest)setCreating(true);},[createRequest]);
 useEffect(()=>{if(requestedResourceId){setSelected(requestedResourceId);setCreating(false);}},[requestedResourceId]);
 useEffect(()=>{if(requestedFunctionId&&!createRequest){const owner=componentFunctionOwner(project,requestedFunctionId);if(owner&&(!requestedResourceId||owner.definition.logicResourceId===requestedResourceId)){setSelected(owner.definition.logicResourceId);setCreating(false);}}},[requestedFunctionId]);
 useEffect(()=>{if(settings)propertiesPanel.current?.focus();},[settings]);
 const r=logicResource(project,selected),d=r&&project.gameplay.mechanicDefinitions.find(d=>d.id===r.definitionId&&d.logicResourceId===r.id);
 const instances=project.gameplay.mechanics.filter(m=>m.definitionId===d?.id&&m.definitionVersion===d?.version),m=instances.find(m=>m.id===instanceId);
 const requestedMethod=requestedFunctionId&&componentFunctionOwner(project,requestedFunctionId);
 const edit=fn=>{if(disabled)return;try{onChange(fn);setError('');}catch(e){setError(e.message);}};
 const create=e=>{e.preventDefault();let id;edit(p=>{id=createComponentResource(p,{name}).id;});if(id){setSelected(id);onSelectResource?.(id);setCreating(false);setSettings(false);setInstance('');onCreateFinished?.();}};
 const add=()=>{let id;edit(p=>{const colon=scopeValue.indexOf(':');id=addMechanicInstance(p,d.id,{kind:scopeValue.slice(0,colon),id:scopeValue.slice(colon+1)},{},d.version).id;});if(id)setInstance(id);};
 const closeSettings=()=>{setSettings(false);connectionsButton.current?.focus();};
 return <section className="component-workspace" aria-label="Компоненты проекта">
  <div className="component-workspace-body">
  <nav className="component-resource-list" aria-label="Исходные ресурсы логики">
   <div className="component-list-heading"><span>Компоненты <small>{resources.length}</small></span><button className="component-icon-button" type="button" disabled={disabled} title="Создать компонент" aria-label="Создать компонент" onClick={()=>setCreating(true)}><Plus aria-hidden="true" size={16}/></button></div>
   <div className="component-resource-items">{resources.map(resource=><button className="component-resource-item" type="button" key={resource.id} title={resource.path} aria-pressed={!creating&&selected===resource.id} onClick={()=>{setSelected(resource.id);onSelectResource?.(resource.id);setCreating(false);setInstance('');setSettings(false);}}><Code2 aria-hidden="true" size={16}/><span>{resource.name}</span><small>JS</small></button>)}</div>
  </nav>
  <div className="component-resource-content">
  {creating?<div className="component-start"><form className="component-create" onSubmit={create}>
   <Code2 className="component-start-icon" aria-hidden="true" size={24}/><h2>Новый компонент</h2><span className="component-language">JavaScript</span>
   <label>Название компонента<input autoFocus aria-label="Название компонента" value={name} onChange={e=>setName(e.target.value)} required disabled={disabled}/></label>
   <div className="component-form-actions"><button className="component-primary" type="submit" disabled={disabled||!name.trim()}>Создать</button><button className="component-quiet" type="button" onClick={()=>{setCreating(false);onCreateFinished?.();}}>Отмена</button></div>
  </form></div>:r&&d&&r.kind==='component.code'?<div className={'component-editor-layout'+(settings?' has-properties':'')}>
   <ComponentCodeEditor key={r.id} project={project} resourceId={r.id} onChange={edit} disabled={disabled} onBack={onBack} backLabel="Назад к механикам" methodId={requestedMethod?.definition?.logicResourceId===r.id?requestedMethod.methodId:undefined} toolbar={<>
    <button ref={connectionsButton} className="component-connections-toggle component-quiet" type="button" aria-label="Подключения компонента" aria-expanded={settings} aria-controls={settings?panelId:undefined} title="Параметры и подключения" onClick={()=>setSettings(!settings)}><Link2 aria-hidden="true" size={15}/><span>Подключения</span><span className="component-count">{instances.length}</span></button>
    {onExport&&<button className="component-icon-button" type="button" title="Экспортировать компонент" aria-label="Экспортировать компонент" onClick={()=>onExport(r,d,m)}><Download aria-hidden="true" size={16}/></button>}
   </>}/>
   {settings&&<aside ref={propertiesPanel} tabIndex={-1} id={panelId} className="component-properties" aria-label="Свойства компонента" onKeyDown={e=>{if(e.key==='Escape'){e.preventDefault();e.stopPropagation();closeSettings();}}}>
    <header><h3>Подключения <span className="component-count">{instances.length}</span></h3><button className="component-icon-button" type="button" aria-label="Закрыть параметры компонента" title="Закрыть" onClick={closeSettings}><X aria-hidden="true" size={16}/></button></header>
    <div className="component-properties-body">
     <fieldset disabled={disabled} className="component-attach"><label>Добавить к<select aria-label="Владелец экземпляра" value={scopeValue} onChange={e=>setScopeValue(e.target.value)}><option value={'project:'+project.id}>Весь проект</option>{project.objects.map(o=><option key={o.id} value={'object:'+o.id}>Объект · {o.name}</option>)}{project.subscenes.map(s=><option key={s.id} value={'scene:'+s.id}>Сцена · {s.name}</option>)}</select></label><button type="button" aria-label="Добавить экземпляр" onClick={add}><Plus aria-hidden="true" size={14}/>Добавить</button></fieldset>
     {instances.length>0&&<section className="component-instance-settings"><label>Подключение<select aria-label="Подключение определения" value={m?.id||''} onChange={e=>setInstance(e.target.value)}><option value="">Выберите подключение</option>{instances.map(x=><option key={x.id} value={x.id}>{componentOwnerName(project,x.scope)} · {componentInstanceName(project,x)}</option>)}</select></label>
      {m&&<fieldset disabled={disabled} className="component-instance-fields" aria-label="Параметры экземпляра"><SchemaFields project={project} scope={m.scope} schema={{...d.parameters,properties:Object.fromEntries(Object.entries(d.parameters.properties).filter(([key])=>!m.fieldBindings?.some(b=>b.target==='parameters'&&b.field===key)))}} value={m.values} onChange={values=>edit(p=>{p.gameplay.mechanics.find(x=>x.id===m.id).values=values;})}/><SchemaFields project={project} scope={m.scope} schema={d.references||{type:'object',properties:{}}} value={m.references} onChange={references=>edit(p=>{p.gameplay.mechanics.find(x=>x.id===m.id).references=references;})}/>{!Object.keys(d.parameters.properties).length&&!Object.keys(d.references?.properties||{}).length&&<p className="component-empty-parameters">Нет параметров</p>}</fieldset>}
     </section>}
     <details className="component-resource-settings"><summary><ChevronRight aria-hidden="true" size={14}/>Свойства файла</summary><label>Путь ресурса<input aria-label="Путь ресурса" disabled={disabled} value={r.path} onChange={e=>{const path=e.target.value;if(path&&!path.split(/[\\/]/).includes('..'))edit(p=>{logicResource(p,r.id).path=path;});}}/></label><p>Общий код для всех подключений.</p></details>
    </div>
   </aside>}
  </div>:r&&r.kind!=='component.code'?<div className="component-start"><div className="component-create"><Code2 className="component-start-icon" aria-hidden="true" size={24}/><h2>{r.name}</h2><p role="status">Доступен только JavaScript. Данные сохранены в прежнем формате.</p><div className="component-form-actions">{onBack&&<button type="button" onClick={onBack}>Назад к механикам</button>}{onExport&&d&&<button type="button" onClick={()=>onExport(r,d)}>Экспортировать компонент</button>}</div></div></div>:<div className="component-start"><div className="component-create"><Code2 className="component-start-icon" aria-hidden="true" size={24}/><h2>Компоненты проекта</h2><p>Логика на JavaScript.</p><button className="component-primary" type="button" disabled={disabled} onClick={()=>setCreating(true)}><Plus aria-hidden="true" size={15}/>Создать компонент</button></div></div>}
  {error&&<p className="component-workspace-error" role="alert">{error}</p>}
  </div>
  </div>
 </section>;
}
