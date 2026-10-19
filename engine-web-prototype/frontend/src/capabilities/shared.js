import {reference} from '../valueSchemas.js';
export const ref=(type,label,extra={})=>({type:'reference',referenceType:type,label,...extra});
export const ownerSchema={type:'union',label:'Владелец',variants:[ref('object','Объект'),ref('scene','Сцена'),ref('project','Проект')]};
export const number=(label,defaultValue=25)=>({type:'number',min:0,default:defaultValue,label});
const sourceFiles={health:'health',game:'health',signal:'health',behavior:'behavior',movement:'movement',weapon:'weapons',inventory:'inventory',interaction:'interaction',object:'interaction',timer:'timers',camera:'camera',resource:'resources',turn:'turns'};
export const operation=(id,label,properties,result,execute,extra={})=>({definition:{...(sourceFiles[id.split('.')[1]]?{source:{path:'src/capabilities/'+sourceFiles[id.split('.')[1]]+'.js',line:1}}:{}),id,parameters:{type:'object',properties},result,editor:{label},...extra},execute});
export const length=v=>Math.hypot(...v),sub=(a,b)=>a.map((v,i)=>v-b[i]);
export const finiteVector=v=>Array.isArray(v)&&v.length===3&&v.every(Number.isFinite);
export function ownerScope(project,owner){
 const id=typeof owner==='string'?owner:owner?.id;
 if(id===project.id)return {kind:'project',id};
 if(project.subscenes.some(s=>s.id===id))return {kind:'scene',id};
 if(project.objects.some(o=>o.id===id))return {kind:'object',id};
 throw Error('Владелец удалён. Выберите объект, сцену или проект.');
}
export const ownerKey=scope=>scope.kind+':'+scope.id;
export const ownerReference=scope=>reference(scope.kind,scope.id);
export function session(c){if(!c.capabilities)throw Error('Операции нужен игровой сеанс.');return c.capabilities;}
export function executionContext(context){return Object.fromEntries(['kind','version','scope','bindings','source','target','initiator','sceneId','mechanicInstanceId','reactionId','rootId','causeChain','nativeRunId','timeSource','sourceId','targetId','ownerId','capabilityOwner','capabilitySceneId','capabilityOwners'].filter(k=>context[k]!==undefined).map(k=>[k,context[k]]));}
