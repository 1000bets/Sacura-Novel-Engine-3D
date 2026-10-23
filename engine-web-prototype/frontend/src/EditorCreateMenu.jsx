import React from 'react';
import {t as tr,useLocale} from './i18n.jsx';
import {Icon} from './StudioParts.jsx';

export const CREATE_MENU_ITEMS=[
  {id:'component',label:'Компонент',icon:'Puzzle'},
  {id:'subscene',label:'Сабсцена',icon:'PanelsTopLeft'},
  {id:'action',label:'Действие',icon:'MousePointer2'},
  {id:'group',label:'Группа действий',icon:'Columns2'},
  {id:'beat',label:'Блок сценария',icon:'MessageSquare'},
  {id:'event',label:'Событие',icon:'Layers'},
  {id:'object',label:'Объект',icon:'Box'},
];

export default function EditorCreateMenu({onCreate,disabled=()=>false}){
  useLocale();
  return <>{CREATE_MENU_ITEMS.map(item=><button key={item.id} disabled={disabled(item.id)} onClick={event=>onCreate(item.id,event)}><Icon name={item.icon}/>{tr(item.label)}</button>)}</>;
}
