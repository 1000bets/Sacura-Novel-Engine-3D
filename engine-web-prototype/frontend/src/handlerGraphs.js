import {normalizeRule} from './ruleModel.js';
import {signalDefinition} from './signalModel.js';
import {parameterSchema} from './authoredMechanics.js';
import {migrateFunctionGraph} from './functionGraph.js';

export const HANDLER_GRAPH_VERSION=1;
const copy=value=>structuredClone(value);
const wire=nodeId=>({op:'node',nodeId,port:'value'});
const get=path=>({op:'get',path});

function parametersFor(project,rule,definition){
 const properties={...signalDefinition(project,rule.signal?.type)?.payloadSchema?.properties,...definition?.parameters?.properties};
 for(const id of [rule.conditionFunctionId,...rule.actions.filter(a=>a.kind==='function').map(a=>a.functionId),...rule.otherwise.filter(a=>a.kind==='function').map(a=>a.functionId)]){
  for(const p of project.functions.find(f=>f.id===id)?.parameters||[])if(!properties[p.name])properties[p.name]={...parameterSchema(p),optional:true};
 }
 return Object.entries(properties).map(([name,schema])=>({name,label:schema.label||name,schema:['variableValue','signalPayload'].includes(schema.type)?{type:'union',variants:[{type:'number'},{type:'boolean'},{type:'string'}],optional:true}:copy(schema)}));
}
function actionNode(action,id,project){
 const base={id,name:action.name||'Действие',position:{x:480,y:0}};
 if(action.kind==='operation')return {...base,kind:'operation',call:copy(action.call)};
 if(action.kind==='function'){const target=project.functions.find(f=>f.id===action.functionId),args=Object.fromEntries((target?.parameters||[]).map(p=>[p.name,action.args?.[p.name]!==undefined?copy(action.args[p.name]):get('args.'+p.name)]));return {...base,kind:'call',functionId:action.functionId,args};}
 if(action.kind==='event')return {...base,kind:'event',eventId:action.eventId,join:action.join||'FLOW_END',overrides:copy(action.overrides||{}),actionOverrides:copy(action.actionOverrides||{}),targetBindings:copy(action.targetBindings||{})};
 if(action.kind==='story')return {...base,kind:'command',command:{command:'goTo',value:{beatId:action.beatId}}};
 if(action.kind==='mechanic')return {...base,kind:'operation',call:{id:'sacura.mechanic.call',version:1,parameters:{mechanic:{kind:'reference',type:'mechanic',mode:'direct',id:action.instanceId}}}};
 throw Error('Невозможно перенести действие обработчика: '+action.kind+'. Исходные данные сохранены.');
}

