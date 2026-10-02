import {t as tr, useLocale, literalLabel} from './i18n.jsx';
import React from 'react';
import {Button,Field,Select} from './StudioParts.jsx';
import {resolveWidget} from './widgetModel.js';
export default function WidgetAssignment({project,character,beat,onChange,onEdit,onCustomize,disabled=false}){
 useLocale();
 const value=beat?beat.widgetId:character?.dialogueWidgetId,effective=resolveWidget(project,'dialogue',beat||{speakerId:character?.id});
 return <div className="widget-assignment"><Field label={beat?tr("Переопределить виджет этой реплики"):tr("Виджет диалога персонажа")}><Select disabled={disabled} value={value||''} options={[["",beat?'Наследовать от персонажа / проекта':'Стандартный виджет проекта'],...(project.widgets||[]).filter(w=>w.kind==='dialogue').map(w=>[w.id,literalLabel(w.name)])]} onChange={id=>onChange(id||null)}/></Field><p className="resource-note">{tr("Используется: ")}{effective.name}. {beat?tr("Переопределение действует только на эту реплику."):tr("Для собственного оформления создайте копию виджета.")}</p><Button icon="PanelsTopLeft" disabled={disabled} onClick={()=>onEdit(effective.id)}>{tr("Редактировать виджет")}</Button>{onCustomize&&<Button icon="Copy" disabled={disabled} onClick={onCustomize}>{tr("Свой вариант")}</Button>}</div>;
}
