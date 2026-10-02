import React,{useEffect,useRef,useState} from 'react';
import {Flower2,Plus,BookOpen,FolderOpen,ArrowRight,LogOut,Upload} from 'lucide-react';
import Editor from './Editor.jsx';
import ThemePicker from './ThemePicker.jsx';
import {accountStorage} from './accountStorage.js';
import {serverStorageEnabled,listServerProjects,listOtherServerProjects,openServerProject,saveServerProject,flushServerSaves} from './serverStorage.js';
import {createEmptyProject,createStandardProject,listLocalProjects,rememberLocalProject,parseProjectFile} from './projectLifecycle.js';
import {readProject} from './projectFiles.js';
import './projectHome.css';

export default function ProjectHome({user=null,onLogout}) {
 const [active,setActive]=useState(null),[projects,setProjects]=useState([]),[otherProjects,setOtherProjects]=useState([]),[loading,setLoading]=useState(true),[busy,setBusy]=useState(false),[error,setError]=useState(''),[creating,setCreating]=useState(false),[name,setName]=useState('Новый проект');
 const operation=useRef(false),fileInput=useRef(null),newButton=useRef(null);
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
  try{await action();}catch(err){setError(err.message||'Не удалось открыть проект.');}
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
 function cancelCreate(){setCreating(false);newButton.current?.focus();}
 if(active)return <Editor key={active.id} initialProject={active} user={user} onLogout={onLogout} onHome={home}/>;
 return <main className="project-home">
  <header className="project-home-header"><div className="project-home-brand"><Flower2 aria-hidden="true" size={28}/><span>Sacura <small>Novel Studio</small></span></div><div className="project-home-account"><ThemePicker/>{user&&<><span>{user.login}</span><button disabled={busy} onClick={()=>run(()=>onLogout())}><LogOut aria-hidden="true" size={16}/>Выйти</button></>}</div></header>
  <div className="project-home-content">
   <p className="project-home-eyebrow">ВАША СЛЕДУЮЩАЯ ИСТОРИЯ</p><h1>С чего начнём?</h1><p className="project-home-intro">Создайте новую визуальную новеллу или продолжите свой проект.</p>
   <section className="project-home-actions" aria-label="Начать работу">
    <button ref={newButton} className="project-home-action" disabled={busy} onClick={()=>{setCreating(true);setError('');}}><Plus aria-hidden="true" size={26}/><strong>Создать новый проект</strong><span>Пустая сцена и чистый сценарий для вашей истории.</span><span className="project-home-link">Начать с нуля <ArrowRight aria-hidden="true" size={16}/></span></button>
    <button className="project-home-action" disabled={busy} onClick={()=>run(()=>open(createStandardProject(),{save:true}))}><BookOpen aria-hidden="true" size={26}/><strong>Загрузить стандартный шаблон</strong><span>«Письма после дождя»: готовый сюжет, выборы и три озвученные реплики с музыкой.</span><span className="project-home-link">Открыть шаблон <ArrowRight aria-hidden="true" size={16}/></span></button>
    <a className="project-home-action" href="#previous-projects"><FolderOpen aria-hidden="true" size={26}/><strong>Запустить прошлый проект</strong><span>Вернитесь к сохранённой истории и продолжите работу.</span><span className="project-home-link">Выбрать проект <ArrowRight aria-hidden="true" size={16}/></span></a>
   </section>
   {creating&&<form className="project-home-create" onSubmit={e=>{e.preventDefault();run(()=>open(createEmptyProject(name),{save:true}));}} onKeyDown={e=>{if(e.key==='Escape'&&!busy)cancelCreate();}}><label htmlFor="new-project-name">Название нового проекта</label><div><input id="new-project-name" autoFocus required maxLength={120} disabled={busy} value={name} onChange={e=>setName(e.target.value)}/><button className="project-home-primary" disabled={busy||!name.trim()} type="submit">Создать</button><button disabled={busy} type="button" onClick={cancelCreate}>Отмена</button></div></form>}
   {error&&<p className="project-home-error" role="alert">{error}</p>}{busy&&<p role="status">Подождите, сохраняем и открываем проект…</p>}
   <section id="previous-projects" className="project-home-recent" aria-labelledby="recent-title"><div className="project-home-section-heading"><h2 id="recent-title">Ваши проекты</h2><button disabled={busy||loading} onClick={refresh}>Обновить список</button></div>
    {loading?<p role="status">Загрузка проектов…</p>:projects.length?<div className="project-home-list">{projects.map(project=><button key={project.id} disabled={busy} onClick={()=>run(async()=>open(serverStorageEnabled?await openServerProject(project.id):project.project))}><FolderOpen aria-hidden="true" size={20}/><span><strong>{project.title}</strong><small>{project.updatedAt?'Сохранён '+new Date(project.updatedAt).toLocaleString('ru-RU'):'Сохранён в этом браузере'}</small></span><span className="project-home-open">Открыть <ArrowRight aria-hidden="true" size={16}/></span></button>)}</div>:<p className="project-home-empty">Пока нет сохранённых проектов. Создайте новый или начните со стандартного шаблона.</p>}
    <input ref={fileInput} type="file" hidden accept="application/json,.json" onChange={e=>{const file=e.target.files?.[0];e.target.value='';if(file)run(async()=>open(parseProjectFile(await file.text()).project,{save:true}));}}/><button className="project-home-import" disabled={busy} onClick={()=>fileInput.current?.click()}><Upload aria-hidden="true" size={16}/>Загрузить проект из файла</button>
   </section>
   {serverStorageEnabled&&user?.login==='syper'&&<section className="project-home-recent" aria-labelledby="other-projects-title"><div className="project-home-section-heading"><h2 id="other-projects-title">Чужие проекты</h2></div>
    {loading?<p role="status">Загрузка проектов…</p>:otherProjects.length?<div className="project-home-list">{otherProjects.map(project=><button key={project.ownerId+':'+project.id} disabled={busy} onClick={()=>run(async()=>open(await openServerProject(project.id,project.ownerId)))}><FolderOpen aria-hidden="true" size={20}/><span><strong>{project.title}</strong><small>Автор: {project.ownerLogin}</small><small>Сохранён {new Date(project.updatedAt).toLocaleString('ru-RU')}</small></span><span className="project-home-open">Открыть <ArrowRight aria-hidden="true" size={16}/></span></button>)}</div>:<p className="project-home-empty">У других пользователей пока нет сохранённых проектов.</p>}
   </section>}
  </div>
 </main>;
}
