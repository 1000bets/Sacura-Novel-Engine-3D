import {schemaDefaults,reference} from './valueSchemas.js';

export const componentDefinitionForInstance=(project,id)=>{if(id?.op==='constant')id=id.value;const instance=project.gameplay?.mechanics?.find(m=>m.id===(typeof id==='string'?id:id?.id));return project.gameplay?.mechanicDefinitions?.find(d=>d.id===instance?.definitionId&&d.version===instance.definitionVersion&&d.componentVersion===1);};
export function componentMethodTarget(project,node){
 const descriptor=node.componentMethod,definition=descriptor?project.gameplay?.mechanicDefinitions?.find(d=>d.id===descriptor.definitionId&&d.version===descriptor.definitionVersion):componentDefinitionForInstance(project,node.call?.parameters?.instance);
 const methodId=descriptor?.methodId||node.call?.parameters?.methodId,method=definition?.methods?.[methodId];return {definition,methodId,method};
}
export const componentFunctionOwner=(project,functionId)=>{for(const definition of project.gameplay?.mechanicDefinitions||[])for(const [methodId,method]of Object.entries(definition.methods||{}))if(method.logic?.functionId===functionId)return {definition,methodId,method};};
export function assertComponentMethodReceiver(project,descriptor,receiver,methodId){
 if(!descriptor)return;
 const definition=componentDefinitionForInstance(project,receiver);
 if(!definition||definition.id!==descriptor.definitionId||definition.version!==descriptor.definitionVersion||methodId!==descriptor.methodId)throw Error('Получатель не соответствует выбранному методу компонента. Выберите экземпляр исходного определения.');
 if(!Object.hasOwn(definition.methods,methodId))throw Error('Метод компонента удалён: '+methodId);
}
export function componentMethodChoices(project,currentFunction){
 const owner=componentFunctionOwner(project,currentFunction?.id);
 return (project.gameplay?.mechanicDefinitions||[]).filter(d=>d.componentVersion===1).flatMap(d=>Object.entries(d.methods).map(([methodId,m])=>({key:'component:'+d.id+':'+methodId,label:d.name+' · '+m.name,category:'Метод компонента',description:'Получатель · аргументы · результат',make:()=>({kind:'operation',componentMethod:{definitionId:d.id,definitionVersion:d.version,methodId},call:{id:'sacura.component.call',version:1,parameters:{...(owner?.definition.id===d.id?{instance:{op:'get',path:'self'}}:{}),methodId,args:schemaDefaults(m.parameters)}},next:''})})));
}
export const componentSignalName=(resourceId,key)=>'user.component-signal.'+resourceId.replace(/[^a-z0-9-]/g,'-')+'.'+key.replace(/[^a-z0-9-]/g,'-');
export const componentInstanceName=(project,m)=>m?.name||componentDefinitionForInstance(project,m?.id)?.name||'Компонент недоступен';
export const componentOwnerName=(project,scope)=>scope?.kind==='project'?(project.title||'Проект'):scope?.kind==='scene'?(project.subscenes.find(s=>s.id===scope.id)?.name||'Сцена удалена'):(project.objects.find(o=>o.id===scope?.id)?.name||'Объект удалён');
export function componentCallSummary(project,call){const p=call?.parameters||{},m=project.gameplay?.mechanics?.find(m=>m.id===(typeof p.instance==='string'?p.instance:p.instance?.id)),d=componentDefinitionForInstance(project,m?.id);return {subject:m?componentOwnerName(project,m.scope):'Получатель не выбран',result:d?.methods?.[p.methodId]?.name||'Метод недоступен',detail:m?componentInstanceName(project,m):'Выберите экземпляр'};}
export const isComponentCall=id=>['sacura.component.call','sacura.component.start'].includes(id);
