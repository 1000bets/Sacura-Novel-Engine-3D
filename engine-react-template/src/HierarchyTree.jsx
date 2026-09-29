import React from 'react';
import {Icon,Button} from './StudioParts.jsx';

export default function HierarchyTree({scene,objects,points,query,selected,selectedIds=[],expanded,onExpanded,onSelectObject,onFrameObject,onVisibility,onPoint,onSelectCamera,onCameras,showCameras,onShowCameras,showDialogue,onShowDialogue,onStory,onAudio}){
 const prefix=scene.id+':',search=query.trim().toLowerCase();
 const isOpen=key=>!!search||(expanded[prefix+key]??!['systems','points'].includes(key));
 const toggle=key=>onExpanded({...expanded,[prefix+key]:!isOpen(key)});
 const all=value=>onExpanded({...expanded,...Object.fromEntries(['scene','systems','cameras','objects','points'].map(key=>[prefix+key,value]))});
 const matches=value=>value.toLowerCase().includes(search);
 const found=objects.filter(o=>matches(o.name));
 const section=(key,label,icon,count)=> <button className="tree-folder tree-disclosure" aria-expanded={isOpen(key)} onClick={()=>toggle(key)}><Icon name={isOpen(key)?'ChevronDown':'ChevronRight'} size={13}/><span>{label}</span><small>{count}</small></button>;
 return <>
  <div className="tree-root-row">
   <button className="tree-scene tree-disclosure" aria-expanded={isOpen('scene')} onClick={()=>toggle('scene')}><Icon name={isOpen('scene')?'ChevronDown':'ChevronRight'} size={13}/><strong>{scene.name}</strong></button>
   <div className="tree-controls"><Button icon="ListTree" title="Развернуть все группы" onClick={()=>all(true)}/><Button icon="ListMinus" title="Свернуть все группы" onClick={()=>all(false)}/></div>
  </div>
  {isOpen('scene')&&<>
   {section('objects','Объекты сцены','Folder',found.length)}
   {isOpen('objects')&&<div className="tree-group-children">{found.map(o=><div className={'tree-object '+(o.type==='Персонаж'?'character ':'')+(selectedIds.includes(o.id)?'selected':'')} key={o.id}>
    <button draggable title={o.name+' · Shift — добавить к выделению · двойной щелчок — в кадр'} aria-label={o.name} aria-pressed={selectedIds.includes(o.id)} onDragStart={e=>e.dataTransfer.setData('application/sacura-asset',JSON.stringify({id:o.id,kind:'objects'}))} onClick={e=>onSelectObject(o.id,{additive:e.shiftKey||e.ctrlKey||e.metaKey})} onDoubleClick={e=>{if(!e.shiftKey&&!e.ctrlKey&&!e.metaKey)onFrameObject(o.id);}}>{o.type==='Персонаж'&&<Icon name="PersonStanding" size={15}/>}<span>{o.name}</span></button>
    <button title={(o.active!==false?'Скрыть ':'Показать ')+o.name} aria-pressed={o.active!==false} onClick={()=>onVisibility(o.id)}><Icon name={o.active!==false?'Eye':'EyeOff'} size={13}/></button>
   </div>)}{!found.length&&<p className="tree-empty">Объекты не найдены</p>}</div>}
   {section('systems','Системы сцены','Settings2')}
   {isOpen('systems')&&<>
   <div className="tree-system-heading">{section('cameras','Камеры','Video',scene.cameras?.length||0)}<Button icon="Settings2" title="Настройки камер сабсцены" onClick={onCameras}/><Button icon={showCameras?'Eye':'EyeOff'} title={showCameras?'Скрыть камеры в редакторе':'Показать камеры в редакторе'} onClick={onShowCameras}/></div>
   {isOpen('cameras')&&(scene.cameras||[]).filter(c=>matches(c.name)).map(c=><button key={c.id} className={'tree-row system '+(selected===c.id?'selected':'')} onClick={()=>onSelectCamera(c.id)}><Icon name={c.mode==='follow'?'UserRoundCheck':'Video'}/><span>{c.name}</span>{c.id===scene.defaultCameraId&&<Icon name="Star"/>}</button>)}
   <div className="tree-system-heading"><button className="tree-row system" onClick={onStory}><Icon name="MessagesSquare"/><span>Система диалогов</span></button><Button icon={showDialogue?'Eye':'EyeOff'} title={showDialogue?'Скрыть диалог':'Показать диалог'} onClick={onShowDialogue}/></div>
   <button className="tree-row system" onClick={onAudio}><Icon name="AudioLines"/><span>Звук и окружение</span><Icon name="ChevronRight"/></button>
   </>}
   {section('points','Точки постановки','MapPin',points.length)}
   {isOpen('points')&&points.filter(p=>matches(p.label)).map(point=><button className="tree-anchor" key={point.id} onClick={()=>onPoint(point)} title={'Перейти к точке «'+point.label+'»'}><Icon name="LocateFixed" size={13}/><span>{point.label}</span><Icon name="CornerUpRight" size={12}/></button>)}
   {isOpen('points')&&!points.length&&<p className="tree-empty">В этой сабсцене нет точек постановки</p>}
  </>}
 </>;
}
