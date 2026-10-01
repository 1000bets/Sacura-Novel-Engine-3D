import React,{useState} from 'react';
import {Icon,Button,Field} from './StudioParts.jsx';
import Dialog from './Dialog.jsx';

export default function ProjectDialog({mode,project,templates,onSubmit,onClose,busy,error}) {
  const [name,setName]=useState(mode==='new'?'Новый проект':mode==='saveAs'?`${project.title} — копия`:project.title);
  const [templateId,setTemplateId]=useState('');
  const title=mode==='new'?'Новый проект':mode==='saveAs'?'Сохранить проект как…':'Записать проект как шаблон';
  return <Dialog title={title} icon={()=> <Icon name={mode==='new'?'FilePlus2':mode==='template'?'BookCopy':'Save'}/>} busy={busy} onClose={onClose} onSubmit={()=>{if(!busy&&name.trim())onSubmit({name,templateId});}}>
      <Field label={mode==='template'?'Название шаблона':'Название проекта'}><input autoFocus required maxLength={120} value={name} disabled={busy} onChange={e=>setName(e.target.value)}/></Field>
      {mode==='new'&&<Field label="Основа проекта"><select aria-label="Основа проекта" value={templateId} disabled={busy} onChange={e=>setTemplateId(e.target.value)}><option value="">Пустой проект</option>{templates.map(t=><option key={t.id} value={t.id}>{t.name}</option>)}</select></Field>}
      <p>{mode==='new'?templateId?'Создаст независимую копию шаблона со сценами, событиями и настройками.':'Одна пустая сцена, камера и строка сценария для начала работы. Без персонажей, декораций и демонстрационных событий.':mode==='template'?'Сохранит сцены, объекты, события, звук и расположение узлов. Шаблон появится в окне нового проекта; файл можно выгрузить отдельно.':'Сохранит отдельную копию проекта под новым названием и сделает её текущей.'}</p>
      {mode==='new'&&<small>Текущий проект останется в резервной копии: «Файл → Восстановить предыдущий проект».</small>}
      {error&&<p className="project-dialog-error" role="alert">{error}</p>}
      <footer><Button type="button" disabled={busy} onClick={onClose}>Отмена</Button><Button type="submit" className="primary" disabled={busy||!name.trim()} icon={busy?'LoaderCircle':mode==='new'?'Plus':'Save'}>{busy?'Сохранение…':mode==='new'?'Создать проект':mode==='template'?'Записать шаблон':'Сохранить копию'}</Button></footer>
  </Dialog>;
}
