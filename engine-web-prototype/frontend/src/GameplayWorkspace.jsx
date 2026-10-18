import ExecutionJournal from './ExecutionJournal.jsx';
import React,{useState,useEffect,useRef} from 'react';
import AuthoredMechanicEditor,{CreateMechanic} from './AuthoredMechanicEditor.jsx';
import {duplicateInstance,planAuthoringChange} from './authoredMechanics.js';
import {Button} from './StudioParts.jsx';
import {GameSelect} from './GameplayCommandFields.jsx';
import {addMechanic,removeMechanic,mechanicsFor,mechanicDefinitions,mechanicDefinition} from './mechanicsModel.js';
import MechanicFields,{RuleFields} from './MechanicFields.jsx';
import MechanicProjectProperties from './MechanicProjectProperties.jsx';
import BlueprintFunctionWorkspace from './BlueprintFunctionWorkspace.jsx';
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


const mechanicIcons={rule:Zap,timer:Timer,interaction:MousePointer2,door:DoorOpen,pickup:Package,movement:Move,camera:Camera,health:Heart,weapon:Crosshair,pursuit:ScanEye,physics:Box,inventory:Backpack,resource:Hash,crosshair:Crosshair,'sacura.turn':ListOrdered};
const shortDescriptions={
 rule:'Запустить действия по сигналу.',timer:'Выполнить действие через заданное время.',interaction:'Запустить событие при осмотре объекта.',door:'Открыть дверь, если найден ключ.',pickup:'Добавить предмет в инвентарь.',movement:'Управлять объектом клавишами или мышью.',camera:'Настроить ракурс и управление камерой.',health:'Задать здоровье выбранного объекта.',weapon:'Настроить стрельбу и боезапас.',pursuit:'Следовать за целью и атаковать рядом с ней.',physics:'Добавить гравитацию и столкновения.',inventory:'Хранить и использовать предметы.',resource:'Считать очки, монеты или предметы.',crosshair:'Показать прицел в игровом кадре.','sacura.turn':'Управлять порядком ходов.'
};
const DefinitionIcon=({id,size=20})=>{const Icon=mechanicIcons[id]||Puzzle;return <Icon aria-hidden="true" size={size}/>;};

