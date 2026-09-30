import {makeAction, TYPES} from './model.js';
import {AUDIO_ASSETS} from './studioModel.js';
import {isObjectInScene, sceneStagingPoints, findStagingPoint} from './sceneEditing.js';
import {enabledCharacterAnimations} from './characterModel.js';

export const ACTION_CATEGORIES = [
  {id:'staging', label:'Персонажи и камера', icon:'Clapperboard', types:['move','pose','camera']},
  {id:'audio', label:'Звук и музыка', icon:'Music2', types:['music','sound','pause','resume','stop','duck']},
  {id:'world', label:'Мир и предметы', icon:'Sun', types:['weather','time','lighting','particles','visibility','door','highlight']},
  {id:'logic', label:'Логика и паузы', icon:'GitBranch', types:['wait','variable']},
];
export const ACTION_HINTS = {
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
    return type==='pose'?'Сначала добавьте персонажа в сабсцену':'Сначала добавьте объект в сабсцену';
  if(type==='variable'&&!Object.keys(project.variables).length)return 'Сначала создайте переменную проекта';
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
  const source=object?.name||({camera:'Камера',audio:'Музыка',world:'Окружение'}[action.target])||'Выберите объект';
  const point=findStagingPoint(sceneStagingPoints(scene,project.objects),action.value);
  const audio=AUDIO_ASSETS.find(asset=>asset.id===action.assetId);
  const value=Array.isArray(action.value)?action.value.join(', '):String(action.value??'');
  if(action.type==='move')return {subject:source,result:point?.label||value||'Выберите точку',detail:`${action.duration??2} с`,icon:'MapPin'};
  if(action.type==='pose')return {subject:source,result:enabledCharacterAnimations(object).find(a=>a.id===action.value)?.name||value||'Выберите анимацию',detail:'Применить позу',icon:'PersonStanding'};
  if(action.type==='camera')return {subject:'Камера',result:project.subscenes.flatMap(s=>s.cameras||[]).find(c=>c.id===action.cameraId)?.name||value,detail:'Сменить ракурс',icon:'Video'};
  if(['music','sound'].includes(action.type))return {subject:action.type==='music'?'Фоновая музыка':'Звук',result:audio?.name||'Аудиофайл не выбран',detail:action.type==='music'?'Продолжается в фоне':audio?'До конца файла':`${action.duration??2} с · без файла`,icon:TYPES[action.type].icon};
  if(action.type==='wait')return {subject:'Пауза',result:action.waitFor?project.events.find(e=>e.id===action.waitFor)?.name||'Событие недоступно':`${action.duration??2} секунды`,detail:action.waitFor?'Дождаться события':'Затем продолжить',icon:'Hourglass'};
  if(action.type==='variable')return {subject:action.target,result:`${action.operation==='add'?'Прибавить':'Задать'} ${value}`,detail:'Изменить переменную',icon:'SlidersHorizontal'};
  if(action.type==='weather')return {subject:'Окружение',result:value,detail:'Применить сразу',icon:({'Ясно':'Sun','Дождь':'CloudRain','Гроза':'CloudLightning','Туман':'CloudFog','Снег':'CloudSnow'})[value]||'Cloud'};
  if(action.type==='time')return {subject:'Время суток',result:value,detail:'Применить сразу',icon:({'Рассвет':'Sunrise','День':'Sun','Закат':'Sunset','Ночь':'Moon'})[value]||'Sun'};
  return {subject:source,result:value||TYPES[action.type]?.label,detail:TYPES[action.type]?.completion==='FINITE'?`${action.duration??2} с`:'Применить сразу',icon:TYPES[action.type]?.icon};
}
export function actionTiming(action) {
  if(action.wait==='NONE')return 'Не задерживает следующий шаг';
  if(action.wait==='STARTED'||TYPES[action.type]?.completion==='CONTINUOUS')return 'Запустить и продолжить';
  return TYPES[action.type]?.completion==='FINITE'?'Дождаться завершения':'Применить и продолжить';
}
export function actionCount(count) {
  const form=new Intl.PluralRules('ru').select(count);
  return `${count} ${{one:'действие',few:'действия',many:'действий',other:'действия'}[form]}`;
}
