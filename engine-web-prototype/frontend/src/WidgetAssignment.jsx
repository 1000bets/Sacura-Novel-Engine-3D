import {t as tr,useLocale,literalLabel} from './i18n.jsx';
import React from 'react';
import {Button,Field,Select} from './StudioParts.jsx';
import {resolveWidget} from './widgetModel.js';
export default function WidgetAssignment({project,character,beat,onChange,onEdit,onCustomize,disabled=false}){
 useLocale();
 const value=beat?beat.widgetId:character?.dialogueWidgetId,effective=resolveWidget(project,'dialogue',beat||{speakerId:character?.id});
 const label=beat?tr('Переопределить виджет этой реплики'):tr('Виджет диалога персонажа');
 return <div className="widget-assignment"><Field label={tr('Виджет')}><Select aria-label={label} title={beat?tr('Переопределение действует только на эту реплику.'):tr('Для собственного оформления создайте копию виджета.')} disabled={disabled} value={value||''} options={[["",literalLabel((beat?tr('Наследовать'):tr('Проект'))+' · '+effective.name)],...(project.widgets||[]).filter(w=>w.kind==='dialogue').map(w=>[w.id,literalLabel(w.name)])]} onChange={id=>onChange(id||null)}/></Field><div className="widget-assignment-actions"><Button icon="PanelsTopLeft" aria-label={tr('Редактировать виджет')} disabled={disabled} onClick={()=>onEdit(effective.id)}>{tr('Редактировать')}</Button>{onCustomize&&<Button icon="Copy" title={tr('Создать независимую копию виджета')} disabled={disabled} onClick={onCustomize}>{tr('Свой вариант')}</Button>}</div></div>;
}
