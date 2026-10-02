import {t as tr, useLocale} from './i18n.jsx';
import React from 'react';
import {interactionTargets,playerControls} from './interactionModel.js';
import {isObjectInScene} from './sceneEditing.js';
import './interactionTools.css';
export default function InteractionInspector({beat,project,scene,onChange}){
 useLocale();
 const targets=interactionTargets(beat),controls=playerControls(beat),items=project.objects.filter(o=>o.type==='Активный меш'&&isObjectInScene(o,scene));
 const patch=value=>onChange({controls:{...controls,...value}});
 return <div className="interaction-settings"><p>{tr("Сюжет продолжится после взаимодействия со всеми отмеченными предметами. Порядок свободный.")}</p>
 {items.map(o=><div key={o.id}><label className="check"><input type="checkbox" checked={targets.includes(o.id)} onChange={e=>{const signals=e.target.checked?[...targets,o.id]:targets.filter(id=>id!==o.id);onChange({signals,signal:signals[0]||''});}}/>{o.name}</label>{targets.includes(o.id)&&<label>{tr("Событие при взаимодействии")}<select value={beat.interactionEvents?.[o.id]||''} onChange={e=>onChange({interactionEvents:{...beat.interactionEvents,[o.id]:e.target.value}})}><option value="">{tr("Только отметить предмет")}</option>{project.events.filter(e=>!e.standardPreset).map(e=><option key={e.id} value={e.id}>{e.name}</option>)}</select></label>}</div>)}
 <label>{tr("Управление игрока")}<select value={controls.mode} onChange={e=>patch({mode:e.target.value})}><option value="none">{tr("Только клики по предметам")}</option><option value="wasd">{tr("Прямое управление · клавиши, стик, экран")}</option><option value="point-click">{tr("Point & click · ходьба по клику")}</option><option value="both">Прямое управление + point & click</option></select></label>
 {controls.mode!=='none'&&<><label>{tr("Управляемый персонаж")}<select value={controls.characterId} onChange={e=>patch({characterId:e.target.value})}><option value="">{tr("Выберите персонажа")}</option>{project.objects.filter(o=>o.type==='Персонаж'&&isObjectInScene(o,scene)).map(o=><option key={o.id} value={o.id}>{o.name}</option>)}</select></label><label>{tr("Скорость, м/с")}<input type="number" min="0.1" max="15" step="0.1" value={controls.speed} onChange={e=>patch({speed:Number(e.target.value)})}/></label><label>{tr("Дальность взаимодействия, м")}<input type="number" min="0" step="0.1" value={controls.radius} onChange={e=>patch({radius:Number(e.target.value)})}/></label><small>{tr("0 — осмотр с любой дистанции. Действие «Осмотреть» выбирает ближайший предмет. Включите область ходьбы и коллизии в настройках сцены для ограничения движения.")}</small></>}
 </div>;
}
