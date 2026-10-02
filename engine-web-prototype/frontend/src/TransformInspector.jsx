import {t as tr, useLocale} from './i18n.jsx';
import React,{useEffect,useState} from 'react';
import {objectTransform} from './sceneEditing.js';
import {Icon,Button} from './StudioParts.jsx';
export default function TransformInspector({object,scene,onChange,onReset,onFocus,onDuplicate,onDelete,disabled}){
 useLocale();
 const value=objectTransform(object,scene.id,scene.kind),serialized=JSON.stringify(value);
 const [draft,setDraft]=useState(value);useEffect(()=>setDraft(JSON.parse(serialized)),[serialized,object.id,scene.id]);
 return <fieldset className="transform-inspector" disabled={disabled}><legend><Icon name="Move" size={14}/> {tr("Положение, поворот, масштаб")}</legend><small>{scene.location} {tr("· изменения только в этой сабсцене")}</small>{[['position','Положение','м'],['rotation','Поворот','°'],['scale','Масштаб','×']].map(([key,label,unit])=><div className="transform-vector" key={key}><span>{tr(label)}<small>{unit}</small></span><div>{['X','Y','Z'].map((axis,i)=><label key={axis}><b className={'axis-'+axis}>{axis}</b><input aria-label={`${label} ${axis}`} type="number" step={key==='rotation'?5:.1} min={key==='scale'?.05:undefined} value={draft[key][i]} onChange={e=>{const val=e.target.value;setDraft(d=>({...d,[key]:d[key].map((v,j)=>i===j?val:v)}));}} onBlur={()=>onChange(draft)} onKeyDown={e=>{if(e.key==='Enter')e.currentTarget.blur();}}/></label>)}</div></div>)}<div className="transform-actions"><Button icon="Focus" onClick={onFocus}>{tr("В кадр")}</Button><Button icon="RotateCcw" onClick={onReset}>{tr("Сбросить")}</Button><Button icon="Copy" onClick={onDuplicate}>{tr("Дубликат")}</Button><Button icon="Trash2" onClick={onDelete}>{tr("Удалить")}</Button></div><p>{tr("Здесь задаются числовые параметры. Для изменения мышью выберите один инструмент над сценой и перетащите цветную ось. W — сдвиг, E — поворот, R — масштаб. F — приблизить объект.")}</p></fieldset>;
}
