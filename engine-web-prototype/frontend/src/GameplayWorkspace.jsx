import ComponentWorkspace from './ComponentWorkspace.jsx';
import {componentFunctionOwner} from './componentCatalog.js';
import {schemaDefaults} from './valueSchemas.js';
import ExecutionJournal from './ExecutionJournal.jsx';
import React,{useState,useEffect,useRef} from 'react';
import AuthoredMechanicEditor,{CreateMechanic} from './AuthoredMechanicEditor.jsx';
import {duplicateInstance,planAuthoringChange} from './authoredMechanics.js';
import {Button} from './StudioParts.jsx';
import {GameSelect} from './GameplayCommandFields.jsx';
import {addMechanic,removeMechanic,mechanicsFor,mechanicDefinitions,mechanicDefinition} from './mechanicsModel.js';
import MechanicFields from './MechanicFields.jsx';
import HandlerProperties from './HandlerProperties.jsx';
import {ensureHandlerGraphs} from './handlerGraphs.js';
import {openOperationSource} from './operationSource.js';
import MechanicProjectProperties from './MechanicProjectProperties.jsx';
import MechanicScriptsWorkspace from './MechanicScriptsWorkspace.jsx';
import PackageExchange from './PackageExchange.jsx';
import {mechanicLabel,ownerLabel,searchable,ruleSummary,mechanicSource} from './mechanicsPresentation.js';
import {createRule} from './ruleModel.js';
import {validateStudio} from './studioModel.js';
import {uid} from './model.js';
import {createSignalDefinition} from './mechanicDescriptions.js';
import './gameplay.css';
import './mechanicsWorkspace.css';
import {t as tr,useLocale,message} from './i18n.jsx';
import {Plus,Search,Upload,Download,Copy,Unplug,Undo2,Redo2,CheckCheck,Play,Puzzle,Timer,DoorOpen,Package,Move,Camera,Heart,Crosshair,ScanEye,Box,Backpack,Hash,ListOrdered,MousePointer2,Zap,Settings2,Braces,FileBox,ChevronDown,Check} from 'lucide-react';
export {prepareGameplayScene} from './gameplayAuthoring.js';


const mechanicSessions=new Map();
const mechanicIcons={rule:Zap,timer:Timer,interaction:MousePointer2,door:DoorOpen,pickup:Package,movement:Move,camera:Camera,health:Heart,weapon:Crosshair,pursuit:ScanEye,physics:Box,inventory:Backpack,resource:Hash,crosshair:Crosshair,'sacura.turn':ListOrdered};
const shortDescriptions={
 rule:'Запустить действия по сигналу.',timer:'Выполнить действие через заданное время.',interaction:'Запустить событие при осмотре объекта.',door:'Открыть дверь, если найден ключ.',pickup:'Добавить предмет в инвентарь.',movement:'Управлять объектом клавишами или мышью.',camera:'Настроить ракурс и управление камерой.',health:'Задать здоровье выбранного объекта.',weapon:'Настроить стрельбу и боезапас.',pursuit:'Следовать за целью и атаковать рядом с ней.',physics:'Добавить гравитацию и столкновения.',inventory:'Хранить и использовать предметы.',resource:'Считать очки, монеты или предметы.',crosshair:'Показать прицел в игровом кадре.','sacura.turn':'Управлять порядком ходов.'
};
function mechanicGraph(project,record){if(!record)return '';const d=mechanicDefinition(record.definitionId||record.capabilityId||record.type,project,record.definitionVersion),rule=project.gameplay.rules.find(r=>r.id===record.ruleId);return [...Object.values(record.implementation?.functions||{}),d?.logic?.functionId,...Object.values(d?.methods||{}).map(m=>m.logic.functionId),rule?.handlerFunctionId,...(d?.rules||[]).map(r=>r.handlerFunctionId),...(d?.machine?.transitions||[]).map(r=>r.handlerFunctionId)].find(id=>project.functions.some(f=>f.id===id))||'';}
const DefinitionIcon=({id,size=20})=>{const Icon=mechanicIcons[id]||Puzzle;return <Icon aria-hidden="true" size={size}/>;};

