import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import {audioActionOptions,END_MODES} from './audioTransitions.js';
import './interactionTools.css';
export default function AudioActionControls({action,onChange,disabled=false}){
 useLocale();
 const options=audioActionOptions(action),type=action.type;
 const controls=type==='stop'||type==='pause'?['fadeOut']:type==='resume'?['fadeIn']:['fadeIn','fadeOut'];
 return <div className="audio-transition-controls">
 {controls.map(key=>{const enabled=action[key+'Enabled']??options[key]>0;return <div className="audio-fade-control" key={key}>
 <label className="check"><input type="checkbox" disabled={disabled} checked={enabled} onChange={e=>onChange({[key+'Enabled']:e.target.checked,[key]:action[key]??(key==='fadeOut'&&type==='stop'?action.duration??2:1)})}/>{key==='fadeIn'?'Fade In':'Fade Out'}</label>
 <label className="action-field"><span>{tr("Продолжительность ")}{key==='fadeIn'?'Fade In':'Fade Out'}{tr(", с")}</span><input type="number" disabled={disabled||!enabled} min="0" step="0.1" value={action[key]??options[key]} onChange={e=>onChange({[key]:Math.max(0,Number(e.target.value))})}/></label>
 </div>;})}
 {type!=='resume'&&<label className="action-field"><span>{tr("Когда конец")}</span><select disabled={disabled} value={options.endMode} onChange={e=>onChange({endMode:e.target.value})}>{END_MODES.map(([value,label])=><option key={value} value={value}>{tr(label)}</option>)}</select></label>}
 <small>{type==='resume'?tr("Плавный вход начинается с сохранённой позиции."):tr("Маркер или музыкальная сетка задают точку завершения; Fade Out заканчивается в этой точке. Без маркеров и сетки завершение начнётся сразу.")}</small>
 </div>;
}
