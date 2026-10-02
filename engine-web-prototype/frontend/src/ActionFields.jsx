import {PhysicsNumber} from './NavigationInspector.jsx';
import {projectAudioAssets} from './audioAssets.js';
import {typedValue,variableName} from './choiceModel.js';
import {ValueField} from './ChoiceInspector.jsx';
import React from "react";
import {characterAnimationOptions} from './characterModel.js';
import { TYPES } from "./studioModel.js";
import {sceneStagingPoints,stagingPointOptions,isObjectInScene} from './sceneEditing.js';

export default function ActionFields({
  action: a,
  project,
  onChange,
  compact = false,
  sceneId,
  onPickPosition,
}) {
  const field = (label, control) => (
    <label className="action-field">
      <span>{label}</span>
      {control}
    </label>
  );
  const select = (key, options) => (
    <select
      value={Array.isArray(a[key])?a[key].join(', '):a[key] ?? ""}
      onChange={(e) => {const value=e.target.value,coordinates=value.split(',').map(Number);onChange({[key]:a.type==='move'&&key==='value'&&coordinates.length===3&&coordinates.every(Number.isFinite)?coordinates:value});}}
    >
      {options.map((x) => {
        const [v, l] = Array.isArray(x) ? x : [x, x];
        return (
          <option value={v} key={v}>
            {l}
          </option>
        );
      })}
    </select>
  );
  const number = (key, min = 0) => (
    <input
      type="number"
      min={min}
      step=".1"
      value={a[key] ?? 2}
      onChange={(e) => onChange({ [key]: Number(e.target.value) })}
    />
  );
  const type = a.type,
    duration =
      ["move", "wait", "duck", "stop"].includes(type) ||
      (type === "sound" && !a.assetId);
  const target=project.objects.find(o=>o.id===a.target),activeScene=project.subscenes.find(scene=>scene.id===(sceneId||target?.subsceneId));
  const pointOptions=activeScene?stagingPointOptions(activeScene,project.objects,a.value):project.subscenes.flatMap(scene=>sceneStagingPoints(scene,project.objects).map(point=>[point.id,`${scene.name} · ${point.label}`]));
  if(type==='move'&&!activeScene&&!pointOptions.some(([id])=>id===a.value)){
    pointOptions.unshift([Array.isArray(a.value)?a.value.join(', '):a.value,Array.isArray(a.value)?`Координаты: ${a.value.join(', ')}`:`Сохранённая точка: ${a.value||'выберите точку'}`]);
  }
  return (
    <div className={"typed-action-fields " + (compact ? "compact" : "")}>
      {["move", "pose", "visibility", "highlight", "door"].includes(type) &&
        field(
          type === "pose" ? "Персонаж" : "Объект",
          select(
            "target",
            ([{id:"",name:"Выберите объект"},...project.objects])
              .filter((o) => !o.id || type !== "pose" || o.type === "Персонаж")
              .filter((o) => !o.id||!sceneId||!activeScene||isObjectInScene(o,activeScene)||o.id===a.target)
              .map((o) => [o.id, o.name]),
          ),
        )}
      {type === "move" &&
        <>
        {field("К точке", select("value", [["", "Выберите точку"],...pointOptions]))}
        <div className="action-coordinates">{['X','Y','Z'].map((axis,i)=><PhysicsNumber key={axis} label={axis} value={Array.isArray(a.value)?a.value[i]:0} onChange={n=>{const value=Array.isArray(a.value)?[...a.value]:[0,0,0];value[i]=n;onChange({value});}}/>)}</div>
        {onPickPosition&&<button type="button" onClick={()=>onPickPosition(a.id)}>Выбрать вручную на сцене</button>}
        {field('Звук шагов',select('footstepAssetId',[["","Не выбран · без звука"],...projectAudioAssets(project).filter(asset=>asset.kind==='sound').map(asset=>[asset.id,asset.name])]))}
        {a.footstepAssetId&&field('Громкость шагов',<input type="number" min="0" max="1" step="0.1" value={a.footstepVolume??1} onChange={e=>onChange({footstepVolume:Math.max(0,Math.min(1,Number(e.target.value)))})}/>)}
        {target?.type==='Персонаж'&&field('Анимация движения',select('animationId',[["","По настройкам персонажа"],...characterAnimationOptions(target,a.animationId)]))}
        </>}
      {type === "pose" &&
        field(
          "Анимация персонажа",
          select("value", [["","Выберите анимацию"],...characterAnimationOptions(target,a.value)]),
        )}
      {type === "camera" && <>
        {field("Камера",select("cameraId",[["","План по умолчанию"],...(a.cameraId&&!project.subscenes.some(s=>s.cameras?.some(c=>c.id===a.cameraId))?[[a.cameraId,"Камера удалена · выберите другую"]]:[]),...project.subscenes.flatMap(s=>(s.cameras||[]).map(c=>[c.id,`${s.location} · ${c.name}${c.mode==='follow'?' · слежение':''}`]))]))}
        {!a.cameraId&&field("План", select("value", ["Общий план", "Крупный план", "План предмета"]))}
      </>}
      {type === "weather" &&
        field("Погода", select("value", ["Ясно", "Дождь", "Гроза", "Туман", "Снег"]))}
      {type==='lighting'&&field('Освещение',select('value',['Тёплый свет','Холодный свет','Приглушить','Яркий свет','Выключить']))}
      {type==='particles'&&field('Частицы',select('value',['Светлячки','Лепестки','Выключить']))}
      {type==='door'&&field('Дверь',select('value',['Открыть','Закрыть']))}
      {type==='highlight'&&field('Подсказка',select('value',['Подсветить','Выключить']))}
      {type === "time" &&
        field(
          "Время суток",
          select("value", ["Рассвет", "День", "Закат", "Ночь"]),
        )}
      {type === "visibility" &&
        field("Видимость", select("value", ["Показать", "Скрыть"]))}
      {["music", "sound"].includes(type) && (
        <>
          {field(
            "Аудиофайл",
            select("assetId", [
              ["", "Без файла"],
              ...projectAudioAssets(project).filter(
                (s) => type !== "music" || s.kind === "music",
              ).map((s) => [s.id, s.name]),
            ]),
          )}
          {field(
            "Громкость · " +
              Math.round((a.volume ?? (type === "music" ? 0.42 : 1)) * 100) +
              "%",
            <input
              type="range"
              min="0"
              max="1"
              step=".01"
              value={a.volume ?? (type === "music" ? 0.42 : 1)}
              onChange={(e) => onChange({ volume: Number(e.target.value) })}
            />,
          )}
          {!compact && (
            <>
              {type === "music" ? (
                <>
                  <label className="check">
                    <input
                      type="checkbox"
                      checked={a.loop !== false}
                      onChange={(e) => onChange({ loop: e.target.checked })}
                    />
                    Повторять по кругу
                  </label>
                  {field("Плавный вход, сек", number("fade"))}
                </>
              ) : (
                <label className="check">
                  <input
                    type="checkbox"
                    checked={a.duck !== false}
                    onChange={(e) => onChange({ duck: e.target.checked })}
                  />
                  Приглушать музыку под голос
                </label>
              )}
            </>
          )}
        </>
      )}
      {["pause", "resume", "stop", "duck"].includes(type) && (
        <div className="action-target-note">Музыка · основной фон</div>
      )}
      {type==='variable'&&(()=>{const valueType=a.valueType||typeof project.variables[a.target],operation=a.operation||(String(a.value).startsWith('+')?'add':'set');return <>
       {field('Переменная',<select value={a.target} onChange={e=>{const target=e.target.value,valueType=typeof project.variables[target];onChange({target,valueType,operation:'set',value:typedValue('',valueType)});}}>{Object.keys(project.variables).map(id=><option key={id} value={id}>{variableName(id)}</option>)}</select>)}
       {valueType==='number'&&field('Операция',<select aria-label="Операция с переменной" value={operation} onChange={e=>onChange({operation:e.target.value,valueType,value:typedValue(a.value,valueType)})}><option value="set">Задать значение</option><option value="add">Прибавить</option></select>)}
       {field(operation==='add'&&valueType==='number'?'Прибавить число':'Значение',<ValueField label="Значение переменной" type={valueType} value={typedValue(a.value,valueType)} onChange={value=>onChange({value,valueType,operation:valueType==='number'?operation:'set'})}/>)}
       <small className="resource-note">Используйте эту переменную в проверках и условиях ответов.</small>
      </>;})()}
      {type === "wait" && (
        <>
          {field(
            "Ожидание",
            <select
              value={a.waitFor ? "dependency" : "delay"}
              onChange={(e) =>
                onChange({
                  waitFor:
                    e.target.value === "delay"
                      ? ""
                      : project.events.find((e) => e.id !== a.id)?.id || "",
                })
              }
            >
              <option value="delay">Пауза на время</option>
              <option value="dependency">
                Результат события · диагностика
              </option>
            </select>,
          )}
          {a.waitFor &&
            field(
              "Событие",
              select(
                "waitFor",
                project.events.map((e) => [e.id, e.name]),
              ),
            )}
        </>
      )}
      {duration &&
        field(
          type === "stop" ? "Затухание, сек" : "Длительность, сек",
          number("duration", 0.1),
        )}
      {!compact && (
        <div className="action-completion">
          <span>Продолжение группы</span>
          <strong>
            {TYPES[type]?.completion === "CONTINUOUS"
              ? "После запуска · фон остаётся"
              : type === "sound" && a.assetId
                ? "Когда закончится файл"
                : duration
                  ? "После завершения"
                  : "После применения"}
          </strong>
        </div>
      )}
    </div>
  );
}
