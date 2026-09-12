import React,{useState} from 'react';
import {Icon,Button} from './StudioParts.jsx';
const fresh=()=>({position:[0,0,0],rotation:[0,0,0],scale:[1,1,1]});
export default function MultiTransformInspector({objects,onChange,onFocus,onReset,onDuplicate,onDelete,disabled}){
 const [draft,setDraft]=useState(fresh);
 return <><div className="inspector-identity"><Icon name="Boxes" size={27}/><div><strong>Выбрано объектов: {objects.length}</strong><small>{objects.map(object=>object.name).join(', ')}</small></div></div>
  <fieldset className="transform-inspector" disabled={disabled}><legend><Icon name="Move" size={14}/> Общая трансформация</legend><small>Сдвиг, поворот и масштаб вокруг общего центра выделения.</small>
  {[['position','Сдвиг группы','м'],['rotation','Поворот группы','°'],['scale','Масштаб группы','×']].map(([key,label,unit])=><div className="transform-vector" key={key}><span>{label}<small>{unit}</small></span><div>{['X','Y','Z'].map((axis,i)=><label key={axis}><b className={'axis-'+axis}>{axis}</b><input aria-label={`${label} ${axis}`} type="number" step={key==='rotation'?5:.1} min={key==='scale'?.05:undefined} value={draft[key][i]} onChange={e=>{const value=e.target.value;setDraft(current=>({...current,[key]:current[key].map((v,j)=>j===i?value:v)}));}} onKeyDown={e=>{if(e.key==='Enter'){e.preventDefault();onChange(draft);setDraft(fresh());}}}/></label>)}</div></div>)}
  <Button icon="Check" onClick={()=>{onChange(draft);setDraft(fresh());}}>Применить ко всем</Button>
  <div className="transform-actions"><Button icon="Focus" onClick={onFocus}>Все в кадр</Button><Button icon="RotateCcw" onClick={onReset}>Сбросить</Button><Button icon="Copy" onClick={onDuplicate}>Дубликаты</Button><Button icon="Trash2" onClick={onDelete}>Удалить</Button></div><p>Shift + щелчок добавляет или убирает объект. Общая ось в сцене изменяет всё выделение. Ctrl+Z отменяет изменение всей группы.</p></fieldset></>;
}
