import {t as tr, useLocale, message} from './i18n.jsx';
import React,{useEffect,useRef,useState} from 'react';
import {Icon,Button,Field} from './StudioParts.jsx';
import './projectDialog.css';

export default function ProjectDialog({mode,project,templates,onSubmit,onClose,busy,error}) {
 useLocale();
  const [name,setName]=useState(mode==='new'?tr('Новый проект'):mode==='saveAs'?`${project.title} — ${tr('копия')}`:project.title);
  const [templateId,setTemplateId]=useState('');
  const dialog=useRef();
  useEffect(()=>{const previous=document.activeElement;dialog.current?.querySelector('input')?.select();return()=>previous?.focus?.();},[]);
  const title=mode==='new'?tr('Новый проект'):mode==='saveAs'?'Сохранить проект как…':'Записать проект как шаблон';
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
      <header><Icon name={mode==='new'?'FilePlus2':mode==='template'?'BookCopy':'Save'}/><h2 id="project-dialog-title">{tr(title)}</h2><Button icon="X" type="button" aria-label={tr("Закрыть")} disabled={busy} onClick={onClose}/></header>
      <Field label={mode==='template'?tr("Название шаблона"):tr("Название проекта")}><input autoFocus required maxLength={120} value={name} disabled={busy} onChange={e=>setName(e.target.value)}/></Field>
      {mode==='new'&&<Field label={tr("Основа проекта")}><select aria-label={tr("Основа проекта")} value={templateId} disabled={busy} onChange={e=>setTemplateId(e.target.value)}><option value="">{tr("Пустой проект")}</option>{templates.map(t=><option key={t.id} value={t.id}>{t.name}</option>)}</select></Field>}
      <p>{mode==='new'?templateId?tr("Создаст независимую копию шаблона со сценами, событиями и настройками."):tr("Одна пустая сцена, камера и строка сценария для начала работы. Без персонажей, декораций и переменных. Стандартные события доступны в библиотеке."):mode==='template'?tr("Сохранит сцены, объекты, события, звук и расположение узлов. Шаблон появится в окне нового проекта; файл можно выгрузить отдельно."):tr("Сохранит отдельную копию проекта под новым названием и сделает её текущей.")}</p>
      {mode==='new'&&<small>{tr("Текущий проект останется в резервной копии: «Файл → Восстановить предыдущий проект».")}</small>}
      {error&&<p className="project-dialog-error" role="alert">{message(error)}</p>}
      <footer><Button type="button" disabled={busy} onClick={onClose}>{tr("Отмена")}</Button><Button type="submit" className="primary" disabled={busy||!name.trim()} icon={busy?'LoaderCircle':mode==='new'?'Plus':'Save'}>{busy?tr("Сохранение…"):mode==='new'?tr("Создать проект"):mode==='template'?tr("Записать шаблон"):tr("Сохранить копию")}</Button></footer>
    </form>
  </div>;
}
