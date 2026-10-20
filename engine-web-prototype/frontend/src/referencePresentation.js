import {referenceRecords} from './valueSchemas.js';
import {isObjectInScene} from './sceneEditing.js';
export const schemaTypeLabel=s=>({integer:'целое число',number:'число',boolean:'да / нет',string:'текст',collection:'список',array:'список',object:'структура',struct:'структура',vector:'вектор',reference:'ссылка'})[s.type]||'значение';
export function referenceLabel(project,type,record){
 const display=x=>x.name||x.text||({beat:'Пустой диалог',event:'Событие без названия',function:'Функция без названия',object:'Объект без названия',scene:'Сцена без названия'})[type]||'Без названия';
 const name=display(record);
 if(type==='variable')return 'Проект · '+name+' · '+schemaTypeLabel({type:project.variableTypes?.[name]||typeof project.variables[name]});
 const same=referenceRecords(project,type).filter(x=>display(x)===name);
 if(same.length<2)return name;
 const scenes=type==='object'?project.subscenes.filter(s=>isObjectInScene(record,s)).map(s=>s.name).slice(0,2).join(', '):'';
 return name+(scenes?' · '+scenes:'')+' · '+(same.findIndex(x=>x.id===record.id)+1);
}
