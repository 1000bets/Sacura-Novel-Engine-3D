import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import {Icon,Button} from './StudioParts.jsx';

export default function HierarchyTree({scene,objects,groups=[],onCreateGroup,onCreateEmptyGroup,onAddObject,onAddPoint,onAddSound,onRootDrop,onSelectGroup,onGroupDrop,disabled,points,query,selected,selectedIds=[],expanded,onExpanded,onSelectObject,onFrameObject,onVisibility,onPoint,onSelectCamera,onCameras,showCameras,onShowCameras,showDialogue,onShowDialogue,onStory,onAudio}){
 useLocale();
 const prefix=scene.id+':',search=query.trim().toLowerCase();
 const isOpen=key=>!!search||(expanded[prefix+key]??key!=='points');
 const toggle=key=>onExpanded({...expanded,[prefix+key]:!isOpen(key)});
 const all=value=>onExpanded({...expanded,...Object.fromEntries(['scene','systems','cameras','objects','points'].map(key=>[prefix+key,value]))});
 const matches=value=>value.toLowerCase().includes(search);
 const found=objects.filter(o=>matches(o.name));
 const groupedIds=new Set(groups.flatMap(g=>g.objectIds));
 const drop=(e,groupId)=>{e.preventDefault();e.stopPropagation();if(disabled)return;try{const asset=JSON.parse(e.dataTransfer.getData('application/sacura-asset'));if(asset.kind==='objects')(groupId?onGroupDrop:onRootDrop)(...(groupId?[groupId,selectedIds.includes(asset.id)?selectedIds:[asset.id]]:[selectedIds.includes(asset.id)?selectedIds:[asset.id]]));}catch{}};
 const renderObject=o=><div className={'tree-object '+(o.type==='Персонаж'?'character ':'')+(selectedIds.includes(o.id)?'selected':'')} key={o.id}>
    <button draggable title={o.name+tr(" · Shift — добавить к выделению · двойной щелчок — в кадр")} aria-label={o.name} aria-pressed={selectedIds.includes(o.id)} onDragStart={e=>e.dataTransfer.setData('application/sacura-asset',JSON.stringify({id:o.id,kind:'objects'}))} onClick={e=>onSelectObject(o.id,{additive:e.shiftKey||e.ctrlKey||e.metaKey})} onDoubleClick={e=>{if(!e.shiftKey&&!e.ctrlKey&&!e.metaKey)onFrameObject(o.id);}}>{o.type==='Персонаж'&&<Icon name="PersonStanding" size={15}/>} {o.type==='Источник света'&&<Icon name="Lightbulb" size={15}/>}<span>{o.name}</span></button>
    <button title={(o.active!==false?tr("Скрыть "):tr("Показать "))+o.name} aria-pressed={o.active!==false} onClick={()=>onVisibility(o.id)}><Icon name={o.active!==false?'Eye':'EyeOff'} size={13}/></button>
   </div>;
 const section=(key,label,icon,count)=> <button className="tree-folder tree-disclosure" aria-expanded={isOpen(key)} onClick={()=>toggle(key)}><Icon name={isOpen(key)?'ChevronDown':'ChevronRight'} size={13}/><span>{tr(label)}</span><small>{count}</small></button>;
 return <>
  <div className="hierarchy-create"><Button icon="Plus" disabled={disabled} onClick={()=>onAddObject()}>{tr("Объект")}</Button><Button icon="FolderPlus" disabled={disabled} onClick={onCreateEmptyGroup}>{tr("Группа")}</Button><Button icon="MapPin" disabled={disabled} onClick={onAddPoint}>{tr("Точка")}</Button><Button icon="Volume2" disabled={disabled} onClick={onAddSound}>{tr("Звук")}</Button></div>
  <small>{tr("Перетащите объект в группу или в «Объекты сцены». Создание доступно и внутри группы.")}</small>
  <div className="tree-root-row">
   <button className="tree-scene tree-disclosure" aria-expanded={isOpen('scene')} onClick={()=>toggle('scene')}><Icon name={isOpen('scene')?'ChevronDown':'ChevronRight'} size={13}/><strong>{scene.name}</strong></button>
   <div className="tree-controls"><Button icon="ListTree" title={tr("Развернуть все группы")} onClick={()=>all(true)}/><Button icon="ListMinus" title={tr("Свернуть все группы")} onClick={()=>all(false)}/></div>
  </div>
  {isOpen('scene')&&<>
   <div className="tree-system-heading" onDragOver={e=>{if(!disabled)e.preventDefault();}} onDrop={e=>drop(e,null)}>{section('objects','Объекты сцены','Folder',found.length)}<Button icon="FolderPlus" title={tr("Сгруппировать выделенные объекты")} disabled={disabled||selectedIds.length<2} onClick={onCreateGroup}/></div>
   {isOpen('objects')&&<div className="tree-group-children">{groups.map(group=><div key={group.id} className="tree-object-group" onDragOver={e=>{if(!disabled&&Array.from(e.dataTransfer.types).includes('application/sacura-asset')){e.preventDefault();e.stopPropagation();}}} onDrop={e=>{e.preventDefault();e.stopPropagation();if(disabled)return;try{const asset=JSON.parse(e.dataTransfer.getData('application/sacura-asset'));if(asset.kind==='objects')onGroupDrop(group.id,selectedIds.includes(asset.id)?selectedIds:[asset.id]);}catch{}}}><div className="tree-root-row"><button aria-expanded={isOpen(group.id)} title={tr("Свернуть или развернуть группу")} onClick={()=>toggle(group.id)}><Icon name={isOpen(group.id)?'ChevronDown':'ChevronRight'} size={13}/></button><button className="tree-folder" onClick={()=>onSelectGroup(group)}><Icon name="Folder" size={14}/><span>{group.name}</span><small>{group.objectIds.length}</small></button><Button icon="Plus" title={tr("Создать объект внутри этой группы")} disabled={disabled} onClick={()=>onAddObject(group.id)}/></div>{isOpen(group.id)&&objects.filter(o=>group.objectIds.includes(o.id)&&(matches(o.name)||matches(group.name))).map(renderObject)}</div>)}{found.filter(o=>!groupedIds.has(o.id)).map(renderObject)}{!found.length&&<p className="tree-empty">{tr("Объекты не найдены")}</p>}</div>}
   {section('systems','Системы сцены','Settings2')}
   {isOpen('systems')&&<div className="tree-system-children">
   <div className="tree-system-heading">{section('cameras','Камеры','Video',scene.cameras?.length||0)}<Button icon="Settings2" title={tr("Настройки камер сабсцены")} onClick={onCameras}/><Button icon={showCameras?'Eye':'EyeOff'} title={showCameras?tr("Скрыть камеры в редакторе"):tr("Показать камеры в редакторе")} onClick={onShowCameras}/></div>
   {isOpen('cameras')&&<div className="tree-camera-children">{(scene.cameras||[]).filter(c=>matches(c.name)).map(c=><button key={c.id} className={'tree-row system '+(selected===c.id?'selected':'')} title={tr("Посмотреть из камеры «")+c.name+'»'} aria-pressed={selected===c.id} onClick={()=>onSelectCamera(c.id)}><Icon name={c.mode==='follow'?'UserRoundCheck':'Video'}/><span>{c.name}</span>{c.id===scene.defaultCameraId&&<Icon name="Star"/>}</button>)}</div>}
   <div className="tree-system-heading"><button className="tree-row system" onClick={onStory}><Icon name="MessagesSquare"/><span>{tr("Система диалогов")}</span></button><Button icon={showDialogue?'Eye':'EyeOff'} title={showDialogue?tr("Скрыть диалог"):tr("Показать диалог")} onClick={onShowDialogue}/></div>
   <button className="tree-row system" onClick={onAudio}><Icon name="AudioLines"/><span>{tr("Звук и окружение")}</span><Icon name="ChevronRight"/></button>
   </div>}
   <div className="tree-system-heading">{section('points','Точки постановки','MapPin',points.length)}<Button icon="Plus" title={tr("Создать точку постановки")} disabled={disabled} onClick={onAddPoint}/></div>
   {isOpen('points')&&points.filter(p=>matches(p.label)).map(point=><button className="tree-anchor" key={point.id} aria-pressed={selected===point.id} onClick={()=>onPoint(point)} title={tr("Настроить точку «")+point.label+'»'}><Icon name="LocateFixed" size={13}/><span>{point.label}</span><Icon name="CornerUpRight" size={12}/></button>)}
   {isOpen('points')&&!points.length&&<p className="tree-empty">{tr("В этой сабсцене нет точек постановки")}</p>}
  </>}
 </>;
}
