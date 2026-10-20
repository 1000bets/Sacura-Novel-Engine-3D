import {allBeats,normalizeBatches,uid,PHASES} from './studioModel.js';
export const EVENT_DRAG_TYPE='application/sacura-event-binding';
export function moveEventBinding(project,sourceBeatId,bindingId,targetBeatId,phase,batchId=null,beforeId=null){
 if(!PHASES.some(p=>p.id===phase))return false;
 const source=allBeats(project).find(b=>b.id===sourceBeatId),target=allBeats(project).find(b=>b.id===targetBeatId);
 const binding=source?.bindings.find(b=>b.id===bindingId);
 if(!binding||!target||bindingId===beforeId)return false;
 normalizeBatches(source);if(source!==target)normalizeBatches(target);
 const batch=target.batches[phase].find(b=>b.id===batchId);
 for(const groups of Object.values(source.batches))for(const group of groups)group.bindingIds=group.bindingIds.filter(id=>id!==bindingId);
 if(source!==target){source.bindings=source.bindings.filter(b=>b.id!==bindingId);target.bindings.push(binding);}
 binding.hook=phase;
 const destination=batch||{id:uid('batch'),mode:'SEQUENTIAL',bindingIds:[]};
 if(!batch)target.batches[phase].push(destination);
 const index=destination.bindingIds.indexOf(beforeId);
 destination.bindingIds.splice(index<0?destination.bindingIds.length:index,0,bindingId);
 normalizeBatches(source);if(source!==target)normalizeBatches(target);
 return true;
}
