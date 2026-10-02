import {t as tr, useLocale, literalLabel} from './i18n.jsx';
import React from 'react';
import {Icon,Button,Field,Select} from './StudioParts.jsx';
import {cameraPose} from './cameraModel.js';

function Section({title,children}){
 useLocale();
 return <details className="inspector-section" open><summary><Icon name="ChevronRight" size={12}/><span>{tr(title)}</span></summary><div className="inspector-fields">{children}</div></details>;
}

export default function CameraInspector({camera:c,scene,objects,state,onChange,onDefault,onDelete,onPilot,onView,onCapture,piloting,running,onEdit}){
 useLocale();
 const characters=objects.filter(o=>o.type==='Персонаж'&&o.active!==false),pose=cameraPose(c,state,objects,scene.kind);
 const patch=value=>onChange(c.id,value);
 const hints={fov:'Меньше угол — крупнее герой; больше — больше окружения в кадре.',smoothing:'0 — мгновенное изменение. Чем больше значение, тем плавнее движение.',distance:'Расстояние от персонажа до камеры.',height:'Высота камеры над уровнем персонажа.',yaw:'Поворот камеры вокруг персонажа.',targetHeight:'Высота точки взгляда: например, лицо героя.'};
 const range=(key,label,min,max,step,unit)=><label className="camera-range" data-help-title={tr(label)} data-help={tr(hints[key])}><span>{tr(label)}<b>{Number(c[key]).toFixed(step<1?1:0)}{tr(unit)}</b></span><input aria-label={tr(label)} type="range" min={min} max={max} step={step} value={c[key]} onChange={e=>patch({[key]:Number(e.target.value)})}/><small>{tr(hints[key])}</small></label>;
 return <div className="camera-inspector">
  <div className="inspector-identity"><Icon name="Video" size={25}/><div><strong>{c.name}</strong><small>{scene.location}</small></div></div>
  {running&&<p className="camera-step-note">{tr("Остановите предпросмотр, чтобы изменить камеру. ")}<Button icon="Square" onClick={onEdit}>{tr("Остановить")}</Button></p>}
  <Section title={tr("Камера")}>
   <fieldset disabled={running}>
    <Field label={tr("Название")}><input aria-label={tr("Название камеры")} value={c.name} onChange={e=>patch({name:e.target.value})}/></Field>
    <Button icon={c.id===scene.defaultCameraId?'Star':'StarOff'} disabled={c.id===scene.defaultCameraId} onClick={()=>onDefault(c.id)}>{c.id===scene.defaultCameraId?tr("Основная камера"):tr("Сделать основной")}</Button>
    <Field label={tr("Режим")}><Select value={c.mode} options={[["fixed","Стоит на месте"],...(characters.length||c.mode==='follow'?[["follow","Следует за персонажем"]]:[])]} onChange={mode=>patch(mode==='fixed'?{mode,position:pose.position,target:pose.target}:{mode,followTargetId:c.followTargetId||characters[0]?.id})}/></Field>
    {c.mode==='follow'&&<><Field label={tr("Персонаж")}><Select value={c.followTargetId||''} options={[["","Выберите персонажа"],...(!characters.some(o=>o.id===c.followTargetId)&&c.followTargetId?[[c.followTargetId,'Персонаж недоступен']]:[]),...characters.map(o=>[o.id,literalLabel(o.name)])]} onChange={followTargetId=>patch({followTargetId})}/></Field>{pose.targetMissing&&<p className="camera-error">{tr("Персонаж скрыт или отсутствует в этой сабсцене. Используется сохранённый неподвижный кадр.")}</p>}</>}
   </fieldset>
  </Section>
  <Section title={tr("Объектив и движение")}>
   <fieldset disabled={running} className="camera-lens-grid">
    {range('fov','Угол обзора',20,90,1,'°')}{range('smoothing','Плавность',0,2,.1,' с')}
    {c.mode==='follow'&&<>{range('distance','Расстояние',.6,10,.1,' м')}{range('height','Высота камеры',.3,5,.1,' м')}{range('yaw','Ракурс вокруг героя',-180,180,1,'°')}{range('targetHeight','Смотреть на высоту',.2,1.6,.1,' м')}</>}
   </fieldset>
  </Section>
  <Section title={tr("Положение и точка взгляда")}>
   {c.mode==='follow'&&<p className="camera-step-note">{tr("Координаты вычисляются по положению персонажа. Настройте расстояние, высоту и ракурс выше.")}</p>}
   <fieldset disabled={running||c.mode==='follow'} className="camera-coordinate-fields">{['position','target'].map(key=><div className="camera-vector" key={key}><span>{key==='position'?tr("Положение камеры"):tr("Точка взгляда")}</span><div>{c[key].map((value,i)=><label key={i}><b className={'axis-'+'XYZ'[i]}>{'XYZ'[i]}</b><input aria-label={`${tr(key==='position'?'Камера':'Точка взгляда')} ${'XYZ'[i]}`} type="number" step=".1" value={Number(value.toFixed(2))} onChange={e=>patch({[key]:c[key].map((n,j)=>i===j?Number(e.target.value):n)})}/></label>)}</div></div>)}</fieldset>
  </Section>
  <Section title={tr("Настроить кадр в сцене")}>
   <div className="camera-shot-actions"><Button icon="Eye" onClick={()=>onView(c.id)}>{tr("Посмотреть кадр")}</Button><Button icon="Move" disabled={running} className={piloting===c.id?'active':''} onClick={()=>onPilot(c.id)}>{piloting===c.id?tr("Настройка в сцене…"):tr("Настроить в сцене")}</Button><Button icon="ScanLine" disabled={running} onClick={()=>onCapture(c.id)}>{piloting===c.id?tr("Сохранить ракурс"):tr("Взять текущий вид сцены")}</Button></div>
   <p className="camera-step-note">{piloting===c.id?tr("Подберите кадр мышью в сцене, затем нажмите «Сохранить ракурс». Крестик над сценой отменяет настройку."):tr("«Настроить в сцене» открывает управление мышью. «Взять текущий вид» сохраняет ракурс из окна сцены.")}</p>
  </Section>
  <Button icon="Trash2" disabled={running} onClick={()=>onDelete(c.id)}>{tr("Удалить камеру")}</Button>
 </div>;
}
