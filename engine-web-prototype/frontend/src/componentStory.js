import {newEvent,addToBatch} from './studioModel.js';
import {componentCallSummary} from './componentCatalog.js';
export function addStoryComponentCall(project,beatId,phase,call,batchId=null){
 const summary=componentCallSummary(project,call),event=newEvent('gameplay',undefined,undefined,summary.subject+' → '+summary.result);event.componentAction=true;Object.assign(event.groups[0].actions[0],{command:call.id,operationVersion:call.version||1,value:structuredClone(call.parameters)});project.events.push(event);const binding=addToBatch(project,beatId,phase,batchId,event.id);if(!binding)throw Error('Реплика для вызова удалена.');binding.localEventId=event.id;return binding;
}
