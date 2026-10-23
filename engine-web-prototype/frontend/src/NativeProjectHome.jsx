import React,{useEffect,useMemo,useRef,useState} from 'react';
import Editor from './Editor.jsx';
import ProjectHome from './ProjectHome.jsx';
import LoadingScreen from './LoadingScreen.jsx';
import {createEmptyProject} from './projectLifecycle.js';

// Only shell defaults are local. Documents come from the Bridge.
export default function NativeProjectHome(){
  const api=window.sacuraDesktop;
  const [session,setSession]=useState(null);
  const seed=useRef(null);if(!seed.current)seed.current=createEmptyProject('Sacura');
  useEffect(()=>{
    let disposed=false,timer;
    const poll=async()=>{
      try {const next=await api.nativeSession();if(!disposed)setSession(old=>JSON.stringify(old)===JSON.stringify(next)?old:next);}
      catch(error){if(!disposed)setSession({phase:'error',error:error.message});}
      if(!disposed)timer=setTimeout(poll,300);
    };poll();return()=>{disposed=true;clearTimeout(timer);};
  },[]);
  const adapter=useMemo(()=>({
    starting:!session||session.phase==='starting',error:session?.error,
    listProjects:()=>api.nativeListProjects(),ready:()=>{document.title='Sacura Studio — Проекты';return api.ready('home');},
    createProject:(name,templateId)=>api.nativeCreateProject(name,templateId),forgetProject:id=>api.nativeForgetProject(id),
    openProject:()=>api.nativeOpenProject(),openRecent:id=>api.nativeOpenRecent(id),
  }),[session]);
  const loading=!session||session.phase==='starting'||!!session.loading;
  return <>
    <div style={{display:'contents'}} inert={loading?true:undefined}>
      {session?.phase==='ready'&&session.connected
        ? <Editor initialProject={seed.current} onHome={()=>api.nativeCloseProject()}/>
        : <ProjectHome nativeAdapter={adapter}/>}
    </div>
    {loading&&<LoadingScreen label={session?.loading?.label||(session?.phase==='starting'?'Загружаем сцену и запускаем движок…':'Запускаем студию…')} project={session?.loading?.project}/>}
  </>;
}
