import React,{useEffect,useState} from 'react';
import {Button} from './StudioParts.jsx';
import {navMeshSettings} from './scenePhysics.js';
import './scenePhysics.css';

export function PhysicsNumber({label,value,onChange,min,max,step=.1}){
 const [draft,setDraft]=useState(String(value));useEffect(()=>setDraft(String(value)),[value]);
 const commit=()=>{const n=Number(draft);if(draft.trim()&&Number.isFinite(n)){const next=Math.max(min??-Infinity,Math.min(max??Infinity,n));setDraft(String(next));if(next!==value)onChange(next);}else setDraft(String(value));};
 return <label>{label}<input type="number" value={draft} min={min} max={max} step={step} onChange={e=>setDraft(e.target.value)} onBlur={commit} onKeyDown={e=>{if(e.key==='Enter')e.currentTarget.blur();}}/></label>;
}
export default function NavigationInspector({scene,disabled,onChange}){
 const n=navMeshSettings(scene),patch=values=>onChange({...n,...values}),vector=(key,index,value)=>patch({[key]:n[key].map((v,i)=>i===index?value:v)});
 return <fieldset className="scene-physics-settings" disabled={disabled}><legend>Nav mesh и коллизии</legend>
 <label className="check"><input type="checkbox" checked={n.enabled} onChange={e=>patch({enabled:e.target.checked})}/>Плоский nav mesh</label>
 <p>Персонажи ходят по горизонтальной области и обходят объекты с включённой коллизией. Точки постановки должны находиться внутри свободной области.</p>
 {n.enabled&&<><div className="physics-fields">{['X','Y','Z'].map((axis,i)=><PhysicsNumber key={axis} label={'Центр '+axis+' · м'} value={n.center[i]} onChange={v=>vector('center',i,v)}/>)}</div>
 <div className="physics-fields">{['Ширина X · м','Глубина Z · м'].map((label,i)=><PhysicsNumber key={label} label={label} value={n.size[i]} min={.5} max={200} onChange={v=>vector('size',i,v)}/>)}<PhysicsNumber label="Ячейка · м" value={n.cellSize} min={.1} max={2} onChange={cellSize=>patch({cellSize})}/></div>
 <label className="check"><input type="checkbox" checked={n.show} onChange={e=>patch({show:e.target.checked})}/>Показывать nav mesh в редакторе</label></>}
 <label className="check"><input type="checkbox" checked={n.showColliders} onChange={e=>patch({showColliders:e.target.checked})}/>Показывать коллизии в редакторе</label>
 <p>Коллизии включаются отдельно в инспекторе каждого объекта. В режиме игры вспомогательные контуры скрыты.</p>
 <Button icon="Box" onClick={()=>patch({enabled:true,show:true})} disabled={n.enabled}>Добавить плоский nav mesh</Button>
 </fieldset>;
}