export default function GameplayWorkspace({project,onChange,onImport,sceneId,selectedObjectId,addRequest,running,preview,onObject,onStop,onPlay,onUndo,onRedo,canUndo=false,canRedo=false,onWidgets,onInput,onEvent,onStory,onReturn}) {
 useLocale();
 const saved=mechanicSessions.get(project.id);
 const [scopeValue,setScope]=useState(saved?.scopeValue||(selectedObjectId?'object:'+selectedObjectId:'scene:'+sceneId)),[type,setType]=useState('timer'),[selected,setSelected]=useState(saved?.selected||''),[functionId,setFunction]=useState(saved?.functionId||''),[panel,setPanel]=useState(()=>saved?.panel||(mechanicsFor(project).length?'instance':'library')),[query,setQuery]=useState(''),[error,setError]=useState(''),[exchange,setExchange]=useState(null),[transfer,setTransfer]=useState(''),[checked,setChecked]=useState(false),[advanced,setAdvanced]=useState(false),[createScript,setCreateScript]=useState(0);
 const [componentLibrary,setComponentLibrary]=useState(false);
 const [resourceId,setResource]=useState(saved?.resourceId||''),[createComponent,setCreateComponent]=useState(0);
 const root=useRef(null),addButton=useRef(null),search=useRef(null),heading=useRef(null),returnFocus=useRef(false);
 const colon=scopeValue.indexOf(':'),scope={kind:scopeValue.slice(0,colon),id:scopeValue.slice(colon+1)},all=mechanicsFor(project),m=all.find(m=>m.id===selected)||all.find(m=>m.scope?.kind===scope.kind&&m.scope.id===scope.id),definition=m&&mechanicDefinition(m.definitionId||m.capabilityId||m.type,project,m.definitionVersion),available=[...mechanicDefinitions(project),...all.filter(r=>r.packageSource&&!r.definitionId).map(r=>({id:'package:'+r.id,name:r.packageSource.name+' · '+mechanicLabel(project,r),description:r.packageSource.description||mechanicDefinition(r.capabilityId||r.type,project)?.description,scopes:[r.scope.kind],packageSource:r.packageSource,sourceMechanic:r}))].filter(d=>d.scopes?.includes(scope.kind)&&(!componentLibrary||d.componentVersion===1)),matches=d=>searchable([d.name,d.description,definitionTitle(d),definitionDescription(d),...d.editor?.keywords||[]].join(' ')).includes(searchable(query)),library=available.filter(matches),actualType=library.some(d=>d.id===type)?type:library[0]?.id,chosen=library.find(d=>d.id===actualType),list=all.filter(record=>!mechanicDefinition(record.definitionId||record.type,project,record.definitionVersion)?.componentVersion).filter(record=>searchable(mechanicLabel(project,record)+' '+ownerLabel(project,record.scope)).includes(searchable(query))),rule=m&&project.gameplay.rules.find(r=>r.id===m.ruleId),issues=checked?validateStudio(project):[];
 const edit=fn=>{if(running)return;setError('');try{const next=structuredClone(project);fn(next);ensureHandlerGraphs(next);onChange(p=>Object.assign(p,next));}catch(e){setError(e.message);}};
 const openLibrary=()=>{setComponentLibrary(false);setExchange(null);setQuery('');setPanel('library');search.current?.focus();};
 const closeExchange=()=>{setExchange(null);heading.current?.focus();};
 const choose=record=>{setSelected(record.id);setScope(record.scope?.kind?record.scope.kind+':'+record.scope.id:'project:'+project.id);const id=mechanicGraph(project,record);if(id)setFunction(id);const d=mechanicDefinition(record.definitionId||record.type,project,record.definitionVersion);if(d?.logicResourceId){setResource(d.logicResourceId);setPanel('components');}else setPanel(id?'functions':'instance');setExchange(null);heading.current?.focus();};
 const openFunction=id=>{setFunction(id);const owner=componentFunctionOwner(project,id);if(owner?.definition.logicResourceId){setResource(owner.definition.logicResourceId);setPanel('components');}else setPanel('functions');};
 const openHandler=record=>{if(record?.handlerFunctionId){openFunction(record.handlerFunctionId);return;}let id;edit(p=>{ensureHandlerGraphs(p);id=p.gameplay.rules.find(r=>r.id===record.id)?.handlerFunctionId;});if(id)openFunction(id);};
 const implementationIds=Object.values(m?.implementation?.functions||{}),logicId=implementationIds[0]||definition?.logic?.functionId||rule?.handlerFunctionId;
 const openMechanicLogic=()=>{if(logicId)openFunction(logicId);else if(rule)openHandler(rule);else if(definition?.source)openOperationSource(definition).catch(e=>setError(e.message));else setPanel(panel==='logic'?'instance':'logic');};
 useEffect(()=>{const cached=mechanicSessions.get(project.id);if(cached){setScope(cached.scopeValue);setSelected(cached.selected);const record=mechanicsFor(project).find(m=>m.id===cached.selected),id=mechanicGraph(project,record);setFunction(cached.panel==='functions'?cached.functionId:id||cached.functionId);const definition=record?.definitionId&&mechanicDefinition(record.definitionId,project,record.definitionVersion);setResource(cached.resourceId||definition?.logicResourceId||'');setPanel(definition?.logicResourceId&&['instance','functions','logic','components'].includes(cached.panel)?'components':cached.panel==='instance'&&id?'functions':cached.panel);return;}const records=mechanicsFor(project),object=selectedObjectId&&project.objects.find(o=>o.id===selectedObjectId),candidates=object?records.filter(r=>r.scope?.kind==='object'&&r.scope.id===object.id):records.filter(r=>r.scope?.kind==='scene'&&r.scope.id===sceneId),pool=candidates.length?candidates:records,first=pool[0];const owner=first?.scope||(object?{kind:'object',id:object.id}:{kind:'scene',id:sceneId});setScope(owner.kind+':'+owner.id);setSelected(first?.id||'');const id=mechanicGraph(project,first);if(id)setFunction(id);const definition=first?.definitionId&&mechanicDefinition(first.definitionId,project,first.definitionVersion);if(definition?.logicResourceId){setResource(definition.logicResourceId);setPanel('components');}else setPanel(id?'functions':records.length?'instance':'library');setExchange(null);setQuery('');},[project.id]);
 useEffect(()=>{mechanicSessions.set(project.id,{scopeValue,selected,functionId,panel,resourceId});},[project.id,scopeValue,selected,functionId,panel,resourceId]);
 useEffect(()=>{if(selectedObjectId&&project.objects.some(o=>o.id===selectedObjectId)){const records=mechanicsFor(project).filter(r=>r.scope?.kind==='object'&&r.scope.id===selectedObjectId),record=records.find(r=>mechanicGraph(project,r))||records[0];if(record)choose(record);else{setScope('object:'+selectedObjectId);setSelected('');setPanel('instance');}}},[selectedObjectId]);
 useEffect(()=>{if(addRequest?.createComponent){setCreateComponent(n=>n+1);setPanel('components');}if(addRequest?.resourceId){setResource(addRequest.resourceId);setPanel('components');}if(addRequest&&project.objects.some(o=>o.id===addRequest.objectId)){setScope('object:'+addRequest.objectId);setPanel('library');setQuery('');}if(addRequest?.functionId)openFunction(addRequest.functionId);if(addRequest?.mechanicId){setSelected(addRequest.mechanicId);if(!addRequest.functionId){const record=all.find(m=>m.id===addRequest.mechanicId);if(record)choose(record);}}},[addRequest]);
 useEffect(()=>{if(panel==='library')search.current?.focus();else if(returnFocus.current){returnFocus.current=false;addButton.current?.focus();}else heading.current?.focus();},[panel,exchange]);
 useEffect(()=>{if(panel==='instance'&&definition?.logicResourceId){setResource(definition.logicResourceId);setPanel('components');}},[panel,definition?.logicResourceId]);
 useEffect(()=>{setChecked(false);},[project]);
 useEffect(()=>{setAdvanced(false);},[m?.id]);
 const missingOwner=m&&(!m.scope||!['object','scene','project'].includes(m.scope.kind)||(m.scope.kind==='project'?m.scope.id!==project.id:!(m.scope.kind==='object'?project.objects:project.subscenes).some(x=>x.id===m.scope.id)));
 const scopes=[['project:'+project.id,tr('Весь проект')],...project.subscenes.map(s=>['scene:'+s.id,tr('Сцена · {0}',[s.name])]),...project.objects.map(o=>['object:'+o.id,tr('Объект · {0}',[o.name])])];
 const copy=independent=>{try{const plan=planAuthoringChange(project,p=>{const record=duplicateInstance(p,m,{independent});if(m.packageSource)record.packageSource=structuredClone(m.packageSource);return record;});edit(p=>{plan.apply(p);const id=mechanicGraph(p,plan.record);setSelected(plan.record.id);if(id)setFunction(id);const definition=plan.record.definitionId&&mechanicDefinition(plan.record.definitionId,p,plan.record.definitionVersion);if(definition?.logicResourceId){setResource(definition.logicResourceId);setPanel('components');}else setPanel(id?'functions':'instance');});}catch(e){setError(e.message);}};
 const createDialogue=(text,index,branch)=>{const id=uid('line');edit(p=>{let chapter=p.chapters.find(c=>c.subsceneId===sceneId);if(!chapter){chapter={id:uid('chapter'),name:'Диалоги',subsceneId:sceneId,beats:[]};p.chapters.push(chapter);}chapter.beats.push({id,kind:'dialogue',speaker:'Рассказчик',text,next:null,bindings:[],mode:'SEQUENTIAL',batches:{BEFORE:[],ON_START:[],AFTER:[]}});p.gameplay.rules.find(r=>r.id===rule.id)[branch][index].beatId=id;});};
 const addReaction=()=>{let functionId;edit(p=>{const record=p.gameplay.mechanics.find(x=>x.id===m.id);const r=createRule({signal:m.type==='pickup'?'pickup':'interact',scope:m.scope,filter:m.scope.kind==='object'?{[m.type==='pickup'?'source':'target']:{kind:'reference',type:'object',mode:'context',role:'self'}}:{}});p.gameplay.rules.push(r);record.ruleId=r.id;ensureHandlerGraphs(p);functionId=r.handlerFunctionId;});if(functionId)openFunction(functionId);};
 function localDefinition(d){return !!d.sourceMechanic||project.gameplay.mechanicDefinitions.some(item=>item.id===d.id?.split('@')[0]);}
 function definitionTitle(d){return localDefinition(d)?d.name:tr(d.name);}
 function definitionDescription(d){return localDefinition(d)?d.description:tr(shortDescriptions[d.id]||d.description);}
 const addSelected=()=>{if(!chosen)return;if(chosen.sourceMechanic)setExchange({root:chosen.sourceMechanic,kind:'mechanic',copy:true,targetScope:scope});else edit(p=>{const record=addMechanic(p,actualType,scope);setSelected(record.id);const id=mechanicGraph(p,record);if(id)setFunction(id);const definition=mechanicDefinition(record.definitionId||record.type,p,record.definitionVersion);if(definition?.logicResourceId){setResource(definition.logicResourceId);setPanel('components');}else setPanel(id?'functions':'instance');setQuery('');});};
 const parameters=m?<div className="mechanics-parameters">
      {definition&&<p className="mechanics-description">{definitionDescription(definition)}</p>}
      <MechanicFields compact project={project} mechanic={m} onChange={edit} onEvent={onEvent} onFunction={openFunction}/>
      {rule&&<p className="mechanics-rule-summary">{ruleSummary(project,rule)}</p>}
      {m.definitionId&&!definition?.componentVersion&&<AuthoredMechanicEditor project={project} mechanic={m} onChange={edit} onFunction={openFunction} onEvent={onEvent}/>}
      <details className="mechanics-instance-advanced" open={advanced}><summary onClick={e=>{e.preventDefault();setAdvanced(!advanced);}}>{tr('Дополнительные свойства экземпляра')}<ChevronDown aria-hidden="true" size={14}/></summary>
       {m.definitionId&&<button type="button" onClick={()=>copy(false)}>{tr('Копия с общим определением')}</button>}
       {m.packageSource&&<p>{tr('Источник: {0} · {1} · {2}',[m.packageSource.name,m.packageSource.author,m.packageSource.version])}</p>}
       {advanced&&!rule&&!m.definitionId&&!m.native&&<button type="button" onClick={addReaction}>{tr('Добавить реакцию')}</button>}
       {advanced&&<MechanicFields advancedOnly project={project} mechanic={m} onChange={edit} onEvent={onEvent} onFunction={openFunction}/>}
      </details>
     </div>:null;
 const instanceProperties=m?<div className="blueprint-instance-properties"><h3>{mechanicLabel(project,m)}</h3><p>{ownerLabel(project,m.scope)}</p><div className="mechanics-tools"><button type="button" onClick={()=>setExchange({root:m,kind:'mechanic'})}><Download size={14} aria-hidden="true"/>{tr('Экспортировать')}</button><button type="button" onClick={()=>m.definitionId?copy(true):setExchange({root:m,kind:'mechanic',copy:true})}><Copy size={14} aria-hidden="true"/>{tr('Создать копию')}</button></div>{parameters}</div>:undefined;
 const title=exchange?(exchange.copy?tr('Создать копию'):exchange.root?tr('Экспортировать'):tr('Импортировать')):panel==='library'?tr('Добавить механику'):panel==='functions'?tr('Скрипты механик'):panel==='properties'?tr('Свойства проекта'):panel==='transfer'?tr('Events и шаблоны'):m?mechanicLabel(project,m):all.length?tr('Выберите механику'):tr('Добавьте первую механику');
 return <div ref={root} className="gameplay-workspace mechanics-workbench" onKeyDown={e=>{if(e.key==='Escape'&&!running){e.preventDefault();if(exchange)closeExchange();else{returnFocus.current=true;setPanel('instance');addButton.current?.focus();}}}}>
  {project.description&&<details className="component-lesson"><summary>Разбор проекта и упражнения</summary><p style={{whiteSpace:'pre-wrap'}}>{project.description}</p><div className="mechanics-tools">{(project.logicResources||[]).filter(r=>r.lesson).map(r=><button type="button" key={r.id} onClick={()=>{setResource(r.id);setPanel('components');}}>Открыть {r.name}</button>)}</div></details>}
  <header className="mechanics-toolbar"><strong>{tr(panel==='functions'&&!exchange?'Скрипты механик':'Механики')}</strong><div className="mechanics-tools">
   <Button className="mechanics-icon-button" disabled={!canUndo||running} onClick={onUndo} title={tr('Отменить')} aria-label={tr('Отменить')}><Undo2 aria-hidden="true" size={16}/></Button>
   <Button className="mechanics-icon-button" disabled={!canRedo||running} onClick={onRedo} title={tr('Повторить')} aria-label={tr('Повторить')}><Redo2 aria-hidden="true" size={16}/></Button>
   <Button disabled={running} onClick={()=>{setPanel('components');setCreateComponent(n=>n+1);setExchange(null);}}><Plus aria-hidden="true" size={15}/>Создать компонент</Button><Button disabled={running} onClick={()=>{openLibrary();setComponentLibrary(true);}}>Добавить компонент</Button>
   <Button disabled={running} onClick={()=>setExchange({})}><Upload aria-hidden="true" size={15}/>Импортировать пакет</Button>
   {panel!=='functions'&&panel!=='components'&&<Button disabled={running} onClick={()=>{setPanel('functions');setExchange(null);setCreateScript(n=>n+1);}}><Plus aria-hidden="true" size={15}/>Добавить скрипт</Button>}
   <span className="mechanics-toolbar-divider" aria-hidden="true"/>
   <Button disabled={running} onClick={()=>setChecked(true)}><CheckCheck aria-hidden="true" size={15}/>{tr('Проверить')}</Button>
   {onPlay&&<Button className="mechanics-primary" disabled={running} onClick={onPlay}><Play aria-hidden="true" size={14}/>{tr('Запустить')}</Button>}
   {running&&<Button onClick={onStop}>{tr('Остановить для редактирования')}</Button>}
  </div></header>
  <fieldset disabled={running} className={'mechanics-content'+(panel==='components'?' mechanics-component-mode':'')}><div className={'mechanics-layout'+(panel==='functions'&&!exchange?' mechanics-function-layout':'')}>
   <aside className="mechanics-catalog" aria-label={tr('Установленные механики')}>
    <div className="mechanics-target"><GameSelect label="Добавить к" value={scopeValue} options={scopes} onChange={value=>{setScope(value);setSelected('');}}/>
     <button ref={addButton} aria-label={tr('Добавить механику')} title={tr('Добавить механику')} className={'mechanics-add mechanics-primary'+(panel==='library'?' active':'')} type="button" onClick={openLibrary}><Plus aria-hidden="true" size={16}/><span>{tr('Добавить механику')}</span></button>
    </div>
    <label className="mechanics-search"><Search aria-hidden="true" size={15}/><span className="mechanics-sr-only">{tr('Поиск механик')}</span><input ref={search} type="search" placeholder={tr('Найти механику…')} value={query} onChange={e=>setQuery(e.target.value)}/></label>
    <button type="button" onClick={()=>{setPanel('components');setExchange(null);}}>Определения компонентов</button><div className="mechanics-list-heading"><h3>Прежние подключения</h3><span>{list.length}</span></div>
    {all.length>0&&<div className="mechanics-mobile-picker"><GameSelect label="Выбрать механику" value={list.some(x=>x.id===m?.id)?m.id:''} options={[["",tr('Выберите…')],...list.map(record=>[record.id,mechanicLabel(project,record)])]} onChange={id=>{const record=all.find(item=>item.id===id);if(record)choose(record);}}/></div>}
    <div className="mechanics-list" onKeyDown={e=>{if(!['ArrowDown','ArrowUp','Home','End'].includes(e.key))return;const buttons=[...e.currentTarget.querySelectorAll('button')],index=buttons.indexOf(document.activeElement);if(index<0)return;e.preventDefault();buttons[e.key==='Home'?0:e.key==='End'?buttons.length-1:(index+(e.key==='ArrowDown'?1:-1)+buttons.length)%buttons.length]?.focus();}}>
     {list.map(record=><button type="button" key={record.id} aria-pressed={!exchange&&panel!=='library'&&m?.id===record.id} onClick={()=>choose(record)}><DefinitionIcon id={record.capabilityId||record.type} size={16}/><span><strong>{mechanicLabel(project,record)}</strong><small>{ownerLabel(project,record.scope)}</small></span></button>)}
    </div>
    {!list.length&&<p className="mechanics-list-empty">{all.length?tr('Ничего не найдено'):tr('Пока нет механик')}</p>}
    <div className="mechanics-sidebar-tools"><button type="button" onClick={()=>setExchange({})}><Upload aria-hidden="true" size={15}/>{tr('Импортировать')}</button>
     <details className="mechanics-more"><summary><Settings2 aria-hidden="true" size={15}/>{tr('Дополнительно')}<ChevronDown aria-hidden="true" size={13}/></summary><div>
      <button type="button" onClick={()=>{setExchange(null);setPanel('components');}}><Braces size={15}/>Компоненты проекта</button>
      <button type="button" onClick={()=>{setExchange(null);setPanel('properties');}}><Settings2 aria-hidden="true" size={15}/>{tr('Свойства проекта')}</button>
      <button type="button" onClick={()=>{setExchange(null);setPanel('functions');}}><Braces aria-hidden="true" size={15}/>{tr('Скрипты механик')}</button>
      <button type="button" onClick={()=>{setExchange(null);setPanel('transfer');}}><FileBox aria-hidden="true" size={15}/>{tr('Events и шаблоны')}</button>
     </div></details>
    </div>
   </aside>
   <section className={'mechanics-inspector mechanics-panel-'+panel} aria-label={tr('Редактор механики')}>
    <h2 ref={heading} className={panel==='functions'&&!exchange?'mechanics-sr-only':undefined} tabIndex={-1}>{title}</h2>
    {exchange?<PackageExchange project={project} root={exchange.root} kind={exchange.kind} onImport={onImport} onChange={edit} copy={exchange.copy} targetScope={exchange.targetScope} initialName={exchange.kind==='mechanic'&&exchange.root?mechanicLabel(project,exchange.root):undefined} onApplied={plan=>{const ids=new Set(all.map(m=>m.id));const added=plan.project.gameplay.mechanics.find(m=>!ids.has(m.id));if(added){setSelected(added.id);setScope(added.scope.kind+':'+added.scope.id);}const id=mechanicGraph(plan.project,added);if(id)setFunction(id);const definition=added?.definitionId&&mechanicDefinition(added.definitionId,plan.project,added.definitionVersion);const importedSource=plan.project.logicResources?.find(r=>!project.logicResources?.some(old=>old.id===r.id));if(definition?.logicResourceId||importedSource){setResource(definition?.logicResourceId||importedSource.id);setPanel('components');}else setPanel(id?'functions':'instance');}} onClose={closeExchange}/>:panel==='library'?<>
     {library.length?<>
      <div className="mechanics-ready-picker"><GameSelect label={componentLibrary?"Компонент":"Готовая механика"} value={actualType} options={library.map(d=>[d.id,definitionTitle(d)])} onChange={setType}/><p>{chosen&&definitionDescription(chosen)}</p></div>
      <ul className="mechanics-library" aria-label={tr('Библиотека механик')}>{library.map(d=><li key={d.id}><button type="button" aria-pressed={actualType===d.id} onClick={()=>setType(d.id)}>
       <span className="mechanics-card-top"><span className="mechanics-card-icon"><DefinitionIcon id={d.id}/></span>{actualType===d.id&&<Check aria-hidden="true" size={16}/>}</span>
       <strong>{definitionTitle(d)}</strong><span className="mechanics-card-description">{definitionDescription(d)}</span>
       {mechanicSource(project,d)?<small className="mechanics-source">{tr('Импортировано')}</small>:localDefinition(d)?<small className="mechanics-source">{tr('Локальная заготовка')}</small>:null}
      </button></li>)}</ul>
      <div className="mechanics-add-bar"><div><strong>{chosen&&definitionTitle(chosen)}</strong><small>{ownerLabel(project,scope)}</small></div><button className="mechanics-primary" type="button" disabled={!chosen} onClick={addSelected}><Plus aria-hidden="true" size={16}/>{tr('Добавить')}</button></div>
     </>:<div className="mechanics-empty"><Search aria-hidden="true" size={25}/><h3>{tr('Механики не найдены')}</h3><p>{tr('Измените запрос или импортируйте заготовку.')}</p><button type="button" onClick={()=>setQuery('')}>{tr('Очистить поиск')}</button></div>}
     <CreateMechanic project={project} selected={m} onChange={edit} onCreated={id=>{setType(id);setQuery('');}}/>
    </>:panel==='components'?<ComponentWorkspace project={project} disabled={running} requestedResourceId={resourceId} onSelectResource={setResource} requestedInstanceId={m?.id} requestedFunctionId={functionId} createRequest={createComponent} onCreateFinished={()=>setCreateComponent(0)} scope={scope} onChange={edit} onBack={()=>onReturn?onReturn():setPanel('instance')} onEvent={onEvent} onExport={(resource,d)=>setExchange({root:d,kind:'component'})}/>:panel==='functions'?<MechanicScriptsWorkspace onFunction={openFunction} project={project} disabled={running} scope={scope} createRequest={createScript} onCreated={(id,recordId)=>{setCreateScript(0);setFunction(id);if(recordId)setSelected(recordId);}} requestedId={functionId} instanceProperties={instanceProperties} onChange={edit} onBack={()=>onReturn?onReturn():setPanel(m?'instance':'library')} onEvent={onEvent} breadcrumb={m?mechanicLabel(project,m)+' · '+ownerLabel(project,m.scope):undefined} onExport={fn=>{setFunction(fn.id);setExchange({root:fn,kind:'function'});}}/>:panel==='properties'?<MechanicProjectProperties project={project} onChange={edit} onInput={onInput} onWidgets={onWidgets} onStory={onStory}/>:panel==='transfer'?<div className="mechanics-transfer-panel">
     <GameSelect label="Событие для экспорта" value={transfer} options={[["","Выберите…"],...project.events.map(e=>[e.id,e.name])]} onChange={setTransfer}/>
     <div className="mechanics-tools"><button type="button" disabled={!transfer} onClick={()=>setExchange({root:project.events.find(e=>e.id===transfer),kind:'event'})}><Download aria-hidden="true" size={15}/>{tr('Экспортировать событие')}</button><button type="button" onClick={()=>setExchange({root:project,kind:'template'})}><FileBox aria-hidden="true" size={15}/>{tr('Экспортировать шаблон проекта')}</button></div>
    </div>:m?<>
     <p className="mechanics-owner">{ownerLabel(project,m.scope)}</p>
     {!definition&&<p role="alert">{tr('Определение недоступно. Исходные данные сохранены.')}</p>}
     {missingOwner&&<p role="alert">{tr('Владелец механики удалён. Привязка сохранена; выберите новый объект при импорте копии.')}</p>}
     <div className="mechanics-instance-toolbar"><button className={'mechanics-logic-toggle'+(panel==='logic'?' active':'')} type="button" aria-pressed={panel==='logic'} onClick={openMechanicLogic}><Braces aria-hidden="true" size={15}/>{panel==='logic'?tr('К параметрам экземпляра'):tr('Изменить логику')}</button><div className="mechanics-tools">
      <button type="button" onClick={()=>setExchange({root:m,kind:'mechanic'})}><Download aria-hidden="true" size={15}/>{tr('Экспортировать')}</button>
      <button type="button" onClick={()=>m.definitionId?copy(true):setExchange({root:m,kind:'mechanic',copy:true})}><Copy aria-hidden="true" size={15}/>{tr('Создать копию')}</button>
      <button type="button" className="mechanics-icon-button mechanics-danger" title={tr('Отключить механику')} aria-label={tr('Отключить механику')} onClick={()=>edit(p=>{removeMechanic(p,m);setSelected('');setPanel(all.length<=1?'library':'instance');})}><Unplug aria-hidden="true" size={16}/></button>
     </div></div>
     {panel==='logic'?m.definitionId?<AuthoredMechanicEditor expanded project={project} mechanic={m} onChange={edit} onFunction={openFunction} onEvent={onEvent}/>:<>
      {implementationIds.length>0&&<div className="graph-handler-list">{Object.entries(m.implementation.functions).map(([key,id])=><button type="button" key={key} onClick={()=>openFunction(id)}><span><strong>{project.functions.find(f=>f.id===id)?.name||key}</strong><small>{tr('Редактируемая реализация')}</small></span><Braces size={16}/></button>)}</div>}
      {rule?<><button type="button" onClick={()=>openHandler(rule)}><Braces size={16}/>{tr('Открыть граф обработчика')}</button><HandlerProperties project={project} rule={rule} onChange={patch=>edit(p=>Object.assign(p.gameplay.rules.find(r=>r.id===rule.id),patch))}/></>:<>
       {!implementationIds.length&&<p>{tr('Настройте реакцию на сигнал.')}</p>}{!m.native&&definition&&<button type="button" onClick={addReaction}><Plus aria-hidden="true" size={15}/>{tr('Добавить реакцию')}</button>}
      </>}{m.eventId&&<button type="button" onClick={()=>onEvent?.(m.eventId)}>{tr('Открыть Event механики')}</button>}
     </>:parameters}
    </>:<div className="mechanics-empty"><Puzzle aria-hidden="true" size={32}/><h3>{tr('Выберите готовую механику')}</h3><button type="button" onClick={openLibrary}><Plus aria-hidden="true" size={16}/>{tr('Открыть библиотеку')}</button></div>}
    {checked&&<section className="mechanics-diagnostics mechanics-validation" aria-label={tr('Диагностика')} role="status"><h3>{tr('Проверка проекта')}</h3>{issues.length?issues.map(i=><p key={i.id}>{message(i.title)}{i.detail?' · '+message(i.detail):''}</p>):<p>{tr('Ошибок в данных нет. Проверьте поведение в игре.')}</p>}</section>}
   </section>
  </div></fieldset>
  <ExecutionJournal project={project} preview={preview} mechanic={m} onFunction={openFunction} onEvent={onEvent} onObject={onObject}/>
  {error&&<p className="mechanics-error" role="alert">{message(error)}</p>}
 </div>;
}
