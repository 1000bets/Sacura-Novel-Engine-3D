import {allBeats} from './studioModel.js';

// Validate the whole edit before writing: a failed reconnect must keep its old wire.
export function connectionError(project,changes){
 const beats=allBeats(project);
 if(!changes.length)return 'Нет связей для изменения.';
 for(const c of changes){
  const beat=beats.find(b=>b.id===c.source);
  if(!beat || beat.kind==='end')return 'У этой реплики нет выхода.';
  if(beat.kind==='choice' ? !beat.choices?.some(x=>'choice:'+x.id===c.sourceHandle) : c.sourceHandle!=='next')return 'Выберите выход реплики или ответа.';
  if(c.target===c.source)return 'Узел не может продолжаться сам в себя.';
  if(c.target && !beats.some(b=>b.id===c.target))return 'Выберите вход существующей реплики.';
 }
 return null;
}
export function applyConnections(project,changes){
 const error=connectionError(project,changes);
 if(error)return error;
 const beats=allBeats(project);
 for(const c of changes){
  const beat=beats.find(b=>b.id===c.source);
  const owner=beat.kind==='choice'?beat.choices.find(x=>'choice:'+x.id===c.sourceHandle):beat;
  owner.next=c.target||null;
 }
 return null;
}
export function pinConnections(edges,pin){
 return edges.filter(e=>pin.type==='source'?e.source===pin.node&&e.sourceHandle===pin.handle:e.target===pin.node);
}
export function movePinConnections(edges,from,to){
 if(from.type!==to.type || (from.node===to.node&&from.handle===to.handle))return [];
 const wires=pinConnections(edges,from);
 return [...wires.map(e=>({source:e.source,sourceHandle:e.sourceHandle,target:null})),...wires.map(e=>({source:from.type==='source'?to.node:e.source,sourceHandle:from.type==='source'?to.handle:e.sourceHandle,target:from.type==='target'?to.node:e.target}))];
}
