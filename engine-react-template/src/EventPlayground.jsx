import React,{useState} from 'react';
import {Icon,Button} from './StudioParts.jsx';
import {TYPES} from './model.js';
export default function EventPlayground({project,preview,onPreview,onEdit,onAdd,scene}){
 const [category,setCategory]=useState('Все');
 const examples=project.events.filter(e=>e.category),categories=['Все',...new Set(examples.map(e=>e.category))];
 return <div className="event-playground"><header><div><strong>Проба событий</strong><span>Запустите в сцене, затем добавьте в нужную реплику.</span></div><Button icon="Play" onClick={()=>onPreview('visual-performance')}>Проиграть постановку</Button></header>
 <nav>{categories.map(c=><button key={c} className={category===c?'active':''} onClick={()=>setCategory(c)}>{c}</button>)}</nav>
 <div className="event-samples">{examples.filter(e=>category==='Все'||e.category===category).map(e=>{
  const a=e.groups[0]?.actions[0],instance=Object.values(preview?.instances||{}).filter(i=>i.eventId===e.id).at(-1),status=instance?.status;
  const available=!!a&&![a?.target].some(id=>!['world','audio','camera','trust'].includes(id)&&!project.objects.some(o=>o.id===id&&(!o.subsceneId||o.subsceneId===scene.id)))&&!(['door','letter'].includes(a?.target)&&scene.kind!=='living');
  return <article key={e.id} className={status==='running'?'running':''}><div className="sample-title"><Icon name={TYPES[a?.type]?.icon} size={20}/><div><strong>{e.name}</strong><small>{!a?'Добавьте действие':e.groups.length>1?`${e.groups.length} последовательных шага`:TYPES[a.type]?.label}</small></div><span className="sample-status">{status==='running'?'Выполняется':status==='paused'?'Пауза':status==='done'?'Применено':''}</span></div>
  <div className="sample-actions"><Button icon="Play" disabled={!available} title={available?'Применить в 3D-сцене':'Объект находится в другой локации'} onClick={()=>onPreview(e.id)}>Проба</Button><Button icon="Workflow" onClick={()=>onEdit(e.id)}>Граф</Button><Button icon="Plus" title="Добавить к выбранной реплике" onClick={()=>onAdd(e.id)}>В реплику</Button></div></article>;
 })}</div></div>;
}
