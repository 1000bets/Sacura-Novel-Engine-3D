import React,{useEffect,useId,useRef} from 'react';
import {X} from 'lucide-react';
import {t as tr,useLocale} from './i18n.jsx';
import './projectDialog.css';

export default function Dialog({title,icon:Icon,onClose,busy=false,onSubmit,children}){
 useLocale();
 const dialog=useRef(null),titleId=useId();
 useEffect(()=>{
  const previous=document.activeElement;
  const field=dialog.current.querySelector('input,button:not(:disabled)');
  (field||dialog.current).focus();field?.select?.();
  return()=>previous?.focus?.();
 },[]);
 function keyDown(event){
  event.stopPropagation();
  if(event.key==='Escape'&&!busy){event.preventDefault();onClose();}
  if(event.key==='Tab'){
   const fields=[...dialog.current.querySelectorAll(':is(button,input,select,a[href]):not(:disabled)')];
   if(!fields.length){event.preventDefault();dialog.current.focus();return;}
   const first=fields[0],last=fields.at(-1);
   if(document.activeElement===dialog.current){event.preventDefault();(event.shiftKey?last:first).focus();return;}
   if(event.shiftKey&&document.activeElement===first){event.preventDefault();last?.focus();}
   else if(!event.shiftKey&&document.activeElement===last){event.preventDefault();first?.focus();}
  }
 }
 return <div className="project-dialog-backdrop" onPointerDown={event=>{if(event.target===event.currentTarget&&!busy)onClose();}}>
  <form ref={dialog} className="project-dialog" tabIndex={-1} role="dialog" aria-modal="true" aria-labelledby={titleId} onKeyDown={keyDown} onSubmit={event=>{event.preventDefault();onSubmit?.();}}>
   <header>{Icon&&<Icon aria-hidden="true" size={20}/>}<h2 id={titleId}>{title}</h2><button type="button" aria-label={tr("Закрыть")} disabled={busy} onClick={onClose}><X aria-hidden="true" size={20}/></button></header>
   {children}
  </form>
 </div>;
}
