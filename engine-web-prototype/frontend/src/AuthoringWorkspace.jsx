import {t as tr, useLocale, message} from './i18n.jsx';
import React,{useEffect,useRef,useState} from 'react';
import {Icon,Button} from './StudioParts.jsx';
import ActionFields from './ActionFields.jsx';
import {sceneFor} from './studioModel.js';
import {TYPES,uid} from './model.js';
import {LIBRARY_KEYS,blankAsset,copyAction,copyGroup,eventFromAsset,authoringProblems} from './authoringModel.js';
import {ACTION_CATEGORIES,ACTION_HINTS,contextualAction,actionUnavailable,describeAction,actionTiming,actionCount} from './authoringPresentation.js';
import './authoringWorkspace.css';

const KINDS={
  event:{plural:'События',label:'Событие',icon:'Layers',hint:'Группы по очереди',create:'Создать событие'},
  group:{plural:'Группы действий',label:'Группа действий',icon:'Columns2',hint:'Команды одновременно',create:'Создать группу'},
  action:{plural:'Действия',label:'Действие',icon:'MousePointer2',hint:'Одна команда',create:'Создать действие'},
};
const newNames={action:'Новое действие',group:'Новая группа',event:'Новое событие'};

export default function AuthoringWorkspace({project,request,mutate,onGraph,onPreview,onPlace,onNotice,beat,hidden}) {
 useLocale();
  const [kind,setKind]=useState('event'),[draft,setDraft]=useState(null),[search,setSearch]=useState('');
  const [picker,setPicker]=useState(null),[selected,setSelected]=useState(null),[hook,setHook]=useState('ON_START');
  const [saved,setSaved]=useState(false),[attempted,setAttempted]=useState(false),[undo,setUndo]=useState(null),[help,setHelp]=useState(false);
  const drafts=useRef(new Map()),bases=useRef(new Map()),recent=useRef({}),current=useRef({kind,draft}),seen=useRef(),errors=useRef(),title=useRef(),canvas=useRef(),inspector=useRef(),root=useRef();
  current.current={kind,draft};
  const scene=sceneFor(project,beat?.id);
  const isDirty=(k,d)=>d&&JSON.stringify(d)!==(bases.current.has(k+d.id)?bases.current.get(k+d.id):JSON.stringify(project[LIBRARY_KEYS[k]].find(a=>a.id===d.id)));
  function open(k,asset) {
    const old=current.current;
    if(isDirty(old.kind,old.draft))drafts.current.set(old.kind+old.draft.id,old.draft);
    else if(old.draft)drafts.current.delete(old.kind+old.draft.id);
    if(old.draft)recent.current[old.kind]=old.draft;
    setKind(k);
    if(asset) {
      const key=k+asset.id,canonical=project[LIBRARY_KEYS[k]].find(a=>a.id===asset.id);
      if(!bases.current.has(key)||!drafts.current.has(key))bases.current.set(key,canonical?JSON.stringify(canonical):null);
      setDraft(structuredClone(drafts.current.get(key)||canonical||asset));
    } else setDraft(null);
    setSaved(false);setAttempted(false);setPicker(null);setSelected(null);setUndo(null);setHelp(false);
  }
  function create(k,type,target) {
    if(k==='action'&&!type){setPicker({mode:'create',target});return;}
    const asset=k==='action'?{...contextualAction(type,project,scene,target),name:TYPES[type].label}: {...blankAsset(k),name:newNames[k]};
    open(k,asset);
  }
  useEffect(()=>{
    const {kind:k,draft:d}=current.current;
    if(!d||isDirty(k,d))return;
    const canonical=project[LIBRARY_KEYS[k]].find(a=>a.id===d.id);
    if(canonical){bases.current.set(k+d.id,JSON.stringify(canonical));setDraft(structuredClone(canonical));}else setDraft(null);
  },[project]);
  useEffect(()=>{
    if(!request||seen.current===request.token)return;
    seen.current=request.token;
    if(request.id||request.draft)open(request.kind,request.draft||project[LIBRARY_KEYS[request.kind]].find(a=>a.id===request.id));
    else create(request.kind,request.type,request.target);
  },[request]);
  useEffect(()=>{if(hidden)setPicker(null);},[hidden]);
  useEffect(()=>{canvas.current?.scrollTo(0,0);root.current?.querySelector('.al-library-item.selected')?.scrollIntoView({block:'nearest'});},[draft?.id]);
  useEffect(()=>{inspector.current?.scrollTo(0,0);root.current?.querySelector('.al-action-card.selected')?.scrollIntoView({block:'nearest'});},[selected?.actionId,draft?.id]);
  const patch=v=>{setDraft(d=>({...d,...v}));setSaved(false);};
  const edit=fn=>{setDraft(d=>{const n=structuredClone(d);fn(n);return n;});setSaved(false);};
  const groups=draft?(kind==='event'?draft.groups:kind==='group'?[draft]:[]):[];
  const selectedGroup=groups.find(g=>g.id===selected?.groupId);
  const selectedAction=kind==='action'?draft:selectedGroup?.actions.find(a=>a.id===selected?.actionId);
  const patchGroup=(id,fn)=>edit(d=>{const g=kind==='group'?d:d.groups.find(g=>g.id===id);if(g)fn(g);});
  const patchAction=v=>kind==='action'?patch(v):patchGroup(selected.groupId,g=>Object.assign(g.actions.find(a=>a.id===selected.actionId),v));
  const problems=draft?authoringProblems(kind,draft):[];
  const stored=project[LIBRARY_KEYS[kind]]||[];
  function items(k) {
    const canonical=project[LIBRARY_KEYS[k]]||[];
    return [...canonical.map(a=>k===kind&&a.id===draft?.id?draft:drafts.current.get(k+a.id)||a),
      ...[...drafts.current.entries()].filter(([key,a])=>key.startsWith(k)&&!canonical.some(x=>x.id===a.id)&&!(k===kind&&a.id===draft?.id)).map(([,a])=>a),
      ...(k===kind&&draft&&!canonical.some(a=>a.id===draft.id)?[draft]:[])];
  }
  const list=items(kind).filter(a=>(a.name||'Без названия').toLowerCase().includes(search.toLowerCase()));
  const dirty=draft&&isDirty(kind,draft);
  function validate() {
    setAttempted(true);
    if(problems.length){requestAnimationFrame(()=>errors.current?.focus());return false;}
    return true;
  }
  function save() {
    if(!validate())return false;
    const original=stored.find(a=>a.id===draft.id),base=bases.current.get(kind+draft.id);
    if(original&&base&&JSON.stringify(original)!==base&&isDirty(kind,draft)) {
      onNotice('Шаблон изменён в другом редакторе. Загрузите сохранённую версию в меню рядом с названием.');return false;
    }
    const value=structuredClone(draft);value.name=value.name.trim();
    mutate(p=>{const collection=p[LIBRARY_KEYS[kind]],index=collection.findIndex(a=>a.id===value.id);if(index<0)collection.push(value);else collection[index]=value;});
    drafts.current.delete(kind+draft.id);bases.current.set(kind+draft.id,JSON.stringify(value));
    setDraft(value);current.current={kind,draft:value};setSaved(true);setUndo(null);
    onNotice(`Сохранено в библиотеку: ${value.name}`);return true;
  }
  function remove(fn,label) {setUndo({draft:structuredClone(draft),label});edit(fn);setSelected(null);}
  function chooseAction(type,template) {
    const a=template?copyAction(template):contextualAction(type,project,scene,picker.target);
    if(picker.mode==='create'){open('action',{...a,name:template?.name||TYPES[type].label});return;}
    if(picker.mode==='replace') {
      const old=selectedAction;
      const replacement={...a,id:old.id,name:old.name===TYPES[old.type]?.label?TYPES[type].label:old.name,description:old.description};
      if(kind==='action'){setDraft(replacement);setSaved(false);}
      else patchGroup(selected.groupId,g=>{g.actions=g.actions.map(item=>item.id===old.id?replacement:item);});
    } else if(picker.mode==='next') {
      const g={id:uid('group'),name:`Группа ${groups.length+1}`,actions:[a]};
      edit(d=>d.groups.push(g));setSelected({groupId:g.id,actionId:a.id});
    } else {patchGroup(picker.groupId,g=>g.actions.push(a));setSelected({groupId:picker.groupId,actionId:a.id});}
    setPicker(null);
  }
  function chooseGroup(g) {edit(d=>d.groups.push(copyGroup(g)));setPicker(null);setSelected(null);}
  const count=groups.reduce((n,g)=>n+g.actions.length,0);
  const placements=kind==='event'&&draft?project.chapters.flatMap(c=>c.beats).flatMap(b=>b.bindings||[]).filter(b=>b.eventId===draft.id).length:0;
  const info=KINDS[kind];
  return <div ref={root} className="authoring-workspace al-workspace" hidden={hidden}>
    <header className="al-header">
      <div className="al-library-title"><Icon name="LibraryBig" size={21}/><h1>{tr("Библиотека действий и событий")}</h1></div>
      <Button icon="CircleHelp" className="al-help-button" aria-expanded={help} onClick={()=>setHelp(!help)}>{tr("Как это устроено")}</Button>
    </header>
    <div className="al-tabs" role="tablist" aria-label={tr("Разделы библиотеки")}>
      {Object.entries(KINDS).map(([k,v])=><button role="tab" aria-selected={kind===k} id={`al-tab-${k}`} aria-controls="al-panel" tabIndex={kind===k?0:-1} key={k} className={kind===k?'active':''} onClick={()=>{setSearch('');open(k,recent.current[k]);}} onKeyDown={e=>{
        const keys=Object.keys(KINDS),i=keys.indexOf(k);let next;
        if(e.key==='ArrowRight')next=keys[(i+1)%keys.length];if(e.key==='ArrowLeft')next=keys[(i+keys.length-1)%keys.length];if(e.key==='Home')next=keys[0];if(e.key==='End')next=keys.at(-1);
        if(next){e.preventDefault();setSearch('');open(next,recent.current[next]);document.getElementById(`al-tab-${next}`)?.focus();}
      }}><Icon name={v.icon} size={19}/><span><strong>{tr(v.plural)}</strong><small>{tr(v.hint)}</small></span><b>{items(k).length}</b></button>)}
    </div>
    {help&&<div className="al-help"><ConceptMap/><Button icon="X" title={tr("Закрыть подсказку")} onClick={()=>setHelp(false)}/></div>}
    <div className="al-body" id="al-panel" role="tabpanel" aria-labelledby={`al-tab-${kind}`}>
      <aside className="al-library" aria-label={tr(info.plural)}>
        <div className="al-library-tools"><Button icon="Plus" onClick={()=>create(kind)}>{tr(info.create)}</Button><label className="al-search"><Icon name="Search" size={16}/><input aria-label={tr("Найти: {0}", [tr(info.plural)])} placeholder={tr("Поиск в библиотеке")} value={search} onChange={e=>setSearch(e.target.value)}/>{search&&<Button icon="X" title={tr("Очистить поиск")} onClick={()=>setSearch('')}/>}</label></div>
        <button className={`al-overview ${!draft?'active':''}`} onClick={()=>open(kind,null)}><Icon name="LayoutGrid" size={15}/>{tr("Обзор")}</button>
        <div className="al-items">{list.map(a=>{
          const unsaved=!stored.some(x=>x.id===a.id)||isDirty(kind,a);
          return <button key={a.id} className={`al-library-item ${draft?.id===a.id?'selected':''}`} aria-current={draft?.id===a.id?'true':undefined} onClick={()=>open(kind,a)}>
            <span className="al-item-icon"><Icon name={kind==='action'?TYPES[a.type]?.icon:info.icon} size={18}/></span><span><strong>{a.name||tr("Без названия")}</strong><small>{kind==='action'?tr(TYPES[a.type]?.label):kind==='group'?tr("{0} · одновременно", [a.actions.length]):tr("{0} · по очереди", [a.groups.length])}</small>{unsaved&&<em>{tr("Черновик")}</em>}</span>
          </button>;
        })}{!list.length&&<div className="al-list-empty"><Icon name={search?'SearchX':info.icon} size={24}/><p>{search?tr("Ничего не найдено"):tr("Здесь будут ваши ")+tr(info.plural).toLowerCase()}</p>{search&&<button onClick={()=>setSearch('')}>{tr("Сбросить поиск")}</button>}</div>}</div>
        <div className="al-library-foot"><Icon name="BookCopy" size={15}/><span>{kind==='event'?tr("Соберите один раз, используйте в истории."):tr("Вставляется в событие как независимая копия.")}</span></div>
      </aside>
      {!draft?<main className="al-overview-main"><Welcome kind={kind} onCreate={()=>create(kind)} onKind={k=>{setSearch('');open(k,null);}}/></main>:<div className="al-editor">
        <header className="al-editor-header">
          <div className="al-identity"><span className="al-eyebrow"><Icon name={info.icon} size={14}/>{tr(info.label)}<span className="al-save-state" role="status">{saved&&!dirty?tr("Сохранено"):dirty?tr("Черновик"):tr("В библиотеке")}</span></span><label className="al-name"><span className="al-sr-only">{tr("Название в библиотеке")}</span><input ref={title} value={draft.name} placeholder={tr(newNames[kind])} aria-invalid={attempted&&!draft.name.trim()||undefined} onChange={e=>patch({name:e.target.value})}/><Icon name="Pencil" size={14}/></label></div>
          <div className="al-editor-actions"><Button icon="Play" title={tr("Проверить в 3D-сцене")} onClick={()=>{if(validate())onPreview(eventFromAsset(kind,draft));}}>{tr("Проверить")}</Button><Button icon={saved&&!dirty?'Check':'CheckCheck'} className="al-primary" onClick={save}>{tr("Сохранить")}</Button><details className="al-menu"><summary aria-label={tr("Другие действия")}><Icon name="Ellipsis"/></summary><div><Button icon="Pencil" onClick={()=>{title.current?.focus();title.current?.select();}}>{tr("Переименовать")}</Button>{stored.some(a=>a.id===draft.id)&&<Button icon="RotateCcw" onClick={()=>{const original=stored.find(a=>a.id===draft.id);setUndo({draft:structuredClone(draft),label:'Загружена сохранённая версия'});drafts.current.delete(kind+draft.id);bases.current.set(kind+draft.id,JSON.stringify(original));setDraft(structuredClone(original));setSaved(false);setSelected(null);}}>{tr("Загрузить сохранённое")}</Button>}{kind==='event'&&<Button icon="Workflow" onClick={()=>{if(save())onGraph(draft.id);}}>{tr("Открыть граф")}</Button>}</div></details></div>
        </header>
        {attempted&&problems.length>0&&<div ref={errors} tabIndex={-1} className="al-errors" role="alert"><Icon name="TriangleAlert" size={18}/><div><strong>{tr("Осталось поправить")}</strong>{problems.map((p,i)=><p key={i}>{message(p)}</p>)}</div></div>}
        {undo&&<div className="al-undo" role="status"><span>{tr(undo.label)}</span><Button icon="Undo2" onClick={()=>{setDraft(undo.draft);setSaved(false);setUndo(null);}}>{tr("Вернуть")}</Button><Button icon="X" title={tr("Скрыть уведомление")} onClick={()=>setUndo(null)}/></div>}
        <div className={`al-editor-body ${selectedAction?'has-selection':''}`}>
          <main ref={canvas} className="al-canvas" aria-label={tr("Сборка")}>
            {kind==='action'?<ActionStage action={draft} project={project} scene={scene}/>:<>
              <div className="al-canvas-heading"><div><h2>{kind==='event'?tr("Последовательность события"):tr("Одновременный запуск")}</h2><span>{kind==='event'?tr("Сверху вниз · внутри группы вместе"):tr("Каждая карточка — отдельное действие")}</span></div><span className="al-count">{actionCount(count)}</span></div>
              {groups.length>0&&<div className="al-endpoint"><Icon name="CirclePlay" size={17}/>{kind==='event'?tr("Начало события"):tr("Старт группы")}</div>}
              {groups.map((g,index)=><React.Fragment key={g.id}>
                {index>0&&<div className="al-connector"><span/><Icon name="ArrowDown" size={16}/><small>{tr("Затем")}</small></div>}
                <section className="al-group" aria-label={tr("Группа {0}", [index+1])}>
                  <header><span className="al-step-number">{String(index+1).padStart(2,'0')}</span><div className="al-group-name">{kind==='event'?<input aria-label={tr("Название группы {0}", [index+1])} value={g.name} placeholder={tr("Группа {0}", [index+1])} onChange={e=>patchGroup(g.id,x=>x.name=e.target.value)}/>:<strong>{draft.name||tr("Группа действий")}</strong>}<small><Icon name="Columns2" size={13}/>{tr("Стартуют вместе")}</small></div>{kind==='event'&&<div className="al-group-tools"><Button icon="ArrowUp" title={tr("Группа {0}: переместить выше", [index+1])} disabled={index===0} onClick={()=>edit(d=>{[d.groups[index-1],d.groups[index]]=[d.groups[index],d.groups[index-1]];})}/><Button icon="ArrowDown" title={tr("Группа {0}: переместить ниже", [index+1])} disabled={index===groups.length-1} onClick={()=>edit(d=>{[d.groups[index+1],d.groups[index]]=[d.groups[index],d.groups[index+1]];})}/><Button icon="Trash2" title={tr("Удалить группу {0}", [index+1])} onClick={()=>remove(d=>{d.groups=d.groups.filter(x=>x.id!==g.id);},'Группа убрана')}/></div>}</header>
                  <div className="al-parallel"><div className="al-parallel-rail" aria-hidden="true"/>{g.actions.map(a=><ActionCard key={a.id} action={a} project={project} scene={scene} selected={selectedAction?.id===a.id} onSelect={()=>setSelected({groupId:g.id,actionId:a.id})}/>)}<button className="al-add-card" onClick={()=>setPicker({mode:'add',groupId:g.id})}><span><Icon name="Plus" size={21}/></span><strong>{g.actions.length?tr("Ещё одновременно"):tr("Добавить действие")}</strong><small>{g.actions.length?tr("В эту же группу"):tr("Выберите, что произойдёт")}</small></button></div>
                  {g.actions.length>0&&<footer><Icon name="GitMerge" size={14}/>{g.actions.some(a=>a.wait==='COMPLETED'&&TYPES[a.type]?.completion==='FINITE')?tr("Дождаться завершения действий"):tr("После запуска команд")}{kind==='event'&&<Icon name="ArrowDown" size={13}/>}</footer>}
                </section>
              </React.Fragment>)}
              {kind==='event'&&(groups.length?<><button className="al-next-group" onClick={()=>setPicker({mode:'next'})}><Icon name="Plus" size={18}/>{tr("Следующая группа")}<small>{tr("После предыдущей")}</small></button><div className="al-endpoint end"><Icon name={draft.retention==='AUTO_CLOSE_ON_FLOW_END'?'CircleCheck':'Infinity'} size={17}/>{draft.retention==='AUTO_CLOSE_ON_FLOW_END'?tr("Конец события"):tr("Фон продолжается")}</div></>:<div className="al-empty-canvas"><MiniStructure kind="event"/><h3>{tr("С чего начнётся событие?")}</h3><p>{tr("Добавьте команду. Её группа станет первым шагом.")}</p><Button icon="Plus" className="al-primary" onClick={()=>setPicker({mode:'next'})}>{tr("Добавить первое действие")}</Button></div>)}
            </>}
            <section className="al-use">
              {kind==='event'?<details><summary><Icon name="MessageSquarePlus" size={18}/><span>{tr("Добавить в историю")}<small>{tr("Запустить на выбранной реплике")}</small></span><Icon name="ChevronDown" size={16}/></summary><div className="al-use-content"><blockquote>{beat?.speaker&&<strong>{beat.speaker}</strong>}{beat?.text||tr("Выбранная реплика")}</blockquote><label>{tr("Когда запустить")}<select value={hook} onChange={e=>setHook(e.target.value)}><option value="BEFORE">{tr("До реплики")}</option><option value="ON_START">{tr("Во время реплики")}</option><option value="AFTER">{tr("После реплики")}</option></select></label><Button icon="Plus" disabled={!beat} onClick={()=>{if(save())onPlace(draft,hook);}}>{tr("Добавить в реплику")}</Button></div></details>:<div className="al-reuse"><Icon name="Layers" size={22}/><span><strong>{tr("Продолжить сборку")}</strong><small>{kind==='action'?tr("Поместить команду в группу и добавить следующие шаги"):tr("Использовать эту группу первым шагом события")}</small></span><Button icon="ArrowRight" onClick={()=>{if(save())open('event',eventFromAsset(kind,draft));}}>{tr("Собрать событие")}</Button></div>}
            </section>
          </main>
          <aside ref={inspector} className="al-inspector" aria-label={selectedAction?tr("Настройки действия"):tr("Настройки сборки")}>
            {selectedAction?<React.Fragment key={selectedAction.id}>
              <header><span className="al-eyebrow">{kind==='action'?tr("Настроить команду"):tr("Группа {0} · действие", [groups.indexOf(selectedGroup)+1])}</span>{kind!=='action'&&<Button icon="X" title={tr("Закрыть настройки действия")} onClick={()=>setSelected(null)}/>}</header>
              <div className="al-inspector-title"><span className="al-command-icon"><Icon name={TYPES[selectedAction.type]?.icon} size={25}/></span><h2>{tr(TYPES[selectedAction.type]?.label)}</h2></div>
              <button className="al-change-type" onClick={()=>setPicker({mode:'replace',target:selectedAction.target})}>{tr("Изменить команду")}<Icon name="ChevronDown" size={13}/></button>
              <div className="al-fields"><ActionFields compact sceneId={scene?.id} project={project} action={selectedAction} onChange={patchAction}/></div>
              <div className="al-timing"><Icon name={TYPES[selectedAction.type]?.completion==='CONTINUOUS'?'Infinity':'GitMerge'} size={16}/><span>{actionTiming(selectedAction)}{selectedAction.type==='music'&&<small>{tr("Музыка остаётся в фоне, пока живёт событие.")}</small>}</span></div>
              {['music','sound'].includes(selectedAction.type)&&<details className="al-disclosure"><summary>{tr("Настройки звука")}<Icon name="ChevronDown" size={14}/></summary><div>{selectedAction.type==='music'?<><label className="al-check"><input type="checkbox" checked={selectedAction.loop!==false} onChange={e=>patchAction({loop:e.target.checked})}/>{tr("Повторять по кругу")}</label></>:<label className="al-check"><input type="checkbox" checked={selectedAction.duck!==false} onChange={e=>patchAction({duck:e.target.checked})}/>{tr("Приглушать музыку под голос")}</label>}</div></details>}
              {kind!=='action'&&<Button className="al-remove" icon="Trash2" onClick={()=>remove(d=>{const g=kind==='group'?d:d.groups.find(g=>g.id===selected.groupId);g.actions=g.actions.filter(a=>a.id!==selectedAction.id);},'Действие убрано')}>{tr("Убрать из группы")}</Button>}
            </React.Fragment>:<>
              <header><span className="al-eyebrow">{kind==='event'?tr("Событие целиком"):tr("Группа целиком")}</span></header>
              <MiniStructure kind={kind}/><h2>{kind==='event'?tr("Один момент истории"):tr("Несколько команд вместе")}</h2><p className="al-inspector-hint">{kind==='event'?tr("Группы идут по стрелкам. Команды внутри каждой группы стартуют одновременно."):tr("Соберите команды, которые должны начаться в один момент.")}</p>
              {count>0&&<div className="al-select-hint"><Icon name="MousePointer2" size={18}/>{tr("Нажмите на карточку, чтобы настроить действие.")}</div>}
              {kind==='event'&&<details className="al-disclosure" key={draft.id}><summary><Icon name="SlidersHorizontal" size={15}/>{tr("Поведение события")}<Icon name="ChevronDown" size={14}/></summary><div><label className="al-field">{tr("После последней группы")}<select value={draft.retention} onChange={e=>patch({retention:e.target.value,...(e.target.value==='HOLD_UNTIL_REPLACED'&&!draft.channel?{channel:`${draft.groups[0]?.actions[0]?.target||'world'}.${draft.groups[0]?.actions[0]?.type||'effect'}`}:{})})}><option value="AUTO_CLOSE_ON_FLOW_END">{tr("Завершить событие")}</option><option value="HOLD_UNTIL_STOPPED">{tr("Продолжать до команды «Стоп»")}</option><option value="HOLD_UNTIL_REPLACED">{tr("Продолжать до замены")}</option></select></label>{draft.retention==='HOLD_UNTIL_REPLACED'&&<label className="al-field">{tr("Роль для замены")}<input value={draft.channel||''} onChange={e=>patch({channel:e.target.value})}/><small>{tr("Новое событие с этой ролью заменит текущее.")}</small></label>}<label className="al-field">{tr("Где может продолжаться")}<select value={draft.owner||'SubScene'} onChange={e=>patch({owner:e.target.value})}><option value="SubScene">{tr("В этой сабсцене")}</option><option value="Scene">{tr("В пределах сцены")}</option><option value="GameSession">{tr("В течение игры")}</option></select></label></div></details>}
              {placements>0&&<p className="al-placements"><Icon name="Link" size={14}/>{tr("Используется в репликах: ")}{placements}{tr(". Сохранение обновит их шаблон.")}</p>}
            </>}
          </aside>
        </div>
      </div>}
    </div>
    {picker&&!hidden&&<ActionPicker mode={picker.mode} project={project} scene={scene} onClose={()=>setPicker(null)} onAction={chooseAction} onGroup={chooseGroup}/>}
  </div>;
}