export default function GameplayWorkspace({project,onChange,onImport,sceneId,selectedObjectId,addRequest,running,preview,onObject,onStop,onPlay,onUndo,onRedo,canUndo=false,canRedo=false,onWidgets,onInput,onEvent,onStory}) {
 useLocale();
 const [scopeValue,setScope]=useState(selectedObjectId?'object:'+selectedObjectId:'scene:'+sceneId),[type,setType]=useState('timer'),[selected,setSelected]=useState(''),[functionId,setFunction]=useState(''),[panel,setPanel]=useState(()=>mechanicsFor(project).length?'instance':'library'),[query,setQuery]=useState(''),[error,setError]=useState(''),[exchange,setExchange]=useState(null),[transfer,setTransfer]=useState(''),[checked,setChecked]=useState(false),[advanced,setAdvanced]=useState(false);
 const root=useRef(null),addButton=useRef(null),search=useRef(null),heading=useRef(null),returnFocus=useRef(false);
 const colon=scopeValue.indexOf(':'),scope={kind:scopeValue.slice(0,colon),id:scopeValue.slice(colon+1)},all=mechanicsFor(project),m=all.find(m=>m.id===selected)||all.find(m=>m.scope?.kind===scope.kind&&m.scope.id===scope.id),definition=m&&mechanicDefinition(m.definitionId||m.capabilityId||m.type,project,m.definitionVersion),available=[...mechanicDefinitions(project),...all.filter(r=>r.packageSource&&!r.definitionId).map(r=>({id:'package:'+r.id,name:r.packageSource.name+' · '+mechanicLabel(project,r),description:r.packageSource.description||mechanicDefinition(r.capabilityId||r.type,project)?.description,scopes:[r.scope.kind],packageSource:r.packageSource,sourceMechanic:r}))].filter(d=>d.scopes?.includes(scope.kind)),matches=d=>searchable([d.name,d.description,definitionTitle(d),definitionDescription(d),...d.editor?.keywords||[]].join(' ')).includes(searchable(query)),library=available.filter(matches),actualType=library.some(d=>d.id===type)?type:library[0]?.id,chosen=library.find(d=>d.id===actualType),list=all.filter(record=>searchable(mechanicLabel(project,record)+' '+ownerLabel(project,record.scope)).includes(searchable(query))),rule=m&&project.gameplay.rules.find(r=>r.id===m.ruleId),issues=checked?validateStudio(project):[];
 const edit=fn=>{if(running)return;setError('');try{const next=structuredClone(project);fn(next);onChange(p=>Object.assign(p,next));}catch(e){setError(e.message);}};
 const openLibrary=()=>{setExchange(null);setQuery('');setPanel('library');search.current?.focus();};
 const closeExchange=()=>{setExchange(null);heading.current?.focus();};
 const choose=record=>{setSelected(record.id);setScope(record.scope?.kind?record.scope.kind+':'+record.scope.id:'project:'+project.id);setPanel('instance');setExchange(null);heading.current?.focus();};
 const openFunction=id=>{setFunction(id);setPanel('functions');};
 useEffect(()=>{const records=mechanicsFor(project),object=selectedObjectId&&project.objects.find(o=>o.id===selectedObjectId),first=object?records.find(r=>r.scope?.kind==='object'&&r.scope.id===object.id):records.find(r=>r.scope?.kind==='scene'&&r.scope.id===sceneId)||records[0];const owner=first?.scope||(object?{kind:'object',id:object.id}:{kind:'scene',id:sceneId});setScope(owner.kind+':'+owner.id);setSelected(first?.id||'');setPanel(records.length?'instance':'library');setExchange(null);setQuery('');},[project.id]);
 useEffect(()=>{if(selectedObjectId&&project.objects.some(o=>o.id===selectedObjectId)){setScope('object:'+selectedObjectId);setSelected('');}},[selectedObjectId]);
 useEffect(()=>{if(addRequest&&project.objects.some(o=>o.id===addRequest.objectId)){setScope('object:'+addRequest.objectId);setPanel('library');setQuery('');}},[addRequest]);
 useEffect(()=>{if(panel==='library')search.current?.focus();else if(returnFocus.current){returnFocus.current=false;addButton.current?.focus();}else heading.current?.focus();},[panel,exchange]);
 useEffect(()=>{setChecked(false);},[project]);
 useEffect(()=>{setAdvanced(false);},[m?.id]);
 const missingOwner=m&&(!m.scope||!['object','scene','project'].includes(m.scope.kind)||(m.scope.kind==='project'?m.scope.id!==project.id:!(m.scope.kind==='object'?project.objects:project.subscenes).some(x=>x.id===m.scope.id)));
 const scopes=[['project:'+project.id,tr('Весь проект')],...project.subscenes.map(s=>['scene:'+s.id,tr('Сцена · {0}',[s.name])]),...project.objects.map(o=>['object:'+o.id,tr('Объект · {0}',[o.name])])];
 const copy=independent=>{try{const plan=planAuthoringChange(project,p=>{const record=duplicateInstance(p,m,{independent});if(m.packageSource)record.packageSource=structuredClone(m.packageSource);return record;});edit(plan.apply);setSelected(plan.record.id);setPanel('instance');}catch(e){setError(e.message);}};
 const createDialogue=(text,index,branch)=>{const id=uid('line');edit(p=>{let chapter=p.chapters.find(c=>c.subsceneId===sceneId);if(!chapter){chapter={id:uid('chapter'),name:'Диалоги',subsceneId:sceneId,beats:[]};p.chapters.push(chapter);}chapter.beats.push({id,kind:'dialogue',speaker:'Рассказчик',text,next:null,bindings:[],mode:'SEQUENTIAL',batches:{BEFORE:[],ON_START:[],AFTER:[]}});p.gameplay.rules.find(r=>r.id===rule.id)[branch][index].beatId=id;});};
 const addReaction=()=>edit(p=>{const record=p.gameplay.mechanics.find(x=>x.id===m.id);const r=createRule({signal:m.type==='pickup'?'pickup':'interact',scope:m.scope,filter:m.scope.kind==='object'?{[m.type==='pickup'?'source':'target']:{kind:'reference',type:'object',mode:'context',role:'self'}}:{}});p.gameplay.rules.push(r);record.ruleId=r.id;});
 function localDefinition(d){return !!d.sourceMechanic||project.gameplay.mechanicDefinitions.some(item=>item.id===d.id.split('@')[0]);}
 function definitionTitle(d){return localDefinition(d)?d.name:tr(d.name);}
 function definitionDescription(d){return localDefinition(d)?d.description:tr(shortDescriptions[d.id]||d.description);}
 const addSelected=()=>{if(!chosen)return;if(chosen.sourceMechanic)setExchange({root:chosen.sourceMechanic,kind:'mechanic',copy:true,targetScope:scope});else edit(p=>{const record=addMechanic(p,actualType,scope);setSelected(record.id);setPanel('instance');setQuery('');});};
 const title=exchange?(exchange.copy?tr('Создать копию'):exchange.root?tr('Экспортировать'):tr('Импортировать')):panel==='library'?tr('Добавить механику'):panel==='functions'?tr('Функции и графы'):panel==='properties'?tr('Свойства проекта'):panel==='transfer'?tr('Events и шаблоны'):m?mechanicLabel(project,m):all.length?tr('Выберите механику'):tr('Добавьте первую механику');
 return <div ref={root} className="gameplay-workspace mechanics-workbench" onKeyDown={e=>{if(e.key==='Escape'&&!running){e.preventDefault();if(exchange)closeExchange();else{returnFocus.current=true;setPanel('instance');addButton.current?.focus();}}}}>
  <header className="mechanics-toolbar"><strong>{tr(panel==='functions'&&!exchange?'Функции и графы':'Механики')}</strong><div className="mechanics-tools">
   <Button className="mechanics-icon-button" disabled={!canUndo||running} onClick={onUndo} title={tr('Отменить')} aria-label={tr('Отменить')}><Undo2 aria-hidden="true" size={16}/></Button>
   <Button className="mechanics-icon-button" disabled={!canRedo||running} onClick={onRedo} title={tr('Повторить')} aria-label={tr('Повторить')}><Redo2 aria-hidden="true" size={16}/></Button>
   <span className="mechanics-toolbar-divider" aria-hidden="true"/>
   <Button disabled={running} onClick={()=>setChecked(true)}><CheckCheck aria-hidden="true" size={15}/>{tr('Проверить')}</Button>
   {onPlay&&<Button className="mechanics-primary" disabled={running} onClick={onPlay}><Play aria-hidden="true" size={14}/>{tr('Запустить')}</Button>}
   {running&&<Button onClick={onStop}>{tr('Остановить для редактирования')}</Button>}
  </div></header>
  <fieldset disabled={running} className="mechanics-content"><div className={'mechanics-layout'+(panel==='functions'&&!exchange?' mechanics-function-layout':'')}>
   <aside className="mechanics-catalog" aria-label={tr('Установленные механики')}>
    <div className="mechanics-target"><GameSelect label="Добавить к" value={scopeValue} options={scopes} onChange={value=>{setScope(value);setSelected('');}}/>
     <button ref={addButton} aria-label={tr('Добавить механику')} title={tr('Добавить механику')} className={'mechanics-add mechanics-primary'+(panel==='library'?' active':'')} type="button" onClick={openLibrary}><Plus aria-hidden="true" size={16}/><span>{tr('Добавить механику')}</span></button>
    </div>
    <label className="mechanics-search"><Search aria-hidden="true" size={15}/><span className="mechanics-sr-only">{tr('Поиск механик')}</span><input ref={search} type="search" placeholder={tr('Найти механику…')} value={query} onChange={e=>setQuery(e.target.value)}/></label>
    <div className="mechanics-list-heading"><h3>{tr('Установленные')}</h3><span>{all.length}</span></div>
    {all.length>0&&<div className="mechanics-mobile-picker"><GameSelect label="Выбрать механику" value={m?.id||''} options={[["",tr('Выберите…')],...list.map(record=>[record.id,mechanicLabel(project,record)])]} onChange={id=>{const record=all.find(item=>item.id===id);if(record)choose(record);}}/></div>}
    <div className="mechanics-list" onKeyDown={e=>{if(!['ArrowDown','ArrowUp','Home','End'].includes(e.key))return;const buttons=[...e.currentTarget.querySelectorAll('button')],index=buttons.indexOf(document.activeElement);if(index<0)return;e.preventDefault();buttons[e.key==='Home'?0:e.key==='End'?buttons.length-1:(index+(e.key==='ArrowDown'?1:-1)+buttons.length)%buttons.length]?.focus();}}>
     {list.map(record=><button type="button" key={record.id} aria-pressed={!exchange&&panel!=='library'&&m?.id===record.id} onClick={()=>choose(record)}><DefinitionIcon id={record.capabilityId||record.type} size={16}/><span><strong>{mechanicLabel(project,record)}</strong><small>{ownerLabel(project,record.scope)}</small></span></button>)}
    </div>
    {!list.length&&<p className="mechanics-list-empty">{all.length?tr('Ничего не найдено'):tr('Пока нет механик')}</p>}
    <div className="mechanics-sidebar-tools"><button type="button" onClick={()=>setExchange({})}><Upload aria-hidden="true" size={15}/>{tr('Импортировать')}</button>
     <details className="mechanics-more"><summary><Settings2 aria-hidden="true" size={15}/>{tr('Дополнительно')}<ChevronDown aria-hidden="true" size={13}/></summary><div>
      <button type="button" onClick={()=>{setExchange(null);setPanel('properties');}}><Settings2 aria-hidden="true" size={15}/>{tr('Свойства проекта')}</button>
      <button type="button" onClick={()=>{setExchange(null);setPanel('functions');}}><Braces aria-hidden="true" size={15}/>{tr('Функции и графы')}</button>
      <button type="button" onClick={()=>{setExchange(null);setPanel('transfer');}}><FileBox aria-hidden="true" size={15}/>{tr('Events и шаблоны')}</button>
     </div></details>
    </div>
   </aside>
   <section className={'mechanics-inspector mechanics-panel-'+panel} aria-label={tr('Редактор механики')}>
    <h2 ref={heading} className={panel==='functions'&&!exchange?'mechanics-sr-only':undefined} tabIndex={-1}>{title}</h2>
    {exchange?<PackageExchange project={project} root={exchange.root} kind={exchange.kind} onImport={onImport} onChange={edit} copy={exchange.copy} targetScope={exchange.targetScope} initialName={exchange.kind==='mechanic'&&exchange.root?mechanicLabel(project,exchange.root):undefined} onApplied={plan=>{const ids=new Set(all.map(m=>m.id));const added=plan.project.gameplay.mechanics.find(m=>!ids.has(m.id));if(added){setSelected(added.id);setScope(added.scope.kind+':'+added.scope.id);}setPanel('instance');}} onClose={closeExchange}/>:panel==='library'?<>
     {library.length?<>
      <div className="mechanics-ready-picker"><GameSelect label="Готовая механика" value={actualType} options={library.map(d=>[d.id,definitionTitle(d)])} onChange={setType}/><p>{chosen&&definitionDescription(chosen)}</p></div>
      <ul className="mechanics-library" aria-label={tr('Библиотека механик')}>{library.map(d=><li key={d.id}><button type="button" aria-pressed={actualType===d.id} onClick={()=>setType(d.id)}>
       <span className="mechanics-card-top"><span className="mechanics-card-icon"><DefinitionIcon id={d.id}/></span>{actualType===d.id&&<Check aria-hidden="true" size={16}/>}</span>
       <strong>{definitionTitle(d)}</strong><span className="mechanics-card-description">{definitionDescription(d)}</span>
       {mechanicSource(project,d)?<small className="mechanics-source">{tr('Импортировано')}</small>:localDefinition(d)?<small className="mechanics-source">{tr('Локальная заготовка')}</small>:null}
      </button></li>)}</ul>
      <div className="mechanics-add-bar"><div><strong>{chosen&&definitionTitle(chosen)}</strong><small>{ownerLabel(project,scope)}</small></div><button className="mechanics-primary" type="button" disabled={!chosen} onClick={addSelected}><Plus aria-hidden="true" size={16}/>{tr('Добавить')}</button></div>
     </>:<div className="mechanics-empty"><Search aria-hidden="true" size={25}/><h3>{tr('Механики не найдены')}</h3><p>{tr('Измените запрос или импортируйте заготовку.')}</p><button type="button" onClick={()=>setQuery('')}>{tr('Очистить поиск')}</button></div>}
     <CreateMechanic project={project} selected={m} onChange={edit} onCreated={id=>{setType(id);setQuery('');}}/>
    </>:panel==='functions'?<BlueprintFunctionWorkspace project={project} requestedId={functionId} onChange={edit} onBack={()=>setPanel(m?'instance':'library')} onExport={fn=>{setFunction(fn.id);setExchange({root:fn,kind:'function'});}}/>:panel==='properties'?<MechanicProjectProperties project={project} onChange={edit} onInput={onInput} onWidgets={onWidgets} onStory={onStory}/>:panel==='transfer'?<div className="mechanics-transfer-panel">
     <GameSelect label="Событие для экспорта" value={transfer} options={[["","Выберите…"],...project.events.map(e=>[e.id,e.name])]} onChange={setTransfer}/>
     <div className="mechanics-tools"><button type="button" disabled={!transfer} onClick={()=>setExchange({root:project.events.find(e=>e.id===transfer),kind:'event'})}><Download aria-hidden="true" size={15}/>{tr('Экспортировать событие')}</button><button type="button" onClick={()=>setExchange({root:project,kind:'template'})}><FileBox aria-hidden="true" size={15}/>{tr('Экспортировать шаблон проекта')}</button></div>
    </div>:m?<>
     <p className="mechanics-owner">{ownerLabel(project,m.scope)}</p>
     {!definition&&<p role="alert">{tr('Определение недоступно. Исходные данные сохранены.')}</p>}
     {missingOwner&&<p role="alert">{tr('Владелец механики удалён. Привязка сохранена; выберите новый объект при импорте копии.')}</p>}
     <div className="mechanics-instance-toolbar"><button className={'mechanics-logic-toggle'+(panel==='logic'?' active':'')} type="button" aria-pressed={panel==='logic'} onClick={()=>setPanel(panel==='logic'?'instance':'logic')}><Braces aria-hidden="true" size={15}/>{panel==='logic'?tr('К параметрам экземпляра'):tr('Изменить логику')}</button><div className="mechanics-tools">
      <button type="button" onClick={()=>setExchange({root:m,kind:'mechanic'})}><Download aria-hidden="true" size={15}/>{tr('Экспортировать')}</button>
      <button type="button" onClick={()=>m.definitionId?copy(true):setExchange({root:m,kind:'mechanic',copy:true})}><Copy aria-hidden="true" size={15}/>{tr('Создать копию')}</button>
      <button type="button" className="mechanics-icon-button mechanics-danger" title={tr('Отключить механику')} aria-label={tr('Отключить механику')} onClick={()=>edit(p=>{removeMechanic(p,m);setSelected('');setPanel(all.length<=1?'library':'instance');})}><Unplug aria-hidden="true" size={16}/></button>
     </div></div>
     {panel==='logic'?m.definitionId?<AuthoredMechanicEditor expanded project={project} mechanic={m} onChange={edit} onFunction={openFunction} onEvent={onEvent}/>:<>
      {rule?<RuleFields key={m.id} project={project} onCreateDialogue={createDialogue} onCreateSignal={name=>edit(p=>{const signal=createSignalDefinition(p,{name});const r=p.gameplay.rules.find(r=>r.id===rule.id);if(r.version===1)r.signal={type:signal.id};else r.trigger=signal.id;})} rule={rule} onChange={patch=>edit(p=>Object.assign(p.gameplay.rules.find(r=>r.id===rule.id),patch))} onEvent={onEvent} onFunction={openFunction}/>:<>
       <p>{tr('Настройте реакцию на сигнал.')}</p>{!m.native&&definition&&<button type="button" onClick={addReaction}><Plus aria-hidden="true" size={15}/>{tr('Добавить реакцию')}</button>}
      </>}{m.eventId&&<button type="button" onClick={()=>onEvent?.(m.eventId)}>{tr('Открыть Event механики')}</button>}
     </>:<div className="mechanics-parameters">
      {definition&&<p className="mechanics-description">{definitionDescription(definition)}</p>}
      <MechanicFields compact project={project} mechanic={m} onChange={edit} onEvent={onEvent} onFunction={openFunction}/>
      {rule&&<p className="mechanics-rule-summary">{ruleSummary(project,rule)}</p>}
      <details className="mechanics-instance-advanced" open={advanced}><summary onClick={e=>{e.preventDefault();setAdvanced(!advanced);}}>{tr('Дополнительные свойства экземпляра')}<ChevronDown aria-hidden="true" size={14}/></summary>
       {m.definitionId&&<button type="button" onClick={()=>copy(false)}>{tr('Копия с общим определением')}</button>}
       {m.packageSource&&<p>{tr('Источник: {0} · {1} · {2}',[m.packageSource.name,m.packageSource.author,m.packageSource.version])}</p>}
       {advanced&&<MechanicFields advancedOnly project={project} mechanic={m} onChange={edit} onEvent={onEvent} onFunction={openFunction}/>}
      </details>
     </div>}
    </>:<div className="mechanics-empty"><Puzzle aria-hidden="true" size={32}/><h3>{tr('Выберите готовую механику')}</h3><button type="button" onClick={openLibrary}><Plus aria-hidden="true" size={16}/>{tr('Открыть библиотеку')}</button></div>}
    {checked&&<section className="mechanics-diagnostics mechanics-validation" aria-label={tr('Диагностика')} role="status"><h3>{tr('Проверка проекта')}</h3>{issues.length?issues.map(i=><p key={i.id}>{message(i.title)}{i.detail?' · '+message(i.detail):''}</p>):<p>{tr('Ошибок в данных нет. Проверьте поведение в игре.')}</p>}</section>}
   </section>
  </div></fieldset>
  <ExecutionJournal project={project} preview={preview} mechanic={m} onFunction={openFunction} onEvent={onEvent} onObject={onObject}/>
  {error&&<p className="mechanics-error" role="alert">{message(error)}</p>}
 </div>;
}
