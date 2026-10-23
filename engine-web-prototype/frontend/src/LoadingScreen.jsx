import React,{useEffect} from 'react';
import {Flower2} from 'lucide-react';

export default function LoadingScreen({label='Запускаем студию…',project}){
  useEffect(()=>{
    // Prevent editor shortcuts from changing a document during project transitions.
    const block=event=>{event.preventDefault();event.stopImmediatePropagation();};
    window.addEventListener('keydown',block,true);
    return()=>window.removeEventListener('keydown',block,true);
  },[]);
  return <div className="sacura-loading" data-project-loading aria-busy="true">
    <div className="sacura-loading-card">
      <div className="sacura-loading-mark" aria-hidden="true"><Flower2 strokeWidth={1.4}/></div>
      <h1>Sacura</h1><div className="sacura-loading-brand">NOVEL STUDIO</div>
      <div className="sacura-loading-track" aria-hidden="true"/>
      <p className="sacura-loading-status" role="status">{label}</p>
      {project&&<div className="sacura-loading-project">{project}</div>}
    </div>
    <div className="sacura-loading-footer">Место для ваших историй</div>
  </div>;
}