// Each rule keeps only signal/admission policy and references to its editable program.
// Existing data is compiled on a detached copy before any record is replaced.
function compileHandler(project,record,key,definition){
 if(record.version!==undefined&&record.version!==1)return null;
 if(record.handlerGraphVersion!==undefined&&record.handlerGraphVersion!==HANDLER_GRAPH_VERSION)throw Error('Версия графа обработчика недоступна: '+record.id+'. Данные сохранены.');
 if(record.handlerFunctionId){if(!project.functions.some(f=>f.id===record.handlerFunctionId))throw Error('Граф обработчика удалён: '+record.name+'.');return null;}
 const r=normalizeRule(project,record),id=key+'-handler',guardId=key+'-guard';
 if(project.functions.some(f=>[id,guardId].includes(f.id)))throw Error('ID графа обработчика уже занят: '+record.name+'.');
 const parameters=parametersFor(project,r,definition),guard={id:guardId,name:(r.name||'Реакция')+' · условие',parameters:copy(parameters),locals:{},resultSchema:{type:'boolean'},preserveExtraArgs:true,entry:guardId+'-return',nodes:[]};
 let condition=r.condition??true;
 if(r.conditionOperation){guard.nodes.push({id:guardId+'-query',kind:'operation',call:copy(r.conditionOperation),next:guardId+'-return',position:{x:0,y:0}});guard.entry=guardId+'-query';condition=wire(guard.entry);}
 else if(r.conditionFunctionId){
  const target=project.functions.find(f=>f.id===r.conditionFunctionId);
  if(!target)throw Error('Функция условия удалена: '+r.conditionFunctionId+'.');
  const args=Object.fromEntries(target.parameters.map(p=>[p.name,r.conditionArgs?.[p.name]!==undefined?copy(r.conditionArgs[p.name]):get('args.'+p.name)]));
  guard.nodes.push({id:guardId+'-query',kind:'call',functionId:target.id,args,next:guardId+'-return',position:{x:0,y:0}});guard.entry=guardId+'-query';condition=wire(guard.entry);
 }
 guard.nodes.push({id:guardId+'-return',kind:'return',name:'Условие выполнено',value:copy(condition),position:{x:340,y:0}});
 const callNodeId=id+'-condition',branchId=id+'-branch',fn={id,name:r.name||'Обработчик',parameters:copy(parameters),locals:{},preserveExtraArgs:true,handler:{signal:copy(r.signal)},admission:{functionId:guardId,callNodeId,branchId},entry:callNodeId,nodes:[
  {id:callNodeId,kind:'call',name:'Проверить условие',functionId:guardId,args:Object.fromEntries(parameters.map(p=>[p.name,get('args.'+p.name)])),next:branchId,position:{x:0,y:100}},
  {id:branchId,kind:'branch',name:'Условие реакции',condition:wire(callNodeId),position:{x:300,y:100}},
 ]};
 const build=(actions,side)=>{
  const nodes=actions.map((a,i)=>({...actionNode(a,id+'-'+side+'-'+i,project),position:{x:660+i*340,y:side==='yes'?0:400}}));
  nodes.push({id:id+'-'+side+'-end',kind:'return',name:'Завершить обработчик',value:side==='yes',position:{x:660+actions.length*340,y:side==='yes'?0:400}});
  for(let i=0;i<nodes.length-1;i++)nodes[i].next=nodes[i+1].id;
  fn.nodes.push(...nodes);return nodes[0].id;
 };
 fn.nodes[1].trueNext=build(r.actions,'yes');fn.nodes[1].falseNext=build(r.otherwise,'no');
 const args=Object.fromEntries(parameters.map(p=>[p.name,get('args.'+p.name)])),invoke={kind:'function',functionId:id,args};
 const next={...copy(record),handlerGraphVersion:HANDLER_GRAPH_VERSION,handlerFunctionId:id,version:1,...(r.legacy&&r.objectId?{matchObjectId:true}:{}),signal:r.signal,scope:r.scope,actions:r.actions.length?[copy(invoke)]:[],otherwise:r.otherwise.length?[copy(invoke)]:[],timeSource:r.timeSource};
 for(const field of ['condition','conditionOperation','conditionFunctionId','conditionArgs','operation','functionId','eventId','nextBeatId','elseOperation','elseFunctionId','elseEventId','elseNextBeatId','args'])delete next[field];
 // Definition-local rules acquire their owner when instantiated.
 if(definition)delete next.scope;
 return {record,next,functions:[migrateFunctionGraph(guard,{persistentState:!!definition}),migrateFunctionGraph(fn,{persistentState:!!definition})]};
}

