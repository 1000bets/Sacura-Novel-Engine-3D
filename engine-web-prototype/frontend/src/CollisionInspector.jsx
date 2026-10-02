import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import {collisionSettings} from './scenePhysics.js';
import {PhysicsNumber} from './NavigationInspector.jsx';
export default function CollisionInspector({object,disabled,onChange}){
 useLocale();
 const c=collisionSettings(object),patch=values=>onChange({...c,...values}),vector=(key,index,value)=>patch({[key]:c[key].map((v,i)=>i===index?value:v)});
 return <fieldset className="scene-physics-settings" disabled={disabled}><legend>{tr("Простая коллизия · Box")}</legend>
 <label className="check"><input type="checkbox" checked={c.enabled} onChange={e=>patch({enabled:e.target.checked})}/>{tr("Коллизия объекта")}</label>
 <p>{tr("Коллизия препятствует пересечению при действиях движения. Отключённый объект можно проходить насквозь.")}</p>
 {c.enabled&&<><label className="check"><input type="checkbox" checked={c.custom} onChange={e=>patch({custom:e.target.checked})}/>{tr("Задать размер вручную")}</label>
 {c.custom?<>{[['size','Размер'],['offset','Смещение']].map(([key,title])=><div className="physics-fields" key={key}>{['X','Y','Z'].map((axis,i)=><PhysicsNumber key={axis} label={tr("{0} {1} · м", [title, axis])} value={c[key][i]} min={key==='size'?.01:undefined} max={key==='size'?200:undefined} onChange={v=>vector(key,i,v)}/>)}</div>)}</>:<p>{tr("Размер вычисляется по видимой геометрии, включая загруженную модель. Коллайдер учитывает положение, поворот и масштаб объекта.")}</p>}
 <p>{tr("Для персонажа включите коллизию, чтобы он учитывал препятствия. Контуры можно включить в настройках сабсцены.")}</p></>}
 </fieldset>;
}
