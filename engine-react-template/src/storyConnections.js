import {isPureNode,dataConnectionError,setInput,getInput,dataTargets,logicType} from './logicModel.js';
import {typedValue} from './choiceModel.js';
import {allBeats} from './studioModel.js';

// Validate the whole edit before writing: a failed reconnect must keep its old wire.
export function connectionError(project,changes){
 const draft=structuredClone(project),beats=allBeats(draft);
 if(!changes.length)return 'Нет связей для изменения.';
 for(const c of changes){
  const beat=beats.find(b=>b.id===c.source);
  if(c.sourceHandle==='value'){const error=dataConnectionError(draft,c);if(error)return error;if(c.target)setInput(beats.find(b=>b.id===c.target),c.targetHandle,c.disconnect?null:c.source);else for(const target of beats)for(const port of dataTargets(target))if(getInput(target,port)===c.source)setInput(target,port,null);continue;}
  if(c.targetHandle&&c.targetHandle!=='in')return 'Ход истории подключается к входу со стрелкой.';
  if(!beat || isPureNode(beat)||beat.kind==='end')return 'У этой реплики нет выхода.';
  if(beat.kind==='branch' ? !['true','false'].includes(c.sourceHandle) : beat.kind==='choice' ? c.sourceHandle!=='next'&&!beat.choices?.some(x=>'choice:'+x.id===c.sourceHandle) : c.sourceHandle!=='next')return 'Выберите выход реплики или ответа.';
  if(c.target===c.source)return 'Узел не может продолжаться сам в себя.';
  if(c.target && !beats.some(b=>b.id===c.target&&!isPureNode(b)))return 'Выберите вход существующей реплики.';
 }
 return null;
}
export function applyConnections(project,changes){
 const error=connectionError(project,changes);
 if(error)return error;
 const beats=allBeats(project);
 for(const c of changes){
  const beat=beats.find(b=>b.id===c.source);
  if(c.sourceHandle==='value'){if(c.target){const target=beats.find(b=>b.id===c.target);setInput(target,c.targetHandle,c.disconnect?null:c.source);if(!c.disconnect&&target.kind==='compare'){target.valueType=logicType(project,beat);target.a=typedValue(target.a,target.valueType);target.b=typedValue(target.b,target.valueType);}}else for(const target of beats)for(const port of dataTargets(target))if(getInput(target,port)===c.source)setInput(target,port,null);continue;}
  if(beat.kind==='branch'){beat[c.sourceHandle==='true'?'trueNext':'falseNext']=c.target||null;continue;}
  if(beat.kind==='choice'&&c.sourceHandle==='next')beat.choiceMode='value';
  const owner=beat.kind==='choice'&&c.sourceHandle!=='next'?beat.choices.find(x=>'choice:'+x.id===c.sourceHandle):beat;
  owner.next=c.target||null;
 }
 return null;
}
export function pinConnections(edges,pin){
 return edges.filter(e=>pin.type==='source'?e.source===pin.node&&e.sourceHandle===pin.handle:e.target===pin.node&&(!pin.handle||(e.targetHandle||'in')===pin.handle));
}
export function movePinConnections(edges,from,to){
 if(from.type!==to.type || (from.node===to.node&&from.handle===to.handle))return [];
 const wires=pinConnections(edges,from);
 return [...wires.map(e=>({source:e.source,sourceHandle:e.sourceHandle,target:e.data?.valueWire?e.target:null,targetHandle:e.targetHandle,disconnect:e.data?.valueWire})),...wires.map(e=>({source:from.type==='source'?to.node:e.source,sourceHandle:from.type==='source'?to.handle:e.sourceHandle,target:from.type==='target'?to.node:e.target,targetHandle:from.type==='target'?to.handle:e.targetHandle}))];
}