function ActionCard({action,project,scene,selected,onSelect}) {
 useLocale();
  const data=describeAction(action,project,scene);
  return <button className={`al-action-card ${selected?'selected':''}`} aria-pressed={selected} onClick={onSelect}>
    <div className="al-card-top"><span className="al-command-icon"><Icon name={TYPES[action.type]?.icon} size={20}/></span><span>{tr(TYPES[action.type]?.label)}</span><Icon name={selected?'SlidersHorizontal':'ChevronRight'} size={14}/></div>
    <strong>{data.subject}</strong><span className="al-card-result">{action.type==='move'&&<Icon name="ArrowRight" size={14}/>} {data.result}</span>
    <small><Icon name={TYPES[action.type]?.completion==='CONTINUOUS'?'Infinity':TYPES[action.type]?.completion==='FINITE'?'Clock3':'Zap'} size={13}/>{message(data.detail)}</small>
  </button>;
}

function ActionStage({action,project,scene}) {
 useLocale();
  const data=describeAction(action,project,scene),moving=action.type==='move';
  return <div className="al-action-stage"><div className="al-canvas-heading"><div><h2>{tr("Одна команда")}</h2><span>{tr(ACTION_HINTS[action.type])}</span></div></div><div className="al-result-preview"><span className="al-preview-label">{tr("РЕЗУЛЬТАТ ДЕЙСТВИЯ")}</span><div className="al-result-diagram"><div className="al-result-object"><div><Icon name={moving?(project.objects.find(o=>o.id===action.target)?.type==='Персонаж'?'PersonStanding':'Box'):action.target==='world'&&action.type!=='wait'?'Globe2':TYPES[action.type]?.icon} size={36}/></div><strong>{data.subject}</strong></div><div className="al-result-path"><span/><Icon name={moving?'Footprints':'ArrowRight'} size={22}/><span/><Icon name="ChevronRight" size={16}/></div><div className="al-result-destination"><div><Icon name={data.icon} size={36}/></div><strong>{data.result}</strong></div></div><span className="al-result-time"><Icon name={TYPES[action.type]?.completion==='CONTINUOUS'?'Infinity':'Clock3'} size={15}/>{message(data.detail)}</span></div><p className="al-action-caption"><Icon name="SlidersHorizontal" size={16}/>{tr("Настройте команду в панели параметров.")}</p></div>;
}

