import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import {Icon,Button} from './StudioParts.jsx';

export default function HierarchyTree({scene,objects,groups=[],onAddObject,onRootDrop,onSelectGroup,onGroupDrop,disabled,points=[],query,selected,selectedIds=[],expanded,onExpanded,onSelectObject,onFrameObject,onVisibility,onPoint,onSelectCamera}){
 useLocale();
 const prefix=scene.id+':',search=query.trim().toLowerCase();
 const isOpen=key=>!!search||(expanded[prefix+key]??true);
 const toggle=key=>onExpanded({...expanded,[prefix+key]:!isOpen(key)});
 const matches=value=>(value||'').toLowerCase().includes(search);
 const found=objects.filter(o=>matches(o.name));
 const cameras=(scene.cameras||[]).filter(c=>matches(c.name));
 const foundPoints=points.filter(p=>matches(p.label));
 const visibleGroups=groups.filter(g=>matches(g.name)||objects.some(o=>g.objectIds.includes(o.id)&&matches(o.name)));
 const groupedIds=new Set(groups.flatMap(g=>g.objectIds));
 const count=objects.length+(scene.cameras?.length||0)+points.length;
 const drop=(e,groupId)=>{
  e.preventDefault();e.stopPropagation();if(disabled)return;
  try{
   const asset=JSON.parse(e.dataTransfer.getData('application/sacura-asset'));
   if(asset.kind!=='objects')return;
   const ids=selectedIds.includes(asset.id)?selectedIds:[asset.id];
   if(groupId)onGroupDrop(groupId,ids);else onRootDrop(ids);
  }catch{}
 };
 const dragOver=e=>{if(!disabled&&Array.from(e.dataTransfer.types).includes('application/sacura-asset')){e.preventDefault();e.stopPropagation();}};
 const renderObject=o=><div className={'tree-object '+(o.type==='Персонаж'?'character ':'')+(selectedIds.includes(o.id)?'selected':'')} key={o.id}>
  <button draggable={!disabled} title={o.name+tr(' · Shift — добавить к выделению · двойной щелчок — в кадр')} aria-label={o.name} aria-pressed={selectedIds.includes(o.id)} onDragStart={e=>e.dataTransfer.setData('application/sacura-asset',JSON.stringify({id:o.id,kind:'objects'}))} onClick={e=>onSelectObject(o.id,{additive:e.shiftKey||e.ctrlKey||e.metaKey})} onDoubleClick={e=>{if(!e.shiftKey&&!e.ctrlKey&&!e.metaKey)onFrameObject(o.id);}}>
   <Icon name={o.type==='Персонаж'?'PersonStanding':o.type==='Источник света'?'Lightbulb':'Box'} size={14}/><span>{o.name}</span>
  </button>
  <button title={(o.active!==false?tr('Скрыть '):tr('Показать '))+o.name} aria-pressed={o.active!==false} onClick={()=>onVisibility(o.id)}><Icon name={o.active!==false?'Eye':'EyeOff'} size={13}/></button>
 </div>;
 return <>
  <div className="tree-root-row scene-objects-heading" onDragOver={dragOver} onDrop={e=>drop(e,null)}>
   <h2>{tr('Объекты сцены')}</h2><span className="scene-object-count">{count}</span>
   {groups.length>0&&<div className="tree-controls"><Button icon="ListTree" title={tr('Развернуть все группы')} onClick={()=>onExpanded({...expanded,...Object.fromEntries(groups.map(g=>[prefix+g.id,true]))})}/><Button icon="ListMinus" title={tr('Свернуть все группы')} onClick={()=>onExpanded({...expanded,...Object.fromEntries(groups.map(g=>[prefix+g.id,false]))})}/></div>}
  </div>
  <div className="scene-objects-children" onDragOver={dragOver} onDrop={e=>drop(e,null)}>
   {cameras.map(c=><button key={c.id} className={'tree-row scene-entity '+(selected===c.id?'selected':'')} title={tr('Посмотреть из камеры «')+c.name+'»'} aria-pressed={selected===c.id} onClick={()=>onSelectCamera(c.id)}><Icon name={c.mode==='follow'?'UserRoundCheck':'Video'} size={14}/><span>{c.name}</span>{c.id===scene.defaultCameraId&&<Icon name="Star" size={12}/>}</button>)}
   {visibleGroups.map(group=><div key={group.id} className="tree-object-group" onDragOver={dragOver} onDrop={e=>drop(e,group.id)}>
    <div className="tree-root-row">
     <button aria-label={tr('Свернуть или развернуть группу')+' '+group.name} aria-expanded={isOpen(group.id)} onClick={()=>toggle(group.id)}><Icon name={isOpen(group.id)?'ChevronDown':'ChevronRight'} size={13}/></button>
     <button className="tree-folder" onClick={()=>onSelectGroup(group)}><Icon name="Folder" size={14}/><span>{group.name}</span><small>{group.objectIds.length}</small></button>
     <Button icon="Plus" title={tr('Создать объект внутри этой группы')} aria-haspopup="menu" disabled={disabled} onClick={event=>onAddObject(group.id,event)}/>
    </div>
    {isOpen(group.id)&&objects.filter(o=>group.objectIds.includes(o.id)&&(matches(o.name)||matches(group.name))).map(renderObject)}
   </div>)}
   {found.filter(o=>!groupedIds.has(o.id)).map(renderObject)}
   {foundPoints.map(point=><button className={'tree-row scene-entity '+(selected===point.id?'selected':'')} key={point.id} aria-pressed={selected===point.id} onClick={()=>onPoint(point)} title={tr('Настроить точку «')+point.label+'»'}><Icon name="MapPin" size={14}/><span>{point.label}</span></button>)}
   {!found.length&&!cameras.length&&!foundPoints.length&&!visibleGroups.length&&<p className="tree-empty">{tr('Объекты не найдены')}</p>}
  </div>
 </>;
}
