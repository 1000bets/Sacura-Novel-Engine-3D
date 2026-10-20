import {t as tr, useLocale} from './i18n.jsx';
import React,{useState} from 'react';
import {Icon,Button} from './StudioParts.jsx';
const fresh=()=>({position:[0,0,0],rotation:[0,0,0],scale:[1,1,1]});
export default function MultiTransformInspector({objects,onChange,onFocus,onReset,onDuplicate,onDelete,disabled}){
 useLocale();
 const [draft,setDraft]=useState(fresh);
 return <><div className="inspector-identity"><Icon name="Boxes" size={27}/><div><strong>{tr("Выбрано объектов: ")}{objects.length}</strong><small>{objects.map(object=>object.name).join(', ')}</small></div></div>
  <fieldset className="transform-inspector" disabled={disabled}><legend><Icon name="Move" size={14}/> {tr("Общая трансформация")}</legend><small>{tr("Сдвиг, поворот и масштаб вокруг общего центра выделения.")}</small>
  {[['position','Сдвиг группы','м'],['rotation','Поворот группы','°'],['scale','Масштаб группы','×']].map(([key,label,unit])=><div className="transform-vector" key={key}><span>{tr(label)}<small>{tr(unit)}</small></span><div>{['X','Y','Z'].map((axis,i)=><label key={axis}><b className={'axis-'+axis}>{axis}</b><input aria-label={`${tr(label)} ${axis}`} type="number" step={key==='rotation'?5:.1} min={key==='scale'?.05:undefined} value={draft[key][i]} onChange={e=>{const value=e.target.value;setDraft(current=>({...current,[key]:current[key].map((v,j)=>j===i?value:v)}));}} onKeyDown={e=>{if(e.key==='Enter'){e.preventDefault();onChange(draft);setDraft(fresh());}}}/></label>)}</div></div>)}
  <Button icon="Check" onClick={()=>{onChange(draft);setDraft(fresh());}}>{tr("Применить ко всем")}</Button>
  <div className="transform-actions"><Button icon="Focus" onClick={onFocus}>{tr("Все в кадр")}</Button><Button icon="RotateCcw" onClick={onReset}>{tr("Сбросить")}</Button><Button icon="Copy" onClick={onDuplicate}>{tr("Дубликаты")}</Button><Button icon="Trash2" onClick={onDelete}>{tr("Удалить")}</Button></div><p>{tr("Shift + щелчок добавляет или убирает объект. Общая ось в сцене изменяет всё выделение. Ctrl+Z отменяет изменение всей группы.")}</p></fieldset></>;
}
