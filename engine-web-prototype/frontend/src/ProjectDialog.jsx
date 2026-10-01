import React,{useEffect,useRef,useState} from 'react';
import {Icon,Button,Field} from './StudioParts.jsx';
import './projectDialog.css';

export default function ProjectDialog({mode,project,templates,onSubmit,onClose,busy,error}) {
  const [name,setName]=useState(mode==='new'?'Новый проект':mode==='saveAs'?`${project.title} — копия`:project.title);
  const [templateId,setTemplateId]=useState('');
  const dialog=useRef();
  useEffect(()=>{const previous=document.activeElement;dialog.current?.querySelector('input')?.select();return()=>previous?.focus?.();},[]);
  const title=mode==='new'?'Новый проект':mode==='saveAs'?'Сохранить проект как…':'Записать проект как шаблон';
  const keyDown=e=>{
    e.stopPropagation();
    if(e.key==='Escape'&&!busy){e.preventDefault();onClose();}
    if(e.key==='Tab'){
      const fields=[...dialog.current.querySelectorAll('button:not(:disabled),input:not(:disabled),select:not(:disabled)')];
      const first=fields[0],last=fields.at(-1);
      if(e.shiftKey&&document.activeElement===first){e.preventDefault();last?.focus();}
      else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first?.focus();}
    }
  };
  return <div className="project-dialog-backdrop" onPointerDown={e=>{if(e.target===e.currentTarget&&!busy)onClose();}}>
    <form ref={dialog} className="project-dialog" role="dialog" aria-modal="true" aria-labelledby="project-dialog-title" onKeyDown={keyDown} onSubmit={e=>{e.preventDefault();if(!busy&&name.trim())onSubmit({name,templateId});}}>
      <header><Icon name={mode==='new'?'FilePlus2':mode==='template'?'BookCopy':'Save'}/><h2 id="project-dialog-title">{title}</h2><Button icon="X" type="button" aria-label="Закрыть" disabled={busy} onClick={onClose}/></header>
      <Field label={mode==='template'?'Название шаблона':'Название проекта'}><input autoFocus required maxLength={120} value={name} disabled={busy} onChange={e=>setName(e.target.value)}/></Field>
      {mode==='new'&&<Field label="Основа проекта"><select aria-label="Основа проекта" value={templateId} disabled={busy} onChange={e=>setTemplateId(e.target.value)}><option value="">Пустой проект</option>{templates.map(t=><option key={t.id} value={t.id}>{t.name}</option>)}</select></Field>}
      <p>{mode==='new'?templateId?'Создаст независимую копию шаблона со сценами, событиями и настройками.':'Одна пустая сцена, камера и строка сценария для начала работы. Без персонажей, декораций и демонстрационных событий.':mode==='template'?'Сохранит сцены, объекты, события, звук и расположение узлов. Шаблон появится в окне нового проекта; файл можно выгрузить отдельно.':'Сохранит отдельную копию проекта под новым названием и сделает её текущей.'}</p>
      {mode==='new'&&<small>Текущий проект останется в резервной копии: «Файл → Восстановить предыдущий проект».</small>}
      {error&&<p className="project-dialog-error" role="alert">{error}</p>}
      <footer><Button type="button" disabled={busy} onClick={onClose}>Отмена</Button><Button type="submit" className="primary" disabled={busy||!name.trim()} icon={busy?'LoaderCircle':mode==='new'?'Plus':'Save'}>{busy?'Сохранение…':mode==='new'?'Создать проект':mode==='template'?'Записать шаблон':'Сохранить копию'}</Button></footer>
    </form>
  </div>;
}
