import {isObjectInScene} from './sceneEditing.js';
import {operationDefinition} from './operationRegistry.js';
import {uid} from './model.js';
import {reference,resolveReference} from './valueSchemas.js';
import {validateExpression} from './expressionValidation.js';
import {validateOperationCall} from './operationRegistry.js';
import {signalDefinition} from './signalModel.js';
export const RULE_SCHEMA_VERSION=1;
export const RULE_LIMITS={signals:128,runs:8,perRule:16,chain:32};
export function createRule({signal='start',scope,actions=[],otherwise=[],...fields}={}){return {id:uid('rule'),name:'Новая реакция',version:1,enabled:true,signal:typeof signal==='string'?{type:signal}:signal,scope,filter:{},condition:null,actions,otherwise,once:false,cooldown:0,reentry:'queue',maxConcurrent:1,maxQueued:16,...fields};}
export function normalizeRule(project,record){
 const scope=record.scope|| (record.objectId?{kind:'object',id:record.objectId}:record.sceneId?{kind:'scene',id:record.sceneId}:{kind:'project',id:project.id});
 const legacyActions=otherwise=>{const out=[];if(!otherwise&&record.mechanicInstanceId)out.push({kind:'mechanic',instanceId:record.mechanicInstanceId});const operation=otherwise?record.elseOperation:record.operation,functionId=otherwise?record.elseFunctionId:record.functionId,eventId=otherwise?record.elseEventId:record.eventId,beatId=otherwise?record.elseNextBeatId:record.nextBeatId;if(operation)out.push({kind:'operation',call:operation});if(functionId)out.push({kind:'function',functionId,args:record.args||{}});if(eventId)out.push({kind:'event',eventId,join:record.join||'FLOW_END',overrides:record.overrides||{},actionOverrides:record.actionOverrides||{}});if(beatId)out.push({kind:'story',beatId});return out;};
 return {...record,seconds:record.seconds??record.timer?.seconds??2,repeat:record.repeat??record.timer?.repeat??false,center:record.center??[0,0,0],size:record.size??[3,2,3],version:record.version??1,scope,signal:record.signal||{type:record.trigger},filter:record.filter||{},actions:record.actions??legacyActions(false),otherwise:record.otherwise??legacyActions(true),once:!!record.once,cooldown:record.cooldown??0,reentry:record.reentry||'queue',maxConcurrent:record.maxConcurrent??1,maxQueued:record.maxQueued??16,timeSource:record.timeSource|| (record.version===1?'game':'native'),legacy:record.version===undefined};
}
export function ownerExists(project,scope,sceneId,mechanicInstanceId){
 if(mechanicInstanceId&&!project.gameplay.mechanics.some(m=>m.id===mechanicInstanceId))return false;
 if(scope.kind==='project')return scope.id===project.id;
 if(scope.kind==='scene')return project.subscenes.some(s=>s.id===scope.id)&&(!sceneId||scope.id===sceneId);
 const scene=project.subscenes.find(s=>s.id===sceneId);return project.objects.some(o=>o.id===scope.id&&o.active!==false&&(!sceneId||scene&&isObjectInScene(o,scene)));
}
export function ruleMatchResult(project,rule,envelope){
 const no=reason=>({matched:false,reason});
 if(rule.version!==1)return no('Версия правила недоступна.');if(rule.enabled===false)return no('Правило отключено.');if(rule.signal?.type!==envelope.type||envelope.ruleId&&envelope.ruleId!==rule.id)return no('Другой сигнал или адресат.');
 if(rule.sceneId&&rule.sceneId!==envelope.context.sceneId||rule.scope.kind==='scene'&&rule.scope.id!==envelope.context.sceneId)return no('Другая сцена.');
 if(!ownerExists(project,rule.scope,envelope.context.sceneId,rule.mechanicInstanceId))return no('Владелец отсутствует, отключён или находится в другой сцене.');
 if(rule.legacy&&rule.objectId&&envelope.type!=='start'&&rule.objectId!==envelope.payload.objectId)return no('Другой объект legacy-сигнала.');
 for(const role of ['source','target','initiator']){const selector=rule.filter[role]||(role==='source'?rule.signal.source:null);if(!selector)continue;const actual=envelope.context[role];if(!actual)return no('В сигнале отсутствует '+({source:'источник',target:'цель',initiator:'инициатор'})[role]+'.');try{const ids=resolveReference(selector,selector.type,{project,scope:rule.scope,bindings:{...envelope.context.bindings,...rule.bindings},sourceId:envelope.context.source?.id,targetId:envelope.context.target?.id});if(selector.elementId&&selector.elementId!==actual.elementId)return no('Другая кнопка.');if(actual.type!==selector.type||!(Array.isArray(ids)?ids.includes(actual.id):ids===actual.id))return no('Не совпал фильтр '+({source:'источника',target:'цели',initiator:'инициатора'})[role]+'.');}catch(e){return no(e.message);}}
 return {matched:true};
}
export const ruleMatches=(project,rule,envelope)=>ruleMatchResult(project,rule,envelope).matched;
export function validateRules(project){
 const issues=[],bad=(id,detail)=>issues.push({id:'rule-'+id,level:'error',title:'Проверьте правило',detail});
 for(const raw of project.gameplay?.rules||[]){
  try{
   const r=normalizeRule(project,raw);
   if(r.version!==1){bad(r.id,'Версия правила недоступна. Данные сохранены.');continue;}
   if(!r.signal||typeof r.signal.type!=='string'||!signalDefinition(project,r.signal.type))bad(r.id,'Сигнал правила недоступен.');
   if(!['skip','queue','restart'].includes(r.reentry)||!Number.isFinite(r.cooldown)||r.cooldown<0||!Number.isInteger(r.maxConcurrent)||r.maxConcurrent<1||r.maxConcurrent>RULE_LIMITS.runs||!Number.isInteger(r.maxQueued)||r.maxQueued<0||r.maxQueued>RULE_LIMITS.perRule)bad(r.id,'Проверьте cooldown и ограничения повторного входа.');
   if(!['game','native'].includes(r.timeSource))bad(r.id,'Источник времени недоступен.');
   if(['timer','tick'].includes(r.signal?.type)&&(!Number.isFinite(r.seconds)||r.seconds<0))bad(r.id,'Укажите время таймера.');
   if(['enter','exit'].includes(r.signal?.type)&&(!Array.isArray(r.center)||r.center.length!==3||!r.center.every(Number.isFinite)||!Array.isArray(r.size)||r.size.length!==3||!r.size.every(v=>Number.isFinite(v)&&v>0)))bad(r.id,'Размер области должен быть больше нуля.');
   if(!r.scope||!['object','scene','project'].includes(r.scope.kind)||!ownerExists(project,r.scope))bad(r.id,'Владелец правила удалён.');
   if(r.condition)validateExpression(r.condition);
   if(r.conditionArgs)validateExpression(r.conditionArgs);
   for(const selector of [...Object.values(r.filter),r.signal?.source].filter(Boolean)){resolveReference(selector,selector.type,{project,scope:r.scope,allowDeferred:true});if(selector.type==='widget'&&selector.elementId&&selector.mode!=='external'&&!project.widgets.find(w=>w.id===selector.id)?.elements.some(e=>e.id===selector.elementId&&e.type==='button'))bad(r.id,'Кнопка источника удалена. Выберите другую кнопку.');}
   if(r.conditionFunctionId&&!project.functions.some(f=>f.id===r.conditionFunctionId))bad(r.id,'Функция условия удалена.');
   if(r.conditionOperation){const d=operationDefinition(r.conditionOperation.id,r.conditionOperation.version);if(!d?.pure||d.result.type!=='boolean')bad(r.id,'Условию нужна операция чтения с логическим результатом.');for(const e of validateOperationCall(r.conditionOperation,{project,kind:'rule',allowExpressions:true,allowDeferred:true}))bad(r.id,e.message);}
   if(!Array.isArray(r.actions)||!Array.isArray(r.otherwise)){bad(r.id,'Действия правила должны быть списком.');continue;}
   for(const a of [...r.actions,...r.otherwise]){
    if(!a||!['operation','event','function','story','mechanic'].includes(a.kind)){bad(r.id,'Неизвестное действие правила.');continue;}
    if(a.kind==='operation')for(const e of validateOperationCall(a.call,{project,kind:'rule',allowExpressions:true,allowDeferred:true}))bad(r.id,e.message);
    if(a.kind==='event'){if(!project.events.some(e=>e.id===a.eventId))bad(r.id,'Действие правила ссылается на удалённое определение.');if(a.join&&!['NONE','STARTED','FLOW_END','EVENT_END'].includes(a.join))bad(r.id,'Ожидание Event недоступно.');}
    if(a.kind==='function'){if(!project.functions.some(f=>f.id===a.functionId))bad(r.id,'Действие правила ссылается на удалённое определение.');validateExpression(a.args);}
    if(a.kind==='story'&&!project.chapters.flatMap(c=>c.beats).some(b=>b.id===a.beatId)||a.kind==='mechanic'&&!project.gameplay.mechanics.some(m=>m.id===a.instanceId))bad(r.id,'Действие правила ссылается на удалённое определение.');
   }
  }catch(e){bad(raw?.id||'shape',e.message);}
 }
 return issues;
}
