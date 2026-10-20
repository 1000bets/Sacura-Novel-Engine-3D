import React from 'react';
import {t as tr,useLocale} from './i18n.jsx';
import {GameSelect,GameNumber} from './GameplayCommandFields.jsx';
import {connectMechanicInput,retainMechanicAttachment} from './mechanicsModel.js';
import {referenceLabel} from './referencePresentation.js';
import './cameraInspector.css';

// Edit the same native configuration consumed by Play and game exports.
export default function PlayerControlFields({project,mechanic,onChange,advancedOnly=false}){
 useLocale();const g=project.gameplay,camera=mechanic.type==='camera',settings=camera?g.cameraSettings:g.controller;
 const edit=fn=>onChange(p=>{retainMechanicAttachment(p,mechanic);fn(p);});
 const change=patch=>edit(p=>Object.assign(camera?p.gameplay.cameraSettings:p.gameplay.controller,patch));
 const number=(key,label,min=0,max,step=.1)=><GameNumber label={label} value={Number(settings[key].toFixed(6))} min={min} max={max} step={step} onChange={v=>change({[key]:v})}/>;
 const input=<details className="player-control-details"><summary>{tr('Действия ввода')}</summary><div><button type="button" onClick={()=>onChange(p=>connectMechanicInput(p,mechanic))}>{tr('Подключить клавиши и мышь')}</button></div></details>;
 const advanced=camera?<>{number('eyeHeight','Высота взгляда')}{number('sensitivity','Чувствительность мыши',0,undefined,.001)}</>:<>{number('jumpSpeed','Сила прыжка')}{number('gravity','Гравитация')}</>;
 const gameplayCamera=['firstPerson','thirdPerson'].includes(g.camera.mode);
 return <div className="player-control-fields">
  {!advancedOnly&&(camera?<>
   <GameSelect label="Режим" value={g.camera.mode} options={[["authored","Камеры сцены"],["firstPerson","От первого лица"],["thirdPerson","От третьего лица"]]} onChange={mode=>edit(p=>{p.gameplay.camera.mode=mode;})}/>
   {gameplayCamera&&<><GameSelect label="Объект" value={g.camera.targetId} options={[["","Выберите…"],...project.objects.map(o=>[o.id,referenceLabel(project,'object',o)])]} onChange={targetId=>edit(p=>{p.gameplay.camera.targetId=targetId;})}/>{number('fov','Угол обзора',1,179,1)}{g.camera.mode==='thirdPerson'&&number('distance','Расстояние')}
   <label className="game-check"><input type="checkbox" checked={!!g.camera.pointerLock} onChange={e=>edit(p=>{p.gameplay.camera.pointerLock=e.target.checked;})}/>{tr('Захватывать мышь')}</label></>}
  </>:<>
   <GameSelect label="Движение" value={g.controller.mode} options={[["none","Отключено"],["point-click","По щелчку"],["wasd","Клавиши / геймпад"],["both","Клавиши и щелчок"]]} onChange={mode=>edit(p=>{p.gameplay.controller.mode=mode;})}/>
   {number('speed','Скорость')}
  </>)}
  {(!camera||gameplayCamera)&&(advancedOnly?advanced:<details className="player-control-details"><summary>{tr(camera?'Дополнительно':'Прыжок и гравитация')}</summary><div>{advanced}</div></details>)}
  {(!camera||gameplayCamera)&&input}
 </div>;
}