function MiniStructure({kind}) {
 useLocale();
  return <div className={`al-mini-structure ${kind}`} aria-hidden="true">{kind==='action'?<span><Icon name="Footprints" size={23}/></span>:kind==='group'?<div><span><Icon name="Footprints" size={18}/></span><span><Icon name="Volume2" size={18}/></span></div>:<><div><span><Icon name="Footprints" size={17}/></span><span><Icon name="Volume2" size={17}/></span></div><Icon name="ArrowRight" size={16}/><div><span><Icon name="Video" size={17}/></span></div></>}</div>;
}
function ConceptMap({onKind}) {
 useLocale();
  return <div className="al-concepts">{['action','group','event'].map((k,i)=><React.Fragment key={k}>{i>0&&<Icon name="ArrowRight" size={18}/>}<button disabled={!onKind} onClick={()=>onKind?.(k)}><MiniStructure kind={k}/><strong>{tr(KINDS[k].label)}</strong><small>{tr(KINDS[k].hint)}</small></button></React.Fragment>)}</div>;
}
function Welcome({kind,onCreate,onKind}) {
 useLocale();
  const [demo,setDemo]=useState(0);
  useEffect(()=>{if(!demo)return;const timer=setTimeout(()=>setDemo(n=>n===3?0:n+1),1400);return()=>clearTimeout(timer);},[demo]);
  return <div className="al-welcome"><span className="al-eyebrow">{tr("ОТ КОМАНДЫ К ИСТОРИИ")}</span><h2>{kind==='event'?tr("Что произойдёт в вашей сцене?"):kind==='group'?tr("В один момент. Вместе."):tr("Одно действие — один результат.")}</h2><p>{kind==='event'?tr("Соберите событие из простых команд. Используйте его в любой реплике."):kind==='group'?tr("Объедините движение, звук и другие команды в одну группу."):tr("Переместить героя, включить музыку или изменить свет.")}</p><ConceptMap onKind={onKind}/><div className="al-demo"><header><span><Icon name="Layers" size={18}/><strong>{tr("Алиса подходит к окну")}</strong><small>{tr("Пример события")}</small></span><button onClick={()=>setDemo(demo?0:1)} aria-label={demo?tr("Остановить пример"):tr("Показать порядок запуска")}><Icon name={demo?'Square':'Play'} size={15}/>{demo?tr("Стоп"):tr("Как запустится")}</button></header><div className="al-demo-flow"><div className={`al-demo-group ${demo===1?'playing':''}`}><div><b>01</b><strong>{tr("Подойти к окну")}</strong><small>{tr("Вместе")}</small></div><div className="al-demo-commands"><span><Icon name="Footprints" size={22}/><strong>{tr("Алиса идёт")}</strong><small>{tr("К окну · 2 сек")}</small></span><span><Icon name="Volume2" size={22}/><strong>{tr("Звучат шаги")}</strong><small>{tr("Пока идёт")}</small></span></div></div><div className="al-demo-arrow"><Icon name="ArrowRight" size={22}/><span>{tr("Затем")}</span></div><div className={`al-demo-group single ${demo===2?'playing':''}`}><div><b>02</b><strong>{tr("Показать эмоцию")}</strong></div><div className="al-demo-commands"><span><Icon name="Video" size={22}/><strong>{tr("Крупный план")}</strong><small>{tr("Камера на Алису")}</small></span></div></div></div><footer aria-live="polite">{demo===1?<><Icon name="Columns2" size={15}/>{tr("Движение и звук начинаются вместе")}</>:demo===2?<><Icon name="ArrowRight" size={15}/>{tr("Теперь камера: первая группа завершена")}</>:demo===3?<><Icon name="Check" size={15}/>{tr("Событие завершено")}</>:<><Icon name="MousePointer2" size={15}/>{tr("Команда → вместе в группе → по очереди в событии")}</>}</footer></div><div className="al-welcome-action"><Button icon="Plus" className="al-primary" onClick={onCreate}>{KINDS[kind].create}</Button><span>{kind==='event'?tr("Начните с одного действия"):kind==='group'?tr("Выберите команды для общего старта"):tr("Выберите, что должно измениться")}</span></div></div>;
}

