import {runComponentBehavior,endComponentBehavior} from '../behaviorTrees.js';
import {startOwnedComponentTask} from '../componentInstances.js';
import {registerOperation} from '../operationRegistry.js';

// One entry point for story Events, input, UI, graphs and JS alike. The schemas
// depend on the selected instance and stable method ID, including void results.
registerOperation({id:'sacura.component.call',version:1,
 contexts:['event','function','rule','mechanic','widget'],editor:{label:'Вызвать метод компонента',category:'Компоненты'},
 parameters:{type:'object',properties:{instance:{type:'reference',referenceType:'mechanic',label:'Экземпляр'},methodId:{type:'string',label:'Метод',componentMethodFor:'instance'},args:{type:'componentArguments',parameter:'instance',methodParameter:'methodId',label:'Аргументы'}}},
 result:{type:'componentResult',parameter:'instance',methodParameter:'methodId'}
},(p,c)=>{if(!c.capabilities)throw Error('Сеанс компонентов недоступен.');return c.capabilities.callComponent(p.instance,p.methodId,p.args,{...c,abortSignal:c.abortSignal});});
registerOperation({id:'sacura.component.start',version:1,
 contexts:['event','function','rule','mechanic','widget'],editor:{label:'Запустить задачу компонента',category:'Компоненты'},
 parameters:{type:'object',properties:{instance:{type:'reference',referenceType:'mechanic',label:'Экземпляр'},methodId:{type:'string',componentMethodFor:'instance',label:'Метод'},args:{type:'componentArguments',parameter:'instance',methodParameter:'methodId',label:'Аргументы'},lifetime:{type:'string',enum:['scene','project','component'],default:'scene',label:'Время жизни'},lifetimeOwner:{type:'union',variants:[{type:'reference',referenceType:'scene'},{type:'reference',referenceType:'project'},{type:'reference',referenceType:'mechanic'}],label:'Владелец задачи'}}},
 result:{type:'object',properties:{kind:{type:'string'},id:{type:'string'}}}
},(p,c)=>startOwnedComponentTask(c.capabilities,p.instance,p.methodId,p.args,p.lifetime,p.lifetimeOwner,c));
registerOperation({id:'sacura.behavior-tree.run',version:1,contexts:['event','function','rule','mechanic'],editor:{label:'Выполнять дерево поведения',category:'Компоненты'},parameters:{type:'object',properties:{instance:{type:'reference',referenceType:'mechanic',label:'Поведение'}}},result:{type:'void'}},(p,c)=>runComponentBehavior(c.capabilities,p.instance,c));
registerOperation({id:'sacura.behavior-tree.stop',version:1,contexts:['event','function','rule','mechanic'],editor:{label:'Остановить дерево поведения',category:'Компоненты'},parameters:{type:'object',properties:{instance:{type:'reference',referenceType:'mechanic',label:'Поведение'}}},result:{type:'void'}},(p,c)=>endComponentBehavior(c.capabilities,p.instance));
