import React from 'react';
import {Icon,Button} from './StudioParts.jsx';

export default function CameraWorkspace({scene,objects,selected,onSelect,onCreate,onFollow,running,onEdit}){
 const cameras=scene.cameras||[],characters=objects.filter(o=>o.type==='Персонаж'&&o.active!==false);
 return <div className="camera-workspace"><header><div><strong><Icon name="Video"/> Камеры</strong><small>{scene.location} · {cameras.length} ракурсов</small></div><Button icon="Plus" disabled={running} onClick={onCreate}>Создать из текущего вида</Button><Button icon="UserRoundCheck" disabled={running||!characters.length} onClick={onFollow}>Новая камера слежения</Button></header>
  {running&&<div className="camera-running">Настройки сохранённой постановки <Button icon="Square" onClick={onEdit}>Остановить и редактировать</Button></div>}
  <p className="camera-list-note">Выберите камеру — её настройки откроются в инспекторе справа, как у остальных компонентов сцены.</p>
  <div className="camera-component-list">{cameras.map(camera=><button className={selected===camera.id?'active':''} key={camera.id} onClick={()=>onSelect(camera.id)}><Icon name={camera.mode==='follow'?'UserRoundCheck':'Video'}/><span><strong>{camera.name}</strong><small>{camera.id===scene.defaultCameraId?'Основная · ':''}{camera.mode==='follow'?'Следует за персонажем':'Фиксированный кадр'}</small></span><Icon name="ChevronRight" size={14}/></button>)}</div>
  {!cameras.length&&<p className="camera-list-note">Сохраните первый ракурс из текущего вида сцены.</p>}
 </div>;
}
