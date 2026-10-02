import React from 'react';
import {Icon,Button,Field,Select} from './StudioParts.jsx';
import {cameraPose} from './cameraModel.js';

function Section({title,children}){
 return <details className="inspector-section" open><summary><Icon name="ChevronRight" size={12}/><span>{title}</span></summary><div className="inspector-fields">{children}</div></details>;
}

export default function CameraInspector({camera:c,scene,objects,state,onChange,onDefault,onDelete,onPilot,onView,onCapture,piloting,running,onEdit}){
 const characters=objects.filter(o=>o.type==='Персонаж'&&o.active!==false),pose=cameraPose(c,state,objects,scene.kind);
 const patch=value=>onChange(c.id,value);
 const hints={fov:'Меньше угол — крупнее герой; больше — больше окружения в кадре.',smoothing:'0 — мгновенное изменение. Чем больше значение, тем плавнее движение.',distance:'Расстояние от персонажа до камеры.',height:'Высота камеры над уровнем персонажа.',yaw:'Поворот камеры вокруг персонажа.',targetHeight:'Высота точки взгляда: например, лицо героя.'};
 const range=(key,label,min,max,step,unit)=><label className="camera-range" data-help-title={label} data-help={hints[key]}><span>{label}<b>{Number(c[key]).toFixed(step<1?1:0)}{unit}</b></span><input aria-label={label} type="range" min={min} max={max} step={step} value={c[key]} onChange={e=>patch({[key]:Number(e.target.value)})}/><small>{hints[key]}</small></label>;
 return <div className="camera-inspector">
  <div className="inspector-identity"><Icon name="Video" size={25}/><div><strong>{c.name}</strong><small>{scene.location}</small></div></div>
  {running&&<p className="camera-step-note">Остановите предпросмотр, чтобы изменить камеру. <Button icon="Square" onClick={onEdit}>Остановить</Button></p>}
  <Section title="Камера">
   <fieldset disabled={running}>
    <Field label="Название"><input aria-label="Название камеры" value={c.name} onChange={e=>patch({name:e.target.value})}/></Field>
    <Button icon={c.id===scene.defaultCameraId?'Star':'StarOff'} disabled={c.id===scene.defaultCameraId} onClick={()=>onDefault(c.id)}>{c.id===scene.defaultCameraId?'Основная камера':'Сделать основной'}</Button>
    <Field label="Режим"><Select value={c.mode} options={[["fixed","Стоит на месте"],...(characters.length||c.mode==='follow'?[["follow","Следует за персонажем"]]:[])]} onChange={mode=>patch(mode==='fixed'?{mode,position:pose.position,target:pose.target}:{mode,followTargetId:c.followTargetId||characters[0]?.id})}/></Field>
    {c.mode==='follow'&&<><Field label="Персонаж"><Select value={c.followTargetId||''} options={[["","Выберите персонажа"],...(!characters.some(o=>o.id===c.followTargetId)&&c.followTargetId?[[c.followTargetId,'Персонаж недоступен']]:[]),...characters.map(o=>[o.id,o.name])]} onChange={followTargetId=>patch({followTargetId})}/></Field>{pose.targetMissing&&<p className="camera-error">Персонаж скрыт или отсутствует в этой сабсцене. Используется сохранённый неподвижный кадр.</p>}</>}
   </fieldset>
  </Section>
  <Section title="Объектив и движение">
   <fieldset disabled={running} className="camera-lens-grid">
    {range('fov','Угол обзора',20,90,1,'°')}{range('smoothing','Плавность',0,2,.1,' с')}
    {c.mode==='follow'&&<>{range('distance','Расстояние',.6,10,.1,' м')}{range('height','Высота камеры',.3,5,.1,' м')}{range('yaw','Ракурс вокруг героя',-180,180,1,'°')}{range('targetHeight','Смотреть на высоту',.2,1.6,.1,' м')}</>}
   </fieldset>
  </Section>
  <Section title="Положение и точка взгляда">
   {c.mode==='follow'&&<p className="camera-step-note">Координаты вычисляются по положению персонажа. Настройте расстояние, высоту и ракурс выше.</p>}
   <fieldset disabled={running||c.mode==='follow'} className="camera-coordinate-fields">{['position','target'].map(key=><div className="camera-vector" key={key}><span>{key==='position'?'Положение камеры':'Точка взгляда'}</span><div>{c[key].map((value,i)=><label key={i}><b className={'axis-'+'XYZ'[i]}>{'XYZ'[i]}</b><input aria-label={`${key==='position'?'Камера':'Точка взгляда'} ${'XYZ'[i]}`} type="number" step=".1" value={Number(value.toFixed(2))} onChange={e=>patch({[key]:c[key].map((n,j)=>i===j?Number(e.target.value):n)})}/></label>)}</div></div>)}</fieldset>
  </Section>
  <Section title="Настроить кадр в сцене">
   <div className="camera-shot-actions"><Button icon="Eye" onClick={()=>onView(c.id)}>Посмотреть кадр</Button><Button icon="Move" disabled={running} className={piloting===c.id?'active':''} onClick={()=>onPilot(c.id)}>{piloting===c.id?'Настройка в сцене…':'Настроить в сцене'}</Button><Button icon="ScanLine" disabled={running} onClick={()=>onCapture(c.id)}>{piloting===c.id?'Сохранить ракурс':'Взять текущий вид сцены'}</Button></div>
   <p className="camera-step-note">{piloting===c.id?'Подберите кадр мышью в сцене, затем нажмите «Сохранить ракурс». Крестик над сценой отменяет настройку.':'«Настроить в сцене» открывает управление мышью. «Взять текущий вид» сохраняет ракурс из окна сцены.'}</p>
  </Section>
  <Button icon="Trash2" disabled={running} onClick={()=>onDelete(c.id)}>Удалить камеру</Button>
 </div>;
}
