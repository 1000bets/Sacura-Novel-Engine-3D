import {GAME_COMMANDS} from './gameplayModel.js';
import {animationLabel} from './characterPresentation.js';
import {t as tr,plural} from './i18n.js';
import {projectAudioAssets} from './audioAssets.js';
import {makeAction, TYPES} from './model.js';
import {isObjectInScene, sceneStagingPoints, findStagingPoint} from './sceneEditing.js';
import {enabledCharacterAnimations} from './characterModel.js';

export const ACTION_CATEGORIES = [
  {id:'staging', label:'Персонажи и камера', icon:'Clapperboard', types:['move','pose','camera']},
  {id:'audio', label:'Звук и музыка', icon:'Music2', types:['music','sound','pause','resume','stop','duck']},
  {id:'world', label:'Мир и предметы', icon:'Sun', types:['weather','time','lighting','particles','visibility','door','highlight']},
  {id:'logic', label:'Логика и паузы', icon:'GitBranch', types:['wait','variable','gameplay']},
];
export const ACTION_HINTS = {
 gameplay:'Здоровье, оружие, предметы, таймеры и функции',
  move:'Отправить героя или предмет к точке', pose:'Выбрать анимацию персонажа', camera:'Показать сцену с другого ракурса',
  music:'Запустить музыку в фоне', sound:'Озвучка или звуковой эффект', pause:'Поставить фоновую музыку на паузу',
  resume:'Вернуть музыку с места остановки', stop:'Завершить фоновую музыку', duck:'Снизить громкость фона',
  weather:'Дождь, снег, туман или ясное небо', time:'Рассвет, день, закат или ночь', lighting:'Изменить освещение сцены',
  particles:'Добавить лепестки или светлячков', visibility:'Показать или скрыть предмет', door:'Открыть или закрыть дверь',
  highlight:'Привлечь внимание к предмету', wait:'Выдержать паузу перед продолжением', variable:'Изменить значение в истории',
};
const objectTypes = ['move','pose','visibility','door','highlight'];
export function availableActionTargets(type, project, scene) {
  return project.objects.filter(o=>(!scene||isObjectInScene(o,scene))&&(type!=='pose'||o.type==='Персонаж'));
}
export function actionUnavailable(type, project, scene) {
  if(objectTypes.includes(type)&&!availableActionTargets(type,project,scene).length)
    return type==='pose'?tr("Сначала добавьте персонажа в сабсцену"):tr("Сначала добавьте объект в сабсцену");
  if(type==='variable'&&!Object.keys(project.variables).length)return tr("Сначала создайте переменную проекта");
  return '';
}
// New commands use the actual scene, never demo-only IDs such as "alice" or "trust".
export function contextualAction(type, project, scene, preferredTarget) {
  const action=makeAction(type);
  if(objectTypes.includes(type)) {
    const targets=availableActionTargets(type,project,scene);
    action.target=(targets.find(o=>o.id===preferredTarget)||targets.find(o=>o.id===action.target||o.builtin===action.target)||(['pose','move'].includes(type)&&targets.find(o=>o.type==='Персонаж'))||targets[0])?.id||'';
  }
  if(type==='move')action.value=sceneStagingPoints(scene,project.objects)[0]?.id||[0,0,0];
  if(type==='pose')action.value=enabledCharacterAnimations(project.objects.find(o=>o.id===action.target))[0]?.id||'';
  if(type==='variable') {
    action.target=Object.keys(project.variables)[0]||'';
    action.valueType=typeof project.variables[action.target];
    action.operation='set';
    action.value=project.variables[action.target]??0;
  }
  return action;
}
export function describeAction(action, project, scene) {
  const object=project.objects.find(o=>o.id===action.target);
  const source=object?.name||({camera:tr("Камера"),audio:tr("Музыка"),world:tr("Окружение")}[action.target])||tr('Выберите объект');
  const point=findStagingPoint(sceneStagingPoints(scene,project.objects),action.value);
  const audio=projectAudioAssets(project).find(asset=>asset.id===action.assetId);
  const value=Array.isArray(action.value)?action.value.join(', '):String(action.value??'');
  if(action.type==='gameplay')return {subject:tr('Игровая команда'),result:tr(GAME_COMMANDS[action.command]||action.command),detail:JSON.stringify(action.value),icon:'Gamepad2'};
  if(action.type==='move')return {subject:source,result:point?.label||value||tr('Выберите точку'),detail:tr("{0} с", [action.duration??2]),icon:'MapPin'};
  if(action.type==='pose')return {subject:source,result:enabledCharacterAnimations(object).find(a=>a.id===action.value)?.name||value||tr('Выберите анимацию'),detail:tr("Применить позу"),icon:'PersonStanding'};
  if(action.type==='camera')return {subject:tr("Камера"),result:project.subscenes.flatMap(s=>s.cameras||[]).find(c=>c.id===action.cameraId)?.name||value,detail:tr("Сменить ракурс"),icon:'Video'};
  if(['music','sound'].includes(action.type))return {subject:action.type==='music'?tr("Фоновая музыка"):tr("Звук"),result:audio?.name||tr('Аудиофайл не выбран'),detail:action.type==='music'?tr("Продолжается в фоне"):audio?tr("До конца файла"):tr("{0} с · без файла", [action.duration??2]),icon:TYPES[action.type].icon};
  if(action.type==='wait')return {subject:tr("Пауза"),result:action.waitFor?project.events.find(e=>e.id===action.waitFor)?.name||tr('Событие недоступно'):tr("{0} секунды", [action.duration??2]),detail:action.waitFor?tr("Дождаться события"):tr("Затем продолжить"),icon:'Hourglass'};
  if(action.type==='variable')return {subject:action.target,result:`${action.operation==='add'?tr("Прибавить"):tr("Задать")} ${value}`,detail:tr("Изменить переменную"),icon:'SlidersHorizontal'};
  if(action.type==='weather')return {subject:tr("Окружение"),result:tr(value),detail:tr("Применить сразу"),icon:({'Ясно':'Sun','Дождь':'CloudRain','Гроза':'CloudLightning','Туман':'CloudFog','Снег':'CloudSnow'})[value]||'Cloud'};
  if(action.type==='time')return {subject:tr("Время суток"),result:tr(value),detail:tr("Применить сразу"),icon:({'Рассвет':'Sunrise','День':'Sun','Закат':'Sunset','Ночь':'Moon'})[value]||'Sun'};
  return {subject:source,result:value||tr(TYPES[action.type]?.label),detail:TYPES[action.type]?.completion==='FINITE'?tr("{0} с", [action.duration??2]):tr("Применить сразу"),icon:TYPES[action.type]?.icon};
}
export function actionTiming(action) {
  if(action.endMode==='event'&&['music','sound','pause','stop'].includes(action.type))return tr("Завершить вместе с событием");
  if(['pause','stop'].includes(action.type))return tr("Дождаться выбранной точки завершения");
  if(action.wait==='NONE')return tr("Не задерживает следующий шаг");
  if(action.wait==='STARTED'||TYPES[action.type]?.completion==='CONTINUOUS')return tr("Запустить и продолжить");
  return TYPES[action.type]?.completion==='FINITE'?tr("Дождаться завершения"):tr("Применить и продолжить");
}
export function actionCount(count) { return plural(count,'действие','действия','действий','action'); }

export function actionSummary(action, project) {
 if(action.type==='gameplay')return tr(GAME_COMMANDS[action.command]||'Игровая команда')+' → '+JSON.stringify(action.value);
 const object=project.objects?.find(o=>o.id===action.target);
 const subject=object?.name||tr(TYPES[action.type]?.label)||action.type;
 const clip=action.type==='pose'&&enabledCharacterAnimations(object).find(item=>item.id===action.value);
 const value=clip?animationLabel(object,clip):action.type==='camera'&&action.cameraId
  ? project.subscenes.flatMap(s=>s.cameras||[]).find(c=>c.id===action.cameraId)?.name||tr('Камера недоступна')
  : Array.isArray(action.value)?action.value.join(', ')
  : ['weather','time','visibility','door','particles','pause','resume','stop','lighting','highlight'].includes(action.type)?tr(action.value):action.value??'';
 return `${subject} → ${value}`;
}