export function ensureHandlerGraphs(project){
 if(project.handlerGraphVersion!==undefined&&project.handlerGraphVersion!==HANDLER_GRAPH_VERSION)throw Error('Версия модели обработчиков недоступна. Данные сохранены.');
 const plans=[],statePlans=[];
 for(const r of project.gameplay?.rules||[]){if(r.mechanicInstanceId&&!r.version)continue;const plan=compileHandler(project,r,'rule-'+r.id);if(plan)plans.push(plan);}
 for(const d of project.gameplay?.mechanicDefinitions||[]){
  for(const r of [...d.rules||[],...d.machine?.transitions||[]]){const plan=compileHandler(project,r,'definition-'+d.id+'-'+d.version+'-'+r.id,d);if(plan)plans.push(plan);}
  for(const state of d.machine?.states||[])for(const phase of ['enter','exit']){
   if(state[phase+'FunctionId'])continue;
   const actions=state[phase]||[];
   const id='definition-'+d.id+'-'+d.version+'-'+state.id+'-'+phase;
   if(project.functions.some(f=>f.id===id))throw Error('ID обработчика состояния уже занят.');
   const parameters=parametersFor(project,{signal:{type:'start'},actions,otherwise:[]},d),nodes=actions.map((a,i)=>({...actionNode(a,id+'-'+i,project),position:{x:i*340,y:0}}));
   nodes.push({id:id+'-return',kind:'return',name:'Завершить',value:true,position:{x:nodes.length*340,y:0}});
   for(let i=0;i<nodes.length-1;i++)nodes[i].next=nodes[i+1].id;
   const fn=migrateFunctionGraph({id,name:d.name+' · '+state.name+' · '+(phase==='enter'?'вход':'выход'),parameters,preserveExtraArgs:true,locals:{},stateSchema:copy(d.stateSchema),entry:nodes[0].id,nodes,handler:{stateId:state.id,phase}},{persistentState:true});
   statePlans.push({state,phase,fn,actions:[{kind:'function',functionId:id,args:Object.fromEntries(parameters.map(p=>[p.name,get('args.'+p.name)]))}]});
  }
 }
 for(const plan of plans){for(const key of Object.keys(plan.record))delete plan.record[key];Object.assign(plan.record,plan.next);project.functions.push(...plan.functions);}
 for(const {state,phase,fn,actions}of statePlans){state[phase+'FunctionId']=fn.id;state[phase]=actions;project.functions.push(fn);}
 project.handlerGraphVersion=HANDLER_GRAPH_VERSION;return project;
}
export const handlerFunction=(project,rule)=>project.functions?.find(f=>f.id===rule?.handlerFunctionId);
export const eventGraphFunction=(project,eventId)=>project.functions?.find(f=>f.handler?.eventId===eventId);
export function setHandlerConditionArgument(project,record,name,value){
 const fn=handlerFunction(project,record),guard=project.functions.find(f=>f.id===fn?.admission?.functionId),call=guard?.nodes.find(n=>n.kind==='call');
 if(call){call.args??={};call.args[name]=copy(value);return;}
 record.conditionArgs??={};record.conditionArgs[name]=copy(value);
}
export function getHandlerConditionArgument(project,record,name){const fn=handlerFunction(project,record),guard=project.functions.find(f=>f.id===fn?.admission?.functionId),call=guard?.nodes.find(n=>n.kind==='call');return call?.args?.[name]??record.conditionArgs?.[name];}
export function getHandlerEvent(project,record){const fn=handlerFunction(project,record);return fn?.nodes.find(n=>n.kind==='event')?.eventId||record.actions?.find(a=>a.kind==='event')?.eventId||record.eventId||'';}
export function setHandlerEvent(project,record,eventId){
 const fn=handlerFunction(project,record),node=fn?.nodes.find(n=>n.kind==='event');if(node){node.eventId=eventId;return;}
 if(fn){const branch=fn.nodes.find(n=>n.id===fn.admission?.branchId),id=fn.id+'-event';if(!branch)throw Error('В обработчике отсутствует вход реакции.');if(fn.nodes.some(n=>n.id===id))throw Error('ID вызова Event занят.');fn.nodes.push({id,kind:'event',name:'Запустить Event',eventId,join:'FLOW_END',next:branch.trueNext,position:{x:660,y:0}});branch.trueNext=id;
  if(!record.actions?.length)record.actions=[{kind:'function',functionId:fn.id,args:Object.fromEntries(fn.parameters.map(p=>[p.name,get('args.'+p.name)]))}];return;
 }
 record.actions=[{kind:'event',eventId,join:'FLOW_END'}];
}
