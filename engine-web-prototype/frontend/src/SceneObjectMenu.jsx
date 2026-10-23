import React from 'react';
import {t as tr,useLocale} from './i18n.jsx';
import {Icon} from './StudioParts.jsx';
import AnchoredPopover from './AnchoredPopover.jsx';

export default function SceneObjectMenu({anchor,onClose,onGroup,onCamera,onPoint,onSound,onObject}){
 useLocale();
 const items=[
  ['Группа','FolderPlus',onGroup],['Камера','Video',onCamera],['Точка постановки','MapPin',onPoint],['Звуковое событие','Volume2',onSound],
  ...[['Персонаж','box','PersonStanding','Персонаж'],['Активный меш','box','MousePointer2','Интерактивный предмет'],['Пустой объект','box','Box','Пустой объект'],['Меш','box','Box','Куб'],['Меш','sphere','Circle','Сфера'],['Меш','cylinder','Cylinder','Цилиндр'],['Источник света','box','Lightbulb','Источник освещения']].map(([type,shape,icon,label])=>[label,icon,()=>onObject(type,shape)])
 ];
 return <AnchoredPopover anchor={anchor} onClose={onClose} role="menu" label={tr('Добавить объект')} id="scene-object-menu" className="scene-object-menu">
  {items.map(([label,icon,action])=><button role="menuitem" key={label} onClick={()=>{onClose();anchor?.focus({preventScroll:true});action();}}><Icon name={icon} size={16}/><span>{tr(label)}</span></button>)}
 </AnchoredPopover>;
}
