import {builtinTemplates,loadBuiltinTemplate,createBuiltinProject} from './builtinTemplates.js';
import {instantiateAuthoringTemplate} from './authoringTemplates.js';
import LanguagePicker from './LanguagePicker.jsx';
import {t as tr, useLocale, localeTag, message} from './i18n.jsx';
import React,{useEffect,useRef,useState} from 'react';
import {Flower2,Plus,FolderOpen,ArrowRight,LogOut,Upload,UserPlus,Search,RotateCw,Clock3,BookOpen,X,Monitor,ArrowUpRight,ChevronDown} from 'lucide-react';
import InviteDialog from './InviteDialog.jsx';
import Dialog from './Dialog.jsx';
import Editor from './Editor.jsx';
import ThemePicker from './ThemePicker.jsx';
import ProjectArtwork from './ProjectArtwork.jsx';
import {accountStorage} from './accountStorage.js';
import {serverStorageEnabled,listServerProjects,listOtherServerProjects,openServerProject,saveServerProject,flushServerSaves} from './serverStorage.js';
import {createEmptyProject,createStandardProject,listLocalProjects,rememberLocalProject,parseProjectFile} from './projectLifecycle.js';
import {readProject} from './projectFiles.js';
import './projectHome.css';

const templatePresentation={
 'builtin-quest':{art:'quest',label:'Квест',detail:'Point & click',icon:'01'},
 'builtin-shooter':{art:'shooter',label:'Шутер',detail:'FPS',icon:'02'},
 'builtin-novel':{art:'novel',label:'Визуальная новелла',detail:'Диалоги и выборы',icon:'03'},
 'builtin-horror':{art:'horror',label:'Последний обход',detail:'Point & click · Хоррор',icon:'04'},
 'builtin-clicker':{art:'clicker',label:'UI-кликер',detail:'Интерфейс и механики',icon:'05'}
};

