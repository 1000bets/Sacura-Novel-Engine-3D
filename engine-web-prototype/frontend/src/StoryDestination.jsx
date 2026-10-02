import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import {Icon} from './StudioParts.jsx';
import {beatPreview,beatIcon} from './storyLabels.js';
export default function StoryDestination({data,source,port,target}){
 useLocale();
 const node=data.project.chapters.flatMap(c=>c.beats).find(b=>b.id===target),wire=source+'/'+port;
 if(!target)return <span className="story-destination-empty"><Icon name="CircleDashed" size={13}/>{tr("Свободный выход")}</span>;
 if(!node)return <span className="story-destination-empty broken"><Icon name="Unplug" size={13}/>{tr("Блок удалён")}</span>;
 return <button className="story-destination nodrag" title={tr("Перейти к ноде: ")+beatPreview(node,8)} aria-label={tr("Перейти к ноде: ")+beatPreview(node,8)} onMouseEnter={()=>data.onTrace?.(wire)} onMouseLeave={()=>data.onTrace?.(null)} onFocus={()=>data.onTrace?.(wire)} onBlur={()=>data.onTrace?.(null)} onClick={e=>{e.stopPropagation();data.onTrace?.(null);data.onReveal(node.id);}}><Icon name="ArrowRight" size={14}/><Icon name={beatIcon(node)} size={14}/><span>{beatPreview(node)}</span><Icon name="Focus" size={13}/></button>;
}