function ActionPicker({mode,project,scene,onClose,onAction,onGroup}) {
 useLocale();
  const dialog=useRef(),[category,setCategory]=useState('staging'),[query,setQuery]=useState('');
  const [source,setSource]=useState('commands');
  useEffect(()=>{const node=dialog.current;node.showModal();node.querySelector('input')?.focus();return()=>node.close();},[]);
  const matches=(text)=>text.toLowerCase().includes(query.toLowerCase());
  const types=query?Object.keys(TYPES).filter(t=>matches(tr(TYPES[t].label)+' '+tr(ACTION_HINTS[t]))):ACTION_CATEGORIES.find(c=>c.id===category).types;
  const templates=project.actionTemplates.filter(a=>matches(a.name)),groups=project.groupTemplates.filter(g=>matches(g.name));
  const title=mode==='replace'?'Что должно произойти?':mode==='next'?'Что произойдёт следующим?':'Добавить действие';
  return <dialog ref={dialog} className="al-picker" aria-labelledby="al-picker-title" onKeyDown={e=>e.stopPropagation()} onCancel={e=>{e.preventDefault();onClose();}} onClick={e=>{if(e.target===e.currentTarget)onClose();}}>
    <div className="al-picker-content"><header><div><span className="al-eyebrow">{mode==='next'?tr("НОВАЯ ГРУППА · СЛЕДУЮЩИЙ ШАГ"):tr("ОДНА КАРТОЧКА · ОДНА КОМАНДА")}</span><h2 id="al-picker-title">{tr(title)}</h2></div><Button icon="X" title={tr("Закрыть выбор действия")} onClick={onClose}/></header><label className="al-picker-search"><Icon name="Search" size={19}/><input autoFocus aria-label={tr("Поиск команд и шаблонов")} placeholder={tr("Например, музыка или переместить")} value={query} onChange={e=>setQuery(e.target.value)}/></label>
      <div className="al-picker-sources"><button className={source==='commands'?'active':''} aria-pressed={source==='commands'} onClick={()=>setSource('commands')}>{tr("Команды")}</button>{mode!=='replace'&&<button className={source==='templates'?'active':''} aria-pressed={source==='templates'} onClick={()=>setSource('templates')}>{tr("Мои действия ")}<small>{project.actionTemplates.length}</small></button>}{mode==='next'&&<button className={source==='groups'?'active':''} aria-pressed={source==='groups'} onClick={()=>setSource('groups')}>{tr("Готовые группы ")}<small>{project.groupTemplates.length}</small></button>}</div>
      <div className={`al-picker-body ${source!=='commands'?'templates':''}`}>
        {source==='commands'?<><nav aria-label={tr("Категории команд")}>{ACTION_CATEGORIES.map(c=><button key={c.id} aria-pressed={category===c.id&&!query} className={category===c.id&&!query?'active':''} onClick={()=>{setCategory(c.id);setQuery('');}}><Icon name={c.icon} size={18}/>{tr(c.label)}</button>)}</nav><div className="al-command-list">{types.map(type=>{const unavailable=actionUnavailable(type,project,scene);return <button key={type} disabled={!!unavailable} onClick={()=>onAction(type)}><span className="al-command-icon"><Icon name={TYPES[type].icon} size={23}/></span><span><strong>{tr(TYPES[type].label)}</strong><small>{unavailable||tr(ACTION_HINTS[type])}</small></span><Icon name="Plus" size={16}/></button>;})}{!types.length&&<p className="al-picker-empty">{tr("Команда не найдена. Попробуйте «звук» или «свет».")}</p>}</div></>:<div className="al-command-list">{(source==='templates'?templates:groups).map(a=><button key={a.id} onClick={()=>source==='templates'?onAction(a.type,a):onGroup(a)}><span className="al-command-icon"><Icon name={source==='groups'?'Columns2':TYPES[a.type]?.icon} size={22}/></span><span><strong>{a.name}</strong><small>{source==='groups'?tr("{0} одновременно", [actionCount(a.actions.length)]):tr(TYPES[a.type]?.label)}</small></span><Icon name="Plus" size={16}/></button>)}{!(source==='templates'?templates:groups).length&&<p className="al-picker-empty">{query?tr("Ничего не найдено. Измените запрос."):tr("Здесь появятся сохранённые шаблоны. Начните с команды.")}</p>}</div>}
      </div><footer><Icon name={mode==='next'?'ArrowDown':'BookCopy'} size={15}/>{mode==='next'?tr("Новая группа запустится после предыдущей."):source==='templates'?tr("Вставится копия. Исходный шаблон сохранится."):tr("Параметры настроите после добавления.")}<button onClick={onClose}>{tr("Отмена ")}<kbd>Esc</kbd></button></footer>
    </div>
  </dialog>;
}
