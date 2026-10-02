import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import {Icon,Button} from './StudioParts.jsx';

export default function CameraWorkspace({scene,objects,selected,onSelect,onCreate,onFollow,running,onEdit}){
 useLocale();
 const cameras=scene.cameras||[],characters=objects.filter(o=>o.type==='Персонаж'&&o.active!==false);
 return <div className="camera-workspace"><header><div><strong><Icon name="Video"/> {tr("Камеры")}</strong><small>{scene.location} · {cameras.length} {tr("ракурсов")}</small></div><Button icon="Plus" disabled={running} onClick={onCreate}>{tr("Создать из текущего вида")}</Button><Button icon="UserRoundCheck" disabled={running||!characters.length} onClick={onFollow}>{tr("Новая камера слежения")}</Button></header>
  {running&&<div className="camera-running">{tr("Настройки сохранённой постановки ")}<Button icon="Square" onClick={onEdit}>{tr("Остановить и редактировать")}</Button></div>}
  <p className="camera-list-note">{tr("Выберите камеру — сцена покажет её ракурс, а настройки откроются в инспекторе справа.")}</p>
  <div className="camera-component-list">{cameras.map(camera=><button className={selected===camera.id?'active':''} key={camera.id} onClick={()=>onSelect(camera.id)}><Icon name={camera.mode==='follow'?'UserRoundCheck':'Video'}/><span><strong>{camera.name}</strong><small>{camera.id===scene.defaultCameraId?tr("Основная · "):''}{camera.mode==='follow'?tr("Следует за персонажем"):tr("Фиксированный кадр")}</small></span><Icon name="ChevronRight" size={14}/></button>)}</div>
  {!cameras.length&&<p className="camera-list-note">{tr("Сохраните первый ракурс из текущего вида сцены.")}</p>}
 </div>;
}