export default function ProjectHome({user=null,onLogout}) {
 useLocale();
 const [active,setActive]=useState(null),[projects,setProjects]=useState([]),[otherProjects,setOtherProjects]=useState([]),[loading,setLoading]=useState(true),[busy,setBusy]=useState(false),[error,setError]=useState(''),[creating,setCreating]=useState(false),[name,setName]=useState(()=>tr('Новый проект'));
 const [inviting,setInviting]=useState(false),[guide,setGuide]=useState(null),[search,setSearch]=useState('');
 const operation=useRef(false),fileInput=useRef(null),createInput=useRef(null);
 useEffect(()=>{if(creating){createInput.current?.focus();createInput.current?.select();}},[creating]);
 async function refresh(){
  setLoading(true);setError('');
  try{
   const [own,others]=await Promise.all([serverStorageEnabled?listServerProjects():listLocalProjects(accountStorage),serverStorageEnabled&&user?.login==='syper'?listOtherServerProjects():[]]);
   setProjects(own);setOtherProjects(others);
  }
  catch(err){setError(err.message);}
  finally{setLoading(false);}
 }
 useEffect(()=>{refresh();},[]);
 async function run(action){
  if(operation.current)return;
  operation.current=true;setBusy(true);setError('');
  try{await action();}catch(err){setError(err.message||tr('Не удалось открыть проект.'));}
  finally{operation.current=false;setBusy(false);}
 }
 async function open(project,{save=false}={}){
  let next=readProject(project);
  if(save&&serverStorageEnabled)next=await saveServerProject(next,null);
  if(!serverStorageEnabled)rememberLocalProject(accountStorage,next);
  setCreating(false);setActive(next);
 }
 async function home(project){
  if(serverStorageEnabled){await saveServerProject(project);await flushServerSaves();}
  else rememberLocalProject(accountStorage,project);
  setActive(null);await refresh();
 }
 function beginCreate(){setName(tr('Новый проект'));setCreating(true);setError('');}
 const filteredProjects=projects.filter(project=>project.title.toLocaleLowerCase(localeTag()).includes(search.trim().toLocaleLowerCase(localeTag())));
 const mainTemplates=builtinTemplates.filter(t=>!t.featured&&!t.extra),extraTemplates=builtinTemplates.filter(t=>t.extra);
 const templateCard=template=>{
  const presentation=templatePresentation[template.id];
  return <article key={template.id} className="project-template-entry">
   <button className="project-template-open" disabled={busy} aria-label={tr('Открыть шаблон: {0}',[tr(template.name)])} onClick={()=>run(async()=>open(await createBuiltinProject(template.id),{save:true}))}>
    <div className="project-template-art"><ProjectArtwork variant={presentation.art}/><span className="project-template-number">{presentation.icon}</span><span className="project-template-arrow"><ArrowUpRight aria-hidden="true" size={20}/></span></div>
    <div className="project-template-copy"><span className="project-template-genre">{tr(presentation.detail)}</span><h3>{tr(presentation.label)}</h3><p>{tr(template.description)}</p></div>
   </button>
   {template.guide&&<button className="project-template-help" onClick={()=>setGuide(template)}><BookOpen aria-hidden="true" size={14}/>{tr('Разбор шаблона')}<ArrowRight aria-hidden="true" size={14}/></button>}
  </article>;
 };
 if(active)return <Editor key={active.id} initialProject={active} user={user} onLogout={onLogout} onHome={home}/>;
 return <main className="project-home">
  <header className="project-home-header">
   <a className="project-home-brand" href="#home-top" aria-label={tr('Sacura — главная')}><span className="project-home-brand-mark"><Flower2 aria-hidden="true" size={27} strokeWidth={1.5}/></span><span>Sacura <small>Novel Studio</small></span></a>
   <nav className="project-home-nav" aria-label={tr('Главная навигация')}><a href="#previous-projects">{tr('Мои проекты')}</a><a href="#project-templates">{tr('Шаблоны')}</a></nav>
   <div className="project-home-account"><LanguagePicker/><ThemePicker/>{user&&<><span className="project-home-user">{user.login}</span>{serverStorageEnabled&&<button className="project-home-icon-button" disabled={busy} title={tr('Пригласить пользователя')} aria-label={tr('Пригласить пользователя')} onClick={()=>setInviting(true)}><UserPlus aria-hidden="true" size={18}/></button>}<button className="project-home-icon-button" title={tr('Выйти')} aria-label={tr('Выйти')} disabled={busy} onClick={()=>run(()=>onLogout())}><LogOut aria-hidden="true" size={18}/></button></>}</div>
  </header>
  <div className="project-home-content" id="home-top">
   <section className="project-home-welcome" aria-labelledby="home-title">
    <div className="project-home-greeting">
     <h1 id="home-title">{tr('Место для ваших')}<br/><span className="project-home-title-accent">{tr('историй.')}</span></h1>
     <p className="project-home-intro">{tr('От первой сцены до целого мира. Создавайте игры и рассказывайте истории в Sacura.')}</p>
     <div className="project-home-start-actions"><button className="project-home-primary" disabled={busy} onClick={beginCreate}><Plus aria-hidden="true" size={19}/>{tr('Создать новый проект')}</button><button className="project-home-file" disabled={busy} onClick={()=>fileInput.current?.click()}><FolderOpen aria-hidden="true" size={18}/>{tr('Открыть из файла')}</button></div>
    </div>
    <article className="project-home-featured">
     <ProjectArtwork/>
     <div className="project-home-featured-top"><span><BookOpen aria-hidden="true" size={14}/>{tr('Знакомство с движком')}</span><span>3D</span></div>
     <div className="project-home-featured-caption"><div><p>{tr('СТАНДАРТНЫЙ ШАБЛОН')}</p><h2>{tr('Письма после дождя')}</h2><span>{tr('История, музыка и мир, который можно исследовать.')}</span></div><button disabled={busy} className="project-home-featured-open" aria-label={tr('Открыть «Письма после дождя»')} title={tr('Открыть шаблон')} onClick={()=>run(async()=>open(await createBuiltinProject('builtin-standard'),{save:true}))}><ArrowUpRight aria-hidden="true" size={23}/></button></div>
    </article>
   </section>

   {error&&!creating&&<p className="project-home-error" role="alert">{message(error)}</p>}
   <section id="project-templates" className="project-home-templates" aria-labelledby="templates-title">
    <div className="project-home-section-heading"><div><h2 id="templates-title">{tr('Начните с готовой идеи')}</h2></div><span className="project-home-section-meta">{tr('{0} шаблона · разные жанры',[mainTemplates.length])}</span></div>
    <div className="project-home-template-grid">{mainTemplates.map(templateCard)}</div>
    {extraTemplates.length>0&&<details className="project-home-extra"><summary>{tr('Ещё')}<span>{tr('Дополнительные примеры')}</span><ChevronDown aria-hidden="true" size={18}/></summary><div className="project-home-template-grid">{extraTemplates.map(templateCard)}</div></details>}
   </section>

   <section id="previous-projects" className="project-home-recent" aria-labelledby="recent-title">
    <div className="project-home-section-heading"><div className="project-home-recent-title"><h2 id="recent-title">{tr('Мои проекты')}</h2>{!loading&&<span className="project-home-count">{projects.length}</span>}</div><div className="project-home-project-tools"><label className="project-home-search"><Search aria-hidden="true" size={17}/><span className="project-home-sr-only">{tr('Найти проект')}</span><input type="search" aria-label={tr('Найти проект')} placeholder={tr('Найти проект…')} value={search} onChange={event=>setSearch(event.target.value)}/>{search&&<button className="project-home-icon-button" aria-label={tr('Очистить поиск')} onClick={()=>setSearch('')}><X aria-hidden="true" size={15}/></button>}</label><button className="project-home-icon-button" disabled={busy||loading} title={tr('Обновить список')} aria-label={tr('Обновить список')} onClick={refresh}><RotateCw aria-hidden="true" size={17}/></button></div></div>
    {busy&&<p className="project-home-status" role="status">{tr('Подождите, сохраняем и открываем проект…')}</p>}
    {loading?<div className="project-home-loading" role="status"><span/>{tr('Загрузка проектов…')}</div>:projects.length?filteredProjects.length?<div className="project-home-list">
     <div className="project-home-list-heading" aria-hidden="true"><span>{tr('Название проекта')}</span><span>{tr('Последнее изменение')}</span><span/></div>
     {filteredProjects.map(project=><button key={project.id} disabled={busy} onClick={()=>run(async()=>open(serverStorageEnabled?await openServerProject(project.id):project.project))}><span className="project-home-project-icon"><FolderOpen aria-hidden="true" size={21}/></span><strong>{project.title}</strong><time dateTime={project.updatedAt||undefined}><Clock3 aria-hidden="true" size={13}/>{project.updatedAt?new Date(project.updatedAt).toLocaleString(localeTag(),{day:'numeric',month:'short',year:'numeric',hour:'2-digit',minute:'2-digit'}):tr('Сохранён в этом браузере')}</time><span className="project-home-open">{tr('Открыть')}<ArrowUpRight aria-hidden="true" size={17}/></span></button>)}
    </div>:<div className="project-home-empty"><Search aria-hidden="true" size={25}/><div><h3>{tr('Проекты не найдены')}</h3><p>{tr('Попробуйте другое название или очистите поиск.')}</p></div><button onClick={()=>setSearch('')}>{tr('Очистить поиск')}</button></div>:<div className="project-home-empty"><span className="project-home-empty-icon"><FolderOpen aria-hidden="true" size={27}/></span><div><h3>{tr('Здесь будут ваши проекты')}</h3><p>{tr('Создайте первый проект или откройте сохранённый файл.')}</p></div><button disabled={busy} onClick={()=>fileInput.current?.click()}><Upload aria-hidden="true" size={16}/>{tr('Загрузить проект из файла')}</button></div>}
   </section>

   {serverStorageEnabled&&user?.login==='syper'&&<section className="project-home-recent" aria-labelledby="other-projects-title"><div className="project-home-section-heading"><h2 id="other-projects-title">{tr('Чужие проекты')}</h2></div>
    {loading?<p role="status">{tr('Загрузка проектов…')}</p>:otherProjects.length?<div className="project-home-list">{otherProjects.map(project=><button key={project.ownerId+':'+project.id} disabled={busy} onClick={()=>run(async()=>open(await openServerProject(project.id,project.ownerId)))}><span className="project-home-project-icon"><FolderOpen aria-hidden="true" size={21}/></span><strong>{project.title}<small>{tr('Автор: ')}{project.ownerLogin}</small></strong><time dateTime={project.updatedAt}>{new Date(project.updatedAt).toLocaleDateString(localeTag())}</time><span className="project-home-open">{tr('Открыть')}<ArrowUpRight aria-hidden="true" size={17}/></span></button>)}</div>:<p className="project-home-empty">{tr('У других пользователей пока нет сохранённых проектов.')}</p>}
   </section>}
   <footer className="project-home-footer"><span><Monitor aria-hidden="true" size={14}/>{serverStorageEnabled?tr('Рабочее пространство Sacura'):tr('Проекты сохраняются в этом браузере')}</span><span>Sacura Novel Studio</span></footer>
   <input ref={fileInput} type="file" hidden accept="application/json,.json" onChange={e=>{const file=e.target.files?.[0];e.target.value='';if(file)run(async()=>open((await parseProjectFile(await file.text())).project,{save:true}));}}/>
  </div>
  {creating&&<Dialog title={tr('Создать новый проект')} icon={Plus} busy={busy} onClose={()=>setCreating(false)} onSubmit={()=>{if(name.trim())run(()=>open(createEmptyProject(name),{save:true}));}}><label className="field" htmlFor="new-project-name">{tr('Название нового проекта')}<input ref={createInput} id="new-project-name" required maxLength={120} disabled={busy} value={name} onChange={e=>setName(e.target.value)}/></label><p>{tr('Пустая сцена и чистый сценарий для вашей истории.')}</p>{error&&<p className="project-dialog-error" role="alert">{message(error)}</p>}<footer><button disabled={busy} type="button" onClick={()=>setCreating(false)}>{tr('Отмена')}</button><button className="project-home-primary" disabled={busy||!name.trim()} type="submit">{busy?tr('Создаём…'):tr('Создать')}</button></footer></Dialog>}
  {guide&&<Dialog title={tr('Разбор шаблона')} icon={BookOpen} onClose={()=>setGuide(null)}><h3 className="project-home-guide-title">{tr(guide.name)}</h3><ol className="project-home-guide-steps">{guide.guide.map(step=><li key={step}>{tr(step)}</li>)}</ol><footer><button type="button" onClick={()=>setGuide(null)}>{tr('Закрыть разбор')}</button></footer></Dialog>}
  {inviting&&<InviteDialog onClose={()=>setInviting(false)}/>}
 </main>;
}
