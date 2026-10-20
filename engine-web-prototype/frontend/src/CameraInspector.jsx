import {t as tr, useLocale, literalLabel} from './i18n.jsx';
import React from 'react';
import {Icon,Button,Field,Select} from './StudioParts.jsx';
import Section from './InspectorSection.jsx';
import {cameraPose} from './cameraModel.js';
import './cameraInspector.css';

export default function CameraInspector({camera:c,scene,objects,state,onChange,onDefault,onDelete,onPilot,onView,onCapture,piloting,running,onEdit}){
 useLocale();
 const characters=objects.filter(o=>o.type==='Персонаж'&&o.active!==false),pose=cameraPose(c,state,objects,scene.kind);
 const patch=value=>onChange(c.id,value),following=c.mode==='follow';
 const number=(key,label,min,max,step=.1)=><Field label={label}><input aria-label={tr(label)} type="number" min={min} max={max} step={step} value={Number(c[key].toFixed(3))} onChange={e=>{if(e.target.value!==''&&Number.isFinite(e.target.valueAsNumber))patch({[key]:e.target.valueAsNumber});}}/></Field>;
 return <div className="story-inspector camera-inspector">
  <header className="story-inspector-heading"><Icon name="Camera" size={16}/><strong>{tr('Камера')}</strong>{c.id===scene.defaultCameraId&&<small><Icon name="Star" size={12}/>{tr('Основная')}</small>}</header>
  {running&&<div className="camera-play-notice"><span>{tr('Предпросмотр')}</span><Button icon="Square" onClick={onEdit}>{tr('Остановить')}</Button></div>}
  <fieldset disabled={running} className="inspector-primary-fields">
   <Field label="Название"><input aria-label={tr('Название камеры')} value={c.name} onChange={e=>patch({name:e.target.value})}/></Field>
   <Field label="Режим"><Select value={c.mode} options={[["fixed","Неподвижная"],...(characters.length||following?[["follow","Слежение"]]:[])]} onChange={mode=>patch(mode==='fixed'?{mode,position:pose.position,target:pose.target}:{mode,followTargetId:c.followTargetId||characters[0]?.id})}/></Field>
   {following&&<><Field label="Персонаж"><Select value={c.followTargetId||''} options={[["","Выберите персонажа"],...(!characters.some(o=>o.id===c.followTargetId)&&c.followTargetId?[[c.followTargetId,'Персонаж недоступен']]:[]),...characters.map(o=>[o.id,literalLabel(o.name)])]} onChange={followTargetId=>patch({followTargetId})}/></Field>{pose.targetMissing&&<p role="alert" className="camera-error">{tr('Персонаж недоступен. Используется сохранённый кадр.')}</p>}</>}
   {number('fov','Угол обзора',20,90,1)}
   {following&&<>{number('distance','Расстояние',.6,20)}{number('height','Высота камеры',undefined,undefined)}</>}
   {c.id!==scene.defaultCameraId&&<Button icon="Star" onClick={()=>onDefault(c.id)}>{tr('Сделать основной')}</Button>}
  </fieldset>
  <div className="camera-shot-actions">
   <Button icon="Eye" onClick={()=>onView(c.id)}>{tr('Посмотреть')}</Button>
   <Button icon="Move" disabled={running} aria-pressed={piloting===c.id} className={piloting===c.id?'active':''} onClick={()=>onPilot(c.id)}>{tr('В сцене')}</Button>
   <Button icon="ScanLine" disabled={running} onClick={()=>onCapture(c.id)}>{piloting===c.id?tr('Сохранить ракурс'):tr('Взять текущий вид')}</Button>
   {piloting===c.id&&<p className="camera-pilot-hint">{tr('Подберите кадр мышью и сохраните ракурс.')}</p>}
  </div>
  {!following&&<Section title="Координаты" open={false}>
   <fieldset disabled={running} className="camera-coordinate-fields">{['position','target'].map(key=><div className="camera-vector" key={key}><span>{key==='position'?tr('Положение камеры'):tr('Точка взгляда')}</span><div>{c[key].map((value,i)=><label key={i}><b className={'axis-'+'XYZ'[i]}>{'XYZ'[i]}</b><input aria-label={`${tr(key==='position'?'Камера':'Точка взгляда')} ${'XYZ'[i]}`} type="number" step=".1" value={Number(value.toFixed(2))} onChange={e=>{if(e.target.value!==''&&Number.isFinite(e.target.valueAsNumber))patch({[key]:c[key].map((n,j)=>i===j?e.target.valueAsNumber:n)});}}/></label>)}</div></div>)}</fieldset>
  </Section>}
  <Section title="Дополнительно" open={false}>
   <fieldset disabled={running}>
    {number('smoothing','Плавность, с',0,2)}
    {following&&<>{number('yaw','Ракурс, °',-180,180,1)}{number('targetHeight','Высота взгляда',undefined,undefined)}</>}
   </fieldset>
  </Section>
  <div className="inspector-footer-action"><Button icon="Trash2" disabled={running} onClick={()=>onDelete(c.id)}>{tr('Удалить камеру')}</Button></div>
 </div>;
}
