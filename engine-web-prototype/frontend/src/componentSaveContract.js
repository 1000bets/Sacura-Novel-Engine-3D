import {isComponent} from './componentModel.js';
export function componentSaveManifest(project){
 return {version:1,instances:project.gameplay.mechanics.filter(m=>isComponent(project.gameplay.mechanicDefinitions.find(d=>d.id===m.definitionId&&d.version===m.definitionVersion))).map(m=>({id:m.id,definitionId:m.definitionId,definitionVersion:m.definitionVersion,stateVersion:project.gameplay.mechanicDefinitions.find(d=>d.id===m.definitionId&&d.version===m.definitionVersion).stateVersion,scope:structuredClone(m.scope),references:structuredClone(m.references||{}),fieldBindings:structuredClone(m.fieldBindings||[])}))};
}
export function assertComponentSaveManifest(project,save){
 if(save.version<3)return;
 const canonical=v=>Array.isArray(v)?v.map(canonical):v&&typeof v==='object'?Object.fromEntries(Object.keys(v).sort().map(k=>[k,canonical(v[k])])):v;
 if(JSON.stringify(canonical(save.components))!==JSON.stringify(canonical(componentSaveManifest(project))))throw Error('Сохранение компонентов не соответствует определениям, версиям или привязкам проекта. Загрузка отменена.');
}
export function assertComponentSaveCheckpoint(rt){
 const tasks=[...(rt.game?.componentTasks?.values()||[])].filter(t=>t.status==='running');
 if(tasks.length){const names=tasks.slice(0,4).map(t=>{const m=rt.project.gameplay.mechanics.find(m=>m.id===t.instanceId),d=rt.project.gameplay.mechanicDefinitions.find(d=>d.id===m?.definitionId);return (m?.name||d?.name||t.instanceId)+' → '+(d?.methods[t.methodId]?.name||t.methodId);});throw Error('Сохранение не записано: выполняются задачи компонентов '+names.join(', ')+'. Произвольный Promise не восстанавливается. Завершите или остановите задачи и сохраните контрольную точку.');}
}
